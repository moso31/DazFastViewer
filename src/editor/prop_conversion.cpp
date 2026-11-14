#include "editor/document.h"
#include "editor/group_transforms.h"
#include "editor/node_properties.h"
#include "editor/prop_split.h"
#include "runtime/graft_surface.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace dfv::editor {
namespace {
using J=nlohmann::json;
size_t target_index(const Document &d,const std::string &id){
  for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].id==id)return t;
  throw std::runtime_error("Prop 对象不存在："+id);
}
J mesh_json(const ir::Mesh &mesh){
  J result={{"id",mesh.id},{"slots",mesh.material_slots},{"smooth",mesh.smooth},{"positions",J::array()},{"triangles",J::array()},{"curves",J::array()}};
  result["polygon_groups"]=mesh.polygon_groups;
  result["prop_regions"]=J::array();for(const auto &r:mesh.prop_regions)result["prop_regions"].push_back({{"id",r.id},{"label",r.label},{"parent",r.parent},{"side",r.side}});
  result["prop_faces"]=mesh.prop_face_regions;result["prop_curves"]=mesh.prop_curve_regions;
  for(auto p:mesh.positions)result["positions"].push_back({p.x,p.y,p.z});
  for(const auto &t:mesh.triangles){J uv=J::array();for(auto p:t.uv)uv.push_back({p.x,p.y});result["triangles"].push_back({{"vertices",t.vertices},{"uv",uv},{"slot",t.material_slot},{"group",t.polygon_group},{"source",t.source_polygon}});}
  for(const auto &c:mesh.curves)result["curves"].push_back({{"vertices",c.vertices},{"uv",{c.uv.x,c.uv.y}},{"slot",c.material_slot}});
  return result;
}
ir::Mesh read_mesh(const J &value){
  ir::Mesh mesh;mesh.id=value.at("id");mesh.material_slots=value.at("slots").get<std::vector<std::string>>();mesh.smooth=value.at("smooth");
  mesh.polygon_groups=value.value("polygon_groups",std::vector<std::string>{});
  for(const auto &r:value.value("prop_regions",J::array()))mesh.prop_regions.push_back({r.at("id"),r.at("label"),r.at("parent"),r.at("side")});
  mesh.prop_face_regions=value.value("prop_faces",std::vector<int>{});mesh.prop_curve_regions=value.value("prop_curves",std::vector<int>{});
  for(const auto &p:value.at("positions"))mesh.positions.push_back({p.at(0).get<float>(),p.at(1).get<float>(),p.at(2).get<float>()});
  for(const auto &v:value.at("triangles")){ir::Triangle t;t.vertices=v.at("vertices").get<std::array<uint32_t,3>>();t.material_slot=v.at("slot");t.polygon_group=v.value("group",0u);t.source_polygon=v.value("source",0u);for(size_t i=0;i<3;++i)t.uv[i]={v.at("uv").at(i).at(0).get<float>(),v.at("uv").at(i).at(1).get<float>()};mesh.triangles.push_back(t);}
  for(const auto &v:value.at("curves")){ir::Curve c;c.vertices=v.at("vertices").get<std::vector<uint32_t>>();c.material_slot=v.at("slot");c.uv={v.at("uv").at(0).get<float>(),v.at("uv").at(1).get<float>()};mesh.curves.push_back(std::move(c));}
  return mesh;
}
// 细分表面和发丝分别固定，曲线顶点不能套用细分后的顶点编号。
ir::Mesh frozen_mesh(const ir::Mesh &source){
  ir::Mesh result;result.id=source.id;result.material_slots=source.material_slots;result.smooth=source.smooth;
  runtime::Subdivision subdivision(source,true);
  if(subdivision.active()){result.positions=subdivision.evaluate(source.positions);result.triangles=subdivision.triangles();}
  else{result.positions=source.positions;for(const auto &t:source.triangles)if(source.draws(t))result.triangles.push_back(t);}
  if(!source.curves.empty()){
    const auto offset=uint32_t(result.positions.size());result.positions.insert(result.positions.end(),source.positions.begin(),source.positions.end());
    result.curves=source.curves;for(auto &curve:result.curves)for(auto &v:curve.vertices)v+=offset;
  }
  return result;
}
}
nlohmann::json prop_mesh_json(const ir::Mesh &mesh){return mesh_json(mesh);}
ir::Mesh read_prop_mesh(const nlohmann::json &value){auto mesh=read_mesh(value);if(!mesh.prop_regions.empty())prop_split_source(mesh);return mesh;}
void restore_prop_draft(Document &d,const J &operation){
  const auto root=operation.at("root").get<std::string>();target_index(d,root);
  for(const auto &id:operation.at("targets"))d.catalog.targets.at(target_index(d,id.get<std::string>())).prop_draft=root;
  auto &target=d.catalog.targets.at(target_index(d,root));target.label+="（待固定 Prop）";
  for(auto &object:d.loaded.objects)if(object.instance==target.instance)object.label=target.label;
  d.operations.push_back(operation);
}
size_t prepare_prop_copy(Document &d,Snapshot &s,size_t selected){
  const auto &source=d.catalog.targets.at(selected);
  if(!source.prop_draft.empty())throw std::runtime_error("当前对象已经是待固定副本，请调整姿势后执行“固定副本为 Prop”");
  const auto original=source.id;
  const auto copies=copy_selection(d,s,{selected},{},{},true);
  if(copies.empty())throw std::runtime_error("当前对象没有可转换的模型");
  const auto prefix=d.operations.back().at("prefix").get<std::string>();const auto root=prefix+original;
  J operation={{"op","prop_draft"},{"root",root},{"targets",J::array()}};
  for(auto t:copies){const auto &id=d.catalog.targets[t].id;
    const bool city_part=std::any_of(d.cities.begin(),d.cities.end(),[&](const auto &city){return id==city->id||id.starts_with(city->id+"/");});
    if(water::find(d.waters,id)||cloud::find(d.clouds,id)||city_part)throw std::runtime_error("请选择普通模型，程序化水体、云或城市不能转换为 Prop");
    operation["targets"].push_back(id);
  }
  restore_prop_draft(d,operation);return target_index(d,root);
}
void restore_static_prop(Document &d,Snapshot &s,const J &operation){
  if(s.generation!=d.generation)throw std::runtime_error("Prop 转换的场景状态已变化");
  const auto root=operation.at("root").get<std::string>();const auto root_node=root.substr(0,root.rfind('/'));
  const auto removed=operation.at("nodes").get<std::set<std::string>>();std::set<uint32_t> instances;
  for(const auto &entry:operation.at("parts")){
    const auto id=entry.at("instance").get<std::string>();const auto found=std::find_if(d.loaded.scene.instances.begin(),d.loaded.scene.instances.end(),[&](const auto &i){return i.id==id;});
    if(found==d.loaded.scene.instances.end())throw std::runtime_error("Prop 网格实例不存在："+id);
    instances.insert(uint32_t(found-d.loaded.scene.instances.begin()));
  }
  std::vector<int> skin_map(d.skeletons.skins.size(),-1);std::vector<runtime::Skin> skins;std::vector<std::vector<runtime::JointPose>> poses;
  for(size_t i=0;i<skin_map.size();++i)if(!instances.contains(d.skeletons.skins[i].instance)){skin_map[i]=int(skins.size());skins.push_back(d.skeletons.skins[i]);poses.push_back(s.poses.at(i));}
  d.skeletons.skins=std::move(skins);d.skeletons.node_formulas.clear();s.poses=std::move(poses);
  for(auto &graph:d.formulas.graphs)if(graph.skin>=0)graph.skin=skin_map.at(size_t(graph.skin));
  std::erase_if(s.pose_pins,[&](const auto &pin){return pin.skin<0||size_t(pin.skin)>=skin_map.size()||skin_map[size_t(pin.skin)]<0;});
  for(auto &pin:s.pose_pins)pin.skin=skin_map.at(size_t(pin.skin));
  std::erase_if(d.loaded.nodes,[&](const auto &node){return removed.contains(node.id);});
  for(const auto &id:removed){s.group_transforms.erase(id);s.node_properties.erase(id);s.parameter_settings.erase(id);}
  for(auto &binding:d.attachments){std::erase_if(binding.items,[&](const auto &i){return removed.contains(i.node);});std::erase_if(binding.nodes,[&](const auto &n){return removed.contains(n.id);});}
  std::erase_if(d.attachments,[](const auto &binding){return binding.items.empty();});
  std::set<std::string> added_nodes;
  for(const auto &entry:operation.at("parts")){
    auto &scene=d.loaded.scene;const auto id=entry.at("instance").get<std::string>();auto instance=std::find_if(scene.instances.begin(),scene.instances.end(),[&](const auto &i){return i.id==id;});const auto index=uint32_t(instance-scene.instances.begin());
    auto mesh=read_prop_mesh(entry.at("mesh"));s.subdivision_levels.erase(scene.meshes.at(instance->mesh).id);
    // 即使原来是共享实例，固定后也持有自己的网格。
    instance->mesh=uint32_t(scene.meshes.size());scene.meshes.push_back(std::move(mesh));
    instance->transform.value=entry.at("transform").get<std::array<float,12>>();instance->visible=entry.at("visible");
    instance->prototype=instance->graft_source=instance->shell_source=instance->shell_root=-1;instance->shell_offset=0;instance->graft_enabled=true;
    auto target=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.instance==index;});
    size_t t=size_t(target-d.catalog.targets.begin());const auto target_id=entry.at("target").get<std::string>();const auto node=target_id.substr(0,target_id.rfind('/'));
    const auto label=entry.at("label").get<std::string>();const std::string parent=node==root_node?std::string{}:"#"+root_node;
    runtime::Target prop;prop.id=target_id;prop.label=label;prop.instance=index;prop.parent=parent;if(!parent.empty())prop.ancestors={parent};prop.content_type="Prop";prop.static_prop=true;
    prop.initial_visible=instance->visible;prop.initial_selectable=t<s.values.size()?s.values[t].selectable:true;prop.has_edit_frame=true;prop.edit_frame=ir::Transform::translate(instance->transform.point({}));
    prop.native_extension.kind=runtime::ExtensionKind::density;
    runtime::Properties value;if(t<s.values.size()){value.selectable=s.values[t].selectable;value.local_shadow=s.values[t].local_shadow;value.extension.density=s.values[t].extension.density;}
    value.visible=instance->visible;value.extension.kind=runtime::ExtensionKind::density;
    if(t==d.catalog.targets.size()){d.catalog.targets.push_back(prop);d.formulas.graphs.emplace_back();s.values.push_back(value);}
    else{d.catalog.targets[t]=prop;d.formulas.graphs[t]={};s.values[t]=value;}
    instance->instance_node=node;instance->instance_group.clear();instance->instance_label=label;s.instance_ground.erase(id);
    if(auto settings=s.parameter_settings.find(id);settings!=s.parameter_settings.end())std::erase_if(settings->second,[](const auto &p){return !p.first.starts_with("material/");});
    auto object=std::find_if(d.loaded.objects.begin(),d.loaded.objects.end(),[&](const auto &o){return o.instance==index;});
    daz::AssetObject asset;asset.instance=index;asset.id=node;asset.label=label;asset.parent=parent;asset.content_type="Prop";asset.edit_frame=prop.edit_frame;
    if(object!=d.loaded.objects.end()){asset.archive=object->archive;asset.material_selection_sets=object->material_selection_sets;*object=std::move(asset);}else d.loaded.objects.push_back(std::move(asset));
    if(added_nodes.insert(node).second){daz::AssetNode n;n.id=node;n.parent=parent;n.label=label;n.world=instance->transform;n.edit_frame=prop.edit_frame;d.loaded.nodes.push_back(std::move(n));}
  }
  // 固定后的可见面已写入几何，不再依靠角色的 GeoGraft 遮盖关系。
  daz::apply_graft_masks(d.loaded);collect_resources(d);release_load_data(d);++d.asset_revision;++s.revision;d.operations.push_back(operation);
}
size_t freeze_prop_copy(Document &d,Snapshot &s,size_t selected){
  const auto root=d.catalog.targets.at(selected).prop_draft;if(root.empty())throw std::runtime_error("请先创建可调姿势的 Prop 副本");
  const auto root_target=target_index(d,root);const ObjectHierarchy hierarchy(d);const auto root_node=hierarchy.targets[root_target];
  std::set<std::string> nodes;std::set<uint32_t> instances;
  for(const auto &[id,parent]:hierarchy.parents)if(hierarchy.contains(root_node,id))nodes.insert(id);
  for(const auto &target:d.catalog.targets)if(target.prop_draft==root){instances.insert(target.instance);nodes.insert(hierarchy.instances[target.instance]);}
  for(uint32_t i=0;i<hierarchy.instances.size();++i)if(nodes.contains(hierarchy.instances[i]))instances.insert(i);
  for(const auto &skin:d.skeletons.skins)if(instances.contains(skin.instance))for(const auto &joint:skin.joints)if(!joint.scene_id.empty())nodes.insert(joint.scene_id);
  auto source=transformed_groups(std::make_shared<Document>(d),s.group_transforms);auto scene=source->loaded.scene;
  runtime::DeformationRuntime deformation(scene,source->catalog.targets,source->skeletons.skins,source->formulas.graphs);deformation.evaluate(scene_properties(*source,s),s.poses);
  apply_instance_node_visibility(*source,s,scene);apply_subdivision_levels(scene,s.subdivision_levels);
  const GroupFrames frames(d,s.group_transforms);const auto bases=group_instance_bases(d,scene,hierarchy,deformation.effective_poses(),&frames);
  apply_instance_ground(scene,d.loaded.scene,s.instance_ground,nullptr,&bases);
  std::map<uint32_t,std::pair<ir::Mesh,ir::Transform>> baked;
  for(const auto &group:runtime::graft_groups(scene))if(std::any_of(group.begin(),group.end(),[&](auto i){return instances.contains(i);})){runtime::GraftSurface surface(scene,group,true);
    for(size_t p=0;p<group.size();++p)if(instances.contains(group[p])){const auto i=group[p];const auto &original=scene.meshes.at(scene.instances[i].mesh);ir::Mesh mesh;mesh.id=original.id;mesh.material_slots=original.material_slots;mesh.smooth=original.smooth;mesh.triangles=surface.triangles(p);for(auto v:surface.vertices(p))mesh.positions.push_back(surface.positions().at(v));
      if(!original.curves.empty()){
        const auto offset=uint32_t(mesh.positions.size());const auto relative=ir::inverse(surface.transform(scene,i))*scene.instances[i].transform;
        for(auto position:original.positions)mesh.positions.push_back(relative.point(position));mesh.curves=original.curves;for(auto &curve:mesh.curves)for(auto &v:curve.vertices)v+=offset;
      }
      baked.emplace(i,std::make_pair(std::move(mesh),surface.transform(scene,i)));
    }
  }
  for(auto i:instances)if(scene.instances[i].prototype>=0){auto prototype=scene.instances[i].prototype;std::set<int> seen;while(prototype>=0&&scene.instances[size_t(prototype)].prototype>=0&&seen.insert(prototype).second)prototype=scene.instances[size_t(prototype)].prototype;
    if(prototype>=0)if(auto source=baked.find(uint32_t(prototype));source!=baked.end())baked.emplace(i,std::make_pair(source->second.first,scene.instances[i].transform*ir::inverse(scene.instances[size_t(prototype)].transform)*source->second.second));
  }
  J operation={{"op","static_prop"},{"root",root},{"nodes",nodes},{"parts",J::array()}};
  for(auto i:instances){const auto &instance=scene.instances.at(i);auto found=baked.find(i);auto mesh=found!=baked.end()?std::move(found->second.first):frozen_mesh(scene.meshes.at(instance.mesh));
    auto origin=i;std::set<uint32_t> seen;while(scene.instances[origin].prototype>=0&&seen.insert(origin).second)origin=uint32_t(scene.instances[origin].prototype);
    const runtime::Skin *skin=nullptr;for(const auto &v:source->skeletons.skins)if(v.instance==origin){skin=&v;break;}
    capture_prop_regions(mesh,scene.meshes.at(scene.instances[origin].mesh),skin);
    // 网格身份必须独立，不能与未转换的共享原型冲突。
    mesh.id=instance.id+"/static-prop";
    const auto transform=found!=baked.end()?found->second.second:instance.transform;
    const auto target=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.instance==i;});const auto id=target==d.catalog.targets.end()?instance.id:target->id;
    auto label=target==d.catalog.targets.end()?instance.instance_label:target->label;const std::string pending="（待固定 Prop）";if(auto at=label.find(pending);at!=std::string::npos)label.erase(at,pending.size());if(id==root)label+="（Prop）";
    operation["parts"].push_back({{"instance",instance.id},{"target",id},{"label",label},{"mesh",mesh_json(mesh)},{"transform",transform.value},{"visible",instance.visible}});
  }
  restore_static_prop(d,s,operation);return target_index(d,root);
}
}
