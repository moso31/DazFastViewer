#include "runtime/conform.h"
#include "runtime/geometry_key.h"
#include "runtime/rigid_follow.h"
#include "diagnostics/load_profile.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <ppl.h>

namespace dfv::runtime {
namespace {
ir::Vec3 add(ir::Vec3 a,ir::Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
ir::Vec3 mul(ir::Vec3 a,float v) {return {a.x*v,a.y*v,a.z*v};}
float dot(ir::Vec3 a,ir::Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
double edge_measure(const ir::Mesh &mesh){double sum=0;for(const auto &t:mesh.triangles)for(int k=0;k<3;++k){const auto e=sub(mesh.positions[t.vertices[k]],mesh.positions[t.vertices[(k+1)%3]]);sum+=double(e.x)*e.x+double(e.y)*e.y+double(e.z)*e.z;}return sum;}
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
  SurfaceBinding nearest(ir::Vec3 p,const std::vector<ir::Vec3> *reference=nullptr,ir::Vec3 reference_point={},float reference_radius=0) const {
    if(branches_.empty()) throw std::runtime_error("Fit To 目标没有可用于形变转移的三角形");
    // 中位数二分树的深度受 size_t 位数约束；每次查询不再分配动态栈。
    float best=std::numeric_limits<float>::max();SurfaceBinding result;std::array<int,8*sizeof(size_t)+1> stack{};size_t pending=1;
    while(pending) {const auto &b=branches_[stack[--pending]];if(box_distance(b.bounds,p)>best) continue;
      if(b.left>=0) {const bool left_first=box_distance(branches_[b.left].bounds,p)<box_distance(branches_[b.right].bounds,p);stack[pending++]=left_first?b.right:b.left;stack[pending++]=left_first?b.left:b.right;continue;}
      for(size_t i=b.begin;i<b.end;++i) {const auto vertices=mesh_.triangles[triangles_[i]].vertices;const auto a=mesh_.positions[vertices[0]],c=mesh_.positions[vertices[1]],d=mesh_.positions[vertices[2]];
        const auto bary=closest_barycentric(p,a,c,d),distance=sub(p,add(add(mul(a,bary.x),mul(c,bary.y)),mul(d,bary.z)));const float square=dot(distance,distance);
        if(square<best) {
          if(reference){const auto x=reference->at(vertices[0]),y=reference->at(vertices[1]),z=reference->at(vertices[2]);const auto w=closest_barycentric(reference_point,x,y,z);const auto gap=sub(reference_point,add(add(mul(x,w.x),mul(y,w.y)),mul(z,w.z)));if(dot(gap,gap)>reference_radius*reference_radius)continue;}
          best=square;result={vertices,bary};result.polygon=mesh_.triangles[triangles_[i]].source_polygon;result.triangle=triangles_[i];}}
    }
    if(best==std::numeric_limits<float>::max())throw std::runtime_error("未找到有效的表面绑定三角形");
    return result;
  }
};
bool transferable(const Morph &m) {return m.evaluable&&m.auto_follow&&m.kind!="alias"&&m.alias_morph<0&&m.has_offsets();}
// 导入校验和渲染准备使用相同静止几何，复用最近点绑定，避免再次逐顶点搜索。
// 缓存只保存几何结果；骨骼、Morph 和 Fit To 依赖仍按当前文档重新校验。
struct CachedSurface {
  uint64_t key=0;size_t bytes=0;
  std::vector<SurfaceBinding> surface;
  std::vector<std::vector<uint32_t>> neighbors;
};
class SurfaceCache {
  std::mutex mutex_;
  std::list<std::shared_ptr<const CachedSurface>> entries_;
  size_t bytes_=0;
  static constexpr size_t budget=128*1024*1024;
public:
  std::shared_ptr<const CachedSurface> get(uint64_t key) {
    std::lock_guard lock(mutex_);
    for(auto i=entries_.begin();i!=entries_.end();++i)if((*i)->key==key){auto result=*i;entries_.splice(entries_.end(),entries_,i);return result;}
    return {};
  }
  void put(const ConformLink &link) {
    size_t cost=sizeof(CachedSurface)+link.surface.size()*sizeof(SurfaceBinding)+link.neighbors.size()*sizeof(std::vector<uint32_t>);
    for(const auto &neighbors:link.neighbors)cost+=neighbors.size()*sizeof(uint32_t);if(cost>budget)return;
    auto saved=std::make_shared<CachedSurface>();saved->key=link.geometry_key;saved->bytes=cost;saved->surface=link.surface;saved->neighbors=link.neighbors;
    std::lock_guard lock(mutex_);for(const auto &entry:entries_)if(entry->key==saved->key)return;
    while(!entries_.empty()&&(bytes_+cost>budget||entries_.size()>=32)){bytes_-=entries_.front()->bytes;entries_.pop_front();}
    bytes_+=cost;entries_.push_back(std::move(saved));
  }
};
SurfaceCache surface_cache;
}
ConformRuntime::ConformRuntime(const ir::Scene &scene,const std::vector<Target> &targets,const std::vector<Skin> &skins,const std::vector<FormulaGraph> &graphs,const ConformRuntime *reuse):targets_(targets) {
  if(graphs.size()!=targets.size()) throw std::runtime_error("Fit To 公式图数量不一致");
  link_for_target_.resize(targets.size(),-1);offsets_.resize(targets.size());revisions_.resize(targets.size());
  bone_rigidity_.resize(targets.size());
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
    if(l.follower_skin>=0)for(const auto &group:cloth.bone_rigidity){BoneRigidity binding;binding.group=group;binding.skin=l.follower_skin;
      const auto &joints=skins[size_t(l.follower_skin)].joints;
      for(const auto &node:group.transform_nodes){size_t found=joints.size();for(size_t j=0;j<joints.size();++j)if(joints[j].id==node){found=j;break;}if(found==joints.size())throw std::runtime_error("刚性区域骨骼不存在："+node);if(found>=l.joints.size()||l.joints[found]<0)binding.joints.push_back(found);}
      bone_rigidity_[l.follower].push_back(std::move(binding));
    }
    GeometryKey key;key.points(body.positions);key.topology(body);key.points(cloth.positions);key.topology(cloth);key.add(follower_to_source);l.geometry_key=key.value;
    const ConformLink *cached=nullptr;if(reuse) for(const auto &old:reuse->links_) if(old.geometry_key==l.geometry_key&&reuse->targets_[old.follower].id==follower.id&&reuse->targets_[old.source].id==source.id) {cached=&old;break;}
    if(cached) {l.surface=cached->surface;l.neighbors=cached->neighbors;stats_.bindings+=l.surface.size();continue;}
    if(const auto saved=surface_cache.get(l.geometry_key)){diagnostics::Scope cache_hit("conform_binding_cache_hit");l.surface=saved->surface;l.neighbors=saved->neighbors;stats_.bindings+=l.surface.size();continue;}
    auto &cached_index=surface_indexes[a.mesh];if(!cached_index) cached_index=std::make_unique<SurfaceIndex>(body);const auto &index=*cached_index;l.surface.reserve(cloth.positions.size());
    l.neighbors.resize(cloth.positions.size());
    for(const auto &triangle:cloth.triangles) for(size_t i=0;i<3;++i) for(size_t j=0;j<3;++j) if(i!=j) l.neighbors[triangle.vertices[i]].push_back(triangle.vertices[j]);
    for(auto &neighbors:l.neighbors) {std::sort(neighbors.begin(),neighbors.end());neighbors.erase(std::unique(neighbors.begin(),neighbors.end()),neighbors.end());}
    l.surface.resize(cloth.positions.size());
    auto bind_vertex=[&](size_t vertex) {const auto p=cloth.positions[vertex];
      const auto query=follower_to_source.point(p);auto binding=index.nearest(query);const auto a=body.positions[binding.vertices[0]],b=body.positions[binding.vertices[1]],c=body.positions[binding.vertices[2]];
      binding.edge1=sub(b,a);binding.edge2=sub(c,a);binding.normal=normal(binding.edge1,binding.edge2);
      const auto offset=sub(query,add(add(mul(a,binding.barycentric.x),mul(b,binding.barycentric.y)),mul(c,binding.barycentric.z)));
      binding.distance=std::sqrt(dot(offset,offset));
      const double aa=dot(binding.edge1,binding.edge1),bb=dot(binding.edge2,binding.edge2),ab=dot(binding.edge1,binding.edge2),u=dot(offset,binding.edge1),v=dot(offset,binding.edge2),det=aa*bb-ab*ab;
      if(det>1e-12*aa*bb) binding.offset_coordinates={float((bb*u-ab*v)/det),float((aa*v-ab*u)/det),dot(offset,binding.normal)};
      l.surface[vertex]=binding;
    };
    if(cloth.positions.size()>=4096)Concurrency::parallel_for(size_t(0),(cloth.positions.size()+255)/256,[&](size_t block){for(size_t i=block*256;i<std::min(cloth.positions.size(),(block+1)*256);++i)bind_vertex(i);});
    else for(size_t i=0;i<cloth.positions.size();++i)bind_vertex(i);
    surface_cache.put(l);stats_.bindings+=l.surface.size();
  }
  previous_weights_.resize(links_.size());source_revisions_.resize(links_.size(),std::numeric_limits<uint64_t>::max());
  follow_revisions_.resize(links_.size(),std::numeric_limits<uint64_t>::max());
  for(const auto &link:links_)if(!surface_frames_.contains(link.source)) {
    const auto &mesh=scene.meshes[scene.instances[targets[link.source].instance].mesh];auto &frames=surface_frames_[link.source];frames.reference=mesh.positions;
    std::vector<std::vector<uint32_t>> neighbors(mesh.positions.size());
    // 源网格的独立拼片可能有作者指定的不同 Morph，保留其拓扑边界。
    for(const auto &f:mesh.triangles)for(auto v:f.vertices)for(auto n:f.vertices)neighbors[v].push_back(n);
    for(auto &n:neighbors){std::sort(n.begin(),n.end());n.erase(std::unique(n.begin(),n.end()),n.end());}
    frames.neighborhoods.resize(neighbors.size());
    for(uint32_t v=0;v<neighbors.size();++v){auto &n=frames.neighborhoods[v];for(auto a:neighbors[v])n.insert(n.end(),neighbors[a].begin(),neighbors[a].end());std::sort(n.begin(),n.end());n.erase(std::unique(n.begin(),n.end()),n.end());}
  }
}
const ConformLink *ConformRuntime::link(size_t target) const {const int i=link_for_target_.at(target);return i<0?nullptr:&links_[size_t(i)];}
void ConformRuntime::project(const std::vector<std::vector<float>> &weights,MorphRuntime &morph) {
  diagnostics::Scope scope("conform_project");
  for(auto target:order_) {const auto *l=link(target);if(!l) continue;const auto i=size_t(link_for_target_[target]);std::vector<float> current;current.reserve(l->projected_morphs.size());
    for(auto m:l->projected_morphs) current.push_back(weights.at(l->source).at(m));
    if(current==previous_weights_[i]&&source_revisions_[i]==revisions_[l->source]&&follow_revisions_[i]==morph.follow_revision(target)) continue;
    std::vector<ir::Vec3> source(vertex_counts_[l->source]);if(!offsets_[l->source].empty()) source=offsets_[l->source];
    for(size_t m=0;m<current.size();++m) if(current[m]!=0) for(const auto &d:targets_[l->source].morphs[l->projected_morphs[m]].data()) source[d.vertex]=add(source[d.vertex],mul(d.delta,current[m]));
    const auto &frames=surface_frames_.at(l->source);std::vector<ir::Transform> rotations(source.size());std::vector<uint8_t> needed(source.size());
    for(const auto &b:l->surface)for(auto v:b.vertices)needed[v]=1;
    auto rotate_vertex=[&](size_t v){
      if(!needed[v])return;const auto &neighbors=frames.neighborhoods[v];if(neighbors.empty())return;
      const auto first=source[neighbors.front()];if(std::all_of(neighbors.begin(),neighbors.end(),[&](auto n){return source[n]==first;}))return;
      std::vector<ir::Vec3> a,b;a.reserve(neighbors.size());b.reserve(neighbors.size());
      for(auto n:neighbors){a.push_back(frames.reference[n]);b.push_back(add(frames.reference[n],source[n]));}rotations[v]=fit_rigid(a,b);
    };
    if(source.size()>=4096)Concurrency::parallel_for(size_t(0),(source.size()+255)/256,[&](size_t block){for(size_t v=block*256;v<std::min(source.size(),(block+1)*256);++v)rotate_vertex(v);});
    else for(size_t v=0;v<source.size();++v)rotate_vertex(v);
    auto &offsets=offsets_[target];offsets.resize(l->surface.size());
    for(size_t v=0;v<offsets.size();++v) {const auto &b=l->surface[v];const auto a=source[b.vertices[0]],c=source[b.vertices[1]],d=source[b.vertices[2]];
      auto delta=add(add(mul(a,b.barycentric.x),mul(c,b.barycentric.y)),mul(d,b.barycentric.z));
      // 用共享顶点邻域的旋转连续插值衣物间隙。单三角面的梯度外推会在
      // 脚趾的细小面、最近面切换及接缝处放大位移，造成鞋面锯齿和错位。
      const auto gap=add(add(mul(b.edge1,b.offset_coordinates.x),mul(b.edge2,b.offset_coordinates.y)),mul(b.normal,b.offset_coordinates.z));
      const auto rotation_delta=add(add(mul(sub(direction(rotations[b.vertices[0]],gap),gap),b.barycentric.x),mul(sub(direction(rotations[b.vertices[1]],gap),gap),b.barycentric.y)),mul(sub(direction(rotations[b.vertices[2]],gap),gap),b.barycentric.z));
      delta=add(delta,rotation_delta);
      offsets[v]=direction(l->source_to_follower,delta);
    }
    // 最近三角形会在腿间、腋下及远离皮肤的裙摆切换。只平滑生成的位移场，
    // 不平滑衣物基础褶皱或作者的修正 Morph，也不把上次结果反馈进来。
    auto filtered=offsets;
    for(int iteration=0;iteration<24;++iteration) {
      for(size_t v=0;v<offsets.size();++v) {const auto &neighbors=l->neighbors[v];if(neighbors.size()<3) continue;
        // 贴体区域保留表面形变；远离身体的裙摆才逐渐增强平滑，避免压平鞋面。
        const float weight=std::clamp((l->surface[v].distance-.03f)/.06f,0.f,.5f);
        ir::Vec3 average;for(auto n:neighbors) average=add(average,offsets[n]);filtered[v]=add(mul(offsets[v],1-weight),mul(average,weight/float(neighbors.size())));}
      offsets.swap(filtered);
    }
    // 只约束自动生成的跟随位移；附件作者提供的 Morph 仍由 MorphRuntime 独立叠加。
    if(!bone_rigidity_[target].empty()) {const auto projected=offsets;
    for(auto &binding:bone_rigidity_[target]){const auto &group=binding.group;double x=0,y=0,z=0;
      for(auto v:group.reference_vertices){const auto p=projected.at(v);x+=p.x;y+=p.y;z+=p.z;}const double count=double(group.reference_vertices.size());binding.offset={float(x/count),float(y/count),float(z/count)};
      for(auto v:group.mask_vertices){const float w=group.weights.at(v);offsets.at(v)=add(mul(offsets[v],1-w),mul(binding.offset,w));}
    }}
    morph.constrain_follow_offsets(target,offsets);
    if(morph.set_follow_offsets(target,offsets)) ++revisions_[target];
    follow_revisions_[i]=morph.follow_revision(target);
    previous_weights_[i]=std::move(current);source_revisions_[i]=revisions_[l->source];++stats_.evaluations;stats_.projected_vertices+=offsets.size();
  }
}
void ConformRuntime::rigidity_pose(std::vector<std::vector<JointPose>> &poses) const {
  for(const auto &groups:bone_rigidity_)for(const auto &binding:groups){const auto p=binding.offset;const ir::Vec3 cm{p.x*100,p.z*100,-p.y*100};
    for(auto j:binding.joints){auto &pose=poses.at(size_t(binding.skin)).at(j);pose.center_offset_cm=add(pose.center_offset_cm,cm);pose.end_offset_cm=add(pose.end_offset_cm,cm);}
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
CollisionRuntime::CollisionRuntime(ir::Scene &scene,const std::vector<Target> &targets,std::function<ir::Transform(uint32_t,uint32_t)> relative,
    std::function<void(uint32_t,const std::vector<ir::Vec3> &,std::vector<ir::Vec3> &)> constrain,std::function<uint64_t(uint32_t)> revision):scene_(scene),relative_(std::move(relative)),constrain_(std::move(constrain)),constraint_revision_(std::move(revision)) {
  if(!relative_) relative_=[this](uint32_t source,uint32_t follower){return ir::inverse(scene_.instances.at(source).transform)*scene_.instances.at(follower).transform;};
  std::map<std::string,std::vector<uint32_t>> ids;
  for(const auto &t:targets) ids["#"+t.id.substr(0,t.id.rfind('/'))].push_back(t.instance);
  std::map<uint32_t,Binding> pending;
  for(const auto &t:targets) {
    const auto &s=t.smoothing;if(!s.enabled||s.collision_iterations<=0||s.collision_target.empty()) continue;
    const auto found=ids.find(s.collision_target);
    if(found==ids.end()||found->second.size()!=1) throw std::runtime_error("无法唯一解析碰撞对象："+s.collision_target);
    Binding b;b.follower=t.instance;b.source=found->second.front();b.settings=s;
    b.reference_edges=edge_measure(scene.meshes.at(scene.instances.at(b.source).mesh));
    const auto &mesh=scene.meshes.at(scene.instances.at(t.instance).mesh);
    if(mesh.triangles.empty()) continue;
    if(scene.meshes.at(scene.instances.at(b.source).mesh).triangles.empty()) throw std::runtime_error("碰撞对象没有表面："+s.collision_target);
    if(t.conform_target==s.collision_target){b.reference_source=scene.meshes.at(scene.instances.at(b.source).mesh).positions;b.reference_follower=mesh.positions;b.reference_relative=relative_(b.source,b.follower);}
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
void CollisionRuntime::reuse(const CollisionRuntime &previous) {
  for(auto &b:bindings_) for(const auto &old:previous.bindings_) if(scene_.instances[b.follower].id==previous.scene_.instances[old.follower].id) {
    b.cache_key=old.cache_key;b.output=old.output;
    // 同一场景只增减其他服装的碰撞时，保留本件衣物的未碰撞输入。
    // 否则会把已修正的显示网格再次当作输入，无法在身体移开后恢复原形。
    if(&scene_==&previous.scene_&&b.source==old.source&&b.follower==old.follower&&b.grafts==old.grafts&&b.settings==old.settings&&b.reference_edges==old.reference_edges&&b.input.size()==old.input.size()) {
      b.input=old.input;b.initialized=old.initialized;b.relative=old.relative;b.graft_relatives=old.graft_relatives;b.constraint_revision=old.constraint_revision;
    }
    break;
  }
}
void CollisionRuntime::reference_positions(uint32_t source,const std::vector<ir::Vec3> &positions) {
  for(auto &b:bindings_)if(!b.reference_source.empty()){
    if(b.source==source){b.reference_source=positions;b.reference_radii.clear();b.initialized=false;b.cache_key=0;}
    if(b.follower==source){b.reference_follower=positions;b.reference_radii.clear();b.initialized=false;b.cache_key=0;}
  }
  for(auto &b:bindings_)if(b.source==source) {
    const auto &mesh=scene_.meshes[scene_.instances[source].mesh];double sum=0;
    for(const auto &f:mesh.triangles)for(int k=0;k<3;++k){const auto e=sub(positions.at(f.vertices[k]),positions.at(f.vertices[(k+1)%3]));sum+=double(e.x)*e.x+double(e.y)*e.y+double(e.z)*e.z;}
    b.reference_edges=sum;b.initialized=false;b.cache_key=0;
  }
}
ir::Delta CollisionRuntime::evaluate(ir::Delta delta,bool parallel) {
  diagnostics::Scope scope("collision_evaluate");
  auto batch=[&](size_t count,const auto &fn) {if(parallel&&count>=512) Concurrency::parallel_for(size_t(0),(count+255)/256,[&](size_t block){for(size_t i=block*256;i<std::min(count,(block+1)*256);++i) fn(i);});else for(size_t i=0;i<count;++i) fn(i);};
  std::set<uint32_t> changed;
  for(const auto &e:delta.meshes) changed.insert(e.index);
  // 先截取所有未修正输入，后续碰撞链的输出不能覆盖下游的蒙皮缓存。
  for(auto &b:bindings_) for(const auto &e:delta.meshes) if(e.index==scene_.instances[b.follower].mesh) b.input=e.positions;
  for(auto &b:bindings_) {
    const auto &follower=scene_.instances[b.follower],&source=scene_.instances[b.source];
    diagnostics::Scope binding_scope(diagnostics::active?follower.id:std::string{});
    const auto relative=relative_(b.source,b.follower);
    bool graft_changed=false;
    std::vector<ir::Transform> graft_relatives;graft_relatives.reserve(b.grafts.size());
    for(size_t k=0;k<b.grafts.size();++k) {const auto g=b.grafts[k];graft_relatives.push_back(relative_(b.source,g));
      graft_changed|=changed.contains(scene_.instances[g].mesh);
      graft_changed|=k>=b.graft_relatives.size()||graft_relatives.back().value!=b.graft_relatives[k].value;
      for(const auto &e:delta.visibility) graft_changed|=e.index==g;
    }
    const auto constraint_revision=constraint_revision_?constraint_revision_(b.follower):0;
    if(b.initialized&&!graft_changed&&!changed.contains(follower.mesh)&&!changed.contains(source.mesh)&&relative.value==b.relative.value&&constraint_revision==b.constraint_revision) continue;
    b.initialized=true;b.relative=relative;b.graft_relatives=std::move(graft_relatives);
    b.constraint_revision=constraint_revision;
    GeometryKey cache;cache.points(b.input);cache.topology(scene_.meshes[follower.mesh]);cache.add(relative);cache.add(uint64_t(b.settings.collision_iterations));cache.add(uint64_t(b.settings.smoothing_iterations));cache.add(b.settings.weight);cache.add(std::bit_cast<uint64_t>(b.reference_edges));
    cache.add(constraint_revision);
    if(!b.reference_source.empty()){cache.points(b.reference_source);cache.points(b.reference_follower);cache.add(b.reference_relative);}
    cache.add(source.id);cache.points(scene_.meshes[source.mesh].positions);cache.topology(scene_.meshes[source.mesh]);cache.add(uint64_t(b.grafts.size()));
    for(size_t k=0;k<b.grafts.size();++k) {const auto &g=scene_.instances[b.grafts[k]];cache.add(g.id);cache.add(uint64_t(g.visible));cache.add(b.graft_relatives[k]);cache.points(scene_.meshes[g.mesh].positions);cache.topology(scene_.meshes[g.mesh]);}
    if(cache.value==b.cache_key&&b.output.size()==b.input.size()) {
      scene_.meshes[follower.mesh].positions=b.output;
      auto found=std::find_if(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e){return e.index==follower.mesh;});
      if(found==delta.meshes.end()) delta.meshes.push_back({follower.mesh,b.output});else found->positions=b.output;
      changed.insert(follower.mesh);++stats_.cache_hits;continue;
    }
    // 基础人体与可见附加网格分别施加外侧约束，避免最近面选到内层后漏碰撞。
    std::vector<ir::Mesh> surfaces{scene_.meshes[source.mesh]};
    for(size_t k=0;k<b.grafts.size();++k) {const auto g=b.grafts[k];if(!scene_.instances[g].visible) continue;
      const auto &instance=scene_.instances[g];auto part=scene_.meshes[instance.mesh];
      const auto matrix=b.graft_relatives[k];
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
    const auto &mesh=scene_.meshes[follower.mesh];
    if(!b.reference_source.empty()&&b.reference_radii.empty()){
      auto rest=scene_.meshes[source.mesh];rest.positions=b.reference_source;SurfaceIndex rest_index(rest);b.reference_radii.resize(b.reference_follower.size());
      batch(b.reference_radii.size(),[&](size_t v){const auto p=b.reference_relative.point(b.reference_follower[v]);const auto hit=rest_index.nearest(p);const auto w=hit.barycentric;const auto q=add(add(mul(rest.positions[hit.vertices[0]],w.x),mul(rest.positions[hit.vertices[1]],w.y)),mul(rest.positions[hit.vertices[2]],w.z));const auto gap=sub(p,q);b.reference_radii[v]=std::sqrt(dot(gap,gap))+.02f;});
    }
    constexpr std::array<ir::Vec3,4> samples{{{1.f/3,1.f/3,1.f/3},{.5f,.5f,0},{.5f,0,.5f},{0,.5f,.5f}}};
    struct Query {ir::Vec3 point,barycentric;size_t triangle=SIZE_MAX;};
    std::vector<std::vector<Query>> queries(surfaces.size(),std::vector<Query>(positions.size()+mesh.triangles.size()*4));
    auto nearest=[&](size_t surface,ir::Vec3 p,size_t slot) {
      auto &cache=queries[surface][slot];SurfaceBinding hit;
      if(parallel&&cache.triangle!=SIZE_MAX&&cache.point==p) {hit.vertices=surfaces[surface].triangles[cache.triangle].vertices;hit.barycentric=cache.barycentric;hit.polygon=surfaces[surface].triangles[cache.triangle].source_polygon;hit.triangle=cache.triangle;}
      else {
        if(surface==0&&!b.reference_source.empty()) {
          // Fit To 碰撞沿静止绑定的局部皮肤寻找接触。放下手臂时，衣身应保持在
          // 躯干外侧，不能改投到原本远离该布面的裸露前臂，再把衣身顶到前臂外面。
          ir::Vec3 reference;float radius=0;
          if(slot<positions.size()){reference=b.reference_relative.point(b.reference_follower[slot]);radius=b.reference_radii[slot];}
          else {const auto face=(slot-positions.size())/4,sample=(slot-positions.size())%4;const auto v=mesh.triangles[face].vertices;const auto w=samples[sample];
            const ir::Vec3 points[]={b.reference_relative.point(b.reference_follower[v[0]]),b.reference_relative.point(b.reference_follower[v[1]]),b.reference_relative.point(b.reference_follower[v[2]])};
            reference=add(add(mul(points[0],w.x),mul(points[1],w.y)),mul(points[2],w.z));
            for(int j=0;j<3;++j){const auto gap=sub(reference,points[j]);radius=std::max(radius,b.reference_radii[v[j]]+std::sqrt(dot(gap,gap)));}
          }
          hit=indexes[surface]->nearest(p,&b.reference_source,reference,radius);
        }else hit=indexes[surface]->nearest(p);
        cache={p,hit.barycentric,hit.triangle};
      }return hit;
    };
    // ERC 总体缩放会进入蒙皮顶点；固定毫米间隙会把微型角色的衣物推裂。
    // 用边长比例保留尺寸一致性，旋转和平移不影响该比例。
    const double current_edges=edge_measure(scene_.meshes[source.mesh]);
    const float shape_scale=b.reference_edges>0?float(std::sqrt(std::max(0.,current_edges)/b.reference_edges)):1.f;
    const float clearance=.0005f*shape_scale;
    auto correction=[&](ir::Vec3 p,size_t slot) {
      const auto input=p;const auto anchor=nearest(0,p,slot);
      for(size_t c=0;c<surfaces.size();++c) {
        const auto &body=surfaces[c];
        if(c&&std::find(body.graft_hidden_polygons.begin(),body.graft_hidden_polygons.end(),anchor.polygon)==body.graft_hidden_polygons.end()) continue;
        const auto hit=c?nearest(c,p,slot):anchor;const auto v=hit.vertices;const auto w=hit.barycentric;
        const auto q=add(add(mul(body.positions[v[0]],w.x),mul(body.positions[v[1]],w.y)),mul(body.positions[v[2]],w.z));
        auto n=add(add(mul(normals[c][v[0]],w.x),mul(normals[c][v[1]],w.y)),mul(normals[c][v[2]],w.z));
        const auto face=normal(sub(body.positions[v[1]],body.positions[v[0]]),sub(body.positions[v[2]],body.positions[v[0]]));
        const auto d=sub(p,q);const float length=std::sqrt(dot(d,d)),nl=std::sqrt(dot(n,n));if(nl<1e-8f) n=face;else n=mul(n,1/nl);
        if(dot(d,n)<0||length<clearance) p=add(q,mul(n,clearance));
      }
      return sub(p,input);
    };
    const auto inverse=ir::inverse(relative);
    auto constrain=[&] {if(constrain_&&constraint_revision){for(auto &p:positions)p=inverse.point(p);constrain_(b.follower,b.input,positions);for(auto &p:positions)p=relative.point(p);}};
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
      {diagnostics::Scope phase("vertices");batch(positions.size(),[&](size_t v){positions[v]=add(positions[v],correction(positions[v],v));});}
      // 同一轮先求出所有接触约束，再按顶点平均写回。逐面立即写回会让
      // 袖口相邻三角形相互推挤，将毫米级穿透放大为厘米级尖角。
      std::vector<std::array<ir::Vec3,4>> face_offsets(mesh.triangles.size());
      {diagnostics::Scope phase("face_queries");batch(mesh.triangles.size(),[&](size_t face){const auto v=mesh.triangles[face].vertices;float longest=0;
        for(int k=0;k<3;++k) {const auto e=sub(positions[v[k]],positions[v[(k+1)%3]]);longest=std::max(longest,dot(e,e));}
        if(longest<.0001f*shape_scale*shape_scale)return;
        for(size_t sample=0;sample<samples.size();++sample){const auto w=samples[sample];
          const auto p=add(add(mul(positions[v[0]],w.x),mul(positions[v[1]],w.y)),mul(positions[v[2]],w.z));
          face_offsets[face][sample]=correction(p,positions.size()+face*4+sample);
        }});}
      {diagnostics::Scope phase("faces");std::vector<ir::Vec3> offsets(positions.size());std::vector<uint32_t> counts(positions.size());
        for(size_t face=0;face<mesh.triangles.size();++face)for(size_t sample=0;sample<samples.size();++sample){const auto d=face_offsets[face][sample];if(d==ir::Vec3{})continue;
          const auto v=mesh.triangles[face].vertices;const auto w=samples[sample];const float weights[]={w.x,w.y,w.z};const float sum=dot(w,w);
          for(int j=0;j<3;++j)if(weights[j]>0){offsets[v[j]]=add(offsets[v[j]],mul(d,weights[j]/sum));++counts[v[j]];}
        }
        for(size_t v=0;v<positions.size();++v)if(counts[v])positions[v]=add(positions[v],mul(offsets[v],1.f/float(counts[v])));
      }
      // 从身体顶点反查服装面，捕获粗服装三角形采样点之间的小凸起。
      auto cloth=mesh;cloth.positions=positions;SurfaceIndex cloth_index(cloth);
      std::vector<ir::Vec3> reverse_offsets(positions.size());std::vector<uint32_t> reverse_counts(positions.size());
      for(size_t c=0;c<surfaces.size();++c) {
      // 查询与约束使用同一份几何。立即写回会让后续查询的重心坐标失效，
      // 并把袖口外裸露手臂的旧命中点反复拉进服装，产生不断放大的尖角。
      std::vector<SurfaceBinding> hits(surfaces[c].positions.size());
      {diagnostics::Scope phase("reverse_queries");batch(hits.size(),[&](size_t k){hits[k]=cloth_index.nearest(surfaces[c].positions[k]);});}
      for(size_t k=0;k<surfaces[c].positions.size();++k) {
        const auto p=surfaces[c].positions[k];const auto &hit=hits[k];const auto v=hit.vertices;const auto w=hit.barycentric;
        if(std::min({w.x,w.y,w.z})<.01f) continue; // 不把裸露区域牵到领口或袖口边缘。
        if(c==0&&!b.reference_source.empty()) {
          // 放下手臂后，裸露的手臂会靠近衣身侧面，不能把它误当成衣身下的凸起。
          // 反向约束只补充静止绑定时已由该布面覆盖的皮肤。
          const auto a=b.reference_relative.point(b.reference_follower[v[0]]),u=b.reference_relative.point(b.reference_follower[v[1]]),z=b.reference_relative.point(b.reference_follower[v[2]]);
          const auto rest=b.reference_source[k],weights=closest_barycentric(rest,a,u,z);
          if(std::min({weights.x,weights.y,weights.z})<.01f)continue;
          const auto gap=sub(rest,add(add(mul(a,weights.x),mul(u,weights.y)),mul(z,weights.z)));
          if(dot(gap,gap)>.02f*.02f)continue;
        }
        const auto q=add(add(mul(cloth.positions[v[0]],w.x),mul(cloth.positions[v[1]],w.y)),mul(cloth.positions[v[2]],w.z));
        const auto n=normal(sub(cloth.positions[v[1]],cloth.positions[v[0]]),sub(cloth.positions[v[2]],cloth.positions[v[0]]));
        const float depth=dot(sub(p,q),n);
        if(depth<=0||depth>.02f*shape_scale||dot(n,normals[c][k])<.5f) continue;
        const auto d=mul(n,depth+clearance);const float sum=w.x*w.x+w.y*w.y+w.z*w.z;
        const float weights[]={w.x,w.y,w.z};for(int j=0;j<3;++j){reverse_offsets[v[j]]=add(reverse_offsets[v[j]],mul(d,weights[j]/sum));++reverse_counts[v[j]];}
      }}
      for(size_t v=0;v<positions.size();++v)if(reverse_counts[v])positions[v]=add(positions[v],mul(reverse_offsets[v],1.f/float(reverse_counts[v])));
      // 碰撞与保形交替投影；不能让碰撞把已经关闭形变的薄鞋底重新顶出尖角。
      constrain();
    }
    {diagnostics::Scope phase("vertices");batch(positions.size(),[&](size_t v){positions[v]=add(positions[v],correction(positions[v],v));});}
    constrain();uint64_t corrected=0;
    for(size_t v=0;v<positions.size();++v) {
      const auto d=sub(positions[v],initial[v]);
      if(dot(d,d)>1e-16f) {positions[v]=inverse.point(positions[v]);++corrected;} else positions[v]=b.input[v];
    }
    auto &current=scene_.meshes[follower.mesh].positions;
    b.output=positions;b.cache_key=cache.value;
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
