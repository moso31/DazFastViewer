#pragma once
#include "kernel/svm/dfv_night_sky_model.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"
CCL_NAMESPACE_BEGIN
ccl_device_noinline void svm_node_dfv_night_sky(ccl_private float *stack,const ccl_global SVMNodeDfvNightSky &node)
{
  const float3 direction=dfv_night_reference_direction(stack_load_float3(stack,node.vector));
  const auto raw=dfv_night_raw(direction,node.quality,stack_valid(node.stars_offset)&&node.density>0);
  float3 galaxy,stars;dfv_night_finish(raw,node.contrast,node.density,&galaxy,&stars);
  if(stack_valid(node.galaxy_offset))stack_store_float3(stack,node.galaxy_offset,galaxy);
  if(stack_valid(node.stars_offset))stack_store_float3(stack,node.stars_offset,stars);
}
CCL_NAMESPACE_END
