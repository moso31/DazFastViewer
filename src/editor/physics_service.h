#pragma once
#include "editor/document.h"
#include "runtime/physics.h"
#include <atomic>
#include <condition_variable>
#include <thread>

namespace dfv::editor {
bool physics_eligible(const Document &document,size_t target);
runtime::PhysicsKind physics_kind(const Document &document,size_t target);
// 只比较会影响几何的输入；收藏、材质等编辑不触发重算。
bool same_physics_input(const Snapshot &a,const Snapshot &b);
bool same_physics_geometry(const Snapshot &a,const Snapshot &b);
struct PhysicsResult {
  uint64_t serial=0,generation=0;
  std::shared_ptr<ir::Scene> scene;
  ir::Delta delta;
  ir::Delta restore;
  runtime::PhysicsStats stats;
  std::map<int,size_t> group_steps;
  std::map<uint32_t,uint64_t> mesh_hashes;
  std::map<uint32_t,ir::Transform> transforms;
  std::string error;
};
class PhysicsService {
  struct Request {uint64_t serial=0;std::shared_ptr<const Document> document;Snapshot snapshot;runtime::PhysicsOptions options;};
  std::mutex mutex_;
  std::condition_variable_any ready_;
  std::optional<Request> pending_;
  std::shared_ptr<PhysicsResult> result_;
  std::shared_ptr<PhysicsResult> acknowledged_;
  std::vector<std::shared_ptr<PhysicsResult>> retired_;
  std::atomic<uint64_t> serial_{0};
  std::atomic<bool> busy_{false};
  std::atomic<bool> frame_requested_{true},reset_{false};
  std::jthread worker_;
public:
  PhysicsService();
  ~PhysicsService();
  uint64_t request(std::shared_ptr<const Document> document,Snapshot snapshot,runtime::PhysicsOptions options);
  void cancel();
  void retire(std::shared_ptr<PhysicsResult> result);
  void acknowledge(std::shared_ptr<PhysicsResult> result);
  void request_frame(){frame_requested_=true;ready_.notify_all();}
  std::shared_ptr<PhysicsResult> take();
  bool busy() const {return busy_.load();}
};
}
