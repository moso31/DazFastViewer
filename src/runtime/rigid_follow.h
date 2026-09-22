#pragma once
#include "render_ir/scene.h"

namespace dfv::runtime {
struct RigidFollow {
  std::string target;
  size_t vertex_count=0;
  std::vector<uint32_t> vertices;
  bool rotate=true;
};
// 参考顶点的最小二乘刚性变换，禁止把表面缩放 / 剪切传到饰品本身。
ir::Transform fit_rigid(const std::vector<ir::Vec3> &reference,const std::vector<ir::Vec3> &current,bool rotate=true);
}
