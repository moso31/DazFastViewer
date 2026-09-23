#include "runtime/picking.h"
#include "runtime/morph.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>

namespace dfv::runtime {
namespace {
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
ir::Vec3 cross3(ir::Vec3 a,ir::Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(ir::Vec3 a,ir::Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
bool box(const ir::Bounds &b,ir::Vec3 o,ir::Vec3 d,float maximum) {
  float lo=0,hi=maximum;
  const float origin[]={o.x,o.y,o.z},dir[]={d.x,d.y,d.z},minimum[]={b.minimum.x,b.minimum.y,b.minimum.z},maxima[]={b.maximum.x,b.maximum.y,b.maximum.z};
  for(int i=0;i<3;++i) {
    if(std::abs(dir[i])<1e-12f) {if(origin[i]<minimum[i]||origin[i]>maxima[i]) return false;continue;}
    float a=(minimum[i]-origin[i])/dir[i],c=(maxima[i]-origin[i])/dir[i];if(a>c) std::swap(a,c);lo=std::max(lo,a);hi=std::min(hi,c);if(lo>hi) return false;
  }return true;
}
}
int PickingScene::build(MeshTree &mesh,int begin,int end) {
  auto &faces_=mesh.faces;auto &branches_=mesh.branches;
  Branch b;b.begin=begin;b.end=end;
  for(int i=begin;i<end;++i) {b.bounds.add(faces_[i].a);b.bounds.add(faces_[i].b);b.bounds.add(faces_[i].c);}
  const auto index=int(branches_.size());branches_.push_back(b);
  if(end-begin>12) {
    auto e=sub(b.bounds.maximum,b.bounds.minimum);int axis=e.x>e.y?(e.x>e.z?0:2):(e.y>e.z?1:2);
    auto center=[axis](const Face &f) {return axis==0?f.a.x+f.b.x+f.c.x:axis==1?f.a.y+f.b.y+f.c.y:f.a.z+f.b.z+f.c.z;};
    const auto middle=(begin+end)/2;std::nth_element(faces_.begin()+begin,faces_.begin()+middle,faces_.begin()+end,[&](const Face &a,const Face &c) {return center(a)<center(c);});
    const auto left=build(mesh,begin,middle),right=build(mesh,middle,end);branches_[index].left=left;branches_[index].right=right;
  }return index;
}
void PickingScene::update(const ir::Scene &scene,const std::vector<uint8_t> &pickable) {
  if(!pickable.empty()&&pickable.size()!=scene.instances.size()) throw std::runtime_error("射线对象掩码数量不一致");
  meshes_.clear();meshes_.resize(scene.meshes.size());instances_.resize(scene.instances.size());used_meshes_.assign(scene.meshes.size(),0);
  for(size_t i=0;i<scene.instances.size();++i) {instances_[i].pickable=pickable.empty()||pickable[i];if(instances_[i].pickable) used_meshes_[scene.instances[i].mesh]=1;}
  for(uint32_t m=0;m<meshes_.size();++m) if(used_meshes_[m]) mesh(scene,m);
  for(uint32_t i=0;i<instances_.size();++i) instance(scene,i);
}
void PickingScene::mesh(const ir::Scene &scene,uint32_t index) {
  auto &tree=meshes_.at(index);tree.faces.clear();tree.branches.clear();const auto &mesh=scene.meshes.at(index);
  tree.faces.reserve(mesh.triangles.size());
  for(size_t t=0;t<mesh.triangles.size();++t) {if(!mesh.draws(mesh.triangles[t])) continue;const auto &v=mesh.triangles[t].vertices;tree.faces.push_back({mesh.positions[v[0]],mesh.positions[v[1]],mesh.positions[v[2]],-1,int(t)});}
  if(!tree.faces.empty()) build(tree,0,int(tree.faces.size()));++stats_.mesh_builds;
}
void PickingScene::instance(const ir::Scene &scene,uint32_t index) {
  const auto &source=scene.instances.at(index);auto &out=instances_.at(index);out.mesh=source.mesh;out.visible=source.visible;out.bounds={};
  if(!out.pickable) return;
  out.inverse=ir::inverse(source.transform);const auto &tree=meshes_.at(out.mesh);
  if(!tree.branches.empty()) {const auto &b=tree.branches[0].bounds;
    for(int c=0;c<8;++c) out.bounds.add(source.transform.point({c&1?b.maximum.x:b.minimum.x,c&2?b.maximum.y:b.minimum.y,c&4?b.maximum.z:b.minimum.z}));}
  ++stats_.instance_updates;
}
void PickingScene::apply(const ir::Scene &scene,const ir::Delta &delta) {
  std::vector<uint8_t> dirty(instances_.size());
  for(const auto &e:delta.meshes) if(used_meshes_.at(e.index)) {mesh(scene,e.index);for(size_t i=0;i<instances_.size();++i) if(instances_[i].mesh==e.index) dirty[i]=1;}
  for(const auto &e:delta.instances) dirty.at(e.index)=1;
  for(const auto &e:delta.visibility) dirty.at(e.index)=1;
  for(uint32_t i=0;i<dirty.size();++i) if(dirty[i]) instance(scene,i);
}
PickHit PickingScene::ray(ir::Vec3 origin,ir::Vec3 direction) const {
  PickHit result;
  for(size_t instance=0;instance<instances_.size();++instance) {
  const auto &object=instances_[instance];if(!object.visible||!object.pickable||object.bounds.empty||!box(object.bounds,origin,direction,result.distance)) continue;
  const auto &tree=meshes_[object.mesh];const auto &branches_=tree.branches;const auto &faces_=tree.faces;
  // 方向不归一化，保持射线参数在非均匀缩放后的世界距离语义。
  const auto o=object.inverse.point(origin);const auto &m=object.inverse.value;
  const ir::Vec3 d{m[0]*direction.x+m[1]*direction.y+m[2]*direction.z,m[4]*direction.x+m[5]*direction.y+m[6]*direction.z,m[8]*direction.x+m[9]*direction.y+m[10]*direction.z};
  std::vector<int> stack{0};
  while(!stack.empty()) {const auto &b=branches_[stack.back()];stack.pop_back();if(!box(b.bounds,o,d,result.distance)) continue;
    if(b.left>=0) {stack.push_back(b.left);stack.push_back(b.right);continue;}
    for(int i=b.begin;i<b.end;++i) {const auto &f=faces_[i];const auto e1=sub(f.b,f.a),e2=sub(f.c,f.a),p=cross3(d,e2);const float determinant=dot(e1,p);if(std::abs(determinant)<1e-10f) continue;
      const auto s=sub(o,f.a);float u=dot(s,p)/determinant;if(u<0||u>1) continue;const auto q=cross3(s,e1);const float v=dot(d,q)/determinant;if(v<0||u+v>1) continue;
      const float distance=dot(e2,q)/determinant;if(distance>1e-5f&&distance<result.distance) result={int(instance),f.triangle,distance};
    }
  }}return result;
}
PickHit PickingScene::screen(const CameraState &camera,int x,int y,int width,int height) const {
  if(x<0||y<0||x>=width||y>=height||width<1||height<1) return {};
  const auto m=camera.matrix();const float extent=std::tan(.8f*.5f),sx=(2*(x+.5f)/width-1)*extent*std::max(1.f,float(width)/height),sy=(1-2*(y+.5f)/height)*extent*std::max(1.f,float(height)/width);
  const auto direction=normalized({m[0]*sx+m[1]*sy+m[2],m[4]*sx+m[5]*sy+m[6],m[8]*sx+m[9]*sy+m[10]});
  return ray({m[3],m[7],m[11]},{direction.x,direction.y,direction.z});
}
int hit_joint(const ir::Mesh &mesh,int triangle,const Skin &skin) {
  if(triangle<0||size_t(triangle)>=mesh.triangles.size()) return -1;
  const auto &face=mesh.triangles[size_t(triangle)];
  if(face.polygon_group<mesh.polygon_groups.size()) for(size_t j=0;j<skin.joints.size();++j) {
    const auto &joint=skin.joints[j];const auto &group=mesh.polygon_groups[face.polygon_group];
    if(joint.id==group||joint.name==group||std::find(joint.aliases.begin(),joint.aliases.end(),group)!=joint.aliases.end()) return int(j);
  }
  std::map<uint32_t,double> weights;for(auto v:face.vertices) if(v<skin.weights.size()) for(const auto &w:skin.weights[v]) weights[w.joint]+=w.weight;
  int result=-1;double maximum=0;for(auto [j,w]:weights) if(w>maximum) {maximum=w;result=int(j);}return result;
}
bool parameter_on_node(const std::string &owner,const std::string &group,const std::string &node) {
  if(node.empty()||owner==node) return true;
  // DAZ 有些头部控制器挂在 Figure 上，用原始区域分组补充，而非按显示名猜测。
  return node=="head"&&(group.starts_with("/Actor/Head/")||group.starts_with("/Pose Controls/Head/")||group=="/Actor/Head"||group=="/Pose Controls/Head");
}
bool JointRegions::within_head(int joint) const {
  if(head<0) return false;
  for(size_t depth=0;joint>=0&&size_t(joint)<parents.size()&&depth<parents.size();++depth) {if(joint==head) return true;joint=parents[size_t(joint)];}return false;
}
JointRegions joint_regions(const ir::Mesh &mesh,const Skin &skin) {
  JointRegions result;result.detail.reserve(mesh.triangles.size());result.body.reserve(mesh.triangles.size());
  auto lower=[](std::string s) {for(auto &c:s) c=char(std::tolower(static_cast<unsigned char>(c)));return s;};
  std::map<std::string,int> by_name;
  auto name=[&](const std::string &s,int j) {if(s.empty()) return;auto [it,inserted]=by_name.emplace(lower(s),j);if(!inserted&&it->second!=j) it->second=-1;};
  for(size_t j=0;j<skin.joints.size();++j) {const auto &bone=skin.joints[j];result.parents.push_back(bone.parent);name(bone.id,int(j));name(bone.name,int(j));for(const auto &alias:bone.aliases) name(alias,int(j));}
  if(auto found=by_name.find("head");found!=by_name.end()) result.head=found->second;
  for(size_t t=0;t<mesh.triangles.size();++t) {
    int detail=hit_joint(mesh,int(t),skin),group=-1;const auto p=mesh.triangles[t].polygon_group;
    if(p<mesh.polygon_groups.size()) if(auto found=by_name.find(lower(mesh.polygon_groups[p]));found!=by_name.end()) group=found->second;
    // 明确的多边形组决定头 / 颈边界，混合蒙皮权重不能把脖子划进 Head。
    const bool head=group>=0?result.within_head(group):result.within_head(detail);
    if(!head&&result.within_head(detail)) detail=group;
    if(head) {std::map<uint32_t,double> weights;for(auto v:mesh.triangles[t].vertices) if(v<skin.weights.size()) for(const auto &w:skin.weights[v]) if(result.within_head(int(w.joint))) weights[w.joint]+=w.weight;
      double maximum=0;for(const auto &[j,w]:weights) if(w>maximum) {maximum=w;detail=int(j);}}
    if(head&&!result.within_head(detail)) detail=result.head;
    result.detail.push_back(detail);result.body.push_back(head?result.head:detail);
  }return result;
}
std::vector<uint8_t> viewport_pick_mask(const ir::Scene &scene,const std::vector<Target> &targets) {
  std::vector<uint8_t> result(scene.instances.size(),1);
  for(const auto &target:targets) if(!target.conform_target.empty()&&!scene.meshes.at(scene.instances.at(target.instance).mesh).graft_target_vertices) result.at(target.instance)=0;
  return result;
}
InstanceGroups::InstanceGroups(const ir::Scene &scene):roots(scene.instances.size()),members(scene.instances.size()) {
  std::map<std::string,uint32_t> groups;
  for(uint32_t i=0;i<scene.instances.size();++i) {
    const auto &v=scene.instances[i];uint32_t root=i;
    if(v.prototype>=0&&!v.instance_group.empty()) root=groups.emplace(v.instance_group,i).first->second;
    roots[i]=root;members[root].push_back(i);
  }
}
ir::Bounds InstanceGroups::bounds(const ir::Scene &scene,uint32_t instance) const {
  ir::Bounds result;
  for(auto i:members.at(roots.at(instance))) {const auto &v=scene.instances.at(i);if(!v.visible) continue;
    const auto &mesh=scene.meshes.at(v.mesh);
    for(const auto &t:mesh.triangles) if(mesh.draws(t)) for(auto p:t.vertices) result.add(v.transform.point(mesh.positions.at(p)));
    for(const auto &c:mesh.curves) for(auto p:c.vertices) result.add(v.transform.point(mesh.positions.at(p)));
  }
  return result;
}
HoverRegion hover_region(const PickHit &hit,int selected_instance,int selected_joint,const std::vector<JointRegions> &regions) {
  if(hit.instance<0||size_t(hit.instance)>=regions.size()) return {};
  const auto &parts=regions[size_t(hit.instance)];
  if(hit.instance!=selected_instance||parts.detail.empty()) return {hit.instance,-1};
  if(hit.triangle<0||size_t(hit.triangle)>=parts.detail.size()) return {};
  const auto t=size_t(hit.triangle);const int joint=parts.body[t]==parts.head&&!parts.within_head(selected_joint)?parts.body[t]:parts.detail[t];
  return joint<0?HoverRegion{}:HoverRegion{hit.instance,joint};
}
HoverRegion selection_region(const PickHit &hit,HoverRegion active,std::span<const HoverRegion> selections,const std::vector<JointRegions> &regions,bool toggle) {
  if(!toggle) return hover_region(hit,active.instance,active.joint,regions);
  if(hit.instance<0||size_t(hit.instance)>=regions.size()) return {};
  // Ctrl 沿用命中角色已有的骨骼选择层级；活动项可能属于另一个角色。
  // 只有整体选择时仍切换整个角色，头部细选上下文与多选集合的顺序无关。
  int joint=-1;
  for(const auto &selected:selections) if(selected.instance==hit.instance&&selected.joint>=0) {
    joint=selected.joint;
    if(regions[size_t(hit.instance)].within_head(joint)) break;
  }
  return hover_region(hit,joint>=0?hit.instance:-1,joint,regions);
}
}
