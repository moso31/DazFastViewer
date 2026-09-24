#include "editor/document.h"
#include "daz/content_entry.h"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace dfv;
using J=nlohmann::json;
namespace fs=std::filesystem;
static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
template<class F> static void rejects(F f,const char *message) {bool rejected=false;try {f();}catch(const std::exception &){rejected=true;}require(rejected,message);}
static editor::Document load(const fs::path &file,const std::vector<fs::path> &roots,bool deferred=false) {
  editor::Document d;d.loaded=daz::load(file,{roots,false,deferred});
  d.catalog=daz::discover_morphs(d.loaded,roots);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);return d;
}
static ir::Scene evaluate(const editor::Document &d,const editor::Snapshot &s) {
  auto scene=d.loaded.scene;runtime::DeformationRuntime r(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);r.evaluate(s.values,s.poses);
  for(const auto &seam:r.graft_seams()) require(seam.max_gap_m<1e-5,"附件接缝未对齐");return scene;
}
static ir::Vec3 point(const ir::Scene &scene,size_t instance,size_t vertex=0) {const auto &i=scene.instances.at(instance);return i.transform.point(scene.meshes.at(i.mesh).positions.at(vertex));}
static double distance(ir::Vec3 a,ir::Vec3 b) {return std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z);}
static void unit() {
  const auto folder=fs::temp_directory_path()/("dfv-wear-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(folder/"data/Body/Morphs");fs::create_directories(folder/"data/Wear");
  const auto entry=folder/"People/Genesis 8 Female/Anatomy/3feetwolf/HD Nipples - 2.0/HD Nipples for G8F - 2.0.dse";
  fs::create_directories(entry.parent_path());std::ofstream(entry)<<"opaque script, never executed";
  const auto backing=folder/"data/3feetwolf/HD Nipples for G8F - 2.0/HD Nipples - 2.0/HD Nipples for G8F - 2.0.duf";
  fs::create_directories(backing.parent_path());std::ofstream(backing)<<"{}";
  require(daz::supported_content_entry(entry)&&daz::content_asset(entry,{folder})==backing,"产品入口没有解析到基础 DUF");
  rejects([&]{daz::content_asset(folder/"unknown.dse",{folder});},"任意 DAZ 脚本被错误接受");
  const auto base=J::parse(R"({"node_library":[{"id":"figure","type":"figure","presentation":{"type":"Actor/Character","auto_fit_base":"/Genesis 8.1/Female","extended_bases":["/Genesis 8/Female"]}},{"id":"bone","name":"head","type":"bone","parent":"#figure"}],
    "geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[10,0,0],[100,0,0],[0,100,0]]},"polygon_material_groups":{"count":1,"values":["Skin"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],
    "uv_set_library":[{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}],
    "modifier_library":[{"skin":{"node":"#figure","geometry":"#mesh","vertex_count":3,"joints":[{"node":"#bone","node_weights":{"count":3,"values":[[0,1],[1,1],[2,1]]}}]}}]})");
  std::ofstream(folder/"data/Body/body.dsf")<<base;
  std::ofstream(folder/"data/Body/Morphs/raise.dsf")<<R"({"modifier_library":[{"id":"Raise","parent":"/data/Body/body.dsf#mesh","channel":{"type":"float","value":0,"min":0,"max":1,"auto_follow":true},"morph":{"vertex_count":3,"deltas":{"count":1,"values":[[1,20,0,0]]}}}]})";
  auto wear=base;wear["node_library"][0]["presentation"]={{"type","Follower/Attachment"},{"preferred_base","/Genesis 8/Female"}};
  wear["geometry_library"][0]["graft"]={{"vertex_count",3},{"poly_count",1},{"vertex_pairs",{{"count",2},{"values",{{0,0},{1,1}}}}},{"hidden_polys",{{"count",1},{"values",{0}}}}};
  std::ofstream(folder/"data/Wear/wear.dsf")<<wear;
  auto prop=base;prop["node_library"][0]["type"]="node";prop["node_library"][0]["presentation"]={{"type","Follower/Accessory"}};prop["modifier_library"]=J::array();
  std::ofstream(folder/"data/Wear/prop.dsf")<<prop;
  J body={{"scene",{{"nodes",J::array()},{"materials",J::array()}}}};
  for(int n=0;n<2;++n) {
    const auto id="body"+std::to_string(n),geom="geom"+std::to_string(n);
    body["scene"]["nodes"].push_back({{"id",id},{"url","/data/Body/body.dsf#figure"},{"translation",{{{"id","x"},{"current_value",n?200:70}}}},{"geometries",{{{"id",geom},{"url","/data/Body/body.dsf#mesh"}}}}});
    body["scene"]["nodes"].push_back({{"id","head"+std::to_string(n)},{"url","/data/Body/body.dsf#bone"},{"parent","#"+id},{"rotation",{{{"id","z"},{"current_value",n?0:30}}}}});
    body["scene"]["materials"].push_back({{"id","mat"+std::to_string(n)},{"geometry","#"+geom},{"groups",{"Skin"}}});
  }
  std::ofstream(folder/"body.duf")<<body;
  J preset={{"asset_info",{{"type","wearable"}}},{"scene",{{"nodes",J::array()},{"materials",J::array()}}}};
  preset["scene"]["nodes"].push_back({{"id","cloth"},{"url","/data/Wear/wear.dsf#figure"},{"parent","name://@selection:"},{"conform_target","name://@selection:"},{"geometries",{{{"id","clothgeom"},{"url","/data/Wear/wear.dsf#mesh"}}}}});
  // 无网格 Group 下面的饰品通过 @selection/head 挂到已保存姿势的角色。
  preset["scene"]["nodes"].push_back({{"id","decorations"},{"type","node"},{"parent","name://@selection/head:"}});
  preset["scene"]["nodes"].push_back({{"id","clip"},{"url","/data/Wear/prop.dsf#figure"},{"parent","#decorations"},{"geometries",{{{"id","clipgeom"},{"url","/data/Wear/prop.dsf#mesh"}}}}});
  for(const auto *name:{"cloth","clip"}) preset["scene"]["materials"].push_back({{"id",std::string(name)+"mat"},{"geometry","#"+std::string(name)+"geom"},{"groups",{"Skin"}}});
  preset["modifier_library"]={{{"id","smoothing"},{"extra",{{{"type","studio/modifier/smoothing"}}}}}};
  preset["scene"]["modifiers"]={{{"id","smooth"},{"url","#smoothing"},{"parent","#cloth"},{"extra",{{{"type","studio_modifier_channels"},{"channels",{{{"channel",{{"id","Collision Item"},{"type","node"},{"node","name://@selection:"}}}}}}}}}}};
  std::ofstream(folder/"wear.duf")<<preset;
  require(daz::inspect_contents(preset).requires_selection,"未发现穿戴选择引用");
  auto alias_only=body;alias_only["scene"]["modifiers"]={{{"formulas",{{{"output","name://@selection#Control:?value"}}}}}};
  require(!daz::inspect_contents(alias_only).requires_selection,"普通场景中的参数地址被误当成穿戴目标");
  rejects([&]{daz::load(folder/"wear.duf",{{folder},false});},"没有目标上下文时静默加载了附件");
  auto d=load(folder/"body.duf",{folder});const auto original=d;auto source=load(folder/"wear.duf",{folder},true);
  require(source.loaded.objects[0].preferred_base=="/Genesis 8/Female","未保留兼容基型");
  d.generation=2;editor::append_document(d,std::move(source));editor::attach_import(d,2,0);
  require(d.catalog.targets[2].conform_target=="#body0"&&d.catalog.targets[2].smoothing.collision_target=="#body0","Fit To / 碰撞目标没有解析到已有角色");
  require(editor::attachment_host(d,3)==0,"从刚性附件无法回溯所属角色");
  require(d.loaded.scene.meshes.at(d.loaded.scene.instances[0].mesh).hidden_polygons.size()==1&&d.loaded.scene.meshes.at(d.loaded.scene.instances[1].mesh).hidden_polygons.empty(),"GeoGraft 遮盖串到另一角色");
  auto snapshot=editor::initial_snapshot(d);snapshot.values[0].transform.translation_cm.x=15;
  auto scene=evaluate(d,snapshot);
  require(distance(point(scene,0),point(scene,2))<1e-5&&distance(point(scene,0),point(scene,3))<1e-5,"已移动 / 摆姿势的角色穿戴后附件位置错误");
  snapshot.poses[0][1].rotation_degrees.z=63;snapshot.values[0].morphs[0]=.6f;scene=evaluate(d,snapshot);
  require(distance(point(scene,0,1),point(scene,2,1))<1e-5,"新附件没有随 Morph / 姿势变化");
  const auto unaffected=point(scene,1);editor::fit_attachment(d,2,1);scene=evaluate(d,snapshot);
  require(d.catalog.targets[2].conform_target=="#body1"&&distance(point(scene,1),point(scene,2))<1e-5&&distance(point(scene,1),point(scene,3))<1e-5,"整套更换目标错误");
  require(distance(point(scene,1),unaffected)==0,"附件更换目标修改了人体");
  require(d.loaded.scene.meshes.at(d.loaded.scene.instances[0].mesh).hidden_polygons.empty(),"更换目标没有恢复原宿主表面");
  editor::fit_attachment(d,3,-1);scene=evaluate(d,snapshot);
  require(d.catalog.targets[2].conform_target.empty()&&d.loaded.scene.meshes.at(d.loaded.scene.instances[1].mesh).hidden_polygons.empty(),"解除挂接保留旧接缝或遮盖");
  editor::fit_attachment(d,2,0);auto refreshed=editor::refresh_parameters(d,0,{folder});
  require(refreshed->catalog.targets[2].conform_target=="#body0","刷新参数丢失新绑定");
  auto invalid=d;invalid.loaded.objects[0].auto_fit_base="/Genesis 8/Male";invalid.loaded.objects[0].extended_bases.clear();
  rejects([&]{editor::fit_attachment(invalid,2,0);},"允许了不兼容性别的 AutoFit");
  rejects([&]{auto copy=d;editor::fit_attachment(copy,2,2);},"附件被误用作宿主");
  rejects([&]{auto copy=d;copy.generation=3;const auto first=copy.catalog.targets.size();editor::append_document(copy,load(folder/"wear.duf",{folder},true));editor::attach_import(copy,first,0);},"重叠 GeoGraft 没有拒绝");
  auto saved=d;saved.attachments.clear();editor::fit_attachment(saved,3,1);
  auto saved_snapshot=editor::initial_snapshot(saved);auto saved_scene=evaluate(saved,saved_snapshot);
  require(distance(point(saved_scene,1),point(saved_scene,3))<1e-5,"已有场景中的骨骼附件更换目标失败");
  auto missing_bone_scene=original;missing_bone_scene.skeletons.skins[0].joints[1].scene_id.clear();
  missing_bone_scene.generation=3;editor::append_document(missing_bone_scene,load(folder/"wear.duf",{folder},true));editor::attach_import(missing_bone_scene,2,0);
  evaluate(missing_bone_scene,editor::initial_snapshot(missing_bone_scene));
  auto hair=load(folder/"wear.duf",{folder},true);
  hair.loaded.objects[0].parent="#clip";hair.loaded.objects[1].parent="name://@selection:";hair.loaded.objects[1].conform_target="#cloth";
  for(auto &n:hair.loaded.nodes) {if(n.id=="cloth") n.parent="#clip";if(n.id=="clip") n.parent="name://@selection:";}
  auto hair_document=original;hair_document.generation=4;editor::append_document(hair_document,std::move(hair));editor::attach_import(hair_document,2,0);
  editor::fit_attachment(hair_document,2,-1);evaluate(hair_document,editor::initial_snapshot(hair_document));
  editor::fit_attachment(hair_document,3,0);evaluate(hair_document,editor::initial_snapshot(hair_document));
  auto rigid=preset;rigid["scene"]["nodes"][2]["parent"]="name://@selection:";
  rigid["scene"]["nodes"][2]["extra"]={{{"type","studio/node/rigid_follow"},{"vertex_count",3},{"rigidity_group",{{"rotation_mode","full"},{"scale_modes",{"none","none","none"}},{"reference_vertices",{{"count",3},{"values",{0,1,2}}}}}}},
    {{"type","studio_node_channels"},{"channels",{{{"channel",{{"id","Follow Target"},{"type","node"},{"node","name://@selection:"}}}}}}}};
  std::ofstream(folder/"rigid.duf")<<rigid;
  auto rigid_document=original;rigid_document.generation=5;editor::append_document(rigid_document,load(folder/"rigid.duf",{folder},true));editor::attach_import(rigid_document,2,0);
  require(rigid_document.catalog.targets[3].rigid_follow.target=="#body0","表面刚性跟随目标没有解析");
  evaluate(rigid_document,editor::initial_snapshot(rigid_document));
  auto deleting=d;auto deleting_snapshot=editor::initial_snapshot(deleting);editor::remove_target(deleting,deleting_snapshot,2);
  require(deleting.attachments.size()==1&&deleting.attachments[0].items.size()==1,"删除单件留下失效套装记录");
  editor::fit_attachment(deleting,2,1);
  require(original.catalog.targets.size()==2&&original.loaded.scene.meshes.at(original.loaded.scene.instances[0].mesh).hidden_polygons.empty(),"待提交修改污染了原文档");
  std::cout<<"Attachment import / posed host / GeoGraft / rigid hair / rebind / detach / isolation: PASS\n";
  // 测试只删除本次创建的唯一临时目录。
  const auto resolved=fs::weakly_canonical(folder),temporary=fs::weakly_canonical(fs::temp_directory_path());
  require(resolved.parent_path()==temporary&&resolved.filename().string().starts_with("dfv-wear-"),"拒绝删除非测试临时目录");fs::remove_all(resolved);
}
static void actual(const fs::path &host,const fs::path &wear,const std::vector<fs::path> &roots) {
  std::cout<<"Loading host"<<std::endl;
  auto d=load(host,roots);size_t target=0;while(target<d.catalog.targets.size()) {try {if(editor::attachment_host(d,target)==target) break;}catch(const std::exception &){}++target;}
  require(target<d.catalog.targets.size(),"真实场景没有兼容宿主");
  std::cout<<"Loading attachment"<<std::endl;
  const auto first=d.catalog.targets.size();auto source=load(wear,roots,true);d.generation=2;editor::append_document(d,std::move(source));
  std::cout<<"Binding attachment"<<std::endl;editor::attach_import(d,first,target);
  auto snapshot=editor::initial_snapshot(d);snapshot.values[target].transform.translation_cm.x=30;
  std::cout<<"Evaluating moved host"<<std::endl;auto scene=evaluate(d,snapshot);require(scene.instances.size()>first,"真实附件未进入场景");
  std::cout<<"Detaching attachment"<<std::endl;editor::fit_attachment(d,first,-1);evaluate(d,snapshot);
  std::cout<<"Rebinding attachment"<<std::endl;editor::fit_attachment(d,first,int(target));evaluate(d,snapshot);
  std::cout<<J({{"status","PASS"},{"added_targets",d.catalog.targets.size()-first},{"attachment_groups",d.attachments.size()},{"host",d.catalog.targets[target].id}}).dump()<<std::endl;
}
int wmain(int argc,wchar_t **argv) {try {if(argc>=5&&std::wstring(argv[1])==L"--actual") {std::vector<fs::path> roots;for(int i=4;i<argc;++i) roots.emplace_back(argv[i]);actual(argv[2],argv[3],roots);}else unit();return 0;}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
