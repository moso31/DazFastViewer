#include "editor/document.h"
#include "daz/pose.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

using namespace dfv;
using namespace dfv::runtime;
using Json=nlohmann::json;
static void require(bool condition,const char *message) {if(!condition) throw std::runtime_error(message);}
template<class F> static void rejects(F operation,const char *message) {bool rejected=false;try {operation();} catch(const std::exception &) {rejected=true;}require(rejected,message);}
static double distance(const std::vector<ir::Vec3> &a,const std::vector<ir::Vec3> &b) {require(a.size()==b.size(),"顶点数量变化");double result=0;for(size_t i=0;i<a.size();++i) result=std::max(result,std::sqrt(std::pow(a[i].x-b[i].x,2)+std::pow(a[i].y-b[i].y,2)+std::pow(a[i].z-b[i].z,2)));return result;}
static size_t parameter(const Target &target,const std::string &id) {for(size_t m=0;m<target.morphs.size();++m) if(target.morphs[m].channel_id==id&&target.morphs[m].kind!="alias"&&target.morphs[m].evaluable) return m;throw std::runtime_error("缺少参数："+id);}
static Morph morph(std::string id,std::vector<SparseOffset> offsets,bool follow=true) {Morph m;m.id=m.channel_id=id;m.offsets=std::move(offsets);m.evaluable=true;m.auto_follow=follow;return m;}
static FormulaGraph graph(const Target &t,int skin) {
  FormulaGraph g;g.skin=skin;
  for(size_t m=0;m<t.morphs.size();++m) {Channel c;c.binding={Property::morph,uint32_t(m),0};g.morph_channels.push_back(int(g.channels.size()));g.channels.push_back(c);}
  if(skin>=0) for(auto property:{Property::rotation,Property::scale,Property::center}) {Channel c;c.binding={property,1,property==Property::rotation?2u:0u};c.initial=property==Property::scale?1:0;g.channels.push_back(c);}
  g.prepare();return g;
}
static void generated_field() {
  ir::Mesh body;body.positions={{-2,-2,0},{0,-2,0},{0,2,0},{-2,2,0},{0,-2,0},{2,-2,0},{2,2,0},{0,2,0}};
  for(const auto vertices:std::array<std::array<uint32_t,3>,4>{{{0,1,2},{0,2,3},{4,5,6},{4,6,7}}}) {ir::Triangle f;f.vertices=vertices;body.triangles.push_back(f);}
  ir::Mesh cloth;for(int y=0;y<7;++y) for(int x=0;x<9;++x) cloth.positions.push_back({(x-4)*.2f,(y-3)*.2f,.1f+(x%2)*.01f});
  for(uint32_t y=0;y<6;++y) for(uint32_t x=0;x<8;++x) {auto a=y*9+x;ir::Triangle f;f.vertices={a,a+1,a+10};cloth.triangles.push_back(f);f.vertices={a,a+10,a+9};cloth.triangles.push_back(f);}
  ir::Scene scene;scene.meshes={body,cloth};scene.instances.resize(2);scene.instances[1].mesh=1;
  Target source;source.id="body/mesh";source.morphs={morph("follow",{})};
  for(uint32_t v=0;v<8;++v) source.morphs[0].offsets.push_back({v,{0,0,v<4?.1f:-.1f}});
  Target follower;follower.id="cloth/mesh";follower.instance=1;follower.conform_target="#body";
  std::vector<Target> targets={source,follower};std::vector<FormulaGraph> graphs={graph(source,-1),graph(follower,-1)};
  const std::vector<Skin> skins;DeformationRuntime runtime(scene,targets,skins,graphs);std::vector<Properties> values(2);values[0].morphs={1};runtime.evaluate(values,{});
  const auto &positions=scene.meshes[1].positions;float jump=0;
  for(size_t y=1;y<6;++y) for(size_t x=1;x<8;++x) {const auto v=y*9+x;const float a=positions[v].z-cloth.positions[v].z,b=positions[v-1].z-cloth.positions[v-1].z;jump=std::max(jump,std::abs(a-b));}
  require(jump<.1f,"最近表面切换仍使衣物产生不连续位移");
  values[0].morphs[0]=0;runtime.evaluate(values,{});require(distance(scene.meshes[1].positions,cloth.positions)==0,"生成场的平滑改变了原始褶皱或重置结果");
}
static void bone_attachment() {
  ir::Scene base;ir::Mesh mesh;mesh.positions={{0,0,0}};mesh.material_slots={"surface"};base.materials.resize(1);
  for(uint32_t i=0;i<4;++i) {base.meshes.push_back(mesh);ir::Instance instance;instance.mesh=i;instance.materials={0};instance.transform=ir::Transform::translate({i==0?2.f:i==1?3.f:i==2?3.5f:8.f,0,0});base.instances.push_back(instance);}
  Target body;body.id="body/mesh";body.morphs={morph("head_size",{},false)};
  Target glasses;glasses.id="glasses/mesh";glasses.instance=1;glasses.parent="#head-instance";
  Target child;child.id="child/mesh";child.instance=2;child.parent="#glasses";
  Target other;other.id="other/mesh";other.instance=3;
  std::vector<Target> targets={body,glasses,child,other};Skin skin;skin.id="body";
  Joint root,head;root.id="body";head.id="head";head.scene_id="head-instance";head.parent=0;skin.joints={root,head};skin.initial.resize(2);skin.weights={{{1,1}}};
  std::vector<Skin> skins={skin};std::vector<FormulaGraph> graphs={graph(body,0),graph(glasses,-1),graph(child,-1),graph(other,-1)};
  Expression scale;scale.output=2;scale.linear_input=0;scale.coefficient=1;graphs[0].expressions={scale};graphs[0].prepare();
  auto scene=base;DeformationRuntime runtime(scene,targets,skins,graphs);std::vector<Properties> values(4);values[0].morphs={1};auto poses=std::vector<std::vector<JointPose>>{skin.initial};poses[0][1].rotation_degrees.z=90;
  runtime.evaluate(values,poses);
  auto close=[&](size_t i,ir::Vec3 expected) {return distance({scene.instances[i].transform.point({})},{expected})<1e-6;};
  require(close(1,{2,0,2})&&close(2,{2,0,3})&&close(3,{8,0,0}),"骨骼附件未跟随 ERC / 姿态，或污染无关对象");
  for(int i=0;i<3;++i) require(runtime.evaluate(values,poses).instances.empty(),"附件重复求值产生更新或累计漂移");
  values[0].transform.translation_cm.x=10;runtime.evaluate(values,poses);require(close(1,{2.1f,0,2})&&close(2,{2.1f,0,3}),"附件未继承角色的手动实例变换");
  values[0].morphs={0};values[0].transform={};poses[0]=skin.initial;runtime.evaluate(values,poses);require(close(1,{3,0,0})&&close(2,{3.5f,0,0}),"骨骼附件重置不能恢复绑定位置");
  // 初始场景已保存骨骼姿态：运行时不能重复应用这一次旋转。
  skins[0].initial[1].rotation_degrees.z=30;poses[0]=skins[0].initial;auto saved=base;DeformationRuntime posed(saved,targets,skins,graphs);posed.evaluate(values,poses);
  require(distance({saved.instances[1].transform.point({})},{{3,0,0}})<1e-6,"附件重复应用场景的初始骨骼姿态");
  skins[0]=skin;editor::Document document;document.generation=9;document.loaded.scene=base;document.catalog.targets=targets;document.skeletons.skins=skins;document.formulas.graphs=graphs;auto duplicate=document;
  editor::append_document(document,std::move(duplicate));auto snapshot=editor::initial_snapshot(document);snapshot.poses[1][1].rotation_degrees.z=90;
  DeformationRuntime appended(document.loaded.scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);appended.evaluate(snapshot.values,snapshot.poses);
  require(distance({document.loaded.scene.instances[1].transform.point({}),document.loaded.scene.instances[5].transform.point({})},{{3,0,0},{2,0,1}})<1e-6,"追加角色的骨骼附件串到已有角色");
}
static void unit() {
  bone_attachment();
  generated_field();
  ir::Mesh body;body.positions={{0,0,0},{1,0,0},{0,1,0}};ir::Triangle triangle;triangle.vertices={0,1,2};body.triangles={triangle};body.material_slots={"surface"};
  auto cloth=body;cloth.positions={{.25f,.25f,.1f},{.5f,.25f,.1f},{.25f,.5f,.1f}};
  ir::Scene scene;scene.meshes={cloth,body,body};scene.materials.resize(1);for(uint32_t m=0;m<3;++m) {ir::Instance instance;instance.mesh=m;instance.materials={0};scene.instances.push_back(instance);}
  Target source;source.id="person/geometry";source.instance=1;source.morphs={morph("built_in",{{0,{.1f,0,0}},{1,{.1f,0,0}},{2,{.1f,0,0}}}),morph("generated",{{1,{0,0,.4f}}}),morph("no_follow",{{0,{0,.5f,0}},{1,{0,.5f,0}},{2,{0,.5f,0}}},false)};
  Target follower;follower.id="cloth/geometry";follower.instance=0;follower.conform_target="#person";follower.parent="#person";follower.morphs={morph("built_in",{{0,{.2f,0,0}},{1,{.2f,0,0}},{2,{.2f,0,0}}})};
  Target unrelated=source;unrelated.id="other/geometry";unrelated.instance=2;
  std::vector<Target> targets={follower,source,unrelated};std::vector<Skin> skins;
  for(uint32_t i=0;i<2;++i) {Skin skin;skin.instance=i;Joint root;root.id=i==0?"cloth":"person";Joint joint;joint.id="chest";joint.parent=0;skin.joints={root,joint};skin.initial.resize(2);skin.weights={{{1,1}},{{1,1}},{{1,1}}};skins.push_back(skin);}
  std::vector<FormulaGraph> graphs={graph(follower,0),graph(source,1),graph(unrelated,-1)};
  std::vector<Properties> values(3);values[0].morphs={0};values[1].morphs=values[2].morphs={0,0,0};std::vector<std::vector<JointPose>> poses={skins[0].initial,skins[1].initial};
  DeformationRuntime runtime(scene,targets,skins,graphs);runtime.evaluate(values,poses);
  require(runtime.conform_links().size()==1,"没有按显式 conform_target 建立关系");
  values[1].morphs={1,1,1};runtime.evaluate(values,poses);auto expected=cloth.positions;
  expected[0].x+=.2f;expected[1].x+=.2f;expected[2].x+=.2f;expected[0].z+=.1f;expected[1].z+=.2f;expected[2].z+=.1f;
  for(auto &p:expected) {p.x-=.04f/std::sqrt(1.16f);p.z+=.1f/std::sqrt(1.16f)-.1f;}
  require(distance(scene.meshes[0].positions,expected)<1e-6,"原生 Morph 优先 / 重心转移 / auto_follow 隔离错误");
  require(distance(scene.meshes[2].positions,body.positions)==0,"Morph 污染其他角色");
  auto stats=runtime.conform_stats();require(runtime.evaluate(values,poses).meshes.empty()&&runtime.conform_stats().evaluations==stats.evaluations,"相同快照重复跟随求值");
  poses[1][1].rotation_degrees.z=90;runtime.evaluate(values,poses);require(distance(scene.meshes[0].positions,deform(skins[0],poses[1],expected))<1e-6,"Morph 跟随后没有正确继承骨骼姿势");
  poses[1][1].scale.x=1.5f;poses[1][1].center_offset_cm.x=10;runtime.evaluate(values,poses);require(distance(scene.meshes[0].positions,deform(skins[0],poses[1],expected))<1e-6,"骨长 / 中心调整未传给服装");
  poses[0][1].rotation_degrees.z=10;runtime.evaluate(values,poses);require(runtime.effective_poses()[0][1].rotation_degrees.z==100,"服装局部姿势调整丢失");
  values[0].morphs[0]=-.2f;runtime.evaluate(values,poses);require(std::abs(runtime.effective()[0][0]-.8f)<1e-6,"服装自己的 Morph 调整丢失");
  values[0].morphs[0]=0;values[1].morphs={0,0,0};poses={skins[0].initial,skins[1].initial};runtime.evaluate(values,poses);require(distance(scene.meshes[0].positions,cloth.positions)==0,"重置后服装产生累计漂移");
  auto cyclic=targets;cyclic[1].conform_target="#cloth";rejects([&] {ConformRuntime bad(scene,cyclic,skins,graphs);},"Fit To 循环未拒绝");
  auto missing=targets;missing[0].conform_target="#missing";rejects([&] {ConformRuntime bad(scene,missing,skins,graphs);},"丢失 Fit To 对象仍静默显示错误形变");
  auto parent_only=targets;parent_only[0].conform_target.clear();ConformRuntime unbound(scene,parent_only,skins,graphs);require(unbound.links().empty(),"普通父子关系被误当服装绑定");
  auto renamed=skins;renamed[0].joints[1].name="Chest";renamed[1].joints[1].name="Different";auto alternate=renamed[1].joints[1];alternate.id="chest-renamed";alternate.name="Chest";renamed[1].joints.push_back(alternate);renamed[1].initial.emplace_back();
  ConformRuntime named(scene,targets,renamed,graphs);require(named.link(0)->joints[1]==2,"跨资产的局部 ID 抢占了唯一名称匹配");
  renamed[1].joints[1].name="Chest";ConformRuntime disambiguated(scene,targets,renamed,graphs);require(disambiguated.link(0)->joints[1]==1,"同名骨骼没有按唯一 ID 消除歧义");
  renamed[0].joints[1].id="unknown";rejects([&] {ConformRuntime ambiguous(scene,targets,renamed,graphs);},"多个同名骨骼被任意选择");
  // 不同实例矩阵：在人体坐标中绑定，位移转换回服装坐标。
  scene.instances[0].transform.value[0]=2;auto transformed=cloth;for(auto &p:transformed.positions) p.x*=.5f;scene.meshes[0]=transformed;
  DeformationRuntime scaled(scene,targets,skins,graphs);values[1].morphs[1]=1;scaled.evaluate(values,poses);auto scaled_expected=transformed.positions;scaled_expected[0].z+=.1f;scaled_expected[1].z+=.2f;scaled_expected[2].z+=.1f;
  for(auto &p:scaled_expected) {p.x-=.02f/std::sqrt(1.16f);p.z+=.1f/std::sqrt(1.16f)-.1f;}
  require(distance(scene.meshes[0].positions,scaled_expected)<1e-6,"绑定没有处理相对实例矩阵");
  editor::Document document;document.generation=5;document.loaded.scene=scene;document.catalog.targets=targets;document.skeletons.skins=skins;document.formulas.graphs=graphs;auto duplicate=document;
  editor::append_document(document,std::move(duplicate));ConformRuntime merged(document.loaded.scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);require(merged.links().size()==2&&merged.link(3)->source==4,"追加场景后服装串到原角色");
  // 角色 ERC 产生的最终修正也要传递，不能只复制用户滑块输入。
  scene.instances[0].transform={};scene.meshes={cloth,body,body};values[1].morphs={0,0,0};
  Expression correction;correction.output=0;correction.linear_input=3;correction.coefficient=1.0/90;graphs[1].expressions={correction};graphs[1].prepare();
  DeformationRuntime driven(scene,targets,skins,graphs);poses[1][1].rotation_degrees.z=45;driven.evaluate(values,poses);require(driven.effective()[0][0]==.5f,"角色 ERC 最终修正未驱动原生服装 Morph");
  auto corrected=cloth.positions;for(auto &p:corrected) p.x+=.1f;require(distance(scene.meshes[0].positions,deform(skins[0],poses[1],corrected))<1e-6,"ERC 原生修正重复投射或蒙皮");
  scene.meshes={cloth,body,body};correction.linear_input=1;graphs[0].expressions={correction};graphs[0].prepare();
  DeformationRuntime own_correction(scene,targets,skins,graphs);own_correction.evaluate(values,poses);require(own_correction.effective()[0][0]==.5f&&distance(scene.meshes[0].positions,deform(skins[0],poses[1],corrected))<1e-6,"服装自带 ERC 与人体修正被重复叠加");
  std::cout<<"Conform authored / barycentric / pose / scale / reset / isolation: PASS\n";
}
static void actual(const std::filesystem::path &file,const std::filesystem::path &output,const std::filesystem::path &shape) {
  const std::vector<std::filesystem::path> roots={L"H:/G1",L"H:/G3",L"C:/Users/Public/Documents/My DAZ 3D Library",L"C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};
  editor::Document document;document.loaded=daz::load(file,{roots,false});std::cout<<"geometry\n"<<std::flush;
  document.catalog=daz::discover_morphs(document.loaded,roots,[](const std::string &s) {std::cout<<s<<std::endl;});document.skeletons=daz::load_skeletons(document.loaded);document.formulas=daz::enable_formulas(document.catalog,document.skeletons);
  auto snapshot=editor::initial_snapshot(document);auto scene=document.loaded.scene;DeformationRuntime runtime(scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);
  const auto original=snapshot;const auto baseline=scene.meshes;Json report={{"status","PASS"},{"links",Json::array()},{"cases",Json::array()}};
  for(const auto &l:runtime.conform_links()) report["links"].push_back({{"follower",document.catalog.targets[l.follower].id},{"source",document.catalog.targets[l.source].id},{"authored",std::count_if(l.morph_sources.begin(),l.morph_sources.end(),[](int m) {return m>=0;})},{"fallback_morphs",l.projected_morphs.size()},{"bound_vertices",l.surface.size()}});
  require(runtime.conform_links().size()==10,"test2 的 10 个穿戴物没有完整绑定");
  auto mesh_for=[&](size_t t) {return scene.instances[document.catalog.targets[t].instance].mesh;};
  auto check=[&](const char *name,bool require_clothes) {runtime.evaluate(snapshot.values,snapshot.poses);Json entry={{"name",name},{"displacements_m",Json::object()}};bool any=false;
    for(const auto &l:runtime.conform_links()) for(size_t m=0;m<l.morph_sources.size();++m) if(l.morph_sources[m]>=0) {const auto &morph=document.catalog.targets[l.follower].morphs[m];float expected=snapshot.values[l.follower].morphs[m]+runtime.effective()[l.source][size_t(l.morph_sources[m])];if(morph.clamped) expected=std::clamp(expected,morph.minimum,morph.maximum);require(std::abs(runtime.effective()[l.follower][m]-expected)<1e-6,"真实服装原生 Morph 没有接收目标最终值");}
    for(size_t t=0;t<document.catalog.targets.size();++t) {const double d=distance(baseline[mesh_for(t)].positions,scene.meshes[mesh_for(t)].positions);entry["displacements_m"][document.catalog.targets[t].id]=d;
      if(t<=2) {if(t==1||t==2) any|=d>1e-6;} else require(d==0,"操作角色 0 污染了其他角色或穿戴物");}
    if(require_clothes) require(any,"真实体型或姿势没有改变服装");report["cases"].push_back(entry);std::cout<<entry.dump()<<std::endl;
    require(runtime.evaluate(snapshot.values,snapshot.poses).meshes.empty(),"真实场景重复求值产生无效更新");
    snapshot=original;runtime.evaluate(snapshot.values,snapshot.poses);for(size_t m=0;m<scene.meshes.size();++m) require(distance(baseline[m].positions,scene.meshes[m].positions)==0,"真实场景归零不能精确恢复");};
  const auto &body=document.catalog.targets[0];
  for(const auto id:{"FBMBodybuilderSize","PBMBreastsSize","SCLArmsLength"}) {set_parameter(body,snapshot.values[0],parameter(body,id),.6f);check(id,true);}
  const auto &body_skin=document.skeletons.skins[0];size_t chest=0;while(chest<body_skin.joints.size()&&body_skin.joints[chest].id!="chestLower"&&body_skin.joints[chest].name!="chestLower") ++chest;require(chest<body_skin.joints.size(),"没有 chestLower 骨骼");
  snapshot.poses[0][chest].rotation_degrees.x=25;set_parameter(body,snapshot.values[0],parameter(body,"FBMBodybuilderSize"),.5f);check("shape_and_chest_pose",true);
  if(!shape.empty()) {auto applied=daz::apply_pose(daz::read_pose(shape),body_skin,snapshot.poses[0],body,snapshot.values[0]);snapshot.poses[0]=applied.joints;snapshot.values[0]=applied.properties;report["shape_preset"]=applied.report;check("shape_preset",false);}
  const auto &stats=runtime.conform_stats();report["stats"]={{"bindings",stats.bindings},{"authored_morphs",stats.authored_morphs},{"evaluations",stats.evaluations},{"projected_vertices",stats.projected_vertices}};
  std::filesystem::create_directories(output.parent_path());std::ofstream(output)<<report.dump(2);std::cout<<"Real scene conform: PASS\n";
}
int wmain(int argc,wchar_t **argv) {try {if(argc>2) actual(argv[1],argv[2],argc>3?std::filesystem::path(argv[3]):std::filesystem::path{});else unit();return 0;} catch(const std::exception &e) {std::cerr<<e.what()<<std::endl;return 1;}}
