#pragma once
#include "editor/pose_drag.h"
#include "runtime/morph.h"

namespace dfv::editor {
enum class GizmoTool {select,translate,rotate,scale};
enum class GizmoSpace {local,world};
struct GizmoSettings {
  GizmoTool tool=GizmoTool::select;
  GizmoSpace space=GizmoSpace::local;
  bool operator==(const GizmoSettings &) const = default;
};
struct GizmoSegment {ir::Vec2 a,b;int handle=-1;};
// 4、5、6 分别是法线为 X、Y、Z 的平移平面，3 保留给等比缩放。
struct GizmoPlane {std::array<ir::Vec2,4> corners;int handle=-1;};
struct GizmoShape {
  std::vector<GizmoSegment> lines;
  std::vector<GizmoPlane> planes;
  ir::Vec2 center;
  ir::Vec3 pivot;
  std::array<ir::Vec3,3> axes;
  float radius=0,pixels=90;
  bool rotation_center=false; // 仅用于绘制轴心标记，不加入手柄命中几何。
  int hit(float x,float y,float tolerance=8) const;
};
// 绘制与命中共用投影几何，坐标为视口物理像素；DAZ 的 Y 轴对应 IR 的 Z 轴。
GizmoShape gizmo_shape(const CameraState &camera,int width,int height,ir::Vec3 pivot,
  const ir::Transform &orientation,GizmoSettings settings,float dpi=1,std::array<bool,3> enabled={true,true,true});
ir::Transform gizmo_rotation(ir::Vec3 axis,float radians);
runtime::TransformValues ground_aligned_transform(const runtime::Target &target,const runtime::TransformValues &value,
  const ir::Transform &loaded,const ir::Transform &current,const ir::Bounds &world_bounds,double ratio);
class GizmoDrag {
  runtime::Target frame_;
  runtime::TransformValues initial_;
  runtime::Skin skin_,proxy_skin_;
  std::vector<runtime::JointPose> input_,effective_,solved_;
  std::vector<ir::Vec3> source_;
  ir::Transform loaded_,prefix_,base_world_,initial_orientation_;
  ir::Vec3 initial_pivot_,plane_press_;
  float start_angle_=0,previous_angle_=0,angle_=0;
  uint64_t event_=0;
  void solve(const ir::Vec3 &position,const ir::Transform &orientation);
  std::vector<runtime::JointPose> effective(const std::vector<runtime::JointPose> &input) const;
public:
  runtime::PosePointer press;
  CameraState camera;
  GizmoSettings settings;
  GizmoShape shape;
  int width=0,height=0,handle=-1,target=-1,joint=-1,skin=-1;
  uint64_t generation=0,revision=0;
  bool active=false,moved=false;
  runtime::TransformValues transform;
  std::vector<runtime::JointPose> poses;
  ir::Mesh proxy;
  ir::Transform world;
  std::array<bool,3> enabled{true,true,true};
  void object(const runtime::Target &target,const runtime::TransformValues &value,const ir::Transform &loaded,const ir::Transform &current);
  void bone(const runtime::Skin &skeleton,int bone,const std::vector<runtime::JointPose> &input,const std::vector<runtime::JointPose> &effective,const ir::Transform &current);
  ir::Vec3 pivot() const;
  ir::Transform orientation() const;
  void layout(CameraState view,int w,int h,GizmoSettings mode,float dpi);
  bool begin(runtime::PosePointer pointer,CameraState view,int w,int h,GizmoSettings mode,float dpi,
    const ir::Mesh &mesh,const std::vector<ir::Vec3> *skin_source=nullptr);
  bool update(runtime::PosePointer pointer);
  bool changed() const;
};
}
