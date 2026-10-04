#pragma once
#include "daz/loader.h"
#include <optional>
namespace dfv::daz {
struct MaterialUVSet {
  std::string uri,owner,label;
  bool operator==(const MaterialUVSet &) const = default;
};
struct MaterialUVCatalog {
  std::vector<MaterialUVSet> sets;
  std::vector<std::string> warnings;
};
MaterialUVSet material_uv_set(const ir::Material &material);
std::optional<MaterialUVSet> material_uv_baseline(const ir::Material &material);
void set_material_uv_set(ir::Material &material,const MaterialUVSet &uv);
// Connectivity, including polygon order, is required for indexed DAZ UV seams.
std::string material_uv_topology(const ir::Mesh &mesh);
MaterialUVCatalog discover_material_uv_sets(const LoadedScene &loaded,size_t instance,const LoadOptions &options,const std::vector<std::shared_ptr<SourceArchive>> &surface_archives={});
bool apply_material_uv(ir::Scene &scene,size_t instance,size_t slot,const ir::Material &preset,const LoadOptions &options);
}
