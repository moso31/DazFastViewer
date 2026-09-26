#pragma once
#include "daz/loader.h"
#include <optional>
#include <limits>

namespace dfv::daz {
struct MaterialAnimation {
  std::string group,channel,property,target;
  nlohmann::json value;
};
// 材质预设常把真正的当前值保存在零帧动画中，material_library 只描述默认值。
inline std::optional<MaterialAnimation> material_animation(const nlohmann::json &animation){
  const auto url=decode_uri(animation.value("url",std::string{}));const auto marker=url.find("#materials");if(marker==std::string::npos)return {};
  const auto question=url.find('?',marker);if(question==std::string::npos)return {};
  MaterialAnimation result;result.target=url.substr(0,marker);auto group=url.substr(marker+10,question-marker-10);if(group.ends_with(':'))group.pop_back();if(group.starts_with('/'))group.erase(0,1);result.group=group.empty()?"*":group;
  auto path=url.substr(question+1);const auto channels=path.find("/channels/");if(channels!=std::string::npos)path.erase(0,channels+10);
  const auto modifications=path.find("/image_modification/");const auto slash=modifications==std::string::npos?path.rfind('/'):modifications;
  result.channel=slash==std::string::npos?std::string{}:path.substr(0,slash);result.property=slash==std::string::npos?path:path.substr(slash+1);const auto &keys=animation.value("keys",nlohmann::json::array());if(keys.empty())return {};
  const nlohmann::json *chosen=nullptr;double best=-std::numeric_limits<double>::infinity();
  for(const auto &key:keys)if(key.is_array()&&key.size()>=2&&key[0].is_number()){const double time=key[0].get<double>();if(!chosen)chosen=&key;if(time<=0&&time>=best){best=time;chosen=&key;}}
  if(!chosen)return {};result.value=(*chosen)[1];return result;
}
}
