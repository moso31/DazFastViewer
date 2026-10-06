#include "cloud/document.h"
#include "water/document.h"
#include "editor/document.h"
#include "editor/object_hierarchy.h"
#include <cmath>

namespace dfv::cloud {
Clouds effective(const editor::Document &d,const editor::Snapshot &s){auto result=d.clouds;for(auto &v:result)for(const auto &o:s.cloud_overrides)if(v->id==o->id){v=o;break;}return result;}
std::vector<Candidate> candidates(const editor::Document &d,const std::vector<ir::Bounds> &instance_bounds){
  // Rank the same object/group set as water using evaluated world bounds,
  // including current scale and deformation, without rescanning mesh vertices.
  std::vector<Candidate> result;for(const auto &c:water::candidates(d,instance_bounds))result.push_back({c.id,c.label,c.volume});return result;
}
void install(editor::Document &d,std::shared_ptr<const Cloud> v,bool record){
  if(v->id.empty())throw std::runtime_error("体积云身份为空");auto shape=mesh(*v);auto mat=material(*v);auto &s=d.loaded.scene;
  auto old=std::find_if(d.clouds.begin(),d.clouds.end(),[&](const auto &o){return o->id==v->id;});
  auto target=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.id==v->id+"/volume";});
  const auto transform=ir::Transform::translate({float(v->config.x),float(v->config.y),float(v->config.height+v->config.thickness*.5)});
  if(old!=d.clouds.end()){
    if(target==d.catalog.targets.end())throw std::runtime_error("体积云缺少场景绑定");auto &i=s.instances.at(target->instance);s.meshes.at(i.mesh)=std::move(shape);s.materials.at(i.materials.at(0))=std::move(mat);i.transform=transform;*old=v;
  }else{
    if(target!=d.catalog.targets.end()||std::any_of(d.loaded.nodes.begin(),d.loaded.nodes.end(),[&](const auto &n){return n.id==v->id;}))throw std::runtime_error("体积云身份冲突");
    ir::Instance i;i.id=v->id+"/volume";i.instance_node=v->id;i.instance_label="体积云";i.mesh=uint32_t(s.meshes.size());i.materials={uint32_t(s.materials.size())};i.transform=transform;
    runtime::Target t;t.id=i.id;t.label="体积云";t.instance=uint32_t(s.instances.size());d.catalog.targets.push_back(t);d.formulas.graphs.emplace_back();
    daz::AssetNode n;n.id=v->id;n.label="体积云";d.loaded.nodes.push_back(n);
    s.instances.push_back(i);s.meshes.push_back(std::move(shape));s.materials.push_back(std::move(mat));d.clouds.push_back(v);
  }
  ++d.asset_revision;s.validate();if(record){nlohmann::json op={{"op","cloud"},{"cloud",json(*v)}};if(!d.operations.empty()&&d.operations.back().value("op","")=="cloud"&&d.operations.back().at("cloud").at("id")==v->id)d.operations.back()=std::move(op);else d.operations.push_back(std::move(op));}
}
bool Runtime::apply(const editor::Document &d,const Clouds &clouds,ir::Scene &scene,ir::Delta *delta){
  if(clouds.empty())return false;
  if(!delta)clear();else for(const auto &e:delta->meshes)meshes_.erase(e.index);
  const editor::ObjectHierarchy hierarchy(d);bool changed=false;
  for(const auto &v:clouds){
    auto instance=std::find_if(scene.instances.begin(),scene.instances.end(),[&](const auto &i){return i.id==v->id+"/volume";});if(instance==scene.instances.end())continue;
    const auto &c=v->config;auto mat=material(*v);auto &volume=*mat.cloud;const auto local=ir::inverse(instance->transform);
    if(c.collisions&&instance->visible)for(const auto &id:c.sources){
      std::vector<Member> members;
      for(size_t i=0;i<scene.instances.size();++i){const auto &o=scene.instances[i];if(!o.visible||find(clouds,o.id)||water::find(d.waters,o.id)||!hierarchy.contains(id,hierarchy.instances.at(i)))continue;
        if(std::all_of(o.materials.begin(),o.materials.end(),[&](auto m){return scene.materials.at(m).opacity<=0;}))continue;
        auto found=meshes_.find(o.mesh);if(found==meshes_.end()){
          const auto &mesh=scene.meshes.at(o.mesh);std::vector<bool> used(mesh.positions.size());
          for(const auto &f:mesh.triangles)if(mesh.draws(f))for(auto j:f.vertices)used[j]=true;
          for(const auto &curve:mesh.curves)for(auto j:curve.vertices)used[j]=true;
          std::vector<ir::Vec3> points;points.reserve(mesh.positions.size());for(size_t j=0;j<used.size();++j)if(used[j])points.push_back(mesh.positions[j]);
          found=meshes_.emplace(o.mesh,std::make_shared<Hull>(convex_hull(points))).first;++hull_builds_;
        }
        if(!found->second->planes.empty())members.push_back({uint32_t(i),o.transform,found->second});
      }
      auto &source=sources_[id];if(source.members!=members){
        source.members=std::move(members);source.hull={};
        if(source.members.size()==1)source.hull=transformed_hull(*source.members[0].hull,source.members[0].transform);
        else if(!source.members.empty()){
          std::vector<ir::Vec3> points;for(const auto &m:source.members)for(auto p:m.hull->vertices)points.push_back(m.transform.point(p));
          source.hull=convex_hull(points);++hull_builds_;
        }
      }
      const auto &hull=source.hull;if(hull.planes.empty())continue;
      ir::CloudCollider collider;collider.center=local.point(hull.center);collider.padding=float(c.padding);collider.softness=float(c.softness);
      const double age=std::min(std::max(0.,c.time),c.trail);const auto &a=local.value;
      collider.tail={float(-age*(a[0]*c.velocity_x+a[1]*c.velocity_y+a[2]*c.velocity_z)),float(-age*(a[4]*c.velocity_x+a[5]*c.velocity_y+a[6]*c.velocity_z)),float(-age*(a[8]*c.velocity_x+a[9]*c.velocity_y+a[10]*c.velocity_z))};
      collider.tail_length=float(age*std::hypot(c.velocity_x,c.velocity_y,c.velocity_z));collider.decay=float(age/c.recovery);
      ir::Bounds box;const auto &b=hull.bounds;for(float x:{b.minimum.x,b.maximum.x})for(float y:{b.minimum.y,b.maximum.y})for(float z:{b.minimum.z,b.maximum.z})box.add(local.point({x,y,z}));
      auto extent=[&](int row){return float(c.padding)*std::hypot(a[row*4],a[row*4+1],a[row*4+2]);};
      const auto h=volume.half_extent,t=collider.tail;
      if(box.minimum.x+std::min(0.f,t.x)-extent(0)>h.x||box.maximum.x+std::max(0.f,t.x)+extent(0)<-h.x||box.minimum.y+std::min(0.f,t.y)-extent(1)>h.y||box.maximum.y+std::max(0.f,t.y)+extent(1)<-h.y||box.minimum.z+std::min(0.f,t.z)-extent(2)>h.z||box.maximum.z+std::max(0.f,t.z)+extent(2)<-h.z)continue;
      // Keep distances in world metres under nonuniform cloud/object scaling.
      const auto &m=instance->transform.value;for(const auto &p:hull.planes){const auto n=p.normal;collider.planes.push_back({{m[0]*n.x+m[4]*n.y+m[8]*n.z,m[1]*n.x+m[5]*n.y+m[9]*n.z,m[2]*n.x+m[6]*n.y+m[10]*n.z},p.distance});}
      volume.colliders.push_back(std::move(collider));
    }
    const auto slot=instance->materials.at(0);if(scene.materials.at(slot)!=mat){scene.materials[slot]=std::move(mat);changed=true;if(delta){std::erase_if(delta->materials,[&](const auto &e){return e.index==slot;});delta->materials.push_back({slot,scene.materials[slot]});}}
    const auto shape=mesh(*v);auto &old=scene.meshes.at(instance->mesh);if(old.positions!=shape.positions){old.positions=shape.positions;changed=true;if(delta){std::erase_if(delta->meshes,[&](const auto &e){return e.index==instance->mesh;});delta->meshes.push_back({instance->mesh,old.positions});}}
  }return changed;
}
}
