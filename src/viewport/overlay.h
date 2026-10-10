#pragma once
#include "render_ir/scene.h"
#include "bench/camera.h"
#include "runtime/picking.h"
#include "editor/gizmo.h"
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
  std::vector<bool> detail_pending_;
  std::vector<ir::Bounds> bounds_;
  std::vector<std::string> identities_;
  std::vector<uint64_t> geometry_keys_;
  uint64_t geometry_builds_=0;
  void draw_instance(size_t instance,GLuint list) const;
  void projection(const CameraState &camera,int width,int height,const ir::Mesh *proxy=nullptr,const ir::Transform *world=nullptr) const;
  void rebuild(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,size_t i,bool preview=false);
public:
  uint64_t geometry_builds()const{return geometry_builds_;}
  void draw_gizmo(const editor::GizmoShape &shape,int width,int height,int active=-1,float dpi=1);
  void update(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions);
  void apply(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,const ir::Delta &delta,bool preview=false);
  void prepare_delta(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,const ir::Delta &delta);
  void swap_delta(HoverOverlay &prepared,const ir::Scene &scene,const ir::Delta &delta);
  void draw(const CameraState &camera,int width,int height,int hovered,int joint=-1,const std::vector<uint32_t> *members=nullptr,const std::vector<std::pair<size_t,size_t>> *surfaces=nullptr);
  size_t surface_count(size_t instance,size_t slot) const;
  size_t triangle_count(int hovered,int joint=-1) const;
  void release();
  void draw_pose(const CameraState &camera,int width,int height,const ir::Mesh &proxy,const ir::Transform &world,
    const std::vector<std::pair<ir::Vec3,ir::Vec3>> &bones,ir::Vec3 goal,const std::vector<uint32_t> &excluded={});
};
}
