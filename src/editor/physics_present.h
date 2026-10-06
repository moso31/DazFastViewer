#pragma once
#include "editor/physics_service.h"
#include "editor/mesh_diagnostics.h"
#include "viewport/overlay.h"
#include "viewport/window.h"
#include <future>
#include <bit>
#include <cmath>

namespace dfv::editor {
// 后台仅构建变化部分的呈现数据；主线程交换受影响的句柄与缓冲区所有权。
struct PreparedPhysics {
  std::shared_ptr<PhysicsResult> result;
  HoverOverlay overlay;
  runtime::PickingScene picking;
  std::vector<size_t> targets;
  std::vector<ir::Bounds> bounds,head_bounds;
  std::map<uint32_t,uint64_t> hashes;
  std::vector<double> displacements;
};
inline std::shared_ptr<PreparedPhysics> prepare_physics_present(std::shared_ptr<PhysicsResult> result,std::shared_ptr<const Document> document,
    Window &window,const std::vector<runtime::JointRegions> &regions,const std::vector<uint8_t> &pickable) {
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
  auto prepared=std::make_shared<PreparedPhysics>();prepared->result=std::move(result);const auto &scene=*prepared->result->scene;
  prepared->picking.prepare_delta(scene,prepared->result->delta,pickable);
  std::set<uint32_t> meshes,instances;for(const auto &e:prepared->result->delta.meshes)meshes.insert(e.index);for(const auto &e:prepared->result->delta.instances)instances.insert(e.index);
  for(size_t ti=0;ti<document->catalog.targets.size();++ti){const auto &target=document->catalog.targets[ti];const auto &i=scene.instances[target.instance];if(!meshes.contains(i.mesh)&&!instances.contains(target.instance))continue;prepared->targets.push_back(ti);const auto &mesh=scene.meshes[i.mesh];const auto &base=document->loaded.scene.meshes[i.mesh].positions;ir::Bounds bounds;double displacement=0;
    for(auto p:mesh.positions)bounds.add(i.transform.point(p));displacement=vertex_displacement(base,mesh.positions,!water::find(document->waters,target.id)).value_or(0);prepared->bounds.push_back(bounds);prepared->displacements.push_back(displacement);
    ir::Bounds head;const auto &r=regions[target.instance];if(r.head>=0)for(size_t f=0;f<r.body.size();++f)if(r.body[f]==r.head)for(auto v:mesh.triangles[f].vertices)head.add(i.transform.point(mesh.positions[v]));prepared->head_bounds.push_back(head);
  }
  for(auto index:meshes){const auto &mesh=scene.meshes[index];uint64_t h=14695981039346656037ull;for(auto p:mesh.positions)for(float x:{p.x,p.y,p.z}){h^=std::bit_cast<uint32_t>(x);h*=1099511628211ull;}prepared->hashes[index]=h;}
  try {GLContext::Binding binding(window.prepare_context);prepared->overlay.prepare_delta(scene,regions,prepared->result->delta);glFinish();}
  catch(...){try{GLContext::Binding binding(window.prepare_context);prepared->overlay.release();}catch(...){}throw;}
  return prepared;
}
inline void release_physics_present(std::shared_ptr<PreparedPhysics> p,Window &window) {
  GLContext::Binding binding(window.prepare_context);p->overlay.release();
}
template<class T> bool physics_ready(std::future<T> &future){return future.valid()&&future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;}
}
