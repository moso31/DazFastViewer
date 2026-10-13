#include "editor/presets.h"
#include "daz/documents.h"
#include <fstream>
#include <chrono>
#include <cmath>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace dfv::editor {
namespace {
using J=nlohmann::json;namespace fs=std::filesystem;
std::string utf8(const fs::path &p){const auto s=p.generic_u8string();return {s.begin(),s.end()};}
std::string encode(const std::string &s,bool path=false){
  constexpr char hex[]="0123456789ABCDEF";std::string out;
  for(unsigned char c:s)if(std::isalnum(c)&&c<128||c=='-'||c=='_'||c=='.'||c=='~'||(path&&(c=='/'||c==':')))out+=char(c);else{out+='%';out+=hex[c>>4];out+=hex[c&15];}return out;
}
double srgb(double v){return v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;}
J color(J v){for(auto &c:v)c=srgb(c.get<double>());return v;}
void animation(J &out,const std::string &url,const J &value){out["scene"]["animations"].push_back({{"url",url},{"keys",J::array({J::array({0,value})})}});}
daz::LoadOptions options(const Document &d){daz::LoadOptions o;if(d.loaded.report.is_object())for(const auto &r:d.loaded.report.value("content_roots",J::array()))o.content_roots.push_back(fs::u8path(r.get<std::string>()));return o;}
std::string texture_uri(const Document &d,const fs::path &file){
  const auto absolute=fs::absolute(file).lexically_normal();const auto path=utf8(absolute),key=daz::source_key(absolute);
  for(const auto &root:options(d).content_roots){auto prefix=daz::source_key(fs::absolute(root).lexically_normal());if(!prefix.ends_with('/'))prefix+='/';if(key.starts_with(prefix))return encode("/"+path.substr(prefix.size()),true);}
  // DSON 的绝对磁盘路径也以 / 开始，否则盘符会被当作 URI scheme 丢弃。
  return encode("/"+path,true);
}
void relocate_images(J &value,const Document &d){
  if(value.is_array())for(auto &item:value)relocate_images(item,d);
  else if(value.is_object())for(auto it=value.begin();it!=value.end();++it){
    if((it.key()=="image_file"||it.key()=="url")&&it->is_string()){const auto path=fs::u8path(daz::decode_uri(it->get<std::string>()));if(path.is_absolute()&&path.has_root_name())*it=texture_uri(d,path);}
    else relocate_images(*it,d);
  }
}
void material(J &out,const Document &d,const Snapshot &snapshot,size_t instance,size_t slot){
  const auto &scene=d.loaded.scene;auto textures=scene.textures;auto m=effective_material(scene,snapshot.material_overrides,instance,slot,textures);
  const auto archive=material_archive(d,{instance,slot},daz::material_uv_set(m).owner);daz::DocumentScope scope(archive);
  auto source=daz::export_material_source(m,options(d));auto channels=source.at("channels");relocate_images(channels,d);
  auto original=m;auto baseline_textures=textures;
  if(!m.source_definition.empty()){auto definition=J::parse(m.source_definition);definition.erase("authored");original.source_definition=definition.dump();original=daz::merge_material_preset({},original,baseline_textures,options(d));}
  const bool fresh=channels.empty();
  auto changed=[&](const char *id){const auto &p=material_parameter(id);return fresh||material_value(m,textures,p)!=material_value(original,baseline_textures,p);};
  auto value=[&](const std::string &id,J v){auto &c=channels[id];if(!c.is_object())c=J::object();c["id"]=id;c["current_value"]=v;c["value"]=v;};
  auto set=[&](const char *p,const char *id,double scale=1){if(changed(p)){auto v=material_value(m,textures,material_parameter(p));if(v.is_number())v=v.get<double>()*scale;value(id,v);}};
  auto tint=[&](const char *p,const char *id){if(changed(p))value(id,color(material_value(m,textures,material_parameter(p))));};
  auto map=[&](const char *p,const char *id){auto &c=channels[id];if(!c.is_object())c=J::object();c["id"]=id;c.erase("image");c.erase("_dfv_export_image");c["image_file"]="";
    const int index=m.*material_parameter(p).texture_member;if(index<0)return;const auto &t=textures.at(size_t(index));
    if(t.layers.empty()&&t.gamma==0)c["image_file"]=texture_uri(d,t.file);
    else {J image={{"map",J::array()},{"map_gamma",t.gamma}};if(t.layers.empty())image["map"].push_back({{"url",texture_uri(d,t.file)}});
      for(const auto &l:t.layers)image["map"].push_back({{"url",texture_uri(d,l.file)},{"operation",l.operation},{"color",{l.color.x,l.color.y,l.color.z}},{"transparency",l.opacity},{"rotation",l.rotation},{"xscale",l.scale.x},{"yscale",l.scale.y},{"xoffset",l.offset.x},{"yoffset",l.offset.y},{"invert",l.invert},{"xmirror",l.mirror_x},{"ymirror",l.mirror_y}});
      c.erase("image_file");c["_dfv_export_image"]=std::move(image);
    }
  };
  const std::pair<const char *,const char *> scalar[]={
    {"metallic","Metallic Weight"},{"opacity","Cutout Opacity"},{"thin_walled","Thin Walled"},
    {"overlay_weight","Diffuse Overlay Weight"},{"overlay_squared","Diffuse Overlay Weight Squared"},{"overlay_roughness","Diffuse Overlay Roughness"},
    {"anisotropy","Glossy Anisotropy"},{"anisotropy_rotation","Glossy Anisotropy Rotations"},{"transmission","Refraction Weight"},{"ior","Refraction Index"},
    {"coat","Top Coat Weight"},{"coat_roughness","Top Coat Roughness"},{"coat_ior","Top Coat IOR"},{"coat_mode","Top Coat Layering Mode"},{"coat_normal","Top Coat Curve Normal"},{"coat_grazing","Top Coat Curve Grazing"},{"coat_exponent","Top Coat Curve Exponent"},
    {"dual_weight","Dual Lobe Specular Weight"},{"dual_ratio","Dual Lobe Specular Ratio"},{"dual_roughness1","Specular Lobe 1 Roughness"},{"dual_roughness2","Specular Lobe 2 Roughness"},{"dual_specular","Dual Lobe Specular Reflectivity"},
    {"normal_strength","Normal Map"},{"bump_strength","Bump Strength"},{"displacement_strength","Displacement Strength"},
    {"emission_luminance","Luminance"},{"emission_units","Luminance Units"},{"emission_temperature","Emission Temperature"},{"emission_efficacy","Luminous Efficacy"},{"emission_two_sided","Two Sided Light"},
    {"hair_melanin","Melanin"},{"hair_redness","Melanin Redness"},{"hair_radial_roughness","Azimuthal Roughness"},
    {"u_scale","Horizontal Tiles"},{"v_scale","Vertical Tiles"},{"u_offset","Horizontal Offset"},{"v_offset","Vertical Offset"},{"subsurface_anisotropy","SSS Direction"}};
  for(auto [p,id]:scalar)set(p,id);
  const std::pair<const char *,const char *> colors[]={{"base_color",m.hair?"Hair Root Color":"diffuse"},{"overlay_color","Diffuse Overlay Color"},{"specular_color","Glossy Color"},{"translucency_color","Translucency Color"},{"coat_color","Top Coat Color"},{"emission_color","Emission Color"},{"hair_tip_color","Hair Tip Color"}};
  for(auto [p,id]:colors)tint(p,id);
  const std::pair<const char *,const char *> maps[]={
    {"color_texture",m.hair?"Hair Root Color":"diffuse"},{"metallic_texture","Metallic Weight"},{"opacity_texture","Cutout Opacity"},{"normal_texture","Normal Map"},{"bump_texture","Bump Strength"},{"displacement_texture","Displacement Strength"},
    {"overlay_texture","Diffuse Overlay Weight"},{"overlay_color_texture","Diffuse Overlay Color"},{"overlay_roughness_texture","Diffuse Overlay Roughness"},
    {"specular_color_texture","Glossy Color"},{"transmission_texture","Refraction Weight"},{"translucency_texture","Translucency Weight"},{"translucency_color_texture","Translucency Color"},
    {"coat_texture","Top Coat Weight"},{"coat_color_texture","Top Coat Color"},{"coat_roughness_texture","Top Coat Roughness"},{"dual_texture","Dual Lobe Specular Weight"},{"dual_roughness1_texture","Specular Lobe 1 Roughness"},{"dual_roughness2_texture","Specular Lobe 2 Roughness"},
    {"emission_color_texture","Emission Color"},{"emission_luminance_texture","Luminance"},{"hair_tip_texture","Hair Tip Color"}};
  for(auto [p,id]:maps)map(p,id);
  if(changed("roughness")||changed("roughness_from_glossiness")||changed("weighted_glossy"))value("Base Mixing",m.weighted_glossy?2:m.roughness_from_glossiness?1:0);
  const auto rough=m.hair?"Roughness":m.roughness_from_glossiness?"Glossiness":"Glossy Roughness";set("roughness",rough);map("roughness_texture",rough);
  if(changed("specular")||changed("weighted_glossy")){value("Glossy Reflectivity",m.specular);value("Glossy Layered Weight",1);if(m.weighted_glossy){value("Glossy Weight",m.specular);value("Diffuse Weight",1-m.specular);}}
  map("specular_texture",m.weighted_glossy?"Glossy Weight":"Glossy Layered Weight");
  set(m.thin_walled?"translucency":"subsurface","Translucency Weight");
  if(changed("refraction_color")||changed("refraction_color_texture")||changed("refraction_roughness")||changed("refraction_roughness_texture")){
    value("Share Glossy Inputs",false);value("Refraction Color",color(material_value(m,textures,material_parameter("refraction_color"))));value("Refraction Roughness",m.refraction_roughness);
    map("refraction_color_texture","Refraction Color");map("refraction_roughness_texture","Refraction Roughness");
  }
  if(changed("bump_distance")||changed("bump_invert")||changed("bump_from_texel_density")){if(m.bump_from_texel_density){channels.erase("Bump Minimum");channels.erase("Bump Maximum");}else{value("Bump Minimum",0);value("Bump Maximum",m.bump_distance*100*(m.bump_invert?-1:1));}}
  set("displacement_min","Minimum Displacement");set("displacement_max","Maximum Displacement");set("hair_root_radius","Line Start Width",.2);set("hair_tip_radius","Line End Width",.2);
  if(changed("subsurface_radius")||changed("subsurface_color")){
    value("SSS Mode",1);value("Transmitted Measurement Distance",.1);value("Scattering Measurement Distance",.1);
    J transmitted=J::array(),scatter=J::array();const auto radii=material_value(m,textures,material_parameter("subsurface_radius"));const auto albedo=material_value(m,textures,material_parameter("subsurface_color"));
    for(size_t c=0;c<3;++c){const auto extinction=1/std::max(1e-6,radii[c].get<double>()*.001),a=std::clamp(albedo[c].get<double>(),0.,1.);transmitted.push_back(srgb(std::exp(-(1-a)*extinction*.001)));scatter.push_back(srgb(std::exp(-a*extinction*.001)));}value("Transmitted Color",transmitted);value("SSS Color",scatter);
  }
  const auto &group=scene.meshes.at(scene.instances.at(instance).mesh).material_slots.at(slot);const auto id="material-"+std::to_string(slot);
  J entry={{"id",id},{"groups",J::array({group})},{"extra",J::array()}};
  entry["extra"].push_back(source.value("shader",J{{"type","studio/material/uber_iray"},{"version","1.1.0.0"}}));
  J extra={{"type","studio_material_channels"},{"channels",J::array()}};
  for(auto it=channels.begin();it!=channels.end();++it){auto c=it.value();c["id"]=it.key();if(c.contains("_dfv_export_image")){auto image=c.at("_dfv_export_image");const auto image_id=id+"-image-"+std::to_string(out["image_library"].size());image["id"]=image_id;out["image_library"].push_back(image);c.erase("_dfv_export_image");c["image"]="#"+image_id;}
    if(c.contains("current_value"))c["value"]=c.at("current_value");
    if(c.value("mappable",false)&&!c.contains("image_file")&&!c.contains("image"))c["image_file"]="";
    const auto address="name://@selection#materials/"+encode(group)+":?"+(it.key()=="diffuse"?std::string("diffuse"):"extra/studio_material_channels/channels/"+encode(it.key()));
    if(c.contains("value"))animation(out,address+"/value",c.at("value"));
    for(const auto *key:{"image","image_file"})if(c.contains(key))animation(out,address+"/"+key,c.at(key));
    if(it.key()=="diffuse")entry["diffuse"]={{"channel",c}};else extra["channels"].push_back({{"channel",c}});
  }
  entry["extra"].push_back(std::move(extra));J exact=J::object();for(const auto &p:material_parameters())exact[p.id]=material_value(m,textures,p);
  entry["extra"].push_back({{"type","dazfastviewer/material"},{"version",1},{"parameters",std::move(exact)}});
  if(source.contains("uv_asset")){auto uv=source.at("uv_asset");uv.erase("id");std::string uv_id;
    for(const auto &saved:out["uv_set_library"]){auto compare=saved;compare.erase("id");if(compare==uv){uv_id=saved.at("id");break;}}
    if(uv_id.empty()){uv_id=id+"-uv";uv["id"]=uv_id;out["uv_set_library"].push_back(uv);}entry["uv_set"]="#"+uv_id;
    animation(out,"name://@selection#materials/"+encode(group)+":?uv_set",entry.at("uv_set"));
  }
  auto definition=entry;definition.erase("groups");definition["name"]=group;definition["label"]=group;
  std::erase_if(definition["extra"].get_ref<J::array_t &>(),[](const J &e){return e.value("type","")=="dazfastviewer/material";});
  out["material_library"].push_back(std::move(definition));entry["url"]="#"+id;entry["id"]=id+"-instance";
  out["scene"]["materials"].push_back(std::move(entry));
}
}
J preset_json(const Document &d,const Snapshot &s,size_t target,PresetKind kind,const fs::path &destination){
  const auto &t=d.catalog.targets.at(target);const auto &properties=s.values.at(target);
  J out={{"file_version","0.6.0.0"},{"asset_info",{{"id",encode(utf8(fs::absolute(destination)),true)},{"type",kind==PresetKind::material?"preset_material":kind==PresetKind::pose?"preset_pose":"preset_shape"},{"contributor",{{"author","DazFastViewer"}}},{"revision","1.0"}}},{"scene",J::object()}};
  if(kind==PresetKind::material){
    out["image_library"]=J::array();out["uv_set_library"]=J::array();out["material_library"]=J::array();out["scene"]["materials"]=J::array();
    const auto &i=d.loaded.scene.instances.at(t.instance);if(i.materials.empty())throw std::runtime_error("选中对象没有可保存的材质");
    for(size_t slot=0;slot<i.materials.size();++slot)material(out,d,s,t.instance,slot);return out;
  }
  out["scene"]["animations"]=J::array();size_t skin_index=d.skeletons.skins.size();for(size_t i=0;i<d.skeletons.skins.size();++i)if(d.skeletons.skins[i].instance==t.instance)skin_index=i;
  if(kind==PresetKind::pose){
    if(skin_index==d.skeletons.skins.size())throw std::runtime_error("请选择带骨骼的角色保存 Pose");
    const auto &skin=d.skeletons.skins.at(skin_index);runtime::validate_pose(skin,s.poses.at(skin_index));
    for(size_t i=0;i<skin.joints.size();++i){const auto &joint=skin.joints[i];const auto &pose=s.poses.at(skin_index).at(i);
      const auto address="name://@selection"+(joint.parent<0?std::string{}:"/"+encode(joint.name))+":?";
      for(auto [name,v]:{std::pair{"translation",pose.translation_cm},std::pair{"rotation",pose.rotation_degrees},std::pair{"scale",pose.scale}}){size_t c=0;for(float a:{v.x,v.y,v.z})animation(out,address+name+"/"+"xyz"[c++]+"/value",a);}
      animation(out,address+"general_scale/value",pose.general_scale);
    }
  }
  J exact=J::array();std::map<std::string,std::pair<float,bool>> native_values;
  for(size_t i=0;i<t.morphs.size();++i){const auto &m=t.morphs[i];if(m.alias_morph>=0||m.kind=="alias"||!m.unsupported.empty()||m.locked)continue;
    if(kind==PresetKind::pose&&!(m.group.starts_with("/Pose")||m.group.find("Expressions")!=std::string::npos))continue;
    std::string node;if(skin_index<d.skeletons.skins.size())for(const auto &j:d.skeletons.skins[skin_index].joints)if(j.id==m.owner&&j.parent>=0)node="/"+encode(j.name);
    const auto name=m.channel_name.empty()?m.channel_id:m.channel_name;if(name.empty())continue;
    const auto address="name://@selection"+node+"#"+encode(name)+":?value/value";
    const auto value=properties.morphs.at(i);if(!std::isfinite(value))throw std::runtime_error("Morph 数值不是有限数");
    exact.push_back({{"node",node.empty()?"":daz::decode_uri(node.substr(1))},{"modifier",name},{"asset",m.id},{"value",value}});
    auto [it,inserted]=native_values.emplace(address,std::pair{value,m.scene_channel});
    // G8.1 的扩展库可能含同名 G8 参数；DAZ 用场景明确写入的非零输入，工程扩展保留全部身份。
    if(!inserted&&((m.scene_channel&&!it->second.second)||(m.scene_channel==it->second.second&&value!=0&&it->second.first==0)))it->second={value,m.scene_channel};
  }
  for(const auto &[address,value]:native_values)animation(out,address,value.first);
  if(!exact.empty())out["scene"]["extra"]=J::array({{{"type","dazfastviewer/parameters"},{"version",1},{"values",exact}}});
  if(out["scene"]["animations"].empty())throw std::runtime_error("选中对象没有可保存的 Morph 参数");return out;
}
void save_preset(const fs::path &file,const Document &d,const Snapshot &s,size_t target,PresetKind kind){
  const auto destination=fs::absolute(file).lexically_normal();auto extension=destination.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
  if(extension!=L".duf")throw std::runtime_error("预设文件必须使用 .duf 后缀");
  if(!d.source_file.empty()&&daz::source_key(destination)==daz::source_key(d.source_file))throw std::runtime_error("不能用预设覆盖源场景，请选择其他文件名");
  const auto body=preset_json(d,s,target,kind,destination).dump(2);auto temporary=destination;temporary+=L".tmp-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
  try{std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);stream.write(body.data(),std::streamsize(body.size()));stream.flush();if(!stream)throw std::runtime_error("预设写入失败");stream.close();if(!stream)throw std::runtime_error("预设关闭失败");
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("预设原子替换失败");
#else
    fs::rename(temporary,destination);
#endif
  }catch(...){std::error_code ec;fs::remove(temporary,ec);throw;}
}
}
