#include "editor/scene_extension.h"
#include "runtime/fit_profile.h"
#include "runtime/subdivision.h"
#include "runtime/forefoot.h"
#include "runtime/pose_edit.h"
#include "diagnostics/load_profile.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

using namespace dfv;
namespace fs=std::filesystem;
using J=nlohmann::json;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
template<class F> static void rejects(F f,const char *why){bool rejected=false;try{f();}catch(const std::exception &){rejected=true;}check(rejected,why);}
static double distance(ir::Vec3 a,ir::Vec3 b){return std::hypot(double(a.x-b.x),double(a.y-b.y),double(a.z-b.z));}
static double difference(const std::vector<ir::Vec3> &a,const std::vector<ir::Vec3> &b){check(a.size()==b.size(),"顶点数量变化");double result=0;for(size_t i=0;i<a.size();++i)result=std::max(result,distance(a[i],b[i]));return result;}
static runtime::FormulaGraph graph(const runtime::Target &t,int skin=-1){runtime::FormulaGraph g;g.skin=skin;for(size_t m=0;m<t.morphs.size();++m){runtime::Channel c;c.binding={runtime::Property::morph,uint32_t(m),0};g.morph_channels.push_back(int(g.channels.size()));g.channels.push_back(c);}g.prepare();return g;}
static void g9_unit(){
  using namespace runtime;
  check(genesis9_footwear_fit("/Genesis 9/Base","/Genesis 8/Female","Follower/Wardrobe/Footwear")&&genesis9_footwear_fit("/Genesis 9/Base","/Genesis 8.1/Female","Follower/Wardrobe/Footwear/Boots"),"遗漏 G9 鞋类适配");
  for(const auto &type:{"Follower/Wardrobe/Shirt","Follower/Hair","Follower/Wardrobe"})check(!genesis9_footwear_fit("/Genesis 9/Base","/Genesis 8/Female",type),"错误开放未经验证的 G9 类型");
  check(!genesis9_footwear_fit("/Genesis 9/Base","/Genesis 8/Male","Follower/Wardrobe/Footwear"),"错误开放 G8 男性跨代鞋类");
  Skin host,follower;host.instance=0;follower.instance=1;
  auto joint=[](const char *name,int parent,float x){Joint j;j.id=j.name=name;j.parent=parent;j.center_cm={x,3,1};j.end_cm={x+1,3,1};return j;};
  host.joints={joint("root",-1,0),joint("hip",0,1),joint("lThighTwist",1,2),joint("lShin",2,3),joint("lFoot",3,4),joint("lMetatarsals",4,5),joint("lToe",5,6)};
  follower.joints={joint("root",-1,0),joint("hip",0,2),joint("l_shin",1,5),joint("l_foot",2,7),joint("l_metatarsal",3,8),joint("l_toes",3,9)};
  follower.joints[5].rotation_order="ZYX";follower.joints[5].orientation_degrees={10,20,30};host.joints[6].orientation_degrees={-5,15,2};
  host.initial.resize(host.joints.size());follower.initial.resize(follower.joints.size());const std::vector<int> mapping={0,1,3,4,5,6};
  ir::Mesh body;body.positions={{.08f,-.04f,.005f},{.11f,-.05f,.005f},{.08f,-.07f,.02f},{.11f,-.08f,.02f}};body.polygon_groups={"lFoot","lToe"};body.triangles={{{0,1,2}},{{1,3,2}}};body.triangles[1].polygon_group=1;
  auto shoe=body;shoe.polygon_groups={"l_foot","l_toes"};follower.weights={{{3,1}},{{4,1}},{{5,1}},{{5,1}}};host.weights={{{4,1}},{{5,1}},{{6,1}},{{6,1}}};
  check(supports_forefoot(shoe,follower),"G9 鞋类未显示前掌处理参数");
  auto poses=host.initial;poses[2].rotation_degrees.z=35;poses[3].rotation_degrees.x=10;poses[4].rotation_degrees={12,8,4};poses[5].rotation_degrees.x=22;poses[6].rotation_degrees={-25,4,9};poses[0].general_scale=1.13f;
  ConformLink link;link.source_skin=0;link.follower_skin=1;link.joints=mapping;link.cross_base=link.retarget_skeleton=true;
  std::vector<Skin> skins={host,follower};std::vector<std::vector<JointPose>> resolved={poses,follower.initial};const auto inherited=conform_pose(link,skins,resolved,follower.initial);const auto matrices=joint_transforms(host,poses);
  for(auto method:{SkinMethod::linear,SkinMethod::dual_quaternion}){follower.method=method;const auto actual=deform_retargeted(follower,host,mapping,inherited,poses,shoe.positions);for(size_t v=0;v<actual.size();++v)check(distance(actual[v],matrices[size_t(mapping[follower.weights[v][0].joint])].point(shoe.positions[v]))<1e-6,"跨代蒙皮遗漏目标父骨骼或旋转顺序错误");check(difference(actual,deform(follower,inherited,shoe.positions))>.001,"测试未覆盖 G9 / G8 父子链差异");}
  auto edited=inherited;edited[5].rotation_degrees.x+=8;auto target_edit=poses;target_edit[6].rotation_degrees.x+=8;const auto moved=deform_retargeted(follower,host,mapping,edited,poses,shoe.positions);check(distance(moved[2],joint_transforms(host,target_edit)[6].point(shoe.positions[2]))<1e-6,"跨代蒙皮丢失鞋子自身骨骼编辑");
  ir::Scene scene;scene.meshes={body,shoe};scene.instances.resize(2);scene.instances[1].mesh=1;SkinningRuntime runtime(scene,skins);runtime.retarget(1,0,mapping);runtime.set_pose(0,poses);runtime.set_pose(1,inherited);runtime.evaluate();const auto prior=scene.meshes[1].positions;poses[2].rotation_degrees.z+=15;runtime.set_pose(0,poses);runtime.evaluate();check(scene.meshes[1].positions!=prior,"目标独有父骨骼动作未使鞋类缓存失效");check(runtime.evaluate().meshes.empty(),"跨代骨架重复求值漂移");
  // 同一女性同时穿 G8 男装与 G9 鞋类，不能共用同一个 Clone 缓存。
  Target a;a.id="body/mesh";a.auto_fit_base="/Genesis 8.1/Female";Morph c8;c8.id=c8.channel_id="CloneGenesis8Male";c8.evaluable=true;auto c9=c8;c9.id=c9.channel_id="CloneGenesis9";
  c9.clone_pose["l_foot"].rotation_degrees.x=30;
  for(uint32_t v=0;v<body.positions.size();++v){c8.offsets.push_back({v,{.02f,0,0}});c9.offsets.push_back({v,{.07f,0,0}});}a.morphs={c8,c9};
  Target b;b.id="male/mesh";b.instance=1;b.preferred_base="/Genesis 8/Male";b.content_type="Follower/Wardrobe/Footwear";b.conform_target="#body";auto c=b;c.id="g9/mesh";c.instance=2;c.preferred_base="/Genesis 9/Base";
  scene.meshes.push_back(shoe);scene.instances.push_back({});scene.instances[2].mesh=2;auto male=host;male.instance=1;follower.instance=2;skins={host,male,follower};std::vector<Target> targets={a,b,c};std::vector<FormulaGraph> graphs={graph(a,0),graph(b,1),graph(c,2)};
  ConformRuntime mixed(scene,targets,skins,graphs);check(std::abs(mixed.link(1)->base_offsets[0].x+.02f)<1e-6&&std::abs(mixed.link(2)->base_offsets[0].x+.07f)<1e-6,"G9 与 G8 男性参考形体串用");check(mixed.link(2)->joints==mapping&&mixed.link(2)->retarget_skeleton,"G9 骨骼映射遗漏");
  check(mixed.link(2)->prepose.size()==shoe.positions.size()&&distance(mixed.link(2)->prepose[0].point(shoe.positions[0]),shoe.positions[0])>.001,"Clone 静止姿势未应用到鞋子");
  auto no_pose=targets;no_pose[0].morphs[1].clone_pose.clear();rejects([&]{ConformRuntime r(scene,no_pose,skins,graphs);},"缺少 Clone 静止姿势仍继续错误适配");
  for(auto method:{SkinMethod::linear,SkinMethod::dual_quaternion}){
    auto skin=follower;skin.method=method;std::vector<JointPose> pose(skin.joints.size());pose[3].rotation_degrees={20,10,30};pose[4].rotation_degrees.x=18;
    const auto transforms=vertex_transforms(skin,pose);ir::Scene local;local.meshes={shoe};local.instances.resize(1);Target own;Morph adjust;adjust.evaluable=true;adjust.offsets={{0,{.01f,.02f,.03f}}};own.morphs={adjust};std::vector<Target> own_targets={own};
    MorphRuntime m(local,own_targets);m.prepose(0,transforms);m.set_morph(0,0,.5);m.evaluate();auto input=shoe.positions;input[0].x+=.005f;input[0].y+=.01f;input[0].z+=.015f;
    check(difference(local.meshes[0].positions,deform(skin,pose,input))<1e-6,"Clone 静止姿势未正确旋转鞋子自身 Morph");
    auto copied_scene=local;MorphRuntime copied(m,copied_scene);copied.set_morph(0,0,0);copied.evaluate();check(difference(copied_scene.meshes[0].positions,deform(skin,pose,shoe.positions))<1e-6,"Clone 静止姿势缓存复制或关闭 Morph 漂移");
  }
  auto missing=targets;missing[0].morphs.pop_back();auto missing_graphs=graphs;missing_graphs[0]=graph(missing[0],0);rejects([&]{ConformRuntime r(scene,missing,skins,missing_graphs);},"缺少 G9 Clone 时错误复用男性 Clone");
  auto unsupported=skins;unsupported[2].weights[0]={{1,1}};rejects([&]{ConformRuntime r(scene,targets,unsupported,graphs);},"未拒绝范围外的 G9 带权重骨骼");
  std::cout<<"G9 footwear profiles / clone pose / clone isolation / target hierarchy / own edits / cache: PASS\n";
}
static void unit(){
  using namespace runtime;
  ir::Scene base;ir::Mesh body;body.positions={{0,0,0},{1,0,0},{0,0,1}};body.triangles={{{0,2,1}}};
  auto cloth=body;for(auto &p:cloth.positions){p.x+=.2f;p.y-=.03f;}
  base.meshes={body,cloth};base.instances.resize(2);base.instances[1].mesh=1;
  Target a;a.id="body/mesh";a.auto_fit_base="/Genesis 8.1/Female";
  Morph clone;clone.id=clone.channel_id="CloneGenesis8Male";clone.evaluable=true;clone.auto_follow=true;
  Morph shape;shape.id=shape.channel_id="Shape";shape.evaluable=true;shape.auto_follow=true;
  for(uint32_t i=0;i<3;++i){clone.offsets.push_back({i,{.2f,0,0}});shape.offsets.push_back({i,{.05f,0,0}});}
  a.morphs={clone,shape};Target b;b.id="cloth/mesh";b.instance=1;b.conform_target="#body";b.preferred_base="/Genesis 8/Male";b.content_type="Follower/Wardrobe/Shirt";
  auto own=shape;for(auto &v:own.offsets)v.delta.x=.9f;b.morphs={own};
  std::vector<Target> targets{a,b};std::vector<Skin> skins;std::vector<FormulaGraph> graphs{graph(a),graph(b)};
  auto scene=base;DeformationRuntime r(scene,targets,skins,graphs);std::vector<Properties> values(2);values[0].morphs={0,1};values[1].morphs={0};r.evaluate(values,{});
  const auto *link=&r.conform_links().front();check(link->cross_base&&link->morph_sources[0]<0,"跨性别错误复用同名男性 Morph");
  auto expected=cloth.positions;for(auto &p:expected)p.x-=.15f;check(difference(scene.meshes[1].positions,expected)<1e-6,"参考形体没有转换到女性 Morph");
  const auto baseline=scene.meshes[1].positions;check(r.evaluate(values,{}).meshes.empty(),"重复求值累积转换");
  values[0].morphs[1]=0;r.evaluate(values,{});for(auto &p:expected)p.x-=.05f;check(difference(scene.meshes[1].positions,expected)<1e-6,"清除 Morph 丢失基础转换");
  values[0].morphs[1]=1;r.evaluate(values,{});check(scene.meshes[1].positions==baseline,"重新启用 Morph 漂移");
  auto fresh=base;DeformationRuntime reused(fresh,targets,skins,graphs,&r);reused.evaluate(values,{});check(fresh.meshes[1].positions==baseline,"复用绑定丢失转换");
  values[1].morphs[0]=.1f;r.evaluate(values,{});for(auto &p:expected)p.x+=.14f;check(difference(scene.meshes[1].positions,expected)<1e-6,"衣物自带调整没有保留");
  auto invalid=targets;invalid[0].morphs.erase(invalid[0].morphs.begin());auto missing_graphs=graphs;missing_graphs[0]=graph(invalid[0]);rejects([&]{ConformRuntime bad(base,invalid,skins,missing_graphs);},"缺少参考形体仍静默穿戴");
  auto graft=base;graft.meshes[1].graft_target_vertices=3;rejects([&]{ConformRuntime bad(graft,targets,skins,graphs);},"允许跨性别 GeoGraft");
  for(const auto &female:{"/Genesis 8/Female","/Genesis 8.1/Female"})for(const auto &male:{"/Genesis 8/Male","/Genesis 8.1/Male"})check(male_to_female_fit(male,female,b.content_type),"遗漏 G8 / G8.1 男装女用");
  check(!male_to_female_fit("/Genesis 8/Female","/Genesis 8/Male",b.content_type)&&!male_to_female_fit(b.preferred_base,a.auto_fit_base,"Follower/Hair"),"转换范围错误扩大");
  Skin female,male;female.joints.resize(1);female.initial.resize(1);female.joints[0].center_cm={1,2,3};female.joints[0].end_cm={4,5,6};female.joints[0].orientation_degrees={7,8,9};
  male=female;male.joints[0].center_cm={20,30,40};male.joints[0].end_cm={50,60,70};male.joints[0].orientation_degrees={10,20,30};
  ConformLink joint_link;joint_link.cross_base=true;joint_link.source_skin=0;joint_link.follower_skin=1;joint_link.joints={0};
  std::vector<std::vector<JointPose>> poses(2,std::vector<JointPose>(1));poses[0][0].rotation_degrees={30,20,10};poses[0][0].center_offset_cm={.5f,0,0};
  const auto pose=conform_pose(joint_link,{female,male},poses,poses[1]);
  check(distance(joint_transforms(female,poses[0])[0].point({.3f,.2f,.1f}),joint_transforms(male,pose)[0].point({.3f,.2f,.1f}))<1e-6,"服装绕男性关节转动");
  female.joints[0].name=male.joints[0].name="hip";std::vector<Skin> correction_skins{female,male};
  auto driver=[](FormulaGraph g,uint32_t morph){Channel c;c.binding={Property::rotation,0,2};const auto input=uint32_t(g.channels.size());g.channels.push_back(c);Expression e;e.output=uint32_t(g.morph_channels[morph]);e.code={{Op::channel,0,input}};g.expressions.push_back(e);g.prepare();return g;};
  std::vector<FormulaGraph> corrections{driver(graph(a,0),1),driver(graph(b,1),0)};
  ConformRuntime authored(base,targets,correction_skins,corrections);check(std::find(authored.link(1)->projected_morphs.begin(),authored.link(1)->projected_morphs.end(),1)==authored.link(1)->projected_morphs.end()&&authored.link(1)->morph_sources[0]<0,"同一骨骼驱动的 JCM 被叠加两次");
  corrections[1].channels.back().binding.axis=1;ConformRuntime distinct(base,targets,correction_skins,corrections);check(std::find(distinct.link(1)->projected_morphs.begin(),distinct.link(1)->projected_morphs.end(),1)!=distinct.link(1)->projected_morphs.end(),"不同骨骼轴的修正被错误视为同一 JCM");
  // 女性轮廓比男性参考凸出 7 cm，粗布面所有正向采样点均落在凸起之外。
  // 必须通过身体到布面的反向约束保护凸起，不能沿用同基型的 2 cm 修正上限。
  ir::Mesh bump;bump.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},{-.1f,-.1f,0},{.1f,-.1f,0},{.1f,.1f,0},{-.1f,.1f,0},{0,0,.07f}};
  bump.polygon_groups={"Chest"};
  bump.triangles={{{0,1,5}},{{0,5,4}},{{1,2,6}},{{1,6,5}},{{2,3,7}},{{2,7,6}},{{3,0,4}},{{3,4,7}},{{4,5,8}},{{5,6,8}},{{6,7,8}},{{7,4,8}}};
  ir::Mesh coarse;coarse.positions={{-.8f,-.8f,.005f},{.8f,-.8f,.005f},{-.4f,.8f,.005f}};coarse.triangles={{{0,1,2}}};
  auto bump_scene=base;bump_scene.meshes={bump,coarse};a.morphs={clone};a.morphs[0].offsets={{8,{0,0,-.07f}}};b.morphs.clear();b.smoothing.enabled=true;b.smoothing.collision_target="#body";
  targets={a,b};graphs={graph(a),graph(b)};values[0].morphs={0};values[1].morphs.clear();DeformationRuntime protection(bump_scene,targets,skins,graphs);protection.evaluate(values,{});
  const auto &p=bump_scene.meshes[1].positions;const double det=(p[1].y-p[2].y)*(p[0].x-p[2].x)+(p[2].x-p[1].x)*(p[0].y-p[2].y);
  const double u=((p[1].y-p[2].y)*(-p[2].x)+(p[2].x-p[1].x)*(-p[2].y))/det,v=((p[2].y-p[0].y)*(-p[2].x)+(p[0].x-p[2].x)*(-p[2].y))/det;
  check(u*p[0].z+v*p[1].z+(1-u-v)*p[2].z>.07,"大幅体型变化仍穿过粗布面");
  // 原本远离布面的肢体在摆姿势后进入布片；静止绑定的局部球不能覆盖它。
  ir::Mesh solids;auto box=[&](float x,float bottom,float top){const uint32_t first=uint32_t(solids.positions.size());for(auto p:std::vector<ir::Vec3>{{-.3f,-.3f,bottom},{.3f,-.3f,bottom},{.3f,.3f,bottom},{-.3f,.3f,bottom},{-.3f,-.3f,top},{.3f,-.3f,top},{.3f,.3f,top},{-.3f,.3f,top}}){p.x+=x;solids.positions.push_back(p);}for(auto v:std::vector<std::array<uint32_t,3>>{{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}}){for(auto &k:v)k+=first;ir::Triangle f;f.vertices=v;solids.triangles.push_back(f);}};
  box(0,-.5f,-.3f);box(2,-.2f,.06f);ir::Mesh panel;panel.positions={{-.1f,-.1f,.01f},{.1f,-.1f,.01f},{0,.1f,.01f}};panel.triangles={{{0,1,2}}};
  auto contact_scene=base;contact_scene.meshes={solids,panel};CollisionRuntime contact(contact_scene,targets);const auto rest_solids=solids.positions;
  for(size_t v=8;v<16;++v)contact_scene.meshes[0].positions[v].x-=2;contact.evaluate({});
  for(auto p:contact_scene.meshes[1].positions)check(p.z>=.0624f&&p.z<.07f,"跨区域肢体没有从布面内部退出");
  check(contact.evaluate({}).meshes.empty(),"补充碰撞重复求值漂移");
  contact_scene.meshes[0].positions=rest_solids;ir::Delta moved;moved.meshes.push_back({0,rest_solids});contact.evaluate(std::move(moved));check(contact_scene.meshes[1].positions==panel.positions,"肢体移开后布面没有复原");
  std::cout<<"Male to female reference / morph isolation / joint alignment / cache / scope: PASS\n";
}
static editor::Document load_wear(const fs::path &file,const std::vector<fs::path> &roots,bool defer=true){editor::Document d;d.source_file=file;d.loaded=daz::load(file,{roots,false,defer});d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);return d;}
static ir::Scene evaluate(const editor::Document &d,const editor::Snapshot &s){auto scene=d.loaded.scene;runtime::DeformationRuntime r(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);r.evaluate(s.values,s.poses);return scene;}
static J points_json(const std::vector<ir::Vec3> &points){J j=J::array();for(auto p:points)j.push_back({p.x,p.y,p.z});return j;}
static std::vector<ir::Vec3> read_points(const J &j){std::vector<ir::Vec3> p;for(const auto &v:j)p.push_back({v[0],v[1],v[2]});return p;}
static J collision_mesh(const ir::Mesh &m){
  J j={{"positions",points_json(m.positions)},{"groups",m.polygon_groups},{"materials",m.material_slots},{"triangles",J::array()},{"polygons",J::array()},{"creases",J::array()},{"corners",m.corners},{"subdivision",{m.subdivision.enabled,m.subdivision.level,m.subdivision.render_level,m.subdivision.algorithm,m.subdivision.edge_interpolation}}};
  for(const auto &f:m.triangles)j["triangles"].push_back({f.vertices,f.polygon_group,f.source_polygon,f.material_slot});
  for(const auto &p:m.polygons)j["polygons"].push_back({p.vertices,p.polygon_group});
  for(const auto &c:m.creases)j["creases"].push_back({c.a,c.b,c.weight});return j;
}
static ir::Mesh read_collision_mesh(const J &j){
  ir::Mesh m;m.positions=read_points(j.at("positions"));m.polygon_groups=j.at("groups").get<std::vector<std::string>>();m.material_slots=j.value("materials",std::vector<std::string>{});
  for(const auto &f:j.at("triangles")){ir::Triangle t;t.vertices=f[0].get<std::array<uint32_t,3>>();t.polygon_group=f[1];t.source_polygon=f[2];if(f.size()>3)t.material_slot=f[3];m.triangles.push_back(t);}
  for(const auto &f:j.at("polygons")){ir::Polygon p;p.vertices=f[0].get<std::vector<uint32_t>>();p.polygon_group=f[1];p.uv.resize(p.vertices.size());m.polygons.push_back(p);}
  for(const auto &c:j.at("creases"))m.creases.push_back({c[0],c[1],c[2]});m.corners=j.at("corners").get<std::vector<std::pair<uint32_t,float>>>();
  const auto &s=j.at("subdivision");m.subdivision={s[0],s[1],s[2],s[3],s[4]};return m;
}
static const ir::Mesh &mesh(const editor::Document &d,const ir::Scene &s,size_t t){return s.meshes.at(s.instances.at(d.catalog.targets.at(t).instance).mesh);}
static void dump(const editor::Document &d,const ir::Scene &s,size_t t,const fs::path &file,bool subdivide=false){auto m=mesh(d,s,t);if(subdivide){runtime::Subdivision sub(m);if(sub.active()){m.positions=sub.evaluate(m.positions);m.triangles=sub.triangles();}}std::ofstream out(file);out.precision(9);for(auto p:m.positions)out<<"v "<<p.x<<' '<<p.y<<' '<<p.z<<'\n';for(const auto &f:m.triangles)out<<"f "<<f.vertices[0]+1<<' '<<f.vertices[1]+1<<' '<<f.vertices[2]+1<<'\n';
  if(file.stem().string().starts_with("female")){J groups={{"names",m.polygon_groups},{"triangles",J::array()}};for(const auto &f:m.triangles)groups["triangles"].push_back(f.polygon_group);auto metadata=file;metadata.replace_extension("json");std::ofstream(metadata)<<groups.dump();}
}
static void actual(const fs::path &file,const fs::path &wear,const fs::path &output){
  const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};fs::create_directories(output);
  std::cout<<"Loading test scene"<<std::endl;auto d=load_wear(file,roots,false);d.generation=1;auto s=editor::initial_snapshot(d);
  // 只在测试副本中保留原场景人物的完整形态和姿势，去掉已有服装与道具，便于检查新服装。
  for(size_t t=d.catalog.targets.size();t-->0;)if(!d.catalog.targets[t].character)editor::remove_target(d,s,t);
  std::vector<size_t> females;size_t male=SIZE_MAX;
  for(size_t t=0;t<d.catalog.targets.size();++t){if(d.catalog.targets[t].auto_fit_base.ends_with("/Female"))females.push_back(t);else if(d.catalog.targets[t].auto_fit_base.ends_with("/Male"))male=t;}
  check(females.size()>=2&&male!=SIZE_MAX,"真实测试需要两位女性和一位男性");const auto host=females.front(),original_count=d.catalog.targets.size();
  auto baseline=evaluate(d,s);std::vector<size_t> clothing;
  for(const auto *name:{"!Tshirt.duf","Bermuda.duf","Flip Flop.duf"}){
    std::cout<<"Importing "<<name<<std::endl;const auto first=d.catalog.targets.size();++d.generation;editor::append_document(d,load_wear(wear/name,roots));editor::attach_import(d,first,host);clothing.push_back(first);
    auto expanded=editor::initial_snapshot(d);std::copy(s.values.begin(),s.values.end(),expanded.values.begin());std::copy(s.poses.begin(),s.poses.end(),expanded.poses.begin());s=std::move(expanded);
  }
  std::cout<<"Evaluating female fit"<<std::endl;auto fitted=evaluate(d,s);J report={{"source",file.generic_string()},{"garments",J::array()}};
  for(size_t t=0;t<original_count;++t)check(mesh(d,fitted,t).positions==mesh(d,baseline,t).positions,"穿戴改变了人物");
  for(auto t:clothing){const auto &m=mesh(d,fitted,t),&base=mesh(d,d.loaded.scene,t);double stretch=0;size_t over=0;for(const auto &f:m.triangles)for(int k=0;k<3;++k){const auto a=f.vertices[k],b=f.vertices[(k+1)%3];const auto old=distance(base.positions[a],base.positions[b]);if(old>.0001){const auto ratio=distance(m.positions[a],m.positions[b])/old;stretch=std::max(stretch,ratio);over+=ratio>5;}}
    for(auto p:m.positions)check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"转换出现无效顶点");report["garments"].push_back({{"label",d.catalog.targets[t].label},{"vertices",m.positions.size()},{"max_edge_ratio",stretch},{"edges_over_5x",over}});dump(d,fitted,t,output/(std::to_string(t)+"-fitted.obj"));
  }dump(d,fitted,host,output/"female.obj");std::ofstream(output/"report.json")<<report.dump(2);
  std::cout<<"Checking rebind and detach"<<std::endl;editor::fit_attachment(d,clothing[0],int(male));auto male_scene=evaluate(d,s);check(d.catalog.targets[clothing[0]].conform_target=="#"+d.loaded.scene.instances[d.catalog.targets[male].instance].id.substr(0,d.loaded.scene.instances[d.catalog.targets[male].instance].id.rfind('/')),"重新绑定男性失败");
  editor::fit_attachment(d,clothing[0],-1);auto detached=evaluate(d,s);check(d.catalog.targets[clothing[0]].conform_target.empty(),"解除适配失败");editor::fit_attachment(d,clothing[0],int(host));auto rebound=evaluate(d,s);check(mesh(d,rebound,clothing[0]).positions==mesh(d,fitted,clothing[0]).positions,"重新绑定女性出现累积误差");
  std::cout<<"Checking shape and pose edits"<<std::endl;auto changed=s;size_t morph=SIZE_MAX;
  for(size_t m=0;m<d.catalog.targets[host].morphs.size();++m){const auto &p=d.catalog.targets[host].morphs[m];if(p.evaluable&&p.auto_follow&&p.channel_id.starts_with("FBM")&&p.offset_count()>5000){morph=m;break;}}
  check(morph!=SIZE_MAX,"缺少真实体型参数");runtime::set_parameter(d.catalog.targets[host],changed.values[host],morph,s.values[host].morphs[morph]+.3f);auto shaped=evaluate(d,changed);check(difference(mesh(d,shaped,clothing[0]).positions,mesh(d,fitted,clothing[0]).positions)>.0001,"女性体型没有传递到男装");report["shape_channel"]=d.catalog.targets[host].morphs[morph].channel_id;
  changed=s;bool posed=false;for(size_t k=0;k<d.skeletons.skins.size();++k)if(d.skeletons.skins[k].instance==d.catalog.targets[host].instance)for(size_t j=0;j<d.skeletons.skins[k].joints.size();++j)if(d.skeletons.skins[k].joints[j].name=="lShldrBend"){changed.poses[k][j].rotation_degrees.z+=20;posed=true;}
  check(posed,"缺少肩部骨骼");auto bent=evaluate(d,changed);check(difference(mesh(d,bent,clothing[0]).positions,mesh(d,fitted,clothing[0]).positions)>.001,"衣物未跟随肩部动作");dump(d,bent,clothing[0],output/"shirt-posed.obj");
  std::cout<<"Copying outfit to second female"<<std::endl;const auto copied=editor::copy_outfit(d,s,host,clothing,{females[1]});check(copied.copied==clothing.size(),"无法将男装复制到另一女性");
  for(size_t t=0;t<original_count;++t)s.values[t].visible=std::find(females.begin(),females.end(),t)!=females.end();
  const auto archive=output/"summer-vibes-test4.dufex";editor::save_scene_extension(archive,d,s);auto expected=evaluate(d,s);
  std::cout<<"Reopening saved scene"<<std::endl;auto reopened=editor::load_scene_extension(archive,roots,100);auto final=evaluate(*reopened.document,reopened.snapshot);
  check(editor::snapshot_json(d,s)==editor::snapshot_json(*reopened.document,reopened.snapshot),"保存重开丢失状态");for(size_t t=0;t<d.catalog.targets.size();++t)check(difference(mesh(d,expected,t).positions,mesh(*reopened.document,final,t).positions)<1e-6,"保存重开形变不一致");
  report["status"]="PASS";report["female_hosts"]=females.size();report["copy_count"]=copied.copied;report["rebind_detach_shape_pose_reopen"]=true;std::ofstream(output/"report.json")<<report.dump(2);std::cout<<report.dump(2)<<std::endl;
}
static void inspect_document(const editor::Document &d,const editor::Snapshot &s,const fs::path &output);
static void forefoot_scene(const fs::path &file,const fs::path &output,const std::string &label={}){
  const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};
  std::cout<<"Loading forefoot scene"<<std::endl;auto restored=editor::load_scene_extension(file,roots,1);auto &d=*restored.document;auto s=restored.snapshot;std::vector<size_t> shoes;
  for(size_t t=0;t<d.catalog.targets.size();++t){const auto k=d.formulas.graphs[t].skin;if((label.empty()||d.catalog.targets[t].label==label)&&!d.catalog.targets[t].character&&k>=0&&runtime::supports_forefoot(mesh(d,d.loaded.scene,t),d.skeletons.skins[size_t(k)])){shoes.push_back(t);s.values[t].forefoot_uniformity=0;}}
  check(!shoes.empty(),"真实前掌测试未找到指定鞋子");auto scene=d.loaded.scene;runtime::DeformationRuntime r(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);r.evaluate(s.values,s.poses);const auto baseline=scene;
  for(auto t:shoes)s.values[t].forefoot_uniformity=.5f;r.evaluate(s.values,s.poses);const auto half=scene;
  for(auto t:shoes)s.values[t].forefoot_uniformity=1;r.evaluate(s.values,s.poses);const auto full=scene;
  check(r.evaluate(s.values,s.poses).meshes.empty(),"真实前掌重复求值漂移");J report={{"source",file.generic_string()},{"shoes",J::array()}};
  for(auto t:shoes){check(mesh(d,baseline,t).positions!=mesh(d,full,t).positions&&mesh(d,half,t).positions!=mesh(d,full,t).positions,"真实前掌参数没有连续生效");report["shoes"].push_back({{"target",t},{"label",d.catalog.targets[t].label},{"max_move_m",difference(mesh(d,baseline,t).positions,mesh(d,full,t).positions)}});}
  for(size_t t=0;t<d.catalog.targets.size();++t)if(std::find(shoes.begin(),shoes.end(),t)==shoes.end())check(mesh(d,baseline,t).positions==mesh(d,full,t).positions,"前掌参数修改了其他对象");
  for(auto t:shoes)s.values[t].forefoot_uniformity=0;r.evaluate(s.values,s.poses);for(auto t:shoes)check(mesh(d,baseline,t).positions==mesh(d,scene,t).positions,"关闭真实前掌参数没有精确恢复");
  for(auto t:shoes)s.values[t].forefoot_uniformity=1;r.evaluate(s.values,s.poses);for(auto t:shoes)check(mesh(d,full,t).positions==mesh(d,scene,t).positions,"再次开启前掌参数发生漂移");
  fs::create_directories(output);const auto archive=output/"summer-vibes-test4.dufex";editor::save_scene_extension(archive,d,s);
  std::cout<<"Reopening forefoot scene"<<std::endl;auto reopened=editor::load_scene_extension(archive,roots,2);const auto final=evaluate(*reopened.document,reopened.snapshot);
  check(editor::snapshot_json(d,s)==editor::snapshot_json(*reopened.document,reopened.snapshot),"前掌场景重开丢失状态");for(size_t t=0;t<d.catalog.targets.size();++t)check(difference(mesh(d,full,t).positions,mesh(*reopened.document,final,t).positions)<1e-6,"前掌场景重开几何不同");
  report["status"]="PASS";report["half_full_disable_repeat_reopen_unchanged_others"]=true;std::ofstream(output/"forefoot-report.json")<<report.dump(2);std::cout<<report.dump(2)<<std::endl;
  inspect_document(*reopened.document,reopened.snapshot,output/"geometry");
}
static void inspect(const fs::path &file,const fs::path &output){
  auto restored=editor::load_scene_extension(file,{"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"},1);
  inspect_document(*restored.document,restored.snapshot,output);
}
static void inspect_document(const editor::Document &d,const editor::Snapshot &s,const fs::path &output){
  auto scene=d.loaded.scene;
  runtime::DeformationRuntime r(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);r.evaluate(s.values,s.poses);
  editor::apply_subdivision_levels(scene,s.subdivision_levels);
  auto raw=d.loaded.scene;runtime::DeformationRuntime uncorrected(raw,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);uncorrected.defer_collision();uncorrected.evaluate(s.values,s.poses);
  for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].auto_fit_base.ends_with("/Female")&&d.catalog.targets[t].conform_target.empty()){
    const auto folder=output/std::to_string(t);fs::create_directories(folder);dump(d,scene,t,folder/"female.obj");dump(d,scene,t,folder/"female-render.obj",true);J report=J::array();
    for(const auto &link:r.conform_links())if(link.source==t&&link.cross_base){const auto f=link.follower;dump(d,scene,f,folder/(std::to_string(f)+"-fitted.obj"));dump(d,scene,f,folder/(std::to_string(f)+"-render.obj"),true);dump(d,raw,f,folder/(std::to_string(f)+"-uncorrected.obj"));report.push_back({{"target",f},{"label",d.catalog.targets[f].label},{"max_collision_m",difference(mesh(d,scene,f).positions,mesh(d,raw,f).positions)}});
      auto reference=mesh(d,d.loaded.scene,t).positions;for(size_t v=0;v<reference.size();++v){reference[v].x-=link.base_offsets[v].x;reference[v].y-=link.base_offsets[v].y;reference[v].z-=link.base_offsets[v].z;}
      const auto relative=ir::inverse(raw.instances[d.catalog.targets[t].instance].transform)*raw.instances[d.catalog.targets[f].instance].transform;
      J fixture={{"body",collision_mesh(mesh(d,d.loaded.scene,t))},{"cloth",collision_mesh(mesh(d,d.loaded.scene,f))},{"reference",points_json(reference)},{"body_input",points_json(mesh(d,raw,t).positions)},{"cloth_input",points_json(mesh(d,raw,f).positions)},{"expected",points_json(mesh(d,scene,f).positions)},{"relative",relative.value},{"world",raw.instances[d.catalog.targets[f].instance].transform.value}};
      if(link.follower_skin>=0){const auto k=size_t(link.follower_skin);fixture["morphed"]=points_json(uncorrected.skin_source(k));fixture["skin"]["joints"]=J::array();fixture["skin"]["poses"]=J::array();fixture["skin"]["weights"]=J::array();fixture["skin"]["dual_quaternion"]=d.skeletons.skins[k].method==runtime::SkinMethod::dual_quaternion;
        for(const auto &joint:d.skeletons.skins[k].joints)fixture["skin"]["joints"].push_back({{"name",joint.name},{"parent",joint.parent},{"center",points_json({joint.center_cm})[0]},{"end",points_json({joint.end_cm})[0]},{"orientation",points_json({joint.orientation_degrees})[0]},{"order",joint.rotation_order},{"inherits_scale",joint.inherits_scale}});
        for(const auto &p:uncorrected.effective_poses()[k])fixture["skin"]["poses"].push_back({{"translation",points_json({p.translation_cm})[0]},{"rotation",points_json({p.rotation_degrees})[0]},{"scale",points_json({p.scale})[0]},{"general_scale",p.general_scale},{"center",points_json({p.center_offset_cm})[0]},{"end",points_json({p.end_offset_cm})[0]},{"orientation",points_json({p.orientation_offset_degrees})[0]}});
        for(const auto &weights:d.skeletons.skins[k].weights){J w=J::array();for(const auto &v:weights)w.push_back({v.joint,v.weight});fixture["skin"]["weights"].push_back(w);}
      }
      std::ofstream(folder/(std::to_string(f)+"-collision.json"))<<fixture.dump();
    }
    std::ofstream(folder/"inspection.json")<<report.dump(2);
  }
}
static void replay(const fs::path &file,const fs::path &output,bool footwear=false){
  J j;std::ifstream(file)>>j;editor::Document d;auto &scene=d.loaded.scene;scene.meshes={read_collision_mesh(j.at("body")),read_collision_mesh(j.at("cloth"))};scene.instances.resize(2);scene.instances[1].mesh=1;
  runtime::Target body;body.id="body/mesh";body.auto_fit_base="/Genesis 8.1/Female";auto cloth=body;cloth.id="cloth/mesh";cloth.instance=1;cloth.auto_fit_base.clear();cloth.preferred_base="/Genesis 8/Male";cloth.content_type="Follower/Wardrobe";cloth.conform_target="#body";cloth.smoothing.enabled=true;cloth.smoothing.collision_target="#body";
  d.catalog.targets={body,cloth};ir::Transform relative;relative.value=j.at("relative").get<std::array<float,12>>();
  if(footwear){runtime::Skin skin;skin.method=j["skin"]["dual_quaternion"].get<bool>()?runtime::SkinMethod::dual_quaternion:runtime::SkinMethod::linear;
    auto point=[](const J &v){return ir::Vec3{v[0],v[1],v[2]};};std::vector<runtime::JointPose> poses;
    for(const auto &v:j["skin"]["joints"]){runtime::Joint n;n.name=v["name"];n.parent=v["parent"];n.center_cm=point(v["center"]);n.end_cm=point(v["end"]);n.orientation_degrees=point(v["orientation"]);n.rotation_order=v["order"];n.inherits_scale=v["inherits_scale"];skin.joints.push_back(n);}
    for(const auto &v:j["skin"]["poses"]){runtime::JointPose p;p.translation_cm=point(v["translation"]);p.rotation_degrees=point(v["rotation"]);p.scale=point(v["scale"]);p.general_scale=v["general_scale"];p.center_offset_cm=point(v["center"]);p.end_offset_cm=point(v["end"]);p.orientation_offset_degrees=point(v["orientation"]);poses.push_back(p);}
    for(const auto &v:j["skin"]["weights"]){std::vector<runtime::Influence> weights;for(const auto &w:v)weights.push_back({w[0],w[1]});skin.weights.push_back(weights);}
    auto morphed=read_points(j["morphed"]);check(difference(runtime::deform(skin,poses,morphed),read_points(j["cloth_input"]))<1e-6,"蒙皮回放与真实场景不一致");
    const auto &base=scene.meshes[1].positions;auto offsets=morphed;for(size_t v=0;v<base.size();++v)offsets[v]={morphed[v].x-base[v].x,morphed[v].y-base[v].y,morphed[v].z-base[v].z};runtime::GarmentDeformation(scene.meshes[1]).constrain_forefoot(base,offsets,1);for(size_t v=0;v<base.size();++v)morphed[v]={base[v].x+offsets[v].x,base[v].y+offsets[v].y,base[v].z+offsets[v].z};j["morphed"]=points_json(morphed);
    runtime::ForefootWeights uniform(scene.meshes[1],skin);check(uniform.active(),"未识别前掌区域");skin=uniform.apply(skin,1);
    j["cloth_input"]=points_json(runtime::deform(skin,poses,morphed));
  }
  runtime::MorphRuntime policy(scene,d.catalog.targets);if(footwear)policy.set_forefoot_uniformity(1,1);
  runtime::CollisionRuntime r(scene,d.catalog.targets,[&](uint32_t,uint32_t){return relative;},[&](uint32_t target,const auto &input,auto &positions){policy.constrain_collision(target,input,positions);},[&](uint32_t target){return policy.garment_revision(target);});r.reference_surface(1,read_points(j.at("reference")));
  scene.meshes[0].positions=read_points(j.at("body_input"));scene.meshes[1].positions=read_points(j.at("cloth_input"));ir::Delta delta;delta.meshes.push_back({1,scene.meshes[1].positions});r.evaluate(std::move(delta));
  fs::create_directories(output);dump(d,scene,1,output/(file.stem().string()+"-fitted.obj"));dump(d,scene,1,output/(file.stem().string()+"-render.obj"),true);
  std::cout<<"Replay max difference: "<<difference(scene.meshes[1].positions,read_points(j.at("expected")))<<std::endl;check(r.evaluate({}).meshes.empty(),"重复碰撞产生漂移");
  j["expected"]=points_json(scene.meshes[1].positions);std::ofstream(output/file.filename())<<j.dump();
}
static void g9_actual(const fs::path &file,const fs::path &wear,const fs::path &output){
  const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};fs::create_directories(output);
  std::cout<<"Loading G9 footwear test scene"<<std::endl;auto restored=editor::load_scene_extension(file,roots,1);auto &d=*restored.document;auto s=restored.snapshot;
  size_t host=SIZE_MAX;std::string node;for(const auto &t:d.catalog.targets)if(t.label=="Flip Flops"&&!t.conform_target.empty()){node=t.conform_target;break;}
  for(size_t t=0;t<d.catalog.targets.size();++t)if(node=="#"+d.catalog.targets[t].id.substr(0,d.catalog.targets[t].id.rfind('/')))host=t;
  if(host==SIZE_MAX)for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].auto_fit_base.ends_with("/Female")){host=t;break;}
  check(host!=SIZE_MAX,"未找到 G8 女性测试目标");const auto baseline=evaluate(d,s);const auto first=d.catalog.targets.size();
  std::cout<<"Importing G9 slipper"<<std::endl;editor::append_document(d,load_wear(wear,roots));editor::attach_import(d,first,host);auto defaults=editor::initial_snapshot(d);
  s.values.insert(s.values.end(),defaults.values.begin()+s.values.size(),defaults.values.end());s.poses.insert(s.poses.end(),defaults.poses.begin()+s.poses.size(),defaults.poses.end());s.generation=d.generation;
  check(d.catalog.targets[first].preferred_base=="/Genesis 9/Base","测试鞋不是 G9 基型");const auto skin=d.formulas.graphs[first].skin;check(skin>=0&&runtime::supports_forefoot(mesh(d,d.loaded.scene,first),d.skeletons.skins[size_t(skin)]),"G9 前掌参数不可用");
  auto scene=d.loaded.scene;runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(s.values,s.poses);const auto zero=scene;
  for(size_t t=0;t<first;++t)check(difference(mesh(d,baseline,t).positions,mesh(d,scene,t).positions)<1e-6,"G9 鞋类绑定修改了原有角色或男装");
  s.values[first].forefoot_uniformity=.5f;runtime.evaluate(s.values,s.poses);const auto half=mesh(d,scene,first).positions;s.values[first].forefoot_uniformity=1;runtime.evaluate(s.values,s.poses);const auto full=scene;
  check(half!=mesh(d,full,first).positions&&mesh(d,zero,first).positions!=mesh(d,full,first).positions,"G9 前掌参数未连续生效");check(runtime.evaluate(s.values,s.poses).meshes.empty(),"G9 重复求值发生漂移");
  s.values[first].forefoot_uniformity=0;runtime.evaluate(s.values,s.poses);check(mesh(d,scene,first).positions==mesh(d,zero,first).positions,"G9 前掌关闭不能恢复");s.values[first].forefoot_uniformity=1;runtime.evaluate(s.values,s.poses);check(mesh(d,scene,first).positions==mesh(d,full,first).positions,"G9 前掌重新开启漂移");
  J report={{"source",file.generic_string()},{"wear",wear.generic_string()},{"host",host},{"shoe",first},{"reference_shape",runtime.conform_links().back().reference_shape}};
  auto center=[](const ir::Mesh &m,const std::string &group){std::set<uint32_t> ids;for(const auto &f:m.triangles)if(m.polygon_groups.at(f.polygon_group)==group)ids.insert(f.vertices.begin(),f.vertices.end());check(!ids.empty(),"缺少脚趾落位检查分组");ir::Vec3 c;for(auto v:ids){c.x+=m.positions[v].x;c.y+=m.positions[v].y;c.z+=m.positions[v].z;}const float count=float(ids.size());return ir::Vec3{c.x/count,c.y/count,c.z/count};};
  for(const auto &side:{"l","r"}){const double gap=distance(center(mesh(d,full,first),std::string(side)+"_toes"),center(mesh(d,full,host),std::string(side)+"Toe"));report[std::string(side)+"_forefoot_center_gap_mm"]=gap*1000;check(gap<.055,"G9 拖鞋整体落位偏离目标脚趾，请检查 Clone 静止姿势");}
  auto changed=s;const auto host_skin=size_t(d.formulas.graphs[host].skin);bool posed=false;for(size_t j=0;j<d.skeletons.skins[host_skin].joints.size();++j)if(d.skeletons.skins[host_skin].joints[j].name=="lMetatarsals"){changed.poses[host_skin][j].rotation_degrees.x+=15;posed=true;}
  check(posed,"目标没有跖骨");auto posed_scene=evaluate(d,changed);check(difference(mesh(d,posed_scene,first).positions,mesh(d,full,first).positions)>.001,"G9 鞋子未跟随目标跖骨链");
  changed=s;bool own=false;for(size_t m=0;m<d.catalog.targets[first].morphs.size();++m)if(d.catalog.targets[first].morphs[m].channel_name=="body_bs_ExpandAll"&&d.catalog.targets[first].morphs[m].evaluable){runtime::set_parameter(d.catalog.targets[first],changed.values[first],m,.2f);own=true;break;}
  check(own,"缺少鞋子自身 ExpandAll 调整");auto expanded=evaluate(d,changed);check(difference(mesh(d,expanded,first).positions,mesh(d,full,first).positions)>.0001&&mesh(d,expanded,host).positions==mesh(d,full,host).positions,"G9 自身 Morph 无效或修改人物");
  changed=s;bool shaped=false;for(size_t m=0;m<d.catalog.targets[host].morphs.size();++m){const auto &p=d.catalog.targets[host].morphs[m];if(p.evaluable&&p.auto_follow&&p.channel_id.starts_with("FBM")&&p.offset_count()>5000){runtime::set_parameter(d.catalog.targets[host],changed.values[host],m,s.values[host].morphs[m]+.2f);shaped=true;report["shape_channel"]=p.channel_id;break;}}
  check(shaped,"缺少人物体型参数");auto shaped_scene=evaluate(d,changed);check(difference(mesh(d,shaped_scene,first).positions,mesh(d,full,first).positions)>.0001,"G8 人物体型未传递到 G9 鞋子");
  std::cout<<"Checking G9 detach and rebind"<<std::endl;editor::fit_attachment(d,first,-1);const auto detached=evaluate(d,s);check(difference(mesh(d,detached,first).positions,mesh(d,full,first).positions)>.001,"解除 G9 绑定仍保留转换");editor::fit_attachment(d,first,int(host));const auto rebound=evaluate(d,s);check(difference(mesh(d,rebound,first).positions,mesh(d,full,first).positions)<1e-6,"重新绑定 G9 鞋子漂移");
  // 测试副本中隐藏同一人物原有鞋子，保留其余衣物、材质、姿势和场景选项。
  report["hidden_previous_shoes"]=J::array();
  for(size_t t=0;t<first;++t)if(d.catalog.targets[t].conform_target==d.catalog.targets[first].conform_target&&
    (d.catalog.targets[t].content_type.starts_with("Follower/Wardrobe/Footwear")||d.catalog.targets[t].label=="Flip Flops")){s.values[t].visible=false;report["hidden_previous_shoes"].push_back(d.catalog.targets[t].id);}
  const auto archive=output/"g9-slipper.dufex";editor::save_scene_extension(archive,d,s);std::cout<<"Reopening G9 slipper scene"<<std::endl;auto reopened=editor::load_scene_extension(archive,roots,2);const auto final=evaluate(*reopened.document,reopened.snapshot);
  check(editor::snapshot_json(d,s)==editor::snapshot_json(*reopened.document,reopened.snapshot),"G9 场景保存重开丢失状态");for(size_t t=0;t<d.catalog.targets.size();++t)check(difference(mesh(d,rebound,t).positions,mesh(*reopened.document,final,t).positions)<1e-6,"G9 场景重开几何不一致");
  report["status"]="PASS";report["pose_shape_own_morph_strength_restore_detach_rebind_reopen"]=true;std::ofstream(output/"report.json")<<report.dump(2);std::cout<<report.dump(2)<<std::endl;inspect_document(*reopened.document,reopened.snapshot,output/"geometry");
}
#include "fit_pose_profile.inl"
#include "torso_fit_profile.inl"
int wmain(int argc,wchar_t **argv){try{if(argc==4&&std::wstring(argv[1])==L"--torso-collision")torso_collision_replay(argv[2],argv[3]);else if(argc==4&&std::wstring(argv[1])==L"--torso-replay")torso_replay(argv[2],argv[3]);else if(argc==4&&std::wstring(argv[1])==L"--torso-scene")torso_profile(argv[2],argv[3]);else if((argc==4||argc==5)&&std::wstring(argv[1])==L"--pose-profile")pose_profile(argv[2],argv[3],argc==5?fs::path(argv[4]):fs::path{});else if(argc==5&&std::wstring(argv[1])==L"--g9")g9_actual(argv[2],argv[3],argv[4]);else if(argc==5&&std::wstring(argv[1])==L"--actual")actual(argv[2],argv[3],argv[4]);else if((argc==4||argc==5)&&std::wstring(argv[1])==L"--forefoot-scene")forefoot_scene(argv[2],argv[3],argc==5?fs::path(argv[4]).string():"");else if(argc==4&&std::wstring(argv[1])==L"--inspect")inspect(argv[2],argv[3]);else if(argc==4&&(std::wstring(argv[1])==L"--replay"||std::wstring(argv[1])==L"--footwear"))replay(argv[2],argv[3],std::wstring(argv[1])==L"--footwear");else {unit();g9_unit();}return 0;}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
