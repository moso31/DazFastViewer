#include "runtime/weight.h"
#include "runtime/pose_edit.h"
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace dfv::runtime {
WeightCalibration calibrate_weight(double liters,double height) {
  if(!std::isfinite(liters)||liters<0||!std::isfinite(height)||height<=0) throw std::runtime_error("体重标定需要非负体积和正数身高");
  const double q=std::pow(height/160.,3),u=liters/q;
  const double loq=std::pow(141.26960385/160.,3),hiq=std::pow(159.74123669/160.,3);
  const double lo=17.28792285/loq,hi=54.59738645/hiq,lc=25./17.28792285,hc=25./22.54333905;
  const double slope=(54.59738645*hc/hiq-25./loq)/(hi-lo),intercept=25./loq-slope*lo;
  const double coefficient=u<=lo?lc:u<hi?(slope*u+intercept)/u:hc;
  const double mass=liters*coefficient;
  if(!std::isfinite(q)||q<=0||!std::isfinite(u)||!std::isfinite(mass)) throw std::runtime_error("体重标定超出计算范围");
  return {mass,coefficient,u<=lo?"极瘦段":u<hi?"过渡段":"高体重段"};
}
namespace {
struct V {double x=0,y=0,z=0;V operator+(V b) const{return {x+b.x,y+b.y,z+b.z};}V operator-(V b) const{return {x-b.x,y-b.y,z-b.z};}V operator*(double s) const{return {x*s,y*s,z*s};}};
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct Sum {double value=0,error=0;void add(double v){const double y=v-error,t=value+y;error=(t-value)-y;value=t;}};
using Edge=std::pair<uint32_t,uint32_t>;
struct Component {std::vector<Edge> edges;std::vector<uint32_t> vertices;};
struct Region {
  std::string label;std::vector<std::array<uint32_t,3>> triangles;std::vector<uint32_t> vertices;
  std::vector<Component> boundaries;size_t boundary_edges=0,nonmanifold=0;
};
Region region(const ir::Mesh &mesh,const std::string &label,const std::set<std::string> &names={}) {
  Region r;r.label=label;std::map<Edge,std::pair<size_t,Edge>> edges;std::set<uint32_t> verts;
  auto face=[&](const auto &indices,uint32_t group){
    if(!names.empty()&&(group>=mesh.polygon_groups.size()||!names.contains(mesh.polygon_groups[group]))) return;
    if(indices.size()<3) return;
    for(auto v:indices) {if(v>=mesh.positions.size()) throw std::runtime_error("重量网格索引无效");verts.insert(v);}
    for(size_t i=1;i+1<indices.size();++i) r.triangles.push_back({indices[0],indices[i],indices[i+1]});
    for(size_t i=0;i<indices.size();++i) {const uint32_t a=indices[i],b=indices[(i+1)%indices.size()];auto &e=edges[std::minmax(a,b)];++e.first;e.second={a,b};}
  };
  if(!mesh.polygons.empty()) for(const auto &p:mesh.polygons) face(p.vertices,p.polygon_group);
  else for(const auto &p:mesh.triangles) face(p.vertices,p.polygon_group);
  r.vertices.assign(verts.begin(),verts.end());std::vector<Edge> boundary;std::map<uint32_t,std::vector<size_t>> links;
  for(const auto &[key,e]:edges) {if(e.first==1){links[e.second.first].push_back(boundary.size());links[e.second.second].push_back(boundary.size());boundary.push_back(e.second);}else if(e.first!=2) ++r.nonmanifold;}
  r.boundary_edges=boundary.size();std::vector<bool> visited(boundary.size());
  for(size_t i=0;i<boundary.size();++i) if(!visited[i]) {
    Component c;std::set<uint32_t> unique;std::vector<size_t> stack{i};visited[i]=true;
    while(!stack.empty()) {const auto e=boundary[stack.back()];stack.pop_back();c.edges.push_back(e);
      for(auto v:{e.first,e.second}) {unique.insert(v);for(auto n:links[v]) if(!visited[n]) {visited[n]=true;stack.push_back(n);}}
    }
    c.vertices.assign(unique.begin(),unique.end());r.boundaries.push_back(std::move(c));
  }
  return r;
}
V centroid(const std::vector<uint32_t> &indices,const std::vector<V> &p) {V v;for(auto i:indices)v=v+p[i];return indices.empty()?v:v*(1./indices.size());}
double volume(const Region &r,const std::vector<V> &p,bool cap,double orientation) {
  Sum sum;for(const auto &t:r.triangles) sum.add(dot(p[t[0]],cross(p[t[1]],p[t[2]])));
  if(cap) {const auto center=centroid(r.vertices,p);for(const auto &b:r.boundaries) {
    if(b.vertices.size()<3||b.edges.size()<3) continue;
    const auto c=centroid(b.vertices,p);auto out=c-center;if(dot(out,out)<1e-18) out={0,0,1};
    for(const auto &[a,d]:b.edges) {const double v=dot(c,cross(p[a],p[d]));sum.add((dot(cross(p[a]-c,p[d]-c),out)>=0?1:-1)*orientation*v);}
  }}
  return std::abs(sum.value/6.)*1000; // m³ -> L
}
}
struct WeightTopology::Data {size_t vertices=0;Region whole;std::vector<Region> parts;};
WeightTopology::WeightTopology(const ir::Mesh &mesh) {
  auto d=std::make_shared<Data>();d->vertices=mesh.positions.size();d->whole=region(mesh,"全身");
  { // 对称肢体固定取一侧，不叠加或平均两侧重量。
    const std::string prefix="l";
    auto names=[&](std::initializer_list<const char *> list){std::set<std::string> r;for(auto n:list)r.insert(prefix+n);return r;};
    auto merge=[](std::initializer_list<std::set<std::string>> lists){std::set<std::string> r;for(const auto &l:lists)r.insert(l.begin(),l.end());return r;};
    auto upper=names({"Shoulder","Shldr","UpperArm","ArmUpper","ShldrBend","ShldrTwist"});
    auto fore=names({"Forearm","LowerArm","ArmLower","ForearmBend","ForearmTwist"});
    auto hand=names({"Hand","Carpal1","Carpal2","Carpal3","Carpal4"});
    std::vector<std::set<std::string>> fingers;for(const auto &stem:{"Thumb","Index","Mid","Ring","Pinky"}) {std::set<std::string> f;for(int i=1;i<=3;++i)f.insert(prefix+stem+std::to_string(i));fingers.push_back(f);hand.insert(f.begin(),f.end());}
    auto thigh=names({"ThighBend","ThighTwist","Thigh"}),shin=names({"Shin"});
    auto foot=names({"Foot","Metatarsals","Toe","BigToe","BigToe_2","SmallToe1","SmallToe1_2","IndexToe","IndexToe_2","SmallToe2","SmallToe2_2","MidToe","MidToe_2","SmallToe3","SmallToe3_2","RingToe","RingToe_2","SmallToe4","SmallToe4_2","PinkyToe","PinkyToe_2"});
    const std::vector<std::pair<std::string,std::set<std::string>>> parts={{"手（含手指）",hand},{"前臂",fore},{"上臂",upper},{"整条手臂",merge({upper,fore,hand})},{"脚（含脚趾）",foot},{"小腿",shin},{"大腿",thigh},{"整条腿",merge({thigh,shin,foot})}};
    for(const auto &[label,n]:parts)d->parts.push_back(region(mesh,label,n));
    const char *labels[]={"拇指","食指","中指","无名指","小指"};for(size_t i=0;i<5;++i)d->parts.push_back(region(mesh,labels[i],fingers[i]));
  }
  data_=std::move(d);
}
WeightResult WeightTopology::measure(const std::vector<ir::Vec3> &positions,const ir::Transform &world,bool character,double density) const {
  if(positions.empty()||positions.size()!=data_->vertices||data_->whole.triangles.empty()) throw std::runtime_error("对象没有可测量的面片网格");
  if(!std::isfinite(density)||density<0) throw std::runtime_error("密度必须为非负有限值");
  std::vector<V> p;V lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};const auto &m=world.value;
  // 先丢弃世界平移，用 double 完成线性变换，远离原点也不会损失小部位精度。
  for(auto v:positions) {V a{double(m[0])*v.x+double(m[1])*v.y+double(m[2])*v.z,double(m[4])*v.x+double(m[5])*v.y+double(m[6])*v.z,double(m[8])*v.x+double(m[9])*v.y+double(m[10])*v.z};
    if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z))throw std::runtime_error("测量网格坐标无效");
    lo={std::min(lo.x,a.x),std::min(lo.y,a.y),std::min(lo.z,a.z)};hi={std::max(hi.x,a.x),std::max(hi.y,a.y),std::max(hi.z,a.z)};p.push_back(a);
  }
  const auto center=(lo+hi)*.5;const auto rotation=rotation_frame(world);const V up{rotation.value[2],rotation.value[6],rotation.value[10]};
  double min_height=INFINITY,max_height=-INFINITY;for(auto &v:p){v=v-center;const auto h=dot(v,up);min_height=std::min(min_height,h);max_height=std::max(max_height,h);}
  WeightResult result;result.height_cm=(max_height-min_height)*100;result.liters=volume(data_->whole,p,false,1);result.boundary_edges=data_->whole.boundary_edges;result.nonmanifold_edges=data_->whole.nonmanifold;
  if(character){const auto c=calibrate_weight(result.liters,result.height_cm);result.kg=c.kg;result.coefficient=c.kg_per_liter;result.branch=c.branch;
    const double determinant=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-double(m[1])*(double(m[4])*m[10]-double(m[6])*m[8])+double(m[2])*(double(m[4])*m[9]-double(m[5])*m[8]);
    for(const auto &r:data_->parts) {const double liters=volume(r,p,true,determinant<0?-1:1);result.parts.push_back({r.label,liters,liters*result.coefficient,r.triangles.size(),r.boundary_edges,r.nonmanifold});}
  }else{result.coefficient=density/1000;result.kg=result.liters*result.coefficient;}
  if(!std::isfinite(result.kg)) throw std::runtime_error("重量超出计算范围");
  if(result.boundary_edges||result.nonmanifold_edges) result.notes.push_back("网格存在开口或非流形边，重量仅供粗略参考");
  return result;
}
}
