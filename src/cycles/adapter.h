#pragma once
#include "render_ir/scene.h"
#include <cstddef>
#include <vector>

namespace ccl {class Scene;class Shader;}
namespace dfv {
struct AdapterStats {size_t meshes=0,instances=0,materials=0,textures=0,unique_triangles=0,triangles=0,camera_updates=0,material_updates=0;};
class CyclesAdapter {
  ccl::Scene &scene_;
  std::vector<ccl::Shader *> shaders_;
  std::vector<ir::Texture> textures_;
  AdapterStats stats_;
  bool loaded_=false;
  void material(ccl::Shader &shader,const ir::Material &value);
public:
  explicit CyclesAdapter(ccl::Scene &scene):scene_(scene) {}
  void load(const ir::Scene &scene);
  // 调用者持有 Cycles Scene 锁；只同步 Delta 中声明的对象。
  void apply(const ir::Delta &delta);
  const AdapterStats &stats() const {return stats_;}
};
}
