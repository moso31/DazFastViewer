#include "runtime/picking.h"
#include "runtime/morph.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>

namespace dfv::runtime {
namespace {
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
struct RayVec {
  double x,y,z;
  RayVec(ir::Vec3 v):x(v.x),y(v.y),z(v.z){}
  RayVec(double a,double b,double c):x(a),y(b),z(c){}
  RayVec operator-(RayVec b)const{return {x-b.x,y-b.y,z-b.z};}
};
RayVec cross3(RayVec a,RayVec b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(RayVec a,RayVec b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
bool box(const ir::Bounds &b,RayVec o,RayVec d,double maximum) {
  double lo=0,hi=maximum;
  const double origin[]={o.x,o.y,o.z},dir[]={d.x,d.y,d.z},minimum[]={b.minimum.x,b.minimum.y,b.minimum.z},maxima[]={b.maximum.x,b.maximum.y,b.maximum.z};
  for(int i=0;i<3;++i) {
    if(dir[i]==0) {if(origin[i]<minimum[i]||origin[i]>maxima[i]) return false;continue;}
    double a=(minimum[i]-origin[i])/dir[i],c=(maxima[i]-origin[i])/dir[i];if(a>c) std::swap(a,c);lo=std::max(lo,a);hi=std::min(hi,c);if(lo>hi) return false;
  }return true;
}
std::array<double,9> inverse_linear(const ir::Transform &transform) {
  const auto &m=transform.value;
  const double a=m[0],b=m[1],c=m[2],d=m[4],e=m[5],f=m[6],g=m[8],h=m[9],j=m[10];
  const double determinant=a*(e*j-f*h)-b*(d*j-f*g)+c*(d*h-e*g);
  const double volume=std::hypot(a,d,g)*std::hypot(b,e,h)*std::hypot(c,f,j);
  if(!std::isfinite(determinant)||!std::isfinite(volume)||volume==0||std::abs(determinant)<=1e-14*volume)throw std::runtime_error("变换矩阵不可逆");
  return {(e*j-f*h)/determinant,(c*h-b*j)/determinant,(b*f-c*e)/determinant,
          (f*g-d*j)/determinant,(a*j-c*g)/determinant,(c*d-a*f)/determinant,
          (d*h-e*g)/determinant,(b*g-a*h)/determinant,(a*e-b*d)/determinant};
}
ir::Bounds world_bounds(const ir::Bounds &local,const ir::Transform &transform) {
  if(local.empty)return {};
  const double minimum[]={local.minimum.x,local.minimum.y,local.minimum.z},maximum[]={local.maximum.x,local.maximum.y,local.maximum.z};
  float lower[3],upper[3];const auto &m=transform.value;
  for(int row=0;row<3;++row){
    double lo=m[row*4+3],hi=lo,magnitude=std::abs(lo);
    for(int axis=0;axis<3;++axis){const double a=double(m[row*4+axis])*minimum[axis],b=double(m[row*4+axis])*maximum[axis];lo+=std::min(a,b);hi+=std::max(a,b);magnitude+=std::max(std::abs(a),std::abs(b));}
    // 世界包围盒只做粗筛；大平移相消时仍应包含真实局部边界。
    const double pad=32*std::numeric_limits<double>::epsilon()*magnitude;
    lower[row]=std::nextafter(float(lo-pad),-std::numeric_limits<float>::infinity());upper[row]=std::nextafter(float(hi+pad),std::numeric_limits<float>::infinity());
  }
  ir::Bounds result;result.add({lower[0],lower[1],lower[2]});result.add({upper[0],upper[1],upper[2]});return result;
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
  for(size_t i=0;i<scene.instances.size();++i) {const auto &v=scene.instances[i];const bool volume=std::any_of(v.materials.begin(),v.materials.end(),[&](auto m){return scene.materials.at(m).cloud.has_value();});instances_[i].pickable=!volume&&(pickable.empty()||pickable[i]);if(instances_[i].pickable) used_meshes_[scene.instances[i].mesh]=1;}
  for(uint32_t m=0;m<meshes_.size();++m) if(used_meshes_[m]) mesh(scene,m);
  for(uint32_t i=0;i<instances_.size();++i) instance(scene,i);
}
void PickingScene::mesh(const ir::Scene &scene,uint32_t index) {
  auto &tree=meshes_.at(index);tree.faces.clear();tree.branches.clear();const auto &mesh=scene.meshes.at(index);
  tree.faces.reserve(mesh.triangles.size());
  for(size_t t=0;t<mesh.triangles.size();++t) {if(!mesh.draws(mesh.triangles[t])) continue;const auto &v=mesh.triangles[t].vertices;tree.faces.push_back({mesh.positions[v[0]],mesh.positions[v[1]],mesh.positions[v[2]],-1,int(t)});}
  if(!tree.faces.empty()) build(tree,0,int(tree.faces.size()));++stats_.mesh_builds;
}
void PickingScene::set_pickable(const ir::Scene &scene,const std::vector<uint8_t> &pickable) {
  if(pickable.size()!=instances_.size())throw std::runtime_error("射线对象掩码数量不一致");
  for(uint32_t i=0;i<instances_.size();++i){const auto &source=scene.instances.at(i);const bool volume=std::any_of(source.materials.begin(),source.materials.end(),[&](auto m){return scene.materials.at(m).cloud.has_value();});
    const bool enabled=pickable[i]&&!volume;auto &item=instances_[i];if(item.pickable==enabled)continue;item.pickable=enabled;
    if(enabled&&!used_meshes_.at(source.mesh)){used_meshes_[source.mesh]=1;mesh(scene,source.mesh);}instance(scene,i);
  }
}
void PickingScene::instance(const ir::Scene &scene,uint32_t index) {
  const auto &source=scene.instances.at(index);auto &out=instances_.at(index);out.mesh=source.mesh;out.visible=source.visible;out.bounds={};
  if(!out.pickable) return;
  out.inverse=inverse_linear(source.transform);const auto &m=source.transform.value;out.translation={m[3],m[7],m[11]};const auto &tree=meshes_.at(out.mesh);
  if(!tree.branches.empty()) out.bounds=world_bounds(tree.branches[0].bounds,source.transform);
  ++stats_.instance_updates;
}
void PickingScene::apply(const ir::Scene &scene,const ir::Delta &delta) {
  std::vector<uint8_t> dirty(instances_.size());
  for(const auto &e:delta.masks)if(used_meshes_.at(e.index)) {mesh(scene,e.index);for(size_t i=0;i<instances_.size();++i)if(instances_[i].mesh==e.index)dirty[i]=1;}
  for(const auto &e:delta.meshes) if(used_meshes_.at(e.index)) {mesh(scene,e.index);for(size_t i=0;i<instances_.size();++i) if(instances_[i].mesh==e.index) dirty[i]=1;}
  for(const auto &e:delta.instances) dirty.at(e.index)=1;
  for(const auto &e:delta.visibility) dirty.at(e.index)=1;
  for(uint32_t i=0;i<dirty.size();++i) if(dirty[i]) instance(scene,i);
}
void PickingScene::prepare_delta(const ir::Scene &scene,const ir::Delta &delta,const std::vector<uint8_t> &pickable) {
  meshes_.resize(scene.meshes.size());instances_.resize(scene.instances.size());used_meshes_.assign(scene.meshes.size(),0);std::vector<uint8_t> dirty(instances_.size());
  for(size_t i=0;i<instances_.size();++i){const auto &v=scene.instances[i];const bool volume=std::any_of(v.materials.begin(),v.materials.end(),[&](auto m){return scene.materials.at(m).cloud.has_value();});instances_[i].pickable=!volume&&(pickable.empty()||pickable.at(i));if(instances_[i].pickable)used_meshes_[scene.instances[i].mesh]=1;}
  for(const auto &e:delta.meshes){if(used_meshes_[e.index])mesh(scene,e.index);for(size_t i=0;i<instances_.size();++i)if(scene.instances[i].mesh==e.index)dirty[i]=1;}
  for(const auto &e:delta.instances)dirty[e.index]=1;
  for(uint32_t i=0;i<dirty.size();++i)if(dirty[i]){
    instance(scene,i);auto &out=instances_[i];if(out.pickable&&out.bounds.empty){ir::Bounds local;for(auto v:scene.meshes[out.mesh].positions)local.add(v);out.bounds=world_bounds(local,scene.instances[i].transform);}
  }
}
void PickingScene::swap_delta(PickingScene &p,const ir::Scene &scene,const ir::Delta &delta) {
  std::vector<uint8_t> dirty(instances_.size());
  for(const auto &e:delta.meshes){std::swap(meshes_[e.index],p.meshes_[e.index]);for(size_t i=0;i<instances_.size();++i)if(scene.instances[i].mesh==e.index)dirty[i]=1;}
  for(const auto &e:delta.instances)dirty[e.index]=1;
  for(size_t i=0;i<dirty.size();++i)if(dirty[i])std::swap(instances_[i],p.instances_[i]);
  stats_.mesh_builds+=p.stats_.mesh_builds;stats_.instance_updates+=p.stats_.instance_updates;
}
PickHit PickingScene::ray(ir::Vec3 origin,ir::Vec3 direction) const {
  PickHit result;double nearest=result.distance;
  if(!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z)||
     !std::isfinite(direction.x)||!std::isfinite(direction.y)||!std::isfinite(direction.z)||
     (direction.x==0&&direction.y==0&&direction.z==0))return result;
  for(size_t instance=0;instance<instances_.size();++instance) {
  const auto &object=instances_[instance];if(!object.visible||!object.pickable||object.bounds.empty||!box(object.bounds,origin,direction,nearest)) continue;
  const auto &tree=meshes_[object.mesh];const auto &branches_=tree.branches;const auto &faces_=tree.faces;
  // 先用 double 消去世界平移，再变换到局部；方向保留世界射线参数语义。
  const auto &m=object.inverse;const auto offset=RayVec(origin)-RayVec(object.translation);
  const auto transform=[&](RayVec v){return RayVec{m[0]*v.x+m[1]*v.y+m[2]*v.z,m[3]*v.x+m[4]*v.y+m[5]*v.z,m[6]*v.x+m[7]*v.y+m[8]*v.z};};
  const auto o=transform(offset),d=transform(direction);
  std::vector<int> stack{0};
  while(!stack.empty()) {const auto &b=branches_[stack.back()];stack.pop_back();if(!box(b.bounds,o,d,nearest)) continue;
    if(b.left>=0) {stack.push_back(b.left);stack.push_back(b.right);continue;}
    for(int i=b.begin;i<b.end;++i) {const auto &f=faces_[i];const auto e1=RayVec(f.b)-RayVec(f.a),e2=RayVec(f.c)-RayVec(f.a),p=cross3(d,e2);const double determinant=dot(e1,p);
      // 阈值随边长和方向大小变化；巨型实例的局部方向很短，不能用固定 1e-10 拒绝。
      const double magnitude=std::abs(e1.x)*(std::abs(d.y*e2.z)+std::abs(d.z*e2.y))+std::abs(e1.y)*(std::abs(d.z*e2.x)+std::abs(d.x*e2.z))+std::abs(e1.z)*(std::abs(d.x*e2.y)+std::abs(d.y*e2.x));
      if(std::abs(determinant)<=8*std::numeric_limits<double>::epsilon()*magnitude)continue;
      const auto s=o-RayVec(f.a);const double u=dot(s,p)/determinant;if(u<0||u>1) continue;const auto q=cross3(s,e1);const double v=dot(d,q)/determinant;if(v<0||u+v>1) continue;
      const double distance=dot(e2,q)/determinant;if(distance>1e-5&&distance<nearest){nearest=distance;result={int(instance),f.triangle,float(distance)};}
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
bool JointRegions::within(int joint,int ancestor) const {
  if(ancestor<0) return false;
  for(size_t depth=0;joint>=0&&depth<=parents.size();++depth) {
    if(joint==ancestor) return true;
    if(size_t(joint)>=parents.size()) break;
    joint=parents[size_t(joint)];
  }return false;
}
bool JointRegions::within_head(int joint) const {return within(joint,head);}
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
    // 多边形组保留二级区域边界；组内蒙皮权重定位三级子部位，覆盖脚趾等共享组。
    int body=group>=0?group:detail;if(result.within_head(body)) body=result.head;
    if(body>=0) {
      detail=body;std::map<uint32_t,double> weights;
      for(auto v:mesh.triangles[t].vertices) if(v<skin.weights.size()) for(const auto &w:skin.weights[v])
        if(result.within(int(w.joint),body)&&(body==result.head||!result.within_head(int(w.joint)))) weights[w.joint]+=w.weight;
      double maximum=0;for(const auto &[j,w]:weights) if(w>maximum) {maximum=w;detail=int(j);}
    }
    result.detail.push_back(detail);result.body.push_back(body);
  }return result;
}
ir::Bounds joint_region_bounds(const ir::Mesh &mesh,const ir::Transform &world,const JointRegions &regions,int joint,const Skin *skin) {
  ir::Bounds result;if(joint<0) return result;
  auto add=[&](const ir::Triangle &face) {for(auto v:face.vertices) result.add(world.point(mesh.positions.at(v)));};
  for(size_t t=0;t<mesh.triangles.size()&&t<regions.detail.size();++t)
    if(mesh.draws(mesh.triangles[t])&&regions.within(regions.detail[t],joint)) add(mesh.triangles[t]);
  // 某些控制骨骼有蒙皮影响，但没有成为任何面的最大权重；树选取仍按受影响的面聚焦。
  if(result.empty&&skin) for(size_t t=0;t<mesh.triangles.size()&&t<regions.body.size();++t) {
    const auto &face=mesh.triangles[t];if(!mesh.draws(face)||!regions.within(joint,regions.body[t])||(regions.within_head(joint)&&regions.body[t]!=regions.head)) continue;
    bool influenced=false;for(auto v:face.vertices) if(v<skin->weights.size()) for(const auto &w:skin->weights[v]) influenced|=w.weight>0&&regions.within(int(w.joint),joint);
    if(influenced) add(face);
  }
  return result;
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
  const auto t=size_t(hit.triangle);const int joint=parts.within(selected_joint,parts.body[t])?parts.detail[t]:parts.body[t];
  return joint<0?HoverRegion{}:HoverRegion{hit.instance,joint};
}
HoverRegion selection_region(const PickHit &hit,HoverRegion active,std::span<const HoverRegion> selections,const std::vector<JointRegions> &regions,bool toggle) {
  if(!toggle) return hover_region(hit,active.instance,active.joint,regions);
  if(hit.instance<0||size_t(hit.instance)>=regions.size()) return {};
  // Ctrl 沿用命中角色已有的骨骼选择层级；活动项可能属于另一个角色。
  // 命中组的细选上下文与集合顺序无关，也不能从另一只脚或其他角色借用。
  int joint=-1;
  const auto &parts=regions[size_t(hit.instance)];
  const int body=hit.triangle>=0&&size_t(hit.triangle)<parts.body.size()?parts.body[size_t(hit.triangle)]:-1;
  for(const auto &selected:selections) if(selected.instance==hit.instance&&selected.joint>=0) {
    joint=selected.joint;
    if(parts.within(joint,body)) break;
  }
  return hover_region(hit,joint>=0?hit.instance:-1,joint,regions);
}
}
