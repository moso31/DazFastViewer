#pragma once
#include "render_ir/scene.h"
#include "runtime/subdivision.h"
#include "runtime/graft_surface.h"
#include <cstddef>
#include <vector>

namespace ccl {class Scene;class Shader;class Mesh;class Hair;class Object;class Light;class BackgroundLight;}
namespace dfv {
struct AdapterStats {size_t meshes=0,instances=0,materials=0,textures=0,unique_triangles=0,triangles=0,camera_updates=0,material_updates=0,geometry_updates=0,instance_updates=0,curves=0,topology_updates=0,scene_updates=0;};
class CyclesAdapter {
  ccl::Scene &scene_;
  std::vector<ccl::Shader *> shaders_;
  std::vector<ir::Texture> textures_;
  std::vector<float> bump_distances_;
  std::vector<std::vector<ccl::Mesh *>> meshes_;
  struct HairBinding {ccl::Hair *hair;std::vector<uint32_t> vertices;};
  std::vector<std::vector<HairBinding>> hairs_;
  std::vector<size_t> vertex_counts_;
  std::vector<runtime::Subdivision> subdivisions_;
  struct GraftBinding {int group=-1;size_t part=0;};
  std::vector<runtime::GraftSurface> grafts_;
  std::vector<GraftBinding> graft_bindings_;
  struct GraftRender {
    std::string id,geometry_key;
    size_t group=0;
    std::vector<int> parts;
    std::vector<ccl::Shader *> shaders;
    std::vector<bool> visible;
    ccl::Mesh *mesh=nullptr;
    ccl::Object *object=nullptr;
  };
  std::vector<GraftRender> graft_renders_;
  bool final_render_=false;
  std::vector<std::vector<ccl::Object *>> objects_;
  std::vector<ccl::Object *> light_objects_;
  std::vector<ccl::Light *> lights_;
  ccl::BackgroundLight *background_light_=nullptr;
  AdapterStats stats_;
  ir::RenderOptions options_;
  ir::Vec3 environment_;
  std::vector<ir::Vec3> light_power_;
  void environment(const ir::RenderOptions &options);
  bool loaded_=false;
  ir::Scene source_;
  std::vector<ir::Material> canonical_materials_;
  std::vector<ccl::Shader *> retired_shaders_;
  std::vector<int> texture_map_;
  void material(ccl::Shader &shader,const ir::Material &value,float texel_distance=0);
public:
  explicit CyclesAdapter(ccl::Scene &scene,bool final_render=false):scene_(scene),final_render_(final_render) {}
  void load(const ir::Scene &scene);
  // 稳定 ID 匹配资源，保留未变化的节点、纹理与设备状态；调用者持有 Scene 锁。
  bool synchronize(const ir::Scene &scene);
  // 调用者持有 Cycles Scene 锁；只同步 Delta 中声明的对象。
  void apply(const ir::Delta &delta);
  const AdapterStats &stats() const {return stats_;}
};
}
