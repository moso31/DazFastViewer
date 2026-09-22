#pragma once
#include "daz/morphs.h"
#include "daz/skeleton.h"
#include "runtime/deformation.h"

namespace dfv::editor {
struct Document {
  uint64_t generation=0;
  uint64_t asset_revision=0;
  daz::LoadedScene loaded;
  daz::MorphCatalog catalog;
  daz::SkinCatalog skeletons;
  daz::FormulaCatalog formulas;
};
struct Snapshot {
  ir::RenderOptions options;
  uint64_t generation=0,revision=0;
  std::vector<runtime::Properties> values;
  std::vector<std::vector<runtime::JointPose>> poses;
  std::vector<ir::AreaLight> lights;
};
Snapshot initial_snapshot(const Document &document);
std::shared_ptr<Document> refresh_parameters(const Document &document,size_t selected,const std::vector<std::filesystem::path> &roots,const std::function<void(const std::string &)> &progress={});
void append_document(Document &destination,Document source);
size_t apply_materials(Document &document,size_t target,const daz::LoadedScene &preset);
void collect_resources(Document &document);
void release_load_data(Document &document);
size_t remove_target(Document &document,Snapshot &snapshot,size_t target);
size_t remove_light(Document &document,Snapshot &snapshot,size_t light);
}
