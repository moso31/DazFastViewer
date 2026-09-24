#include "runtime/graft_surface.h"
#include <map>
#include <set>
#include <numeric>
#include <cmath>
#include <stdexcept>

namespace dfv::runtime {
bool same_mesh_topology(const ir::Mesh &a,const ir::Mesh &b) {
  return a.positions.size()==b.positions.size()&&a.triangles==b.triangles&&a.polygons==b.polygons&&a.curves==b.curves&&
    a.subdivision==b.subdivision&&a.creases==b.creases&&a.corners==b.corners&&a.hidden_polygons==b.hidden_polygons&&a.smooth==b.smooth&&
    a.graft_vertex_pairs==b.graft_vertex_pairs&&a.graft_target_vertices==b.graft_target_vertices&&a.graft_target_polygons==b.graft_target_polygons&&a.material_slots==b.material_slots;
}
uint32_t graft_root(const ir::Scene &scene,uint32_t instance) {
  std::set<uint32_t> seen;
  while(scene.instances.at(instance).graft_source>=0) {
    if(!seen.insert(instance).second) throw std::runtime_error("GeoGraft 宿主关系存在循环");
    instance=uint32_t(scene.instances[instance].graft_source);
  }
  return instance;
}
std::vector<std::vector<uint32_t>> graft_groups(const ir::Scene &scene) {
  std::map<uint32_t,std::vector<uint32_t>> groups;
  for(uint32_t i=0;i<scene.instances.size();++i) if(scene.instances[i].prototype<0&&scene.instances[i].graft_source>=0) groups[graft_root(scene,i)].push_back(i);
  std::vector<std::vector<uint32_t>> result;
  for(auto &[root,pending]:groups) {
    std::vector<uint32_t> group{root};std::set<uint32_t> added{root};
    while(!pending.empty()) {
      auto it=std::find_if(pending.begin(),pending.end(),[&](auto i){return added.contains(uint32_t(scene.instances[i].graft_source));});
      if(it==pending.end()) throw std::runtime_error("GeoGraft 的直接宿主不是可细分原型");
      group.push_back(*it);added.insert(*it);pending.erase(it);
    }
    result.push_back(std::move(group));
  }
  return result;
}
struct GraftSurface::Topology {
  struct Vertex {uint32_t part,index;};
  struct Part {std::vector<uint32_t> vertices;std::vector<ir::Triangle> triangles;};
  Subdivision subdivision;
  std::vector<Vertex> cage;
  std::vector<ir::Triangle> triangles;
  std::vector<Part> parts;
  Topology(const ir::Mesh &mesh,bool final_render):subdivision(mesh,final_render) {}
};
struct GraftSurface::Evaluated {std::vector<ir::Vec3> cage,positions,normals;};
GraftSurface::GraftSurface(const ir::Scene &scene,std::vector<uint32_t> members,bool final_render):members_(std::move(members)) {
  const auto &root=scene.instances.at(members_.at(0));ir::Mesh combined;combined.id=root.id+"/graft-surface";
  combined.subdivision=scene.meshes[root.mesh].subdivision;
  std::map<uint32_t,size_t> part_index;std::vector<std::vector<uint32_t>> mapping(members_.size());
  std::vector<Topology::Vertex> cage;std::vector<std::pair<uint32_t,uint32_t>> face_sources;
  std::map<std::pair<uint32_t,uint32_t>,float> creases;std::map<uint32_t,float> corners;
  for(uint32_t part=0;part<members_.size();++part) {
    const auto index=members_[part];const auto &instance=scene.instances.at(index);const auto &mesh=scene.meshes.at(instance.mesh);
    auto &map=mapping[part];map.resize(mesh.positions.size(),UINT32_MAX);
    if(part) {
      const auto parent=part_index.at(uint32_t(instance.graft_source));const auto &host=scene.meshes[scene.instances[members_[parent]].mesh];
      if(mesh.graft_target_vertices!=host.positions.size()||(mesh.graft_target_polygons&&mesh.graft_target_polygons!=host.source_polygon_count)) throw std::runtime_error("共同细分的 GeoGraft 目标拓扑不匹配");
      for(auto pair:mesh.graft_vertex_pairs) {
        if(pair[0]>=map.size()||pair[1]>=mapping[parent].size()||map[pair[0]]!=UINT32_MAX) throw std::runtime_error("共同细分的 GeoGraft 顶点对无效");
        map[pair[0]]=mapping[parent][pair[1]];
      }
    }
    for(uint32_t v=0;v<map.size();++v) if(map[v]==UINT32_MAX) {map[v]=uint32_t(cage.size());cage.push_back({part,v});}
    part_index[index]=part;
    if(mesh.polygons.empty()) throw std::runtime_error("共同细分缺少 GeoGraft 多边形拓扑");
    for(uint32_t p=0;p<mesh.polygons.size();++p) if(!std::binary_search(mesh.hidden_polygons.begin(),mesh.hidden_polygons.end(),p)) {
      auto polygon=mesh.polygons[p];for(auto &v:polygon.vertices) v=map.at(v);
      combined.polygons.push_back(std::move(polygon));face_sources.push_back({part,p});
    }
    for(auto c:mesh.creases) {auto a=map.at(c.a),b=map.at(c.b);if(a>b) std::swap(a,b);creases[{a,b}]=std::max(creases[{a,b}],c.weight);}
    for(auto [v,w]:mesh.corners) corners[map.at(v)]=std::max(corners[map.at(v)],w);
  }
  combined.positions.resize(cage.size());
  for(auto [edge,w]:creases) combined.creases.push_back({edge.first,edge.second,w});for(auto [v,w]:corners) combined.corners.push_back({v,w});
  for(uint32_t p=0;p<combined.polygons.size();++p) {
    const auto &face=combined.polygons[p];
    for(size_t k=1;k+1<face.vertices.size();++k) {ir::Triangle t;t.vertices={face.vertices[0],face.vertices[k],face.vertices[k+1]};t.uv={face.uv[0],face.uv[k],face.uv[k+1]};t.material_slot=face.material_slot;t.polygon_group=face.polygon_group;t.source_polygon=p;combined.triangles.push_back(t);}
  }
  auto topology=std::make_shared<Topology>(combined,final_render);topology->cage=std::move(cage);topology->parts.resize(members_.size());
  topology->triangles=topology->subdivision.active()?topology->subdivision.triangles():combined.triangles;
  std::vector<std::map<uint32_t,uint32_t>> compact(members_.size());
  for(auto t:topology->triangles) {
    const auto [part,polygon]=face_sources.at(t.source_polygon);auto &out=topology->parts[part];t.source_polygon=polygon;
    for(auto &v:t.vertices) {auto [it,inserted]=compact[part].emplace(v,uint32_t(out.vertices.size()));if(inserted) out.vertices.push_back(v);v=it->second;}
    out.triangles.push_back(t);
  }
  topology_=std::move(topology);evaluate(scene);
}
bool GraftSurface::compatible(const ir::Scene &next,const ir::Scene &old,const std::vector<uint32_t> &members) const {
  if(members.size()!=members_.size()) return false;
  for(size_t p=0;p<members.size();++p) {
    const auto &a=next.instances[members[p]],&b=old.instances[members_[p]];
    if(a.id!=b.id||!same_mesh_topology(next.meshes[a.mesh],old.meshes[b.mesh])) return false;
    if((a.graft_source<0)!=(b.graft_source<0)) return false;
    if(a.graft_source>=0&&next.instances[size_t(a.graft_source)].id!=old.instances[size_t(b.graft_source)].id) return false;
  }
  return true;
}
bool GraftSurface::evaluate(const ir::Scene &scene) {
  std::vector<ir::Transform> relative(members_.size());const auto inverse=ir::inverse(scene.instances[members_[0]].transform);
  for(size_t p=1;p<members_.size();++p) relative[p]=inverse*scene.instances[members_[p]].transform;
  auto output=std::make_shared<Evaluated>();output->cage.reserve(topology_->cage.size());
  for(auto v:topology_->cage) output->cage.push_back(relative[v.part].point(scene.meshes[scene.instances[members_[v.part]].mesh].positions[v.index]));
  if(evaluated_&&output->cage==evaluated_->cage) return false;
  output->positions=topology_->subdivision.evaluate(output->cage);output->normals.resize(output->positions.size());
  // 与 Cycles 的顶点法线相同，累加单位面法线。所有部件共用该结果，拆分不会产生法线断层。
  for(const auto &t:topology_->triangles) {
    const auto a=output->positions[t.vertices[0]],b=output->positions[t.vertices[1]],c=output->positions[t.vertices[2]];
    const ir::Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};ir::Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    const float length=std::hypot(n.x,n.y,n.z);if(length>1e-20f) {n.x/=length;n.y/=length;n.z/=length;}
    for(auto vertex:t.vertices) {auto &normal=output->normals[vertex];normal.x+=n.x;normal.y+=n.y;normal.z+=n.z;}
  }
  for(auto &n:output->normals) {const float length=std::hypot(n.x,n.y,n.z);if(length>1e-20f) {n.x/=length;n.y/=length;n.z/=length;}}
  evaluated_=std::move(output);return true;
}
const std::vector<ir::Triangle> &GraftSurface::triangles(size_t part) const {return topology_->parts.at(part).triangles;}
const std::vector<uint32_t> &GraftSurface::vertices(size_t part) const {return topology_->parts.at(part).vertices;}
const std::vector<ir::Vec3> &GraftSurface::positions() const {return evaluated_->positions;}
const std::vector<ir::Vec3> &GraftSurface::normals() const {return evaluated_->normals;}
ir::Transform GraftSurface::transform(const ir::Scene &scene,uint32_t instance) const {
  const auto &object=scene.instances.at(instance),&root=scene.instances[members_[0]];
  if(object.prototype<0) return root.transform;
  return object.transform*ir::inverse(scene.instances[size_t(object.prototype)].transform)*root.transform;
}
}
