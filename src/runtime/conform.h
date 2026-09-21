#pragma once
#include "runtime/formula.h"
#include "runtime/morph.h"
#include "runtime/skeleton.h"

namespace dfv::runtime {
struct SurfaceBinding {
  std::array<uint32_t,3> vertices{};ir::Vec3 barycentric;
  ir::Vec3 edge1,edge2,normal,offset_coordinates;
  uint32_t polygon=0;
};
struct ConformLink {
  size_t follower=0,source=0;
  int follower_skin=-1,source_skin=-1;
  std::vector<int> joints,morph_sources;
  std::vector<size_t> projected_morphs;
  std::vector<SurfaceBinding> surface;
  std::vector<std::vector<uint32_t>> neighbors;
  ir::Transform source_to_follower;
};
struct ConformStats {uint64_t bindings=0,authored_morphs=0,projected_vertices=0,evaluations=0;};
// 仅由 DSON conform_target 绑定实例；parent 本身不代表 Fit To。
// 绑定固定在静止网格上，传递的是蒙皮前位移，避免累积和二次蒙皮。
class ConformRuntime {
  const std::vector<Target> &targets_;
  std::vector<ConformLink> links_;
  std::vector<int> link_for_target_;
  std::vector<size_t> order_,vertex_counts_;
  std::vector<std::vector<float>> previous_weights_;
  std::vector<std::vector<ir::Vec3>> offsets_;
  std::vector<uint64_t> revisions_,source_revisions_;
  ConformStats stats_;
public:
  ConformRuntime(const ir::Scene &scene,const std::vector<Target> &targets,const std::vector<Skin> &skins,const std::vector<FormulaGraph> &graphs);
  const ConformLink *link(size_t target) const;
  const auto &order() const {return order_;}
  const auto &links() const {return links_;}
  const auto &stats() const {return stats_;}
  void project(const std::vector<std::vector<float>> &weights,MorphRuntime &morph);
};
// 保留穿戴物的局部编辑与额外骨骼，跟随骨骼使用角色最终 ERC 姿势。
std::vector<JointPose> conform_pose(const ConformLink &link,const std::vector<Skin> &skins,const std::vector<std::vector<JointPose>> &resolved,const std::vector<JointPose> &input);
struct CollisionStats {uint64_t evaluations=0,corrected_vertices=0;};
// 每次从未碰撞的蒙皮输出求值，不将上次修正反馈到 Morph 或蒙皮。
class CollisionRuntime {
  struct Binding {
    uint32_t follower=0,source=0;
    ir::MeshSmoothing settings;
    std::vector<uint32_t> grafts;
    std::vector<ir::Vec3> input;
    std::vector<std::vector<uint32_t>> neighbors;
    ir::Transform relative;
    bool initialized=false;
  };
  ir::Scene &scene_;
  std::vector<Binding> bindings_;
  CollisionStats stats_;
public:
  CollisionRuntime(ir::Scene &scene,const std::vector<Target> &targets);
  ir::Delta evaluate(ir::Delta delta);
  const auto &stats() const {return stats_;}
};
}
