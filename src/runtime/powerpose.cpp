#include "runtime/powerpose.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

namespace dfv::runtime {
namespace {
std::string lower(std::string s) {for(auto &c:s) c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
int channel(const Joint &joint,std::string prop) {
  prop=lower(prop);
  for(int c=0;c<3;++c) {
    if(prop==std::string(1,"xyz"[c])+"rot") return 3+c;
    if(prop==std::string(1,"xyz"[c])+"translate") return c;
    if(prop==std::array<std::string,3>{"firstrot","secondrot","thirdrot"}[c]) return 3+int(joint.rotation_order.at(c)-'X');
  }
  return -1;
}
int tool_joint(const Skin &skin,int joint,int c) {
  // DAZ 的工具旋转跨 Bend / Twist 拆分骨。先解析工具身份，再检查实际目标锁定，
  // 不能在用户锁定 Twist 后退回 Bend，也不能因 Bend 被解锁而换轴。
  static const std::map<std::pair<std::string,int>,std::string> split={
    {{"lshldr",3},"lShldrTwist"},{{"rshldr",3},"rShldrTwist"},
    {{"lthigh",4},"lThighTwist"},{{"rthigh",4},"rThighTwist"},
    {{"lforearm",3},"lWrist"},{{"rforearm",3},"rWrist"}};
  auto it=split.find({lower(skin.joints.at(joint).id),c});
  if(it!=split.end()) {const int found=powerpose_joint(skin,it->second);if(found>=0) return found;}
  return joint;
}
}
const std::vector<PoseTemplate> &powerpose_templates() {
  static const auto templates=[] {
    const auto data=nlohmann::json::parse(
      #include "runtime/powerpose_data.inl"
    );
    std::vector<PoseTemplate> result;
    for(const auto &t:data) {PoseTemplate page;page.name=t.at("name");
      for(const auto &p:t.at("points")) {PosePoint point;point.id=p.at("id");point.label=p.at("label");point.node=p.at("node");point.target=p.at("target");
        point.x=p.at("x");point.y=p.at("y");point.male_x=p.at("male_x");point.male_y=p.at("male_y");point.disabled=p.at("disabled");
        const auto kind=p.at("kind");point.kind=kind=="group"?PosePointKind::group:kind=="property"?PosePointKind::property:kind=="template"?PosePointKind::navigation:PosePointKind::node;
        for(size_t i=0;i<4;++i) for(const auto &b:p.at("slots").at(i)) point.slots[i].push_back({b.at("node"),b.at("property"),b.at("sign"),b.at("size")});
        page.points.push_back(std::move(point));
      }result.push_back(std::move(page));
    }return result;
  }();return templates;
}
int powerpose_joint(const Skin &skin,const std::string &name) {
  if(name=="Figure") {for(size_t j=0;j<skin.joints.size();++j) if(skin.joints[j].parent<0) return int(j);return -1;}
  const auto match=lower(name);
  // 稳定 ID 优先，再使用内部名称与原生别名，最后才接受显示标签。
  for(size_t j=0;j<skin.joints.size();++j) if(lower(skin.joints[j].id)==match) return int(j);
  for(size_t j=0;j<skin.joints.size();++j) {const auto &bone=skin.joints[j];if(lower(bone.name)==match) return int(j);for(const auto &alias:bone.aliases) if(lower(alias)==match) return int(j);}
  for(size_t j=0;j<skin.joints.size();++j) if(lower(skin.joints[j].label)==match) return int(j);
  return -1;
}
BoundPosePoint bind_powerpose(const Skin &skin,const PosePoint &point) {
  BoundPosePoint bound;bound.point=&point;bound.hip_translation=point.id=="B21";bound.figure=point.id=="B35"||point.id=="B36";
  for(size_t slot=0;slot<4;++slot) for(const auto &binding:point.slots[slot]) {
    int j=powerpose_joint(skin,binding.node),c=j<0?-1:channel(skin.joints[j],binding.property);
    if(c>=0) j=tool_joint(skin,j,c);
    if(j<0||c<0||!editable_channel(skin,j,c)) {bound.unavailable.push_back(binding.node+" / "+binding.property);continue;}
    const auto &limit=skin.joints[j].channels[c];const float range=limit.maximum-limit.minimum;
    if(!std::isfinite(range)||range<=0||!std::isfinite(binding.size)||binding.size<=0) {bound.unavailable.push_back(binding.node+" / "+binding.property);continue;}
    bound.slots[slot].push_back({j,c,binding.sign*range/binding.size});bound.editable=true;
    if(std::find(bound.joints.begin(),bound.joints.end(),j)==bound.joints.end()) bound.joints.push_back(j);
  }
  bound.pin_allowed=point.kind==PosePointKind::node&&!bound.figure&&!bound.joints.empty()&&!ik_chain(skin,bound.joints.front()).empty();
  return bound;
}
std::vector<JointPose> apply_powerpose(const Skin &skin,const BoundPosePoint &point,const std::vector<JointPose> &start,bool right,double dx,double dy,double precision) {
  if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(precision)||precision<=0) throw std::runtime_error("PowerPose 位移无效");
  auto result=start;std::map<std::pair<int,int>,double> changes;
  for(int axis=0;axis<2;++axis) for(const auto &c:point.slots[(right?2:0)+axis]) changes[{c.joint,c.channel}]+=(axis?dy:dx)*precision*c.rate;
  // 相同参数同时出现在横纵轴时先合并，再限位一次；未触及的通道保持原值。
  for(const auto &[key,delta]:changes) if(delta!=0) set_joint_value(skin,result,key.first,key.second,float(joint_value(start.at(key.first),key.second)+delta));
  return result;
}
std::vector<JointPose> reset_powerpose(const Skin &skin,const BoundPosePoint &point,const std::vector<JointPose> &start) {
  auto result=start;
  for(const auto &slot:point.slots) for(const auto &c:slot) {
    // 已载入姿势可能本来就在限位外；恢复应精确还原该通道，不能顺手修正源姿势。
    auto &v=c.channel<3?result.at(c.joint).translation_cm:result.at(c.joint).rotation_degrees;
    float &axis=c.channel%3==0?v.x:c.channel%3==1?v.y:v.z;axis=joint_value(skin.initial.at(c.joint),c.channel);
  }return result;
}
}
