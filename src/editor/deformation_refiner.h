#pragma once
#include "editor/document.h"
#include "diagnostics/load_profile.h"
#include <condition_variable>
#include <thread>

namespace dfv::editor {
// One bounded worker: one running snapshot, one replaceable latest request and
// one result. It owns its scene/runtime; the viewport never shares mutable caches.
class DeformationRefiner {
public:
  struct Result {
    uint64_t ticket=0;
    ir::Delta geometry;
    runtime::CollisionStats collision;
    diagnostics::LoadProfile profile;
    std::string error;
    double seconds=0;
  };
private:
  struct Request {
    uint64_t ticket=0;
    std::shared_ptr<const Document> document;
    std::vector<runtime::Properties> values;
    std::vector<std::vector<runtime::JointPose>> poses;
    std::vector<ir::Transform> frames;
  };
  std::mutex mutex_;
  std::condition_variable_any wake_;
  uint64_t ticket_=0;
  std::optional<Request> pending_;
  std::unique_ptr<Result> result_;
  // Last member: join before destroying the mailbox and its synchronization.
  std::jthread worker_;
  void run(std::stop_token stop) {
    std::shared_ptr<const Document> document;
    std::unique_ptr<ir::Scene> scene;
    std::unique_ptr<runtime::DeformationRuntime> runtime;
    while(!stop.stop_requested()) {
      Request request;
      {std::unique_lock lock(mutex_);if(!wake_.wait(lock,stop,[&]{return pending_.has_value();}))break;request=std::move(*pending_);pending_.reset();}
      if(!request.document){runtime.reset();scene.reset();document.reset();continue;}
      auto result=std::make_unique<Result>();result->ticket=request.ticket;
      const auto begin=std::chrono::steady_clock::now();diagnostics::active=&result->profile;
      try {
        if(document!=request.document) {
          runtime.reset();document=request.document;scene=std::make_unique<ir::Scene>(document->loaded.scene);
          runtime=std::make_unique<runtime::DeformationRuntime>(*scene,document->catalog.targets,document->skeletons.skins,document->formulas.graphs);
        }
        if(!request.frames.empty()&&!runtime->reframe(request.frames))throw std::runtime_error("后台形变的组参考框架不一致");
        runtime->evaluate(request.values,request.poses);
        // Send absolute geometry, including unchanged meshes: intervening preview
        // revisions may have touched a mesh that this worker never evaluated.
        std::set<uint32_t> meshes;
        for(const auto &target:document->catalog.targets) {
          if(water::find(document->waters,target.id))continue;
          const auto &instance=scene->instances.at(target.instance);meshes.insert(instance.mesh);
          result->geometry.instances.push_back({target.instance,instance.transform});
        }
        for(const auto &instance:scene->instances)if(instance.shell_source>=0)meshes.insert(instance.mesh);
        for(auto mesh:meshes)result->geometry.meshes.push_back({mesh,scene->meshes[mesh].positions});
        result->collision=runtime->collision_stats();
      }catch(const std::exception &e){result->error=e.what();runtime.reset();document.reset();}
      diagnostics::active=nullptr;
      result->seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
      {std::lock_guard lock(mutex_);if(request.ticket==ticket_)result_=std::move(result);}
    }
  }
public:
  uint64_t request(std::shared_ptr<const Document> document,const Snapshot &snapshot,const std::vector<ir::Transform> &frames) {
    std::lock_guard lock(mutex_);const auto ticket=++ticket_;
    pending_=Request{ticket,std::move(document),snapshot.values,snapshot.poses,frames};result_.reset();
    if(!worker_.joinable())worker_=std::jthread([this](std::stop_token stop){run(stop);});
    wake_.notify_one();return ticket;
  }
  void cancel() {std::lock_guard lock(mutex_);++ticket_;pending_=Request{};result_.reset();wake_.notify_one();}
  std::unique_ptr<Result> take() {std::lock_guard lock(mutex_);return std::move(result_);}
};
}
