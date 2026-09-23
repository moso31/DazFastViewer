#include "runtime/subdivision.h"
#include "daz/subdivision.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace dfv;
static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
static ir::Mesh quad() {
  ir::Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
  ir::Polygon p;p.vertices={0,1,2,3};p.uv={{0,0},{1,0},{1,1},{0,1}};p.material_slot=7;m.polygons={p};
  ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};m.triangles={a,b};m.subdivision={true,1,2,0,1,0};return m;
}
int main() {
 try {
  auto m=quad();runtime::Subdivision s(m);const auto positions=s.evaluate(m.positions);
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
