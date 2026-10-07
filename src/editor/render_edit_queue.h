#pragma once
#include "render_ir/scene.h"
#include <algorithm>

namespace dfv::editor {
// 光追繁忙时合并各对象的最新修改，白模求值和输入无需等待设备场景锁。
struct RenderEditQueue {
  ir::Delta delta;
  std::vector<uint32_t> water_meshes;
  bool pending=false,synchronize=false;
  void merge(const ir::Delta &next,bool full=false) {
    auto update=[](auto &saved,const auto &changes){for(const auto &change:changes){auto it=std::find_if(saved.begin(),saved.end(),[&](const auto &v){return v.index==change.index;});if(it==saved.end())saved.push_back(change);else *it=change;}};
    update(delta.meshes,next.meshes);update(delta.instances,next.instances);update(delta.visibility,next.visibility);update(delta.lights,next.lights);update(delta.materials,next.materials);
    update(delta.grafts,next.grafts);update(delta.masks,next.masks);
    if(next.camera)delta.camera=next.camera;if(next.options)delta.options=next.options;
    synchronize|=full;pending=true;
  }
  void clear(){delta={};water_meshes.clear();pending=synchronize=false;}
};
}
