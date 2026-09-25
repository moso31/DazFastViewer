#include "editor/document.h"
#include "daz/pose.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <chrono>

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

static void mesh_collision() {
  ir::Scene scene;ir::Mesh body;body.positions={{-2,-2,0},{2,-2,0},{2,2,0},{-2,2,0}};
  ir::Triangle f;f.vertices={0,1,2};body.triangles.push_back(f);f.vertices={0,2,3};body.triangles.push_back(f);
  ir::Mesh cloth;cloth.positions={{-.5f,-.5f,-.1f},{.5f,-.5f,-.1f},{0,.5f,-.1f}};f.vertices={0,1,2};cloth.triangles={f};
  scene.meshes={body,cloth};scene.instances.resize(2);scene.instances[1].mesh=1;
  Target a;a.id="body/mesh";Target b;b.id="cloth/mesh";b.instance=1;b.smoothing.enabled=true;b.smoothing.collision_target="#body";
  std::vector<Target> targets={a,b};CollisionRuntime runtime(scene,targets);auto d=runtime.evaluate({});
  require(d.meshes.size()==1,"初始碰撞未输出服装更新");for(auto p:scene.meshes[1].positions) require(p.z>=.00049f,"穿入平面的服装未推出");
  const auto baseline=scene.meshes[1].positions;
  {
    auto grid=scene;auto &surface=grid.meshes[1];surface.positions.clear();surface.triangles.clear();
    for(int y=0;y<33;++y) for(int x=0;x<33;++x) surface.positions.push_back({(x-16)*.03f,(y-16)*.03f,-.01f});
    for(uint32_t y=0;y<32;++y) for(uint32_t x=0;x<32;++x) {const auto v=y*33+x;surface.triangles.push_back({{v,v+1,v+34}});surface.triangles.push_back({{v,v+34,v+33}});}
    auto serial=grid;CollisionRuntime reference(serial,targets),optimized(grid,targets);reference.evaluate({},false);optimized.evaluate({});
    require(grid.meshes[1].positions==serial.meshes[1].positions,"并行碰撞改变了顺序修正结果");
    for(auto &p:grid.meshes[0].positions) p.z=.02f;serial.meshes[0]=grid.meshes[0];ir::Delta moved;moved.meshes.push_back({0,grid.meshes[0].positions});
    reference.evaluate(moved,false);optimized.evaluate(moved);require(grid.meshes[1].positions==serial.meshes[1].positions,"并行碰撞在姿势更新后产生不同结果");
  }
  {
    auto rebuilt=scene;rebuilt.meshes[1]=cloth;CollisionRuntime cached(rebuilt,targets);cached.reuse(runtime);cached.evaluate({});
    require(cached.stats().evaluations==0&&cached.stats().cache_hits==1&&rebuilt.meshes[1].positions==baseline,"相同场景重建未复用碰撞结果");
    for(auto &p:rebuilt.meshes[0].positions) p.z=.3f;ir::Delta changed;changed.meshes.push_back({0,rebuilt.meshes[0].positions});cached.evaluate(changed);
    auto reference=rebuilt;reference.meshes[1]=cloth;CollisionRuntime fresh(reference,targets);fresh.evaluate({});
    require(cached.stats().evaluations==1&&rebuilt.meshes[1].positions==reference.meshes[1].positions,"宿主形变后错误复用旧碰撞结果");
    rebuilt=scene;rebuilt.meshes[1]=cloth;auto adjusted=targets;adjusted[1].smoothing.collision_iterations++;
    CollisionRuntime settings(rebuilt,adjusted);settings.reuse(runtime);settings.evaluate({});require(settings.stats().cache_hits==0,"碰撞设置改变后没有使缓存失效");
  }
  require(runtime.evaluate({}).meshes.empty(),"相同输入重复执行碰撞");
  for(auto &p:scene.meshes[0].positions) p.z=.2f;d={};d.meshes.push_back({0,scene.meshes[0].positions});runtime.evaluate(d);
  for(auto p:scene.meshes[1].positions) require(p.z>=.20049f,"只有碰撞对象改变时服装没有更新");
  scene.meshes[0]=body;d={};d.meshes.push_back({0,body.positions});runtime.evaluate(d);
  require(distance(scene.meshes[1].positions,baseline)==0,"碰撞求值累积漂移");
  scene.instances[1].transform=ir::Transform::translate({0,0,1});runtime.evaluate({});
  require(distance(scene.meshes[1].positions,cloth.positions)==0,"服装移出碰撞区后没有恢复未修正输入");
  auto cyclic=targets;cyclic[0].smoothing=b.smoothing;cyclic[0].smoothing.collision_target="#cloth";
  rejects([&] {CollisionRuntime bad(scene,cyclic);},"碰撞循环未拒绝");
  auto disabled=targets;disabled[1].smoothing.enabled=false;scene.meshes[1]=cloth;scene.instances[1].transform={};CollisionRuntime off(scene,disabled);
  require(off.evaluate({}).meshes.empty()&&distance(scene.meshes[1].positions,cloth.positions)==0,"关闭碰撞仍修改几何");
  auto graft=body;graft.graft_target_vertices=4;graft.graft_hidden_polygons={0};for(auto &p:graft.positions) p.z=.2f;
  scene.meshes.push_back(graft);scene.instances.push_back({});scene.instances.back().mesh=2;
  Target g;g.id="graft/mesh";g.instance=2;g.conform_target="#body";g.smoothing=b.smoothing;
  auto combined=targets;combined.push_back(g);CollisionRuntime composite(scene,combined);composite.evaluate({});
  for(auto p:scene.meshes[1].positions) require(p.z>=.20049f,"服装碰撞没有包含 GeoGraft 表面");
  scene.instances[2].visible=false;d={};d.visibility.push_back({2,false});composite.evaluate(d);
  for(auto p:scene.meshes[1].positions) require(std::abs(p.z-.0005f)<1e-6f,"隐藏 GeoGraft 后没有恢复人体碰撞表面");
  scene.instances[2].visible=true;for(auto &p:scene.meshes[2].positions) p.z=-.02f;
  d={};d.meshes.push_back({2,scene.meshes[2].positions});d.visibility.push_back({2,true});composite.evaluate(d);
  for(auto p:scene.meshes[1].positions) require(p.z>=.00049f,"内层 GeoGraft 使服装重新穿入基础人体");
  scene.meshes={body,cloth};scene.instances.resize(2);a.morphs={morph("bulge",{{0,{0,0,.2f}},{1,{0,0,.2f}},{2,{0,0,.2f}},{3,{0,0,.2f}}})};
  targets={a,b};std::vector<Skin> skins;std::vector<FormulaGraph> graphs={graph(a,-1),graph(b,-1)};
  DeformationRuntime deformed(scene,targets,skins,graphs);std::vector<Properties> values(2);values[0].morphs={0};deformed.evaluate(values,{});
  const auto initial=scene.meshes[1].positions;values[0].morphs[0]=1;deformed.evaluate(values,{});
  for(auto p:scene.meshes[1].positions) require(p.z>=.20049f,"完整形变管线未在 Morph 后更新碰撞");
  values[0].morphs[0]=0;deformed.evaluate(values,{});require(distance(scene.meshes[1].positions,initial)==0,"完整形变碰撞归零发生漂移");
  ir::Mesh bump;const ir::Vec3 peak{.11f,.08f,.01f};bump.positions={{.09f,.06f,0},{.13f,.06f,0},{.13f,.10f,0},{.09f,.10f,0},peak};
  for(uint32_t k=0;k<4;++k) {ir::Triangle face;face.vertices={k,(k+1)%4,4};bump.triangles.push_back(face);}
  auto coarse=cloth;for(auto &p:coarse.positions) p.z=.002f;
  scene.meshes={bump,coarse};targets[0].morphs.clear();CollisionRuntime small_bump(scene,targets);small_bump.evaluate({});
  const auto &p=scene.meshes[1].positions;const auto u=ir::Vec3{p[1].x-p[0].x,p[1].y-p[0].y,p[1].z-p[0].z},v=ir::Vec3{p[2].x-p[0].x,p[2].y-p[0].y,p[2].z-p[0].z};
  const ir::Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
  require((peak.x-p[0].x)*n.x+(peak.y-p[0].y)*n.y+(peak.z-p[0].z)*n.z<0,"粗服装面漏掉采样点之间的小凸起");
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
  auto fitted=scene;fitted.meshes[0]=body;fitted.meshes[1]=cloth;for(auto &p:fitted.meshes[1].positions) p.z=.01f;
  DeformationRuntime close_fit(fitted,targets,skins,graphs);close_fit.evaluate(values,{});
  require(std::abs(fitted.meshes[1].positions[3*9+2].z-.11f)<1e-6f,"贴体的自动跟随被平滑压回身体内部");
  values[0].morphs[0]=0;runtime.evaluate(values,{});require(distance(scene.meshes[1].positions,cloth.positions)==0,"生成场的平滑改变了原始褶皱或重置结果");
}
static void bone_attachment() {
  ir::Scene base;ir::Mesh mesh;mesh.positions={{0,0,0}};mesh.material_slots={"surface"};base.materials.resize(1);
  for(uint32_t i=0;i<4;++i) {base.meshes.push_back(mesh);ir::Instance instance;instance.mesh=i;instance.materials={0};instance.transform=ir::Transform::translate({i==0?2.f:i==1?3.f:i==2?3.5f:8.f,0,0});base.instances.push_back(instance);}
  Target body;body.id="body/mesh";body.morphs={morph("head_size",{},false)};
  Target glasses;glasses.id="glasses/mesh";glasses.instance=1;glasses.parent="#head-instance";
  glasses.parent="#empty-pivot";glasses.ancestors={"#empty-pivot","#head-instance","#body"};
  Target child;child.id="child/mesh";child.instance=2;child.parent="#glasses";
  child.parent="#empty-child";child.ancestors={"#empty-child","#glasses","#empty-pivot","#head-instance","#body"};
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
  poses[0][1].rotation_degrees.z=0;values[0].morphs[0]=0;
  poses[0][1].center_offset_cm={10,0,0};runtime.evaluate(values,poses);
  require(close(1,{3.1f,0,0})&&close(2,{3.6f,0,0}),"刚性附件漏掉关节中心的 Morph 位移");
  poses[0][1].center_offset_cm={};runtime.evaluate(values,poses);
  require(close(1,{3,0,0})&&close(2,{3.5f,0,0}),"关节中心恢复后附件发生漂移");

}
static void root_follower_and_visibility() {
  ir::Scene scene;ir::Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{0,1,0}};ir::Triangle face;face.vertices={0,1,2};mesh.triangles={face};
  scene.meshes={mesh,mesh,mesh};scene.instances.resize(3);for(int i=0;i<3;++i) scene.instances[i].mesh=i;
  Target body;body.id="body/mesh";Target shoe;shoe.id="shoe/mesh";shoe.instance=1;shoe.conform_target="#body";
  Target child;child.id="child/mesh";child.instance=2;child.parent="#body";
  std::vector<Target> targets={body,shoe,child};std::vector<FormulaGraph> graphs={graph(body,-1),graph(shoe,-1),graph(child,-1)};std::vector<Skin> skins;
  DeformationRuntime runtime(scene,targets,skins,graphs);std::vector<Properties> values(3);runtime.evaluate(values,{});
  values[0].transform.translation_cm={10,20,30};runtime.evaluate(values,{});
  require(distance({scene.instances[0].transform.point({})},{scene.instances[1].transform.point({})})<1e-7,"根层级 Fit To 没有跟随目标交互变换");
  values[0].visible=false;auto delta=runtime.evaluate(values,{});
  require(!scene.instances[0].visible&&scene.instances[2].visible&&scene.instances[1].visible,"普通节点可见性错误地沿 parent 或 Fit To 传播");
  require(delta.visibility.size()==1&&delta.meshes.empty()&&delta.instances.empty(),"切换可见性不应重算顶点或实例变换");
  values[2].visible=false;values[0].visible=true;runtime.evaluate(values,{});
  require(scene.instances[0].visible&&!scene.instances[2].visible,"显示父对象覆盖了子对象自己的隐藏状态");
  values[0].transform={};runtime.evaluate(values,{});require(distance({scene.instances[1].transform.point({})},{{}})==0,"根层级 Fit To 重置产生漂移");
}
static void graft_collision_restore() {
  ir::Mesh body;body.id="body";body.positions={{-2,-2,0},{2,-2,0},{2,2,0},{-2,2,0}};
  body.triangles={{{0,1,2}},{{0,2,3}}};body.triangles[0].source_polygon=0;body.triangles[1].source_polygon=1;
  auto graft=body;graft.id="graft";graft.graft_target_vertices=4;graft.graft_hidden_polygons={0,1};graft.graft_vertex_pairs={{0,0},{1,1},{2,2},{3,3}};for(auto &p:graft.positions) p.z=.1f;
  ir::Mesh cloth;cloth.id="cloth";cloth.positions={{-.3f,-.3f,.05f},{.3f,-.3f,.05f},{0,.3f,.05f}};cloth.triangles={{{0,1,2}}};
  ir::Scene scene;scene.meshes={body,graft,cloth};scene.instances.resize(3);for(int i=0;i<3;++i) {scene.instances[i].mesh=i;scene.instances[i].id=scene.meshes[i].id;}
  Target a;a.id="body/mesh";a.morphs={morph("raise",{{0,{0,0,.2f}},{1,{0,0,.2f}},{2,{0,0,.2f}},{3,{0,0,.2f}}},false)};
  Target b;b.id="graft/mesh";b.instance=1;b.conform_target="#body";
  Target c;c.id="cloth/mesh";c.instance=2;c.smoothing.enabled=true;c.smoothing.collision_target="#body";
  std::vector<Target> targets={a,b,c};std::vector<FormulaGraph> graphs={graph(a,-1),graph(b,-1),graph(c,-1)};std::vector<Skin> skins;
  DeformationRuntime runtime(scene,targets,skins,graphs);std::vector<Properties> values(3);values[0].morphs={0};runtime.evaluate(values,{});const auto before=scene.meshes[2].positions;
  values[0].morphs[0]=1;runtime.evaluate(values,{});values[0].morphs[0]=0;runtime.evaluate(values,{});
  require(distance(before,scene.meshes[2].positions)<1e-6,"GeoGraft 旧接缝参与碰撞，恢复姿势后服装残留形变");
  require(distance(scene.meshes[2].positions,cloth.positions)<1e-6,"服装碰撞没有使用当前接缝位置");
}
static void unit() {
  graft_collision_restore();
  {
    // 共同祖先的平移 / 旋转 / 缩放应完全保留碰撞缓存，包括 GeoGraft。
    ir::Scene scene;ir::Mesh body;body.positions={{-2,-2,0},{2,-2,0},{2,2,0},{-2,2,0}};
    ir::Triangle face;face.vertices={0,1,2};body.triangles.push_back(face);face.vertices={0,2,3};body.triangles.push_back(face);
    auto cloth=body;for(auto &p:cloth.positions) {p.x*=.25f;p.y*=.25f;p.z=-.01f;}
    auto graft=body;for(auto &p:graft.positions) p.z=.01f;graft.graft_target_vertices=4;graft.graft_hidden_polygons={0};
    scene.meshes={body,cloth,graft,body};scene.instances.resize(4);
    TransformValues base;base.translation_cm={123,-45,67};base.rotation_degrees={17,23,31};
    for(uint32_t i=0;i<4;++i) {scene.instances[i].mesh=i;scene.instances[i].transform=make_transform(base);}
    std::vector<Target> targets(4);for(size_t i=0;i<4;++i) {targets[i].instance=uint32_t(i);targets[i].id=std::to_string(i)+"/mesh";}
    targets[1].conform_target=targets[2].conform_target="#0";targets[1].smoothing.enabled=true;targets[1].smoothing.collision_target="#0";
    std::vector<FormulaGraph> graphs;for(const auto &t:targets) graphs.push_back(graph(t,-1));const std::vector<Skin> skins;
    DeformationRuntime runtime(scene,targets,skins,graphs);std::vector<Properties> values(4);runtime.evaluate(values,{});
    const auto initial=scene;const auto collisions=runtime.collision_stats().evaluations;
    for(int step=0;step<24;++step) {
      values[0].transform.translation_cm={step*1.31f,-step*.71f,step*.19f};values[0].transform.rotation_degrees={step*.9f,step*1.3f,step*.7f};values[0].transform.scale={1.1f,.9f,1.2f};
      require(!runtime.prepare(values,{}).pending,"刚性移动触发资源补载");const auto delta=runtime.evaluate(values,{});
      require(delta.meshes.empty()&&runtime.collision_stats().evaluations==collisions,"共同移动重复碰撞或生成了几何更新");
      require(scene.instances[0].transform.value==scene.instances[1].transform.value&&scene.instances[0].transform.value==scene.instances[2].transform.value,"共同移动丢失服装 / GeoGraft 跟随");
      require(scene.instances[3].transform.value==initial.instances[3].transform.value,"实例移动污染无关对象");
      for(size_t m=0;m<scene.meshes.size();++m) require(distance(scene.meshes[m].positions,initial.meshes[m].positions)==0,"刚性移动改变局部顶点");
    }
    values[1].transform.translation_cm.z=.002f;runtime.evaluate(values,{});
    require(runtime.collision_stats().evaluations>collisions,"微小的独立服装移动没有重新碰撞");
    const auto local=runtime.collision_stats().evaluations;values[2].transform.translation_cm.z=2;runtime.evaluate(values,{});
    require(runtime.collision_stats().evaluations>local,"GeoGraft 独立移动没有重新碰撞");
    const auto graft_moved=runtime.collision_stats().evaluations;values[2].visible=false;runtime.evaluate(values,{});
    require(runtime.collision_stats().evaluations>graft_moved,"GeoGraft 可见性变化没有重新碰撞");
  }
  {
    const std::vector<ir::Vec3> a={{1,2,3},{2,2,3},{1,3,3},{2,3,3}},b={{-1,4,5},{-1,5,5},{-2,4,5},{-2,5,5}};
    const auto fit=fit_rigid(a,b);for(size_t i=0;i<a.size();++i) require(distance({fit.point(a[i])},{b[i]})<1e-6,"平面参考组的刚性拟合旋转 / 位移错误");
    ir::Scene scene;ir::Mesh mesh;mesh.positions=a;scene.meshes={mesh,mesh};scene.instances.resize(2);scene.instances[1].mesh=1;
    Target source;source.id="surface/mesh";source.morphs={morph("move",{{0,{.5f,0,0}},{1,{.5f,0,0}},{2,{.5f,0,0}},{3,{.5f,0,0}}})};
    Target follower;follower.id="accessory/mesh";follower.instance=1;follower.rigid_follow={"#surface",4,{0,1,2,3},true};
    std::vector<Target> targets={source,follower};std::vector<Skin> skins;std::vector<FormulaGraph> graphs={graph(source,-1),graph(follower,-1)};
    DeformationRuntime runtime(scene,targets,skins,graphs);std::vector<Properties> values(2);values[0].morphs={1};runtime.evaluate(values,{});
    require(distance({scene.instances[1].transform.point({})},{{.5f,0,0}})<1e-6,"表面 Morph 未带动刚性附件");
    require(runtime.evaluate(values,{}).instances.empty(),"刚性跟随重复求值漂移");
    values[0].transform.translation_cm.x=20;runtime.evaluate(values,{});require(distance({scene.instances[1].transform.point({})},{{.7f,0,0}})<1e-6,"表面跟随重复叠加实例位移");
    values[0].transform={};values[0].morphs={0};runtime.evaluate(values,{});require(distance({scene.instances[1].transform.point({})},{{0,0,0}})<1e-6,"刚性表面跟随恢复失败");
  }
  {
    ir::Scene scene;ir::Mesh mesh;mesh.positions={{0,0,0}};scene.meshes={mesh};scene.instances.resize(1);
    Target t;t.id="body/mesh";t.morphs={morph("shape",{{0,{1,0,0}}})};t.morphs[0].clamped=true;
    std::vector<Target> targets={t};auto g=graph(t,-1);g.channels[0].clamped=true;g.channels[0].minimum=0;g.channels[0].maximum=1;
    std::vector<FormulaGraph> graphs={g};std::vector<Skin> skins;DeformationRuntime runtime(scene,targets,skins,graphs);
    std::vector<Properties> values(1);values[0].morphs={2};runtime.evaluate(values,{});require(scene.meshes[0].positions[0].x==1,"保存通道的 ERC 限幅语义改变");
    set_parameter(t,values[0],0,12.5f);runtime.evaluate(values,{});require(scene.meshes[0].positions[0].x==12.5f&&runtime.effective()[0][0]==12.5f,"手动数值仍被 ERC / Morph 限幅");
    set_parameter(t,values[0],0,-3);runtime.evaluate(values,{});require(scene.meshes[0].positions[0].x==-3,"负 Morph 输入被限幅");
    values[0].unlimited_morphs.clear();values[0].morphs={2};runtime.evaluate(values,{});require(scene.meshes[0].positions[0].x==1,"重置未恢复原始通道语义");
  }
  mesh_collision();
  root_follower_and_visibility();
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
  {const auto before=scene.meshes;const auto evaluations=runtime.skin_stats().evaluations,collisions=runtime.collision_stats().evaluations;auto candidate=poses;candidate[1][1].rotation_degrees.z=25;
    const auto resolved=runtime.resolve_poses(values,candidate);require(resolved[1][1].rotation_degrees.z==25&&resolved[0][1].rotation_degrees.z==25,"骨架预求值没有保留角色和服装的公式依赖");
    require(runtime.skin_stats().evaluations==evaluations&&runtime.collision_stats().evaluations==collisions&&runtime.input_poses()==poses,"IK 校准触发了完整形变或改写输入");
    for(size_t m=0;m<scene.meshes.size();++m) require(scene.meshes[m].positions==before[m].positions,"IK 校准改变了场景顶点");}
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
static void cached_collision(const std::filesystem::path &input,const std::filesystem::path &output) {
  Json data;std::ifstream(input)>>data;ir::Scene scene;std::vector<Target> targets;
  for(const auto &o:data["objects"]) {
    ir::Instance instance;instance.mesh=uint32_t(scene.meshes.size());instance.transform.value=o["transform"].get<std::array<float,12>>();
    const auto inverse=ir::inverse(instance.transform);ir::Mesh mesh;
    mesh.graft_target_vertices=o.value("graft_target_vertices",0u);mesh.graft_hidden_polygons=o.value("graft_hidden_polygons",std::vector<uint32_t>{});
    for(const auto &p:o["uncorrected_world"]) mesh.positions.push_back(inverse.point({p[0],p[1],p[2]}));
    for(size_t f=0;f<o["triangles"].size();++f) {ir::Triangle face;face.vertices=o["triangles"][f].get<std::array<uint32_t,3>>();face.source_polygon=o["triangle_polygons"][f];mesh.triangles.push_back(face);}
    Target t;t.id=o["id"];t.instance=uint32_t(scene.instances.size());t.conform_target=o.value("conform_target","");
    t.smoothing.enabled=o.value("collision_enabled",false);t.smoothing.collision_target=o.value("collision_target","");
    targets.push_back(std::move(t));scene.instances.push_back(instance);scene.meshes.push_back(std::move(mesh));
  }
  auto reference=scene;CollisionRuntime runtime(scene,targets),serial(reference,targets);const auto begin=std::chrono::steady_clock::now();runtime.evaluate({});const auto middle=std::chrono::steady_clock::now();serial.evaluate({},false);const auto end=std::chrono::steady_clock::now();
  for(size_t i=0;i<scene.meshes.size();++i) require(scene.meshes[i].positions==reference.meshes[i].positions,"真实缓存碰撞与串行参考不一致");
  data["collision_comparison"]={{"exact_match",true},{"optimized_ms",std::chrono::duration<double,std::milli>(middle-begin).count()},{"serial_ms",std::chrono::duration<double,std::milli>(end-middle).count()}};
  require(runtime.evaluate({}).meshes.empty(),"缓存碰撞重复求值发生漂移");
  for(size_t i=0;i<targets.size();++i) {auto &world=data["objects"][i]["world"];world=Json::array();for(auto p:scene.meshes[i].positions) {p=scene.instances[i].transform.point(p);world.push_back({p.x,p.y,p.z});}}
  std::ofstream(output)<<data.dump();std::cout<<"Cached collision: PASS\n";
}
int wmain(int argc,wchar_t **argv) {try {if(argc==4&&std::wstring(argv[1])==L"--collision-cache") cached_collision(argv[2],argv[3]);else if(argc>2) actual(argv[1],argv[2],argc>3?std::filesystem::path(argv[3]):std::filesystem::path{});else unit();return 0;} catch(const std::exception &e) {std::cerr<<e.what()<<std::endl;return 1;}}
