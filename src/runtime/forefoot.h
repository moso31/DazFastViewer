#pragma once
#include "runtime/skeleton.h"

namespace dfv::runtime {
struct ForefootRegion {
  char side=0;
  std::vector<float> blend,mass;
};
// 在静止鞋型中定义连续的前掌支撑区；同一范围用于 Morph、蒙皮与碰撞。
std::vector<ForefootRegion> forefoot_regions(const ir::Mesh &mesh);
void constrain_forefoot(const std::vector<ForefootRegion> &regions,const std::vector<ir::Vec3> &reference,
                        std::vector<ir::Vec3> &offsets,float strength);
// 左右前掌独立求平均，按原始面积采样，避免局部网格加密偏置结果。
class ForefootWeights {
  struct Region {std::vector<Influence> mean;std::vector<float> blend;};
  std::vector<Region> regions_;
public:
  ForefootWeights(const ir::Mesh &mesh,const Skin &skin);
  bool active() const {return !regions_.empty();}
  Skin apply(const Skin &skin,float strength) const;
};
bool supports_forefoot(const ir::Mesh &mesh,const Skin &skin);
}
