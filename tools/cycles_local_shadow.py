"""局部阴影查询的可重现 Cycles 补丁；不修改上游 checkout。"""


def apply(root, files, replace):
    for target, source in (
        ("src/kernel/device/optix/dfv_local_shadow_types.h", "local_shadow_types.h"),
        ("src/kernel/device/optix/dfv_local_shadow.h", "local_shadow_kernel.h"),
        ("src/device/optix/dfv_local_shadow.inl", "local_shadow_host.inl"),
        ("src/device/optix/dfv_local_shadow_tree.h", "local_shadow_tree.h"),
    ):
        content = (root / "src/cycles" / source).read_text(encoding="utf-8")
        content = content.replace('"local_shadow_types.h"', '"kernel/device/optix/dfv_local_shadow_types.h"')
        files[target] = content.encode("ascii", "backslashreplace")
    replace("src/kernel/device/optix/CMakeLists.txt", "set(SRC_KERNEL_DEVICE_OPTIX_HEADERS", "set(SRC_KERNEL_DEVICE_OPTIX_HEADERS\n  dfv_local_shadow_types.h\n  dfv_local_shadow.h")
    replace("src/scene/object.h", "  BoundBox bounds;", "  BoundBox bounds;\n  NODE_SOCKET_API(bool, dfv_local_shadow)")
    replace("src/scene/object.cpp", '  SOCKET_BOOLEAN(use_holdout, "Use Holdout", false);',
            '  SOCKET_BOOLEAN(dfv_local_shadow, "Local Shadow Queries", false);\n  SOCKET_BOOLEAN(use_holdout, "Use Holdout", false);')
    replace("src/scene/object.cpp", "    if (visibility_is_modified()) {", "    if (visibility_is_modified() || dfv_local_shadow_is_modified()) {")
    replace("src/kernel/device/optix/globals.h", '#include "kernel/types.h"',
            '#include "kernel/types.h"\n#include "kernel/device/optix/dfv_local_shadow_types.h"')
    replace("src/kernel/device/optix/globals.h", "  void *osl_colorsystem;", """  void *osl_colorsystem;
  const DfvLocalShadowObject *dfv_shadow_objects;
  const DfvLocalShadowNode *dfv_shadow_nodes;
  int dfv_shadow_node_count;""")
    replace("src/device/optix/device_impl.h", '#  include "util/task.h"',
            '#  include "util/task.h"\n#  include "kernel/device/optix/dfv_local_shadow_types.h"')
    replace("src/device/optix/device_impl.h", "  OptixTraversableHandle tlas_handle = 0;", """  OptixTraversableHandle tlas_handle = 0;
  uint64_t dfv_blas_revision = 0;
  struct DfvShadowCache {
    OptixInstance instance = {};
    float bounds[6] = {};
    uint64_t revision = 0;
    unique_ptr<BVHOptiX> bvh;
  };
  vector<DfvShadowCache> dfv_shadow_cache;
  device_vector<DfvLocalShadowObject> dfv_shadow_objects;
  device_vector<DfvLocalShadowNode> dfv_shadow_nodes;
  void dfv_update_local_shadows(BVH *, const device_vector<OptixInstance> &);""")
    replace("src/bvh/optix.h", "  uint64_t traversable_handle;", "  uint64_t traversable_handle;\n  uint64_t dfv_revision = 0;")
    replace("src/device/optix/device_impl.cpp", "  bool use_fast_trace_bvh = (bvh->params.bvh_type == BVH_TYPE_STATIC);", """  if (!bvh->params.top_level) bvh->dfv_revision = ++dfv_blas_revision;
  bool use_fast_trace_bvh = (bvh->params.bvh_type == BVH_TYPE_STATIC);""")
    replace("src/device/optix/device_impl.cpp", '      launch_params(this, "kernel_params", false)',
            '      launch_params(this, "kernel_params", false),\n      dfv_shadow_objects(this, "local shadow objects", MEM_READ_ONLY),\n      dfv_shadow_nodes(this, "local shadow nodes", MEM_READ_ONLY)')
    replace("src/device/optix/device_impl.cpp", '#  include "device/optix/device_impl.h"',
            '#  include "device/optix/device_impl.h"\n#  include "device/optix/dfv_local_shadow_tree.h"')
    replace("src/device/optix/device_impl.cpp", "OptiXDevice::~OptiXDevice()", '#include "device/optix/dfv_local_shadow.inl"\n\nOptiXDevice::~OptiXDevice()')
    replace("src/device/optix/device_impl.cpp", "  sbt_data.free();", "  dfv_shadow_cache.clear();\n  dfv_shadow_objects.free();\n  dfv_shadow_nodes.free();\n  free_bvh_memory_delayed();\n  sbt_data.free();")
    replace("src/device/optix/device_impl.cpp", "    tlas_handle = bvh_optix->traversable_handle;", "    tlas_handle = bvh_optix->traversable_handle;\n    dfv_update_local_shadows(bvh, instances);")
    replace("src/device/optix/device_impl.cpp", "    update_launch_params(offsetof(KernelParamsOptiX, data), host, size);", """    const auto pointer = dfv_shadow_objects.device_pointer;
    const auto nodes = dfv_shadow_nodes.device_pointer;
    const int count = int(dfv_shadow_nodes.size());
    update_launch_params(offsetof(KernelParamsOptiX, dfv_shadow_objects), (void *)&pointer, sizeof(pointer));
    update_launch_params(offsetof(KernelParamsOptiX, dfv_shadow_nodes), (void *)&nodes, sizeof(nodes));
    update_launch_params(offsetof(KernelParamsOptiX, dfv_shadow_node_count), (void *)&count, sizeof(count));
    update_launch_params(offsetof(KernelParamsOptiX, data), host, size);""")
    replace("src/kernel/device/optix/bvh.h", "  Intersection isect = get_intersection();", """  Intersection isect = get_intersection();
  const DfvLocalShadowRay *local = get_payload_ptr_2<DfvLocalShadowRay>();
  if (local) {
    const double world_t = fma(double(isect.t), local->t_scale, local->t_offset);
    if (world_t < ray->tmin || world_t > ray->tmax) { optixIgnoreIntersection(); return; }
    isect.t = float(world_t);
    if (local->opaque) {
      if (!(kernel_data_fetch(objects, isect.object).visibility & ray_visibility) ||
          intersection_skip_self_shadow(ray->self, isect.object, isect.prim)) {
        optixIgnoreIntersection(); return;
      }
#ifdef __SHADOW_LINKING__
      if (intersection_skip_shadow_link(kg, ray->self, isect.object)) {
        optixIgnoreIntersection(); return;
      }
#endif
      payload->throughput = 0.0f;
      optixTerminateRay(); return;
    }
  }""")
    replace("src/kernel/device/optix/bvh.h", "  if (isect.t > payload->max_record_isect_t) {", "  if (!local && isect.t > payload->max_record_isect_t) {")
    replace("src/kernel/device/optix/bvh.h", "ccl_device_intersect bool scene_intersect_shadow(KernelGlobals kg,",
            '#ifdef __TRANSPARENT_SHADOWS__\n#include "kernel/device/optix/dfv_local_shadow.h"\n#endif\nccl_device_intersect bool scene_intersect_shadow(KernelGlobals kg,')
    shadow_signature = """ccl_device_intersect bool scene_intersect_shadow(KernelGlobals kg,
                                                 const ccl_private Ray *ray,
                                                 const uint visibility)
{"""
    replace("src/kernel/device/optix/bvh.h", shadow_signature, shadow_signature + """
#ifdef __TRANSPARENT_SHADOWS__
  BVHShadowAllPayload local_payload{};
  if (dfv_local_shadow_all(ray, visibility, local_payload, true)) {
    return local_payload.throughput == 0.0f;
  }
#endif""")
    replace("src/kernel/device/optix/bvh.h", "    ccl_private BVHShadowAllPayload &ccl_restrict payload)\n{", """    ccl_private BVHShadowAllPayload &ccl_restrict payload)
{
  if (dfv_local_shadow_all(ray, ray_visibility, payload)) return;""")
