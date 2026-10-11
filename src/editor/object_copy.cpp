#include "editor/document.h"
#include "editor/object_hierarchy.h"
#include <set>

namespace dfv::editor {
std::vector<size_t> copy_selection(Document &d,Snapshot &s,const std::vector<size_t> &targets,const std::vector<std::string> &selected_nodes,const std::vector<size_t> &lights,bool children,const std::string &identity_prefix){
  if(s.generation!=d.generation)throw std::runtime_error("复制对象的场景状态已变化");
  const ObjectHierarchy hierarchy(d);std::set<std::string> roots(selected_nodes.begin(),selected_nodes.end());
  for(auto t:targets)roots.insert(hierarchy.targets.at(t));for(auto l:lights)roots.insert(s.lights.at(l).id);
  if(roots.empty())return {};
  auto nodes=roots;
  if(children){
    bool changed=true;while(changed){const auto count=nodes.size();
      for(const auto &[id,parent]:hierarchy.parents)if(nodes.contains(parent))nodes.insert(id);
      for(const auto &t:d.catalog.targets)for(const auto &ref:{t.conform_target,t.rigid_follow.target})if(ref.starts_with('#')&&nodes.contains(ref.substr(1)))nodes.insert(hierarchy.targets.at(&t-d.catalog.targets.data()));
      changed=count!=nodes.size();
    }
  }
  // 骨骼属于模型本体，单独复制角色也必须复制其骨架，但不扩展骨骼下的附件。
  for(const auto &skin:d.skeletons.skins)if(nodes.contains(hierarchy.instances.at(skin.instance)))for(const auto &joint:skin.joints)if(!joint.scene_id.empty())nodes.insert(joint.scene_id);
  const Document original=d;const Snapshot state=s;std::string prefix=identity_prefix;
  auto occupied=[&]{for(const auto &n:d.loaded.nodes)if(n.id.starts_with(prefix))return true;for(const auto &i:d.loaded.scene.instances)if(i.id.starts_with(prefix))return true;for(const auto &l:s.lights)if(l.id.starts_with(prefix))return true;return false;};
  if(prefix.empty()){size_t serial=0;do{prefix="copy-"+std::to_string(serial++)+"/";}while(occupied());}else if(occupied())throw std::runtime_error("复制对象身份重复");
  std::map<std::string,std::string> ids;for(const auto &id:nodes)ids[id]=prefix+id;
  auto rename=[&](std::string &ref){if(ref.starts_with('#')&&ids.contains(ref.substr(1)))ref="#"+ids.at(ref.substr(1));};
  std::map<uint32_t,uint32_t> instances,meshes,materials;std::map<int,int> skins;std::vector<size_t> copies;
  for(size_t i=0;i<original.loaded.scene.instances.size();++i){const auto &old=original.loaded.scene.instances[i];
    bool keep=nodes.contains(hierarchy.instances[i])||nodes.contains(old.id)||nodes.contains(old.instance_group);
    for(const auto &city:original.cities)if(nodes.contains(city->id)&&old.id.starts_with(city->id+"/"))keep=true;
    if(!keep)continue;
    ids[old.id]=prefix+old.id;auto copy=old;copy.id=prefix+old.id;
    if(!meshes.contains(old.mesh)){auto mesh=original.loaded.scene.meshes.at(old.mesh);ids[mesh.id]=prefix+mesh.id;mesh.id=prefix+mesh.id;meshes[old.mesh]=uint32_t(d.loaded.scene.meshes.size());d.loaded.scene.meshes.push_back(std::move(mesh));}
    copy.mesh=meshes.at(old.mesh);
    for(auto &m:copy.materials){if(!materials.contains(m)){auto material=original.loaded.scene.materials.at(m);material.id=prefix+material.id;materials[m]=uint32_t(d.loaded.scene.materials.size());d.loaded.scene.materials.push_back(std::move(material));}m=materials.at(m);}
    instances[uint32_t(i)]=uint32_t(d.loaded.scene.instances.size());d.loaded.scene.instances.push_back(std::move(copy));
  }
  auto instance_index=[&](int i){return i>=0&&instances.contains(uint32_t(i))?int(instances.at(uint32_t(i))):i;};
  for(const auto &[old,index]:instances){auto &i=d.loaded.scene.instances.at(index);i.prototype=instance_index(i.prototype);i.graft_source=instance_index(i.graft_source);i.shell_source=instance_index(i.shell_source);i.shell_root=instance_index(i.shell_root);if(ids.contains(i.instance_node))i.instance_node=ids.at(i.instance_node);if(ids.contains(i.instance_group))i.instance_group=ids.at(i.instance_group);if(roots.contains(hierarchy.instances.at(old)))i.instance_label+=" 副本";}
  for(const auto &o:original.loaded.objects)if(instances.contains(o.instance)){auto copy=o;copy.id=prefix+o.id;copy.instance=instances.at(o.instance);rename(copy.parent);rename(copy.conform_target);rename(copy.smoothing.collision_target);rename(copy.rigid_follow.target);if(roots.contains(o.id))copy.label+=" 副本";d.loaded.objects.push_back(std::move(copy));}
  for(size_t i=0;i<original.skeletons.skins.size();++i)if(instances.contains(original.skeletons.skins[i].instance)){
    auto skin=original.skeletons.skins[i];skin.instance=instances.at(skin.instance);ids[skin.id]=prefix+skin.id;skin.id=prefix+skin.id;for(auto &j:skin.joints)if(!j.scene_id.empty())j.scene_id=prefix+j.scene_id;
    skins[int(i)]=int(d.skeletons.skins.size());d.skeletons.skins.push_back(std::move(skin));s.poses.push_back(state.poses.at(i));
  }
  for(size_t t=0;t<original.catalog.targets.size();++t)if(instances.contains(original.catalog.targets[t].instance)){
    auto target=original.catalog.targets[t];ids[target.id]=prefix+target.id;target.id=prefix+target.id;target.instance=instances.at(target.instance);rename(target.parent);rename(target.conform_target);rename(target.smoothing.collision_target);rename(target.rigid_follow.target);for(auto &a:target.ancestors)rename(a);if(roots.contains(hierarchy.targets[t]))target.label+=" 副本";
    auto graph=original.formulas.graphs.at(t);if(graph.skin>=0)graph.skin=skins.at(graph.skin);copies.push_back(d.catalog.targets.size());d.catalog.targets.push_back(std::move(target));d.formulas.graphs.push_back(std::move(graph));s.values.push_back(state.values.at(t));
  }
  for(const auto &n:original.loaded.nodes)if(nodes.contains(n.id)){auto copy=n;copy.id=prefix+n.id;rename(copy.parent);if(roots.contains(n.id))copy.label+=" 副本";d.loaded.nodes.push_back(std::move(copy));}
  for(const auto &light:state.lights)if(nodes.contains(light.id)){auto copy=light;copy.id=prefix+light.id;s.lights.push_back(std::move(copy));}d.loaded.scene.lights=s.lights;
  for(const auto &b:original.attachments){auto copy=b;std::erase_if(copy.items,[&](const auto &v){return !nodes.contains(v.node);});if(copy.items.empty())continue;if(ids.contains(copy.host))copy.host=ids.at(copy.host);for(auto &v:copy.items){v.node=ids.at(v.node);rename(v.parent);rename(v.conform);rename(v.collision);rename(v.rigid);}std::erase_if(copy.nodes,[&](const auto &n){return !nodes.contains(n.id);});for(auto &n:copy.nodes){n.id=ids.at(n.id);rename(n.parent);}d.attachments.push_back(std::move(copy));}
  auto copy_map=[&](auto &to,const auto &from){for(const auto &[key,value]:from)if(ids.contains(key))to[ids.at(key)]=value;};
  copy_map(s.subdivision_levels,state.subdivision_levels);copy_map(s.material_overrides,state.material_overrides);copy_map(s.group_transforms,state.group_transforms);copy_map(s.node_properties,state.node_properties);copy_map(s.instance_ground,state.instance_ground);copy_map(s.parameter_settings,state.parameter_settings);copy_map(s.city_views,state.city_views);
  for(const auto &[key,archive]:original.material_archives)if(ids.contains(key.first))d.material_archives[{ids.at(key.first),key.second}]=archive;
  for(const auto &pin:state.pose_pins)if(skins.contains(pin.skin)){auto copy=pin;copy.skin=skins.at(pin.skin);s.pose_pins.push_back(std::move(copy));}
  for(const auto &city:original.cities)if(nodes.contains(city->id))d.cities.push_back(city::prefixed(*city,prefix));
  auto copy_water=[&](auto &to,const auto &from){for(const auto &v:from)if(ids.contains(v->id)){auto copy=std::make_shared<water::Water>(*v);copy->id=ids.at(v->id);for(auto &source:copy->config.sources)if(ids.contains(source.id))source.id=ids.at(source.id);to.push_back(std::move(copy));}};
  auto copy_cloud=[&](auto &to,const auto &from){for(const auto &v:from)if(ids.contains(v->id)){auto copy=std::make_shared<cloud::Cloud>(*v);copy->id=ids.at(v->id);for(auto &source:copy->config.sources)if(ids.contains(source))source=ids.at(source);to.push_back(std::move(copy));}};
  copy_water(d.waters,original.waters);copy_water(s.water_overrides,state.water_overrides);copy_cloud(d.clouds,original.clouds);copy_cloud(s.cloud_overrides,state.cloud_overrides);
  d.operations.push_back({{"op","copy_nodes"},{"nodes",roots},{"children",children},{"prefix",prefix}});daz::apply_graft_masks(d.loaded);release_load_data(d);d.loaded.scene.validate();++s.revision;return copies;
}
}
