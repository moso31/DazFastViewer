#include "editor/physics_service.h"
#include "runtime/physics_json.h"
#include "editor/scene_extension.h"
#include <chrono>
#include <iostream>
#include <thread>
using namespace dfv;
using namespace dfv::runtime;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static ir::Scene fixture(ir::Mesh mesh){ir::Scene s;s.meshes.push_back(std::move(mesh));ir::Instance i;i.id="object/mesh";s.instances.push_back(i);return s;}
static ir::Mesh cloth(){ir::Mesh m;const uint32_t n=33;for(uint32_t y=0;y<n;++y)for(uint32_t x=0;x<n;++x)m.positions.push_back({x/32.f,y/32.f,1+y/64.f});for(uint32_t y=0;y+1<n;++y)for(uint32_t x=0;x+1<n;++x){auto a=y*n+x;m.triangles.push_back({{a,a+1,a+n}});m.triangles.push_back({{a+1,a+n+1,a+n}});}return m;}
static void rigid(){
  ir::Mesh m;m.positions={{-.1f,-.1f,1},{.1f,.1f,1.2f}};auto s=fixture(m);PhysicsTarget t;t.kind=PhysicsKind::rigid;PhysicsOptions o;
  auto r=settle_physics(s,{t},o);check(r.delta.instances.size()==1,"刚体没有结果");float height=r.delta.instances[0].transform.point(m.positions[0]).z;check(std::abs(height)<.01f,"刚体未落在虚拟地面");
  o.ground=false;r=settle_physics(s,{t},o);check(r.delta.instances[0].transform.point(m.positions[0]).z<-.2f,"关闭地面仍阻止下落");
  o.ground=true;o.ground_height=.5f;r=settle_physics(s,{t},o);check(std::abs(r.delta.instances[0].transform.point(m.positions[0]).z-.5f)<.01f,"DAZ Y 地面高度转换错误");
  check(s.instances[0].transform==ir::Transform{},"求解修改了原始场景");
}
static void soft(PhysicsKind kind){
  auto m=cloth();if(kind==PhysicsKind::hair_curves){m={};for(uint32_t strand=0;strand<240;++strand){ir::Curve c;for(uint32_t v=0;v<25;++v){c.vertices.push_back(uint32_t(m.positions.size()));m.positions.push_back({strand*.001f,v*.015f,1.5f-v*.01f});}m.curves.push_back(c);}}
  auto s=fixture(m);PhysicsTarget t;t.kind=kind;t.settings.max_distance=.2f;auto r=settle_physics(s,{t},{});check(r.delta.meshes.size()==1,"软体缺少结果");const auto &p=r.delta.meshes[0].positions;check(p.size()==m.positions.size(),"改变了源拓扑");size_t moved=0,fixed=0;double max_move=0;
  for(size_t i=0;i<p.size();++i){check(std::isfinite(p[i].x)&&std::isfinite(p[i].y)&&std::isfinite(p[i].z),"软体产生非有限值");const auto a=m.positions[i],b=p[i];double d=std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);max_move=std::max(max_move,d);moved+=d>.001;fixed+=d<1e-6;}
  std::cout<<"soft "<<int(kind)<<" moved="<<moved<<" fixed="<<fixed<<" max="<<max_move<<" particles="<<r.stats.particles<<" ms="<<r.stats.solve_ms<<'\n';
  check(moved>10&&fixed>10,"软体未变形或根部没有固定");check(max_move<.23,"活动距离限制失效");check(r.stats.particles<=4096,"代理超出预算");
  if(kind==PhysicsKind::hair_curves)for(const auto &c:m.curves)check(p[c.vertices.front()]==m.positions[c.vertices.front()],"发根漂移");
  int calls=0;bool cancelled=false;try{settle_physics(s,{t},{},[&]{return ++calls>8;});}catch(const std::exception &){cancelled=true;}check(cancelled,"任务未响应取消");
}
static std::shared_ptr<editor::Document> document(){auto d=std::make_shared<editor::Document>();d->generation=1;d->loaded.scene=fixture(cloth());runtime::Target t;t.id="object/mesh";t.label="衣物";d->catalog.targets={t};d->formulas.graphs.resize(1);return d;}
static void body_collision(){
  auto m=cloth();for(auto &p:m.positions)p.z=1.5f;auto scene=fixture(m);ir::Mesh body;body.positions={{-2,-2,1.35f},{2,-2,1.35f},{2,2,1.35f},{-2,2,1.35f}};body.triangles={{{0,1,2}},{{0,2,3}}};scene.meshes.push_back(body);ir::Instance collider;collider.mesh=1;scene.instances.push_back(collider);
  PhysicsTarget target;target.kind=PhysicsKind::cloth;target.host=1;target.settings.max_distance=.4f;PhysicsOptions options;options.ground=false;auto result=settle_physics(scene,{target},options);float minimum=2;for(auto p:result.delta.meshes[0].positions)minimum=std::min(minimum,p.z);check(minimum>=1.347f&&minimum<1.4f,"宿主静态网格没有阻止布料穿过");
  auto rest=m;scene.instances.resize(1);for(auto &p:scene.meshes[0].positions){p.y=-p.y;p.z=3-p.z;}target.attachment_rest=&rest;target.host=-1;result=settle_physics(scene,{target},options);const auto &positions=result.delta.meshes[0].positions;for(size_t i=0;i<positions.size();++i)if(rest.positions[i].y>.85f)check(std::hypot(positions[i].x-scene.meshes[0].positions[i].x,positions[i].y-scene.meshes[0].positions[i].y,positions[i].z-scene.meshes[0].positions[i].z)<1e-6,"姿态旋转更换了根部固定点");
}
static void settings_and_service(){
  check(!physics_object_from_json(nullptr).enabled,"旧场景默认启用了物理");PhysicsObjectSettings s;s.enabled=true;s.kind=PhysicsKind::cloth;s.fixed_fraction=.3f;s.joined=false;s.excluded_surfaces={"heel"};check(physics_object_from_json(physics_json(s))==s,"物理参数不能往返保存");
  bool rejected=false;try{physics_object_from_json({{"kind",99}});}catch(...){rejected=true;}check(rejected,"无效配置未拒绝");
  auto d=document();check(editor::physics_eligible(*d,0),"普通对象不可启用物理");d->loaded.scene.instances[0].shell_source=0;check(!editor::physics_eligible(*d,0),"Shell 错误出现物理");d->loaded.scene.instances[0].shell_source=-1;
  daz::AssetObject object;object.instance=0;object.figure=true;object.content_type="Actor/Character";d->loaded.objects={object};runtime::Skin skin;skin.instance=0;d->skeletons.skins={skin};check(!editor::physics_eligible(*d,0),"角色本体错误出现物理");
  d->loaded.objects[0].content_type="Follower/Hair";check(editor::physics_eligible(*d,0)&&editor::physics_kind(*d,0)==PhysicsKind::hair_cards,"贴片发型误判");d->loaded.objects.clear();d->skeletons.skins.clear();
  auto snapshot=editor::initial_snapshot(*d);snapshot.values[0].physics=s;auto other=snapshot;other.values[0].physics.mass=2;check(!editor::same_physics_input(snapshot,other),"物理编辑未使缓存失效");
  auto json=editor::snapshot_json(*d,snapshot);editor::apply_snapshot_json(*d,other,json);check(other.values[0].physics==s,"场景快照未恢复物理参数");json["objects"][d->catalog.targets[0].id].erase("physics");editor::apply_snapshot_json(*d,other,json);check(!other.values[0].physics.enabled,"旧场景恢复时错误沿用物理开关");
  editor::PhysicsService service;service.request(d,snapshot,{});snapshot.values[0].physics.enabled=false;const auto serial=service.request(d,snapshot,{});
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);bool finished=false;while(std::chrono::steady_clock::now()<deadline){if(auto r=service.take()){check(r->serial==serial&&r->error.empty()&&!r->scene,"过期结果覆盖最新关闭状态");finished=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}check(finished,"后台任务没有完成");
  snapshot.values[0].physics.enabled=true;snapshot.values[0].physics.run_sequence=1;service.request(d,snapshot,{});deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);finished=false;
  while(std::chrono::steady_clock::now()<deadline){service.request_frame();if(auto r=service.take()){check(r->error.empty(),r->error.c_str());if(!r->scene)continue;check(r->delta.meshes.size()==1&&r->restore.meshes[0].positions==d->loaded.scene.meshes[0].positions,"后台求值或恢复基线错误");finished=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}check(finished,"后台解算超时");
}

static void manual_commands(){
  auto d=document();auto input=editor::initial_snapshot(*d);input.values[0].physics.enabled=true;input.values[0].physics.kind=PhysicsKind::cloth;input.values[0].physics.rounds=2;
  editor::PhysicsService service;uint64_t serial=service.request(d,input,{});
  auto wait=[&](size_t steps){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(std::chrono::steady_clock::now()<until){service.request_frame();if(auto r=service.take()){check(r->error.empty(),r->error.c_str());if(r->serial==serial&&r->stats.steps>=steps){service.acknowledge(r);return r;}}std::this_thread::sleep_for(std::chrono::milliseconds(5));}throw std::runtime_error("运行命令等待超时");};
  check(!wait(0)->scene,"启用但未运行时产生了几何提交");input.values[0].physics.run_sequence=10;serial=service.request(d,input,{});check(wait(2)->scene!=nullptr,"手动启动没有产生结果");
  input.values[0].physics.run_sequence=0;serial=service.request(d,input,{});auto old=wait(2);check(!old->scene&&!old->stats.active_objects&&old->stats.steps==2,"恢复旧快照重放了运行命令");
  input.values[0].physics.run_sequence=11;serial=service.request(d,input,{});check(wait(4)->stats.steps==4,"恢复快照后不能再次手动运行");
  input.values[0].physics.enabled=false;serial=service.request(d,input,{});wait(0);input.values[0].physics.enabled=true;serial=service.request(d,input,{});auto enabled=wait(0);check(!enabled->scene&&!enabled->stats.active_objects&&enabled->stats.steps==0,"重新启用重放了旧运行命令");
}

static double distance(ir::Vec3 a,ir::Vec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
static void continuous(){
  auto scene=std::make_shared<ir::Scene>(fixture(cloth()));PhysicsTarget target;target.kind=PhysicsKind::cloth;target.settings.rounds=20;
  PhysicsSimulation simulation;PhysicsOptions options;simulation.update(scene,{target},options,{});simulation.step();check(simulation.stats().steps==0,"启用后未经点击自行运行");
  ++target.settings.run_sequence;simulation.update(scene,{target},options,{});simulation.step();auto first=simulation.output();check(first.stats.steps==1,"首帧等待了批量落定");
  for(int i=0;i<99;++i)simulation.step();auto before=simulation.output();check(before.stats.steps==20&&!before.stats.active_objects,"没有恰好执行 N 轮");check(before.delta.meshes[0].positions!=first.delta.meshes[0].positions,"没有持续显示中间状态");
  target.settings.rounds=7;simulation.update(scene,{target},options,{});check(simulation.output().delta.meshes[0].positions==before.delta.meshes[0].positions,"调整轮数破坏了当前位置");
  ++target.settings.run_sequence;simulation.update(scene,{target},options,{});simulation.step();check(simulation.stats().steps==21,"继续运行没有从当前状态推进");
  options.paused=true;simulation.options(options);before=simulation.output();for(int i=0;i<10;++i)simulation.step();check(simulation.output().delta.meshes[0].positions==before.delta.meshes[0].positions,"全局暂停改变了状态");
  options.paused=false;simulation.options(options);for(int i=0;i<20;++i)simulation.step();check(simulation.stats().steps==27,"暂停消耗了轮数预算");
  target.settings.damping=4;simulation.update(scene,{target},options,{});check(simulation.output().delta.meshes[0].positions==scene->meshes[0].positions&&!simulation.stats().active_objects,"修改物理参数没有回到初始状态并停止");
  ++target.settings.run_sequence;simulation.update(scene,{target},options,{});simulation.step();++target.settings.reset_sequence;simulation.update(scene,{target},options,{});check(simulation.output().delta.meshes[0].positions==scene->meshes[0].positions&&!simulation.stats().active_objects,"重置未恢复初始状态");
  target.settings.reset_sequence=100;target.settings.run_sequence=101;simulation.update(scene,{target},options,{});simulation.step();check(simulation.stats().active_objects==1,"合并的重置后运行命令丢失");options.refresh_hz=137;auto current=simulation.output().delta.meshes[0].positions;simulation.options(options);check(simulation.output().delta.meshes[0].positions==current,"改变刷新率重置了模拟状态");
}
static ir::Mesh two_parts(){
  ir::Mesh mesh;mesh.material_slots={"heel","laces"};
  for(int part=0;part<2;++part){const auto base=uint32_t(mesh.positions.size());for(auto p:std::vector<ir::Vec3>{{0,0,1},{.15f,0,1},{0,.15f,1.15f},{.15f,.15f,1.15f}}){p.z+=part*.3f;mesh.positions.push_back(p);}mesh.triangles.push_back({{base,base+1,base+2},{},uint32_t(part)});mesh.triangles.push_back({{base+1,base+3,base+2},{},uint32_t(part)});}
  return mesh;
}
static void surfaces(){
  auto scene=std::make_shared<ir::Scene>(fixture(two_parts()));PhysicsTarget target;target.settings.run_sequence=1;target.settings.rounds=180;target.kind=PhysicsKind::rigid;target.settings.excluded_surfaces={"laces"};PhysicsSimulation simulation;simulation.update(scene,{target},{},{});for(int i=0;i<90;++i)simulation.step();auto output=simulation.output();check(output.delta.instances.empty()&&output.delta.meshes.size()==1,"选择部分表面却移动了整个实例");
  const auto &original=scene->meshes[0].positions;auto positions=output.delta.meshes[0].positions;for(size_t i=4;i<8;++i)check(positions[i]==original[i],"未勾选的子材质被改变");check(distance(positions[0],original[0])>.5,"选中的子材质没有下落");
  target.settings.excluded_surfaces.clear();PhysicsSimulation joined;joined.update(scene,{target},{},{});for(int i=0;i<100;++i)joined.step();output=joined.output();check(output.delta.instances.size()==1,"整体刚体没有统一变换");const auto &transform=output.delta.instances[0].transform;check(std::abs(distance(transform.point(original[0]),transform.point(original[4]))-distance(original[0],original[4]))<1e-5,"整体组合散架");
  target.settings.joined=false;PhysicsSimulation split;split.update(scene,{target},{},{});for(int i=0;i<180;++i)split.step();output=split.output();check(output.delta.instances.empty()&&output.delta.meshes.size()==1,"独立模式没有分别模拟子材质");
  target.settings.joined=true;target.kind=PhysicsKind::cloth;target.settings.fixed_fraction=.4f;PhysicsSimulation cloth_joined;cloth_joined.update(scene,{target},{},{});for(int i=0;i<180;++i)cloth_joined.step();const auto connected=cloth_joined.output().delta.meshes[0].positions;
  target.settings.joined=false;PhysicsSimulation cloth_split;cloth_split.update(scene,{target},{},{});for(int i=0;i<180;++i)cloth_split.step();const auto independent=cloth_split.output().delta.meshes[0].positions;
  check(connected[0].z>independent[0].z+.02f,"柔性整体没有连接分离的鞋跟和鞋带网格");
  target.kind=PhysicsKind::rigid;target.settings.joined=true;target.host=1;PhysicsSimulation attached;attached.update(scene,{target},{},{});for(int i=0;i<30;++i)attached.step();output=attached.output();check(std::abs(output.delta.instances[0].transform.point(original[0]).z-original[0].z)<1e-6,"刚性穿戴物脱离角色下落");
  auto shared=std::make_shared<ir::Scene>(fixture(two_parts()));shared->meshes[0].positions.resize(4);shared->meshes[0].triangles={{{0,1,2},{},0},{{0,2,3},{},0},{{1,3,2},{},1}};
  target.host=-1;target.settings.excluded_surfaces={"laces"};PhysicsSimulation boundary;boundary.update(shared,{target},{},{});for(int i=0;i<60;++i)boundary.step();output=boundary.output();for(auto v:{1,2,3})check(output.delta.meshes[0].positions[v]==shared->meshes[0].positions[v],"刚体移动了未选表面的共享边界");
}
static void bounded_hair_collision(){
  auto scene=std::make_shared<ir::Scene>(fixture(cloth()));
  // 发片从碰撞壳附近开始，用逐帧位移检查约束投影是否瞬间弹飞。
  ir::Mesh body;body.positions={{-2,-2,1.2f},{2,-2,1.2f},{2,2,1.2f},{-2,2,1.2f}};body.triangles={{{0,1,2}},{{0,2,3}}};scene->meshes.push_back(body);ir::Instance host;host.mesh=1;scene->instances.push_back(host);
  PhysicsTarget target;target.settings.run_sequence=1;target.settings.rounds=180;target.kind=PhysicsKind::hair_cards;target.host=1;target.settings.thickness=.01f;PhysicsSimulation simulation;simulation.update(scene,{target},{},{1});auto previous=scene->meshes[0].positions;
  for(int step=0;step<150;++step){simulation.step();const auto positions=simulation.output().delta.meshes[0].positions;for(size_t i=0;i<positions.size();++i){check(distance(previous[i],positions[i])<.0081,"头发碰撞发生超出单步预算的弹飞");check(distance(positions[i],scene->meshes[0].positions[i])<=target.settings.max_distance+.001,"头发活动上限失效");}previous=positions;}
}
static void independent_characters(){
  auto d=std::make_shared<editor::Document>();d->generation=23;
  for(uint32_t k=0;k<2;++k){
    const auto host=k*2,garment=host+1;ir::Mesh body;d->loaded.scene.meshes.push_back(body);d->loaded.scene.meshes.push_back(cloth());
    ir::Instance actor;actor.id="actor"+std::to_string(k)+"/mesh";actor.mesh=host;ir::Instance dress;dress.id="dress"+std::to_string(k)+"/mesh";dress.mesh=garment;d->loaded.scene.instances.push_back(actor);d->loaded.scene.instances.push_back(dress);
    Target a;a.id=actor.id;a.instance=host;Target b;b.id=dress.id;b.instance=garment;b.parent="#actor"+std::to_string(k);d->catalog.targets.push_back(a);d->catalog.targets.push_back(b);
    Skin skin;skin.instance=host;for(const char *name:{"hip","head","lHand","rHand","lFoot","rFoot"}){Joint joint;joint.id=name;skin.joints.push_back(joint);}skin.initial.resize(skin.joints.size());d->skeletons.skins.push_back(skin);daz::AssetObject object;object.instance=host;object.figure=true;object.content_type="Actor/Character";d->loaded.objects.push_back(object);
  }
  d->formulas.graphs.resize(4);auto input=editor::initial_snapshot(*d);for(auto t:{1,3}){input.values[t].physics.enabled=true;input.values[t].physics.kind=PhysicsKind::cloth;input.values[t].physics.rounds=600;input.values[t].physics.run_sequence=1;}
  editor::PhysicsService service;auto serial=service.request(d,input,{});
  auto wait=[&](auto predicate){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(std::chrono::steady_clock::now()<until){service.request_frame();if(auto r=service.take()){check(r->error.empty(),r->error.c_str());if(r->serial==serial&&predicate(*r)){service.acknowledge(r);return r;}}std::this_thread::sleep_for(std::chrono::milliseconds(5));}throw std::runtime_error("独立角色物理超时");};
  auto before=wait([](const auto &r){return r.group_steps.size()==2&&r.group_steps.at(0)>=6;});
  input.values[1].physics.paused=true;serial=service.request(d,input,{});auto frozen=wait([](const auto &r){return r.group_steps.size()==2;});
  auto later=wait([&](const auto &r){return r.group_steps.at(2)>=frozen->group_steps.at(2)+5;});check(later->group_steps.at(0)==frozen->group_steps.at(0),"暂停一个角色后自身仍在推进");
  auto positions=[&](const auto &r,uint32_t index){for(const auto &e:r.delta.meshes)if(e.index==index)return e.positions;throw std::runtime_error("缺少角色输出");};check(std::none_of(later->delta.meshes.begin(),later->delta.meshes.end(),[](const auto &e){return e.index==1;}),"无变化对象仍然触发网格更新");
  input.values[1].physics.mass=2;serial=service.request(d,input,{});auto edited=wait([](const auto &r){return r.group_steps.size()==2;});check(edited->group_steps.at(2)>=later->group_steps.at(2)&&positions(*edited,1)==d->loaded.scene.meshes[1].positions,"修改参数没有单独重置被修改角色");
  input.values[1].physics.enabled=false;serial=service.request(d,input,{});auto disabled=wait([](const auto &r){return r.group_steps.size()==1;});check(disabled->group_steps.at(2)>=edited->group_steps.at(2),"关闭一个角色重置另一个角色");check(std::none_of(disabled->delta.meshes.begin(),disabled->delta.meshes.end(),[](const auto &e){return e.index==1;}),"已经恢复的对象重复发送恢复更新");
  auto shared=std::make_shared<editor::Document>(*d);++shared->generation;shared->loaded.scene.instances.push_back(shared->loaded.scene.instances[1]);input.generation=shared->generation;input.values[1].physics.enabled=true;serial=service.request(shared,input,{});
  auto partial=wait([](const auto &r){return r.stats.objects==1&&!r.stats.warning.empty();});check(partial->group_steps.contains(2),"不支持的附件阻止了另一个角色模拟");
}
int main(){try{rigid();soft(PhysicsKind::cloth);soft(PhysicsKind::hair_cards);soft(PhysicsKind::hair_curves);body_collision();settings_and_service();manual_commands();continuous();surfaces();bounded_hair_collision();independent_characters();std::cout<<"PASS\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
