#pragma once
#include "daz/loader.h"
#include "runtime/morph.h"
namespace dfv::daz {
struct MorphCatalog {std::vector<runtime::Target> targets;nlohmann::json report;};
MorphCatalog discover_morphs(LoadedScene &loaded,const std::vector<std::filesystem::path> &content_roots);
}
