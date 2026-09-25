#pragma once
#include <map>
#include <set>
#include <string>
#include <optional>
namespace dfv::runtime {
struct FavoriteState {
  // 对象内以骨骼 ID 分组，空键代表对象本身；false 明确表示取消收藏。
  std::map<std::string,std::map<std::string,bool>> nodes;
  std::set<std::string> fallback;
  bool operator==(const FavoriteState &) const = default;
};
}
