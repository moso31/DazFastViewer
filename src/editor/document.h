#pragma once
#include "daz/morphs.h"
#include "daz/skeleton.h"
#include "runtime/deformation.h"
#include <map>

namespace dfv::editor {
struct AttachmentItem {
  std::string node,parent,conform,collision,rigid;
  ir::Transform transform,edit_frame,translation_frame;
};
struct AttachmentBinding {
  std::string host;
  std::vector<AttachmentItem> items;
  std::vector<daz::AssetNode> nodes;
};
struct Document {
  uint64_t generation=0;
  uint64_t asset_revision=0;
  daz::LoadedScene loaded;
  daz::MorphCatalog catalog;
  daz::SkinCatalog skeletons;
  daz::FormulaCatalog formulas;
  std::vector<AttachmentBinding> attachments;
};
struct Snapshot {
  // 按稳定网格身份保存，追加／删除对象不会把级别套到其他对象。
  std::map<std::string,int> subdivision_levels;
  ir::RenderOptions options;
  uint64_t generation=0,revision=0;
  std::vector<runtime::Properties> values;
  std::vector<std::vector<runtime::JointPose>> poses;
  std::vector<ir::AreaLight> lights;
};
Snapshot initial_snapshot(const Document &document);
int subdivision_level(const Document &document,const Snapshot &snapshot,size_t target);
bool apply_subdivision_levels(ir::Scene &scene,const std::map<std::string,int> &levels);
std::shared_ptr<Document> refresh_parameters(const Document &document,size_t selected,const std::vector<std::filesystem::path> &roots,const std::function<void(const std::string &)> &progress={});
void append_document(Document &destination,Document source);
// 以下操作作用于调用方的待提交副本；失败时不发布该副本。
size_t attachment_host(const Document &document,size_t selected);
void attach_import(Document &document,size_t first_target,size_t host);
void fit_attachment(Document &document,size_t follower,int host);
size_t apply_materials(Document &document,size_t target,const daz::LoadedScene &preset);
void collect_resources(Document &document);
void release_load_data(Document &document);
size_t remove_target(Document &document,Snapshot &snapshot,size_t target);
size_t remove_light(Document &document,Snapshot &snapshot,size_t light);
}
