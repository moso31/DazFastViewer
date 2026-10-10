#include "cycles/adapter.h"
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
static ir::Scene fixture(float distance,int layers,float opacity,bool enabled){
  ir::Scene s;s.environment={0,0,0};ir::Material surface;surface.id="receiver";surface.base_color={.8f,.8f,.8f};surface.roughness=1;surface.specular=0;s.materials.push_back(surface);
  surface.id="blocker";surface.opacity=opacity;s.materials.push_back(surface);
  ir::Mesh mesh;mesh.id="plane";mesh.positions={{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0}};mesh.material_slots={"surface"};mesh.smooth=false;
  ir::Triangle a;a.vertices={0,1,2};mesh.triangles.push_back(a);a.vertices={0,2,3};mesh.triangles.push_back(a);s.meshes.push_back(mesh);
  mesh.id="blocker-plane";s.meshes.push_back(mesh);
  ir::Instance receiver;receiver.id="receiver";receiver.materials={0};receiver.local_shadow=enabled;s.instances.push_back(receiver);
  // 接收面用巨大实例变换表达，保持可解析的 20 米方形世界几何。
  for(auto &v:s.meshes[0].positions){v.x/=65536;v.y/=65536;}
  s.instances[0].transform.value[0]=s.instances[0].transform.value[5]=s.instances[0].transform.value[10]=65536;
  // 灯和遮挡位于相机背后，只通过阴影影响接收面。
  for(int i=0;i<layers;++i){ir::Instance blocker;blocker.id="blocker-"+std::to_string(i);blocker.mesh=1;blocker.materials={1};blocker.transform=ir::Transform::translate({0,0,distance*.5f+i*.25f});s.instances.push_back(blocker);}
  ir::AreaLight light;light.id="point";light.kind=ir::LightKind::point;light.transform=ir::Transform::translate({0,0,distance});light.power={distance*distance*10,distance*distance*10,distance*distance*10};s.lights.push_back(light);
  CameraState camera;camera.target={0,0,0};camera.distance=distance<16?1.f:4.f;camera.pitch=1.2f;s.camera=render_camera(camera,64,64);return s;
}
int main(int argc,char **argv){try{
  check(argc>=3,"LocalShadowRenderTest output runtime");const fs::path output=fs::absolute(argv[1]);fs::create_directories(output);
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[2]).string(),(output/"cache").string());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  ccl::SessionParams p;p.device=devices.front();p.background=p.headless=true;p.samples=32;p.threads=4;p.use_resolution_divider=false;p.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;
  ccl::Session session(p,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);
  scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(1);scene.integrator->set_transparent_max_bounce(16);scene.integrator->set_seed(1337);
  CyclesAdapter adapter(scene);ccl::BufferParams buffers;buffers.width=buffers.full_width=buffers.height=buffers.full_height=64;J results=J::array();
  auto render=[&](ir::Scene source,const std::string &name){
    {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}
    fs::create_directories(output/name);auto driver=std::make_unique<Output>(output/name);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(p,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");
    double energy=0;for(int y=16;y<48;++y)for(int x=16;x<48;++x)for(int c=0;c<3;++c){const float v=result->linear_pixels[(y*64+x)*4+c];check(std::isfinite(v),"Nonfinite radiance");energy+=v;}
    results.push_back({{"case",name},{"energy",energy}});std::cout<<results.back().dump()<<std::endl;return energy;
  };
  for(float distance:{8.f,8000.f,1000000.f}){
    const auto prefix=std::to_string(int(distance));const double clear=render(fixture(distance,0,1,true),prefix+"-clear");check(clear>1,"Unoccluded reference is dark");
    for(int layers:{1,3,6})for(float opacity:{1.f,.5f}){
      const auto name=prefix+"-"+std::to_string(layers)+"-"+std::to_string(opacity);
      const double reference=render(fixture(distance,layers,opacity,false),name+"-off");
      const double local=render(fixture(distance,layers,opacity,true),name+"-on");
      check(std::abs(reference-local)/clear<.005,"Local queries changed shadow transmission");
      check(std::abs(local/clear-std::pow(1.-opacity,layers))<.005,"Occlusion does not match known layered transmission");
    }
    auto moved=fixture(distance,1,1,true);render(moved,prefix+"-before-move");
    const auto updates=adapter.stats().geometry_updates;moved.camera.transform.value[3]+=2;
    render(moved,prefix+"-camera");check(adapter.stats().geometry_updates==updates,"Camera motion rebuilt geometry");
    moved.camera.transform.value[3]-=2; // 近点光源照度随位置变化；恢复相机后再比较 clear。
    moved.instances[1].transform.value[3]=100;check(render(moved,prefix+"-blocker-moved")/clear>.99,"Moved blocker retained stale local bounds");
    moved.instances[1].transform.value[3]=0;check(render(moved,prefix+"-blocker-restored")/clear<.005,"Restored blocker lost shadow");
    moved.instances[1].transform.value[0]=-.5f;moved.instances[1].transform.value[5]=2;moved.instances[1].transform.value[8]=.25f;
    check(render(moved,prefix+"-mirrored-shear")/clear<.005,"Negative scale or shear lost shadow");
    for(auto &v:moved.meshes[1].positions)v.x+=100;
    check(render(moved,prefix+"-geometry-edited")/clear>.99,"Geometry edit retained stale local IAS");
    for(auto &v:moved.meshes[1].positions)v.x-=100;
    check(render(moved,prefix+"-geometry-restored")/clear<.005,"Restored geometry lost shadow");
  }
  // 多于 4 层的透明网格需要分批着色；同一网格与多个实例均应保留最近交点。
  // 低不透明度让漏层、重复衰减和透明反弹上限错误在能量比较中可见。
  for(bool merged:{false,true})for(int layers:{4,5,12,16,17}){
    auto source=fixture(8000,layers,.1f,true);
    if(merged){
      ir::Mesh mesh;mesh.id="layered-blocker";mesh.material_slots={"surface"};mesh.smooth=false;
      constexpr float scale=2250000;
      for(int i=layers-1;i>=0;--i){
        const auto offset=uint32_t(mesh.positions.size());
        for(auto v:source.meshes[1].positions){v.z+=i*.25f;v.x/=scale;v.y/=scale;v.z/=scale;mesh.positions.push_back(v);}
        for(auto triangle:source.meshes[1].triangles){for(auto &v:triangle.vertices)v+=offset;mesh.triangles.push_back(triangle);}
      }
      source.meshes[1]=std::move(mesh);source.instances.resize(2);
      auto &blocker=source.instances[1];blocker.local_shadow=true;
      blocker.transform.value[0]=blocker.transform.value[5]=blocker.transform.value[10]=scale;
    }
    const auto name=std::string(merged?"batch-mesh-":"batch-instances-")+std::to_string(layers);
    auto empty=source;for(size_t i=1;i<empty.instances.size();++i)empty.instances[i].visible=false;
    const double clear=render(empty,name+"-clear");check(clear>1,"Batch reference is dark");
    auto original=source;for(auto &o:original.instances)o.local_shadow=false;
    const double before=render(original,name+"-off"),after=render(source,name+"-on");
    const double expected=layers>16?0:std::pow(.9,layers);
    check(std::abs(after-before)/clear<.005,"Batched local transparency differs from world traversal");
    check(std::abs(after/clear-expected)<.005,"Batched transparency lost layers or exceeded bounce limit");
  }
  // 无限距离光源、普通接收面与巨型遮挡物，覆盖原先被入口条件排除的光路。
  for(bool distant:{false,true})for(bool giant_blocker:{false,true}){
    const std::string prefix=std::string(distant?"sun":"sky")+(giant_blocker?"-giant-blocker":"-giant-receiver");
    auto make=[&](int layers,float opacity,bool enabled){
      auto source=fixture(8,layers,opacity,enabled);
      if(distant){auto &light=source.lights[0];light.kind=ir::LightKind::distant;light.power={3,3,3};light.angle=0;}
      else{source.lights.clear();source.environment={1,1,1};}
      if(giant_blocker){
        source.instances[0].local_shadow=false;source.instances[0].transform=ir::Transform{};
        for(auto &v:source.meshes[0].positions){v.x*=65536;v.y*=65536;}
        for(auto &v:source.meshes[1].positions){v.x/=65536;v.y/=65536;}
        for(size_t i=1;i<source.instances.size();++i){auto &o=source.instances[i];o.local_shadow=enabled;o.transform.value[0]=o.transform.value[5]=o.transform.value[10]=65536;}
      }
      // 不相交的密集对象与巨大远处包围盒，检查层级筛选不会误漏真实遮挡。
      for(int i=0;i<80;++i){auto object=source.instances[0];object.id="distractor-"+std::to_string(i);object.local_shadow=false;object.transform.value[3]=1000+i*100;object.transform.value[7]=-1000;source.instances.push_back(object);}
      return source;
    };
    const double clear=render(make(0,1,false),prefix+"-clear");check(clear>1,"Infinite-light reference is dark");
    for(int layers:{1,6})for(float opacity:{1.f,.5f}){
      auto source=make(layers,opacity,true);const auto name=prefix+"-"+std::to_string(layers)+"-"+std::to_string(opacity);
      auto before=source;for(auto &o:before.instances)o.local_shadow=false;
      const double reference=render(before,name+"-off"),local=render(source,name+"-on");
      check(std::abs(reference-local)/clear<.005,"Infinite or reverse-direction shadow changed");
      if(distant)check(std::abs(local/clear-std::pow(1.-opacity,layers))<.005,"Sunlight transmission is incorrect");
      if(layers==1&&opacity==1){source.instances[1].visible=false;check(std::abs(render(source,name+"-hidden")-clear)/clear<.005,"Hidden blocker retained shadow");}
    }
  }
  std::ofstream(output/"checks.json")<<J{{"result","PASS"},{"stages",results}}.dump(2);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
