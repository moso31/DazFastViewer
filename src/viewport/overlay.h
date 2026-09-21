#pragma once
#include "render_ir/scene.h"
#include "bench/camera.h"
#include "runtime/picking.h"
#include <epoxy/gl.h>
#include <map>

namespace dfv {
// Beauty 显示之后的几何覆盖阶段；只使用呈现 context，不修改 Cycles 材质和采样状态。
class HoverOverlay {
  std::vector<GLuint> lists_;
  std::vector<size_t> triangle_counts_;
  std::vector<std::map<int,std::pair<GLuint,size_t>>> parts_;
public:
  void update(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions);
  void draw(const CameraState &camera,int width,int height,int hovered,int joint=-1);
  size_t triangle_count(int hovered,int joint=-1) const;
  void release();
};
}
