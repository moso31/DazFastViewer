#pragma once
#include "daz/loader.h"
namespace dfv::daz {
bool apply_material_uv(ir::Scene &scene,size_t instance,size_t slot,const ir::Material &preset,const LoadOptions &options);
}
