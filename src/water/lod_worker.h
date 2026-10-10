#pragma once
#include "water/model.h"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace dfv::water {
// 一个执行中的任务、一个可替换的最新请求。网格连接和波浪采样不占用视口线程。
class LodWorker {
public:
  struct Input {std::shared_ptr<const Water> water;uint32_t mesh;ir::Vec3 eye;};
  struct Result {uint64_t ticket=0;std::vector<std::pair<uint32_t,std::shared_ptr<const ir::Mesh>>> meshes;std::string error;double seconds=0;};
private:
  struct Request {uint64_t ticket;std::vector<Input> inputs;};
  std::mutex mutex_;
  std::condition_variable_any wake_;
  uint64_t ticket_=0;
  std::optional<Request> pending_;
  std::unique_ptr<Result> result_;
  std::jthread worker_;
  void run(std::stop_token stop){
    std::map<std::string,MeshCache> caches;
    while(!stop.stop_requested()){
      Request request;{std::unique_lock lock(mutex_);if(!wake_.wait(lock,stop,[&]{return pending_.has_value();}))break;request=std::move(*pending_);pending_.reset();}
      auto result=std::make_unique<Result>();result->ticket=request.ticket;const auto begin=std::chrono::steady_clock::now();
      try{
        std::erase_if(caches,[&](const auto &entry){return std::none_of(request.inputs.begin(),request.inputs.end(),[&](const auto &v){return v.water->id==entry.first;});});
        for(const auto &v:request.inputs){
          {std::lock_guard lock(mutex_);if(request.ticket!=ticket_||stop.stop_requested())break;}
          auto progress=[&](const std::string &){std::lock_guard lock(mutex_);if(request.ticket!=ticket_||stop.stop_requested())throw std::runtime_error("水体任务已取消");};
          result->meshes.emplace_back(v.mesh,cached_mesh(*v.water,v.eye,caches[v.water->id],progress));
        }
      }catch(const std::exception &e){result->error=e.what();}
      result->seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
      {std::lock_guard lock(mutex_);if(request.ticket==ticket_)result_=std::move(result);}
    }
  }
public:
  uint64_t request(std::vector<Input> inputs){
    std::lock_guard lock(mutex_);const auto ticket=++ticket_;pending_=Request{ticket,std::move(inputs)};result_.reset();
    if(!worker_.joinable())worker_=std::jthread([this](std::stop_token stop){run(stop);});wake_.notify_one();return ticket;
  }
  void cancel(){request({});}
  std::unique_ptr<Result> take(){std::lock_guard lock(mutex_);return std::move(result_);}
};
}
