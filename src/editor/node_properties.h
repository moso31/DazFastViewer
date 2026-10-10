#pragma once
#include "editor/object_hierarchy.h"
#include "runtime/visibility.h"
#include "runtime/picking.h"
#include "cloud/document.h"

namespace dfv::editor {
inline NodeProperties node_properties(const Document &d,const Snapshot &s,const std::string &id) {
  if(auto found=s.node_properties.find(id);found!=s.node_properties.end())return found->second;
  for(const auto &n:d.loaded.nodes)if(n.id==id)return {n.visible,n.selectable};
  return {};
}
inline void validate_node_properties(const Document &d,const std::map<std::string,NodeProperties> &values) {
  for(const auto &[id,v]:values)if(std::none_of(d.loaded.nodes.begin(),d.loaded.nodes.end(),[&](const auto &n){return n.id==id&&n.group;}))throw std::runtime_error("属性组不存在："+id);
}
inline void prune_node_properties(const Document &d,std::map<std::string,NodeProperties> &values) {
  std::erase_if(values,[&](const auto &p){return std::none_of(d.loaded.nodes.begin(),d.loaded.nodes.end(),[&](const auto &n){return n.id==p.first&&n.group;});});
}
inline NodeProperties ancestor_properties(const Document &d,const Snapshot &s,const ObjectHierarchy &h,std::string id) {
  NodeProperties result;std::set<std::string> seen;
  while(!id.empty()&&seen.insert(id).second){
    if(h.groups.contains(id)){const auto state=node_properties(d,s,id);result.visible&=state.visible;result.selectable&=state.selectable;}
    auto p=h.parents.find(id);id=p==h.parents.end()?std::string{}:p->second;
  }
  return result;
}
// 父组只影响求值副本，重新开启父组时保留子对象原有的独立开关。
inline std::vector<runtime::Properties> scene_properties(const Document &d,const Snapshot &s) {
  auto values=s.values;const ObjectHierarchy h(d,false);
  for(size_t t=0;t<values.size();++t){const auto state=ancestor_properties(d,s,h,h.targets[t]);values[t].visible&=state.visible;values[t].selectable&=state.selectable;
    // 云的范围参数是快照编辑；通过既有变换求值保留组参考框架和撤销语义。
    const auto &id=d.catalog.targets[t].id;const auto *base=cloud::find(d.clouds,id),*edited=cloud::find(s.cloud_overrides,id);
    if(base&&edited){auto &p=values[t].transform.translation_cm;const auto &a=base->config,&b=edited->config;
      p.x+=float((b.x-a.x)*100);p.y+=float((b.height-a.height+(b.thickness-a.thickness)*.5)*100);p.z-=float((b.y-a.y)*100);}
  }
  return values;
}
inline std::vector<uint8_t> scene_pick_mask(const Document &d,const Snapshot &s,const ir::Scene &scene) {
  auto mask=runtime::viewport_pick_mask(scene,d.catalog.targets);const auto values=scene_properties(d,s);
  const auto children=runtime::visibility_children(scene,d.catalog.targets);std::vector<size_t> disabled;std::vector<bool> selectable(values.size());
  for(size_t t=0;t<values.size();++t){selectable[t]=values[t].selectable&&!water::find(d.waters,d.catalog.targets[t].id);if(!selectable[t])disabled.push_back(t);}
  while(!disabled.empty()){const auto t=disabled.back();disabled.pop_back();for(auto child:children[t])if(selectable[child]){selectable[child]=false;disabled.push_back(child);}}
  for(size_t t=0;t<values.size();++t)if(!selectable[t])mask.at(d.catalog.targets[t].instance)=0;
  const ObjectHierarchy h(d,false);
  for(size_t i=0;i<scene.instances.size();++i)if(scene.instances[i].prototype>=0&&!ancestor_properties(d,s,h,h.instances[i]).selectable)mask[i]=0;
  return mask;
}
inline void apply_instance_node_visibility(const Document &d,const Snapshot &s,ir::Scene &scene,ir::Delta *delta=nullptr) {
  const ObjectHierarchy h(d,false);
  for(size_t i=0;i<scene.instances.size();++i)if(scene.instances[i].prototype>=0){
    bool visible=ancestor_properties(d,s,h,h.instances[i]).visible;
    for(const auto &n:d.loaded.nodes)if(n.id==scene.instances[i].instance_node){visible&=n.visible;break;}
    auto &instance=scene.instances[i];if(instance.visible!=visible){instance.visible=visible;if(delta)delta->visibility.push_back({uint32_t(i),visible});}
  }
}
}
