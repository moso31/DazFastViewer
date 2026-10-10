// 粗筛仅做浮点 slab 测试。扩大舍入区间，不能因粗筛误差漏掉窄小对象。
ccl_device_forceinline bool dfv_shadow_node_hit(
    const DfvLocalShadowNode &node, const ccl_private Ray *ray, const float3 inverse)
{
  float near_t = ray->tmin, far_t = ray->tmax;
  for (int axis = 0; axis < 3; ++axis) {
    const float p = (&ray->P.x)[axis], d = (&ray->D.x)[axis];
    if (d == 0.0f) {
      if (p < node.lower[axis] || p > node.upper[axis]) return false;
      continue;
    }
    const float a = (node.lower[axis]-p)*(&inverse.x)[axis];
    const float b = (node.upper[axis]-p)*(&inverse.x)[axis];
    // 极小方向或极端坐标溢出时保守保留，由叶节点的双精度裁剪决定。
    if (!isfinite(a) || !isfinite(b)) continue;
    const float pad = 8.0f*FLT_EPSILON*fmaxf(fabsf(a), fabsf(b)) + 1.0e-6f;
    near_t = fmaxf(near_t, fminf(a,b)-pad);
    far_t = fminf(far_t, fmaxf(a,b)+pad);
    if (near_t > far_t) return false;
  }
  return true;
}

// 已在 ccl 命名空间内。每个候选对象使用有限的局部射线区间。
ccl_device_intersect bool dfv_local_shadow_all(
    const ccl_private Ray *ray, const uint visibility,
    ccl_private BVHShadowAllPayload &payload, const bool opaque = false)
{
  const int count = kernel_params.dfv_shadow_node_count;
  if (count == 0 || !intersection_ray_valid(ray)) return false;
  const auto *objects = kernel_params.dfv_shadow_objects;
  const auto *nodes = kernel_params.dfv_shadow_nodes;
  const float3 inverse = make_float3(1.0f/ray->D.x, 1.0f/ray->D.y, 1.0f/ray->D.z);

  const BVHShadowAllPayload original = payload;
  uint ray_mask = visibility & 0xFF;
  if (ray_mask == 0) ray_mask = 0xFF;
  for (int index = 0; index < count;) {
    const auto &node = nodes[index];
    if (!(node.visibility & visibility) || !dfv_shadow_node_hit(node, ray, inverse)) {
      index = node.escape;
      continue;
    }
    ++index;
    if (node.object < 0) continue;
    const auto &object = objects[node.object];
    if (!object.handle || !(object.visibility & visibility)) continue;
    double p[3], d[3];
    double near_t = ray->tmin, far_t = ray->tmax;
    bool hit = true;
    for (int axis = 0; axis < 3; ++axis) {
      const double *m = &object.inverse[axis*4];
      p[axis] = fma(m[0], double(ray->P.x), fma(m[1], double(ray->P.y), fma(m[2], double(ray->P.z), m[3])));
      d[axis] = m[0]*double(ray->D.x) + m[1]*double(ray->D.y) + m[2]*double(ray->D.z);
      const double extent = double(object.upper[axis])-object.lower[axis];
      const double pad = 1.0e-5 * fmax(fmax(fabs(double(object.lower[axis])), fabs(double(object.upper[axis]))), extent) + 1.0e-7;
      const double lo = double(object.lower[axis])-pad, hi = double(object.upper[axis])+pad;
      if (d[axis] == 0.0) {
        if (p[axis] < lo || p[axis] > hi) hit = false;
      }
      else {
        const double a = (lo-p[axis])/d[axis], b = (hi-p[axis])/d[axis];
        near_t = fmax(near_t, fmin(a,b));
        far_t = fmin(far_t, fmax(a,b));
      }
    }
    if (!hit || near_t > far_t) continue;
    if (object.fallback) { payload = original; return false; }
    const double length = sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
    if (!(length > 0.0) || !isfinite(length)) { payload = original; return false; }
    const float3 local_p = make_float3(float(fma(d[0],near_t,p[0])), float(fma(d[1],near_t,p[1])), float(fma(d[2],near_t,p[2])));
    const float3 local_d = make_float3(float(d[0]/length), float(d[1]/length), float(d[2]/length));
    const float local_max = nextafterf(float((far_t-near_t)*length), FLT_MAX);
    if (!isfinite(local_p.x) || !isfinite(local_p.y) || !isfinite(local_p.z) ||
        !isfinite(local_max)) { payload = original; return false; }
    DfvLocalShadowRay local = {near_t, 1.0/length, opaque};
    uint p0 = pointer_pack_to_uint_0(&payload), p1 = pointer_pack_to_uint_1(&payload);
    uint p2 = pointer_pack_to_uint_0(&local), p3 = pointer_pack_to_uint_1(&local);
    uint p4 = visibility, p5 = 0;
    uint p6 = pointer_pack_to_uint_0(ray), p7 = pointer_pack_to_uint_1(ray);
    optixTraverse(object.handle, local_p, local_d, 0.0f, local_max, ray->time,
                  ray_mask, OPTIX_RAY_FLAG_ENFORCE_ANYHIT, 1, 0, 0,
                  p0,p1,p2,p3,p4,p5,p6,p7);
    if (payload.throughput == 0.0f) return true;
  }
  // 过滤器已跨所有对象保留最近的 N 个网格交点；着色后会推进 tmin 并查询下一批。
  // 因此透明网格超过记录容量仍可完成局部查询，不能仅因多层透明就重走世界 BVH。
  // 曲线交点会立即累计透光率但不占记录槽；若需要分批，保留原路径以免后续批次
  // 再次累计记录区间以外的曲线衰减。体积对象已在候选阶段回退。
  if (payload.num_recorded_hits > INTEGRATOR_SHADOW_ISECT_SIZE &&
      payload.num_transparent_hits != payload.num_recorded_hits) {
    payload = original;
    return false;
  }
  return true;
}
