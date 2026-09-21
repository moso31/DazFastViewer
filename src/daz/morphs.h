#pragma once
#include "daz/loader.h"
#include "runtime/morph.h"
#include "daz/formulas.h"
namespace dfv::daz {
struct MorphCatalog {std::vector<runtime::Target> targets;nlohmann::json report;std::vector<FormulaSource> formulas;};
MorphCatalog discover_morphs(LoadedScene &loaded,const std::vector<std::filesystem::path> &content_roots);
}
