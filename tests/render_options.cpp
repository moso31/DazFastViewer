#include "editor/document.h"
#include "render_ir/options_json.h"
#include "bench/camera.h"
#include <fstream>
#include <iostream>
using namespace dfv;
using J=nlohmann::json;
static void require(bool b,const char *s) {if(!b) throw std::runtime_error(s);}
static void local_pivot(const std::filesystem::path &folder) {
  const auto file=folder/"pivot.duf";
  auto axes=[](double x,double y,double z) {return J::array({{{"id","x"},{"value",x}},{{"id","y"},{"value",y}},{{"id","z"},{"value",z}}});};
  J d=J::parse(R"({"geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[10,0,0],[10,10,0],[10,0,10]]},"polygon_material_groups":{"count":1,"values":["surface"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],"uv_set_library":[{"id":"uv","uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}],"scene":{"nodes":[{"id":"parent"},{"id":"prop","parent":"#parent","geometries":[{"id":"shape","url":"#mesh"}]}]}})");
  d["scene"]["materials"]={{{"id","mat"},{"geometry","#shape"},{"groups",{"surface"}}}};d["uv_set_library"][0]["vertex_count"]=3;
  auto &parent=d["scene"]["nodes"][0];parent["translation"]=axes(100,30,-20);parent["rotation"]=axes(0,25,0);parent["scale"]=axes(1.2,1.2,1.2);
  auto &node=d["scene"]["nodes"][1];node["center_point"]=axes(10,3,1);node["translation"]=axes(20,4,8);node["rotation"]=axes(15,20,35);node["orientation"]=axes(12,18,3);node["rotation_order"]="ZXY";
  std::ofstream(file)<<d.dump();auto loaded=daz::load(file,{{folder},false});auto catalog=daz::discover_morphs(loaded,{folder});runtime::MorphRuntime runtime(loaded.scene,catalog.targets);
  runtime::TransformValues edit;edit.rotation_degrees={23,-7,51};edit.translation_cm={10,5,-3};edit.scale={2,3,.5f};runtime.set_transform(0,edit);runtime.evaluate();
  node["rotation"]=axes(38,13,86);node["translation"]=axes(30,9,5);node["scale"]=axes(2,3,.5);std::ofstream(file)<<d.dump();const auto expected=daz::load(file,{{folder},false});
  const auto &a=loaded.scene.instances[0].transform.value,&b=expected.scene.instances[0].transform.value;
  for(size_t i=0;i<a.size();++i) require(std::abs(a[i]-b[i])<2e-5,"编辑没有绕 DAZ 中心和局部轴旋转 / 缩放");
}
int main() {try {
  const auto folder=std::filesystem::temp_directory_path()/"dfv-render-options";std::filesystem::create_directories(folder);
  const auto file=folder/"scene.duf";std::ofstream(folder/"studio.hdr")<<"fixture";
  J tone=J::array(),env=J::array();
  auto channel=[](const char *id,J value,const char *group) {return J{{"group",group},{"channel",{{"id",id},{"type","float"},{"value",value}}}};};
  for(const auto &[id,value]:std::vector<std::pair<const char *,double>>{{"Exposure Value",14},{"Shutter Speed",256},{"Aperture",8},{"Film ISO",100},{"Gamma",2.2},{"cm2 Factor",1}}) tone.push_back(channel(id,value,"/Tone Mapping"));
  env.push_back(channel("Environment Mode",1,"/Environment"));env.push_back(channel("Environment Map",1,"/Environment/Dome"));env.push_back(channel("Environment Intensity",2,"/Environment/Dome"));env.push_back(channel("Dome Rotation",34,"/Environment/Dome"));
  J d={{"node_library",J::array({{{"id","tone"},{"extra",{{{"type","studio/node/tone_mapper"}},{{"type","studio_node_channels"},{"channels",tone}}}}},{{"id","env"},{"extra",{{{"type","studio/node/environment"}},{{"type","studio_node_channels"},{"channels",env}}}}}})},
    {"scene",{{"nodes",J::array({{{"id","saved-tone"},{"url","#tone"}},{{"id","saved-env"},{"url","#env"},{"extra",{{{"type","studio_node_channels"},{"channels",{{{"channel",{{"id","Environment Map"},{"current_value",3},{"image_file","studio.hdr"}}}}}}}}}}})}}}};
  std::ofstream(file)<<d.dump();auto loaded=daz::load(file,{{folder},false});const auto &o=loaded.scene.options;
  require(ir::number(o.environment,"Environment Map",0)==3&&ir::number(o.environment,"Environment Intensity",0)==2,"节点覆盖或资产通道继承失败");
  require(o.environment_file==std::filesystem::weakly_canonical(folder/"studio.hdr"),"HDRI 路径丢失");
  require(!ir::scene_lights(o)&&std::abs(ir::exposure(o)-.5f)<1e-6f,"Dome Only / EV 曝光错误");
  require(ir::options_from_json(ir::options_json(o))==o,"渲染选项保存 / 重读不一致");
  auto edited=o;ir::set_option(edited.tonemapper,0,0,15);require(ir::number(edited.tonemapper,"Shutter Speed",0)==512&&ir::exposure(edited)==.25f,"EV 未联动快门或没有减半曝光");
  ir::set_option(edited.tonemapper,3,0,200);require(ir::number(edited.tonemapper,"Exposure Value",0)==14,"ISO 未联动 EV");
  ir::set_option(edited.tonemapper,3,0,0);require(ir::exposure(edited)==1,"任意曝光模式错误");
  ir::set_option(edited.environment,3,0,-720);require(ir::number(edited.environment,"Dome Rotation",0)==-720,"环境角度被 UI 范围截断");
  auto brighter=o;for(auto &p:brighter.tonemapper.parameters) if(p.id=="Exposure Value") p.value[0]-=1;
  require(ir::display_color(brighter,{.1f,.2f,.3f}).x>ir::display_color(o,{.1f,.2f,.3f}).x,"离线色调没有应用曝光");
  local_pivot(folder);
  const auto count=loaded.scene.lights.size();ir::add_studio(loaded.scene);require(loaded.scene.lights.size()==count,"覆盖了保存的环境光照");
  editor::Document document;document.loaded=loaded;auto snapshot=editor::initial_snapshot(document);require(snapshot.options==o,"快照未继承渲染选项");editor::collect_resources(document);require(document.loaded.scene.options==o,"资源回收丢失环境");
  CameraState camera;camera.yaw=0;camera.pitch=0;camera.target={};camera.distance=2;camera.move(1,0,0,1);require(std::abs(camera.target.y-1.2f)<1e-6f&&camera.distance==2,"W 位移错误");camera.move(-1,0,0,1);require(std::abs(camera.target.y)<1e-6f,"S 恢复错误");camera.move(0,1,1,1);require(camera.target.x>0&&camera.target.z>0,"DE 移动方向错误");
  runtime::TransformValues transform;transform.general_scale=2;require(runtime::make_transform(transform).point({1,0,0}).x==2,"总体缩放未应用");
  std::cout<<"render options / persistence / exposure / camera: PASS\n";return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<std::endl;return 1;}}
