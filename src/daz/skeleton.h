#pragma once
#include "daz/loader.h"
#include "runtime/skeleton.h"

namespace dfv::daz {
struct SkinCatalog {std::vector<runtime::Skin> skins;nlohmann::json report;};
SkinCatalog load_skeletons(const LoadedScene &loaded);
}
