/* DazFastViewer analytic height atmosphere. Injected by prepare_cycles.py. */
#pragma once

CCL_NAMESPACE_BEGIN
#define DFV_FOG_INLINE ccl_device_inline
#include "kernel/integrator/dfv_fog_column.h"

ccl_device_inline Spectrum dfv_fog_transmittance(KernelGlobals kg,
                                                 IntegratorState state,
                                                 const float distance,
                                                 const bool background = false)
{
  const float4 settings = kernel_data.background.dfv_fog_settings;
  const float d = dfv_fog_column(INTEGRATOR_STATE(state, ray, P).z,
                                 INTEGRATOR_STATE(state, ray, D).z,
                                 distance, settings.x, settings.y, settings.z, background);
  const float4 beta = kernel_data.background.dfv_fog_extinction;
  return rgb_to_spectrum(make_float3(expf(-beta.x * d), expf(-beta.y * d), expf(-beta.z * d)));
}

ccl_device_inline Spectrum dfv_fog_radiance(KernelGlobals kg)
{
  const float4 color = kernel_data.background.dfv_fog_color;
  const float scale = color.w != 0.0f ? kernel_data.background.dfv_fog_settings.w : 1.0f;
  return rgb_to_spectrum(make_float3(color.x, color.y, color.z) * scale);
}

ccl_device_inline void dfv_matte_fog_surface(KernelGlobals kg,
                                            IntegratorState state,
                                            ccl_private ShaderData *sd,
                                            ccl_global float *ccl_restrict render_buffer)
{
  if (kernel_data.background.dfv_fog_extinction.w == 0.0f ||
      !(INTEGRATOR_STATE(state, path, visibility) & PATH_RAY_VISIBILITY_CAMERA)) {
    return;
  }
  // Transparent hits keep the original ray origin: ray_length is the full
  // camera distance, including cutout sheets and volume boundaries.
  const Spectrum transmission = dfv_fog_transmittance(kg, state, sd->ray_length);
  const Spectrum fog = (one_spectrum() - transmission) * dfv_fog_radiance(kg) *
                       surface_shader_alpha(sd);
  film_write_surface_emission(kg, state, fog, 1.0f, render_buffer, LIGHTGROUP_NONE);
  sd->closure_emission_background *= transmission;
  for (int i = 0; i < sd->num_closure; ++i) {
    ccl_private ShaderClosure *sc = &sd->closure[i];
    // Preserve cutout transmission and its sampling probability. Attenuation
    // belongs to the actual surface, never the transparent rectangle around it.
    if (!CLOSURE_IS_BSDF_TRANSPARENT(sc->type) && !CLOSURE_IS_HOLDOUT(sc->type)) {
      sc->weight *= transmission;
    }
  }
}

ccl_device_inline Spectrum dfv_matte_fog_background(KernelGlobals kg,
                                                   IntegratorState state,
                                                   const Spectrum background)
{
  if (kernel_data.background.dfv_fog_extinction.w == 0.0f ||
      !(INTEGRATOR_STATE(state, path, visibility) & PATH_RAY_VISIBILITY_CAMERA)) {
    return background;
  }
  const Spectrum amount = one_spectrum() - dfv_fog_transmittance(kg, state, 0.0f, true);
  return background * (one_spectrum() - amount) + dfv_fog_radiance(kg) * amount;
}

CCL_NAMESPACE_END
