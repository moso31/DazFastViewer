#include "runtime/graft_surface.h"
#include "editor/document.h"
#include "bench/graft_fixture.h"
#include <iostream>
#include <set>
#include <cmath>
#include <stdexcept>
using namespace dfv;
static void require(bool v,const char *message) {if(!v) throw std::runtime_error(message);}
static bool close(ir::Vec3 a,ir::Vec3 b) {return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<2e-6;}
int main() {try {
  auto scene=bench::graft_fixture();scene.validate();auto groups=runtime::graft_groups(scene);require(groups==std::vector<std::vector<uint32_t>>{{0,1}},"附件分组错误");
  runtime::GraftSurface surface(scene,groups[0],false);
  // 手工建立完整曲面的参考拓扑，不经过 GeoGraft 分组或索引拼接实现。
  auto expected=scene.meshes[0];expected.hidden_polygons.clear();expected.polygons.erase(expected.polygons.begin()+4);expected.positions.push_back({1.5f,1.5f,.6f});
  const uint32_t boundary[]={5,6,10,9};
  for(int i=0;i<4;++i) {auto p=scene.meshes[1].polygons[i];p.vertices={boundary[i],boundary[(i+1)%4],16};expected.polygons.push_back(p);}
  runtime::Subdivision reference(expected);const auto points=reference.evaluate(expected.positions);
  require(points.size()==surface.positions().size(),"共同细分与完整曲面的顶点数量不符");
  for(size_t i=0;i<points.size();++i) require(close(points[i],surface.positions()[i]),"共同细分位置不等于完整曲面参考");
  std::set<uint32_t> body_vertices(surface.vertices(0).begin(),surface.vertices(0).end());size_t shared=0;
  for(auto v:surface.vertices(1)) if(body_vertices.contains(v)) {++shared;auto n=surface.normals()[v];require(std::abs(std::hypot(n.x,n.y,n.z)-1)<1e-5,"共同边界法线无效");}
  require(shared==16,"2 级边界没有共享全部细分顶点");
  for(const auto &t:surface.triangles(1)) {require(t.material_slot==1&&t.source_polygon<4,"附件材质槽或面来源丢失");for(auto uv:t.uv) require(uv.x>2,"共享几何错误地合并了面角 UV");}
  require(!surface.evaluate(scene),"静止场景重复细分");
  auto changed=scene;changed.meshes[1].positions[4].z+=.2f;auto moved=surface;require(moved.evaluate(changed),"附件内部 Morph 未更新共同曲面");require(moved.positions()!=surface.positions(),"附件变形没有影响接缝邻域");
  require(moved.evaluate(scene)&&moved.positions()==surface.positions(),"恢复 Morph 出现累计漂移");
  changed=scene;changed.instances[1].visible=false;require(!moved.evaluate(changed),"显隐不应改变共同拓扑／法线");
  changed=scene;for(auto &i:changed.instances) i.transform=ir::Transform::translate({2,0,0})*i.transform;
  moved.evaluate(changed);for(size_t i=0;i<points.size();++i) require(close(moved.positions()[i],surface.positions()[i]),"共同移动改变了局部形状");
  require(moved.transform(changed,0)==moved.transform(changed,1),"部件没有共享宿主渲染坐标系");
  auto copy=scene.instances[1];copy.id="graft-copy";copy.prototype=1;copy.graft_source=-1;copy.transform=ir::Transform::translate({10,0,0})*copy.transform;changed=scene;changed.instances.push_back(copy);
  require(runtime::graft_groups(changed)==groups,"DAZ Instance 重复参与控制网格");
  const auto a=surface.transform(changed,2).point(points[0]),b=surface.transform(changed,0).point(points[0]);require(close(a,{b.x+10,b.y,b.z}),"附件原型实例变换错误");
  changed=scene;changed.instances[0].graft_source=1;bool rejected=false;try {runtime::graft_groups(changed);}catch(...) {rejected=true;}require(rejected,"没有拒绝附件循环");
  changed=scene;changed.meshes[1].graft_vertex_pairs[0][1]=100;rejected=false;try {runtime::GraftSurface bad(changed,{0,1},false);}catch(...) {rejected=true;}require(rejected,"无效顶点映射进入 OpenSubdiv");
  changed=scene;changed.instances.pop_back();changed.meshes[0].hidden_polygons.clear();require(runtime::graft_groups(changed).empty(),"删除附件后仍保留共同细分组");
  changed=scene;changed.meshes[0].subdivision.level=0;changed.meshes[0].subdivision.render_level=0;changed.meshes[0].subdivision.enabled=false;runtime::GraftSurface base(changed,{0,1},false);require(base.positions().size()==17,"0 级没有恢复共同控制网格");
  editor::Document document;document.loaded.scene=scene;runtime::Target target;target.instance=1;document.catalog.targets.push_back(target);auto snapshot=editor::initial_snapshot(document);
  require(editor::subdivision_level(document,snapshot,0)==2,"附件界面未显示宿主细分等级");snapshot.subdivision_levels[editor::subdivision_mesh(document,0).id]=1;editor::apply_subdivision_levels(document.loaded.scene,snapshot.subdivision_levels);
  require(document.loaded.scene.meshes[0].subdivision.level==1&&document.loaded.scene.meshes[1].subdivision.level==1,"附件修改未同步共同等级");
  // 嵌套附件替换父附件的一面，不能直接把其顶点编号映射到根宿主。
  changed=scene;auto nested=changed.meshes[1];nested.id="nested";nested.graft_target_vertices=5;nested.graft_target_polygons=4;nested.graft_vertex_pairs={{0,0},{1,1},{2,4}};nested.graft_hidden_polygons={0};nested.positions={nested.positions[0],nested.positions[1],nested.positions[4]};nested.polygons.resize(1);nested.polygons[0].vertices={0,1,2};nested.triangles.resize(1);nested.triangles[0].vertices={0,1,2};nested.source_polygon_count=1;changed.meshes[1].hidden_polygons={0};changed.meshes.push_back(nested);
  auto nested_instance=changed.instances[1];nested_instance.id="nested";nested_instance.mesh=2;nested_instance.graft_source=1;changed.instances.push_back(nested_instance);auto nested_groups=runtime::graft_groups(changed);require(nested_groups==std::vector<std::vector<uint32_t>>{{0,1,2}},"嵌套次序错误");runtime::GraftSurface nested_surface(changed,nested_groups[0],false);
  require(nested_surface.positions().size()==surface.positions().size(),"嵌套替换引入重复顶点");for(auto p:nested_surface.positions()) require(std::any_of(surface.positions().begin(),surface.positions().end(),[&](auto q){return close(p,q);}),"嵌套替换改变相同几何");
  std::cout<<"GeoGraft shared topology / normals / UV / morph / instances / subdivision: PASS\n";return 0;
}catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}}
