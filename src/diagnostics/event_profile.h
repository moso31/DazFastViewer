#pragma once
#include "diagnostics/load_profile.h"
#include <functional>

namespace dfv::diagnostics {
// 仅专项诊断启用；沿用线程本地 Scope，异常退出时也恢复原采集器。
class EventProfile {
  LoadProfile profile_;
  LoadProfile *previous_=active;
  bool enabled_;
  std::function<void(const char *,double)> emit_;
public:
  EventProfile(bool enabled,std::function<void(const char *,double)> emit):enabled_(enabled),emit_(std::move(emit)) {if(enabled_) active=&profile_;}
  ~EventProfile() {
    if(!enabled_) return;
    active=previous_;
    for(const auto &[name,value]:profile_.timings) emit_(("rebuild/"+name).c_str(),value.seconds*1000);
  }
  EventProfile(const EventProfile &)=delete;
  EventProfile &operator=(const EventProfile &)=delete;
};
}
