#include "runtime/subdivision.h"
#include "daz/subdivision.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <map>
#include <set>
using namespace dfv;
static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
static ir::Mesh quad() {
  ir::Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
  ir::Polygon p;p.vertices={0,1,2,3};p.uv={{0,0},{1,0},{1,1},{0,1}};p.material_slot=7;m.polygons={p};
  ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};m.triangles={a,b};m.subdivision={true,1,2,0,1,0};return m;
}
// 对照真正可绘制的三角面边界，验证追踪包含孔洞边界，且没有使用基础层索引冒充细分层。
static void check_boundaries(ir::Mesh mesh) {
  using Edge=std::pair<uint32_t,uint32_t>;
  auto edge=[](uint32_t a,uint32_t b)->Edge {return {std::min(a,b),std::max(a,b)};};
  for(int algorithm:{0,1,2}) for(int level:{1,2}) {
    mesh.subdivision.algorithm=algorithm;mesh.subdivision.level=level;
    runtime::Subdivision normal(mesh),traced(mesh,false,true);
    const auto positions=traced.evaluate(mesh.positions);
    require(normal.evaluate(mesh.positions)==positions&&normal.triangles()==traced.triangles(),"边界诊断改变了渲染结果");
    require(normal.boundaries().empty(),"默认路径不应保存诊断边界");
    std::map<Edge,int> counts;std::set<Edge> expected,actual;
    for(const auto &t:traced.triangles()) for(int c=0;c<3;++c) ++counts[edge(t.vertices[c],t.vertices[(c+1)%3])];
    for(auto [e,count]:counts) if(count==1) expected.insert(e);
    require(traced.boundaries().size()==4,"可见四边形的边界数量错误");
    for(const auto &b:traced.boundaries()) {
      require(b.a<mesh.positions.size()&&b.b<mesh.positions.size(),"基础边界端点越界");
      require(b.vertices.size()==size_t((1<<level)+1),"细分边界链长度错误");
      for(auto v:b.vertices) require(v<positions.size(),"最终细分顶点越界");
      for(size_t i=1;i<b.vertices.size();++i) actual.insert(edge(b.vertices[i-1],b.vertices[i]));
    }
    require(actual==expected,"追踪边界与最终可见面边界不一致");
  }
}
int main() {
 try {
  check_boundaries(quad());
  {auto hole=quad();hole.positions.push_back({2,0,0});hole.positions.push_back({2,1,0});ir::Polygon p;p.vertices={1,4,5,2};p.uv={{1,0},{2,0},{2,1},{1,1}};hole.polygons.push_back(p);hole.hidden_polygons={1};check_boundaries(hole);}
  auto m=quad();runtime::Subdivision s(m);const auto positions=s.evaluate(m.positions);
  {auto invalid=m;invalid.polygons[0].vertices[1]=invalid.polygons[0].vertices[0];bool failed=false;try {runtime::Subdivision bad(invalid);}catch(...) {failed=true;}require(failed,"重复顶点进入了细分库");}
  {auto invalid=m;invalid.creases.push_back({0,999,1});bool failed=false;try {runtime::Subdivision bad(invalid);}catch(...) {failed=true;}require(failed,"越界折痕进入了细分库");}
  {auto heavy=m;heavy.polygons.resize(1200,heavy.polygons[0]);bool failed=false;try {runtime::validate_subdivision_budget(heavy,6);}catch(...) {failed=true;}require(failed,"高等级在分配前没有进行预算校验");}
  require(positions.size()==9&&s.triangles().size()==8,"原始四边面一级细分应为 9 顶点 / 8 三角面");
  require(positions[0]==m.positions[0],"Sharp Corners 没有保留边界角点");
  bool center=false;for(auto p:positions) center|=p==ir::Vec3{.5f,.5f,0};require(center,"面中心插值错误");
  for(const auto &t:s.triangles()) {require(t.material_slot==7&&t.source_polygon==0,"细分丢失源面及材质槽");for(int c=0;c<3;++c) {auto p=positions[t.vertices[c]];require(std::abs(t.uv[c].x-p.x)<1e-6&&std::abs(t.uv[c].y-p.y)<1e-6,"UV 插值错误");}}
  auto moved=m.positions;for(auto &p:moved) {p.x+=3;p.z+=2;}const auto updated=s.evaluate(moved);
  for(size_t i=0;i<updated.size();++i) require(std::abs(updated[i].x-positions[i].x-3)<1e-6&&std::abs(updated[i].z-2)<1e-6,"缓存模板没有正确应用形变后顶点");
  require(s.evaluate(m.positions)==positions,"细分反复更新产生累计漂移");
  runtime::Subdivision render(m,true);require(render.triangles().size()==32,"渲染最低细分级别未生效");
  m.subdivision.edge_interpolation=2;runtime::Subdivision rounded(m);require(rounded.evaluate(m.positions)[0]==ir::Vec3{.125f,.125f,0},"Sharp Edges 模式的角点平滑错误");
  m.subdivision.algorithm=1;runtime::Subdivision linear(m);require(linear.evaluate(m.positions)[0]==m.positions[0],"Bilinear 错误地平滑了控制点");
  m.subdivision.algorithm=2;runtime::Subdivision loop(m);require(loop.triangles().size()==8,"Loop 三角化 / 一级细分错误");
  m.hidden_polygons={0};runtime::Subdivision hidden(m);require(hidden.triangles().empty(),"GeoGraft 隐藏面在细分后重新出现");
  m.subdivision.enabled=false;runtime::Subdivision base(m);require(!base.active()&&base.evaluate(m.positions)==m.positions,"Base 模式仍然细分");
  m.subdivision.enabled=true;m.subdivision.level=7;bool failed=false;try {runtime::Subdivision invalid(m);}catch(...) {failed=true;}require(failed,"过高级别没有显式拒绝");
  using J=nlohmann::json;
  const auto asset=J::parse(R"({"type":"subdivision_surface","extra":[{"type":"studio_geometry_channels","channels":[{"channel":{"id":"SubDIALevel","value":2}},{"channel":{"id":"SubDRenderLevel","value":3}},{"channel":{"id":"SubDAlgorithmControl","value":0}}]}]})");
  const auto instance=J::parse(R"({"current_subdivision_level":1,"edge_interpolation_mode":"edges_and_corners","extra":[{"type":"studio_geometry_channels","channels":[{"channel":{"id":"SubDAlgorithmControl","current_value":1}}]}]})");
  const auto settings=daz::subdivision_settings(asset,instance);require(settings.enabled&&settings.level==1&&settings.render_level==3&&settings.algorithm==1&&settings.edge_interpolation==1,"资产默认 / 场景覆盖读取错误");
  std::cout<<"Subdivision topology / UV / modes / deformation / masks / DSON: PASS\n";return 0;
 }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
