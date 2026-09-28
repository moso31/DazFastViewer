#pragma once
#include "runtime/physics_settings.h"
#include "render_ir/scene.h"
#include <functional>
#include <memory>

namespace dfv::runtime {
struct PhysicsTarget {uint32_t instance=0;int host=-1;PhysicsKind kind=PhysicsKind::rigid;PhysicsObjectSettings settings;const ir::Mesh *attachment_rest=nullptr;};
struct PhysicsStats {size_t objects=0,particles=0,steps=0,active_objects=0;double prepare_ms=0,solve_ms=0;std::string warning;};
struct PhysicsOutput {ir::Delta delta;PhysicsStats stats;};
// 一个独立的角色（或普通物体组）持有一个世界。step 始终只推进固定的一小步。
// 参数变化只重置受影响的对象；每个对象运行指定轮数后保留状态。
class PhysicsSimulation {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  PhysicsSimulation();
  ~PhysicsSimulation();
  void update(std::shared_ptr<const ir::Scene> scene,const std::vector<PhysicsTarget> &targets,const PhysicsOptions &options,
              const std::vector<uint32_t> &colliders,const std::function<bool()> &cancel={});
  void options(const PhysicsOptions &options);
  void step(const std::function<bool()> &cancel={});
  PhysicsOutput output(const std::function<bool()> &cancel={}) const;
  PhysicsStats stats() const;
};
// 离线测试工具的便利函数；编辑器只使用上面的持续模拟接口。
PhysicsOutput settle_physics(const ir::Scene &scene,const std::vector<PhysicsTarget> &targets,const PhysicsOptions &options,const std::function<bool()> &cancel={});
}
