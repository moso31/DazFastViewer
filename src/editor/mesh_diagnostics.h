#pragma once
#include "render_ir/scene.h"
#include <cmath>
#include <optional>
#include <span>

namespace dfv::editor {
// Vertex displacement is meaningful only when both arrays address the same
// vertices. A procedural remesh can also change identities without changing size.
inline std::optional<double> vertex_displacement(std::span<const ir::Vec3> reference,
    std::span<const ir::Vec3> evaluated,bool corresponding_vertices) {
  if(!corresponding_vertices||reference.size()!=evaluated.size())return std::nullopt;
  double maximum=0;
  for(size_t i=0;i<reference.size();++i){
    const double x=double(evaluated[i].x)-reference[i].x,y=double(evaluated[i].y)-reference[i].y,z=double(evaluated[i].z)-reference[i].z;
    maximum=std::max(maximum,std::sqrt(x*x+y*y+z*z));
  }
  return maximum;
}
}
