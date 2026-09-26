#pragma once

namespace dfv {
// 只改变视口渲染资源与着色，不写回 DAZ 材质或场景参数。
struct RenderQuality {
  int texture_limit=0;
  int transparent_bounces=32;
  bool subsurface=true;
  bool bump_and_normal=true;
  bool operator==(const RenderQuality &) const=default;
};
}
