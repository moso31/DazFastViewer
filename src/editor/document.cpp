#include "editor/document.h"
#include "editor/group_transforms.h"
#include "daz/documents.h"
#include "daz/material_uv.h"
#include "runtime/graft_surface.h"
#include <algorithm>
#include <map>
#include <set>

namespace dfv::editor {
const ir::Mesh &subdivision_mesh(const Document &document,size_t target) {
  const auto &scene=document.loaded.scene;auto instance=document.catalog.targets.at(target).instance;
  if(scene.instances.at(instance).prototype>=0) instance=uint32_t(scene.instances[instance].prototype);
  return scene.meshes.at(scene.instances.at(runtime::graft_root(scene,instance)).mesh);
}
int subdivision_level(const Document &document,const Snapshot &snapshot,size_t target) {
  const auto &mesh=subdivision_mesh(document,target);
  const auto found=snapshot.subdivision_levels.find(mesh.id);if(found!=snapshot.subdivision_levels.end()) return found->second;
  return mesh.subdivision.enabled?std::max(mesh.subdivision.level,mesh.subdivision.render_level):0;
}
bool apply_subdivision_levels(ir::Scene &scene,const std::map<std::string,int> &levels) {
  for(const auto &[id,level]:levels) if(level<0||level>6) throw std::runtime_error("细分等级超出支持范围");
  bool changed=false;
  for(auto &mesh:scene.meshes) if(!mesh.polygons.empty()) {
    auto next=mesh.subdivision;const auto found=levels.find(mesh.id);
    const int level=found==levels.end()?(next.enabled?std::max(next.level,next.render_level):0):found->second;
    if(level<0||level>6) throw std::runtime_error("细分等级超出支持范围");
    next.enabled=level>0;next.level=next.render_level=level;changed|=next!=mesh.subdivision;mesh.subdivision=next;
  }
  for(const auto &group:runtime::graft_groups(scene)) {
    const auto settings=scene.meshes[scene.instances[group[0]].mesh].subdivision;
    for(auto member:group) {auto &s=scene.meshes[scene.instances[member].mesh].subdivision;changed|=s.enabled!=settings.enabled||s.level!=settings.level||s.render_level!=settings.render_level;s.enabled=settings.enabled;s.level=settings.level;s.render_level=settings.render_level;}
  }
  return changed;
}
std::shared_ptr<Document> refresh_parameters(const Document &document,size_t selected,const std::vector<std::filesystem::path> &roots,const std::function<void(const std::string &)> &progress) {
  const auto &target=document.catalog.targets.at(selected);
  auto loaded=document.loaded;
  std::set<std::string> selected_nodes{target.id.substr(0,target.id.rfind('/'))};
  bool changed=true;while(changed) {const auto n=selected_nodes.size();
    for(const auto &node:loaded.nodes) if(node.parent.starts_with('#')&&selected_nodes.contains(node.parent.substr(1))) selected_nodes.insert(node.id);
    for(const auto &o:loaded.objects)
    if((o.parent.starts_with('#')&&selected_nodes.contains(o.parent.substr(1)))||(o.conform_target.starts_with('#')&&selected_nodes.contains(o.conform_target.substr(1)))) selected_nodes.insert(o.id);changed=selected_nodes.size()!=n;}
  std::erase_if(loaded.objects,[&](const auto &o){return !selected_nodes.contains(o.id);});
  // 追加的对象仍从各自原始 DUF 获取覆盖与节点公式，不使用主文档的来源。
  std::map<std::filesystem::path,std::vector<daz::AssetObject>> groups;
  for(auto object:loaded.objects) {
    for(const auto &[file,version]:object.geometry_versions) if(daz::file_version(file)!=version) throw std::runtime_error("基础几何资产已变化，请重新打开场景后再刷新参数");
    const auto source=object.source_file;if(!object.source_node.empty()) object.id=object.source_node;groups[source].push_back(std::move(object));
  }
  auto result=std::make_shared<Document>(document);
  for(const auto &[source,objects]:groups) {
  loaded.objects=objects;if(!source.empty()) {const auto u8=source.generic_u8string();loaded.report["input"]=std::string(u8.begin(),u8.end());}
  auto catalog=daz::discover_morphs(loaded,roots,progress,true);
  auto source_skins=daz::load_skeletons(loaded);auto skins=document.skeletons;skins.node_formulas.resize(skins.skins.size());
  for(size_t s=0;s<source_skins.skins.size();++s) for(size_t old=0;old<skins.skins.size();++old) if(source_skins.skins[s].instance==skins.skins[old].instance) skins.node_formulas[old]=std::move(source_skins.node_formulas[s]);
  auto formulas=daz::enable_formulas(catalog,skins);
  for(size_t t=0;t<catalog.targets.size();++t) {
    const auto found=std::find_if(result->catalog.targets.begin(),result->catalog.targets.end(),[&](const auto &old){return old.id==catalog.targets[t].id;});
    if(found==result->catalog.targets.end()) throw std::runtime_error("刷新时对象身份已变化");
    catalog.targets[t].initial_visible=found->initial_visible;catalog.targets[t].ancestors_visible=found->ancestors_visible;
    const size_t index=size_t(found-result->catalog.targets.begin());*found=std::move(catalog.targets[t]);result->formulas.graphs[index]=std::move(formulas.graphs[t]);
  }
  }
  ++result->asset_revision;return result;
}
namespace {
template<class T> std::vector<int> retain(std::vector<T> &items,const std::vector<bool> &keep) {
  std::vector<int> mapping(items.size(),-1);std::vector<T> retained;
  for(size_t i=0;i<items.size();++i) if(keep.at(i)) {mapping[i]=int(retained.size());retained.push_back(std::move(items[i]));}
  items=std::move(retained);return mapping;
}
std::string node_id(const runtime::Target &target) {return target.id.substr(0,target.id.rfind('/'));}
bool refers_to(const std::string &uri,const std::set<std::string> &ids) {return uri.starts_with('#')&&ids.contains(uri.substr(1));}
}
void release_load_data(Document &document) {
  document.loaded.source_documents.clear();
  // 原始公式和完整诊断已写入加载报告；运行时只保留已编译的图与可编辑数据。
  document.catalog.formulas={};document.skeletons.node_formulas={};
  document.catalog.report={{"targets",document.catalog.targets.size()}};
  document.skeletons.report={{"skins",document.skeletons.skins.size()}};
  document.formulas.report={{"graphs",document.formulas.graphs.size()}};
  if(!document.loaded.report.is_object()) document.loaded.report=nlohmann::json::object();
  document.loaded.report.erase("imports");document.loaded.report["instances"]=document.loaded.scene.instances.size();
}
void collect_resources(Document &document) {
  auto &scene=document.loaded.scene;
  std::vector<bool> meshes(scene.meshes.size()),materials(scene.materials.size());
  for(const auto &instance:scene.instances) {meshes.at(instance.mesh)=true;for(auto m:instance.materials) materials.at(m)=true;}
  const auto mesh_map=retain(scene.meshes,meshes),material_map=retain(scene.materials,materials);
  for(auto &instance:scene.instances) {instance.mesh=uint32_t(mesh_map.at(instance.mesh));for(auto &m:instance.materials) m=uint32_t(material_map.at(m));}
  std::vector<ir::Texture> textures;std::map<std::pair<std::string,ir::ColorSpace>,int> texture_map;
  for(auto &material:scene.materials) for(auto *index:ir::texture_indices(material)) if(*index>=0) {
    const auto &texture=scene.textures.at(size_t(*index));const auto key=std::make_pair(texture.id,texture.colorspace);
    auto [found,inserted]=texture_map.emplace(key,int(textures.size()));if(inserted) textures.push_back(texture);*index=found->second;
  }
  scene.textures=std::move(textures);scene.validate();
}
static size_t remove_nodes(Document &document,Snapshot &snapshot,std::set<std::string> removed) {
  if(snapshot.generation!=document.generation) throw std::runtime_error("删除对象的文档世代不匹配");
  auto &scene=document.loaded.scene;
  // 骨骼和空节点也参与闭包，以清理挂在手、头等部位上的附件。
  bool changed=true;
  while(changed) {
    const auto count=removed.size();
    for(const auto &node:document.loaded.nodes) if(refers_to(node.parent,removed)) removed.insert(node.id);
    for(const auto &object:document.loaded.objects) if(refers_to(object.parent,removed)||refers_to(object.conform_target,removed)||refers_to(object.rigid_follow.target,removed)) removed.insert(object.id);
    for(const auto &t:document.catalog.targets) if(refers_to(t.parent,removed)||refers_to(t.conform_target,removed)||refers_to(t.rigid_follow.target,removed)) removed.insert(node_id(t));
    changed=removed.size()!=count;
  }
  std::vector<bool> instances(scene.instances.size(),true);
  for(const auto &object:document.loaded.objects) if(removed.contains(object.id)) instances.at(object.instance)=false;
  for(const auto &t:document.catalog.targets) if(removed.contains(node_id(t))) instances.at(t.instance)=false;
  for(size_t i=0;i<scene.instances.size();++i) {const auto &v=scene.instances[i];if((v.shell_source>=0&&!instances.at(size_t(v.shell_source)))||(v.prototype>=0&&(!instances.at(size_t(v.prototype))||removed.contains(v.instance_node)))) instances[i]=false;}
  std::vector<bool> targets,skins;
  for(const auto &t:document.catalog.targets) targets.push_back(instances.at(t.instance));
  for(const auto &s:document.skeletons.skins) skins.push_back(instances.at(s.instance));
  const auto instance_map=retain(scene.instances,instances),skin_map=retain(document.skeletons.skins,skins);
  for(auto &i:scene.instances) {if(i.prototype>=0) i.prototype=instance_map.at(size_t(i.prototype));if(i.graft_source>=0) i.graft_source=instance_map.at(size_t(i.graft_source));if(i.shell_source>=0)i.shell_source=instance_map.at(size_t(i.shell_source));if(i.shell_root>=0)i.shell_root=instance_map.at(size_t(i.shell_root));}
  retain(snapshot.poses,skins);retain(document.catalog.targets,targets);retain(snapshot.values,targets);retain(document.formulas.graphs,targets);
  for(auto &t:document.catalog.targets) {t.instance=uint32_t(instance_map.at(t.instance));if(refers_to(t.smoothing.collision_target,removed)) t.smoothing.collision_target.clear();}
  for(auto &s:document.skeletons.skins) s.instance=uint32_t(instance_map.at(s.instance));
  for(auto &g:document.formulas.graphs) if(g.skin>=0) g.skin=skin_map.at(size_t(g.skin));
  std::erase_if(document.loaded.objects,[&](const auto &o) {return removed.contains(o.id)||instance_map.at(o.instance)<0;});
  for(auto &o:document.loaded.objects) {o.instance=uint32_t(instance_map.at(o.instance));if(refers_to(o.smoothing.collision_target,removed)) o.smoothing.collision_target.clear();}
  std::erase_if(document.loaded.nodes,[&](const auto &n) {return removed.contains(n.id);});
  for(auto &binding:document.attachments) {
    std::erase_if(binding.items,[&](const auto &i){return removed.contains(i.node);});
    std::erase_if(binding.nodes,[&](const auto &n){return removed.contains(n.id);});
  }
  std::erase_if(document.attachments,[](const auto &b){return b.items.empty();});
  std::erase_if(snapshot.lights,[&](const auto &l) {return removed.contains(l.id);});scene.lights=snapshot.lights;
  daz::apply_graft_masks(document.loaded);collect_resources(document);prune_material_overrides(scene,snapshot.material_overrides);prune_instance_ground(scene,snapshot.instance_ground);prune_group_transforms(document,snapshot.group_transforms);release_load_data(document);++snapshot.revision;
  return std::count(targets.begin(),targets.end(),false);
}
size_t remove_target(Document &document,Snapshot &snapshot,size_t target) {
  const auto &selected=document.catalog.targets.at(target);const auto id=selected.id;std::set<std::string> removed{node_id(selected)};
  for(const auto &object:document.loaded.objects) if(object.instance==selected.instance) removed.insert(object.id);
  const auto count=remove_nodes(document,snapshot,std::move(removed));document.operations.push_back({{"op","remove"},{"target",id}});return count;
}
size_t remove_light(Document &document,Snapshot &snapshot,size_t light) {
  const auto id=snapshot.lights.at(light).id;const auto count=remove_nodes(document,snapshot,{id});
  document.operations.push_back({{"op","remove_light"},{"id",id}});return count;
}
namespace {
bool material_group_matches(const Document &d,const daz::LoadedScene &preset,size_t instance,const std::string &slot,const std::string &group){
  if(group==slot||group=="*")return true;
  if(group!="Torso"||(slot!="Body"&&slot!="Head"))return false;
  // Genesis 8.1 split the G8 Torso surface into Body and Head. The G8 UV
  // preset is applied per destination surface below, so both use its torso map.
  bool genesis81=false;for(const auto &o:d.loaded.objects)if(o.instance==instance){const auto file=o.geometry_file.filename().wstring();genesis81|=file==L"Genesis8_1Female.dsf"||file==L"Genesis8_1Male.dsf";}
  if(!genesis81)return false;
  for(const auto &m:preset.report.at("materials"))for(const auto &g:m.at("groups"))if(g==slot)return false;
  return true;
}
bool material_owner_matches(const Document &d,const ir::Material &material,size_t instance){
  if(material.source_definition.empty())return true;const auto definition=nlohmann::json::parse(material.source_definition);const auto node=definition.value("target_node",std::string{});if(node.empty())return true;
  for(const auto &object:d.loaded.objects)if(object.instance==instance){if(object.id==node||object.source_node==node)return true;
    auto uri=definition.value("target_uri",std::string{});if(uri.ends_with(':'))uri.pop_back();const auto slash=uri.rfind('/');const auto name=slash==std::string::npos?uri:uri.substr(slash+1);if(object.id==name||object.source_node==name)return true;
    if(definition.value("target_root",false)){for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].instance==instance){try{return attachment_host(d,t)==t&&object.parent.empty();}catch(const std::exception &){return object.parent.empty();}}}
  }
  return false;
}
void replace_surface_material(Document &d,Snapshot *snapshot,const daz::LoadedScene &preset,size_t instance,size_t slot,size_t material_index,int offset){
  auto &scene=d.loaded.scene;auto &i=scene.instances.at(instance);const auto &incoming=preset.scene.materials.at(material_index);const auto base=scene.materials.at(i.materials.at(slot));auto material=incoming;auto touched=material_preset_parameters(incoming);
  if(incoming.source_definition.empty()){for(auto *index:ir::texture_indices(material))if(*index>=0)*index+=offset;}
  else {daz::LoadOptions options;for(const auto &root:(d.loaded.report.is_object()?d.loaded.report.value("content_roots",nlohmann::json::array()):nlohmann::json::array()))options.content_roots.push_back(std::filesystem::u8path(root.get<std::string>()));for(const auto &root:preset.report.value("content_roots",nlohmann::json::array()))options.content_roots.push_back(std::filesystem::u8path(root.get<std::string>()));daz::apply_material_uv(scene,instance,slot,incoming,options);material=daz::merge_material_preset(base,incoming,scene.textures,options);touched=material_preset_parameters(incoming,&material);for(const auto &p:material_parameters())if(!touched.contains(p.id))p.copy(material,base);}
  const auto actual_uv=daz::material_uv_set(scene.materials.at(i.materials.at(slot)));if(!actual_uv.uri.empty())daz::set_material_uv_set(material,actual_uv);
  ir::validate(material,scene.textures.size());i.materials[slot]=uint32_t(scene.materials.size());scene.materials.push_back(std::move(material));
  if(snapshot)if(auto object=snapshot->material_overrides.find(i.id);object!=snapshot->material_overrides.end())if(auto patch=object->second.find(scene.meshes.at(i.mesh).material_slots.at(slot));patch!=object->second.end())for(const auto &id:touched)patch->second.erase(id);
}
}
size_t apply_materials(Document &document,size_t target,const daz::LoadedScene &preset,Snapshot *snapshot) {
  const auto &scene=document.loaded.scene;const auto root=document.catalog.targets.at(target).instance;std::vector<MaterialSurface> surfaces;
  const bool hierarchical=preset.report.value("hierarchical_material",false);const auto selected=node_id(document.catalog.targets.at(target));
  for(size_t t=0;t<document.catalog.targets.size();++t){const auto &candidate=document.catalog.targets[t];bool include=t==target;
    if(hierarchical&&!include){try{include=attachment_host(document,t)==target;}catch(const std::exception &){}include|=std::find(candidate.ancestors.begin(),candidate.ancestors.end(),"#"+selected)!=candidate.ancestors.end();}
    if(include)for(size_t slot=0;slot<scene.instances.at(candidate.instance).materials.size();++slot)surfaces.push_back({candidate.instance,slot});
  }
  auto scratch=Snapshot{};return apply_surface_materials(document,snapshot?*snapshot:scratch,preset,surfaces);
}
size_t apply_surface_materials(Document &d,Snapshot &snapshot,const daz::LoadedScene &preset,const std::vector<MaterialSurface> &surfaces){
  if(preset.report.value("preset_type","")=="preset_layered_image")throw std::runtime_error("独立 LIE 预设需要合并原有底图与图层，目前尚未支持直接叠加。请使用普通材质预设，或先在 DAZ 中合成并保存完整材质。");
  auto &scene=d.loaded.scene;std::map<std::pair<size_t,size_t>,std::vector<size_t>> matches;
  for(auto surface:surfaces){const auto &i=scene.instances.at(surface.instance);const auto &slot=scene.meshes.at(i.mesh).material_slots.at(surface.slot);
    for(size_t m=0;m<preset.scene.materials.size();++m)for(const auto &group:preset.report.at("materials").at(m).at("groups"))if(material_group_matches(d,preset,surface.instance,slot,group.get<std::string>())&&material_owner_matches(d,preset.scene.materials[m],surface.instance)){auto &list=matches[{surface.instance,surface.slot}];if(std::find(list.begin(),list.end(),m)==list.end())list.push_back(m);break;}
  }
  if(matches.empty())throw std::runtime_error("材质预设没有匹配选中的表面；通用 Shader 预设可应用于任意表面");
  const auto offset=int(scene.textures.size());scene.textures.insert(scene.textures.end(),preset.scene.textures.begin(),preset.scene.textures.end());nlohmann::json identities=nlohmann::json::array();
  for(const auto &[surface,m]:matches){auto &i=scene.instances.at(surface.first);const auto slot=scene.meshes.at(i.mesh).material_slots.at(surface.second);
    for(auto index:m)replace_surface_material(d,&snapshot,preset,surface.first,surface.second,index,offset);identities.push_back({{"instance",i.id},{"slot",slot}});
  }
  collect_resources(d);prune_material_overrides(scene,snapshot.material_overrides);
  if(preset.report.contains("input"))d.operations.push_back({{"op","surface_materials"},{"surfaces",identities},{"file",preset.report.at("input")}});return matches.size();
}
std::vector<std::string> paste_material(Document &d,Snapshot &snapshot,const nlohmann::json &copy,const std::vector<MaterialSurface> &surfaces){
  std::vector<std::string> warnings;if(surfaces.empty())return warnings;auto &scene=d.loaded.scene;auto textures=scene.textures;
  const auto copied=read_copied_material(copy,textures);nlohmann::json identities=nlohmann::json::array();
  for(auto s:surfaces){const auto &i=scene.instances.at(s.instance);identities.push_back({{"instance",i.id},{"slot",scene.meshes.at(i.mesh).material_slots.at(s.slot)}});}
  daz::LoadOptions options;if(d.loaded.report.is_object())for(const auto &root:d.loaded.report.value("content_roots",nlohmann::json::array()))options.content_roots.push_back(std::filesystem::u8path(root.get<std::string>()));
  scene.textures=std::move(textures);
  for(auto s:surfaces){auto &i=scene.instances.at(s.instance);const auto slot=scene.meshes.at(i.mesh).material_slots.at(s.slot);auto m=copied;
    const auto previous_uv=daz::material_uv_set(scene.materials.at(i.materials.at(s.slot)));auto applied_uv=previous_uv;
    if(!daz::material_uv_set(copied).uri.empty()&&copy.contains("uv_topology")){
      try{if(copy.at("uv_topology")!=daz::material_uv_topology(scene.meshes.at(i.mesh)))throw std::runtime_error("源与目标的网格拓扑不同");
        daz::apply_material_uv(scene,s.instance,s.slot,copied,options);applied_uv=daz::material_uv_set(scene.materials.at(i.materials.at(s.slot)));
      }catch(const std::exception &e){warnings.push_back(slot+"：已保留目标 UV Set（"+e.what()+"）");}
    }
    if(applied_uv!=daz::material_uv_set(m))daz::set_material_uv_set(m,applied_uv);
    m.id="pasted/"+i.id+"/"+slot;i.materials.at(s.slot)=uint32_t(scene.materials.size());scene.materials.push_back(std::move(m));
    if(auto object=snapshot.material_overrides.find(i.id);object!=snapshot.material_overrides.end())object->second.erase(slot);
  }
  collect_resources(d);prune_material_overrides(scene,snapshot.material_overrides);
  d.operations.push_back({{"op","paste_material"},{"surfaces",identities},{"material",copy}});
  return warnings;
}
bool change_material_uv(Document &d,const daz::MaterialUVSet &uv,const std::vector<MaterialSurface> &surfaces){
  if(uv.uri.empty()||surfaces.empty())return false;daz::LoadOptions options;
  if(d.loaded.report.is_object())for(const auto &root:d.loaded.report.value("content_roots",nlohmann::json::array()))options.content_roots.push_back(std::filesystem::u8path(root.get<std::string>()));
  ir::Material request;daz::set_material_uv_set(request,uv);auto identities=nlohmann::json::array();bool changed=false;
  for(auto s:surfaces){const auto &i=d.loaded.scene.instances.at(s.instance);const auto slot=d.loaded.scene.meshes.at(i.mesh).material_slots.at(s.slot);
    const auto &before=d.loaded.scene.materials.at(i.materials.at(s.slot));const auto baseline=daz::material_uv_baseline(before).value_or(daz::material_uv_set(before));
    const bool edited=daz::apply_material_uv(d.loaded.scene,s.instance,s.slot,request,options);changed|=edited;
    if(edited){auto &material=d.loaded.scene.materials.at(i.materials.at(s.slot));auto definition=nlohmann::json::parse(material.source_definition);
      if(daz::material_uv_set(material).uri==baseline.uri)definition.erase("uv_baseline");else definition["uv_baseline"]={{"uri",baseline.uri},{"owner",baseline.owner},{"label",baseline.label}};material.source_definition=definition.dump();}
    identities.push_back({{"instance",i.id},{"slot",slot}});
  }
  if(!changed)return false;daz::sync_instance_meshes(d.loaded.scene);collect_resources(d);
  d.operations.push_back({{"op","material_uv"},{"surfaces",identities},{"uri",uv.uri},{"owner",uv.owner},{"label",uv.label}});return true;
}
bool reset_material_uv(Document &d,const std::vector<MaterialSurface> &surfaces){
  bool changed=false;for(auto s:surfaces){const auto &i=d.loaded.scene.instances.at(s.instance);const auto baseline=daz::material_uv_baseline(d.loaded.scene.materials.at(i.materials.at(s.slot)));if(baseline&&!baseline->uri.empty())changed|=change_material_uv(d,*baseline,{s});}return changed;
}
Snapshot initial_snapshot(const Document &document) {
  Snapshot result;result.options=document.loaded.scene.options;result.generation=document.generation;result.revision=1;
  for(const auto &skin:document.skeletons.skins) result.poses.push_back(skin.initial);
  for(const auto &target:document.catalog.targets) {
    runtime::Properties p;p.extension=target.native_extension;for(const auto &m:target.morphs) p.morphs.push_back(m.evaluable||m.unsupported.empty()?m.initial:0);
    p.visible=target.initial_visible.value_or(document.loaded.scene.instances.at(target.instance).visible);
    runtime::sync_aliases(target,p);result.values.push_back(std::move(p));
  }
  result.lights=document.loaded.scene.lights;return result;
}
void append_document(Document &destination,Document source,const std::string &identity_prefix) {
  auto &a=destination.loaded.scene;auto &b=source.loaded.scene;
  const int textures=int(a.textures.size());const auto materials=uint32_t(a.materials.size()),meshes=uint32_t(a.meshes.size()),instances=uint32_t(a.instances.size());
  const auto skins=int(destination.skeletons.skins.size());
  auto prefix=identity_prefix;
  if(prefix.empty()) {
    auto used=[&](const auto &items){return std::any_of(items.begin(),items.end(),[&](const auto &v){return v.id.starts_with(prefix);});};
    auto serial=destination.generation;do {prefix="instance-"+std::to_string(serial++)+"/";} while(used(destination.catalog.targets)||used(a.lights)||used(destination.loaded.nodes)||used(a.meshes));
  }
  const auto path=source.source_file.generic_u8string();destination.operations.push_back({{"op","append"},{"prefix",prefix},{"file",std::string(path.begin(),path.end())},{"operations",source.operations}});
  a.textures.insert(a.textures.end(),b.textures.begin(),b.textures.end());
  for(auto m:b.materials) {
    m.id=prefix+m.id;
    for(auto *index:ir::texture_indices(m)) if(*index>=0) *index+=textures;
    a.materials.push_back(std::move(m));
  }
  for(auto &m:b.meshes) {m.id=prefix+m.id;a.meshes.push_back(std::move(m));}
  for(auto i:b.instances) {i.id=prefix+i.id;i.mesh+=meshes;if(i.prototype>=0) i.prototype+=int(instances);if(i.graft_source>=0) i.graft_source+=int(instances);if(i.shell_source>=0)i.shell_source+=int(instances);if(i.shell_root>=0)i.shell_root+=int(instances);if(!i.instance_node.empty()) i.instance_node=prefix+i.instance_node;if(!i.instance_group.empty()) i.instance_group=prefix+i.instance_group;for(auto &m:i.materials) m+=materials;a.instances.push_back(std::move(i));}
  for(auto l:b.lights) {l.id=prefix+l.id;a.lights.push_back(std::move(l));}
  for(auto n:source.loaded.nodes) {n.id=prefix+n.id;if(n.parent.starts_with('#')) n.parent="#"+prefix+n.parent.substr(1);destination.loaded.nodes.push_back(std::move(n));}
  for(auto o:source.loaded.objects) {if(o.rigid_follow.target.starts_with('#')) o.rigid_follow.target="#"+prefix+o.rigid_follow.target.substr(1);o.instance+=instances;o.id=prefix+o.id;if(o.parent.starts_with('#')) o.parent="#"+prefix+o.parent.substr(1);if(o.conform_target.starts_with('#')) o.conform_target="#"+prefix+o.conform_target.substr(1);if(o.smoothing.collision_target.starts_with('#')) o.smoothing.collision_target="#"+prefix+o.smoothing.collision_target.substr(1);destination.loaded.objects.push_back(std::move(o));}
  for(auto &t:source.catalog.targets) {if(t.rigid_follow.target.starts_with('#')) t.rigid_follow.target="#"+prefix+t.rigid_follow.target.substr(1);t.instance+=instances;t.id=prefix+t.id;if(t.parent.starts_with('#')) t.parent="#"+prefix+t.parent.substr(1);for(auto &ancestor:t.ancestors) if(ancestor.starts_with('#')) ancestor="#"+prefix+ancestor.substr(1);if(t.conform_target.starts_with('#')) t.conform_target="#"+prefix+t.conform_target.substr(1);if(t.smoothing.collision_target.starts_with('#')) t.smoothing.collision_target="#"+prefix+t.smoothing.collision_target.substr(1);destination.catalog.targets.push_back(std::move(t));}
  for(auto &s:source.skeletons.skins) {s.instance+=instances;s.id=prefix+s.id;for(auto &j:s.joints) if(!j.scene_id.empty()) j.scene_id=prefix+j.scene_id;destination.skeletons.skins.push_back(std::move(s));}
  for(auto &g:source.formulas.graphs) {if(g.skin>=0) g.skin+=skins;destination.formulas.graphs.push_back(std::move(g));}
  auto rename=[&](std::string &uri){if(uri.starts_with('#')) uri="#"+prefix+uri.substr(1);};
  for(auto &b:source.attachments) {
    if(!b.host.empty()) b.host=prefix+b.host;
    for(auto &i:b.items) {i.node=prefix+i.node;rename(i.parent);rename(i.conform);rename(i.collision);rename(i.rigid);}
    for(auto &n:b.nodes) {n.id=prefix+n.id;rename(n.parent);}
    destination.attachments.push_back(std::move(b));
  }
  release_load_data(destination);
  a.validate();
}
}
