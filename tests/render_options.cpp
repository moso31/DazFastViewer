#include "editor/document.h"
#include "render_ir/options_json.h"
#include "bench/camera.h"
#include <fstream>
#include <iostream>
using namespace dfv;
using J=nlohmann::json;
static void require(bool b,const char *s) {if(!b) throw std::runtime_error(s);}
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
  const auto count=loaded.scene.lights.size();ir::add_studio(loaded.scene);require(loaded.scene.lights.size()==count,"覆盖了保存的环境光照");
  editor::Document document;document.loaded=loaded;auto snapshot=editor::initial_snapshot(document);require(snapshot.options==o,"快照未继承渲染选项");editor::collect_resources(document);require(document.loaded.scene.options==o,"资源回收丢失环境");
  CameraState camera;camera.yaw=0;camera.pitch=0;camera.target={};camera.distance=2;camera.move(1,0,0,1);require(std::abs(camera.target.y-1.2f)<1e-6f&&camera.distance==2,"W 位移错误");camera.move(-1,0,0,1);require(std::abs(camera.target.y)<1e-6f,"S 恢复错误");camera.move(0,1,1,1);require(camera.target.x>0&&camera.target.z>0,"DE 移动方向错误");
  runtime::TransformValues transform;transform.general_scale=2;require(runtime::make_transform(transform).point({1,0,0}).x==2,"总体缩放未应用");
  std::cout<<"render options / persistence / exposure / camera: PASS\n";return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<std::endl;return 1;}}
