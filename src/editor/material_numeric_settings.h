#pragma once
#include "render_ir/scene.h"
#include "runtime/parameter_settings.h"

namespace dfv::editor {
// 只继承与源通道一一对应的数值；混合、色彩转换和多通道推导值使用原生默认规则。
inline std::map<std::string,runtime::ParameterSettings> material_numeric_settings(const ir::Material &material){
  std::map<std::string,runtime::ParameterSettings> result;
  if(material.source_definition.empty())return result;
  const auto definition=nlohmann::json::parse(material.source_definition);
  const auto channels=definition.value("channels",nlohmann::json::object());
  const std::pair<const char *,const char *> mapping[]={
    {"metallic","Metallic Weight"},{"opacity","Cutout Opacity"},
    {"overlay_weight","Diffuse Overlay Weight"},{"overlay_roughness","Diffuse Overlay Roughness"},
    {"anisotropy","Glossy Anisotropy"},{"anisotropy_rotation","Glossy Anisotropy Rotations"},
    {"transmission","Refraction Weight"},{"ior","Refraction Index"},
    {"translucency","Translucency Weight"},{"subsurface_anisotropy","SSS Direction"},
    {"coat","Top Coat Weight"},{"coat_roughness","Top Coat Roughness"},{"coat_ior","Top Coat IOR"},
    {"coat_normal","Top Coat Curve Normal"},{"coat_grazing","Top Coat Curve Grazing"},{"coat_exponent","Top Coat Curve Exponent"},
    {"dual_weight","Dual Lobe Specular Weight"},{"dual_ratio","Dual Lobe Specular Ratio"},
    {"dual_roughness1","Specular Lobe 1 Roughness"},{"dual_roughness2","Specular Lobe 2 Roughness"},{"dual_specular","Dual Lobe Specular Reflectivity"},
    {"normal_strength","Normal Map"},{"bump_strength","Bump Strength"},
    {"displacement_strength","Displacement Strength"},{"displacement_min","Minimum Displacement"},{"displacement_max","Maximum Displacement"},
    {"emission_luminance","Luminance"},{"emission_temperature","Emission Temperature"},{"emission_efficacy","Luminous Efficacy"},
    {"hair_melanin","Melanin"},{"hair_redness","Melanin Redness"},{"hair_radial_roughness","Azimuthal Roughness"},
    {"u_scale","Horizontal Tiles"},{"v_scale","Vertical Tiles"},{"u_offset","Horizontal Offset"},{"v_offset","Vertical Offset"}
  };
  auto inherit=[&](const std::string &id,const std::string &name){
    auto found=channels.find(name);if(found==channels.end()||!found->is_object())return;const auto &c=*found;
    runtime::ParameterSettings s{c.value("clamped",false),c.value("min",0.),c.value("max",1.),c.value("step_size",.1)};
    if(!std::isfinite(s.step)||s.step<=0)s.step=.1;runtime::validate(s);result[id]=s;
  };
  for(const auto &[id,name]:mapping)inherit(id,name);
  // 粗糙度字段有多种来源；只沿用实际参与转换的通道。
  bool share=false;if(auto c=channels.find("Share Glossy Inputs");c!=channels.end()){const auto v=c->value("current_value",c->value("value",nlohmann::json{}));share=v.is_boolean()?v.get<bool>():v.is_number()&&v.get<double>()!=0;}
  inherit("roughness",material.hair?"Roughness":material.transmission>0&&!share?"Refraction Roughness":material.roughness_from_glossiness?"Glossiness":"Glossy Roughness");
  return result;
}
}
