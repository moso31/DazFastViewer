/* Shared CPU/GPU analytic integral of rho(z)=exp(-max(z-base,0)/height).
 * Define DFV_FOG_INLINE to the device qualifier before including in Cycles. */
#pragma once
#ifndef DFV_FOG_INLINE
#  include <cmath>
#  define DFV_FOG_INLINE inline
#endif

DFV_FOG_INLINE float dfv_fog_column(float origin_height,
                                  const float dz,
                                  const float distance,
                                  const float start,
                                  const float base,
                                  const float height,
                                  const bool background)
{
  float h = origin_height - base + dz * start;
  // A horizontal/downward ray never exits this horizontally infinite layer.
  // Upward misses integrate exactly to infinity, without a proxy sky distance.
  if (background) {
    if (dz <= 0.0f) return 1.0e30f;
    const float above = expf(fminf(logf(height) - logf(dz) - fmaxf(h, 0.0f) / height, 69.07755f));
    return fminf(fmaxf(-h, 0.0f) / dz + above, 1.0e30f);
  }
  float length = fmaxf(distance - start, 0.0f);
  if (length == 0.0f) return 0.0f;
  if (dz == 0.0f) return length * expf(-fmaxf(h, 0.0f) / height);
  float below = 0.0f;
  if (h < 0.0f) {
    if (dz < 0.0f) return length;
    below = fminf(length, -h / dz);
    length -= below;
    h = 0.0f;
  }
  else if (dz < 0.0f) {
    const float above = fminf(length, h / -dz);
    below = length - above;
    length = above;
  }
  // Factor around the lower endpoint: no exp(large positive number), no
  // cancellation for almost horizontal rays, and no 0 * infinity.
  const float lower = fmaxf(0.0f, dz < 0.0f ? h + dz * length : h);
  const float q = fabsf(dz) * length / height;
  const float average = q < 0.001f ? 1.0f - q * 0.5f + q * q / 6.0f : -expm1f(-q) / q;
  return below + length * expf(-lower / height) * average;
}
#undef DFV_FOG_INLINE
