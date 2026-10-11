#pragma once
#include "render_ir/scene.h"

namespace dfv::runtime {
struct MeshPush {
  float offset_m=0;
  bool post_smooth=false;
  // 空数组表示没有权重图；显式权重图中未列出的顶点权重为零。
  std::vector<float> weights;
};
void apply_mesh_push(const ir::Mesh &mesh,std::vector<ir::Vec3> &positions,const std::vector<MeshPush> &modifiers,bool post_smooth);
}
