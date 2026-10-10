#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "bench/output.h"
#include "bench/fixtures.h"
#include "scene/scene.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "scene/camera.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <iostream>
#include <numbers>
#include <nlohmann/json.hpp>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
static ir::Scene fixture(){
  ir::Scene source;source.environment={.7f,.8f,.9f};
  ir::Material iris;iris.id="iris";iris.base_color={.13f,.23f,.035f};iris.roughness=.4f;source.materials.push_back(iris);
  ir::Material glass;glass.id="cornea";glass.base_color={1,1,1};glass.transmission=1;glass.roughness=.03f;glass.ior=1.38f;glass.thin_walled=true;source.materials.push_back(glass);
  // Two clear eye layers, with a submillimetre gap over an opaque iris.
  for(int layer=0;layer<3;++layer){ir::Mesh mesh;mesh.id="sphere-"+std::to_string(layer);mesh.material_slots={"surface"};constexpr uint32_t rings=40,segments=80;const float radius=.012f+layer*.0001f;
    mesh.positions.push_back({0,0,1.75f+radius});
    for(uint32_t y=1;y<rings;++y)for(uint32_t x=0;x<segments;++x){const float theta=2*std::numbers::pi_v<float>*x/segments,phi=std::numbers::pi_v<float>*y/rings;mesh.positions.push_back({radius*std::sin(phi)*std::cos(theta),radius*std::sin(phi)*std::sin(theta),1.75f+radius*std::cos(phi)});}
    const auto bottom=uint32_t(mesh.positions.size());mesh.positions.push_back({0,0,1.75f-radius});
    auto face=[&](uint32_t a,uint32_t b,uint32_t c){ir::Triangle t;t.vertices={a,b,c};mesh.triangles.push_back(t);};
    for(uint32_t x=0;x<segments;++x){const uint32_t next=(x+1)%segments;face(0,1+x,1+next);face(bottom,bottom-segments+next,bottom-segments+x);for(uint32_t y=0;y<rings-2;++y){const auto a=1+y*segments+x,b=1+y*segments+next,c=a+segments,d=b+segments;face(a,c,b);face(b,c,d);}}
    source.meshes.push_back(std::move(mesh));ir::Instance instance;instance.id="eye-layer-"+std::to_string(layer);instance.mesh=layer;instance.materials={layer?1u:0u};source.instances.push_back(instance);
  }
  ir::AreaLight light;light.id="area";light.width=light.height=.02f;light.power={.015f,.012f,.01f};light.transform=ir::Transform::translate({.02f,-.035f,1.8f});source.lights.push_back(light);
  CameraState camera;camera.target={0,0,1.75f};camera.distance=.039f;camera.yaw=.3f;camera.pitch=.1f;source.camera=render_camera(camera,256,256);return source;
}
static void translate(ir::Scene &source,ir::Vec3 offset){auto move=[&](ir::Transform &t){t.value[3]+=offset.x;t.value[7]+=offset.y;t.value[11]+=offset.z;};for(auto &i:source.instances)move(i.transform);for(auto &l:source.lights)move(l.transform);move(source.camera.transform);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/eye-surface/regression");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=256;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(12);
  const auto initial=fixture();auto source=initial;CyclesAdapter adapter(scene);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=buffers.height=buffers.full_height=256;std::vector<float> reference;J stages=J::array();bool passed=true;
  auto render=[&](const char *name){const auto folder=output/name;fs::create_directories(folder);auto driver=std::make_unique<Output>(folder);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");if(reference.empty())reference=result->linear_pixels;
    double error=0,energy=0;size_t seams=0;for(int y=32;y<224;++y)for(int x=32;x<224;++x){const size_t p=(y*256+x)*4;double diff=0;for(size_t c=0;c<3;++c){check(std::isfinite(result->linear_pixels[p+c]),"Nonfinite radiance");const double d=std::abs(double(reference[p+c])-result->linear_pixels[p+c]);error+=d;diff+=d;energy+=reference[p+c];}seams+=diff>.18;}
    const double relative=error/std::max(energy,1e-12);passed&=relative<.01;stages.push_back({{"stage",name},{"relative_l1",relative},{"seam_pixels",seams}});std::cout<<stages.back().dump()<<std::endl;return result->linear_pixels;};
  render("origin");source=initial;translate(source,{-106,-497,60});{ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}render("far-synchronized");
  source=initial;translate(source,{-128,-512,128});{ccl::thread_scoped_lock lock(scene.mutex);ir::Delta delta;delta.camera=source.camera;for(uint32_t i=0;i<source.instances.size();++i)delta.instances.push_back({i,source.instances[i].transform});for(uint32_t i=0;i<source.lights.size();++i)delta.lights.push_back({i,source.lights[i]});adapter.apply(delta);}render("far-delta");
  {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(initial);}render("restored");
  for(float scale:{100.f,.1f,1.f}){source=initial;for(auto &i:source.instances){i.transform.value[0]=i.transform.value[5]=i.transform.value[10]=scale;}for(auto &l:source.lights){l.width*=scale;l.height*=scale;l.power.x*=scale*scale;l.power.y*=scale*scale;l.power.z*=scale*scale;for(int axis:{3,7,11})l.transform.value[axis]*=scale;}for(int axis:{3,7,11})source.camera.transform.value[axis]*=scale;translate(source,{-106,-497,60});
    // Compare the same encoded camera/light positions, including their input
    // float rounding. Otherwise a tiny eye is seen from a different camera.
    auto expected=source;translate(expected,{106,497,-60});{ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(expected);}reference.clear();render(("scale-reference-"+std::to_string(scale)).c_str());
    {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}render(("scale-distant-"+std::to_string(scale)).c_str());}
  // Cross a render-origin cell using only a camera edit. A later complete sync
  // must reproduce it exactly and must not rebuild mesh buffers.
  source=initial;translate(source,{.499f,.499f,.749f});{ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}
  CameraState camera;camera.target={.499f,.499f,2.499f};camera.distance=.039f;camera.yaw=-.3f;camera.pitch=.1f;source.camera=render_camera(camera,256,256);
  const auto geometry_updates=adapter.stats().geometry_updates;{ccl::thread_scoped_lock lock(scene.mutex);ir::Delta delta;delta.camera=source.camera;adapter.apply(delta);}reference.clear();render("camera-cell-delta");
  {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}render("camera-cell-synchronized");check(adapter.stats().geometry_updates==geometry_updates,"Camera rebase rebuilt geometry");
  // 连续平移穿过多个 1 m 单元：预览不更新全场景原点，停下后无需再改变
  // 相机输入即可恢复精确坐标。完整图像必须与普通同步相同。
  {
    ccl::thread_scoped_lock lock(scene.mutex);const auto before=adapter.stats().origin_updates;ir::Delta move;
    for(int k=1;k<=20;++k){move.camera=source.camera;move.camera->transform.value[3]+=k*.25f;adapter.apply(move,true,4);}
    check(adapter.stats().origin_updates==before,"Navigation rebased every metre");
    adapter.apply({});check(adapter.stats().origin_updates==before+1,"Navigation end did not restore precise origin");
    const auto matrix=scene.camera->get_matrix();check(std::max({std::abs(matrix.x.w),std::abs(matrix.y.w),std::abs(matrix.z.w)})<=.50001f,"Final camera is not near render origin");
    auto synchronized=source;move.camera->transform.value[3]+=5;adapter.apply(move,true,4);synchronized.camera=*move.camera;adapter.synchronize(synchronized);
    const auto synced=scene.camera->get_matrix();check(std::max({std::abs(synced.x.w),std::abs(synced.y.w),std::abs(synced.z.w)})<=.50001f,"Full sync changed origin without updating camera");
    // 超过预览范围的瞬移及时重定位，防止无限积累坐标误差。
    move.camera->transform.value[3]+=1000;adapter.apply(move,true,4);check(adapter.stats().origin_updates==before+2,"Large navigation jump did not rebase");
    // 在预览状态回到眼球特写，再用空增量结束导航。
    move.camera=source.camera;adapter.apply(move,true,.0039f);const auto close=scene.camera->get_matrix();check(std::max({std::abs(close.x.w),std::abs(close.y.w),std::abs(close.z.w)})<=.50001f,"Close-up navigation lost precise origin");adapter.apply({});
  }
  render("navigation-restored-eye");check(adapter.stats().geometry_updates==geometry_updates,"Navigation rebuilt eye meshes");
  source.materials[0].base_color={.05f,.09f,.3f};{ccl::thread_scoped_lock lock(scene.mutex);ir::Delta delta;delta.materials={{0,source.materials[0]}};adapter.apply(delta);}reference.clear();render("material-delta");
  {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}render("material-synchronized");
  std::ofstream(output/"checks.json")<<J{{"result",passed?"PASS":"FAIL"},{"device",devices.front().description},{"samples",params.samples},{"stages",stages}}.dump(2);check(passed,"Distant transparent surfaces acquired triangle seams");return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<std::endl;return 1;}}
