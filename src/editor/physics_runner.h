#pragma once
#include "runtime/physics.h"
#include <condition_variable>
#include <future>
#include <thread>
#include <atomic>
#include <optional>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace dfv::editor {
// 每个角色一个常驻工作线程。重建甲的约束或展开甲的显示网格时，乙仍继续小步模拟。
class PhysicsRunner {
  struct Command {uint64_t serial;std::function<void(runtime::PhysicsSimulation &,const std::function<bool()> &)> run;};
  std::mutex mutex_;
  std::condition_variable_any ready_;
  std::optional<Command> command_;
  std::atomic<uint64_t> serial_{0};
  runtime::PhysicsStats stats_;
  std::string error_;
  std::jthread worker_;
  template<class F> auto invoke(F function,const std::function<bool()> &cancel={}) {
    using T=std::invoke_result_t<F,runtime::PhysicsSimulation &,const std::function<bool()> &>;
    auto promise=std::make_shared<std::promise<T>>();auto future=promise->get_future();const auto serial=++serial_;
    {std::lock_guard lock(mutex_);command_=Command{serial,[function=std::move(function),promise](auto &simulation,const auto &cancelled){
      try{if constexpr(std::is_void_v<T>){function(simulation,cancelled);promise->set_value();}else promise->set_value(function(simulation,cancelled));}catch(...){promise->set_exception(std::current_exception());}
    }};}
    ready_.notify_all();
    while(future.wait_for(std::chrono::milliseconds(5))!=std::future_status::ready)if(cancel&&cancel()){++serial_;throw std::runtime_error("物理输入已更新");}
    return future.get();
  }
public:
  PhysicsRunner():worker_([this](std::stop_token stop){
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
#endif
    using Clock=std::chrono::steady_clock;runtime::PhysicsSimulation simulation;auto next=Clock::now();
    while(!stop.stop_requested()){
      std::optional<Command> command;
      {std::unique_lock lock(mutex_);ready_.wait_until(lock,stop,next,[&]{return command_.has_value();});if(stop.stop_requested())break;command=std::move(command_);command_.reset();}
      if(command){auto cancelled=[&]{return stop.stop_requested()||serial_.load()!=command->serial;};command->run(simulation,cancelled);}
      if(Clock::now()>=next){
        try{simulation.step([&]{return stop.stop_requested();});}
        catch(const std::exception &e){std::lock_guard lock(mutex_);error_=e.what();}
        next=Clock::now()+std::chrono::milliseconds(16);
      }
      {std::lock_guard lock(mutex_);stats_=simulation.stats();}
    }
  }){}
  ~PhysicsRunner(){worker_.request_stop();++serial_;ready_.notify_all();}
  void update(std::shared_ptr<const ir::Scene> scene,std::vector<runtime::PhysicsTarget> targets,runtime::PhysicsOptions options,std::vector<uint32_t> colliders,const std::function<bool()> &cancel){
    invoke([scene=std::move(scene),targets=std::move(targets),options,colliders=std::move(colliders)](auto &simulation,const auto &cancelled){simulation.update(scene,targets,options,colliders,cancelled);},cancel);
    std::lock_guard lock(mutex_);error_.clear();
  }
  void options(runtime::PhysicsOptions options,const std::function<bool()> &cancel){invoke([options](auto &simulation,const auto &){simulation.options(options);},cancel);}
  runtime::PhysicsStats stats(){std::lock_guard lock(mutex_);return stats_;}
  runtime::PhysicsOutput output(const std::function<bool()> &cancel){
    {std::lock_guard lock(mutex_);if(!error_.empty())throw std::runtime_error(error_);}
    return invoke([](auto &simulation,const auto &cancelled){return simulation.output(cancelled);},cancel);
  }
};
}
