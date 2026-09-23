#pragma once
#include "cycles/adapter.h"
#include "daz/loader.h"
#include "bench/output.h"
#include "bench/fixtures.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/mesh.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include <fstream>

namespace dfv {
// 在真实设备上检查置换后的顶点；覆盖导入、数据色彩空间和增量重算。
inline int displacement_check(const ccl::DeviceInfo &device,const std::filesystem::path &output) {
  using J=nlohmann::json;using namespace ccl;
  std::filesystem::create_directories(output);
  auto require=[](bool ok,const char *why) {if(!ok) throw std::runtime_error(why);};
  for(auto [name,value]:std::vector<std::pair<std::string,float>>{{"black",0},{"gray",.5f},{"white",1}}) {
    const auto file=(output/(name+".exr")).string();auto image=OIIO::ImageOutput::create(file);
    std::vector<float> pixels(4*4*3,value);require(image&&image->open(file,OIIO::ImageSpec(4,4,3,OIIO::TypeDesc::FLOAT))&&image->write_image(OIIO::TypeDesc::FLOAT,pixels.data())&&image->close(),"置换验证贴图写入失败");
  }
  auto d=J::parse(R"({"geometry_library":[{"id":"mesh","vertices":{"count":4,"values":[[0,0,0],[100,0,0],[100,100,0],[0,100,0]]},"polygon_material_groups":{"count":1,"values":["surface"]},"polylist":{"count":1,"values":[[0,0,0,1,2,3]]},"default_uv_set":"#uv"}],"uv_set_library":[{"id":"uv","vertex_count":4,"uvs":{"count":4,"values":[[0,0],[1,0],[1,1],[0,1]]}}],"scene":{"nodes":[],"materials":[]}})");
  const std::vector<std::string> maps={"black","gray","white","white","white",""};
  for(int i=0;i<6;++i) {
    const auto id=std::to_string(i);d["scene"]["nodes"].push_back({{"id","object"+id},{"translation",J::array({150*i,0,0})},{"geometries",J::array({{{"id","shape"+id},{"url","#mesh"}}})}});
    J channels=J::array();
    auto channel=[&](const char *key,J value) {channels.push_back({{"channel",{{"id",key},{"value",value}}}});};
    channel("Displacement Strength",2);if(!maps[i].empty()) channels.back()["channel"]["image_file"]=maps[i]+".exr";
    channel("Minimum Displacement",i==3?3:-1);channel("Maximum Displacement",i==3?-1:3);channel("Displacement Active",i!=4);
    d["scene"]["materials"].push_back({{"id","material"+id},{"geometry","#shape"+id},{"groups",{"surface"}},{"extra",J::array({{{"channels",channels}}})}});
  }
  const auto file=output/"fixture.duf";std::ofstream(file)<<d.dump(2);
  auto loaded=daz::load(file,{{output},false});auto source=loaded.scene;
  auto clone=source.instances[2];clone.id="scaled-copy";clone.prototype=2;clone.transform.value[0]=clone.transform.value[5]=clone.transform.value[10]=2;clone.transform.value[3]=10;source.instances.push_back(clone);
  const auto original=source.materials;
  CameraState camera;camera.target={4,0,.5f};camera.distance=12;camera.yaw=0;camera.pitch=.1f;source.camera=render_camera(camera,256,128);ir::add_studio(source);
  SessionParams params;params.device=device;params.background=params.headless=true;params.samples=4;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  SceneParams sp;sp.background=true;sp.bvh_type=BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;
  Session session(params,sp);auto &scene=*session.scene;
  auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);
  scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);
  CyclesAdapter adapter(scene);adapter.load(source);
  BufferParams buffers;buffers.width=buffers.full_width=256;buffers.height=buffers.full_height=128;
  std::vector<Mesh *> geometries;
  for(size_t i=0;i<7;++i) {
    Mesh *found=nullptr;for(auto *object:scene.objects) if(object->name==ustring(source.instances[i].id)) found=static_cast<Mesh *>(object->get_geometry());
    require(found!=nullptr,"缺少置换测试对象");geometries.push_back(found);
  }
  require(geometries[2]==geometries[6],"置换破坏了实例网格共享");
  J checks=J::array();
  auto run=[&](const char *name,const std::array<float,7> &heights,float morph=0) {
    const auto folder=output/name;std::filesystem::create_directories(folder);auto result=std::make_unique<Output>(folder);auto *written=result.get();session.set_output_driver(std::move(result));
    session.reset(params,buffers);session.start();session.wait();
    require(!session.progress.get_error(),session.progress.get_error_message().c_str());require(written->written&&written->error.empty(),"置换测试渲染失败");
    float maximum=0;J measured=J::array();
    for(size_t i=0;i<7;++i) {
      const auto *mesh=geometries[i];require(mesh->num_verts()==4,"置换错误改变源网格拓扑");
      const auto *p=mesh->get_position();float error=0;
      for(int v=0;v<4;++v) error=std::max(error,std::abs(p[v].y-(morph-heights[i])));
      maximum=std::max(maximum,error);measured.push_back({{"id",source.instances[i].id},{"y_m",p[0].y},{"error_m",error}});
    }
    checks.push_back({{"stage",name},{"vertices",measured},{"maximum_error_m",maximum}});
    std::ofstream(output/"displacement-check.json")<<J({{"status",maximum<2e-6?"RUNNING":"FAIL"},{"checks",checks},{"shared_geometry",true}}).dump(2);
    require(maximum<2e-6,"GPU 置换后的顶点高度错误，详见 displacement-check.json");
  };
  const std::array<float,7> enabled={-.02f,.02f,.06f,-.02f,0,0,.06f};
  run("initial",enabled);
  auto apply=[&](const ir::Delta &delta) {thread_scoped_lock lock(scene.mutex);adapter.apply(delta);};
  ir::Delta color;for(uint32_t i=0;i<original.size();++i) {auto m=original[i];m.base_color={.6f,.2f,.1f};color.materials.push_back({i,m});}apply(color);run("color-edit",enabled);
  ir::Delta off;for(uint32_t i=0;i<original.size();++i) {auto m=original[i];m.displacement_strength=0;off.materials.push_back({i,m});}apply(off);run("disabled",{});
  ir::Delta on;for(uint32_t i=0;i<original.size();++i) on.materials.push_back({i,original[i]});apply(on);run("restored",enabled);
  ir::Delta morph;auto positions=source.meshes[0].positions;for(auto &p:positions) p.y+=.1f;morph.meshes.push_back({0,positions});apply(morph);run("morph",enabled,.1f);
  apply(color);run("morph-color-edit",enabled,.1f);
  ir::Delta reset;reset.meshes.push_back({0,source.meshes[0].positions});apply(reset);run("morph-reset",enabled);
  ir::Delta view;view.camera=source.camera;view.camera->transform.value[3]+=.1f;apply(view);run("camera",enabled);
  ir::Delta move;auto transform=source.instances[0].transform;transform.value[3]+=.2f;move.instances.push_back({0,transform});apply(move);run("object-transform",enabled);
  std::ofstream(output/"displacement-check.json")<<J({{"status","PASS"},{"checks",checks},{"shared_geometry",true},{"device",device.id}}).dump(2);
  std::cout<<"Displacement GPU checks: PASS (9 stages)\n";return 0;
}
}
