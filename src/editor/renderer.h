#pragma once
#include "editor/document.h"
#include "cycles/adapter.h"
#include "viewport/window.h"
#include "bench/telemetry.h"
#include <memory>
#include <thread>

namespace dfv::editor {
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
  std::vector<ir::Bounds> bounds;
  std::vector<ir::Bounds> head_bounds;
  std::vector<bool> visible;
  uint64_t clicks=0;
  int hit_target=-1,hit_joint=-1,hovered=-1,hovered_joint=-1,width=0,height=0;
  size_t hovered_triangles=0;
  uint64_t selection_generation=0;
  int selected_target=-1,selected_joint=-1;
  CameraState camera;
  int pointer_x=-1,pointer_y=-1,hovered_detail_joint=-1;
  double max_displacement=0;
  int samples=0;
  std::string error;
  std::string edit_error;
};
class Renderer {
  std::filesystem::path output_;
  Telemetry telemetry_;
  std::unique_ptr<Window> window_;
  std::mutex mutex_;
  std::shared_ptr<const Document> document_;
  Snapshot snapshot_;
  RenderStatus status_;
  std::jthread thread_;
  int requested_width_=0,requested_height_=0;
  uint64_t selection_generation_=0;
  int selected_target_=-1,selected_joint_=-1;
  void run(std::stop_token stop);
public:
  Renderer(HWND host,int width,int height,const std::filesystem::path &output);
  ~Renderer();
  void set_document(std::shared_ptr<const Document> document,const Snapshot &snapshot,bool frame_scene=true);
  void resize(int width,int height);
  void pointer(int x,int y,bool click=false);
  void select(uint64_t generation,int target,int joint=-1);
  void edit(const Snapshot &snapshot);
  RenderStatus status();
  void orbit(float x,float y);
  void frame(const ir::Bounds &bounds);
};
}
