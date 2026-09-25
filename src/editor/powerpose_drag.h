#pragma once
#include "runtime/powerpose.h"
#include "runtime/morph.h"
#include "bench/camera.h"

namespace dfv::editor {
class PowerPoseDrag {
  runtime::Skin limits_,proxy_skin_;
  std::vector<runtime::JointPose> initial_,effective_,proposed_,solved_;
  std::vector<ir::Vec3> source_;
  std::vector<runtime::IkGoal> pins_;
  std::vector<int> chain_;
  runtime::TransformValues initial_transform_;
  ir::Transform base_world_;
  ir::Transform loaded_world_;
  runtime::Target figure_frame_;
  uint64_t event_=0;
public:
  runtime::PowerPoseInput press;
  runtime::BoundPosePoint bound;
  CameraState camera;
  int width=0,height=0;
  bool active=false,moved=false;
  ir::Mesh proxy;
  ir::Transform world;
  runtime::TransformValues transform;
  runtime::IkResult result;
  std::vector<std::pair<ir::Vec3,ir::Vec3>> bones;
  void begin(const runtime::Skin &skin,const ir::Mesh &mesh,const std::vector<ir::Vec3> &source,
    const std::vector<runtime::JointPose> &input,const std::vector<runtime::JointPose> &effective,
    ir::Transform world,runtime::TransformValues transform,runtime::BoundPosePoint binding,
    runtime::PowerPoseInput pointer,CameraState view,int w,int h,std::vector<runtime::IkGoal> pins,
    const runtime::Target *figure=nullptr,ir::Transform loaded_world={});
  bool update(const runtime::Skin &skin,const runtime::PowerPoseInput &pointer);
  bool changed() const {return bound.figure?transform.translation_cm!=initial_transform_.translation_cm||transform.rotation_degrees!=initial_transform_.rotation_degrees:proposed_!=initial_;}
  std::vector<runtime::JointPose> commit(const runtime::Skin &skin,const std::function<std::vector<runtime::JointPose>(const std::vector<runtime::JointPose>&)> &resolve);
};
}
