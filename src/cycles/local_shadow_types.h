#pragma once

CCL_NAMESPACE_BEGIN

// 通过 OptiX 的空闲载荷寄存器传递，不扩展共享的 Cycles 阴影状态。
struct DfvLocalShadowRay {
  double t_offset, t_scale;
  bool opaque;
};

// 巨型组启用后，所有来源的阴影射线均可查询这些局部结构。
struct DfvLocalShadowObject {
  double inverse[12];
  float lower[3], upper[3];
  uint64_t handle;
  uint visibility;
  uint fallback;
};

// 深度优先布局；未命中时直接跳到 escape，不需要 GPU 遍历栈。
struct DfvLocalShadowNode {
  float lower[3], upper[3];
  int escape, object;
  uint visibility;
};

CCL_NAMESPACE_END
