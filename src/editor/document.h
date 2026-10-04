#pragma once
#include "daz/morphs.h"
#include "daz/skeleton.h"
#include "runtime/deformation.h"
#include "runtime/pose_edit.h"
#include "editor/materials.h"
#include "editor/instance_ground.h"
#include "daz/material_uv.h"
#include "city/model.h"
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
  std::map<std::string,std::shared_ptr<daz::SourceArchive>> archives;
  std::map<std::pair<std::string,std::string>,std::shared_ptr<daz::SourceArchive>> material_archives;
  std::filesystem::path source_file;
  nlohmann::json operations=nlohmann::json::array();
  uint64_t generation=0;
  uint64_t asset_revision=0;
  daz::LoadedScene loaded;
  daz::MorphCatalog catalog;
  daz::SkinCatalog skeletons;
  daz::FormulaCatalog formulas;
  std::vector<AttachmentBinding> attachments;
  city::Cities cities;
};
struct Snapshot {
  std::optional<std::array<float,6>> view;
  city::Views city_views;
  std::map<std::string,runtime::TransformValues> group_transforms;
  InstanceGrounds instance_ground;
  MaterialOverrides material_overrides;
  std::optional<runtime::FavoriteState> control_favorites;
  // 按稳定网格身份保存，追加／删除对象不会把级别套到其他对象。
  std::map<std::string,int> subdivision_levels;
  ir::RenderOptions options;
  uint64_t generation=0,revision=0;
  std::vector<runtime::Properties> values;
  std::vector<std::vector<runtime::JointPose>> poses;
  std::vector<ir::AreaLight> lights;
  std::vector<runtime::PosePin> pose_pins;
};
Snapshot initial_snapshot(const Document &document);
std::string retain_archive(Document &document,const std::shared_ptr<daz::SourceArchive> &archive);
std::shared_ptr<daz::SourceArchive> material_archive(const Document &,MaterialSurface,const std::string &owner);
const ir::Mesh &subdivision_mesh(const Document &document,size_t target);
int subdivision_level(const Document &document,const Snapshot &snapshot,size_t target);
bool apply_subdivision_levels(ir::Scene &scene,const std::map<std::string,int> &levels);
std::shared_ptr<Document> refresh_parameters(const Document &document,size_t selected,const std::vector<std::filesystem::path> &roots,const std::function<void(const std::string &)> &progress={});
void append_document(Document &destination,Document source,const std::string &identity_prefix={});
// 以下操作作用于调用方的待提交副本；失败时不发布该副本。
size_t attachment_host(const Document &document,size_t selected);
void attach_import(Document &document,size_t first_target,size_t host);
void fit_attachment(Document &document,size_t follower,int host);
size_t apply_materials(Document &document,size_t target,const daz::LoadedScene &preset,Snapshot *snapshot=nullptr);
size_t apply_surface_materials(Document &document,Snapshot &snapshot,const daz::LoadedScene &preset,const std::vector<MaterialSurface> &surfaces);
std::vector<std::string> paste_material(Document &document,Snapshot &snapshot,const nlohmann::json &copy,const std::vector<MaterialSurface> &surfaces);
bool change_material_uv(Document &document,const daz::MaterialUVSet &uv,const std::vector<MaterialSurface> &surfaces);
bool reset_material_uv(Document &document,const std::vector<MaterialSurface> &surfaces);
void collect_resources(Document &document);
void release_load_data(Document &document);
size_t remove_target(Document &document,Snapshot &snapshot,size_t target);
size_t remove_light(Document &document,Snapshot &snapshot,size_t light);
}
