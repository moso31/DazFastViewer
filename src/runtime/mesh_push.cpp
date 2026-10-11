#include "runtime/mesh_push.h"
#include <cmath>
#include <map>
#include <stdexcept>

namespace dfv::runtime {
void apply_mesh_push(const ir::Mesh &mesh,std::vector<ir::Vec3> &positions,const std::vector<MeshPush> &modifiers,bool post_smooth) {
  for(const auto &modifier:modifiers) {
    if(modifier.post_smooth!=post_smooth||modifier.offset_m==0)continue;
    if(!std::isfinite(modifier.offset_m)||(!modifier.weights.empty()&&modifier.weights.size()!=positions.size()))throw std::runtime_error("Push 修改器的偏移或权重数量无效");
    std::vector<ir::Vec3> normals(positions.size());
    for(const auto &f:mesh.triangles) {
      const auto a=positions[f.vertices[0]],b=positions[f.vertices[1]],c=positions[f.vertices[2]];
      const ir::Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
      const ir::Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
      for(auto i:f.vertices){normals[i].x+=n.x;normals[i].y+=n.y;normals[i].z+=n.z;}
    }
    // UV 拆点共享法线，避免外扩时把本来重合的拼缝推开。
    std::map<std::array<float,3>,ir::Vec3> shared;
    for(size_t i=0;i<positions.size();++i){const auto p=positions[i],n=normals[i];auto &sum=shared[{p.x,p.y,p.z}];sum.x+=n.x;sum.y+=n.y;sum.z+=n.z;}
    for(size_t i=0;i<positions.size();++i){auto &p=positions[i];const auto n=shared.at({p.x,p.y,p.z});const float length=std::hypot(n.x,n.y,n.z),weight=modifier.weights.empty()?1:modifier.weights[i];
      if(!std::isfinite(weight)||weight<0||weight>1)throw std::runtime_error("Push 修改器的顶点权重无效");
      if(length>0){const float amount=modifier.offset_m*weight/length;p.x+=n.x*amount;p.y+=n.y*amount;p.z+=n.z*amount;}
    }
  }
}
}
