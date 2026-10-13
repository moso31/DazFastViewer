#pragma once
#include "editor/document.h"

namespace dfv::editor {
enum class PresetKind {material,pose,morph};
nlohmann::json preset_json(const Document &,const Snapshot &,size_t target,PresetKind,const std::filesystem::path &destination);
void save_preset(const std::filesystem::path &,const Document &,const Snapshot &,size_t target,PresetKind);
}
