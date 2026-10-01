#include "editor/document.h"
#include "editor/render_edit_queue.h"
#include "render_ir/options_json.h"
#include "bench/camera.h"
#include "render_ir/sun_sky.h"
#include "daz/resource_paths.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace dfv;
using J=nlohmann::json;
static void require(bool b,const char *s) {if(!b) throw std::runtime_error(s);}
static void environment_paths(const std::filesystem::path &parent) {
  namespace fs=std::filesystem;
  const auto folder=parent/("paths-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto library=folder/"G1",installation=folder/L"DAZ Studio 测试",iray=installation/"shaders/iray";
  const auto hdr=library/L"Runtime/Textures/Test/sky #100% 中文.hdr",builtin=iray/"resources/dfv-test-sky.hdr";
  for(const auto &p:{hdr,builtin}) {fs::create_directories(p.parent_path());std::ofstream(p)<<"fixture";}
  auto utf8=[](const fs::path &p){const auto s=p.generic_u8string();return std::string(s.begin(),s.end());};
  auto encode=[](const std::string &s){std::string out;const char *hex="0123456789ABCDEF";for(const unsigned char c:s) {
    if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c==':'||c=='.'||c=='-'||c=='_')out+=char(c);
    else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
  }return out;};
  const auto scene=folder/"environment.duf";
  auto load=[&](const std::string &uri,const std::vector<fs::path> &roots) {
    const J node={{"id","environment"},{"extra",J::array({{{"type","studio/node/environment"}},
      {{"type","studio_node_channels"},{"channels",J::array({{{"channel",{{"id","Environment Map"},{"value",1},{"image_file",uri}}}}})}}})}};
    std::ofstream(scene)<<J{{"scene",{{"nodes",J::array({node})}}}}.dump();
    return daz::load(scene,{roots,false});
  };
  auto check=[&](const daz::LoadedScene &loaded,const fs::path &expected) {
    const auto canonical=fs::weakly_canonical(expected);
    require(loaded.scene.options.environment_file==canonical,"HDRI resolved to the wrong file");
    require(loaded.report.at("warnings").empty(),"Resolved HDRI still has a missing warning");
    const auto &deps=loaded.report.at("dependencies");require(std::find(deps.begin(),deps.end(),utf8(canonical))!=deps.end(),"Resolved HDRI missing from dependencies");
  };
  check(load(encode(utf8(hdr)),{library}),hdr);
  check(load("file:///"+encode(utf8(hdr)),{library}),hdr);
  check(load("/"+encode(utf8(hdr.lexically_relative(library))),{library}),hdr);
  const auto previous=fs::path(hdr.root_name()==L"Z:"?L"Y:/":L"Z:/")/hdr.relative_path();
  auto old_uri=encode(utf8(previous));check(load(old_uri,{library}),hdr);
  old_uri.replace(1,1,"%3A");check(load("/"+old_uri,{library}),hdr);
  auto windows_uri=encode(utf8(previous));std::replace(windows_uri.begin(),windows_uri.end(),'/','\\');check(load(windows_uri,{library}),hdr);
  const auto moved=daz::relocated_library_paths("H:/G1/Runtime/Textures/Test/sky.hdr",{"G:/G3","G:/g1/"});
  require(moved.size()==1&&moved[0]==fs::path("G:/g1/Runtime/Textures/Test/sky.hdr"),"Drive relocation chose the wrong content library");
  require(daz::relocated_library_paths("H:/G10/sky.hdr",{"G:/G1"}).empty(),"Drive relocation matched a partial directory name");
  require(daz::relocated_library_paths("H:/G1/../Other/sky.hdr",{"G:/G1"}).empty(),"Drive relocation escaped the library");
  const auto missing=load("H:/UnknownLibrary/sky.hdr",{library});
  require(missing.scene.options.environment_file.empty()&&missing.report["warnings"][0]["code"]=="environment_map_missing","Missing HDRI was silently replaced");
  // A process-local override simulates a nonstandard DAZ installation without editing the registry.
  struct Override {
    std::wstring previous;
    explicit Override(const fs::path &p) {
      const auto size=GetEnvironmentVariableW(L"DFV_DAZ_STUDIO_PATH",nullptr,0);
      if(size){previous.resize(size);previous.resize(GetEnvironmentVariableW(L"DFV_DAZ_STUDIO_PATH",previous.data(),size));}
      require(SetEnvironmentVariableW(L"DFV_DAZ_STUDIO_PATH",p.c_str()),"Cannot set test installation");
    }
    ~Override(){SetEnvironmentVariableW(L"DFV_DAZ_STUDIO_PATH",previous.empty()?nullptr:previous.c_str());}
  } override(installation);
  const auto automatic=load("/resources/dfv-test-sky.hdr",{library});check(automatic,builtin);
  require(automatic.report["content_roots"].size()==1&&!automatic.report["iray_resource_roots"].empty(),"Iray roots leaked into the content library list");
  check(load("/resources/dfv-test-sky.hdr",{iray}),builtin);
  const auto custom=library/"resources/dfv-test-sky.hdr";fs::create_directories(custom.parent_path());std::ofstream(custom)<<"custom";
  check(load("/resources/dfv-test-sky.hdr",{library}),custom);
  const auto absent=load("/resources/dfv-test-missing.hdr",{library});
  require(absent.scene.options.environment_file.empty()&&absent.report["warnings"][0]["code"]=="environment_map_missing","Missing built-in HDRI was not reported");
}
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
  {editor::RenderEditQueue queue;ir::Delta first;first.visibility={{0,false},{1,true}};first.meshes={{0,{{1,2,3}}}};queue.merge(first);
    ir::Delta last;last.visibility={{0,true}};last.meshes={{0,{{4,5,6}}}};queue.merge(last,true);
    require(queue.pending&&queue.synchronize&&queue.delta.visibility.size()==2&&queue.delta.visibility[0].visible&&queue.delta.visibility[1].visible&&queue.delta.meshes.size()==1&&queue.delta.meshes[0].positions[0].x==4,"等待期间丢失显隐或最新几何");queue.clear();require(!queue.pending&&!queue.synchronize&&queue.delta.meshes.empty(),"提交后残留过期编辑");}

  const auto folder=std::filesystem::temp_directory_path()/"dfv-render-options";std::filesystem::create_directories(folder);
  environment_paths(folder);
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
  camera.distance=320000;camera.dolly(1);require(camera.distance>280000&&camera.distance<320000,"大场景聚焦后滚轮突然跳回 2000 米");
  ir::OptionNode solar;for(const auto &[id,value]:std::vector<std::pair<std::string,double>>{{"SS Day",2458929},{"SS Time",43200},{"SS UTC Offset",0},{"SS Latitude",0},{"SS Longitude",0}}) {ir::Option p;p.id=id;p.value={value};solar.parameters.push_back(p);}
  const auto noon=ir::solar_direction(solar);require(noon.z>.998f,"春分赤道正午太阳方向错误");solar.parameters[1].value={0};require(ir::solar_direction(solar).z<-.998f,"午夜太阳方向错误");
  solar.parameters[1].value={43200};solar.parameters[4].value={90};require(std::abs(ir::solar_direction(solar).z)<.05f,"经度没有改变太阳时角");
  runtime::TransformValues transform;transform.general_scale=2;require(runtime::make_transform(transform).point({1,0,0}).x==2,"总体缩放未应用");
  std::cout<<"render options / persistence / exposure / camera: PASS\n";return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<std::endl;return 1;}}
