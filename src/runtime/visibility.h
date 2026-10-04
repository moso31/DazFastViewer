#pragma once
#include "runtime/morph.h"

namespace dfv::runtime {
// Scene descendants inherit visibility. Fit To and rigid-follow dependencies
// only establish character ownership: a hidden scalp must not hide hair fitted
// to it. Neither kind of inheritance changes authored Properties::visible.
inline std::vector<std::vector<size_t>> visibility_children(const ir::Scene &scene,const std::vector<Target> &targets) {
  std::vector<std::vector<size_t>> children(targets.size());
  std::map<std::string,std::vector<size_t>> nodes;
  std::map<uint32_t,size_t> instances;
  for(size_t t=0;t<targets.size();++t) {nodes[targets[t].id.substr(0,targets[t].id.rfind('/'))].push_back(t);instances[targets[t].instance]=t;}
  auto link=[&](size_t owner,size_t child) {if(owner!=child)children[owner].push_back(child);};
  auto reference=[&](const std::string &ref,size_t child) {
    if(!ref.starts_with('#'))return;
    if(auto found=nodes.find(ref.substr(1));found!=nodes.end())for(auto owner:found->second)link(owner,child);
  };
  for(size_t t=0;t<targets.size();++t) {
    const auto &target=targets[t];reference(target.parent,t);
    for(const auto &ancestor:target.ancestors)reference(ancestor,t);
    reference("#"+target.id.substr(0,target.id.rfind('/')),t);
    const auto &instance=scene.instances.at(target.instance);
    for(auto owner:{instance.graft_source,instance.shell_source})if(owner>=0) {
      if(auto found=instances.find(uint32_t(owner));found!=instances.end())link(found->second,t);
    }
  }
  auto resolve=[&](const std::string &ref)->int {
    if(ref.starts_with('#'))if(auto found=nodes.find(ref.substr(1));found!=nodes.end()&&found->second.size()==1)return int(found->second.front());
    return -1;
  };
  for(size_t t=0;t<targets.size();++t) {
    std::set<size_t> seen;int owner=int(t);
    while(owner>=0&&seen.insert(size_t(owner)).second) {
      const auto &candidate=targets[size_t(owner)];
      if(candidate.character&&candidate.conform_target.empty()) {link(size_t(owner),t);break;}
      const auto &instance=scene.instances.at(candidate.instance);
      int next=resolve(candidate.conform_target);
      if(next<0)next=resolve(candidate.rigid_follow.target);
      for(auto source:{instance.graft_source,instance.shell_source})if(next<0&&source>=0) {
        if(auto found=instances.find(uint32_t(source));found!=instances.end())next=int(found->second);
      }
      if(next<0)next=resolve(candidate.parent);
      for(const auto &ancestor:candidate.ancestors)if(next<0)next=resolve(ancestor);
      owner=next;
    }
  }
  for(auto &list:children){std::sort(list.begin(),list.end());list.erase(std::unique(list.begin(),list.end()),list.end());}
  return children;
}
inline std::vector<bool> effective_visibility(const std::vector<Target> &targets,const std::vector<std::vector<size_t>> &children,const std::vector<Properties> &values) {
  std::vector<bool> visible(targets.size());std::vector<size_t> hidden;
  for(size_t t=0;t<targets.size();++t) {visible[t]=values.at(t).visible&&targets[t].ancestors_visible;if(!visible[t])hidden.push_back(t);}
  while(!hidden.empty()) {
    const auto owner=hidden.back();hidden.pop_back();
    for(auto child:children.at(owner))if(visible[child]){visible[child]=false;hidden.push_back(child);}
  }
  return visible;
}
}
