#include "editor/geograft_materials.h"
#include "daz/content_entry.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace dfv::editor {
namespace {
namespace fs=std::filesystem;
using J=nlohmann::json;
std::string path_text(const fs::path &path){const auto value=path.generic_u8string();return {value.begin(),value.end()};}
std::string key(const std::string &name){
  std::string result;for(unsigned char c:name)if(std::isalnum(c))result+=char(std::tolower(c));return result;
}
std::string region(const std::string &name){
  const auto n=key(name);
  if(n=="body"||n=="torso"||n=="breast"||n=="breasts"||n=="inner"||n=="nipples"||n.starts_with("hdnip")||n.starts_with("gptorso")||n.starts_with("gpvagina")||n.starts_with("gplabia")||n.starts_with("gprectum")||n=="genitalia"||n=="genitals"||n=="vagina"||n=="labia"||n=="anus")return "body";
  if(n=="arm"||n=="arms"||n=="forearms"||n=="shoulders")return "arms";
  if(n=="leg"||n=="legs"||n=="hips")return "legs";
  return n;
}
size_t source_slot(const std::vector<std::string> &slots,const std::string &destination){
  const auto exact=key(destination);std::vector<size_t> matches;
  if(!exact.empty())for(size_t i=0;i<slots.size();++i)if(key(slots[i])==exact)matches.push_back(i);
  if(matches.empty())for(size_t i=0;i<slots.size();++i)if(!exact.empty()&&region(slots[i])==region(destination))matches.push_back(i);
  if(matches.size()!=1)throw std::runtime_error(matches.empty()?"找不到对应的角色皮肤表面":"角色存在多个可能的皮肤表面，无法唯一匹配");
  return matches.front();
}
bool hd_uv(const daz::MaterialUVSet &uv){return key(uv.label)=="hduvs"||daz::entry_key(fs::u8path(daz::decode_uri(uv.uri))).ends_with("#hd uvs");}
std::string uv_name(const std::string &name){
  auto n=key(daz::decode_uri(name));if(n.ends_with("uvs"))n.resize(n.size()-3);else if(n.ends_with("uv"))n.resize(n.size()-2);return n;
}
std::string uv_id(const daz::MaterialUVSet &uv){const auto hash=uv.uri.find('#');return hash==std::string::npos?std::string{}:uv_name(uv.uri.substr(hash+1));}
bool default_uv(const daz::MaterialUVSet &uv){return uv_name(uv.label)=="default"||uv_id(uv)=="default";}
bool generic_uv(const std::string &name){return name.empty()||name=="default"||name=="base"||name=="basefemale"||name=="base81female"||name=="basemale"||name=="base81male";}
std::vector<std::string> uv_origin(const daz::MaterialUVSet &uv){
  const auto path=daz::entry_key(fs::u8path(daz::decode_uri(uv.uri.substr(0,uv.uri.find('#')))));const auto begin=path.find("/uv sets/");std::vector<std::string> result;
  if(begin==std::string::npos)return result;
  for(const auto &part:fs::u8path(path.substr(begin+9)).parent_path())result.push_back(key(path_text(part)));return result;
}
std::set<std::string> words(const std::string &text){
  std::set<std::string> result;std::string word;for(unsigned char c:text){if(std::isalnum(c))word+=char(std::tolower(c));else if(!word.empty()){result.insert(word);word.clear();}}if(!word.empty())result.insert(word);return result;
}
bool skin_name_in_file(const std::string &name,const fs::path &file){
  auto tokens=words(name);for(const auto *word:{"uv","uvs","set","sets","body","torso","genital","genitals","genitalia","base","default"})tokens.erase(word);
  if(tokens.empty()||std::none_of(tokens.begin(),tokens.end(),[](const auto &word){return word.size()>=3;}))return false;
  const auto filename=words(path_text(file.stem()));return std::all_of(tokens.begin(),tokens.end(),[&](const auto &token){return filename.contains(token);});
}
std::vector<fs::path> skin_textures(const J &material){
  std::vector<fs::path> result;const auto &parameters=material.at("parameters");
  auto add=[&](const J &texture){if(texture.is_null())return;const auto file=texture.value("file",std::string{});if(!file.empty())result.push_back(fs::u8path(file));for(const auto &layer:texture.value("layers",J::array())){const auto path=layer.value("file",std::string{});if(!path.empty())result.push_back(fs::u8path(path));}};
  add(parameters.at("color_texture"));if(result.empty())for(const auto &p:material_parameters())if(p.kind==MaterialParameter::texture)add(parameters.at(p.id));return result;
}
int texture_uv_match(const daz::MaterialUVSet &uv,const fs::path &texture){
  const auto origin=uv_origin(uv);std::set<std::string> directories;for(const auto &part:texture.parent_path())directories.insert(key(path_text(part)));
  int score=0;
  // 产品目录比 UV 的显示名可靠：多个 Hamster 产品都把 UV 命名为 HS Keicy。
  if(origin.size()>=2&&origin.back().size()>=3&&!generic_uv(origin.back())&&directories.contains(origin.back()))score=200+(directories.contains(origin.front())?20:0);
  if(!generic_uv(uv_name(uv.label))&&skin_name_in_file(uv.label,texture))score=std::max(score,80);
  return score;
}
std::optional<daz::MaterialUVSet> choose_skin_uv(const daz::MaterialUVCatalog &catalog,const J &material){
  ir::Material source;source.source_definition=material.at("source_definition").get<std::string>();const auto source_uv=daz::material_uv_set(source);
  const auto source_name=uv_name(source_uv.label),source_id=uv_id(source_uv);const auto source_origin=uv_origin(source_uv);const auto textures=skin_textures(material);
  int best=0;std::optional<daz::MaterialUVSet> choice;bool ambiguous=false;
  for(const auto &uv:catalog.sets){
    if(hd_uv(uv)||default_uv(uv))continue;int score=0;
    const bool same=(!source_name.empty()&&source_name==uv_name(uv.label))||(!source_id.empty()&&source_id==uv_id(uv));
    if(same){score=generic_uv(source_name)&&generic_uv(source_id)?100:400;if(!source_origin.empty()&&source_origin==uv_origin(uv))score+=40;}
    int texture_score=0;for(const auto &file:textures)texture_score=std::max(texture_score,texture_uv_match(uv,file));score+=texture_score;
    if(score>best){best=score;choice=uv;ambiguous=false;}else if(score>0&&score==best)ambiguous=true;
  }
  if(choice&&!ambiguous)return choice;
  if(catalog.default_set&&!hd_uv(*catalog.default_set))return catalog.default_set;
  choice.reset();for(const auto &uv:catalog.sets)if(default_uv(uv)){if(choice)return {};choice=uv;}return choice;
}
daz::MaterialUVCatalog skin_uv_catalog(const Document &d,size_t instance,const std::vector<fs::path> &roots){
  daz::LoadOptions options;options.content_roots=roots;if(d.loaded.report.is_object())for(const auto &root:d.loaded.report.value("content_roots",J::array())){const auto path=fs::u8path(root.get<std::string>());if(std::find(options.content_roots.begin(),options.content_roots.end(),path)==options.content_roots.end())options.content_roots.push_back(path);}
  std::vector<std::shared_ptr<daz::SourceArchive>> archives;const auto &i=d.loaded.scene.instances.at(instance);
  for(size_t slot=0;slot<i.materials.size();++slot)archives.push_back(material_archive(d,{instance,slot},daz::material_uv_set(d.loaded.scene.materials.at(i.materials[slot])).owner));
  return daz::discover_material_uv_sets(d.loaded,instance,options,archives);
}
void restore_skin_uv(Document &d,MaterialSurface surface){
  const auto &i=d.loaded.scene.instances.at(surface.instance);const auto &m=d.loaded.scene.materials.at(i.materials.at(surface.slot));
  const auto current=daz::material_uv_set(m);
  if(!hd_uv(current)){if(!current.uri.empty()&&!default_uv(current))throw std::runtime_error("无法匹配当前皮肤 UV，且附件没有可用的 Default UVs");return;}
  // HD 局部贴图与整身皮肤采用不同坐标，先恢复附件自己的基础 UV。
  if(const auto baseline=daz::material_uv_baseline(m);baseline&&!baseline->uri.empty()&&!hd_uv(*baseline)){change_material_uv(d,*baseline,{surface});return;}
  for(const auto &o:d.loaded.objects)if(o.instance==surface.instance&&daz::entry_key(o.geometry_file).find("/3feetwolf/hd nipples for g8f - 2.0/")!=std::string::npos){
    const auto file=o.geometry_file.parent_path()/"UV Sets/3feetwolf/Base/default.dsf";const auto bytes=file.generic_u8string();const std::string path(bytes.begin(),bytes.end());
    change_material_uv(d,{path+"#default",path,"default"},{surface});return;
  }
  throw std::runtime_error("无法恢复局部 HD UV 对应的基础皮肤 UV");
}
void apply_skin_uv(Document &d,MaterialSurface surface,const daz::MaterialUVCatalog &catalog,const J &material){
  const auto chosen=choose_skin_uv(catalog,material);if(!chosen){restore_skin_uv(d,surface);return;}
  try{change_material_uv(d,*chosen,{surface});}
  catch(const std::exception &){if(!catalog.default_set||catalog.default_set->uri==chosen->uri)throw;change_material_uv(d,*catalog.default_set,{surface});}
}
}
GeograftMaterialStrategy geograft_material_strategy(const Document &d,size_t target){
  const auto instance=d.catalog.targets.at(target).instance;
  for(const auto &o:d.loaded.objects)if(o.instance==instance){
    const auto file=daz::entry_key(o.geometry_file);
    if(file.find("/3feetwolf/hd nipples for g8f - 2.0/")!=std::string::npos)return GeograftMaterialStrategy::hd_nipples;
    if(file.find("/meipex/goldenpalace/")!=std::string::npos)return GeograftMaterialStrategy::golden_palace;
  }
  return GeograftMaterialStrategy::skin;
}
GeograftMaterialUpdate update_geograft_materials(Document &d,Snapshot &s,size_t target,const std::vector<fs::path> &roots,const fs::path &directory,const std::string &preparation_error){
  const auto host=attachment_host(d,target);const auto grafts=geograft_targets(d,host);
  if(std::find(grafts.begin(),grafts.end(),target)==grafts.end())throw std::runtime_error("此对象不是角色的 Geograft 附件");
  const auto instance=d.catalog.targets.at(target).instance,host_instance=d.catalog.targets.at(host).instance;
  const auto slots=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(instance).mesh).material_slots;
  const auto host_slots=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(host_instance).mesh).material_slots;
  const auto instance_id=d.loaded.scene.instances.at(instance).id;
  GeograftMaterialUpdate result;result.strategy_error=preparation_error;
  std::set<std::pair<std::string,std::string>> completed;
  const auto strategy=geograft_material_strategy(d,target);
  if(strategy!=GeograftMaterialStrategy::skin&&preparation_error.empty())try{
    // 专用策略可能在改 UV 后才失败，不将它的半成品带入通用回退。
    auto next=d;auto state=s;const auto first=next.operations.size();
    if(strategy==GeograftMaterialStrategy::hd_nipples)apply_hd_textures(next,state,target,directory,roots,instance);
    else copy_golden_palace_materials(next,state,target,instance);
    for(size_t op=first;op<next.operations.size();++op)if(next.operations[op].value("op","")=="paste_material")
      for(const auto &surface:next.operations[op].at("surfaces"))completed.emplace(surface.at("instance").get<std::string>(),surface.at("slot").get<std::string>());
    d=std::move(next);s=std::move(state);result.updated=completed.size();
  }catch(const std::exception &e){result.strategy_error=e.what();}
  std::optional<daz::MaterialUVCatalog> uv_catalog;
  for(size_t slot=0;slot<slots.size();++slot){
    if(completed.contains({instance_id,slots[slot]}))continue;
    try{
      const auto source=source_slot(host_slots,slots[slot]);
      auto material=copy_material(d.loaded.scene,s.material_overrides,{host_instance,source});material.erase("uv_topology");
      // HD 产品的内部支撑面始终隐藏；专用策略失败后的皮肤复制也必须遵守。
      if(strategy==GeograftMaterialStrategy::hd_nipples&&slots[slot]=="Inner"){material["parameters"]["opacity"]=0;material["parameters"]["opacity_texture"]=nullptr;}
      if(!uv_catalog)uv_catalog=skin_uv_catalog(d,instance,roots);
      auto next=d;auto state=s;const MaterialSurface destination{instance,slot};
      apply_skin_uv(next,destination,*uv_catalog,material);paste_material(next,state,material,{destination});
      d=std::move(next);s=std::move(state);++result.updated;++result.fallback;
    }catch(const std::exception &e){result.unresolved.push_back(slots[slot]+"："+e.what());}
  }
  if(slots.empty())result.unresolved.push_back("附件没有可更新的材质表面");
  return result;
}
}
