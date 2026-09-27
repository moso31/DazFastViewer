#include "runtime/geometry_shell.h"
#include <cmath>
#include <functional>
#include <stdexcept>

namespace dfv::runtime {
ir::Delta update_geometry_shells(ir::Scene &scene,ir::Delta delta,bool force) {
  std::vector<uint8_t> state(scene.instances.size());
  std::function<void(size_t)> update=[&](size_t index) {
    const auto &shell=scene.instances.at(index);if(shell.shell_source<0||shell.prototype>=0||state[index]==2)return;
    if(state[index]==1)throw std::runtime_error("Geometry Shell 宿主关系存在循环");state[index]=1;
    update(size_t(shell.shell_source));
    const auto &source=scene.instances.at(size_t(shell.shell_source));const auto &host=scene.meshes.at(source.mesh);auto &mesh=scene.meshes.at(shell.mesh);
    if(mesh.positions.size()!=host.positions.size())throw std::runtime_error("Geometry Shell 与宿主顶点数不一致");
    const bool dirty=force||std::any_of(delta.meshes.begin(),delta.meshes.end(),[&](const auto &d){return d.index==source.mesh||d.index==shell.mesh;})||
      std::any_of(delta.instances.begin(),delta.instances.end(),[&](const auto &d){return d.index==index||d.index==uint32_t(shell.shell_source);});
    if(dirty) {
      const auto relative=ir::inverse(scene.instances.at(size_t(shell.shell_root)).transform)*source.transform;std::vector<ir::Vec3> points;points.reserve(host.positions.size());
      for(auto p:host.positions)points.push_back(relative.point(p));
      if(shell.shell_offset!=0) {
        std::vector<ir::Vec3> normals(points.size());
        for(const auto &t:host.triangles)if(host.draws(t)) {
          const auto a=points[t.vertices[0]],b=points[t.vertices[1]],c=points[t.vertices[2]];
          const ir::Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
          const ir::Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
          for(auto i:t.vertices){normals[i].x+=n.x;normals[i].y+=n.y;normals[i].z+=n.z;}
        }
        for(size_t i=0;i<points.size();++i){const auto n=normals[i];const auto length=std::hypot(n.x,n.y,n.z);if(length>0){const auto amount=shell.shell_offset/length;points[i].x+=n.x*amount;points[i].y+=n.y*amount;points[i].z+=n.z*amount;}}
      }
      if(mesh.positions!=points){mesh.positions=std::move(points);auto found=std::find_if(delta.meshes.begin(),delta.meshes.end(),[&](const auto &d){return d.index==shell.mesh;});if(found==delta.meshes.end())delta.meshes.push_back({shell.mesh,mesh.positions});else found->positions=mesh.positions;}
    }
    state[index]=2;
  };
  for(size_t i=0;i<scene.instances.size();++i)update(i);return delta;
}
}
