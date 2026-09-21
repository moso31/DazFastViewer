#include "editor/document.h"

namespace dfv::editor {
size_t apply_materials(Document &document,size_t target,const daz::LoadedScene &preset) {
  auto &scene=document.loaded.scene;auto &instance=scene.instances.at(document.catalog.targets.at(target).instance);
  const auto &slots=scene.meshes[instance.mesh].material_slots;const int offset=int(scene.textures.size());
  scene.textures.insert(scene.textures.end(),preset.scene.textures.begin(),preset.scene.textures.end());size_t applied=0;
  for(size_t m=0;m<preset.scene.materials.size();++m) for(size_t slot=0;slot<slots.size();++slot) {
    const auto &groups=preset.report.at("materials").at(m).at("groups");bool matches=false;
    for(const auto &group:groups) matches|=group.get<std::string>()==slots[slot];
    if(!matches) continue;
    auto material=preset.scene.materials[m];for(auto *index:{&material.color_texture,&material.roughness_texture,&material.opacity_texture,&material.normal_texture,&material.bump_texture}) if(*index>=0) *index+=offset;
    instance.materials[slot]=uint32_t(scene.materials.size());scene.materials.push_back(std::move(material));++applied;
  }
  if(!applied) throw std::runtime_error("材质预设没有匹配当前对象的表面组");return applied;
}
Snapshot initial_snapshot(const Document &document) {
  Snapshot result;result.generation=document.generation;result.revision=1;
  for(const auto &skin:document.skeletons.skins) result.poses.push_back(skin.initial);
  for(const auto &target:document.catalog.targets) {
    runtime::Properties p;for(const auto &m:target.morphs) p.morphs.push_back(m.evaluable||m.unsupported.empty()?m.initial:0);
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
    for(auto *index:{&m.color_texture,&m.roughness_texture,&m.opacity_texture,&m.normal_texture,&m.bump_texture}) if(*index>=0) *index+=textures;
    a.materials.push_back(std::move(m));
  }
  for(auto &m:b.meshes) {m.id=prefix+m.id;a.meshes.push_back(std::move(m));}
  for(auto i:b.instances) {i.id=prefix+i.id;i.mesh+=meshes;for(auto &m:i.materials) m+=materials;a.instances.push_back(std::move(i));}
  for(auto l:b.lights) {l.id=prefix+l.id;a.lights.push_back(std::move(l));}
  for(auto o:source.loaded.objects) {o.instance+=instances;o.id=prefix+o.id;if(o.parent.starts_with('#')) o.parent="#"+prefix+o.parent.substr(1);if(o.conform_target.starts_with('#')) o.conform_target="#"+prefix+o.conform_target.substr(1);destination.loaded.objects.push_back(std::move(o));}
  for(auto &t:source.catalog.targets) {t.instance+=instances;t.id=prefix+t.id;if(t.parent.starts_with('#')) t.parent="#"+prefix+t.parent.substr(1);if(t.conform_target.starts_with('#')) t.conform_target="#"+prefix+t.conform_target.substr(1);destination.catalog.targets.push_back(std::move(t));}
  for(auto &s:source.skeletons.skins) {s.instance+=instances;s.id=prefix+s.id;destination.skeletons.skins.push_back(std::move(s));}
  for(auto &g:source.formulas.graphs) {if(g.skin>=0) g.skin+=skins;destination.formulas.graphs.push_back(std::move(g));}
  destination.loaded.report["imports"].push_back(source.loaded.report);
  a.validate();
}
}
