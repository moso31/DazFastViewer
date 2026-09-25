#include "runtime/pose_edit.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <numbers>
#include <limits>
#include <stdexcept>

namespace dfv::runtime {
namespace {
float &axis(ir::Vec3 &v,int i) {return i==0?v.x:i==1?v.y:v.z;}
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
double norm(ir::Vec3 p) {return std::sqrt(double(p.x)*p.x+double(p.y)*p.y+double(p.z)*p.z);}
ir::Vec3 point(const Skin &skin,const std::vector<JointPose> &poses,const std::vector<ir::Transform> &matrices,int j,bool end) {
  auto p=end?skin.joints.at(j).end_cm:skin.joints.at(j).center_cm;const auto offset=end?poses.at(j).end_offset_cm:poses.at(j).center_offset_cm;
  return matrices.at(j).point({(p.x+offset.x)*.01f,-(p.z+offset.z)*.01f,(p.y+offset.y)*.01f});
}
std::string name(const Joint &j) {auto s=j.name;std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});return s;}
std::array<double,3> angle_residual(const ir::Transform &target,const ir::Transform &current) {
  double r[3][3]{};for(int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k) r[i][j]+=double(target.value[i*4+k])*current.value[j*4+k];
  const double theta=std::acos(std::clamp((r[0][0]+r[1][1]+r[2][2]-1)*.5,-1.,1.));
  std::array<double,3> v{r[2][1]-r[1][2],r[0][2]-r[2][0],r[1][0]-r[0][1]};
  if(theta>std::numbers::pi-1e-3) {
    int i=0;if(r[1][1]>r[i][i]) i=1;if(r[2][2]>r[i][i]) i=2;const int j=(i+1)%3,k=(i+2)%3;
    std::array<double,3> axis{};axis[i]=std::sqrt(std::max(0.,(r[i][i]+1)*.5));axis[j]=(r[j][i]+r[i][j])/(4*axis[i]);axis[k]=(r[k][i]+r[i][k])/(4*axis[i]);
    const double sign=axis[0]*v[0]+axis[1]*v[1]+axis[2]*v[2]<0?-1:1;for(int a=0;a<3;++a) v[a]=axis[a]*theta*sign;
  } else {const double factor=theta<1e-5?.5:theta/(2*std::sin(theta));for(auto &a:v) a*=factor;}return v;
}
}
float joint_value(const JointPose &p,int channel) {
  if(channel<0||channel>9) throw std::out_of_range("无效骨骼通道");if(channel==9) return p.general_scale;
  const auto v=channel<3?p.translation_cm:channel<6?p.rotation_degrees:p.scale;return channel%3==0?v.x:channel%3==1?v.y:v.z;
}
bool editable_channel(const Skin &skin,size_t joint,int channel) {
  const auto &c=skin.joints.at(joint).channels.at(channel);
  return c.present&&!c.locked&&!skin.static_local_weights&&!(channel>=6&&skin.separate_scale_weights)&&!(c.clamped&&c.minimum==c.maximum);
}
void set_joint_value(const Skin &skin,std::vector<JointPose> &poses,size_t joint,int channel,float value) {
  if(!std::isfinite(value)) throw std::runtime_error("骨骼参数必须为有限值");
  if(!editable_channel(skin,joint,channel)) throw std::runtime_error("该骨骼通道已锁定或暂不支持");
  const auto &c=skin.joints.at(joint).channels.at(channel);if(c.clamped) value=std::clamp(value,c.minimum,c.maximum);
  auto candidate=poses.at(joint);if(channel==9) candidate.general_scale=value;else axis(channel<3?candidate.translation_cm:channel<6?candidate.rotation_degrees:candidate.scale,channel%3)=value;
  if(candidate.scale.x==0||candidate.scale.y==0||candidate.scale.z==0||candidate.general_scale==0) throw std::runtime_error("骨骼缩放不能为零");poses.at(joint)=candidate;
}
ir::Vec3 joint_point(const Skin &skin,const std::vector<JointPose> &poses,int joint,bool end) {return point(skin,poses,joint_transforms(skin,poses),joint,end);}
ir::Transform rotation_frame(const ir::Transform &transform) {
  // 取极分解的正交部分；非均匀缩放不应成为角度约束。
  auto r=transform;r.value[3]=r.value[7]=r.value[11]=0;
  for(int iteration=0;iteration<24;++iteration) {const auto inverse=ir::inverse(r);auto next=r;double change=0;
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) {const auto k=i*4+j;next.value[k]=(r.value[k]+inverse.value[j*4+i])*.5f;change=std::max(change,double(std::abs(next.value[k]-r.value[k])));}
    r=next;if(change<1e-7) break;
  }return r;
}
ir::Transform joint_orientation(const Skin &skin,const std::vector<JointPose> &poses,int joint) {std::vector<ir::Transform> rotations;joint_transforms(skin,poses,&rotations);return rotations.at(joint);}
double orientation_error_degrees(const ir::Transform &a,const ir::Transform &b) {
  double trace=0;for(int i=0;i<3;++i) for(int j=0;j<3;++j) trace+=double(a.value[i*4+j])*b.value[i*4+j];
  return std::acos(std::clamp((trace-1)*.5,-1.,1.))*180/std::numbers::pi;
}
std::vector<IkGoal> pin_goals(std::span<const PosePin> pins,int skin,const ir::Transform &world) {
  std::vector<IkGoal> result;const auto inverse=ir::inverse(world),rotation=ir::inverse(rotation_frame(world));
  for(const auto &pin:pins) if(pin.skin==skin&&(pin.position||pin.angle)) {IkGoal g{pin.joint,true,inverse.point(pin.world),20};g.fix_position=pin.position;g.fix_orientation=pin.angle;g.orientation=rotation*pin.world_orientation;result.push_back(g);}return result;
}
std::vector<int> ik_chain(const Skin &skin,int joint) {
  std::vector<int> result;if(joint<0||size_t(joint)>=skin.joints.size()||skin.static_local_weights) return result;
  const auto clicked=name(skin.joints[joint]);
  const bool finger=clicked.find("thumb")!=std::string::npos||clicked.find("index")!=std::string::npos||clicked.find("mid")!=std::string::npos||clicked.find("ring")!=std::string::npos||clicked.find("pinky")!=std::string::npos;
  for(int j=joint;j>=0&&result.size()<8;j=skin.joints[j].parent) {
    const auto n=name(skin.joints[j]);if(skin.joints[j].parent<0||n=="hip"||n=="pelvis") break;
    if(j!=joint&&(n.find("collar")!=std::string::npos||(finger&&(n.find("hand")!=std::string::npos||n.find("carpal")!=std::string::npos)))) break;
    result.push_back(j);
    if(n.find("shldrbend")!=std::string::npos||n=="lshldr"||n=="rshldr"||n.find("thighbend")!=std::string::npos||n=="lthigh"||n=="rthigh") break;
    if(j!=joint&&((clicked=="head"&&n.find("necklower")!=std::string::npos)||(clicked.find("eye")!=std::string::npos&&n=="head"))) {result.pop_back();break;}
  }
  return result;
}
IkResult solve_ik(const Skin &skin,std::vector<JointPose> &poses,std::span<const int> chain,std::span<const IkGoal> goals,int iterations) {
  validate_pose(skin,poses);const auto initial=poses;IkResult result;
  if(goals.empty()) return result;
  for(const auto &goal:goals) if(goal.joint<0||size_t(goal.joint)>=poses.size()||!std::isfinite(goal.position.x)||!std::isfinite(goal.position.y)||!std::isfinite(goal.position.z)||!std::isfinite(goal.weight)||goal.weight<=0) throw std::runtime_error("无效 IK 目标");
  const bool angles=std::any_of(goals.begin(),goals.end(),[](const auto &g){return g.fix_orientation;});
  for(const auto &g:goals) if(g.fix_orientation) for(float v:g.orientation.value) if(!std::isfinite(v)) throw std::runtime_error("无效 IK 固定角度");
  auto measure=[&] {
    for(const auto &g:goals) if(g.fix_position) result.error=std::max(result.error,norm(sub(g.position,joint_point(skin,poses,g.joint,g.end))));
    for(const auto &g:goals) if(g.fix_orientation) result.angle_error_degrees=std::max(result.angle_error_degrees,orientation_error_degrees(g.orientation,joint_orientation(skin,poses,g.joint)));
    result.changed=poses!=initial;
  };
  struct Dof {int joint,channel;};std::vector<Dof> dofs;
  for(int j:chain) for(int c=3;c<6;++c) if(editable_channel(skin,size_t(j),c)&&std::none_of(dofs.begin(),dofs.end(),[&](auto d){return d.joint==j&&d.channel==c;})) dofs.push_back({j,c});
  if(dofs.empty()) {measure();return result;}
  auto residual=[&](const std::vector<JointPose> &p) {
    std::vector<ir::Transform> rotations;const auto matrices=joint_transforms(skin,p,angles?&rotations:nullptr);std::vector<double> r;
    for(const auto &g:goals) {const double w=std::sqrt(std::max(.001f,g.weight));
      if(g.fix_position) {const auto d=sub(g.position,point(skin,p,matrices,g.joint,g.end));r.insert(r.end(),{d.x*w,d.y*w,d.z*w});}
      // 旋转向量同时固定三个轴，包括 Twist；使用最短旋转，避免欧拉角跨界与矩阵差在 180 度退化。
      if(g.fix_orientation) for(auto a:angle_residual(g.orientation,rotations[g.joint])) r.push_back(a*.2*w);
    }return r;
  };
  auto squared=[](const auto &r) {double s=0;for(auto v:r) s+=v*v;return s;};
  auto r=residual(poses);double error=squared(r);const size_t n=dofs.size(),m=r.size();
  for(int iteration=0;iteration<std::clamp(iterations,1,64)&&error>1e-10;++iteration) {
    ++result.iterations;std::vector<std::vector<double>> jac(n,std::vector<double>(m));
    for(size_t d=0;d<n;++d) {
      const auto [j,c]=dofs[d];const float saved=joint_value(poses[j],c);const auto &limit=skin.joints[j].channels[c];
      float step=.1f;if(limit.clamped&&saved+step>limit.maximum) step=-step;
      axis(poses[j].rotation_degrees,c-3)=saved+step;const auto test=residual(poses);axis(poses[j].rotation_degrees,c-3)=saved;
      for(size_t k=0;k<m;++k) jac[d][k]=(r[k]-test[k])/step;
    }
    std::vector<std::vector<double>> a(n,std::vector<double>(n+1));
    for(size_t d=0;d<n;++d) {for(size_t e=0;e<n;++e) for(size_t k=0;k<m;++k) a[d][e]+=jac[d][k]*jac[e][k];a[d][d]+=1e-8;for(size_t k=0;k<m;++k) a[d][n]+=jac[d][k]*r[k];}
    for(size_t d=0;d<n;++d) {
      size_t pivot=d;for(size_t e=d+1;e<n;++e) if(std::abs(a[e][d])>std::abs(a[pivot][d])) pivot=e;std::swap(a[d],a[pivot]);
      const double divisor=a[d][d];for(size_t k=d;k<=n;++k) a[d][k]/=divisor;
      for(size_t e=0;e<n;++e) if(e!=d) {const double v=a[e][d];for(size_t k=d;k<=n;++k) a[e][k]-=v*a[d][k];}
    }
    double max_step=0;for(size_t d=0;d<n;++d) max_step=std::max(max_step,std::abs(a[d][n]));const double step_scale=max_step>12?12/max_step:1;
    const auto before=poses;bool improved=false;
    for(double factor:{1.,.5,.25,.125}) {
      poses=before;
      for(size_t d=0;d<n;++d) {const auto [j,c]=dofs[d];const auto &limit=skin.joints[j].channels[c];
        float value=joint_value(poses[j],c)+float(a[d][n]*step_scale*factor);
        // 已保存的越限姿势可逐步回到范围，不因编辑另一关节突然跳动。
        if(limit.clamped) value=std::clamp(value,std::min(limit.minimum,joint_value(initial[j],c)),std::max(limit.maximum,joint_value(initial[j],c)));
        axis(poses[j].rotation_degrees,c-3)=value;
      }
      auto candidate=residual(poses);const double next=squared(candidate);
      if(next<error-1e-14) {r=std::move(candidate);error=next;improved=true;break;}
    }
    if(!improved) {
      poses=before;
      // 共线链的内收目标在一阶雅可比上退化。小角度种子允许下一轮离开奇点。
      if(iteration==0&&error>1e-6&&dofs.size()>1) {const auto [j,c]=dofs.front();const auto &limit=skin.joints[j].channels[c];float v=joint_value(poses[j],c);v+=(limit.clamped&&v+3>limit.maximum)?-3.f:3.f;axis(poses[j].rotation_degrees,c-3)=v;r=residual(poses);error=squared(r);continue;}
      break;
    }
  }
  if(error>squared(residual(initial))+1e-12) poses=initial;
  measure();return result;
}
Skin ik_limits(const Skin &skin,const std::vector<JointPose> &input,const std::vector<JointPose> &effective) {
  Skin result=skin;result.weights.clear();
  for(size_t j=0;j<result.joints.size();++j) for(int c=3;c<6;++c) {const float offset=joint_value(effective.at(j),c)-joint_value(input.at(j),c);auto &limit=result.joints[j].channels[c];limit.minimum+=offset;limit.maximum+=offset;}
  return result;
}
IkResult refine_ik_input(const Skin &skin,std::vector<JointPose> &input,std::span<const int> chain,std::span<const IkGoal> goals,
  const std::function<std::vector<JointPose>(const std::vector<JointPose>&)> &resolve) {
  const auto original=input;auto candidate=input;double best_score=std::numeric_limits<double>::max();IkResult best;
  for(int iteration=0;iteration<10;++iteration) {
    const auto effective=resolve(candidate);IkResult measured;double score=0;
    for(size_t k=0;k<goals.size();++k) {const auto &g=goals[k];if(g.fix_position) {const double e=norm(sub(g.position,joint_point(skin,effective,g.joint,g.end)));score+=g.weight*e*e;measured.error=std::max(measured.error,e);}
      if(g.fix_orientation) {const double a=orientation_error_degrees(g.orientation,joint_orientation(skin,effective,g.joint));measured.angle_error_degrees=std::max(measured.angle_error_degrees,a);score+=g.weight*std::pow(a*std::numbers::pi/180*.2,2);}}
    if(score<best_score) {best_score=score;best=measured;input=candidate;}else if(iteration>1) break;
    if(measured.error<.0001&&measured.angle_error_degrees<.05) break;
    auto solved=effective;solve_ik(ik_limits(skin,candidate,effective),solved,chain,goals,48);const auto next=ik_input(candidate,effective,solved);if(next==candidate) break;candidate=next;
  }
  best.changed=input!=original;return best;
}
std::vector<JointPose> ik_input(const std::vector<JointPose> &input,const std::vector<JointPose> &effective,const std::vector<JointPose> &solved) {
  auto result=input;
  for(size_t j=0;j<result.size();++j) for(int c=0;c<3;++c) axis(result[j].rotation_degrees,c)+=joint_value(solved.at(j),c+3)-joint_value(effective.at(j),c+3);
  return result;
}
bool ik_selection_allowed(size_t count,int selected,int hit,bool modified,bool figure) {return count==1&&selected>=0&&selected==hit&&!modified&&figure;}
}
