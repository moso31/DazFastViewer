#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>

namespace dfv::daz {
struct LoadOptions {std::vector<std::filesystem::path> content_roots;bool strict=false;};
struct LoadedScene {ir::Scene scene;nlohmann::json report;};
LoadedScene load(const std::filesystem::path &file,const LoadOptions &options={});
}
