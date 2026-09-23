#pragma once
#include "render_ir/scene.h"
#include <set>

namespace dfv::runtime {
struct JointPose {
  ir::Vec3 translation_cm{},rotation_degrees{},scale{1,1,1};
  float general_scale=1;
  ir::Vec3 center_offset_cm{},end_offset_cm{},orientation_offset_degrees{};
  bool operator==(const JointPose &) const = default;
};
struct Joint {
  std::string id,name,label,rotation_order="XYZ";
  // 保存场景中的骨骼实例 ID；不同角色的同名骨骼不能共用附件绑定。
  std::string scene_id;
  std::vector<std::string> aliases;
  int parent=-1;
  ir::Vec3 center_cm{},end_cm{},orientation_degrees{};
  bool inherits_scale=true;
};
struct Influence {uint32_t joint=0;double weight=0;};
enum class SkinMethod {linear,dual_quaternion};
struct Skin {
  std::string id;
  uint32_t instance=0;
  SkinMethod method=SkinMethod::dual_quaternion;
  std::vector<Joint> joints;
  std::vector<JointPose> initial;
  std::vector<std::vector<Influence>> weights;
  // 实例矩阵已包含 Figure 的保存缩放；ERC 在原通道空间求值后再换算为相对值。
  float root_general_scale=1;
  bool separate_scale_weights=false;
  bool static_local_weights=false;
};
struct SkinStats {uint64_t evaluations=0,vertices=0,joints=0;};
// 姿势在 DAZ 厘米 / Y 向上坐标内求值；几何输入输出均使用 Render IR 坐标。
void validate_pose(const Skin &skin,const std::vector<JointPose> &pose);
std::vector<ir::Transform> joint_transforms(const Skin &skin,const std::vector<JointPose> &pose);
std::vector<ir::Vec3> deform(const Skin &skin,const std::vector<JointPose> &pose,const std::vector<ir::Vec3> &source);
class SkinningRuntime {
  ir::Scene &scene_;
  const std::vector<Skin> &skins_;
  std::vector<std::vector<JointPose>> poses_;
  std::vector<std::vector<ir::Vec3>> sources_;
  std::set<size_t> dirty_;
  SkinStats stats_;
public:
  SkinningRuntime(ir::Scene &scene,const std::vector<Skin> &skins);
  bool set_pose(size_t skin,const std::vector<JointPose> &pose);
  // 接收 Morph 输出，缓存蒙皮前顶点；绝不以上一次蒙皮结果作为输入。
  ir::Delta evaluate(ir::Delta before={});
  const auto &stats() const {return stats_;}
};
}
