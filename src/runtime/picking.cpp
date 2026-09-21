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
int PickingScene::build(int begin,int end) {
  Branch b;b.begin=begin;b.end=end;
  for(int i=begin;i<end;++i) {b.bounds.add(faces_[i].a);b.bounds.add(faces_[i].b);b.bounds.add(faces_[i].c);}
  const auto index=int(branches_.size());branches_.push_back(b);
  if(end-begin>12) {
    auto e=sub(b.bounds.maximum,b.bounds.minimum);int axis=e.x>e.y?(e.x>e.z?0:2):(e.y>e.z?1:2);
    auto center=[axis](const Face &f) {return axis==0?f.a.x+f.b.x+f.c.x:axis==1?f.a.y+f.b.y+f.c.y:f.a.z+f.b.z+f.c.z;};
    const auto middle=(begin+end)/2;std::nth_element(faces_.begin()+begin,faces_.begin()+middle,faces_.begin()+end,[&](const Face &a,const Face &c) {return center(a)<center(c);});
    const auto left=build(begin,middle),right=build(middle,end);branches_[index].left=left;branches_[index].right=right;
  }return index;
}
void PickingScene::update(const ir::Scene &scene,const std::vector<uint8_t> &pickable) {
  if(!pickable.empty()&&pickable.size()!=scene.instances.size()) throw std::runtime_error("射线对象掩码数量不一致");
  faces_.clear();branches_.clear();
  for(size_t i=0;i<scene.instances.size();++i) {if(!pickable.empty()&&!pickable[i]) continue;const auto &instance=scene.instances[i];const auto &mesh=scene.meshes[instance.mesh];
    for(size_t t=0;t<mesh.triangles.size();++t) {const auto &v=mesh.triangles[t].vertices;faces_.push_back({instance.transform.point(mesh.positions[v[0]]),instance.transform.point(mesh.positions[v[1]]),instance.transform.point(mesh.positions[v[2]]),int(i),int(t)});}}
  if(!faces_.empty()) build(0,int(faces_.size()));
}
PickHit PickingScene::ray(ir::Vec3 origin,ir::Vec3 direction) const {
  PickHit result;if(branches_.empty()) return result;std::vector<int> stack{0};
  while(!stack.empty()) {const auto &b=branches_[stack.back()];stack.pop_back();if(!box(b.bounds,origin,direction,result.distance)) continue;
    if(b.left>=0) {stack.push_back(b.left);stack.push_back(b.right);continue;}
    for(int i=b.begin;i<b.end;++i) {const auto &f=faces_[i];const auto e1=sub(f.b,f.a),e2=sub(f.c,f.a),p=cross3(direction,e2);const float determinant=dot(e1,p);if(std::abs(determinant)<1e-10f) continue;
      const auto s=sub(origin,f.a);float u=dot(s,p)/determinant;if(u<0||u>1) continue;const auto q=cross3(s,e1);const float v=dot(direction,q)/determinant;if(v<0||u+v>1) continue;
      const float distance=dot(e2,q)/determinant;if(distance>1e-5f&&distance<result.distance) result={f.instance,f.triangle,distance};
    }
  }return result;
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
std::vector<uint8_t> viewport_pick_mask(size_t instances,const std::vector<Target> &targets) {
  std::vector<uint8_t> result(instances,1);
  for(const auto &target:targets) if(!target.conform_target.empty()) result.at(target.instance)=0;
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
}
