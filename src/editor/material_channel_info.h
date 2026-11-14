#pragma once
#include "editor/materials.h"
#include "viewport/render_quality.h"

namespace dfv::editor {
struct MaterialChannelInfo {
  std::string status,detail;
  std::set<std::string> parameters;
  bool unsupported=false;
};
// 源值不等于渲染值：区分转换、模式选择、关闭的层、元数据和真正未实现的通道。
inline MaterialChannelInfo material_channel_info(const ir::Material &m,const ir::MaterialSourceChannel &c,
    const nlohmann::json &channels,const RenderQuality &quality,bool uses_curves=false){
  using J=nlohmann::json;MaterialChannelInfo info;
  auto value=[&](const char *id,double fallback){const auto it=channels.find(id);if(it==channels.end())return fallback;const auto v=it->value("current_value",it->value("value",J{}));return v.is_boolean()?(v.get<bool>()?1.:0.):v.is_number()?v.get<double>():fallback;};
  if(auto it=channels.find(c.id);it!=channels.end()){
    ir::Material single;single.source_definition=J{{"channels",{{c.id,*it}}}}.dump();
    info.parameters=material_preset_parameters(single,&m);
  }
  const auto &id=c.id;const bool share=value("Share Glossy Inputs",0)!=0;
  const int mixing=int(value("Base Mixing",0));
  if(id=="Tags"||id=="Line Preview Color"||id=="OutlineColorID"){
    info.status="说明 / 预览信息";info.detail="DAZ 标签或预览颜色，不是表面着色参数。";return info;
  }
  if((id=="Refraction Color"||id=="Refraction Roughness"||id=="Refraction Glossiness")&&share){
    info.status="源输入未采用";info.detail="导入时 Share Glossy Inputs 已启用，折射颜色和粗糙度来自高光通道；编辑器中的独立折射参数仍可单独修改。";info.parameters.clear();return info;
  }
  const bool glossiness=id=="Glossiness"||id=="Refraction Glossiness"||id=="Top Coat Glossiness"||id=="Specular Lobe 1 Glossiness"||id=="Specular Lobe 2 Glossiness"||id=="Backscattering Glossiness"||id=="Metallic Flakes Glossiness";
  if(glossiness&&!m.roughness_from_glossiness){info.status="源模式未采用";info.detail="当前使用粗糙度；保留的光泽度数值不代表它参与着色。";info.parameters.clear();return info;}
  if(((id=="Glossy Weight"||id=="Diffuse Weight")&&mixing!=2)||((id=="Glossy Layered Weight"||id=="Glossy Reflectivity")&&mixing==2)||(id=="Glossy Specular"&&mixing!=1)){
    info.status="源模式未采用";info.detail="该通道不用于当前 Base Mixing 模式。";info.parameters.clear();return info;
  }
  std::string inactive;
  if(id.starts_with("Line ")&&!uses_curves)inactive="所选表面没有曲线几何，线宽不影响三角形网格。";
  if(id.starts_with("Top Coat")&&id!="Top Coat Weight"&&m.coat<=0)inactive="清漆权重为 0。";
  if((id.starts_with("Metallic Flakes"))&&id!="Metallic Flakes Weight"&&value("Metallic Flakes Weight",0)==0)inactive="源材质的金属薄片层权重为 0。";
  if(id.starts_with("Backscattering")&&id!="Backscattering Weight"&&value("Backscattering Weight",0)==0)inactive="源材质的背向散射层权重为 0。";
  if(id.starts_with("SSS ")||id=="Scattering Measurement Distance"||id=="Transmitted Color"||id=="Transmitted Measurement Distance"){
    if(!quality.subsurface)inactive="应用设置已禁用次表面散射；保留材质数值。";
    else if(m.thin_walled||m.subsurface<=0)inactive="当前材质未使用体积次表面散射。";
  }
  info.unsupported=!c.mapped;
  if(!inactive.empty()){info.status=c.mapped?"当前未启用":"未启用层 / 未支持";info.detail=inactive+(c.mapped?"":" 此源通道尚无渲染实现。");return info;}
  if(!c.mapped){info.status="未支持";info.detail="源文件中的数值已保留，但目前没有对应的渲染实现。非零源值也不会自动生效。";return info;}
  info.status="已转换";info.detail="下列当前值包含材质预设和编辑覆盖；颜色为线性值。";
  bool texture_mapped=false;for(const auto &p:info.parameters)texture_mapped|=material_parameter(p).kind==MaterialParameter::texture;
  if(!c.image.empty()&&!texture_mapped){info.status="仅数值已转换";info.detail="常量已转换，源贴图尚未映射。\n"+info.detail;}
  return info;
}
}
