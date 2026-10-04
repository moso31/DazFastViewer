#pragma once
#include "render_ir/scene.h"
#include "runtime/rigid_follow.h"
#include <nlohmann/json.hpp>
#include <memory>
#include "daz/source_archive.h"

namespace dfv::daz {
struct LoadOptions {std::vector<std::filesystem::path> content_roots;bool strict=false;bool defer_selection=false;};
// 穿戴预设中的选择引用在追加到目标文档后解析，不当作文件内部 ID。
bool selection_reference(const std::string &uri);
struct GeometrySource {std::filesystem::path file;std::string id;};
struct MaterialSelectionSet {
  std::string name,parent;
  std::vector<std::string> materials;
};
struct AssetObject {
  uint32_t instance=0;
  std::string id,label,parent,geometry_id;
  std::filesystem::path geometry_file;
  bool figure=false;
  std::string geometry_instance_id;
  std::string conform_target;
  std::vector<GeometrySource> geometry_sources;
  ir::MeshSmoothing smoothing;
  std::filesystem::path source_file;
  std::string source_node;
  ir::Vec3 translation_cm{},rotation_degrees{},scale{1,1,1};
  float general_scale=1;
  std::vector<std::pair<std::filesystem::path,std::string>> geometry_versions;
  ir::Transform edit_frame,translation_frame;
  std::string rotation_order="XYZ";
  runtime::RigidFollow rigid_follow;
  std::string content_type,preferred_base,auto_fit_base;
  std::vector<std::string> extended_bases;
  bool attachment_bind_rest=false;
  std::vector<MaterialSelectionSet> material_selection_sets;
  std::shared_ptr<SourceArchive> archive;
};
struct AssetNode {
  std::string id,parent,label;bool group=false;
  ir::Transform world,edit_frame,translation_frame;
  ir::Vec3 translation_cm{},rotation_degrees{},scale{1,1,1};
  float general_scale=1;
  std::string rotation_order="XYZ";
  bool visible=true;
};
struct LoadedScene {ir::Scene scene;nlohmann::json report;std::vector<AssetObject> objects;std::vector<AssetNode> nodes;std::vector<std::shared_ptr<const nlohmann::json>> source_documents;std::shared_ptr<SourceArchive> archive;};
inline std::shared_ptr<SourceArchive> instance_archive(const LoadedScene &loaded,size_t instance){
  const auto &i=loaded.scene.instances.at(instance);if(i.prototype>=0)instance=size_t(i.prototype);else if(i.shell_source>=0)instance=size_t(i.shell_source);
  for(const auto &o:loaded.objects)if(o.instance==instance&&o.archive)return o.archive;return loaded.archive;
}
struct DufContents {bool instantiate=false,materials=false,properties=false,requires_selection=false;};
DufContents inspect_contents(const nlohmann::json &document);
LoadedScene load(const std::filesystem::path &file,const LoadOptions &options={});
ir::Material merge_material_preset(const ir::Material &base,const ir::Material &preset,std::vector<ir::Texture> &textures,const LoadOptions &options={});
void apply_graft_masks(LoadedScene &loaded,bool defer_selection=false);
void sync_instance_meshes(ir::Scene &scene);
nlohmann::json read_document_file(const std::filesystem::path &file);
std::string decode_uri(const std::string &uri);
}
