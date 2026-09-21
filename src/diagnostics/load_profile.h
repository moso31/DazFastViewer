#pragma once
#include <chrono>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// 仅诊断入口设置 active；正常编辑器不收集数据，也不改变加载语义。
namespace dfv::diagnostics {
using Clock=std::chrono::steady_clock;
struct Timing {double seconds=0,self_seconds=0;size_t calls=0;};
struct FileTiming {double seconds=0;size_t calls=0,bytes=0,unpacked_bytes=0;};
struct LoadProfile {
  std::map<std::string,Timing> timings;
  std::map<std::string,FileTiming> files;
};
inline thread_local LoadProfile *active=nullptr;
class Scope;
inline thread_local Scope *parent=nullptr;
class Scope {
  LoadProfile *profile_=active;
  Scope *parent_=nullptr;
  Clock::time_point begin_;
  std::string path_;
  double children_=0;
public:
  explicit Scope(std::string_view name) {
    if(!profile_) return;
    parent_=parent;if(parent_) {path_=parent_->path_;path_+='/';}path_+=name;
    begin_=Clock::now();parent=this;
  }
  ~Scope() {
    if(!profile_) return;
    const double elapsed=std::chrono::duration<double>(Clock::now()-begin_).count();
    auto &entry=profile_->timings[path_];entry.seconds+=elapsed;entry.self_seconds+=elapsed-children_;++entry.calls;
    if(parent_) parent_->children_+=elapsed;
    parent=parent_;
  }
  Scope(const Scope &)=delete;
  Scope &operator=(const Scope &)=delete;
};
struct FileScope {
  LoadProfile *profile=active;
  Clock::time_point begin;
  std::string path;
  size_t bytes=0,unpacked_bytes=0;
  explicit FileScope(std::string name):path(std::move(name)) {if(profile) begin=Clock::now();}
  ~FileScope() {
    if(!profile) return;
    auto &entry=profile->files[path];++entry.calls;entry.bytes+=bytes;entry.unpacked_bytes+=unpacked_bytes;
    entry.seconds+=std::chrono::duration<double>(Clock::now()-begin).count();
  }
};
}
