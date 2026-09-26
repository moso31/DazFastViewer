#include "editor/materials.h"
#include <cmath>
#include <stdexcept>
#include <sstream>

namespace dfv::editor {
namespace {
using J=nlohmann::json;using M=ir::Material;using P=MaterialParameter;
std::string path_string(const std::filesystem::path &p){auto s=p.generic_u8string();return {s.begin(),s.end()};}
J vec(ir::Vec3 v){return J::array({v.x,v.y,v.z});}
J vec(ir::Vec2 v){return J::array({v.x,v.y});}
void range(double v,double lo,double hi){if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error("材质参数超出有效范围");}
J texture_json(const ir::Texture &t){
  J j={{"file",path_string(t.file)},{"colorspace",int(t.colorspace)},{"gamma",t.gamma},{"layers",J::array()}};
  for(const auto &l:t.layers)j["layers"].push_back({{"file",path_string(l.file)},{"operation",l.operation},{"color",vec(l.color)},{"opacity",l.opacity},{"rotation",l.rotation},{"scale",vec(l.scale)},{"offset",vec(l.offset)},{"invert",l.invert},{"mirror_x",l.mirror_x},{"mirror_y",l.mirror_y}});
  return j;
}
ir::Texture read_texture(const J &j,ir::ColorSpace colorspace){
  ir::Texture t;t.file=std::filesystem::u8path(j.at("file").get<std::string>());t.id=path_string(t.file);t.colorspace=colorspace;t.gamma=j.value("gamma",0.f);range(t.gamma,0,100);
  if(t.file.empty()||!t.file.is_absolute())throw std::runtime_error("材质贴图需要绝对路径");
  for(const auto &j:j.value("layers",J::array())){
    ir::ImageLayer l;l.file=std::filesystem::u8path(j.at("file").get<std::string>());l.operation=j.at("operation");
    const auto c=j.at("color").get<std::array<float,3>>();const auto s=j.at("scale").get<std::array<float,2>>(),o=j.at("offset").get<std::array<float,2>>();
    l.color={c[0],c[1],c[2]};l.scale={s[0],s[1]};l.offset={o[0],o[1]};l.opacity=j.at("opacity");l.rotation=j.at("rotation");l.invert=j.at("invert");l.mirror_x=j.at("mirror_x");l.mirror_y=j.at("mirror_y");
    for(float v:{c[0],c[1],c[2],s[0],s[1],o[0],o[1],l.opacity,l.rotation})if(!std::isfinite(v))throw std::runtime_error("分层贴图数值无效");
    if(!l.file.empty()&&!l.file.is_absolute())throw std::runtime_error("分层贴图需要绝对路径");t.layers.push_back(std::move(l));
  }return t;
}
}
const std::vector<P> &material_parameters(){
  static const auto parameters=[] {
    std::vector<P> out;std::string group;
    auto scalar=[&](const char *id,const char *label,float M::*member,double lo=0,double hi=1,double step=.01,double scale=1){
      P p;p.id=id;p.label=label;p.group=group;p.minimum=lo;p.maximum=hi;p.step=step;
      p.copy=[=](M &dst,const M &src){dst.*member=src.*member;};p.read=[=](const M &m)->J{return double(m.*member)*scale;};p.write=[=](M &m,const J &v){const double x=v.get<double>();range(x,lo,hi);m.*member=float(x/scale);};out.push_back(std::move(p));};
    auto color=[&](const char *id,const char *label,ir::Vec3 M::*member,bool radius=false){
      P p;p.id=id;p.label=label;p.group=group;p.kind=radius?P::vector:P::color;p.maximum=radius?1000:10;p.step=.01;
      p.copy=[=](M &dst,const M &src){dst.*member=src.*member;};p.read=[=](const M &m)->J{auto v=m.*member;return radius?J::array({v.x*1000.,v.y*1000.,v.z*1000.}):vec(v);};
      p.write=[=](M &m,const J &v){auto a=v.get<std::array<double,3>>();for(auto x:a)range(x,0,radius?1000:std::pow((10.+.055)/1.055,2.4));double scale=radius?1000:1;m.*member={float(a[0]/scale),float(a[1]/scale),float(a[2]/scale)};};out.push_back(std::move(p));};
    auto flag=[&](const char *id,const char *label,bool M::*member){P p;p.id=id;p.label=label;p.group=group;p.kind=P::boolean;p.copy=[=](M &dst,const M &src){dst.*member=src.*member;};p.read=[=](const M &m)->J{return m.*member;};p.write=[=](M &m,const J &v){m.*member=v.get<bool>();};out.push_back(std::move(p));};
    auto choices=[&](const char *id,const char *label,int M::*member,std::vector<std::string> names){P p;p.id=id;p.label=label;p.group=group;p.kind=P::choice;p.choices=names;p.copy=[=](M &dst,const M &src){dst.*member=src.*member;};p.read=[=](const M &m)->J{return m.*member;};p.write=[=](M &m,const J &v){const double x=v.get<double>();range(x,0,double(names.size()-1));if(x!=std::floor(x))throw std::runtime_error("材质选项不是整数");m.*member=int(x);};out.push_back(std::move(p));};
    auto texture=[&](const char *id,const char *label,int M::*member,bool srgb=false){P p;p.id=id;p.label=label;p.group=group;p.kind=P::texture;p.texture_member=member;p.copy=[=](M &dst,const M &src){dst.*member=src.*member;};p.colorspace=srgb?ir::ColorSpace::srgb:ir::ColorSpace::linear;out.push_back(std::move(p));};
    group="基础 / Base";
    color("base_color","基础色 · Base Color",&M::base_color);texture("color_texture","基础色贴图",&M::color_texture,true);
    scalar("metallic","金属度 · Metallic Weight",&M::metallic);texture("metallic_texture","金属度贴图",&M::metallic_texture);
    scalar("opacity","裁切透明度 · Cutout Opacity",&M::opacity);texture("opacity_texture","透明度贴图",&M::opacity_texture);
    flag("thin_walled","薄壁 · Thin Walled",&M::thin_walled);flag("hair","毛发着色 · Hair",&M::hair);
    group="漫反射覆盖 / Diffuse Overlay（近似）";
    scalar("overlay_weight","覆盖层权重 · Diffuse Overlay Weight",&M::overlay_weight);texture("overlay_texture","覆盖层权重贴图",&M::overlay_texture);flag("overlay_squared","权重平方 · Weight Squared",&M::overlay_squared);
    color("overlay_color","覆盖层颜色 · Diffuse Overlay Color",&M::overlay_color);texture("overlay_color_texture","覆盖层颜色贴图",&M::overlay_color_texture,true);scalar("overlay_roughness","覆盖层粗糙度 · Diffuse Overlay Roughness",&M::overlay_roughness);texture("overlay_roughness_texture","覆盖层粗糙度贴图",&M::overlay_roughness_texture);
    group="高光 / Glossy";
    scalar("roughness","粗糙度 / 光泽度 · Roughness / Glossiness",&M::roughness);texture("roughness_texture","粗糙度 / 光泽度贴图",&M::roughness_texture);
    flag("roughness_from_glossiness","使用光泽度（反转粗糙度）",&M::roughness_from_glossiness);
    scalar("specular","高光强度 · Specular",&M::specular);texture("specular_texture","高光权重贴图",&M::specular_texture);
    color("specular_color","高光颜色 · Glossy Color",&M::specular_color);texture("specular_color_texture","高光颜色贴图",&M::specular_color_texture,true);
    flag("weighted_glossy","加权高光 / 漫反射混合",&M::weighted_glossy);
    scalar("anisotropy","各向异性 · Glossy Anisotropy",&M::anisotropy);scalar("anisotropy_rotation","各向异性旋转 · Rotations",&M::anisotropy_rotation,-100,100);
    group="折射与透光 / Refraction";
    scalar("transmission","折射权重 · Refraction Weight",&M::transmission);texture("transmission_texture","折射权重贴图",&M::transmission_texture);
    scalar("ior","折射率 · Refraction Index",&M::ior,1,4);
    scalar("translucency","薄壁透光权重 · Translucency",&M::translucency);texture("translucency_texture","透光 / SSS 权重贴图",&M::translucency_texture);
    color("translucency_color","透光颜色 · Translucency Color",&M::translucency_color);texture("translucency_color_texture","透光颜色贴图",&M::translucency_color_texture,true);
    group="皮下散射 / SSS";
    scalar("subsurface","皮下散射权重 · Subsurface",&M::subsurface);color("subsurface_radius","散射半径 RGB（mm）",&M::subsurface_radius,true);
    flag("separate_subsurface_color","独立散射颜色",&M::separate_subsurface_color);color("subsurface_color","散射颜色 · Subsurface Color",&M::subsurface_color);
    scalar("subsurface_anisotropy","散射方向 · SSS Direction",&M::subsurface_anisotropy,-.9,.9);
    group="清漆 / Top Coat";
    scalar("coat","清漆权重 · Top Coat Weight",&M::coat);texture("coat_texture","清漆权重贴图",&M::coat_texture);
    color("coat_color","清漆颜色 · Top Coat Color",&M::coat_color);texture("coat_color_texture","清漆颜色贴图",&M::coat_color_texture,true);
    scalar("coat_roughness","清漆粗糙度 · Top Coat Roughness",&M::coat_roughness);texture("coat_roughness_texture","清漆粗糙度贴图",&M::coat_roughness_texture);
    scalar("coat_ior","清漆折射率 · Top Coat IOR",&M::coat_ior,1,4);choices("coat_mode","清漆分层 · Layering Mode",&M::coat_mode,{"反射率 / Reflectivity","加权 / Weighted","菲涅耳 / Fresnel","自定义曲线 / Custom Curve"});
    scalar("coat_normal","正向反射 · Curve Normal",&M::coat_normal);scalar("coat_grazing","掠射反射 · Curve Grazing",&M::coat_grazing);scalar("coat_exponent","曲线指数 · Curve Exponent",&M::coat_exponent,.001,100,.1);
    group="双高光 / Dual Lobe";
    scalar("dual_weight","双高光权重 · Dual Lobe Weight",&M::dual_weight);texture("dual_texture","双高光权重贴图",&M::dual_texture);
    scalar("dual_ratio","双高光比例 · Dual Lobe Ratio",&M::dual_ratio);scalar("dual_roughness1","第一高光粗糙度 · Lobe 1",&M::dual_roughness1);scalar("dual_roughness2","第二高光粗糙度 · Lobe 2",&M::dual_roughness2);scalar("dual_specular","双高光反射率 · Reflectivity",&M::dual_specular);
    texture("dual_roughness1_texture","第一高光粗糙度贴图",&M::dual_roughness1_texture);texture("dual_roughness2_texture","第二高光粗糙度贴图",&M::dual_roughness2_texture);
    group="凹凸、法线与置换 / Geometry";
    scalar("normal_strength","法线强度 · Normal Map",&M::normal_strength,0,10);texture("normal_texture","法线贴图",&M::normal_texture);
    scalar("bump_strength","凹凸强度 · Base Bump",&M::bump_strength,0,10);texture("bump_texture","凹凸贴图",&M::bump_texture);
    scalar("bump_distance","凹凸高度范围（mm）",&M::bump_distance,0,1000,.01,1000);flag("bump_from_texel_density","按纹素密度估算凹凸高度",&M::bump_from_texel_density);flag("bump_invert","反转凹凸",&M::bump_invert);
    scalar("displacement_strength","置换强度 · Displacement Strength",&M::displacement_strength,-10,10);texture("displacement_texture","置换贴图",&M::displacement_texture);
    scalar("displacement_min","最小置换（cm）",&M::displacement_min,-1000,1000,.01,100);scalar("displacement_max","最大置换（cm）",&M::displacement_max,-1000,1000,.01,100);
    group="自发光 / Emission";
    color("emission_color","发光颜色 · Emission Color",&M::emission_color);texture("emission_color_texture","发光颜色贴图",&M::emission_color_texture,true);
    scalar("emission_luminance","亮度 · Luminance",&M::emission_luminance,0,1e12,100);texture("emission_luminance_texture","亮度贴图",&M::emission_luminance_texture);
    choices("emission_units","亮度单位 · Luminance Units",&M::emission_units,{"cd/m²","kcd/m²","cd/ft²","cd/cm²","lm","W"});
    scalar("emission_temperature","色温（K，0 = 禁用）",&M::emission_temperature,0,40000,100);scalar("emission_efficacy","光效（lm/W）",&M::emission_efficacy,0,1000,1);flag("emission_two_sided","双面发光 · Two Sided Light",&M::emission_two_sided);
    group="毛发 / Hair";
    color("hair_tip_color","发梢颜色 · Hair Tip Color",&M::hair_tip_color);scalar("hair_melanin","黑色素 · Melanin",&M::hair_melanin);scalar("hair_redness","红色素 · Melanin Redness",&M::hair_redness);scalar("hair_radial_roughness","径向粗糙度 · Azimuthal Roughness",&M::hair_radial_roughness);
    texture("hair_tip_texture","发梢颜色贴图",&M::hair_tip_texture,true);
    scalar("hair_root_radius","发根半径（mm）",&M::hair_root_radius,0,10,.001,1000);scalar("hair_tip_radius","发梢半径（mm）",&M::hair_tip_radius,0,10,.001,1000);
    group="贴图坐标 / UV";
    for(bool offset:{false,true})for(int axis=0;axis<2;++axis){P p;p.id=offset?(axis?"v_offset":"u_offset"):(axis?"v_scale":"u_scale");p.label=offset?(axis?"垂直偏移 · Vertical Offset":"水平偏移 · Horizontal Offset"):(axis?"垂直平铺 · Vertical Tiles":"水平平铺 · Horizontal Tiles");p.group=group;p.minimum=-10000;p.maximum=10000;p.copy=[=](M &dst,const M &src){auto &a=offset?dst.uv_offset:dst.uv_scale;auto b=offset?src.uv_offset:src.uv_scale;(axis?a.y:a.x)=axis?b.y:b.x;};p.read=[=](const M &m)->J{auto v=offset?m.uv_offset:m.uv_scale;return axis?v.y:v.x;};p.write=[=](M &m,const J &j){double x=j.get<double>();range(x,-10000,10000);auto &v=offset?m.uv_offset:m.uv_scale;(axis?v.y:v.x)=float(x);};out.push_back(std::move(p));}
    return out;
  }();return parameters;
}
const P &material_parameter(const std::string &id){for(const auto &p:material_parameters())if(p.id==id)return p;throw std::runtime_error("未知材质参数："+id);}
std::set<std::string> material_preset_parameters(const M &preset,const M *context){
  std::set<std::string> result;if(preset.source_definition.empty()){for(const auto &p:material_parameters())result.insert(p.id);return result;}
  const auto definition=J::parse(preset.source_definition);const auto channels=definition.value("authored",definition.at("channels"));
  struct Mapping {const char *channel,*values,*textures;};
  static const Mapping mappings[]={
    {"diffuse","base_color","color_texture"},{"Metallic Weight","metallic","metallic_texture"},{"Cutout Opacity","opacity","opacity_texture"},
    {"Diffuse Overlay Weight","overlay_weight","overlay_texture"},{"Diffuse Overlay Color","overlay_color","overlay_color_texture"},{"Diffuse Overlay Roughness","overlay_roughness","overlay_roughness_texture"},{"Diffuse Overlay Weight Squared","overlay_squared",""},
    {"Glossy Roughness","roughness","roughness_texture"},{"Glossiness","roughness roughness_from_glossiness","roughness_texture"},{"Base Mixing","roughness roughness_from_glossiness roughness_texture specular specular_texture weighted_glossy",""},
    {"Glossy Layered Weight","specular","specular_texture"},{"Glossy Weight","specular","specular_texture"},{"Glossy Reflectivity","specular",""},{"Diffuse Weight","specular",""},
    {"Glossy Color","specular_color base_color","specular_color_texture color_texture"},{"Glossy Anisotropy","anisotropy",""},{"Glossy Anisotropy Rotations","anisotropy_rotation",""},
    {"Refraction Weight","transmission ior base_color color_texture roughness roughness_texture roughness_from_glossiness","transmission_texture"},{"Refraction Index","ior",""},{"Refraction Color","base_color","color_texture"},{"Refraction Roughness","roughness","roughness_texture"},{"Share Glossy Inputs","base_color color_texture roughness roughness_texture roughness_from_glossiness",""},
    {"Thin Walled","thin_walled translucency subsurface subsurface_color subsurface_radius separate_subsurface_color subsurface_anisotropy emission_two_sided",""},
    {"Translucency Weight","translucency subsurface","translucency_texture"},{"Translucency Color","translucency_color","translucency_color_texture"},{"SSS Direction","subsurface_anisotropy",""},
    {"Transmitted Color","subsurface_radius subsurface_color separate_subsurface_color",""},{"SSS Color","subsurface_radius subsurface_color separate_subsurface_color",""},{"SSS Mode","subsurface_radius subsurface_color separate_subsurface_color",""},{"SSS Amount","subsurface_radius subsurface_color separate_subsurface_color",""},{"Transmitted Measurement Distance","subsurface_radius subsurface_color",""},{"Scattering Measurement Distance","subsurface_radius subsurface_color",""},{"Base Color Effect","subsurface_color",""},{"SSS Reflectance Tint","subsurface_color",""},
    {"Top Coat Weight","coat","coat_texture"},{"Top Coat Color","coat_color","coat_color_texture"},{"Top Coat Roughness","coat_roughness","coat_roughness_texture"},{"Top Coat IOR","coat_ior",""},{"Top Coat Layering Mode","coat_mode coat_normal",""},{"Reflectivity","coat_normal",""},{"Top Coat Curve Normal","coat_normal",""},{"Top Coat Curve Grazing","coat_grazing",""},{"Top Coat Curve Exponent","coat_exponent",""},
    {"Dual Lobe Specular Weight","dual_weight","dual_texture"},{"Dual Lobe Specular Ratio","dual_ratio",""},{"Dual Lobe Specular Reflectivity","dual_specular",""},{"Specular Lobe 1 Roughness","dual_roughness1","dual_roughness1_texture"},{"Specular Lobe 2 Roughness","dual_roughness2","dual_roughness2_texture"},
    {"Normal Map","normal_strength","normal_texture"},{"Bump Strength","bump_strength","bump_texture"},{"Bump Minimum","bump_distance bump_invert bump_from_texel_density",""},{"Bump Maximum","bump_distance bump_invert bump_from_texel_density",""},
    {"Displacement Strength","displacement_strength","displacement_texture"},{"Displacement Active","displacement_strength",""},{"Minimum Displacement","displacement_min",""},{"Maximum Displacement","displacement_max",""},
    {"Emission Color","emission_color","emission_color_texture"},{"Luminance","emission_luminance","emission_luminance_texture"},{"Luminance Units","emission_units",""},{"Emission Temperature","emission_temperature",""},{"Luminous Efficacy","emission_efficacy",""},{"Two Sided Light","emission_two_sided",""},
    {"Hair Root Color","hair base_color","color_texture"},{"Root Transmission Color","hair base_color","color_texture"},{"Hair Tip Color","hair_tip_color","hair_tip_texture"},{"Tip Transmission Color","hair_tip_color","hair_tip_texture"},
    {"Roughness","roughness roughness_from_glossiness","roughness_texture"},{"Azimuthal Roughness","hair_radial_roughness",""},{"Melanin","hair_melanin",""},{"Melanin Redness","hair_redness",""},{"Line Start Width","hair_root_radius",""},{"Line End Width","hair_tip_radius",""},
    {"Horizontal Tiles","u_scale",""},{"Vertical Tiles","v_scale",""},{"Horizontal Offset","u_offset",""},{"Vertical Offset","v_offset",""}
  };
  bool share=false;if(context&&!context->source_definition.empty()){auto raw=J::parse(context->source_definition).at("channels");if(raw.contains("Share Glossy Inputs")){auto c=raw.at("Share Glossy Inputs");auto v=c.value("current_value",c.value("value",J{}));share=v.is_boolean()?v.get<bool>():v.is_number()&&v.get<double>()!=0;}}
  auto add=[&](const Mapping &mapping,const char *fields){std::istringstream stream(fields);std::string id;while(stream>>id){const std::string channel=mapping.channel;
    if(context&&(id=="base_color"||id=="color_texture")){if(channel=="Glossy Color"&&!(context->transmission>0&&share))continue;if(channel=="Refraction Color"&&!(context->transmission>0&&!share))continue;if(channel=="diffuse"&&(context->transmission>0||context->hair))continue;}
    if(context&&(id=="roughness"||id=="roughness_texture"||id=="roughness_from_glossiness")){if((channel=="Glossy Roughness"||channel=="Glossiness")&&(context->hair||(context->transmission>0&&!share)))continue;if(channel=="Refraction Roughness"&&!(context->transmission>0&&!share))continue;}
    result.insert(id);
  }};
  for(const auto &mapping:mappings)if(auto c=channels.find(mapping.channel);c!=channels.end()){if(c->contains("value")||c->contains("current_value"))add(mapping,mapping.values);if(c->contains("image")||c->contains("image_file"))add(mapping,mapping.textures);}
  return result;
}
J material_value(const M &m,const std::vector<ir::Texture> &textures,const P &p){if(p.kind!=P::texture)return p.read(m);int t=m.*p.texture_member;return t<0?J{}:texture_json(textures.at(size_t(t)));}
void set_material_value(M &m,std::vector<ir::Texture> &textures,const P &p,const J &value){
  if(p.kind!=P::texture){p.write(m,value);return;}
  if(value.is_null()){m.*p.texture_member=-1;return;}
  auto t=read_texture(value,p.colorspace);
  auto found=std::find_if(textures.begin(),textures.end(),[&](const auto &old){auto a=old,b=t;a.id.clear();b.id.clear();return a==b;});
  if(found==textures.end()){m.*p.texture_member=int(textures.size());textures.push_back(std::move(t));}else m.*p.texture_member=int(found-textures.begin());
}
M effective_material(const ir::Scene &source,const MaterialOverrides &overrides,size_t instance,size_t slot,std::vector<ir::Texture> &textures){
  const auto &i=source.instances.at(instance);auto m=source.materials.at(i.materials.at(slot));
  if(auto object=overrides.find(i.id);object!=overrides.end())if(auto patch=object->second.find(source.meshes.at(i.mesh).material_slots.at(slot));patch!=object->second.end())for(const auto &[key,value]:patch->second)set_material_value(m,textures,material_parameter(key),value);
  ir::validate(m,textures.size());return m;
}
bool apply_material_overrides(ir::Scene &scene,const ir::Scene &source,const MaterialOverrides &overrides,ir::Delta *delta){
  auto textures=source.textures;auto materials=source.materials;std::vector<std::vector<uint32_t>> bindings;
  for(size_t index=0;index<source.instances.size();++index){const auto &i=source.instances[index];auto bound=i.materials;
    if(auto object=overrides.find(i.id);object!=overrides.end())for(size_t slot=0;slot<bound.size();++slot){const auto &name=source.meshes.at(i.mesh).material_slots.at(slot);auto patch=object->second.find(name);if(patch==object->second.end()||patch->second.empty())continue;
      auto m=effective_material(source,overrides,index,slot,textures);m.id="dfv-material/"+std::to_string(i.id.size())+":"+i.id+"/"+name;bound[slot]=uint32_t(materials.size());materials.push_back(std::move(m));}
    bindings.push_back(std::move(bound));
  }
  bool layout=textures!=scene.textures||materials.size()!=scene.materials.size();
  for(size_t i=0;i<bindings.size();++i)layout|=bindings[i]!=scene.instances.at(i).materials;
  if(delta&&!layout)for(size_t i=0;i<materials.size();++i)if(materials[i]!=scene.materials[i])delta->materials.push_back({uint32_t(i),materials[i]});
  scene.textures=std::move(textures);scene.materials=std::move(materials);for(size_t i=0;i<bindings.size();++i)scene.instances[i].materials=std::move(bindings[i]);return layout;
}
void prune_material_overrides(const ir::Scene &source,MaterialOverrides &overrides){
  for(auto entry=overrides.begin();entry!=overrides.end();){const auto i=std::find_if(source.instances.begin(),source.instances.end(),[&](const auto &i){return i.id==entry->first;});if(i!=source.instances.end()){const auto &slots=source.meshes.at(i->mesh).material_slots;std::erase_if(entry->second,[&](const auto &p){return p.second.empty()||std::find(slots.begin(),slots.end(),p.first)==slots.end();});}if(i==source.instances.end()||entry->second.empty())entry=overrides.erase(entry);else ++entry;}
}
void validate_material_overrides(const ir::Scene &source,const MaterialOverrides &overrides){
  auto copy=overrides;prune_material_overrides(source,copy);if(copy!=overrides)throw std::runtime_error("材质覆盖引用不存在的对象、表面或空修改");auto textures=source.textures;
  for(size_t i=0;i<source.instances.size();++i)if(overrides.contains(source.instances[i].id))for(size_t slot=0;slot<source.instances[i].materials.size();++slot)effective_material(source,overrides,i,slot,textures);
}
}
