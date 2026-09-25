#pragma once
#include "render_ir/scene.h"
#include <memory>
#include <string>

namespace dfv::runtime {
struct WeightCalibration {double kg=0,kg_per_liter=0;std::string branch;};
WeightCalibration calibrate_weight(double liters,double height_cm);
struct PartWeight {
  std::string label;
  double liters=0,kg=0;
  size_t triangles=0,boundary_edges=0,nonmanifold_edges=0;
};
struct WeightResult {
  double height_cm=0,liters=0,kg=0,coefficient=0;
  std::string branch;
  size_t boundary_edges=0,nonmanifold_edges=0;
  std::vector<PartWeight> parts;
  std::vector<std::string> notes;
  std::vector<std::string> missing_morphs;
};
// 拓扑只预处理一次，后续复用三角形、面组与封口连通分量。
class WeightTopology {
  struct Data;
  std::shared_ptr<const Data> data_;
public:
  explicit WeightTopology(const ir::Mesh &mesh);
  WeightResult measure(const std::vector<ir::Vec3> &positions,const ir::Transform &world,bool character,double density=1000) const;
};
}
