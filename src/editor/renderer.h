#pragma once
#include "editor/document.h"
#include "editor/selection.h"
#include "runtime/powerpose.h"
#include "editor/gizmo.h"
#include "cycles/adapter.h"
#include "viewport/window.h"
#include "viewport/quality.h"
#include "bench/telemetry.h"
#include <memory>
#include <thread>

namespace dfv::editor {
// 采样参数和可重建的视口画质；诊断可覆盖，降噪始终关闭。
struct SamplingSettings : RenderQuality {
  int samples=4096,min_bounces=0,transparent_min_bounces=0;
  float adaptive_threshold=.01f;
  bool blue_noise=true;
  bool interaction_probe=false;
  bool rebuild_probe=false;
  double update_interval_seconds=.125;
  bool prune_hidden=true;
  bool quality_override=false;
};
// 仅供 --render-profile 的受控消融实验，始终从原场景恢复后应用。
struct RenderProbe {
  uint64_t serial=0;
  bool disable_sss=false,disable_bump=false;
  int transparent_bounces=32;
  std::vector<std::string> hidden;
};
struct RenderStatus {
  bool graphics_recovering=false,graphics_blocked=false;
  unsigned graphics_attempts=0;
  uint64_t graphics_recoveries=0;
  SamplingSettings sampling;
  uint64_t probe_serial=0;
  size_t gpu_device_bytes=0,gpu_host_bytes=0;
  bool pose_gizmo=false,gizmo_available=false;
  int pose_light=-1;
  GizmoShape gizmo_shape;
  ir::Transform gizmo_light_transform;
  uint64_t pose_commit=0,pose_revision=0,pose_generation=0,pose_previews=0;
  int pose_skin=-1,pose_joint=-1;
  bool pose_dragging=false,pose_restoring=false;
  bool pose_powerpose=false,pose_figure=false;
  int pose_target=-1;
  runtime::TransformValues pose_transform;
  double pose_solve_ms=0,pose_error=0,pose_latency_ms=0,pose_angle_error=0;
  double pose_restore_max_error=0; // 仅 interaction_probe 的逐顶点恢复诊断。
  std::vector<runtime::JointPose> pose_input;
  std::vector<std::vector<runtime::JointPose>> effective_poses;
  std::vector<std::vector<runtime::JointPose>> input_poses;
  std::vector<ir::Transform> skin_world;
  std::vector<ir::Transform> target_world;
  int weight_target=-1;
  uint64_t weight_generation=0;
  std::shared_ptr<const std::vector<ir::Vec3>> weight_positions;
  uint64_t generation=0,applied_revision=0,presented_revision=0,frames=0;
  uint64_t requested_epoch=0,presented_epoch=0;
  AdapterStats adapter;
  runtime::EvaluationStats evaluation;
  runtime::SkinStats skinning;
  runtime::FormulaStats formulas;
  runtime::ConformStats conform;
  runtime::CollisionStats collision;
  std::vector<std::vector<float>> effective;
  std::vector<runtime::JointPose> effective_roots;
  std::vector<ir::Bounds> bounds;
  std::vector<ir::Bounds> head_bounds;
  std::vector<bool> visible;
  uint64_t clicks=0,focus_requests=0,ground_requests=0;
  ir::Bounds selection_bounds;
  int hit_target=-1,hit_joint=-1,hovered=-1,hovered_joint=-1,width=0,height=0;
  size_t hovered_triangles=0;
  size_t material_hover_primitives=0;
  uint64_t selection_generation=0;
  int selected_target=-1,selected_joint=-1;
  std::vector<Selection> selections;
  bool hit_toggle=false,hover_toggle=false;
  CameraState camera;
  int pointer_x=-1,pointer_y=-1,hovered_detail_joint=-1;
  double max_displacement=0;
  int samples=0;
  bool preview=false;
  ViewportQuality quality;
  int render_width=0,render_height=0;
  uint64_t last_preview_frame=0;
  double present_time=0;
  uint64_t sessions=0;
  std::vector<uint64_t> mesh_hashes;
  std::vector<runtime::GraftSeam> graft_seams;
  std::vector<std::array<float,12>> instance_transforms;
  std::string error;
  std::string edit_error;
  size_t pending_payloads=0;
  std::string resource_error;
};
class Renderer {
  std::filesystem::path output_;
  SamplingSettings sampling_;
  ViewportQuality quality_;
  RenderProbe probe_;
  Telemetry telemetry_;
  std::unique_ptr<Window> window_;
  std::mutex mutex_;
  RenderQuality requested_render_quality_;
  std::shared_ptr<const Document> document_;
  Snapshot snapshot_;
  RenderStatus status_;
  std::jthread thread_;
  int requested_width_=0,requested_height_=0;
  uint64_t selection_generation_=0;
  uint64_t material_hover_generation_=0;
  std::vector<std::pair<size_t,size_t>> material_hover_;
  uint64_t retry_resources_=0;
  std::atomic<uint64_t> render_restarts_{0};
  bool edit_active_=false;
  std::vector<runtime::PosePin> pose_pins_;
  runtime::PowerPoseInput powerpose_input_;
  GizmoSettings gizmo_settings_;
  uint64_t interaction_revision_=0;
  double edit_preview_until_=0,resize_preview_until_=0;
  int selected_target_=-1,selected_joint_=-1;
  std::vector<Selection> selections_;
  bool ik_allowed_=false;
  void run(std::stop_token stop);
public:
  Renderer(HWND host,int width,int height,const std::filesystem::path &output,SamplingSettings sampling={});
  ~Renderer();
  void set_document(std::shared_ptr<const Document> document,const Snapshot &snapshot,bool frame_scene=true);
  void resize(int width,int height);
  void quality(ViewportQuality value) {std::lock_guard lock(mutex_);value.percent=std::clamp(value.percent,50,100);quality_=value;}
  void render_quality(RenderQuality value) {std::lock_guard lock(mutex_);requested_render_quality_=value;}
  void render_probe(RenderProbe value) {std::lock_guard lock(mutex_);probe_=std::move(value);}
  void pointer(int x,int y,bool click=false,bool toggle=false);
  void automated_pointer() {window_->automated_pointer=true;}
  void select(uint64_t generation,int target,int joint=-1,std::vector<Selection> selections={},bool ik_allowed=false);
  void hover_materials(uint64_t generation,const std::vector<MaterialSurface> &surfaces){std::lock_guard lock(mutex_);material_hover_generation_=generation;material_hover_.clear();for(auto s:surfaces)material_hover_.emplace_back(s.instance,s.slot);}
  void camera_view(const std::array<float,6> &view);
  void edit(const Snapshot &snapshot);
  void interaction(bool active);
  void powerpose(runtime::PowerPoseInput input) {std::lock_guard lock(mutex_);powerpose_input_=std::move(input);}
  void gizmo(GizmoSettings settings) {std::lock_guard lock(mutex_);gizmo_settings_=settings;++window_->pose_selection;}
  void pose_pins(std::vector<runtime::PosePin> pins) {std::lock_guard lock(mutex_);pose_pins_=std::move(pins);++window_->pose_selection;}
  void retry_resources();
  void restart_render() {++render_restarts_;}
#ifdef DFV_GL_RECOVERY_TEST
  void inject_graphics_failure(bool upload,GLContext::Fault fault) {(upload?window_->render_context:window_->present_context).inject(fault);}
#endif
  RenderStatus status();
  CameraState input_camera();
  void orbit(float x,float y);
  void keyboard(int key,bool pressed);
  void frame(const ir::Bounds &bounds);
  void focus(const ir::Bounds &bounds);
  void trace(const char *event,double duration_ms=0) {telemetry_.event(event,{},duration_ms);}
};
}
