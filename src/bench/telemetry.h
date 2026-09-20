#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>

namespace dfv {
inline double now() {
  static const auto origin=std::chrono::steady_clock::now();
  return std::chrono::duration<double>(std::chrono::steady_clock::now()-origin).count();
}
struct Frame {
  uint64_t id=0, epoch=0;
  int samples=0, width=0, height=0;
  double produced=0;
};
class Telemetry {
  std::mutex mutex_;
  std::ofstream events_;
  std::ofstream swaps_;
public:
  std::atomic<uint64_t> produced{0}, submitted{0}, displayed_epoch{0}, readback_bytes{0}, skipped{0};
  std::atomic<double> first_frame{-1}, measurement_start{-1}, measurement_end{-1};
  std::atomic<uint64_t> measured_frames{0};
  explicit Telemetry(const std::filesystem::path &directory) {
    std::filesystem::create_directories(directory);
    events_.open(directory/"events.csv");
    if(!events_) throw std::runtime_error("不能写入性能日志");
    events_<<"event,seconds,frame_id,camera_epoch,samples,width,height,value_ms\n";
    events_<<std::fixed<<std::setprecision(6);
    swaps_.open(directory/"swaps.csv");
    if(!swaps_) throw std::runtime_error("不能写入呈现日志");
    swaps_<<"swap_id,qpc_begin,qpc_end,frame_id,camera_epoch\n";
  }
  void swap(uint64_t id,int64_t begin,int64_t end,const Frame &frame) {
    swaps_<<id<<','<<begin<<','<<end<<','<<frame.id<<','<<frame.epoch<<'\n';
  }
  void event(const char *kind,const Frame &frame={},double duration_ms=0) {
    std::lock_guard lock(mutex_);
    events_<<kind<<','<<now()<<','<<frame.id<<','<<frame.epoch<<','<<frame.samples<<','
           <<frame.width<<','<<frame.height<<','<<duration_ms<<'\n';
  }
  void present(const Frame &frame) {
    submitted.fetch_add(1);
    if(first_frame.load()<0) first_frame.store(now());
    const auto previous=displayed_epoch.exchange(frame.epoch);
    const double begin=measurement_start.load(),end=measurement_end.load(),time=now();
    if(previous!=frame.epoch && begin>=0 && time>=begin && (end<0 || time<end)) measured_frames++;
    event("present_submitted",frame);
  }
};
}
