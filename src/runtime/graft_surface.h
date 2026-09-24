#pragma once
#include "runtime/subdivision.h"

namespace dfv::runtime {
bool same_mesh_topology(const ir::Mesh &a,const ir::Mesh &b);
uint32_t graft_root(const ir::Scene &scene,uint32_t instance);
// 每组根宿主在前，嵌套附件跟在自己的宿主之后；DAZ Instance 复用原型组。
std::vector<std::vector<uint32_t>> graft_groups(const ir::Scene &scene);

class GraftSurface {
  struct Topology;
  struct Evaluated;
  std::shared_ptr<const Topology> topology_;
  std::shared_ptr<const Evaluated> evaluated_;
  std::vector<uint32_t> members_;
public:
  GraftSurface(const ir::Scene &scene,std::vector<uint32_t> members,bool final_render);
  bool compatible(const ir::Scene &next,const ir::Scene &old,const std::vector<uint32_t> &members) const;
  void rebind(std::vector<uint32_t> members) {members_=std::move(members);}
  // 只有控制网格改变才重新插值；共同平移、显隐和相机变化不重建模板。
  bool evaluate(const ir::Scene &scene);
  const auto &members() const {return members_;}
  const std::vector<ir::Triangle> &triangles(size_t part) const;
  const std::vector<uint32_t> &vertices(size_t part) const;
  const std::vector<ir::Vec3> &positions() const;
  const std::vector<ir::Vec3> &normals() const;
  ir::Transform transform(const ir::Scene &scene,uint32_t instance) const;
};
}
