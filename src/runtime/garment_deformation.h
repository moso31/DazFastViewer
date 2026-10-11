#pragma once
#include "render_ir/scene.h"
#include <set>

namespace dfv::runtime {
struct DeformationRegion {
  std::string id,label;
  std::vector<uint32_t> groups;
};
// 只列出实际有面的分组；左右对称部位共用开关，求值仍分开拟合。
std::vector<DeformationRegion> deformation_regions(const ir::Mesh &mesh);
class GarmentDeformation {
  std::vector<DeformationRegion> regions_;
  std::vector<std::vector<uint32_t>> groups_;
  std::vector<uint32_t> representatives_;
public:
  explicit GarmentDeformation(const ir::Mesh &mesh);
  // reference 已包含衣物自身的适配和手动 Morph。仅约束自动跟随位移。
  void constrain(const std::set<std::string> &disabled,const std::vector<ir::Vec3> &reference,
                 std::vector<ir::Vec3> &offsets,bool uniform_scale=false,bool transition=true) const;
};
}
