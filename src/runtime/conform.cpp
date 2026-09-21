#include "runtime/conform.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>

namespace dfv::runtime {
namespace {
ir::Vec3 add(ir::Vec3 a,ir::Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
ir::Vec3 mul(ir::Vec3 a,float v) {return {a.x*v,a.y*v,a.z*v};}
float dot(ir::Vec3 a,ir::Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
ir::Vec3 normal(ir::Vec3 a,ir::Vec3 b) {const ir::Vec3 n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};const float length=std::sqrt(dot(n,n));return length>0?mul(n,1/length):ir::Vec3{};}
ir::Vec3 direction(const ir::Transform &m,ir::Vec3 v) {const auto &a=m.value;return {a[0]*v.x+a[1]*v.y+a[2]*v.z,a[4]*v.x+a[5]*v.y+a[6]*v.z,a[8]*v.x+a[9]*v.y+a[10]*v.z};}
float box_distance(const ir::Bounds &b,ir::Vec3 p) {
  const auto q=sub(p,{std::clamp(p.x,b.minimum.x,b.maximum.x),std::clamp(p.y,b.minimum.y,b.maximum.y),std::clamp(p.z,b.minimum.z,b.maximum.z)});return dot(q,q);
}
// 最近点由平面内投影及三个边界线段共同竞争，退化三角形仍有定义。
ir::Vec3 closest_barycentric(ir::Vec3 p,ir::Vec3 a,ir::Vec3 b,ir::Vec3 c) {
  const auto ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);const double aa=dot(ab,ab),bb=dot(ac,ac),abac=dot(ab,ac),u=dot(ap,ab),v=dot(ap,ac),det=aa*bb-abac*abac;
  if(det>1e-12*aa*bb) {const double y=(bb*u-abac*v)/det,z=(aa*v-abac*u)/det;if(y>=0&&z>=0&&y+z<=1) return {float(1-y-z),float(y),float(z)};}
  float best=std::numeric_limits<float>::max();ir::Vec3 result;
  const ir::Vec3 vertices[]={a,b,c},basis[]={{1,0,0},{0,1,0},{0,0,1}};
  for(int i=0;i<3;++i) {const int j=(i+1)%3;const auto edge=sub(vertices[j],vertices[i]);const float length=dot(edge,edge),t=length>0?std::clamp(dot(sub(p,vertices[i]),edge)/length,0.f,1.f):0;
    const auto d=sub(p,add(vertices[i],mul(edge,t)));const float distance=dot(d,d);if(distance<best) {best=distance;result=add(mul(basis[i],1-t),mul(basis[j],t));}}
  return result;
}
class SurfaceIndex {
  struct Branch {ir::Bounds bounds;size_t begin=0,end=0;int left=-1,right=-1;};
  const ir::Mesh &mesh_;std::vector<size_t> triangles_;std::vector<Branch> branches_;
  int build(size_t begin,size_t end) {
    Branch branch;branch.begin=begin;branch.end=end;
    for(size_t i=begin;i<end;++i) for(auto v:mesh_.triangles[triangles_[i]].vertices) branch.bounds.add(mesh_.positions[v]);
    const int index=int(branches_.size());branches_.push_back(branch);
    if(end-begin>12) {const auto extent=sub(branch.bounds.maximum,branch.bounds.minimum);const int axis=extent.x>extent.y?(extent.x>extent.z?0:2):(extent.y>extent.z?1:2);
      auto center=[&](size_t face) {float sum=0;for(auto v:mesh_.triangles[face].vertices) {const auto p=mesh_.positions[v];sum+=axis==0?p.x:axis==1?p.y:p.z;}return sum;};
      const size_t middle=(begin+end)/2;std::nth_element(triangles_.begin()+begin,triangles_.begin()+middle,triangles_.begin()+end,[&](size_t a,size_t b) {return center(a)<center(b);});
      const int left=build(begin,middle),right=build(middle,end);branches_[index].left=left;branches_[index].right=right;}
    return index;
  }
public:
  explicit SurfaceIndex(const ir::Mesh &mesh):mesh_(mesh) {triangles_.resize(mesh.triangles.size());std::iota(triangles_.begin(),triangles_.end(),0);if(!triangles_.empty()) build(0,triangles_.size());}
  SurfaceBinding nearest(ir::Vec3 p) const {
    if(branches_.empty()) throw std::runtime_error("Fit To 目标没有可用于形变转移的三角形");
    float best=std::numeric_limits<float>::max();SurfaceBinding result;std::vector<int> stack{0};
    while(!stack.empty()) {const auto &b=branches_[stack.back()];stack.pop_back();if(box_distance(b.bounds,p)>best) continue;
      if(b.left>=0) {const bool left_first=box_distance(branches_[b.left].bounds,p)<box_distance(branches_[b.right].bounds,p);stack.push_back(left_first?b.right:b.left);stack.push_back(left_first?b.left:b.right);continue;}
      for(size_t i=b.begin;i<b.end;++i) {const auto vertices=mesh_.triangles[triangles_[i]].vertices;const auto a=mesh_.positions[vertices[0]],c=mesh_.positions[vertices[1]],d=mesh_.positions[vertices[2]];
        const auto bary=closest_barycentric(p,a,c,d),distance=sub(p,add(add(mul(a,bary.x),mul(c,bary.y)),mul(d,bary.z)));const float square=dot(distance,distance);
        if(square<best) {best=square;result={vertices,bary};}}
    }return result;
  }
};
bool transferable(const Morph &m) {return m.evaluable&&m.auto_follow&&m.kind!="alias"&&m.alias_morph<0&&!m.offsets.empty();}
}
ConformRuntime::ConformRuntime(const ir::Scene &scene,const std::vector<Target> &targets,const std::vector<Skin> &skins,const std::vector<FormulaGraph> &graphs):targets_(targets) {
  if(graphs.size()!=targets.size()) throw std::runtime_error("Fit To 公式图数量不一致");
  link_for_target_.resize(targets.size(),-1);offsets_.resize(targets.size());revisions_.resize(targets.size());
  std::map<std::string,std::vector<size_t>> identifiers;
  for(size_t t=0;t<targets.size();++t) {identifiers[targets[t].id.substr(0,targets[t].id.rfind('/'))].push_back(t);vertex_counts_.push_back(scene.meshes.at(scene.instances.at(targets[t].instance).mesh).positions.size());}
  for(size_t t=0;t<targets.size();++t) {const auto &uri=targets[t].conform_target;if(uri.empty()) continue;
    const auto found=identifiers.find(uri.starts_with('#')?uri.substr(1):uri);
    if(found==identifiers.end()||found->second.size()!=1) throw std::runtime_error("无法唯一解析 Fit To 目标："+targets[t].id+" -> "+uri);
    ConformLink link;link.follower=t;link.source=found->second.front();link.follower_skin=graphs[t].skin;link.source_skin=graphs[link.source].skin;
    link_for_target_[t]=int(links_.size());links_.push_back(std::move(link));}
  std::vector<int> state(targets.size());std::function<void(size_t)> visit=[&](size_t t) {if(state[t]==2) return;if(state[t]==1) throw std::runtime_error("Fit To 关系存在循环");state[t]=1;if(const auto *l=link(t)) visit(l->source);state[t]=2;order_.push_back(t);};
  for(size_t t=0;t<targets.size();++t) visit(t);
  for(auto &l:links_) {
    const auto &source=targets[l.source];const auto &follower=targets[l.follower];const auto &g=graphs[l.follower];
    std::map<std::string,std::vector<size_t>> names;for(size_t m=0;m<source.morphs.size();++m) if(transferable(source.morphs[m])) names[source.morphs[m].channel_id].push_back(m);
    l.morph_sources.resize(follower.morphs.size(),-1);std::set<size_t> authored;
    for(size_t m=0;m<follower.morphs.size();++m) {const auto &morph=follower.morphs[m];if(!transferable(morph)||morph.channel_id.empty()) continue;
      const auto found=names.find(morph.channel_id);if(found==names.end()||found->second.size()!=1) continue;const auto source_morph=found->second.front();
      // 自带 ERC 的修正由继承的骨骼驱动，不再叠加同一人体修正值。
      const int c=g.morph_channels.at(m);bool driven=false;if(c>=0) for(auto e:g.incoming.at(size_t(c))) driven|=g.expressions[e].enabled;
      if(!driven) l.morph_sources[m]=int(source_morph);authored.insert(source_morph);++stats_.authored_morphs;
    }
    for(size_t m=0;m<source.morphs.size();++m) if(transferable(source.morphs[m])&&!authored.contains(m)) l.projected_morphs.push_back(m);
    if(l.follower_skin>=0&&l.source_skin>=0) {
      const auto &a=skins.at(size_t(l.source_skin)),&b=skins.at(size_t(l.follower_skin));l.joints.resize(b.joints.size(),-1);
      for(size_t j=0;j<b.joints.size();++j) {if(j==0&&!a.joints.empty()) {l.joints[j]=0;continue;}const auto &joint=b.joints[j];
        // 跨资产的 name 比局部 ID 更稳定；G8.1 的部分 Carpal ID 已与名称错位。
        std::vector<size_t> names;for(size_t k=1;k<a.joints.size();++k) if(!joint.name.empty()&&joint.name==a.joints[k].name) names.push_back(k);
        if(names.size()==1) l.joints[j]=int(names.front());
        else {
          for(size_t k=1;k<a.joints.size();++k) if(joint.id==a.joints[k].id&&(names.empty()||joint.name==a.joints[k].name)) {
            if(l.joints[j]>=0) throw std::runtime_error("Fit To 骨骼 ID 不唯一："+joint.id);l.joints[j]=int(k);
          }
          if(names.size()>1&&l.joints[j]<0) throw std::runtime_error("Fit To 骨骼名称不唯一："+joint.name);
        }
      }
    }
    const auto &a=scene.instances.at(source.instance),&b=scene.instances.at(follower.instance);const auto follower_to_source=inverse(a.transform)*b.transform;l.source_to_follower=inverse(b.transform)*a.transform;
    const auto &body=scene.meshes.at(a.mesh),&cloth=scene.meshes.at(b.mesh);SurfaceIndex index(body);l.surface.reserve(cloth.positions.size());
    l.neighbors.resize(cloth.positions.size());
    for(const auto &triangle:cloth.triangles) for(size_t i=0;i<3;++i) for(size_t j=0;j<3;++j) if(i!=j) l.neighbors[triangle.vertices[i]].push_back(triangle.vertices[j]);
    for(auto &neighbors:l.neighbors) {std::sort(neighbors.begin(),neighbors.end());neighbors.erase(std::unique(neighbors.begin(),neighbors.end()),neighbors.end());}
    for(auto p:cloth.positions) {
      const auto query=follower_to_source.point(p);auto binding=index.nearest(query);const auto a=body.positions[binding.vertices[0]],b=body.positions[binding.vertices[1]],c=body.positions[binding.vertices[2]];
      binding.edge1=sub(b,a);binding.edge2=sub(c,a);binding.normal=normal(binding.edge1,binding.edge2);
      const auto offset=sub(query,add(add(mul(a,binding.barycentric.x),mul(b,binding.barycentric.y)),mul(c,binding.barycentric.z)));
      const double aa=dot(binding.edge1,binding.edge1),bb=dot(binding.edge2,binding.edge2),ab=dot(binding.edge1,binding.edge2),u=dot(offset,binding.edge1),v=dot(offset,binding.edge2),det=aa*bb-ab*ab;
      if(det>1e-12*aa*bb) binding.offset_coordinates={float((bb*u-ab*v)/det),float((aa*v-ab*u)/det),dot(offset,binding.normal)};
      l.surface.push_back(binding);
    }
    stats_.bindings+=l.surface.size();
  }
  previous_weights_.resize(links_.size());source_revisions_.resize(links_.size(),std::numeric_limits<uint64_t>::max());
}
const ConformLink *ConformRuntime::link(size_t target) const {const int i=link_for_target_.at(target);return i<0?nullptr:&links_[size_t(i)];}
void ConformRuntime::project(const std::vector<std::vector<float>> &weights,MorphRuntime &morph) {
  for(auto target:order_) {const auto *l=link(target);if(!l) continue;const auto i=size_t(link_for_target_[target]);std::vector<float> current;current.reserve(l->projected_morphs.size());
    for(auto m:l->projected_morphs) current.push_back(weights.at(l->source).at(m));
    if(current==previous_weights_[i]&&source_revisions_[i]==revisions_[l->source]) continue;
    std::vector<ir::Vec3> source(vertex_counts_[l->source]);if(!offsets_[l->source].empty()) source=offsets_[l->source];
    for(size_t m=0;m<current.size();++m) if(current[m]!=0) for(const auto &d:targets_[l->source].morphs[l->projected_morphs[m]].offsets) source[d.vertex]=add(source[d.vertex],mul(d.delta,current[m]));
    auto &offsets=offsets_[target];offsets.resize(l->surface.size());
    for(size_t v=0;v<offsets.size();++v) {const auto &b=l->surface[v];const auto a=source[b.vertices[0]],c=source[b.vertices[1]],d=source[b.vertices[2]],edge1=sub(c,a),edge2=sub(d,a);
      auto delta=add(add(mul(a,b.barycentric.x),mul(c,b.barycentric.y)),mul(d,b.barycentric.z));
      // 随表面旋转衣物原有间隙，不能只平移最近点，否则凸起处会穿插。
      delta=add(delta,add(mul(edge1,b.offset_coordinates.x),mul(edge2,b.offset_coordinates.y)));
      delta=add(delta,mul(sub(normal(add(b.edge1,edge1),add(b.edge2,edge2)),b.normal),b.offset_coordinates.z));
      offsets[v]=direction(l->source_to_follower,delta);
    }
    // 最近三角形会在腿间、腋下及远离皮肤的裙摆切换。只平滑生成的位移场，
    // 不平滑衣物基础褶皱或作者的修正 Morph，也不把上次结果反馈进来。
    auto filtered=offsets;
    for(int iteration=0;iteration<24;++iteration) {
      for(size_t v=0;v<offsets.size();++v) {const auto &neighbors=l->neighbors[v];if(neighbors.size()<3) continue;
        ir::Vec3 average;for(auto n:neighbors) average=add(average,offsets[n]);filtered[v]=add(mul(offsets[v],.5f),mul(average,.5f/float(neighbors.size())));}
      offsets.swap(filtered);
    }
    if(morph.set_follow_offsets(target,offsets)) ++revisions_[target];
    previous_weights_[i]=std::move(current);source_revisions_[i]=revisions_[l->source];++stats_.evaluations;stats_.projected_vertices+=offsets.size();
  }
}
std::vector<JointPose> conform_pose(const ConformLink &link,const std::vector<Skin> &skins,const std::vector<std::vector<JointPose>> &resolved,const std::vector<JointPose> &input) {
  auto result=input;if(link.source_skin<0||link.follower_skin<0) return result;
  const auto &initial=skins.at(size_t(link.follower_skin)).initial;const auto &source=resolved.at(size_t(link.source_skin));
  auto scale=[](ir::Vec3 source,ir::Vec3 input,ir::Vec3 initial) {return ir::Vec3{source.x*input.x/initial.x,source.y*input.y/initial.y,source.z*input.z/initial.z};};
  for(size_t j=0;j<link.joints.size();++j) if(link.joints[j]>=0) {const auto &a=source.at(size_t(link.joints[j])),&b=input.at(j),&base=initial.at(j);auto &p=result.at(j);
    p.translation_cm=add(a.translation_cm,sub(b.translation_cm,base.translation_cm));p.rotation_degrees=add(a.rotation_degrees,sub(b.rotation_degrees,base.rotation_degrees));
    p.scale=scale(a.scale,b.scale,base.scale);p.general_scale=a.general_scale*b.general_scale/base.general_scale;
    p.center_offset_cm=add(a.center_offset_cm,sub(b.center_offset_cm,base.center_offset_cm));p.end_offset_cm=add(a.end_offset_cm,sub(b.end_offset_cm,base.end_offset_cm));p.orientation_offset_degrees=add(a.orientation_offset_degrees,sub(b.orientation_offset_degrees,base.orientation_offset_degrees));
  }return result;
}
}
