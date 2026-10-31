#include "runtime/conform.h"
#include "runtime/geometry_key.h"
#include "runtime/rigid_follow.h"
#include "runtime/fit_profile.h"
#include "diagnostics/load_profile.h"
#include <algorithm>
#include <bit>
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
  const std::vector<ir::Vec3> *reference_=nullptr;
  std::vector<ir::Bounds> reference_bounds_;
  void build(size_t begin,size_t end,int index) {
    Branch branch;branch.begin=begin;branch.end=end;
    for(size_t i=begin;i<end;++i) for(auto v:mesh_.triangles[triangles_[i]].vertices) branch.bounds.add(mesh_.positions[v]);
    branches_[index]=branch;
    if(end-begin>12) {const auto extent=sub(branch.bounds.maximum,branch.bounds.minimum);const int axis=extent.x>extent.y?(extent.x>extent.z?0:2):(extent.y>extent.z?1:2);
      auto center=[&](size_t face) {float sum=0;for(auto v:mesh_.triangles[face].vertices) {const auto p=mesh_.positions[v];sum+=axis==0?p.x:axis==1?p.y:p.z;}return sum;};
      const size_t middle=(begin+end)/2;std::nth_element(triangles_.begin()+begin,triangles_.begin()+middle,triangles_.begin()+end,[&](size_t a,size_t b) {return center(a)<center(b);});
      const int left=index*2+1,right=left+1;branches_[index].left=left;branches_[index].right=right;
      if(end-begin>=8192)Concurrency::parallel_invoke([&]{build(begin,middle,left);},[&]{build(middle,end,right);});
      else {build(begin,middle,left);build(middle,end,right);}}
  }
public:
  explicit SurfaceIndex(const ir::Mesh &mesh,const std::vector<ir::Vec3> *reference=nullptr):mesh_(mesh),reference_(reference) {
    diagnostics::Scope phase("surface_index_build");triangles_.resize(mesh.triangles.size());std::iota(triangles_.begin(),triangles_.end(),0);if(!triangles_.empty()){branches_.resize(std::bit_ceil((triangles_.size()+11)/12)*2-1);build(0,triangles_.size(),0);}
    if(reference_){reference_bounds_.resize(branches_.size());for(size_t i=branches_.size();i-->0;){const auto &b=branches_[i];auto &bounds=reference_bounds_[i];
      if(b.left<0){for(size_t k=b.begin;k<b.end;++k)for(auto v:mesh_.triangles[triangles_[k]].vertices)bounds.add(reference_->at(v));}
      else for(auto child:{b.left,b.right}){bounds.add(reference_bounds_[child].minimum);bounds.add(reference_bounds_[child].maximum);}}}
  }
  // 碰撞迭代只改顶点，拓扑不变；复用树的分区，仅自底向上更新包围盒。
  void refit() {
    diagnostics::Scope phase("surface_index_refit");
    for(size_t i=branches_.size();i-->0;){auto &b=branches_[i];b.bounds={};
      if(b.left<0){for(size_t k=b.begin;k<b.end;++k)for(auto v:mesh_.triangles[triangles_[k]].vertices)b.bounds.add(mesh_.positions[v]);}
      else for(auto child:{b.left,b.right}){b.bounds.add(branches_[child].bounds.minimum);b.bounds.add(branches_[child].bounds.maximum);}}
  }
  // 有向交点计数识别身体自接触时重叠的体积；奇偶计数会把两层重叠误判为外侧。
  // 只在确实位于封闭表面内部时沿布面外侧寻找出口，靠近裸露手臂不产生推力。
  std::optional<ir::Vec3> exterior(ir::Vec3 p,ir::Vec3 ray,float clearance) const {
    const float length=std::sqrt(dot(ray,ray));if(length<1e-8f)return {};ray=mul(ray,1/length);
    struct Hit {float distance;int sign;};std::vector<Hit> hits;
    std::array<int,8*sizeof(size_t)+1> stack{};size_t pending=branches_.empty()?0:1;
    while(pending){const auto &branch=branches_[stack[--pending]];float near=0,far=std::numeric_limits<float>::max();
      const float origin[]={p.x,p.y,p.z},direction[]={ray.x,ray.y,ray.z},lo[]={branch.bounds.minimum.x,branch.bounds.minimum.y,branch.bounds.minimum.z},hi[]={branch.bounds.maximum.x,branch.bounds.maximum.y,branch.bounds.maximum.z};
      bool valid=true;for(int a=0;a<3;++a){if(std::abs(direction[a])<1e-9f){if(origin[a]<lo[a]||origin[a]>hi[a])valid=false;}else{float x=(lo[a]-origin[a])/direction[a],y=(hi[a]-origin[a])/direction[a];if(x>y)std::swap(x,y);near=std::max(near,x);far=std::min(far,y);}}
      if(!valid||far<near)continue;if(branch.left>=0){stack[pending++]=branch.left;stack[pending++]=branch.right;continue;}
      for(size_t f=branch.begin;f<branch.end;++f){const auto v=mesh_.triangles[triangles_[f]].vertices;const auto a=mesh_.positions[v[0]],u=sub(mesh_.positions[v[1]],a),w=sub(mesh_.positions[v[2]],a);
        const ir::Vec3 n{u.y*w.z-u.z*w.y,u.z*w.x-u.x*w.z,u.x*w.y-u.y*w.x};const float denominator=dot(n,ray);if(std::abs(denominator)<1e-12f)continue;
        const float t=dot(n,sub(a,p))/denominator;if(t<=1e-7f)continue;const auto q=add(p,mul(ray,t)),bary=closest_barycentric(q,a,add(a,u),add(a,w));
        const auto error=sub(q,add(a,add(mul(u,bary.y),mul(w,bary.z))));if(dot(error,error)>1e-12f)continue;
        hits.push_back({t,denominator>0?1:-1});
      }
    }
    std::sort(hits.begin(),hits.end(),[](const auto &a,const auto &b){return a.distance<b.distance;});
    std::vector<Hit> crossings;for(auto h:hits){if(!crossings.empty()&&std::abs(h.distance-crossings.back().distance)<1e-6f){if(h.sign!=crossings.back().sign)crossings.back().sign=0;}else crossings.push_back(h);}
    int winding=0;for(auto h:crossings)winding+=h.sign;if(winding<=0)return {};
    for(auto h:crossings){winding-=h.sign;if(winding<=0)return add(p,mul(ray,h.distance+clearance));}return {};
  }
  SurfaceBinding nearest(ir::Vec3 p,const std::vector<ir::Vec3> *reference=nullptr,ir::Vec3 reference_point={},float reference_radius=0,float maximum=std::numeric_limits<float>::max(),size_t hint=SIZE_MAX) const {
    if(branches_.empty()) throw std::runtime_error("Fit To 目标没有可用于形变转移的三角形");
    // 中位数二分树的深度受 size_t 位数约束；每次查询不再分配动态栈。
    float best=maximum;SurfaceBinding result;std::array<int,8*sizeof(size_t)+1> stack{};size_t pending=1;
    // 上一轮命中只提供搜索上界，仍按树顺序求精确最近点，保留等距面的选择顺序。
    if(hint<mesh_.triangles.size()) {const auto v=mesh_.triangles[hint].vertices;bool valid=true;
      if(reference){const auto a=reference->at(v[0]),b=reference->at(v[1]),c=reference->at(v[2]),w=closest_barycentric(reference_point,a,b,c);const auto d=sub(reference_point,add(add(mul(a,w.x),mul(b,w.y)),mul(c,w.z)));valid=dot(d,d)<=reference_radius*reference_radius;}
      if(valid){const auto a=mesh_.positions[v[0]],b=mesh_.positions[v[1]],c=mesh_.positions[v[2]],w=closest_barycentric(p,a,b,c);const auto d=sub(p,add(add(mul(a,w.x),mul(b,w.y)),mul(c,w.z)));
        // 三角形插值和包围盒距离的舍入路径不同，搜索上界需留保守余量。
        const float radius=std::sqrt(dot(d,d))+1e-5f*std::max(1.f,std::sqrt(dot(p,p)));best=std::min(best,radius*radius);}}
    while(pending) {const auto index=stack[--pending];const auto &b=branches_[index];if(box_distance(b.bounds,p)>best) continue;
      if(reference&&reference==reference_&&box_distance(reference_bounds_[index],reference_point)>reference_radius*reference_radius)continue;
      if(b.left>=0) {const bool left_first=box_distance(branches_[b.left].bounds,p)<box_distance(branches_[b.right].bounds,p);stack[pending++]=left_first?b.right:b.left;stack[pending++]=left_first?b.left:b.right;continue;}
      for(size_t i=b.begin;i<b.end;++i) {const auto vertices=mesh_.triangles[triangles_[i]].vertices;const auto a=mesh_.positions[vertices[0]],c=mesh_.positions[vertices[1]],d=mesh_.positions[vertices[2]];
        const auto bary=closest_barycentric(p,a,c,d),distance=sub(p,add(add(mul(a,bary.x),mul(c,bary.y)),mul(d,bary.z)));const float square=dot(distance,distance);
        if(square<best) {
          if(reference){const auto x=reference->at(vertices[0]),y=reference->at(vertices[1]),z=reference->at(vertices[2]);const auto w=closest_barycentric(reference_point,x,y,z);const auto gap=sub(reference_point,add(add(mul(x,w.x),mul(y,w.y)),mul(z,w.z)));if(dot(gap,gap)>reference_radius*reference_radius)continue;}
          best=square;result={vertices,bary};result.polygon=mesh_.triangles[triangles_[i]].source_polygon;result.triangle=triangles_[i];}}
    }
    if(result.triangle==SIZE_MAX&&maximum==std::numeric_limits<float>::max())throw std::runtime_error("未找到有效的表面绑定三角形");
    return result;
  }
};
bool transferable(const Morph &m) {return m.evaluable&&m.auto_follow&&m.kind!="alias"&&m.alias_morph<0&&m.has_offsets();}
bool upper_garment(const Target &target,const ir::Scene &scene) {
  const auto &instance=scene.instances[target.instance];const auto &cloth=scene.meshes[instance.mesh];const auto &type=target.content_type;
  const bool hint=type.starts_with("Follower/Wardrobe")||type=="Follower"||((type.empty()||type=="Actor/Character")&&!target.preferred_base.empty());
  if(!hint||target.character||!target.auto_fit_base.empty()||cloth.graft_target_vertices||!cloth.graft_vertex_pairs.empty()||instance.shell_source>=0||type.starts_with("Follower/Wardrobe/Underwear")||type.starts_with("Follower/Wardrobe/Swimwear"))return false;
  return std::any_of(cloth.triangles.begin(),cloth.triangles.end(),[&](const auto &f){if(f.polygon_group>=cloth.polygon_groups.size())return false;auto name=cloth.polygon_groups[f.polygon_group];for(auto &c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return name.find("chest")!=std::string::npos||name.find("pectoral")!=std::string::npos;});
}
// 只将由相同骨骼轴驱动的衣物修正视为已覆盖，避免男性 JCM 与女性投射重复叠加。
// 体型控制器或混合了其他输入的公式不通过此校验。
std::set<std::pair<std::string,uint32_t>> pose_drivers(const FormulaGraph &graph,const std::vector<Skin> &skins,size_t morph) {
  std::set<std::pair<std::string,uint32_t>> result;
  if(graph.skin<0||graph.morph_channels.at(morph)<0)return result;
  std::vector<uint32_t> pending{uint32_t(graph.morph_channels[morph])};std::set<uint32_t> seen;
  while(!pending.empty()) {
    const auto channel=pending.back();pending.pop_back();if(!seen.insert(channel).second)continue;
    const auto &binding=graph.channels.at(channel).binding;
    if(binding.property==Property::rotation){const auto &joint=skins.at(size_t(graph.skin)).joints.at(binding.index);if(joint.name.empty())return {};result.emplace(joint.name,binding.axis);continue;}
    if(binding.property!=Property::morph)return {};
    bool driven=false;for(auto e:graph.incoming.at(channel))if(graph.expressions[e].enabled){driven=true;const auto inputs=graph.expressions[e].inputs();pending.insert(pending.end(),inputs.begin(),inputs.end());}
    if(!driven)return {};
  }
  return result;
}
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
  std::map<std::pair<size_t,std::string>,ir::Mesh> reference_meshes;
  std::map<std::pair<size_t,std::string>,std::unique_ptr<SurfaceIndex>> surface_indexes;
  for(auto &l:links_) {
    diagnostics::Scope binding_scope(diagnostics::active?"conform_binding/"+targets[l.follower].id:std::string{});
    const auto &source=targets[l.source];const auto &follower=targets[l.follower];const auto &g=graphs[l.follower];
    l.reference_shape=fit_reference_shape(follower.preferred_base,source.auto_fit_base,follower.content_type);l.cross_base=!l.reference_shape.empty();
    l.retarget_skeleton=genesis9_footwear_fit(follower.preferred_base,source.auto_fit_base,follower.content_type);
    std::map<std::string,std::vector<size_t>> names;for(size_t m=0;m<source.morphs.size();++m) if(transferable(source.morphs[m])) names[source.morphs[m].channel_id].push_back(m);
    l.morph_sources.resize(follower.morphs.size(),-1);std::set<size_t> authored;
    for(size_t m=0;m<follower.morphs.size();++m) {const auto &morph=follower.morphs[m];if(!transferable(morph)||morph.channel_id.empty()) continue;
      const auto found=names.find(morph.channel_id);if(found==names.end()||found->second.size()!=1) continue;const auto source_morph=found->second.front();
      // 自带 ERC 的修正由继承的骨骼驱动，不再叠加同一人体修正值。
      const int c=g.morph_channels.at(m);bool driven=false;if(c>=0) for(auto e:g.incoming.at(size_t(c))) driven|=g.expressions[e].enabled;
      if(l.cross_base){const auto drivers=pose_drivers(g,skins,m);if(drivers.empty()||drivers!=pose_drivers(graphs[l.source],skins,source_morph))continue;}
      else if(!driven) l.morph_sources[m]=int(source_morph);
      authored.insert(source_morph);++stats_.authored_morphs;
    }
    for(size_t m=0;m<source.morphs.size();++m) if(transferable(source.morphs[m])&&!authored.contains(m)) l.projected_morphs.push_back(m);
    if(l.follower_skin>=0&&l.source_skin>=0) {
      const auto &a=skins.at(size_t(l.source_skin)),&b=skins.at(size_t(l.follower_skin));l.joints.resize(b.joints.size(),-1);
      for(size_t j=0;j<b.joints.size();++j) {if(j==0&&!a.joints.empty()) {l.joints[j]=0;continue;}const auto &joint=b.joints[j];const auto match=l.retarget_skeleton?genesis9_joint_to_genesis8(joint.name):joint.name;
        // 跨资产的 name 比局部 ID 更稳定；G8.1 的部分 Carpal ID 已与名称错位。
        std::vector<size_t> names;for(size_t k=1;k<a.joints.size();++k) if(!joint.name.empty()&&match==a.joints[k].name) names.push_back(k);
        if(names.size()==1) l.joints[j]=int(names.front());
        else {
          for(size_t k=1;k<a.joints.size();++k) if(joint.id==a.joints[k].id&&(names.empty()||match==a.joints[k].name)) {
            if(l.joints[j]>=0) throw std::runtime_error("Fit To 骨骼 ID 不唯一："+joint.id);l.joints[j]=int(k);
          }
          if(names.size()>1&&l.joints[j]<0) throw std::runtime_error("Fit To 骨骼名称不唯一："+joint.name);
        }
      }
      if(l.retarget_skeleton)for(const auto &weights:b.weights)for(const auto &w:weights)if(w.weight>0&&
        (!genesis9_footwear_weight(b.joints.at(w.joint).name)||l.joints.at(w.joint)<0))
        throw std::runtime_error("此 G9 鞋类的蒙皮使用了尚未支持的骨骼："+b.joints.at(w.joint).name+"；当前支持小腿、脚、跖骨和脚趾");
    }
    if(l.retarget_skeleton&&(l.follower_skin<0||l.source_skin<0))throw std::runtime_error("G9 鞋类适配需要服装和目标角色的骨架");
    const auto &a=scene.instances.at(source.instance),&b=scene.instances.at(follower.instance);const auto follower_to_source=inverse(a.transform)*b.transform;l.source_to_follower=inverse(b.transform)*a.transform;
    const auto &original_body=scene.meshes.at(a.mesh);auto cloth=scene.meshes.at(b.mesh);
    const auto frame_key=std::make_pair(l.source,l.reference_shape);
    if(l.cross_base) {
      if(cloth.graft_target_vertices||!cloth.graft_vertex_pairs.empty())throw std::runtime_error("GeoGraft 不支持跨基型服装转换");
      const Morph *clone=nullptr;
        for(const auto &m:source.morphs)if(m.channel_id==l.reference_shape&&m.has_offsets()&&m.intrinsic_error.empty()) {
          if(clone)throw std::runtime_error("服装转换参考形体不唯一："+l.reference_shape);clone=&m;
        }
        if(!clone)throw std::runtime_error("服装适配需要目标女性的 "+l.reference_shape+" 参考形体，请安装包含该 Clone 的 Genesis 8 Female 资源后刷新参数");
      if(l.retarget_skeleton){
        if(clone->clone_pose.empty())throw std::runtime_error("G9 鞋类适配还需要 CloneGenesis9.dsj 静止姿势模板，请安装对应 Clone 资源后刷新参数");
        const auto &skin=skins.at(size_t(l.follower_skin));std::vector<JointPose> pose(skin.joints.size());
        for(size_t j=0;j<pose.size();++j){const auto name=j==0?"ROOT_SKELETON":skin.joints[j].name;if(const auto it=clone->clone_pose.find(name);it!=clone->clone_pose.end())pose[j]=it->second;}
        l.prepose=vertex_transforms(skin,pose);for(size_t v=0;v<cloth.positions.size();++v)cloth.positions[v]=l.prepose[v].point(cloth.positions[v]);
      }
      if(!reference_meshes.contains(frame_key)) {
        auto reference=original_body;
        for(const auto &offset:clone->data())reference.positions.at(offset.vertex)=add(reference.positions.at(offset.vertex),offset.delta);
        reference_meshes.emplace(frame_key,std::move(reference));
      }
      const auto &reference=reference_meshes.at(frame_key).positions;l.base_offsets.resize(reference.size());
      for(size_t v=0;v<reference.size();++v)l.base_offsets[v]=sub(original_body.positions[v],reference[v]);
    }
    const auto &body=l.cross_base?reference_meshes.at(frame_key):original_body;
    if(l.follower_skin>=0)for(const auto &group:cloth.bone_rigidity){BoneRigidity binding;binding.group=group;binding.skin=l.follower_skin;
      const auto &joints=skins[size_t(l.follower_skin)].joints;
      for(const auto &node:group.transform_nodes){size_t found=joints.size();for(size_t j=0;j<joints.size();++j)if(joints[j].id==node){found=j;break;}if(found==joints.size())throw std::runtime_error("刚性区域骨骼不存在："+node);if(found>=l.joints.size()||l.joints[found]<0)binding.joints.push_back(found);}
      bone_rigidity_[l.follower].push_back(std::move(binding));
    }
    GeometryKey key;key.points(body.positions);key.topology(body);key.points(cloth.positions);key.topology(cloth);key.add(follower_to_source);l.geometry_key=key.value;
    const ConformLink *cached=nullptr;if(reuse) for(const auto &old:reuse->links_) if(old.geometry_key==l.geometry_key&&reuse->targets_[old.follower].id==follower.id&&reuse->targets_[old.source].id==source.id) {cached=&old;break;}
    if(cached) {l.surface=cached->surface;l.neighbors=cached->neighbors;stats_.bindings+=l.surface.size();continue;}
    if(const auto saved=surface_cache.get(l.geometry_key)){diagnostics::Scope cache_hit("conform_binding_cache_hit");l.surface=saved->surface;l.neighbors=saved->neighbors;stats_.bindings+=l.surface.size();continue;}
    auto &cached_index=surface_indexes[frame_key];if(!cached_index) cached_index=std::make_unique<SurfaceIndex>(body);const auto &index=*cached_index;l.surface.reserve(cloth.positions.size());
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
  for(auto &link:links_){
    // 商业上衣也会被标为 Skirt 或 Actor/Character；结合适配声明和实际胸片。
    // 真正角色、GeoGraft、几何壳不能因为带有胸部分组就作为布料处理。
    if(!link.cross_base&&!upper_garment(targets[link.follower],scene))continue;
    const auto &body=scene.meshes[scene.instances[targets[link.source].instance].mesh];link.torso_envelope.resize(link.surface.size());
    for(size_t v=0;v<link.surface.size();++v){const auto g=body.triangles.at(link.surface[v].triangle).polygon_group;if(g>=body.polygon_groups.size())continue;auto name=body.polygon_groups[g];for(auto &c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');link.torso_envelope[v]=(name=="chest"||name.find("pectoral")!=std::string::npos||name.find("abdomen")!=std::string::npos)?255:0;}
    // 解剖分组边界不是服装接缝，平滑权重，避免胸片与袖片之间突然跳变。
    std::vector<float> mask(link.torso_envelope.begin(),link.torso_envelope.end()),filtered=mask;
    for(int iteration=0;iteration<64;++iteration){for(size_t v=0;v<mask.size();++v)if(!link.neighbors[v].empty()){float sum=0;for(auto n:link.neighbors[v])sum+=mask[n];filtered[v]=.5f*(mask[v]+sum/float(link.neighbors[v].size()));}mask.swap(filtered);}
    for(size_t v=0;v<mask.size();++v)link.torso_envelope[v]=uint8_t(std::clamp(mask[v],0.f,255.f));
  }
  previous_weights_.resize(links_.size());source_revisions_.resize(links_.size(),std::numeric_limits<uint64_t>::max());
  follow_revisions_.resize(links_.size(),std::numeric_limits<uint64_t>::max());
  for(const auto &link:links_)if(!surface_frames_.contains({link.source,link.reference_shape})) {
    const auto key=std::make_pair(link.source,link.reference_shape);
    const auto &mesh=link.cross_base?reference_meshes.at(key):scene.meshes[scene.instances[targets[link.source].instance].mesh];auto &frames=surface_frames_[key];frames.reference=mesh.positions;
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
  if(projection_inputs_.size()!=links_.size()){
    projection_inputs_.resize(links_.size());for(size_t i=0;i<links_.size();++i){const auto &l=links_[i];const auto &frames=surface_frames_.at({l.source,l.reference_shape});auto &vertices=projection_inputs_[i].vertices;
      for(const auto &b:l.surface)for(auto v:b.vertices){vertices.push_back(v);vertices.insert(vertices.end(),frames.neighborhoods[v].begin(),frames.neighborhoods[v].end());}
      std::sort(vertices.begin(),vertices.end());vertices.erase(std::unique(vertices.begin(),vertices.end()),vertices.end());}
  }
  for(auto target:order_) {const auto *l=link(target);if(!l) continue;const auto i=size_t(link_for_target_[target]);std::vector<float> current;current.reserve(l->projected_morphs.size());
    for(auto m:l->projected_morphs) current.push_back(weights.at(l->source).at(m));
    if(current==previous_weights_[i]&&source_revisions_[i]==revisions_[l->source]&&follow_revisions_[i]==morph.follow_revision(target)) continue;
    std::vector<ir::Vec3> source(vertex_counts_[l->source]);if(!offsets_[l->source].empty()) source=offsets_[l->source];
    if(l->cross_base)for(size_t v=0;v<source.size();++v)source[v]=add(source[v],l->base_offsets[v]);
    for(size_t m=0;m<current.size();++m) if(current[m]!=0) for(const auto &d:targets_[l->source].morphs[l->projected_morphs[m]].data()) source[d.vertex]=add(source[d.vertex],mul(d.delta,current[m]));
    // 只比较实际绑定及其旋转邻域。脚部 JCM 变化不应重算上衣的整个投射和平滑场。
    auto &input=projection_inputs_[i];bool same=input.offsets.size()==input.vertices.size()&&follow_revisions_[i]==morph.follow_revision(target);
    for(size_t k=0;same&&k<input.vertices.size();++k)same=input.offsets[k]==source[input.vertices[k]];
    if(same){previous_weights_[i]=std::move(current);source_revisions_[i]=revisions_[l->source];continue;}
    input.offsets.resize(input.vertices.size());for(size_t k=0;k<input.vertices.size();++k)input.offsets[k]=source[input.vertices[k]];
    const auto &frames=surface_frames_.at({l->source,l->reference_shape});std::vector<ir::Transform> rotations(source.size());std::vector<uint8_t> needed(source.size());
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
    // 上衣的连续布片不能逐点复制身体凹陷。同基型自动 Morph 也需要过渡，
    // 只处理自动生成的位移，作者提供的体型修正、口袋和门襟不参与平滑。
    for(int iteration=0;iteration<(!l->torso_envelope.empty()?96:24);++iteration) {
      for(size_t v=0;v<offsets.size();++v) {const auto &neighbors=l->neighbors[v];if(neighbors.size()<3) continue;
        // 贴体区域保留表面形变；远离身体的裙摆才逐渐增强平滑，避免压平鞋面。
        const float minimum=l->cross_base?.35f:l->torso_envelope.empty()?0.f:(.35f/255)*l->torso_envelope[v];
        float weight=std::clamp((l->surface[v].distance-.03f)/.06f,minimum,.5f);
        if(iteration>=24&&!l->cross_base)weight*=l->torso_envelope[v]/255.f;
        ir::Vec3 average;for(auto n:neighbors) average=add(average,offsets[n]);filtered[v]=add(mul(offsets[v],1-weight),mul(average,weight/float(neighbors.size())));}
      offsets.swap(filtered);
    }
    if(!l->torso_envelope.empty()){
      // 胸腹布片跨过体型凹陷，避免把乳沟和胸下沟完整刻进布面。
      // 只向外平滑生成的位移，不改变作者褶皱；鞋、袖口和裤腿不参与。
      std::vector<ir::Vec3> outward(offsets.size());
      for(size_t v=0;v<offsets.size();++v)if(l->torso_envelope[v]){const auto &b=l->surface[v];auto n=add(add(mul(direction(rotations[b.vertices[0]],b.normal),b.barycentric.x),mul(direction(rotations[b.vertices[1]],b.normal),b.barycentric.y)),mul(direction(rotations[b.vertices[2]],b.normal),b.barycentric.z));n=direction(l->source_to_follower,n);const float length=std::sqrt(dot(n,n));if(length>0)outward[v]=mul(n,1/length);}
      for(int iteration=0;iteration<192;++iteration){filtered=offsets;
        for(size_t v=0;v<offsets.size();++v)if(l->torso_envelope[v]&&!l->neighbors[v].empty()){ir::Vec3 average;for(auto n:l->neighbors[v])average=add(average,offsets[n]);average=mul(average,1.f/float(l->neighbors[v].size()));const float shift=(.4f/255)*l->torso_envelope[v]*std::max(0.f,dot(sub(average,offsets[v]),outward[v]));filtered[v]=add(offsets[v],mul(outward[v],shift));}
        offsets.swap(filtered);
      }
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
    if(link.cross_base) {
      const auto &source_joint=skins.at(size_t(link.source_skin)).joints.at(size_t(link.joints[j]));
      const auto &follower_joint=skins.at(size_t(link.follower_skin)).joints.at(j);
      p.center_offset_cm=add(p.center_offset_cm,sub(source_joint.center_cm,follower_joint.center_cm));
      p.end_offset_cm=add(p.end_offset_cm,sub(source_joint.end_cm,follower_joint.end_cm));
      p.orientation_offset_degrees=add(p.orientation_offset_degrees,sub(source_joint.orientation_degrees,follower_joint.orientation_degrees));
    }
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
    const auto node="#"+t.id.substr(0,t.id.rfind('/'));
    for(const auto &other:targets)if(other.instance!=t.instance&&(other.conform_target==node||other.smoothing.collision_target==node))b.required_when_hidden=true;
    for(size_t i=0;i<scene.instances.size();++i)if(i!=t.instance&&(scene.instances[i].mesh==scene.instances[t.instance].mesh||scene.instances[i].shell_source==int(t.instance)))b.required_when_hidden=true;
    for(const auto &source:targets)if(source.instance==b.source)b.cross_base=!fit_reference_shape(t.preferred_base,source.auto_fit_base,t.content_type).empty();
    if(b.cross_base){b.settings.collision_iterations=std::max(b.settings.collision_iterations,10);b.settings.smoothing_iterations=std::max(b.settings.smoothing_iterations,8);}
    b.reference_edges=edge_measure(scene.meshes.at(scene.instances.at(b.source).mesh));
    const auto &mesh=scene.meshes.at(scene.instances.at(t.instance).mesh);
    if(b.cross_base){auto subdivide=[](ir::Mesh m){if(m.polygons.empty()||!m.subdivision.enabled)return Subdivision{};m.subdivision.level=1;m.hidden_polygons.clear();return Subdivision(m);};b.source_subdivision=subdivide(scene.meshes.at(scene.instances.at(b.source).mesh));b.follower_subdivision=subdivide(mesh);}
    if(mesh.triangles.empty()) continue;
    if(scene.meshes.at(scene.instances.at(b.source).mesh).triangles.empty()) throw std::runtime_error("碰撞对象没有表面："+s.collision_target);
    if(t.conform_target==s.collision_target){b.reference_source=scene.meshes.at(scene.instances.at(b.source).mesh).positions;b.reference_follower=mesh.positions;b.reference_relative=relative_(b.source,b.follower);}
    b.preserve_layers=!b.cross_base&&!b.reference_follower.empty()&&upper_garment(t,scene);
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
    if(&scene_==&previous.scene_&&b.source==old.source&&b.follower==old.follower&&b.grafts==old.grafts&&b.settings==old.settings&&b.cross_base==old.cross_base&&b.preserve_layers==old.preserve_layers&&b.reference_edges==old.reference_edges&&b.input.size()==old.input.size()) {
      b.input=old.input;b.initialized=old.initialized;b.relative=old.relative;b.graft_relatives=old.graft_relatives;b.constraint_revision=old.constraint_revision;
    }
    break;
  }
}
void CollisionRuntime::reference_positions(uint32_t source,const std::vector<ir::Vec3> &positions) {
  for(auto &b:bindings_)if(!b.reference_source.empty()){
    if(b.source==source){b.reference_source=positions;b.reference_radii.clear();b.initialized=false;b.cache_key=0;}
    if(b.follower==source){b.reference_follower=positions;b.reference_radii.clear();b.layers.clear();b.layers_ready=false;b.initialized=false;b.cache_key=0;}
  }
  for(auto &b:bindings_)if(b.source==source) {
    const auto &mesh=scene_.meshes[scene_.instances[source].mesh];double sum=0;
    for(const auto &f:mesh.triangles)for(int k=0;k<3;++k){const auto e=sub(positions.at(f.vertices[k]),positions.at(f.vertices[(k+1)%3]));sum+=double(e.x)*e.x+double(e.y)*e.y+double(e.z)*e.z;}
    b.reference_edges=sum;b.initialized=false;b.cache_key=0;
  }
}
void CollisionRuntime::reference_surface(uint32_t follower,const std::vector<ir::Vec3> &positions) {
  for(auto &b:bindings_)if(b.follower==follower&&!b.reference_source.empty()) {
    if(b.reference_source.size()!=positions.size())throw std::runtime_error("碰撞参考形体拓扑不一致");
    b.reference_source=positions;b.reference_radii.clear();b.initialized=false;b.cache_key=0;
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
    // 独立隐藏衣物无需碰撞；作为其他对象的宿主、碰撞体或共享网格时仍完整求值。
    // 重新显示时即使没有新的姿势增量，也必须重新校验当前输入。
    if(!follower.visible&&!b.required_when_hidden){b.initialized=false;continue;}
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
    cache.add(uint64_t(b.cross_base));
    cache.add(uint64_t(b.preserve_layers));
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
      indexes.push_back(std::make_unique<SurfaceIndex>(body,parallel&& &body==&surfaces[0]&&!b.reference_source.empty()?&b.reference_source:nullptr));normals.emplace_back(body.positions.size());auto &n=normals.back();
      for(const auto &f:body.triangles) {
        const auto a=body.positions[f.vertices[0]],u=sub(body.positions[f.vertices[1]],a),v=sub(body.positions[f.vertices[2]],a);
        const ir::Vec3 face{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        for(auto k:f.vertices) n[k]=add(n[k],face);
      }
      for(auto &v:n) {const float length=std::sqrt(dot(v,v));if(length>0) v=mul(v,1/length);}
    }
    auto positions=b.input;for(auto &p:positions) p=relative.point(p);const auto initial=positions;
    const auto &mesh=scene_.meshes[follower.mesh];
    if(b.preserve_layers&&!b.layers_ready){
      diagnostics::Scope phase("cloth_layer_binding");auto rest=mesh;rest.positions=b.reference_follower;SurfaceIndex index(rest);std::vector<ir::Vec3> normals(rest.positions.size());
      for(const auto &f:rest.triangles){const auto a=rest.positions[f.vertices[0]],u=sub(rest.positions[f.vertices[1]],a),v=sub(rest.positions[f.vertices[2]],a);const ir::Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};for(auto k:f.vertices)normals[k]=add(normals[k],n);}
      auto bind=[&](std::array<uint32_t,3> outer,ir::Vec3 weights,ir::Vec3 n){const float length=std::sqrt(dot(n,n));if(length<1e-8f)return;n=mul(n,1/length);const auto p=add(add(mul(rest.positions[outer[0]],weights.x),mul(rest.positions[outer[1]],weights.y)),mul(rest.positions[outer[2]],weights.z));
        for(float depth:{.002f,.005f,.01f}){const auto hit=index.nearest(sub(p,mul(n,depth)));const auto ids=hit.vertices;
          if(std::any_of(ids.begin(),ids.end(),[&](auto k){return std::any_of(outer.begin(),outer.end(),[&](auto v){return k==v||std::binary_search(b.neighbors[v].begin(),b.neighbors[v].end(),k);});}))continue;
          const auto a=rest.positions[ids[0]],c=rest.positions[ids[1]],d=rest.positions[ids[2]],face=normal(sub(c,a),sub(d,a));if(dot(n,face)<.75f)continue;
          const auto w=hit.barycentric,q=add(add(mul(a,w.x),mul(c,w.y)),mul(d,w.z)),gap=sub(p,q);const float separation=dot(gap,face);const auto tangent=sub(gap,mul(face,separation));
          if(separation<.0003f||separation>.006f||dot(tangent,tangent)>.000004f)continue;
          b.layers.push_back({outer,weights,hit});break;
        }
      };
      for(uint32_t v=0;v<rest.positions.size();++v)bind({v,v,v},{1,0,0},normals[v]);
      for(const auto &f:rest.triangles){const auto v=f.vertices;bind(v,{1.f/3,1.f/3,1.f/3},normal(sub(rest.positions[v[1]],rest.positions[v[0]]),sub(rest.positions[v[2]],rest.positions[v[0]])));}
      b.layers_ready=true;
    }
    std::vector<ir::Vec3> cloth_normals(positions.size());
    if(b.cross_base){for(const auto &f:mesh.triangles){const auto a=positions[f.vertices[0]],u=sub(positions[f.vertices[1]],a),v=sub(positions[f.vertices[2]],a);const ir::Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};for(auto k:f.vertices)cloth_normals[k]=add(cloth_normals[k],n);}for(auto &n:cloth_normals){const float length=std::sqrt(dot(n,n));if(length>0)n=mul(n,1/length);}}
    // 胸腹、臀腿可发生厘米级轮廓变化；脚趾和手指等薄部位继续使用紧的反向约束。
    std::vector<uint8_t> large_contour(surfaces[0].positions.size());
    if(b.cross_base)for(const auto &face:surfaces[0].triangles)if(face.polygon_group<surfaces[0].polygon_groups.size()) {
      auto group=surfaces[0].polygon_groups[face.polygon_group];for(auto &c:group)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
      if(group.find("chest")!=std::string::npos||group.find("pectoral")!=std::string::npos||group.find("abdomen")!=std::string::npos||group=="hip"||group=="pelvis"||group.find("thigh")!=std::string::npos)for(auto v:face.vertices)large_contour[v]=1;
    }
    if(!b.reference_source.empty()&&b.reference_radii.empty()){
      auto rest=scene_.meshes[source.mesh];rest.positions=b.reference_source;SurfaceIndex rest_index(rest);b.reference_radii.resize(b.reference_follower.size());
      batch(b.reference_radii.size(),[&](size_t v){const auto p=b.reference_relative.point(b.reference_follower[v]);const auto hit=rest_index.nearest(p);const auto w=hit.barycentric;const auto q=add(add(mul(rest.positions[hit.vertices[0]],w.x),mul(rest.positions[hit.vertices[1]],w.y)),mul(rest.positions[hit.vertices[2]],w.z));const auto gap=sub(p,q);b.reference_radii[v]=std::sqrt(dot(gap,gap))+(b.cross_base?.04f:.02f);});
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
          hit=indexes[surface]->nearest(p,&b.reference_source,reference,radius,std::numeric_limits<float>::max(),parallel?cache.triangle:SIZE_MAX);
        }else hit=indexes[surface]->nearest(p,nullptr,{},0,std::numeric_limits<float>::max(),parallel?cache.triangle:SIZE_MAX);
        cache={p,hit.barycentric,hit.triangle};
      }return hit;
    };
    // ERC 总体缩放会进入蒙皮顶点；固定毫米间隙会把微型角色的衣物推裂。
    // 用边长比例保留尺寸一致性，旋转和平移不影响该比例。
    const double current_edges=edge_measure(scene_.meshes[source.mesh]);
    const float shape_scale=b.reference_edges>0?float(std::sqrt(std::max(0.,current_edges)/b.reference_edges)):1.f;
    // 跨体型衣物在细分后仍需留出布料间隙，不能仅在基础三角面上相切。
    const float clearance=(b.cross_base?.0025f:.0005f)*shape_scale;
    auto exterior_correction=[&](const SurfaceIndex &index,const ir::Mesh &body,ir::Vec3 p,ir::Vec3 outward,float limit,size_t *hint=nullptr){
      const float radius=limit+1e-5f*std::max(1.f,shape_scale);const auto hit=index.nearest(p,nullptr,{},0,parallel?radius*radius:std::numeric_limits<float>::max(),parallel&&hint?*hint:SIZE_MAX);if(hint)*hint=hit.triangle;if(hit.triangle==SIZE_MAX)return ir::Vec3{};const auto v=hit.vertices;const auto w=hit.barycentric;
      const auto q=add(add(mul(body.positions[v[0]],w.x),mul(body.positions[v[1]],w.y)),mul(body.positions[v[2]],w.z));
      const auto n=normal(sub(body.positions[v[1]],body.positions[v[0]]),sub(body.positions[v[2]],body.positions[v[0]]));
      if(dot(sub(p,q),n)>=-1e-6f)return ir::Vec3{};
      auto outside=index.exterior(p,n,clearance);
      if(!outside)return ir::Vec3{};
      auto try_direction=[&](ir::Vec3 direction){if(auto candidate=index.exterior(p,direction,clearance))if(!outside||dot(sub(*candidate,p),sub(*candidate,p))<dot(sub(*outside,p),sub(*outside,p)))outside=candidate;};
      try_direction(outward);
      const float nearby=2*(std::sqrt(dot(sub(p,q),sub(p,q)))+clearance);
      if(!outside||dot(sub(*outside,p),sub(*outside,p))>nearby*nearby)for(auto axis:std::array<ir::Vec3,6>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}})try_direction(axis);
      if(outside&&dot(sub(*outside,p),sub(*outside,p))>limit*limit)return ir::Vec3{};
      return outside?sub(*outside,p):ir::Vec3{};
    };
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
      if(b.cross_base){ir::Vec3 outward;
          if(slot<positions.size())outward=cloth_normals[slot];else{const auto face=(slot-positions.size())/4,sample=(slot-positions.size())%4;const auto vertices=mesh.triangles[face].vertices;const auto weights=samples[sample];outward=add(add(mul(cloth_normals[vertices[0]],weights.x),mul(cloth_normals[vertices[1]],weights.y)),mul(cloth_normals[vertices[2]],weights.z));}
          p=add(p,exterior_correction(*indexes[0],surfaces[0],p,outward,.08f*shape_scale));
      }
      return sub(p,input);
    };
    const auto inverse=ir::inverse(relative);
    auto constrain=[&] {diagnostics::Scope phase("garment_constraints");if(constrain_&&constraint_revision){for(auto &p:positions)p=inverse.point(p);constrain_(b.follower,b.input,positions);for(auto &p:positions)p=relative.point(p);}};
    auto cloth=mesh;std::unique_ptr<SurfaceIndex> cloth_index;std::vector<std::vector<SurfaceBinding>> reverse_hits(surfaces.size());for(size_t c=0;c<surfaces.size();++c)reverse_hits[c].resize(surfaces[c].positions.size());
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
      // 串行诊断路径保留完整重建及无界查询，作为优化结果的独立参照。
      cloth.positions=positions;if(parallel&&cloth_index)cloth_index->refit();else cloth_index=std::make_unique<SurfaceIndex>(cloth);
      std::vector<ir::Vec3> reverse_offsets(positions.size());std::vector<uint32_t> reverse_counts(positions.size());
      for(size_t c=0;c<surfaces.size();++c) {
      // 查询与约束使用同一份几何。立即写回会让后续查询的重心坐标失效，
      // 并把袖口外裸露手臂的旧命中点反复拉进服装，产生不断放大的尖角。
      auto &hits=reverse_hits[c];
      // 面内命中点的欧氏距离就是法向深度；超过原有深度上限的点不可能产生修正。
      {diagnostics::Scope phase("reverse_queries");batch(hits.size(),[&](size_t k){const float radius=(c==0&&large_contour[k]?.08f:.02f)*shape_scale+1e-5f*std::max(1.f,shape_scale);hits[k]=cloth_index->nearest(surfaces[c].positions[k],nullptr,{},0,parallel?radius*radius:std::numeric_limits<float>::max(),parallel?hits[k].triangle:SIZE_MAX);});}
      for(size_t k=0;k<surfaces[c].positions.size();++k) {
        const bool broad=c==0&&large_contour[k];
        const auto p=surfaces[c].positions[k];const auto &hit=hits[k];if(hit.triangle==SIZE_MAX)continue;const auto v=hit.vertices;const auto w=hit.barycentric;
        if(std::min({w.x,w.y,w.z})<.01f) continue; // 不把裸露区域牵到领口或袖口边缘。
        if(c==0&&!b.reference_source.empty()) {
          // 放下手臂后，裸露的手臂会靠近衣身侧面，不能把它误当成衣身下的凸起。
          // 反向约束只补充静止绑定时已由该布面覆盖的皮肤。
          const auto a=b.reference_relative.point(b.reference_follower[v[0]]),u=b.reference_relative.point(b.reference_follower[v[1]]),z=b.reference_relative.point(b.reference_follower[v[2]]);
          const auto rest=b.reference_source[k],weights=closest_barycentric(rest,a,u,z);
          if(!broad&&std::min({weights.x,weights.y,weights.z})<.01f)continue;
          const auto gap=sub(rest,add(add(mul(a,weights.x),mul(u,weights.y)),mul(z,weights.z)));
          const float rest_radius=broad?.04f:.02f;
          if(dot(gap,gap)>rest_radius*rest_radius)continue;
        }
        const auto q=add(add(mul(cloth.positions[v[0]],w.x),mul(cloth.positions[v[1]],w.y)),mul(cloth.positions[v[2]],w.z));
        const auto n=normal(sub(cloth.positions[v[1]],cloth.positions[v[0]]),sub(cloth.positions[v[2]],cloth.positions[v[0]]));
        const float depth=dot(sub(p,q),n);
        if(depth<=0||depth>(broad?.08f:.02f)*shape_scale||dot(n,normals[c][k])<.5f) continue;
        const auto d=mul(n,depth+clearance);const float sum=w.x*w.x+w.y*w.y+w.z*w.z;
        const float weights[]={w.x,w.y,w.z};for(int j=0;j<3;++j){reverse_offsets[v[j]]=add(reverse_offsets[v[j]],mul(d,weights[j]/sum));++reverse_counts[v[j]];}
      }}
      for(size_t v=0;v<positions.size();++v)if(reverse_counts[v])positions[v]=add(positions[v],mul(reverse_offsets[v],1.f/float(reverse_counts[v])));
      // 碰撞与保形交替投影；不能让碰撞把已经关闭形变的薄鞋底重新顶出尖角。
      constrain();
    }
    {diagnostics::Scope phase("vertices");batch(positions.size(),[&](size_t v){positions[v]=add(positions[v],correction(positions[v],v));});}
    // Catmull-Clark 会将已避让的控制笼重新拉入身体；在一级细分表面求接触，
    // 再回传控制笼，保持原始拓扑、UV 和用户的显示细分等级。
    if(b.cross_base&&b.follower_subdivision.active()){
      diagnostics::Scope phase("subdivision_contact");
      auto body=surfaces[0];if(b.source_subdivision.active()){body.positions=b.source_subdivision.evaluate(body.positions);body.triangles=b.source_subdivision.triangles();}
      SurfaceIndex index(body);const auto outward=b.follower_subdivision.evaluate(cloth_normals);
      std::vector<size_t> hints(outward.size(),SIZE_MAX);
      const auto coarse=positions;const float limit=6*clearance;
      for(int iteration=0;iteration<12;++iteration){const auto refined=b.follower_subdivision.evaluate(positions);std::vector<ir::Vec3> offsets(refined.size());
        batch(refined.size(),[&](size_t v){offsets[v]=exterior_correction(index,body,refined[v],outward[v],limit,&hints[v]);});
        if(std::all_of(offsets.begin(),offsets.end(),[](auto p){return p==ir::Vec3{};}))break;
        const auto cage=b.follower_subdivision.cage_offsets(offsets);for(size_t v=0;v<positions.size();++v){auto change=sub(add(positions[v],cage[v]),coarse[v]);const float length=std::sqrt(dot(change,change));if(length>limit)change=mul(change,limit/length);positions[v]=add(coarse[v],change);}constrain();
      }
    }
    // 自接触处相邻点可能选择不同出口。约束碰撞位移的空间梯度，避免把
    // 毫米级网格边拉成袖口尖角；不平滑衣物本身的褶皱或已有体型形变。
    if(b.cross_base)for(int iteration=0;iteration<12;++iteration){auto filtered=positions;
      for(size_t v=0;v<positions.size();++v){ir::Vec3 change;size_t count=0;const auto offset=sub(positions[v],initial[v]);
        for(auto n:b.neighbors[v]){const auto edge=sub(initial[n],initial[v]),gap=sub(sub(positions[n],initial[n]),offset);const float length=std::sqrt(dot(gap,gap)),limit=std::max(2*std::sqrt(dot(edge,edge)),clearance);
          if(length>limit){change=add(change,mul(gap,.5f*(1-limit/length)));++count;}}
        if(count)filtered[v]=add(positions[v],mul(change,1.f/float(count)));
      }positions.swap(filtered);
    }
    constrain();
    // 身体避让可能把内层布面顶穿外层口袋。静止时绑定邻近的同向布层，
    // 每次只恢复本次蒙皮输入的层间距离，不缓存姿势，也不把碰撞输出累积回输入。
    if(!b.layers.empty()){diagnostics::Scope phase("cloth_layers");
      auto point=[](const std::vector<ir::Vec3> &p,const SurfaceBinding &s){const auto w=s.barycentric;return add(add(mul(p[s.vertices[0]],w.x),mul(p[s.vertices[1]],w.y)),mul(p[s.vertices[2]],w.z));};
      for(int pass=0;pass<8;++pass)for(const auto &layer:b.layers){const auto &s=layer.inner;const auto ids=s.vertices;const SurfaceBinding outer{layer.vertices,layer.barycentric};
        const auto original=normal(sub(initial[ids[1]],initial[ids[0]]),sub(initial[ids[2]],initial[ids[0]]));const float gap=dot(sub(point(initial,outer),point(initial,s)),original);if(gap<=0)continue;
        const auto n=normal(sub(positions[ids[1]],positions[ids[0]]),sub(positions[ids[2]],positions[ids[0]]));const float separation=dot(sub(point(positions,outer),point(positions,s)),n);
        if(separation<gap){const auto w=layer.barycentric;const float weights[]={w.x,w.y,w.z},sum=dot(w,w);for(size_t k=0;k<3;++k)if(weights[k]>0){auto &p=positions[layer.vertices[k]];p=add(p,mul(n,(gap-separation)*weights[k]/sum));}}
      }
    }
    uint64_t corrected=0;
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
