#pragma once
#include "runtime/pose_edit.h"

namespace dfv::runtime {
enum class PosePointKind {node,group,property,navigation};
struct PoseBinding {std::string node,property;float sign=1,size=300;};
struct PosePoint {
  std::string id,label,node,target;
  PosePointKind kind=PosePointKind::node;
  int x=0,y=0,male_x=0,male_y=0;
  bool disabled=false;
  std::array<std::vector<PoseBinding>,4> slots; // 左横、左纵、右横、右纵。
};
struct PoseTemplate {std::string name;std::vector<PosePoint> points;};
struct PoseChannel {int joint=-1,channel=-1;float rate=0;bool operator==(const PoseChannel &) const=default;};
struct BoundPosePoint {
  const PosePoint *point=nullptr;
  std::array<std::vector<PoseChannel>,4> slots;
  std::vector<int> joints;
  std::vector<std::string> unavailable;
  bool editable=false,figure=false,hip_translation=false,pin_allowed=false;
};
const std::vector<PoseTemplate> &powerpose_templates();
int powerpose_joint(const Skin &skin,const std::string &name);
BoundPosePoint bind_powerpose(const Skin &skin,const PosePoint &point);
std::vector<JointPose> apply_powerpose(const Skin &skin,const BoundPosePoint &point,
  const std::vector<JointPose> &start,bool right,double dx,double dy,double precision=1);
std::vector<JointPose> reset_powerpose(const Skin &skin,const BoundPosePoint &point,const std::vector<JointPose> &start);
// 鼠标命令在 UI 和渲染线程之间只保留最新状态；身份在按下时固定。
struct PowerPoseInput {
  uint64_t serial=0,event=0,generation=0,revision=0;
  int skin=-1,target=-1;
  std::string instance,point;
  bool right=false,held=false,moved=false,cancelled=false;
  double dx=0,dy=0,precision=1,input_seconds=0;
};
}
