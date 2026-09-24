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
  std::vector<ir::Transform> transforms_;
  std::vector<bool> visible_;
  void rebuild(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,size_t i);
public:
  void update(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions);
  void apply(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,const ir::Delta &delta);
  void draw(const CameraState &camera,int width,int height,int hovered,int joint=-1,const std::vector<uint32_t> *members=nullptr);
  size_t triangle_count(int hovered,int joint=-1) const;
  void release();
  void draw_pose(const CameraState &camera,int width,int height,const ir::Mesh &proxy,const ir::Transform &world,
    const std::vector<std::pair<ir::Vec3,ir::Vec3>> &bones,ir::Vec3 goal);
};
}
