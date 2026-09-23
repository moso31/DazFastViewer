#include "runtime/skeleton.h"
#include "diagnostics/load_profile.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace dfv::runtime {
namespace {
struct V {double x=0,y=0,z=0;};
V operator+(V a,V b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
V operator-(V a,V b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
V operator*(V a,double b) {return {a.x*b,a.y*b,a.z*b};}
V v(ir::Vec3 p) {return {p.x,p.y,p.z};}
struct M {double a[3][3]{{1,0,0},{0,1,0},{0,0,1}};};
M operator*(const M &a,const M &b) {M c;for(int i=0;i<3;++i) for(int j=0;j<3;++j) {c.a[i][j]=0;for(int k=0;k<3;++k) c.a[i][j]+=a.a[i][k]*b.a[k][j];}return c;}
V operator*(const M &m,V p) {return {m.a[0][0]*p.x+m.a[0][1]*p.y+m.a[0][2]*p.z,m.a[1][0]*p.x+m.a[1][1]*p.y+m.a[1][2]*p.z,m.a[2][0]*p.x+m.a[2][1]*p.y+m.a[2][2]*p.z};}
M transpose(const M &m) {M t;for(int i=0;i<3;++i) for(int j=0;j<3;++j) t.a[i][j]=m.a[j][i];return t;}
M rotation(ir::Vec3 degrees,const std::string &order) {
  M result;const double values[]={degrees.x,degrees.y,degrees.z};
  for(char c:order) {const int axis=c-'X',j=(axis+1)%3,k=(axis+2)%3;const double angle=values[axis]*std::numbers::pi/180.;
    M r;r.a[j][j]=r.a[k][k]=std::cos(angle);r.a[j][k]=-std::sin(angle);r.a[k][j]=std::sin(angle);result=r*result;}
  return result;
}
M scaling(const JointPose &p,bool inverse=false) {M m;const double s[]={p.scale.x*p.general_scale,p.scale.y*p.general_scale,p.scale.z*p.general_scale};for(int i=0;i<3;++i) m.a[i][i]=inverse?1/s[i]:s[i];return m;}
struct Q {double w=0,x=0,y=0,z=0;};
Q operator+(Q a,Q b) {return {a.w+b.w,a.x+b.x,a.y+b.y,a.z+b.z};}
Q operator*(Q a,double s) {return {a.w*s,a.x*s,a.y*s,a.z*s};}
Q operator*(Q a,Q b) {return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};}
Q conjugate(Q q) {return {q.w,-q.x,-q.y,-q.z};}
double dot(Q a,Q b) {return a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z;}
Q quaternion(const M &m) {
  Q q;const double trace=m.a[0][0]+m.a[1][1]+m.a[2][2];
  if(trace>0) {const double s=2*std::sqrt(trace+1);q={s/4,(m.a[2][1]-m.a[1][2])/s,(m.a[0][2]-m.a[2][0])/s,(m.a[1][0]-m.a[0][1])/s};}
  else {int i=0;if(m.a[1][1]>m.a[i][i]) i=1;if(m.a[2][2]>m.a[i][i]) i=2;const int j=(i+1)%3,k=(i+2)%3;
    const double s=2*std::sqrt(1+m.a[i][i]-m.a[j][j]-m.a[k][k]);double xyz[3]{};xyz[i]=s/4;xyz[j]=(m.a[i][j]+m.a[j][i])/s;xyz[k]=(m.a[i][k]+m.a[k][i])/s;q={(m.a[k][j]-m.a[j][k])/s,xyz[0],xyz[1],xyz[2]};}
  return q*(1/std::sqrt(dot(q,q)));
}
bool same(ir::Vec3 a,ir::Vec3 b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same(const JointPose &a,const JointPose &b) {return same(a.translation_cm,b.translation_cm)&&same(a.rotation_degrees,b.rotation_degrees)&&same(a.scale,b.scale)&&a.general_scale==b.general_scale&&same(a.center_offset_cm,b.center_offset_cm)&&same(a.end_offset_cm,b.end_offset_cm)&&same(a.orientation_offset_degrees,b.orientation_offset_degrees);}
bool finite(ir::Vec3 a) {return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
struct Palette {M rotation,scale,matrix;V origin,translation,rest_origin,stretch_translation;Q real,dual;};
ir::Vec3 add(ir::Vec3 a,ir::Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
}
void validate_pose(const Skin &skin,const std::vector<JointPose> &pose) {
  if(pose.size()!=skin.joints.size()) throw std::runtime_error("骨骼姿势数量与骨架不一致");
  for(size_t i=0;i<pose.size();++i) {
    const auto &j=skin.joints[i];const auto &p=pose[i];
    if(j.parent>=int(i)||j.parent<-1) throw std::runtime_error("骨架必须按父节点在先排序，且不能成环");
    if(!finite(j.center_cm)||!finite(j.orientation_degrees)||!finite(p.translation_cm)||!finite(p.rotation_degrees)||!finite(p.scale)||!std::isfinite(p.general_scale)||!finite(p.center_offset_cm)||!finite(p.end_offset_cm)||!finite(p.orientation_offset_degrees)) throw std::runtime_error("骨骼参数包含非有限数值");
    auto order=j.rotation_order;std::sort(order.begin(),order.end());if(order!="XYZ") throw std::runtime_error("未知骨骼旋转顺序："+j.rotation_order);
    if(p.scale.x==0||p.scale.y==0||p.scale.z==0||p.general_scale==0) throw std::runtime_error("骨骼缩放不能为零");
    if(skin.separate_scale_weights&&(p.scale.x!=1||p.scale.y!=1||p.scale.z!=1||p.general_scale!=1)) throw std::runtime_error("此资产需要独立缩放权重，暂不支持非单位骨骼缩放："+skin.id);
    if(skin.static_local_weights&&(!same(p.translation_cm,{})||!same(p.rotation_degrees,{})||!same(p.scale,{1,1,1})||p.general_scale!=1||!same(p.center_offset_cm,{})||!same(p.end_offset_cm,{})||!same(p.orientation_offset_degrees,{}))) throw std::runtime_error("此资产需要 TriAx，暂不支持该骨骼变换："+skin.id);
  }
}
static std::vector<Palette> build_palettes(const Skin &skin,const std::vector<JointPose> &pose) {
  validate_pose(skin,pose);
  std::vector<Palette> palettes(pose.size());
  for(size_t i=0;i<pose.size();++i) {
    const auto &j=skin.joints[i];const auto &p=pose[i];auto &out=palettes[i];
    const auto center=v(add(j.center_cm,p.center_offset_cm));const auto orient=rotation(add(j.orientation_degrees,p.orientation_offset_degrees),"XYZ");const auto inv=transpose(orient);
    out.rotation=orient*rotation(p.rotation_degrees,j.rotation_order)*inv;out.scale=orient*scaling(p)*inv;
    out.origin=center+v(p.translation_cm);out.rest_origin=center;
    if(j.parent>=0) {
      const auto &parent=palettes[j.parent];const auto &pj=skin.joints[j.parent];
      const auto offset=center-v(add(pj.center_cm,pose[j.parent].center_offset_cm));
      out.origin=parent.matrix*(offset+v(p.translation_cm))+parent.origin;out.rest_origin=parent.scale*offset+parent.rest_origin;
      out.rotation=parent.rotation*out.rotation;
      auto inherited=parent.scale;
      if(!j.inherits_scale) {
        const auto parent_orientation=rotation(add(pj.orientation_degrees,pose[j.parent].orientation_offset_degrees),"XYZ");
        inherited=inherited*parent_orientation*scaling(pose[j.parent],true)*transpose(parent_orientation);
      }
      out.scale=inherited*out.scale;
    }
    out.matrix=out.rotation*out.scale;out.translation=out.origin-out.matrix*center;
    // 两阶段：先在绑定姿势中按权重伸缩，再绕伸缩后的关节位置做刚性 DQS。
    out.stretch_translation=out.rest_origin-out.scale*center;const auto rigid_translation=out.origin-out.rotation*out.rest_origin;
    out.real=quaternion(out.rotation);out.dual=(Q{0,rigid_translation.x,rigid_translation.y,rigid_translation.z}*out.real)*.5;
  }
  return palettes;
}
std::vector<ir::Transform> joint_transforms(const Skin &skin,const std::vector<JointPose> &pose) {
  std::vector<ir::Transform> result;
  for(const auto &p:build_palettes(skin,pose)) {
    ir::Transform transform;
    const int axes[]={0,2,1};const double signs[]={1,-1,1};
    for(int r=0;r<3;++r) for(int c=0;c<3;++c) transform.value[r*4+c]=float(signs[r]*signs[c]*p.matrix.a[axes[r]][axes[c]]);
    transform.value[3]=float(p.translation.x*.01);transform.value[7]=float(-p.translation.z*.01);transform.value[11]=float(p.translation.y*.01);
    result.push_back(transform);
  }
  return result;
}
std::vector<ir::Vec3> deform(const Skin &skin,const std::vector<JointPose> &pose,const std::vector<ir::Vec3> &source) {
  const auto palettes=build_palettes(skin,pose);
  if(source.size()!=skin.weights.size()) throw std::runtime_error("蒙皮权重与网格顶点数不一致");
  bool neutral=true;for(const auto &p:pose) neutral=neutral&&same(p.translation_cm,{})&&same(p.rotation_degrees,{})&&same(p.scale,{1,1,1})&&p.general_scale==1;
  auto result=source;
  for(size_t i=0;i<source.size();++i) {
    if(!finite(source[i])) throw std::runtime_error("蒙皮输入包含非有限顶点");
    const auto &weights=skin.weights[i];double sum=0;size_t reference=0;
    for(size_t k=0;k<weights.size();++k) {const auto &w=weights[k];if(w.joint>=pose.size()||!std::isfinite(w.weight)||w.weight<=0) throw std::runtime_error("无效蒙皮权重");sum+=w.weight;if(w.weight>weights[reference].weight) reference=k;}
    if(neutral||weights.empty()) continue;
    const V original{source[i].x*100.,source[i].z*100.,-source[i].y*100.};V out;
    if(skin.method==SkinMethod::linear) {
      for(const auto &w:weights) {const auto &m=palettes[w.joint];out=out+(m.matrix*original+m.translation)*(w.weight/sum);}
    } else {
      Q real,dual;V stretched;const auto ref=palettes[weights[reference].joint].real;
      for(const auto &w:weights) {const auto &m=palettes[w.joint];const double weight=(dot(ref,m.real)<0?-1:1)*w.weight/sum;real=real+m.real*weight;dual=dual+m.dual*weight;stretched=stretched+(m.scale*original+m.stretch_translation)*(w.weight/sum);}
      const double norm=std::sqrt(dot(real,real));if(norm<1e-12) throw std::runtime_error("双四元数混合退化");real=real*(1/norm);dual=dual*(1/norm);dual=dual+real*(-dot(real,dual));
      const auto q=real*Q{0,stretched.x,stretched.y,stretched.z}*conjugate(real)+(dual*conjugate(real))*2;out={q.x,q.y,q.z};
    }
    result[i]={float(out.x*.01),float(-out.z*.01),float(out.y*.01)};
    if(!finite(result[i])) throw std::runtime_error("蒙皮结果包含非有限顶点");
  }
  return result;
}
SkinningRuntime::SkinningRuntime(ir::Scene &scene,const std::vector<Skin> &skins):scene_(scene),skins_(skins) {
  diagnostics::Scope scope("skin_construct");
  std::set<uint32_t> meshes;
  for(const auto &s:skins) {const auto mesh=scene.instances.at(s.instance).mesh;if(!meshes.insert(mesh).second) throw std::runtime_error("蒙皮对象必须拥有独立网格");
    validate_pose(s,s.initial);sources_.push_back(scene.meshes.at(mesh).positions);poses_.push_back(s.initial);dirty_.insert(poses_.size()-1);}
}
bool SkinningRuntime::set_pose(size_t skin,const std::vector<JointPose> &pose) {
  validate_pose(skins_.at(skin),pose);auto &old=poses_.at(skin);
  if(std::equal(pose.begin(),pose.end(),old.begin(),[](const auto &a,const auto &b) {return same(a,b);})) return false;
  old=pose;dirty_.insert(skin);return true;
}
ir::Delta SkinningRuntime::evaluate(ir::Delta delta) {
  diagnostics::Scope scope("skin_evaluate");
  for(size_t i=0;i<skins_.size();++i) {const auto mesh=scene_.instances[skins_[i].instance].mesh;
    for(const auto &edit:delta.meshes) if(edit.index==mesh) {sources_[i]=edit.positions;dirty_.insert(i);}}
  for(auto i:dirty_) {
    const auto mesh=scene_.instances[skins_[i].instance].mesh;auto result=deform(skins_[i],poses_[i],sources_[i]);scene_.meshes[mesh].positions=result;
    auto it=std::find_if(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e) {return e.index==mesh;});
    if(it==delta.meshes.end()) delta.meshes.push_back({mesh,std::move(result)});else it->positions=std::move(result);
    ++stats_.evaluations;stats_.vertices+=sources_[i].size();stats_.joints+=poses_[i].size();
  }
  dirty_.clear();return delta;
}
}
