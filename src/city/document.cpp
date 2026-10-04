#include "city/document.h"
#include "editor/document.h"
#include <unordered_set>

namespace dfv::city {
void remove(editor::Document &d,const std::string &id,bool record) {
  auto city=std::find_if(d.cities.begin(),d.cities.end(),[&](const auto &v){return v->id==id;});
  if(city==d.cities.end())return;
  auto belongs=[&](const std::string &v){return v==id||v.starts_with(id+"/");};
  auto &s=d.loaded.scene;std::vector<int> mapping(s.instances.size(),-1);size_t next=0;
  for(size_t i=0;i<s.instances.size();++i)if(!belongs(s.instances[i].id))mapping[i]=int(next++);
  std::erase_if(s.instances,[&](const auto &v){return belongs(v.id);});
  auto remap=[&](int &v){if(v>=0)v=mapping.at(size_t(v));};
  for(auto &i:s.instances){remap(i.prototype);remap(i.graft_source);remap(i.shell_source);remap(i.shell_root);}
  for(auto &o:d.loaded.objects)o.instance=uint32_t(mapping.at(o.instance));
  for(auto &t:d.catalog.targets)t.instance=uint32_t(mapping.at(t.instance));
  for(auto &skin:d.skeletons.skins)skin.instance=uint32_t(mapping.at(skin.instance));
  std::erase_if(d.loaded.nodes,[&](const auto &v){return belongs(v.id);});
  d.cities.erase(city);editor::collect_resources(d);++d.asset_revision;
  if(record)d.operations.push_back({{"op","city_remove"},{"id",id}});
}
void install(editor::Document &d,Generated generated,bool record) {
  const auto &city=*generated.city;remove(d,city.id,false);
  auto &a=d.loaded.scene;auto &b=generated.loaded.scene;
  const int textures=int(a.textures.size());const auto materials=uint32_t(a.materials.size()),meshes=uint32_t(a.meshes.size()),instances=uint32_t(a.instances.size());
  a.textures.insert(a.textures.end(),b.textures.begin(),b.textures.end());
  for(auto &m:b.materials){for(auto *t:ir::texture_indices(m))if(*t>=0)*t+=textures;a.materials.push_back(std::move(m));}
  for(auto &m:b.meshes)a.meshes.push_back(std::move(m));
  for(auto &i:b.instances){i.mesh+=meshes;if(i.prototype>=0)i.prototype+=int(instances);for(auto &m:i.materials)m+=materials;a.instances.push_back(std::move(i));}
  d.loaded.nodes.insert(d.loaded.nodes.end(),generated.loaded.nodes.begin(),generated.loaded.nodes.end());
  d.cities.push_back(std::move(generated.city));++d.asset_revision;a.validate();
  if(record)d.operations.push_back({{"op","city"},{"id",city.id},{"config",json(city.config)}});
}
void prune(const editor::Document &d,editor::Snapshot &s) {
  std::unordered_set<std::string> ids;for(const auto &i:d.loaded.scene.instances)ids.insert(i.id);
  std::erase_if(s.material_overrides,[&](const auto &v){return !ids.contains(v.first);});
  std::erase_if(s.instance_ground,[&](const auto &v){return !ids.contains(v.first);});
  ids.clear();for(const auto &n:d.loaded.nodes)ids.insert(n.id);
  std::erase_if(s.group_transforms,[&](const auto &v){return !ids.contains(v.first);});
  std::erase_if(s.city_views,[&](const auto &v){return std::none_of(d.cities.begin(),d.cities.end(),[&](const auto &c){return c->id==v.first;});});
}
}
