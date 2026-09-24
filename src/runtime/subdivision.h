#pragma once
#include "render_ir/scene.h"
#include <memory>
#include <span>

namespace dfv::runtime {
struct SubdivisionBoundary {uint32_t a=0,b=0;std::vector<uint32_t> vertices;};
// 在构建模板或触碰设备网格之前校验开销，供界面及渲染线程共同使用。
void validate_subdivision_budget(const ir::Mesh &mesh,int level);
// 缓存细分模板和 UV；Morph / Skinning 只重新插值位置，相机更新不接触细分。
class Subdivision {
  struct Data;
  std::shared_ptr<const Data> data_;
public:
  Subdivision()=default;
  Subdivision(const ir::Mesh &mesh,bool final_render=false,bool capture_boundaries=false);
  std::vector<ir::Vec3> evaluate(std::span<const ir::Vec3> cage) const;
  const std::vector<ir::Triangle> &triangles() const;
  // 诊断用：保留基础可见边界在最终细分层的顶点链，避免以基础顶点误代渲染接缝。
  const std::vector<SubdivisionBoundary> &boundaries() const;
  bool active() const {return bool(data_);}
};
}
