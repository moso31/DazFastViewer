#include "editor/document.h"
#include "daz/documents.h"
#include <algorithm>
#include <map>
#include <set>

namespace dfv::editor {
int subdivision_level(const Document &document,const Snapshot &snapshot,size_t target) {
  const auto &mesh=document.loaded.scene.meshes.at(document.loaded.scene.instances.at(document.catalog.targets.at(target).instance).mesh);
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
  for(size_t i=0;i<scene.instances.size();++i) {const auto &v=scene.instances[i];if(v.prototype>=0&&(!instances.at(size_t(v.prototype))||removed.contains(v.instance_node))) instances[i]=false;}
  std::vector<bool> targets,skins;
  for(const auto &t:document.catalog.targets) targets.push_back(instances.at(t.instance));
  for(const auto &s:document.skeletons.skins) skins.push_back(instances.at(s.instance));
  const auto instance_map=retain(scene.instances,instances),skin_map=retain(document.skeletons.skins,skins);
  for(auto &i:scene.instances) if(i.prototype>=0) i.prototype=instance_map.at(size_t(i.prototype));
  retain(snapshot.poses,skins);retain(document.catalog.targets,targets);retain(snapshot.values,targets);retain(document.formulas.graphs,targets);
  for(auto &t:document.catalog.targets) {t.instance=uint32_t(instance_map.at(t.instance));if(refers_to(t.smoothing.collision_target,removed)) t.smoothing.collision_target.clear();}
  for(auto &s:document.skeletons.skins) s.instance=uint32_t(instance_map.at(s.instance));
  for(auto &g:document.formulas.graphs) if(g.skin>=0) g.skin=skin_map.at(size_t(g.skin));
  std::erase_if(document.loaded.objects,[&](const auto &o) {return removed.contains(o.id);});
  for(auto &o:document.loaded.objects) {o.instance=uint32_t(instance_map.at(o.instance));if(refers_to(o.smoothing.collision_target,removed)) o.smoothing.collision_target.clear();}
  std::erase_if(document.loaded.nodes,[&](const auto &n) {return removed.contains(n.id);});
  for(auto &binding:document.attachments) {
    std::erase_if(binding.items,[&](const auto &i){return removed.contains(i.node);});
    std::erase_if(binding.nodes,[&](const auto &n){return removed.contains(n.id);});
  }
  std::erase_if(document.attachments,[](const auto &b){return b.items.empty();});
  std::erase_if(snapshot.lights,[&](const auto &l) {return removed.contains(l.id);});scene.lights=snapshot.lights;
  daz::apply_graft_masks(document.loaded);collect_resources(document);release_load_data(document);++snapshot.revision;
  return std::count(targets.begin(),targets.end(),false);
}
size_t remove_target(Document &document,Snapshot &snapshot,size_t target) {
  const auto &selected=document.catalog.targets.at(target);std::set<std::string> removed{node_id(selected)};
  for(const auto &object:document.loaded.objects) if(object.instance==selected.instance) removed.insert(object.id);
  return remove_nodes(document,snapshot,std::move(removed));
}
size_t remove_light(Document &document,Snapshot &snapshot,size_t light) {
  return remove_nodes(document,snapshot,{snapshot.lights.at(light).id});
}
size_t apply_materials(Document &document,size_t target,const daz::LoadedScene &preset) {
  auto &scene=document.loaded.scene;auto &instance=scene.instances.at(document.catalog.targets.at(target).instance);
  const auto &slots=scene.meshes[instance.mesh].material_slots;const int offset=int(scene.textures.size());
  std::vector<std::pair<size_t,size_t>> matches;
  for(size_t m=0;m<preset.scene.materials.size();++m) for(size_t slot=0;slot<slots.size();++slot) {
    const auto &groups=preset.report.at("materials").at(m).at("groups");bool matched=false;
    for(const auto &group:groups) matched|=group.get<std::string>()==slots[slot];
    if(matched) matches.emplace_back(m,slot);
  }
  if(matches.empty()) throw std::runtime_error("材质预设没有匹配当前对象的表面组");
  scene.textures.insert(scene.textures.end(),preset.scene.textures.begin(),preset.scene.textures.end());
  for(const auto &[m,slot]:matches) {
    auto material=preset.scene.materials[m];for(auto *index:ir::texture_indices(material)) if(*index>=0) *index+=offset;
    instance.materials[slot]=uint32_t(scene.materials.size());scene.materials.push_back(std::move(material));
  }
  collect_resources(document);return matches.size();
}
Snapshot initial_snapshot(const Document &document) {
  Snapshot result;result.options=document.loaded.scene.options;result.generation=document.generation;result.revision=1;
  for(const auto &skin:document.skeletons.skins) result.poses.push_back(skin.initial);
  for(const auto &target:document.catalog.targets) {
    runtime::Properties p;for(const auto &m:target.morphs) p.morphs.push_back(m.evaluable||m.unsupported.empty()?m.initial:0);
    p.visible=document.loaded.scene.instances.at(target.instance).visible;
    runtime::sync_aliases(target,p);result.values.push_back(std::move(p));
  }
  result.lights=document.loaded.scene.lights;return result;
}
void append_document(Document &destination,Document source) {
  auto &a=destination.loaded.scene;auto &b=source.loaded.scene;
  const int textures=int(a.textures.size());const auto materials=uint32_t(a.materials.size()),meshes=uint32_t(a.meshes.size()),instances=uint32_t(a.instances.size());
  const auto skins=int(destination.skeletons.skins.size());
  const auto prefix="instance-"+std::to_string(destination.generation)+"/";
  a.textures.insert(a.textures.end(),b.textures.begin(),b.textures.end());
  for(auto m:b.materials) {
    m.id=prefix+m.id;
    for(auto *index:ir::texture_indices(m)) if(*index>=0) *index+=textures;
    a.materials.push_back(std::move(m));
  }
  for(auto &m:b.meshes) {m.id=prefix+m.id;a.meshes.push_back(std::move(m));}
  for(auto i:b.instances) {i.id=prefix+i.id;i.mesh+=meshes;if(i.prototype>=0) i.prototype+=int(instances);if(!i.instance_node.empty()) i.instance_node=prefix+i.instance_node;if(!i.instance_group.empty()) i.instance_group=prefix+i.instance_group;for(auto &m:i.materials) m+=materials;a.instances.push_back(std::move(i));}
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
