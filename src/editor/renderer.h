#pragma once
#include "editor/document.h"
#include "editor/selection.h"
#include "cycles/adapter.h"
#include "viewport/window.h"
#include "bench/telemetry.h"
#include <memory>
#include <thread>

namespace dfv::editor {
// 启动时固定的采样参数，诊断可覆盖；降噪始终关闭。
struct SamplingSettings {
  int samples=4096,min_bounces=0,transparent_min_bounces=0;
  float adaptive_threshold=.01f;
  bool blue_noise=true;
  bool interaction_probe=false;
  bool rebuild_probe=false;
};
struct RenderStatus {
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
  uint64_t clicks=0,focus_requests=0;
  ir::Bounds selection_bounds;
  int hit_target=-1,hit_joint=-1,hovered=-1,hovered_joint=-1,width=0,height=0;
  size_t hovered_triangles=0;
  uint64_t selection_generation=0;
  int selected_target=-1,selected_joint=-1;
  std::vector<Selection> selections;
  bool hit_toggle=false,hover_toggle=false;
  CameraState camera;
  int pointer_x=-1,pointer_y=-1,hovered_detail_joint=-1;
  double max_displacement=0;
  int samples=0;
  bool preview=false;
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
  Telemetry telemetry_;
  std::unique_ptr<Window> window_;
  std::mutex mutex_;
  std::shared_ptr<const Document> document_;
  Snapshot snapshot_;
  RenderStatus status_;
  std::jthread thread_;
  int requested_width_=0,requested_height_=0;
  uint64_t selection_generation_=0;
  uint64_t retry_resources_=0;
  bool edit_active_=false;
  uint64_t interaction_revision_=0;
  double edit_preview_until_=0,resize_preview_until_=0;
  int selected_target_=-1,selected_joint_=-1;
  std::vector<Selection> selections_;
  void run(std::stop_token stop);
public:
  Renderer(HWND host,int width,int height,const std::filesystem::path &output,SamplingSettings sampling={});
  ~Renderer();
  void set_document(std::shared_ptr<const Document> document,const Snapshot &snapshot,bool frame_scene=true);
  void resize(int width,int height);
  void pointer(int x,int y,bool click=false,bool toggle=false);
  void select(uint64_t generation,int target,int joint=-1,std::vector<Selection> selections={});
  void camera_view(const std::array<float,6> &view);
  void edit(const Snapshot &snapshot);
  void interaction(bool active);
  void retry_resources();
  RenderStatus status();
  CameraState input_camera();
  void orbit(float x,float y);
  void keyboard(int key,bool pressed);
  void frame(const ir::Bounds &bounds);
  void focus(const ir::Bounds &bounds);
  void trace(const char *event,double duration_ms=0) {telemetry_.event(event,{},duration_ms);}
};
}
