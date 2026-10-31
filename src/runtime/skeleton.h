#pragma once
#include "render_ir/scene.h"
#include <set>
#include <array>
#include <map>

namespace dfv::runtime {
struct JointPose {
  ir::Vec3 translation_cm{},rotation_degrees{},scale{1,1,1};
  float general_scale=1;
  ir::Vec3 center_offset_cm{},end_offset_cm{},orientation_offset_degrees{};
  bool operator==(const JointPose &) const = default;
};
// 原生 DSF 定义叠加 DUF 实例覆盖；不从显示名称猜测旋转轴。
struct JointChannel {
  std::string label,group;
  float minimum=-10000,maximum=10000,step=.1f,initial=0;
  bool present=false,visible=true,locked=false,clamped=false,percent=false;
  bool minimum_authored=false,maximum_authored=false,step_authored=false;
};
struct Joint {
  std::string id,name,label,rotation_order="XYZ";
  // 保存场景中的骨骼实例 ID；不同角色的同名骨骼不能共用附件绑定。
  std::string scene_id;
  std::vector<std::string> aliases;
  int parent=-1;
  ir::Vec3 center_cm{},end_cm{},orientation_degrees{};
  bool inherits_scale=true;
  std::array<JointChannel,10> channels; // translation XYZ, rotation XYZ, scale XYZ, general_scale。
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
std::vector<ir::Transform> joint_transforms(const Skin &skin,const std::vector<JointPose> &pose,std::vector<ir::Transform> *rotations=nullptr);
std::vector<ir::Vec3> deform(const Skin &skin,const std::vector<JointPose> &pose,const std::vector<ir::Vec3> &source);
std::vector<ir::Transform> vertex_transforms(const Skin &skin,const std::vector<JointPose> &pose);
// 跨代鞋类保留服装权重，按映射使用目标完整骨架，避免漏掉目标独有的父骨骼。
std::vector<ir::Vec3> deform_retargeted(const Skin &follower,const Skin &host,const std::vector<int> &mapping,
  const std::vector<JointPose> &follower_pose,const std::vector<JointPose> &host_pose,const std::vector<ir::Vec3> &source);
class SkinningRuntime {
  ir::Scene &scene_;
  const std::vector<Skin> &skins_;
  std::vector<std::vector<JointPose>> poses_;
  std::vector<std::vector<ir::Vec3>> sources_;
  std::set<size_t> dirty_;
  std::map<size_t,Skin> uniform_forefeet_;
  std::map<size_t,float> forefoot_strengths_;
  struct Retarget {size_t source;std::vector<int> mapping;};
  std::map<size_t,Retarget> retargets_;
  SkinStats stats_;
public:
  SkinningRuntime(ir::Scene &scene,const std::vector<Skin> &skins);
  SkinningRuntime(const SkinningRuntime &s,ir::Scene &scene):scene_(scene),skins_(s.skins_),poses_(s.poses_),sources_(s.sources_),dirty_(s.dirty_),uniform_forefeet_(s.uniform_forefeet_),forefoot_strengths_(s.forefoot_strengths_),retargets_(s.retargets_),stats_(s.stats_) {}
  bool set_pose(size_t skin,const std::vector<JointPose> &pose);
  void set_forefoot(size_t skin,float strength,const ir::Mesh &reference);
  void retarget(size_t follower,size_t source,const std::vector<int> &mapping);
  // 接收 Morph 输出，缓存蒙皮前顶点；绝不以上一次蒙皮结果作为输入。
  ir::Delta evaluate(ir::Delta before={});
  const auto &stats() const {return stats_;}
  const auto &source(size_t skin) const {return sources_.at(skin);}
};
}
