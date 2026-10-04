#pragma once
#include "editor/document.h"
#include <set>

namespace dfv::editor {
// A Geometry Shell is one scene node, even when the renderer splits it into
// meshes for the figure and its grafts. Keep those meshes in one UI selection.
struct ObjectTargets {
  std::vector<size_t> primary;
  std::vector<std::vector<size_t>> members;
  explicit ObjectTargets(const Document &d){
    std::map<uint32_t,std::string> ids;for(const auto &o:d.loaded.objects)ids[o.instance]=o.id;
    std::map<std::string,size_t> shells;primary.resize(d.catalog.targets.size());members.resize(primary.size());
    for(size_t t=0;t<primary.size();++t){const auto instance=d.catalog.targets[t].instance;size_t root=t;
      if(d.loaded.scene.instances.at(instance).shell_source>=0&&ids.contains(instance)&&!ids.at(instance).empty())root=shells.try_emplace(ids.at(instance),t).first->second;
      primary[t]=root;members[root].push_back(t);
    }
  }
};
inline void sync_object_transform(const Document &d,Snapshot &snapshot,size_t target){
  if(d.loaded.scene.instances.at(d.catalog.targets.at(target).instance).shell_source<0)return;
  const ObjectTargets objects(d);const auto transform=snapshot.values.at(target).transform;
  for(auto part:objects.members.at(objects.primary.at(target)))snapshot.values.at(part).transform=transform;
}
// Geometry-free groups and bone parents participate in the same hierarchy as Props.
struct ObjectHierarchy {
  std::map<std::string,std::string> parents,labels;
  std::vector<std::string> instances,targets;
  std::set<std::string> groups;
  explicit ObjectHierarchy(const Document &d,bool include_fit_to=true) {
    auto id=[](const std::string &ref){return ref.starts_with('#')?ref.substr(1):std::string{};};
    for(const auto &n:d.loaded.nodes){parents[n.id]=id(n.parent);labels[n.id]=n.label;if(n.group)groups.insert(n.id);}
    instances.resize(d.loaded.scene.instances.size());
    for(const auto &o:d.loaded.objects){instances.at(o.instance)=o.id;parents[o.id]=id(o.parent);labels[o.id]=o.label;}
    for(const auto &t:d.catalog.targets){auto &node=instances.at(t.instance);if(node.empty())node=t.id.substr(0,t.id.rfind('/'));targets.push_back(node);labels[node]=t.label;
      if(parents[node].empty())parents[node]=id(t.parent);
      if(parents[node].empty()&&!t.ancestors.empty())parents[node]=id(t.ancestors.front());
    }
    for(size_t i=0;i<instances.size();++i)if(instances[i].empty()){const auto &v=d.loaded.scene.instances[i];instances[i]=v.instance_node.empty()?v.id:v.instance_node;labels.try_emplace(instances[i],v.instance_label.empty()?v.id:v.instance_label);}
    // Fit To can link clothing even when it is not parented under the figure.
    for(size_t i=0;include_fit_to&&i<targets.size();++i)if(parents[targets[i]].empty()) {
      const auto &t=d.catalog.targets[i];const auto owner=id(t.conform_target.empty()?t.rigid_follow.target:t.conform_target);
      if(!owner.empty()&&owner!=targets[i])parents[targets[i]]=owner;
    }
  }
  bool contains(const std::string &root,std::string node) const {
    std::set<std::string> seen;
    while(!node.empty()&&seen.insert(node).second){if(node==root)return true;auto p=parents.find(node);node=p==parents.end()?std::string{}:p->second;}
    return false;
  }
};
inline ir::Bounds ground_bounds(const Document &d,size_t target,const std::vector<ir::Bounds> &bounds,const std::vector<bool> &visible,bool body_only) {
  auto result=bounds.at(target);if(body_only)return result;
  const ObjectHierarchy hierarchy(d);
  for(size_t i=0;i<d.catalog.targets.size();++i) {
    if(i==target||i>=bounds.size()||(i<visible.size()&&!visible[i])||bounds[i].empty)continue;
    bool member=hierarchy.contains(hierarchy.targets[target],hierarchy.targets[i]);
    if(!member)try{member=attachment_host(d,i)==target;}catch(const std::exception &){}
    if(member){result.add(bounds[i].minimum);result.add(bounds[i].maximum);}
  }
  return result;
}
}
