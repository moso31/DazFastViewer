#pragma once
#include "render_ir/scene.h"
namespace dfv::runtime {
// 在 Morph、蒙皮、碰撞及 GeoGraft 焊接之后跟随最终控制网格。
ir::Delta update_geometry_shells(ir::Scene &scene,ir::Delta delta={},bool force=false);
}
