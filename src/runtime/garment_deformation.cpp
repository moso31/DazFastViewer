#include "runtime/garment_deformation.h"
#include "runtime/rigid_follow.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>

namespace dfv::runtime {
namespace {
ir::Vec3 add(ir::Vec3 a,ir::Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
ir::Vec3 mul(ir::Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
std::string region_id(std::string name) {
  if(name.size()>1&&(name[0]=='l'||name[0]=='r')&&std::isupper(static_cast<unsigned char>(name[1])))name.erase(0,1);
  if(name=="Toe")name="Toes";
  return name;
}
// 衣物的鞋面、鞋底、衣领常以未焊接的拼片搭接。距离场跨拼片连续，
// 不能只沿拓扑边扩散，否则相邻布片会在约束边界处被拉开。
class RegionPoints {
  const std::vector<ir::Vec3> &points_;
  std::vector<uint32_t> vertices_;
  static float component(ir::Vec3 p,int axis){return axis==0?p.x:axis==1?p.y:p.z;}
  void build(size_t begin,size_t end,int axis) {
    if(begin==end)return;const auto mid=(begin+end)/2;
    std::nth_element(vertices_.begin()+begin,vertices_.begin()+mid,vertices_.begin()+end,[&](auto a,auto b){return component(points_[a],axis)<component(points_[b],axis);});
    build(begin,mid,(axis+1)%3);build(mid+1,end,(axis+1)%3);
  }
  void nearest(size_t begin,size_t end,int axis,ir::Vec3 p,float &best) const {
    if(begin==end)return;const auto mid=(begin+end)/2;const auto q=points_[vertices_[mid]],d=sub(p,q);best=std::min(best,d.x*d.x+d.y*d.y+d.z*d.z);
    const float side=component(p,axis)-component(q,axis);
    if(side<0){nearest(begin,mid,(axis+1)%3,p,best);if(side*side<best)nearest(mid+1,end,(axis+1)%3,p,best);}
    else {nearest(mid+1,end,(axis+1)%3,p,best);if(side*side<best)nearest(begin,mid,(axis+1)%3,p,best);}
  }
public:
  RegionPoints(const std::vector<ir::Vec3> &points,const std::vector<uint32_t> &vertices):points_(points),vertices_(vertices){build(0,vertices_.size(),0);}
  float distance(ir::Vec3 p,float limit) const {float best=limit*limit;nearest(0,vertices_.size(),0,p,best);return std::sqrt(best);}
};
}
std::vector<DeformationRegion> deformation_regions(const ir::Mesh &mesh) {
  std::set<uint32_t> used;for(const auto &f:mesh.triangles)if(f.polygon_group<mesh.polygon_groups.size())used.insert(f.polygon_group);
  std::vector<DeformationRegion> result;
  for(auto group:used) {
    const auto id=region_id(mesh.polygon_groups[group]);
    auto found=std::find_if(result.begin(),result.end(),[&](const auto &r){return r.id==id;});
    if(found==result.end()){result.push_back({id,id,{}});found=std::prev(result.end());}
    found->groups.push_back(group);
  }
  return result;
}
GarmentDeformation::GarmentDeformation(const ir::Mesh &mesh):regions_(deformation_regions(mesh)),groups_(mesh.polygon_groups.size()),representatives_(mesh.positions.size()) {
  // 仅共享约束，不焊接或改写拓扑；UV / 材质拆点使用相同的边界位移。
  std::map<std::array<float,3>,uint32_t> coincident;
  for(uint32_t v=0;v<mesh.positions.size();++v){const auto p=mesh.positions[v];representatives_[v]=coincident.emplace(std::array{p.x,p.y,p.z},v).first->second;}
  for(const auto &f:mesh.triangles)for(int k=0;k<3;++k) {
    const auto v=representatives_.at(f.vertices[k]);
    if(f.polygon_group<groups_.size())groups_[f.polygon_group].push_back(v);
  }
  for(auto &list:groups_){std::sort(list.begin(),list.end());list.erase(std::unique(list.begin(),list.end()),list.end());}
}
void GarmentDeformation::constrain(const std::set<std::string> &disabled,const std::vector<ir::Vec3> &reference,std::vector<ir::Vec3> &offsets,bool uniform_scale,bool transition) const {
  if(disabled.empty())return;
  if(reference.size()!=representatives_.size()||offsets.size()!=reference.size())throw std::runtime_error("服装形变约束顶点数量不一致");
  if(std::all_of(offsets.begin(),offsets.end(),[](ir::Vec3 p){return p==ir::Vec3{};}))return;
  std::vector<int> owner(reference.size(),-1),parent(groups_.size());std::iota(parent.begin(),parent.end(),0);
  auto root=[&](int g){while(parent[g]!=g){parent[g]=parent[parent[g]];g=parent[g];}return g;};
  for(const auto &r:regions_)if(disabled.contains(r.id))for(auto g:r.groups)for(auto v:groups_[g]) {
    if(owner[v]>=0)parent[root(owner[v])]=root(int(g));else owner[v]=int(g);
  }
  std::map<int,std::vector<uint32_t>> islands;
  for(uint32_t v=0;v<owner.size();++v)if(owner[v]>=0)islands[root(owner[v])].push_back(v);
  if(islands.empty())return;
  if(!transition) {
    const auto squared=[](ir::Vec3 p){return p.x*p.x+p.y*p.y+p.z*p.z;};
    for(size_t v=0;v<offsets.size();++v){const auto r=representatives_[v];if(squared(offsets[v])>squared(offsets[r]))offsets[r]=offsets[v];}
    for(size_t v=0;v<offsets.size();++v)offsets[v]=offsets[representatives_[v]];
  }
  std::vector<uint32_t> flexible;
  if(!transition)for(const auto &r:regions_)if(!disabled.contains(r.id))for(auto g:r.groups)flexible.insert(flexible.end(),groups_[g].begin(),groups_[g].end());
  std::sort(flexible.begin(),flexible.end());flexible.erase(std::unique(flexible.begin(),flexible.end()),flexible.end());
  const RegionPoints boundary(reference,flexible);
  std::vector<ir::Vec3> correction(reference.size());std::vector<float> blending(reference.size());
  for(const auto &[group,vertices]:islands) {
    std::vector<ir::Vec3> a,b;a.reserve(vertices.size());b.reserve(vertices.size());
    for(auto v:vertices){a.push_back(reference[v]);b.push_back(add(reference[v],offsets[v]));}
    auto fit=fit_rigid(a,b);
    if(uniform_scale) {
      ir::Vec3 center_a,center_b;const float n=float(a.size());
      for(size_t i=0;i<a.size();++i){center_a=add(center_a,mul(a[i],1/n));center_b=add(center_b,mul(b[i],1/n));}
      double numerator=0,denominator=0;
      for(size_t i=0;i<a.size();++i){const auto x=sub(a[i],center_a),y=sub(b[i],center_b);const ir::Vec3 r{fit.value[0]*x.x+fit.value[1]*x.y+fit.value[2]*x.z,fit.value[4]*x.x+fit.value[5]*x.y+fit.value[6]*x.z,fit.value[8]*x.x+fit.value[9]*x.y+fit.value[10]*x.z};numerator+=double(r.x)*y.x+double(r.y)*y.y+double(r.z)*y.z;denominator+=double(x.x)*x.x+double(x.y)*x.y+double(x.z)*x.z;}
      const float scale=denominator>1e-20&&numerator>0?float(numerator/denominator):1;
      for(int r=0;r<3;++r){for(int c=0;c<3;++c)fit.value[r*4+c]*=scale;fit.value[r*4+3]=0;}
      const auto translation=sub(center_b,fit.point(center_a));fit.value[3]=translation.x;fit.value[7]=translation.y;fit.value[11]=translation.z;
    }
    for(auto v:vertices)correction[v]=sub(fit.point(reference[v]),add(reference[v],offsets[v]));
    ir::Bounds bounds;for(auto v:vertices)bounds.add(reference[v]);const float width=bounds.extent()*.35f;
    // 接缝保留柔性避让带：关闭部位的内部保形，边界不能被刚性投影压回身体里。
    if(!transition&&!flexible.empty()&&width>0)for(auto v:vertices){const float x=boundary.distance(reference[v],width)/width;correction[v]=mul(correction[v],x*x*x*(x*(x*6-15)+10));}
    if(width>0&&transition) {
      RegionPoints points(reference,vertices);
      for(size_t v=0;v<reference.size();++v)if(owner[v]<0&&representatives_[v]==v) {
        const float distance=points.distance(reference[v],width);if(distance>=width)continue;
        const float x=distance/width,weight=1-x*x*x*(x*(x*6-15)+10);
        correction[v]=add(correction[v],mul(sub(fit.point(reference[v]),add(reference[v],offsets[v])),weight));blending[v]+=weight;
      }
    }
  }
  for(size_t v=0;v<correction.size();++v)if(owner[v]<0&&blending[v]>1)correction[v]=mul(correction[v],1/blending[v]);
  for(size_t v=0;v<offsets.size();++v)if(representatives_[v]==v)offsets[v]=add(offsets[v],correction[v]);
  // 碰撞可能分别推开同位置的拆点；约束区共享最终位移，而不只是共享修正量。
  for(size_t v=0;v<offsets.size();++v)if(representatives_[v]!=v){const auto r=representatives_[v];if(!transition||owner[r]>=0||blending[r]>0)offsets[v]=offsets[r];}
}
}
