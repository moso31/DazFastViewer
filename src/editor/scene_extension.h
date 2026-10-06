#pragma once
#include "editor/document.h"

namespace dfv::editor {
std::filesystem::path extension_path(std::filesystem::path source);
nlohmann::json snapshot_json(const Document &document,const Snapshot &snapshot,bool water_state=true);
// 先验证完整覆盖，再一次替换快照；错误不会留下部分应用的结果。
void apply_snapshot_json(const Document &document,Snapshot &snapshot,const nlohmann::json &data);
void save_scene_extension(const std::filesystem::path &file,const Document &document,const Snapshot &snapshot);
// Recovery envelope retains v1 editor state (including existing PCG/physics settings).
// Disk scene saves upgrade this envelope to v2 and omit those excluded systems.
nlohmann::json scene_extension_json(const Document &document,const Snapshot &snapshot);
// PCG keeps its existing v1 workflow until it has a v2 persistence contract.
bool legacy_scene_extension(const Document &document);
struct RestoredScene {std::shared_ptr<Document> document;Snapshot snapshot;};
RestoredScene restore_scene_extension(const nlohmann::json &data,const std::filesystem::path &folder,const std::vector<std::filesystem::path> &roots,uint64_t generation,const std::function<void(const std::string &)> &progress={});
RestoredScene load_scene_extension(const std::filesystem::path &file,const std::vector<std::filesystem::path> &roots,uint64_t generation,const std::function<void(const std::string &)> &progress={});
}
