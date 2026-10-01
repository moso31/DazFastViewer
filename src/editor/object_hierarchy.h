#pragma once
#include "editor/document.h"
#include <set>

namespace dfv::editor {
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
