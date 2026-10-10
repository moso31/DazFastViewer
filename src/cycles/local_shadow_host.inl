// 局部 IAS 只保存身份变换，复用 GAS。相机平移不会改变缓存键。
void OptiXDevice::dfv_update_local_shadows(BVH *bvh,
                                         const device_vector<OptixInstance> &instances)
{
  bool enabled = false;
  for (const Object *ob : bvh->objects) {
    if (!ob->get_dfv_local_shadow()) continue;
    // 阈值使用传入 Cycles 的实例矩阵，包含资源本身的单位与缩放。
    // 普通尺度不分配额外结构；组进入巨型范围后，其全部附件仍继承。
    const auto &tfm = ob->get_tfm();
    for (int column = 0; column < 3; ++column) {
      const double x = tfm.x[column], y = tfm.y[column], z = tfm.z[column];
      enabled |= x*x + y*y + z*z > 10001.0*10001.0;
    }
  }
  if (!enabled || pipeline_options.usesMotionBlur) {
    dfv_shadow_objects.free();
    dfv_shadow_nodes.free();
    dfv_shadow_cache.clear();
    return;
  }

  dfv_shadow_objects.alloc(bvh->objects.size());
  memset(dfv_shadow_objects.data(), 0,
         dfv_shadow_objects.size() * sizeof(DfvLocalShadowObject));
  dfv_shadow_cache.resize(bvh->objects.size());
  vector<bool> used(bvh->objects.size(), false);
  std::vector<DfvLocalShadowNode> leaves;
  leaves.reserve(instances.size());
  size_t rebuilt = 0;
  for (size_t i = 0; i < instances.size(); ++i) {
    OptixInstance instance = instances.data()[i];
    const size_t index = instance.instanceId;
    Object *ob = bvh->objects[index];
    Geometry *geometry = ob->get_geometry();
    auto &data = dfv_shadow_objects[index];
    data.visibility = ob->visibility_for_tracing();
    data.fallback = geometry->has_volume || (!geometry->is_mesh() && !geometry->is_hair());

    // 输入仍是 Cycles 的浮点场景；求逆、裁剪与消去长距离时使用双精度。
    const float *m = instance.transform;
    const double a = m[0], b = m[1], c = m[2];
    const double d = m[4], e = m[5], f = m[6];
    const double g = m[8], h = m[9], j = m[10];
    const double det = a * (e*j-f*h) - b * (d*j-f*g) + c * (d*h-e*g);
    if (!std::isfinite(det) || det == 0.0) {
      // 不允许不完整的候选表漏掉奇异变换的对象。
      dfv_shadow_objects.free();
      dfv_shadow_nodes.free();
      dfv_shadow_cache.clear();
      return;
    }
    const double inv[9] = {(e*j-f*h)/det, (c*h-b*j)/det, (b*f-c*e)/det,
                           (f*g-d*j)/det, (a*j-c*g)/det, (c*d-a*f)/det,
                           (d*h-e*g)/det, (b*g-a*h)/det, (a*e-b*d)/det};
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) data.inverse[row*4+col] = inv[row*3+col];
      data.inverse[row*4+3] = -(inv[row*3]*m[3] + inv[row*3+1]*m[7] + inv[row*3+2]*m[11]);
      data.lower[row] = geometry->bounds.min[row];
      data.upper[row] = geometry->bounds.max[row];
    }
    auto leaf = dfv_shadow_empty_bounds();
    leaf.object = int(index);
    leaf.visibility = data.visibility;
    // 与叶节点局部裁剪使用相同的扩张量，再向外舍入世界包围盒。
    // 即使巨大平移与局部坐标相消，也包含 CPU 求和误差。
    for (int row = 0; row < 3; ++row) {
      double lo = m[row*4+3], hi = lo, magnitude = std::abs(lo);
      for (int axis = 0; axis < 3; ++axis) {
        const double lower = data.lower[axis], upper = data.upper[axis];
        const double pad = 1.0e-5*std::max(std::max(std::abs(lower), std::abs(upper)), upper-lower) + 1.0e-7;
        const double a = double(m[row*4+axis])*(lower-pad);
        const double b = double(m[row*4+axis])*(upper+pad);
        lo += std::min(a,b);
        hi += std::max(a,b);
        magnitude += std::max(std::abs(a),std::abs(b));
      }
      const double pad = 64*std::numeric_limits<double>::epsilon()*magnitude + 1.0e-6;
      leaf.lower[row] = std::nextafter(float(lo-pad), -std::numeric_limits<float>::infinity());
      leaf.upper[row] = std::nextafter(float(hi+pad), std::numeric_limits<float>::infinity());
      if (!std::isfinite(leaf.lower[row]) || !std::isfinite(leaf.upper[row])) {
        dfv_shadow_objects.free();
        dfv_shadow_nodes.free();
        dfv_shadow_cache.clear();
        return;
      }
    }
    leaves.push_back(leaf);
    memset(instance.transform, 0, sizeof(instance.transform));
    instance.transform[0] = instance.transform[5] = instance.transform[10] = 1.0f;
    auto &cache = dfv_shadow_cache[index];
    const auto revision = static_cast<BVHOptiX *>(geometry->bvh.get())->dfv_revision;
    if (!cache.bvh || memcmp(&cache.instance, &instance, sizeof(instance)) != 0 ||
        cache.revision != revision || memcmp(cache.bounds, data.lower, 6*sizeof(float)) != 0) {
      cache.bvh = make_unique<BVHOptiX>(bvh->params, vector<Geometry *>{}, vector<Object *>{}, this);
      device_vector<OptixInstance> local(this, "local shadow instance", MEM_READ_ONLY);
      local.alloc(1);
      local[0] = instance;
      local.copy_to_device();
      OptixBuildInput input = {};
      input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
      input.instanceArray.instances = local.device_pointer;
      input.instanceArray.numInstances = 1;
      if (!build_optix_bvh(cache.bvh.get(), OPTIX_BUILD_OPERATION_BUILD, input, 0)) {
        dfv_shadow_objects.free();
        dfv_shadow_nodes.free();
        dfv_shadow_cache.clear();
        return;
      }
      cache.instance = instance;
      cache.revision = revision;
      memcpy(cache.bounds, data.lower, 6*sizeof(float));
      ++rebuilt;
    }
    data.handle = cache.bvh->traversable_handle;
    used[index] = true;
  }
  for (size_t i = 0; i < used.size(); ++i) if (!used[i]) dfv_shadow_cache[i].bvh.reset();
  dfv_shadow_objects.copy_to_device();
  const auto nodes = dfv_build_shadow_tree(std::move(leaves));
  dfv_shadow_nodes.alloc(nodes.size());
  for (size_t i = 0; i < nodes.size(); ++i) dfv_shadow_nodes[i] = nodes[i];
  dfv_shadow_nodes.copy_to_device();
  LOG_DEBUG << "Local shadow IAS: " << instances.size() << " objects, " << rebuilt
            << " rebuilt, " << nodes.size() << " candidate nodes";
}
