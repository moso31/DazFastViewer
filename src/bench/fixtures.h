#pragma once
#include "bench/camera.h"
#include "render_ir/scene.h"
namespace dfv {
ir::Scene build_fixture(bool medium,const std::filesystem::path &assets);
inline ir::Camera render_camera(const CameraState &state,int width,int height) {
  ir::Camera camera;camera.transform.value=state.matrix();camera.width=width;camera.height=height;camera.revision=state.epoch;return camera;
}
}
