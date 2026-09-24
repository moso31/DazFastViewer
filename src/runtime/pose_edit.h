#pragma once
#include "runtime/skeleton.h"
#include <span>
#include <functional>

namespace dfv::runtime {
float joint_value(const JointPose &pose,int channel);
void set_joint_value(const Skin &skin,std::vector<JointPose> &poses,size_t joint,int channel,float value);
bool editable_channel(const Skin &skin,size_t joint,int channel);
// 所有目标点使用角色局部 Render IR 坐标（米）；对象矩阵在交互边界转换。
ir::Vec3 joint_point(const Skin &skin,const std::vector<JointPose> &poses,int joint,bool end=false);
struct IkGoal {int joint=-1;bool end=true;ir::Vec3 position{};float weight=1;bool fix_position=true,fix_orientation=false;ir::Transform orientation;};
struct PosePin {int skin=-1,joint=-1;ir::Vec3 world;bool position=true,angle=false;ir::Transform world_orientation;};
struct IkResult {double error=0;int iterations=0;bool changed=false;double angle_error_degrees=0;};
ir::Transform rotation_frame(const ir::Transform &transform);
ir::Transform joint_orientation(const Skin &skin,const std::vector<JointPose> &poses,int joint);
double orientation_error_degrees(const ir::Transform &a,const ir::Transform &b);
std::vector<IkGoal> pin_goals(std::span<const PosePin> pins,int skin,const ir::Transform &world);
std::vector<int> ik_chain(const Skin &skin,int joint);
IkResult solve_ik(const Skin &skin,std::vector<JointPose> &poses,std::span<const int> chain,std::span<const IkGoal> goals,int iterations=24);
IkResult refine_ik_input(const Skin &skin,std::vector<JointPose> &input,std::span<const int> chain,std::span<const IkGoal> goals,
  const std::function<std::vector<JointPose>(const std::vector<JointPose>&)> &resolve);
// 初始 ERC 差值保留在预览中，提交时只写回用户输入的改变量。
Skin ik_limits(const Skin &skin,const std::vector<JointPose> &input,const std::vector<JointPose> &effective);
std::vector<JointPose> ik_input(const std::vector<JointPose> &input,const std::vector<JointPose> &effective,const std::vector<JointPose> &solved);
// 连续左键输入保留按下身份；松开、取消和阈值不依赖渲染帧率。
struct PosePointer {
  uint64_t serial=0,revision=0,selection=0;
  double input_seconds=0;
  int start_x=0,start_y=0,x=0,y=0;
  bool held=false,moved=false,cancelled=false,modified=false;
};
bool ik_selection_allowed(size_t count,int selected_target,int hit_target,bool modified,bool figure);
}
