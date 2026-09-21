#include "runtime/morph_data.h"
#include <deque>
#include <thread>
#include <cmath>

namespace dfv::runtime {
namespace {
class PayloadWorkers {
  std::mutex mutex_;std::condition_variable changed_;bool stopped_=false;
  std::deque<std::function<void()>> work_;
  std::deque<std::pair<std::weak_ptr<MorphPayload>,size_t>> resident_;
  std::vector<std::jthread> threads_;size_t bytes_=0;
public:
  PayloadWorkers() {for(int i=0;i<4;++i) threads_.emplace_back([this] {for(;;) {
    std::function<void()> task;{std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return stopped_||!work_.empty();});if(stopped_&&work_.empty()) return;task=std::move(work_.front());work_.pop_front();}task();
  }});}
  ~PayloadWorkers() {{std::lock_guard lock(mutex_);stopped_=true;}changed_.notify_all();for(auto &thread:threads_) thread.join();}
  void push(std::function<void()> task) {{std::lock_guard lock(mutex_);work_.push_back(std::move(task));}changed_.notify_one();}
  void trim() {
    std::lock_guard lock(mutex_);size_t attempts=resident_.size();
    while(bytes_>512*1024*1024&&attempts--&&!resident_.empty()) {
      auto entry=resident_.front();resident_.pop_front();auto old=entry.first.lock();
      if(!old||old->state()==PayloadState::unloaded||old->evict()) bytes_-=entry.second;else resident_.push_back(entry);
    }
  }
  void resident(const std::shared_ptr<MorphPayload> &payload) {
    std::lock_guard lock(mutex_);const size_t size=payload->count*sizeof(SparseOffset);
    for(auto i=resident_.begin();i!=resident_.end();) {const auto p=i->first.lock();if(!p||p==payload) {bytes_-=i->second;i=resident_.erase(i);}else ++i;}bytes_+=size;
    // 512 MiB 是可淘汰缓存预算；当前求值持有的缓冲区允许超出预算。
    size_t attempts=resident_.size();while(bytes_>512*1024*1024&&attempts--&&!resident_.empty()) {
      auto entry=resident_.front();resident_.pop_front();auto old=entry.first.lock();
      if(!old||old->evict()) bytes_-=entry.second;else resident_.push_back(entry);
    }
    resident_.push_back({payload,size});
  }
};
PayloadWorkers &workers() {static PayloadWorkers instance;return instance;}
}
void trim_morph_cache() {workers().trim();}
void MorphPayload::request(bool retry) {
  {std::lock_guard lock(mutex_);if(state_!=PayloadState::unloaded&&!(retry&&state_==PayloadState::failed)) return;state_=PayloadState::loading;error_.clear();}
  workers().push([self=shared_from_this()]{self->run();});
}
void MorphPayload::run() {
  try {
    auto result=loader_();if(result.size()!=count) throw std::runtime_error("Morph 差值数量与目录不一致");
    std::vector<uint8_t> seen(vertices);
    for(const auto &d:result) {
      if(d.vertex>=vertices||seen[d.vertex]||!std::isfinite(d.delta.x)||!std::isfinite(d.delta.y)||!std::isfinite(d.delta.z)) throw std::runtime_error("Morph 差值索引、重复记录或数值无效");seen[d.vertex]=1;
    }
    {std::lock_guard lock(mutex_);data_=std::make_shared<const OffsetBuffer>(std::move(result));state_=PayloadState::ready;}
    workers().resident(shared_from_this());
  } catch(const std::exception &e) {std::lock_guard lock(mutex_);data_.reset();state_=PayloadState::failed;error_=e.what();}
  changed_.notify_all();
}
std::shared_ptr<const OffsetBuffer> MorphPayload::acquire() const {std::lock_guard lock(mutex_);return data_;}
std::shared_ptr<const OffsetBuffer> MorphPayload::ensure() {
  for(;;) {request();std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return state_!=PayloadState::loading;});
    if(state_==PayloadState::failed) throw std::runtime_error(error_);if(data_) return data_;}
}
PayloadState MorphPayload::state() const {std::lock_guard lock(mutex_);return state_;}
std::string MorphPayload::error() const {std::lock_guard lock(mutex_);return error_;}
size_t MorphPayload::evict() {std::lock_guard lock(mutex_);if(!data_||data_.use_count()!=1) return 0;const auto bytes=data_->size()*sizeof(SparseOffset);data_.reset();state_=PayloadState::unloaded;return bytes;}
}
