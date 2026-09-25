#pragma once
#include "runtime/pose_edit.h"
#include "bench/camera.h"

namespace dfv::editor {
void build_pose_proxy(const runtime::Skin &skin,const ir::Mesh &mesh,const std::vector<ir::Vec3> &source,
  runtime::Skin &proxy_skin,ir::Mesh &proxy,std::vector<ir::Vec3> &proxy_source);
// 拖动只处理有预算上限的宿主基础笼；不求服装、曲线头发、碰撞、细分或 Cycles。
class PoseDrag {
  runtime::Skin solver_,proxy_skin_;
  std::vector<runtime::JointPose> input_,effective_,solved_;
  std::vector<ir::Vec3> source_;
  std::vector<int> chain_;
  std::vector<runtime::IkGoal> pins_;
  CameraState camera_;
  ir::Transform world_,inverse_;
  ir::Vec3 start_;
  float pixel_scale_=0;
  int width_=0,height_=0;
  uint64_t pointer_revision_=0;
public:
  runtime::PosePointer press;
  uint64_t generation=0,revision=0;
  int skin=-1,joint=-1,target=-1;
  bool active=false,moved=false;
  ir::Mesh proxy;
  runtime::IkGoal goal;
  runtime::IkResult result;
  std::vector<std::pair<ir::Vec3,ir::Vec3>> bones;
  void begin(const runtime::Skin &skeleton,const ir::Mesh &mesh,const std::vector<ir::Vec3> &source,
    const std::vector<runtime::JointPose> &input,const std::vector<runtime::JointPose> &effective,
    ir::Transform world,int bone,runtime::PosePointer pointer,CameraState camera,int width,int height,std::vector<runtime::IkGoal> pins={});
  bool update(runtime::PosePointer pointer);
  auto input() const {return runtime::ik_input(input_,effective_,solved_);}
  std::vector<runtime::JointPose> commit(const runtime::Skin &skin,const std::function<std::vector<runtime::JointPose>(const std::vector<runtime::JointPose>&)> &resolve);
  const auto &world() const {return world_;}
  const auto &camera() const {return camera_;}
};
}
