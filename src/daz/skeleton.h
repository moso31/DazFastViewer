#pragma once
#include "daz/loader.h"
#include "runtime/skeleton.h"

namespace dfv::daz {
struct SkinCatalog {
  std::vector<runtime::Skin> skins;
  // 与 skins 同序，保留所属基础骨架中的节点公式，不能只扫描 Morph 文件。
  std::vector<nlohmann::json> node_formulas;
  nlohmann::json report;
};
SkinCatalog load_skeletons(const LoadedScene &loaded);
}
