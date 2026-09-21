#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>

namespace dfv::daz {
struct LoadOptions {std::vector<std::filesystem::path> content_roots;bool strict=false;};
struct GeometrySource {std::filesystem::path file;std::string id;};
struct AssetObject {
  uint32_t instance=0;
  std::string id,label,parent,geometry_id;
  std::filesystem::path geometry_file;
  bool figure=false;
  std::string geometry_instance_id;
  std::string conform_target;
  std::vector<GeometrySource> geometry_sources;
  ir::MeshSmoothing smoothing;
};
struct AssetNode {std::string id,parent;};
struct LoadedScene {ir::Scene scene;nlohmann::json report;std::vector<AssetObject> objects;std::vector<AssetNode> nodes;};
struct DufContents {bool instantiate=false,materials=false,properties=false;};
DufContents inspect_contents(const nlohmann::json &document);
LoadedScene load(const std::filesystem::path &file,const LoadOptions &options={});
nlohmann::json read_document_file(const std::filesystem::path &file);
std::string decode_uri(const std::string &uri);
}
