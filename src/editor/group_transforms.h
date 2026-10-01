#pragma once
#include "editor/object_hierarchy.h"
#include <functional>

namespace dfv::editor {
using GroupTransforms=std::map<std::string,runtime::TransformValues>;
inline const daz::AssetNode *group_node(const Document &d,const std::string &id) {
  for(const auto &n:d.loaded.nodes)if(n.group&&n.id==id)return &n;return nullptr;
}
inline runtime::Target group_frame(const daz::AssetNode &node) {
  runtime::Target t;t.id=node.id;t.label=node.label;t.has_edit_frame=true;t.edit_frame=node.edit_frame;t.translation_frame=node.translation_frame;t.base_rotation_degrees=node.rotation_degrees;t.rotation_order=node.rotation_order;return t;
}
inline void validate_group_transforms(const Document &d,const GroupTransforms &values) {
  for(const auto &[id,v]:values){if(!group_node(d,id))throw std::runtime_error("变换组不存在："+id);runtime::validate_transform(v);}
}
inline void prune_group_transforms(const Document &d,GroupTransforms &values) {
  std::erase_if(values,[&](const auto &p){return !group_node(d,p.first);});
}
// Group edits change reference frames, leaving each child's own Transform and pose intact.
// Conjugating child edit frames preserves the order of interleaved Group / Prop ancestors.
struct GroupFrames {
  ObjectHierarchy hierarchy;
  std::map<std::string,ir::Transform> deltas;
  explicit GroupFrames(const Document &d,const GroupTransforms &values):hierarchy(d,false) {
    validate_group_transforms(d,values);if(values.empty())return;std::set<std::string> visiting;
    std::function<ir::Transform(const std::string &)> resolve=[&](const std::string &id)->ir::Transform {
      if(id.empty())return {};if(auto f=deltas.find(id);f!=deltas.end())return f->second;
      if(!visiting.insert(id).second)throw std::runtime_error("组父子关系存在循环");
      auto p=hierarchy.parents.find(id);auto delta=resolve(p==hierarchy.parents.end()?std::string{}:p->second);
      if(auto v=values.find(id);v!=values.end()){const auto &node=*group_node(d,id);delta=delta*runtime::parameter_transform(v->second,group_frame(node),node.world);}
      visiting.erase(id);deltas[id]=delta;return delta;
    };
    for(const auto &n:d.loaded.nodes)resolve(n.id);for(const auto &id:hierarchy.instances)resolve(id);for(const auto &l:d.loaded.scene.lights)resolve(l.id);
  }
  ir::Transform delta(const std::string &id) const {auto f=deltas.find(id);return f==deltas.end()?ir::Transform{}:f->second;}
};
inline runtime::Target group_target_frame(const Document &d,size_t index,const GroupFrames &frames) {
  const auto &source=d.catalog.targets.at(index);const auto delta=frames.delta(frames.hierarchy.targets[index]);runtime::Target target;
  target.has_edit_frame=true;target.edit_frame=delta*(source.has_edit_frame?source.edit_frame:ir::Transform::translate(d.loaded.scene.instances[source.instance].transform.point({})));
  target.translation_frame=delta*source.translation_frame;target.base_rotation_degrees=source.base_rotation_degrees;target.rotation_order=source.rotation_order;return target;
}
inline std::shared_ptr<const Document> transformed_groups(std::shared_ptr<const Document> source,const GroupTransforms &values) {
  if(values.empty())return source;const GroupFrames frames(*source,values);auto d=std::make_shared<Document>(*source);
  for(size_t i=0;i<d->loaded.scene.instances.size();++i){auto &v=d->loaded.scene.instances[i];v.transform=frames.delta(frames.hierarchy.instances[i])*v.transform;}
  for(size_t i=0;i<d->catalog.targets.size();++i){auto &t=d->catalog.targets[i];const auto delta=frames.delta(frames.hierarchy.targets[i]);
    // Legacy targets rotate around their loaded origin; make that frame explicit before moving it.
    if(!t.has_edit_frame){t.edit_frame=ir::Transform::translate(source->loaded.scene.instances[t.instance].transform.point({}));t.has_edit_frame=true;}
    t.edit_frame=delta*t.edit_frame;t.translation_frame=delta*t.translation_frame;
  }
  for(auto &o:d->loaded.objects){const auto delta=frames.delta(o.id);o.edit_frame=delta*o.edit_frame;o.translation_frame=delta*o.translation_frame;}
  for(auto &n:d->loaded.nodes)if(n.group)n.world=frames.delta(n.id)*n.world;
  for(auto &l:d->loaded.scene.lights)l.transform=frames.delta(l.id)*l.transform;
  return d;
}
inline ir::Transform node_parent_delta(const Document &d,const ir::Scene &evaluated,const ObjectHierarchy &hierarchy,const std::string &id,const std::vector<std::vector<runtime::JointPose>> &poses,const GroupFrames *frames=nullptr) {
  auto reference=[&](uint32_t i){return frames?frames->delta(hierarchy.instances[i])*d.loaded.scene.instances.at(i).transform:d.loaded.scene.instances.at(i).transform;};
  auto found=hierarchy.parents.find(id);auto parent=found==hierarchy.parents.end()?std::string{}:found->second;std::set<std::string> seen;
  // An empty parent is a scene root, not a reference to a skeleton's uninstantiated joint.
  while(!parent.empty()&&seen.insert(parent).second){const auto p=parent;
    for(size_t t=0;t<hierarchy.targets.size();++t)if(hierarchy.targets[t]==p){const auto i=d.catalog.targets[t].instance;return evaluated.instances.at(i).transform*ir::inverse(reference(i));}
    for(size_t s=0;s<d.skeletons.skins.size();++s){const auto &skin=d.skeletons.skins[s];for(size_t j=0;j<skin.joints.size();++j)if(skin.joints[j].scene_id==p){
      const auto &pose=s<poses.size()?poses[s]:skin.initial;const auto bind=runtime::joint_transforms(skin,skin.initial),current=runtime::joint_transforms(skin,pose);
      const auto now=pose[j].center_offset_cm,initial=skin.initial[j].center_offset_cm;const auto moved=ir::Transform::translate({(now.x-initial.x)*.01f,-(now.z-initial.z)*.01f,(now.y-initial.y)*.01f});
      return evaluated.instances.at(skin.instance).transform*current[j]*moved*ir::inverse(reference(skin.instance)*bind[j]);
    }}
    auto next=hierarchy.parents.find(p);parent=next==hierarchy.parents.end()?std::string{}:next->second;
  }
  return {};
}
inline ir::Transform group_world(const Document &d,const ir::Scene &evaluated,const std::string &id,const std::vector<std::vector<runtime::JointPose>> &poses={},const GroupFrames *frames=nullptr) {
  const auto *node=group_node(d,id);if(!node)throw std::runtime_error("变换组不存在："+id);
  const auto world=frames?frames->delta(id)*node->world:node->world;
  if(frames)return node_parent_delta(d,evaluated,frames->hierarchy,id,poses,frames)*world;
  return node_parent_delta(d,evaluated,ObjectHierarchy(d,false),id,poses)*world;
}
inline ir::Transform group_member_parent_delta(const Document &d,const ir::Scene &evaluated,const ObjectHierarchy &hierarchy,const std::string &id,const std::vector<std::vector<runtime::JointPose>> &poses,const GroupFrames *frames=nullptr) {
  for(const auto &group:hierarchy.groups)if(hierarchy.contains(group,id))return node_parent_delta(d,evaluated,hierarchy,id,poses,frames);return {};
}
inline std::vector<ir::AreaLight> group_lights(const Document &d,const ir::Scene &evaluated,const ObjectHierarchy &hierarchy,std::vector<ir::AreaLight> lights,const std::vector<std::vector<runtime::JointPose>> &poses,const GroupFrames *frames=nullptr) {
  for(auto &light:lights)light.transform=group_member_parent_delta(d,evaluated,hierarchy,light.id,poses,frames)*light.transform;return lights;
}
inline std::vector<ir::Transform> group_instance_bases(const Document &d,const ir::Scene &evaluated,const ObjectHierarchy &hierarchy,const std::vector<std::vector<runtime::JointPose>> &poses,const GroupFrames *frames=nullptr) {
  std::vector<ir::Transform> result;result.reserve(d.loaded.scene.instances.size());
  for(size_t i=0;i<d.loaded.scene.instances.size();++i){const auto &instance=d.loaded.scene.instances[i];auto world=instance.transform;
    if(frames)world=frames->delta(hierarchy.instances[i])*world;
    if(instance.prototype>=0)world=group_member_parent_delta(d,evaluated,hierarchy,hierarchy.instances[i],poses,frames)*world;
    result.push_back(world);
  }return result;
}
inline std::vector<uint32_t> group_instances(const Document &d,const std::string &id) {
  const ObjectHierarchy hierarchy(d,false);std::vector<uint32_t> result;for(size_t i=0;i<hierarchy.instances.size();++i)if(hierarchy.contains(id,hierarchy.instances[i]))result.push_back(uint32_t(i));return result;
}
inline ir::Mesh group_proxy(const ir::Scene &scene,const std::vector<uint32_t> &members,const ir::Transform &world) {
  ir::Mesh result;const auto inverse=ir::inverse(world);size_t triangles=0;
  for(auto i:members)if(scene.instances.at(i).visible)triangles+=scene.meshes.at(scene.instances[i].mesh).triangles.size();
  const size_t stride=std::max<size_t>(1,(triangles+39999)/40000);size_t counter=0;
  for(auto i:members){const auto &instance=scene.instances.at(i);if(!instance.visible)continue;const auto &mesh=scene.meshes.at(instance.mesh);const auto transform=inverse*instance.transform;
    for(const auto &triangle:mesh.triangles){if(counter++%stride||!mesh.draws(triangle))continue;ir::Triangle t;for(size_t k=0;k<3;++k){t.vertices[k]=uint32_t(result.positions.size());result.positions.push_back(transform.point(mesh.positions.at(triangle.vertices[k])));}result.triangles.push_back(t);}
  }
  return result;
}
}
