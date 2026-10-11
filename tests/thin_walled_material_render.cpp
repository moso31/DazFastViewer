#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "bench/output.h"
#include "bench/fixtures.h"
#include "scene/scene.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/thin-walled-material");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  ir::Scene source;source.environment={1,1,1};ir::Material material;material.id="thin-card";material.thin_walled=true;material.base_color={0,0,0};material.translucency_color={0,0,0};material.roughness=.3f;source.materials.push_back(material);
  ir::Mesh mesh;mesh.id="card";mesh.material_slots={"thin-card"};mesh.positions={{-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1}};
  ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};mesh.triangles={a,b};source.meshes.push_back(mesh);
  ir::Instance instance;instance.id="card";instance.materials={0};source.instances.push_back(instance);CameraState camera;camera.target={0,0,0};camera.yaw=camera.pitch=0;camera.distance=3;source.camera=render_camera(camera,128,128);
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=128;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_transparent_max_bounce(64);
  // 关闭体积 SSS 后，薄壁透光仍应保留；它不能吞掉外层反射。
  RenderQuality quality;quality.subsurface=false;CyclesAdapter adapter(scene,false,false,quality);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=buffers.height=buffers.full_height=128;J stages=J::array();
  auto render=[&](const char *name){
    {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}const auto folder=output/name;fs::create_directories(folder);auto driver=std::make_unique<Output>(folder);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");
    double mean=0;for(int y=40;y<88;++y)for(int x=40;x<88;++x)for(int c=0;c<3;++c)mean+=result->linear_pixels[(y*128+x)*4+c];mean/=48*48*3;stages.push_back({{"name",name},{"mean",mean}});return mean;
  };
  auto &m=source.materials[0];m.weighted_glossy=true;m.specular=1;const auto glossy=render("glossy");m.translucency=.8f;const auto glossy_translucent=render("glossy-translucent");
  check(glossy>.5&&std::abs(glossy-glossy_translucent)<.01,"薄壁透光错误衰减了加权高光");
  m.specular=0;m.translucency=0;m.coat=1;m.coat_mode=1;const auto coat=render("coat");m.translucency=1;const auto coat_translucent=render("coat-translucent");
  check(coat>.5&&std::abs(coat-coat_translucent)<.01,"薄壁透光错误衰减了 Top Coat");
  m.coat=.2f;m.specular=.19f;m.translucency=.8f;const auto layered=render("layered");
  // 黑色的反射/透光底层没有能量；两个白色外层应保留各自权重。
  check(std::abs(layered-(.2+.8*.19))<.04,"薄壁叠层反射能量不正确");
  m.coat=m.specular=0;m.translucency=0;const auto black=render("black");m.opacity=0;const auto cutout=render("cutout");
  check(black<.01&&cutout>.95,"最终 Cutout 未作用于整个表面");
  m.opacity=1;m.translucency=1;m.translucency_color={.4f,.4f,.4f};const auto transmitted=render("diffuse-transmission");check(std::abs(transmitted-.4)<.02,"薄壁漫透射被禁用或权重错误");
  std::ofstream(output/"checks.json")<<J{{"result","PASS"},{"device",devices.front().description},{"samples",params.samples},{"stages",stages}}.dump(2);return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<std::endl;return 1;}}
