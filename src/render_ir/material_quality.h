#pragma once
#include "render_ir/scene.h"
#include "viewport/render_quality.h"

namespace dfv::ir {
inline Material viewport_material(Material value,const RenderQuality &quality) {
  if(!quality.subsurface) value.subsurface=0;
  if(!quality.bump_and_normal) {value.bump_texture=-1;value.normal_texture=-1;}
  return value;
}
}
