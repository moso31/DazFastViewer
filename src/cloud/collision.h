#pragma once
#include "render_ir/scene.h"
#include <span>

namespace dfv::cloud {
inline constexpr int hull_vertex_budget=24;
struct Hull {
  ir::Vec3 center;
  ir::Bounds bounds;
  std::vector<ir::Vec3> vertices;
  std::vector<ir::CloudPlane> planes;
};
// A low-resolution convex mesh built from actual evaluated vertices, never an
// ellipsoid or a box fallback. Coplanar meshes retain their zero thickness.
Hull convex_hull(std::span<const ir::Vec3> points);
Hull transformed_hull(const Hull &,const ir::Transform &);
}
