#include "runtime/conform.h"
#include "diagnostics/load_profile.h"
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
    // 中位数二分树的深度受 size_t 位数约束；每次查询不再分配动态栈。
    float best=std::numeric_limits<float>::max();SurfaceBinding result;std::array<int,8*sizeof(size_t)+1> stack{};size_t pending=1;
    while(pending) {const auto &b=branches_[stack[--pending]];if(box_distance(b.bounds,p)>best) continue;
      if(b.left>=0) {const bool left_first=box_distance(branches_[b.left].bounds,p)<box_distance(branches_[b.right].bounds,p);stack[pending++]=left_first?b.right:b.left;stack[pending++]=left_first?b.left:b.right;continue;}
      for(size_t i=b.begin;i<b.end;++i) {const auto vertices=mesh_.triangles[triangles_[i]].vertices;const auto a=mesh_.positions[vertices[0]],c=mesh_.positions[vertices[1]],d=mesh_.positions[vertices[2]];
        const auto bary=closest_barycentric(p,a,c,d),distance=sub(p,add(add(mul(a,bary.x),mul(c,bary.y)),mul(d,bary.z)));const float square=dot(distance,distance);
        if(square<best) {best=square;result={vertices,bary};result.polygon=mesh_.triangles[triangles_[i]].source_polygon;}}
    }return result;
  }
};
bool transferable(const Morph &m) {return m.evaluable&&m.auto_follow&&m.kind!="alias"&&m.alias_morph<0&&m.has_offsets();}
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
  std::map<uint32_t,std::unique_ptr<SurfaceIndex>> surface_indexes;
  for(auto &l:links_) {
    diagnostics::Scope binding_scope(diagnostics::active?"conform_binding/"+targets[l.follower].id:std::string{});
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
    const auto &body=scene.meshes.at(a.mesh),&cloth=scene.meshes.at(b.mesh);
    auto &cached_index=surface_indexes[a.mesh];if(!cached_index) cached_index=std::make_unique<SurfaceIndex>(body);const auto &index=*cached_index;l.surface.reserve(cloth.positions.size());
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
  diagnostics::Scope scope("conform_project");
  for(auto target:order_) {const auto *l=link(target);if(!l) continue;const auto i=size_t(link_for_target_[target]);std::vector<float> current;current.reserve(l->projected_morphs.size());
    for(auto m:l->projected_morphs) current.push_back(weights.at(l->source).at(m));
    if(current==previous_weights_[i]&&source_revisions_[i]==revisions_[l->source]) continue;
    std::vector<ir::Vec3> source(vertex_counts_[l->source]);if(!offsets_[l->source].empty()) source=offsets_[l->source];
    for(size_t m=0;m<current.size();++m) if(current[m]!=0) for(const auto &d:targets_[l->source].morphs[l->projected_morphs[m]].data()) source[d.vertex]=add(source[d.vertex],mul(d.delta,current[m]));
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
CollisionRuntime::CollisionRuntime(ir::Scene &scene,const std::vector<Target> &targets):scene_(scene) {
  std::map<std::string,std::vector<uint32_t>> ids;
  for(const auto &t:targets) ids["#"+t.id.substr(0,t.id.rfind('/'))].push_back(t.instance);
  std::map<uint32_t,Binding> pending;
  for(const auto &t:targets) {
    const auto &s=t.smoothing;if(!s.enabled||s.collision_iterations<=0||s.collision_target.empty()) continue;
    const auto found=ids.find(s.collision_target);
    if(found==ids.end()||found->second.size()!=1) throw std::runtime_error("无法唯一解析碰撞对象："+s.collision_target);
    Binding b;b.follower=t.instance;b.source=found->second.front();b.settings=s;
    const auto &mesh=scene.meshes.at(scene.instances.at(t.instance).mesh);
    if(mesh.triangles.empty()) continue;
    if(scene.meshes.at(scene.instances.at(b.source).mesh).triangles.empty()) throw std::runtime_error("碰撞对象没有表面："+s.collision_target);
    if(!mesh.graft_target_vertices) for(const auto &g:targets) if(g.instance!=t.instance&&g.conform_target==s.collision_target) {
      const auto &surface=scene.meshes.at(scene.instances.at(g.instance).mesh);
      if(surface.graft_target_vertices==scene.meshes.at(scene.instances.at(b.source).mesh).positions.size()) b.grafts.push_back(g.instance);
    }
    b.input=mesh.positions;b.neighbors.resize(mesh.positions.size());
    for(const auto &f:mesh.triangles) for(int a=0;a<3;++a) for(int c=0;c<3;++c) if(a!=c) b.neighbors[f.vertices[a]].push_back(f.vertices[c]);
    for(auto &n:b.neighbors) {std::sort(n.begin(),n.end());n.erase(std::unique(n.begin(),n.end()),n.end());}
    pending.emplace(b.follower,std::move(b));
  }
  std::map<uint32_t,int> state;
  std::function<void(uint32_t)> visit=[&](uint32_t i) {
    if(!pending.contains(i)||state[i]==2) return;
    if(state[i]==1) throw std::runtime_error("碰撞对象关系存在循环");state[i]=1;
    visit(pending.at(i).source);for(auto g:pending.at(i).grafts) visit(g);
    state[i]=2;bindings_.push_back(std::move(pending.at(i)));
  };
  for(const auto &[i,b]:pending) visit(i);
}
ir::Delta CollisionRuntime::evaluate(ir::Delta delta) {
  diagnostics::Scope scope("collision_evaluate");
  std::set<uint32_t> changed;
  for(const auto &e:delta.meshes) changed.insert(e.index);
  // 先截取所有未修正输入，后续碰撞链的输出不能覆盖下游的蒙皮缓存。
  for(auto &b:bindings_) for(const auto &e:delta.meshes) if(e.index==scene_.instances[b.follower].mesh) b.input=e.positions;
  for(auto &b:bindings_) {
    const auto &follower=scene_.instances[b.follower],&source=scene_.instances[b.source];
    const auto relative=ir::inverse(source.transform)*follower.transform;
    bool graft_changed=false;
    for(auto g:b.grafts) {
      graft_changed|=changed.contains(scene_.instances[g].mesh);
      for(const auto &e:delta.instances) graft_changed|=e.index==g||e.index==b.source;
      for(const auto &e:delta.visibility) graft_changed|=e.index==g;
    }
    if(b.initialized&&!graft_changed&&!changed.contains(follower.mesh)&&!changed.contains(source.mesh)&&relative.value==b.relative.value) continue;
    b.initialized=true;b.relative=relative;
    // 基础人体与可见附加网格分别施加外侧约束，避免最近面选到内层后漏碰撞。
    std::vector<ir::Mesh> surfaces{scene_.meshes[source.mesh]};
    for(auto g:b.grafts) if(scene_.instances[g].visible) {
      const auto &instance=scene_.instances[g];auto part=scene_.meshes[instance.mesh];
      const auto matrix=ir::inverse(source.transform)*instance.transform;
      for(auto &p:part.positions) p=matrix.point(p);surfaces.push_back(std::move(part));
    }
    std::vector<std::unique_ptr<SurfaceIndex>> indexes;
    std::vector<std::vector<ir::Vec3>> normals;
    for(const auto &body:surfaces) {
      indexes.push_back(std::make_unique<SurfaceIndex>(body));normals.emplace_back(body.positions.size());auto &n=normals.back();
      for(const auto &f:body.triangles) {
        const auto a=body.positions[f.vertices[0]],u=sub(body.positions[f.vertices[1]],a),v=sub(body.positions[f.vertices[2]],a);
        const ir::Vec3 face{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        for(auto k:f.vertices) n[k]=add(n[k],face);
      }
      for(auto &v:n) {const float length=std::sqrt(dot(v,v));if(length>0) v=mul(v,1/length);}
    }
    auto positions=b.input;for(auto &p:positions) p=relative.point(p);const auto initial=positions;
    // 半毫米安全间隙用于基础三角面近似，不改变资源本身或人体顶点。
    constexpr float clearance=.0005f;
    auto correction=[&](ir::Vec3 p) {
      const auto input=p;const auto anchor=indexes[0]->nearest(p);
      for(size_t c=0;c<surfaces.size();++c) {
        const auto &body=surfaces[c];
        if(c&&std::find(body.graft_hidden_polygons.begin(),body.graft_hidden_polygons.end(),anchor.polygon)==body.graft_hidden_polygons.end()) continue;
        const auto hit=c?indexes[c]->nearest(p):anchor;const auto v=hit.vertices;const auto w=hit.barycentric;
        const auto q=add(add(mul(body.positions[v[0]],w.x),mul(body.positions[v[1]],w.y)),mul(body.positions[v[2]],w.z));
        auto n=add(add(mul(normals[c][v[0]],w.x),mul(normals[c][v[1]],w.y)),mul(normals[c][v[2]],w.z));
        const auto face=normal(sub(body.positions[v[1]],body.positions[v[0]]),sub(body.positions[v[2]],body.positions[v[0]]));
        const auto d=sub(p,q);const float length=std::sqrt(dot(d,d)),nl=std::sqrt(dot(n,n));if(nl<1e-8f) n=face;else n=mul(n,1/nl);
        if(dot(d,n)<0||length<clearance) p=add(q,mul(n,clearance));
      }
      return sub(p,input);
    };
    const auto &mesh=scene_.meshes[follower.mesh];
    for(int iteration=0;iteration<b.settings.collision_iterations;++iteration) {
      if(iteration>0) for(int smooth=0;smooth<b.settings.smoothing_iterations;++smooth) {
        auto filtered=positions;
        for(size_t v=0;v<positions.size();++v) if(!b.neighbors[v].empty()) {
          ir::Vec3 average;for(auto n:b.neighbors[v]) average=add(average,sub(positions[n],initial[n]));
          const auto d=sub(positions[v],initial[v]);filtered[v]=add(initial[v],add(mul(d,1-b.settings.weight),mul(average,b.settings.weight/float(b.neighbors[v].size()))));
        }
        positions.swap(filtered);
      }
      // 同时处理顶点与较粗三角形内部，避免顶点已在外面、面仍穿过胸部。
      for(auto &p:positions) p=add(p,correction(p));
      for(const auto &f:mesh.triangles) {
        const auto v=f.vertices;float longest=0;
        for(int k=0;k<3;++k) {const auto e=sub(positions[v[k]],positions[v[(k+1)%3]]);longest=std::max(longest,dot(e,e));}
        if(longest<.0001f) continue;
        for(const ir::Vec3 w:std::array<ir::Vec3,4>{{{1.f/3,1.f/3,1.f/3},{.5f,.5f,0},{.5f,0,.5f},{0,.5f,.5f}}}) {
          const auto p=add(add(mul(positions[v[0]],w.x),mul(positions[v[1]],w.y)),mul(positions[v[2]],w.z));
          const auto d=correction(p);const float sum=w.x*w.x+w.y*w.y+w.z*w.z;
          positions[v[0]]=add(positions[v[0]],mul(d,w.x/sum));positions[v[1]]=add(positions[v[1]],mul(d,w.y/sum));positions[v[2]]=add(positions[v[2]],mul(d,w.z/sum));
        }
      }
      // 从身体顶点反查服装面，捕获粗服装三角形采样点之间的小凸起。
      auto cloth=mesh;cloth.positions=positions;SurfaceIndex cloth_index(cloth);
      for(size_t c=0;c<surfaces.size();++c) for(size_t k=0;k<surfaces[c].positions.size();++k) {
        const auto p=surfaces[c].positions[k];const auto hit=cloth_index.nearest(p);const auto v=hit.vertices;const auto w=hit.barycentric;
        if(std::min({w.x,w.y,w.z})<.01f) continue; // 不把裸露区域牵到领口或袖口边缘。
        const auto q=add(add(mul(positions[v[0]],w.x),mul(positions[v[1]],w.y)),mul(positions[v[2]],w.z));
        const auto n=normal(sub(positions[v[1]],positions[v[0]]),sub(positions[v[2]],positions[v[0]]));
        const float depth=dot(sub(p,q),n);
        if(depth<=0||depth>.02f||dot(n,normals[c][k])<.5f) continue;
        const auto d=mul(n,depth+clearance);const float sum=w.x*w.x+w.y*w.y+w.z*w.z;
        positions[v[0]]=add(positions[v[0]],mul(d,w.x/sum));positions[v[1]]=add(positions[v[1]],mul(d,w.y/sum));positions[v[2]]=add(positions[v[2]],mul(d,w.z/sum));
      }
    }
    for(auto &p:positions) p=add(p,correction(p));
    const auto inverse=ir::inverse(relative);uint64_t corrected=0;
    for(size_t v=0;v<positions.size();++v) {
      const auto d=sub(positions[v],initial[v]);
      if(dot(d,d)>1e-16f) {positions[v]=inverse.point(positions[v]);++corrected;} else positions[v]=b.input[v];
    }
    auto &current=scene_.meshes[follower.mesh].positions;
    const bool different=!std::equal(positions.begin(),positions.end(),current.begin(),[](auto a,auto b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;});
    if(different) {
      current=positions;auto e=std::find_if(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e) {return e.index==follower.mesh;});
      if(e==delta.meshes.end()) delta.meshes.push_back({follower.mesh,std::move(positions)});else e->positions=std::move(positions);
      changed.insert(follower.mesh);
    }
    ++stats_.evaluations;stats_.corrected_vertices+=corrected;
  }
  return delta;
}

}
