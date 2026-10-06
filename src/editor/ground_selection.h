#pragma once
#include "editor/group_transforms.h"
#include "editor/gizmo.h"

namespace dfv::editor {
struct GroundSelection {
  std::vector<std::string> roots;
  std::vector<uint32_t> instances;
  bool operator==(const GroundSelection &) const=default;
};
inline GroundSelection ground_selection(const Document &d,std::vector<std::string> selected) {
  const ObjectHierarchy hierarchy(d);const runtime::InstanceGroups groups(d.loaded.scene);
  std::sort(selected.begin(),selected.end());selected.erase(std::unique(selected.begin(),selected.end()),selected.end());
  GroundSelection result;
  for(const auto &id:selected)if(std::none_of(selected.begin(),selected.end(),[&](const auto &parent){return parent!=id&&hierarchy.contains(parent,id);}))result.roots.push_back(id);
  for(uint32_t i=0;i<hierarchy.instances.size();++i)if(std::any_of(result.roots.begin(),result.roots.end(),[&](const auto &id){return hierarchy.contains(id,hierarchy.instances[i]);})) {
    for(auto member:groups.members[groups.roots[i]])result.instances.push_back(member);
  }
  std::sort(result.instances.begin(),result.instances.end());result.instances.erase(std::unique(result.instances.begin(),result.instances.end()),result.instances.end());return result;
}
inline ir::Bounds ground_selection_bounds(const GroundSelection &selection,const std::vector<ir::Bounds> &bounds) {
  ir::Bounds result;for(auto i:selection.instances){const auto &b=bounds.at(i);if(!b.empty){result.add(b.minimum);result.add(b.maximum);}}return result;
}
// Compute into a copy so a bad frame cannot partially move a multi-selection.
inline bool align_ground_selection(const Document &d,Snapshot &snapshot,const GroundSelection &selection,
  const std::vector<ir::Bounds> &bounds,const std::vector<ir::Transform> &worlds,const std::map<std::string,ir::Transform> &group_worlds) {
  auto reference=bounds;for(size_t t=0;t<d.catalog.targets.size();++t){const auto &id=d.catalog.targets[t].id;const auto *w=water::find(snapshot.water_overrides,id);if(!w)w=water::find(d.waters,id);if(w)reference.at(d.catalog.targets[t].instance)=water::reference_bounds(*w,worlds.at(t));}
  const auto combined=ground_selection_bounds(selection,reference);const auto shift=ground_vertical_shift(combined,0);if(shift==0)return false;
  const ObjectHierarchy hierarchy(d);const runtime::InstanceGroups groups(d.loaded.scene);auto next=snapshot;
  for(const auto &id:selection.roots) {
    if(const auto *node=group_node(d,id)) {
      auto &value=next.group_transforms[id];value=ground_aligned_transform(group_frame(*node),value,node->world,group_worlds.at(id),combined,0);
    } else if(auto target=std::find(hierarchy.targets.begin(),hierarchy.targets.end(),id);target!=hierarchy.targets.end()) {
      const auto t=size_t(target-hierarchy.targets.begin());auto &value=next.values.at(t).transform;
      value=ground_aligned_transform(d.catalog.targets[t],value,d.loaded.scene.instances[d.catalog.targets[t].instance].transform,worlds.at(t),combined,0);
    } else {
      bool found=false;for(uint32_t i=0;i<hierarchy.instances.size();++i)if(hierarchy.instances[i]==id&&d.loaded.scene.instances[i].prototype>=0&&groups.roots[i]==i){next.instance_ground[d.loaded.scene.instances[i].id].offset_m+=shift;found=true;}
      if(!found)throw std::runtime_error("无法移动所选地面对齐对象："+id);
    }
  }
  validate_group_transforms(d,next.group_transforms);validate_instance_ground(d.loaded.scene,next.instance_ground);snapshot=std::move(next);return true;
}
}
