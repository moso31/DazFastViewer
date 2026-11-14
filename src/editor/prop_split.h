#pragma once
#include "editor/document.h"

namespace dfv::editor {
struct PropSplitSource {
  std::vector<ir::Mesh::PropRegion> regions;
  std::vector<int> faces,curves;
  bool bones=false;
};
struct PropSplitPart {std::string label;std::vector<int> regions;};
// 固定外形后按来源面追溯区域，细分顶点无需保留整套蒙皮。
void capture_prop_regions(ir::Mesh &frozen,const ir::Mesh &source,const runtime::Skin *skin);
PropSplitSource prop_split_source(const ir::Mesh &mesh);
std::vector<PropSplitPart> prop_split_sides(const PropSplitSource &source);
std::vector<size_t> prop_split_counts(const PropSplitSource &source,const std::vector<PropSplitPart> &parts);
std::vector<size_t> split_static_prop(Document &,Snapshot &,size_t,const std::vector<PropSplitPart> &);
void restore_split_prop(Document &,Snapshot &,const nlohmann::json &);
nlohmann::json prop_mesh_json(const ir::Mesh &);
ir::Mesh read_prop_mesh(const nlohmann::json &);
}
