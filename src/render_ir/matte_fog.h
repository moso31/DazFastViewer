#pragma once
#include "render_ir/options.h"

namespace dfv::ir {
inline bool matte_fog_option(const std::string &id) {
  return id=="Matte Fog"||id=="Matte Fog Visibility"||id=="Matte Fog Visibility Tint"||
    id=="Matte Fog Brightness"||id=="Matte Fog Brightness Relative to Environment"||id=="Matte Fog Brightness Tint"||
    id=="DFV Matte Fog Start"||id=="DFV Matte Fog Base Height"||id=="DFV Matte Fog Scale Height";
}
// Upgrade DAZ's saved conditional visibility and the old viewer's supported=false.
// Preserve values and unrelated options.
inline void ensure_matte_fog_options(OptionNode &node) {
  if(node.id.empty())return;
  // Retire the camera-only background overlay. Old files adopt the same
  // atmosphere for surfaces and misses; never interpret an angle as metres.
  std::erase_if(node.parameters,[](const auto &p){return p.id=="DFV Matte Fog Background Distance"||p.id=="DFV Matte Fog Horizon Height";});
  auto add=[&](const char *id,const char *label,const char *type,std::vector<double> value,double low,double high,double step) {
    auto it=std::find_if(node.parameters.begin(),node.parameters.end(),[&](const auto &p){return p.id==id;});
    if(it==node.parameters.end()) {Option p;p.id=id;p.value=std::move(value);node.parameters.push_back(std::move(p));it=std::prev(node.parameters.end());}
    auto &p=*it;p.label=label;p.type=type;p.group="/Environment/Matte Fog";p.supported=true;p.visible=true;
    p.minimum=low;p.maximum=high;p.step=step;p.choices.clear();
    if(p.value.size()!=(p.type=="float_color"?3:1))p.supported=false;
  };
  add("Matte Fog","Matte Fog","bool",{0},0,1,1);
  add("Matte Fog Visibility","能见度（m）","float",{10000},.001,1e9,10);
  add("Matte Fog Visibility Tint","能见度颜色倍率","float_color",{1,1,1},0,1,.01);
  add("Matte Fog Brightness","雾亮度","float",{1},0,1e6,.05);
  add("Matte Fog Brightness Relative to Environment","雾亮度相对环境","bool",{1},0,1,1);
  add("Matte Fog Brightness Tint","雾颜色","float_color",{1,1,1},0,1,.01);
  add("DFV Matte Fog Start","起雾距离（m）","float",{0},0,1e9,10);
  add("DFV Matte Fog Base Height","雾基准高度（m）","float",{0},-1e9,1e9,10);
  add("DFV Matte Fog Scale Height","雾密度衰减高度（m）","float",{1000},.001,1e9,10);
}
struct MatteFog {
  bool enabled=false,relative=true;
  Vec3 extinction{},color{1,1,1};
  float start=0,base_height=0,scale_height=1000;
};
inline MatteFog matte_fog(const RenderOptions &options) {
  const auto &n=options.environment;MatteFog fog;
  auto finite=[](double v,double fallback){return std::isfinite(v)?v:fallback;};
  fog.enabled=number(n,"Matte Fog",0)!=0;
  fog.relative=number(n,"Matte Fog Brightness Relative to Environment",1)!=0;
  const double visibility=std::clamp(finite(number(n,"Matte Fog Visibility",10000),10000),.001,1e9);
  const auto tint=color(n,"Matte Fog Visibility Tint");
  // Koschmieder visibility: 2% contrast remains at the specified distance.
  auto extinction=[&](float v){return float(3.912023005428146/(visibility*std::clamp(finite(v,1),.000001,1.0)));};
  fog.extinction={extinction(tint.x),extinction(tint.y),extinction(tint.z)};
  const double brightness=std::clamp(finite(number(n,"Matte Fog Brightness",1),1),0.0,1e6);
  const auto rgb=color(n,"Matte Fog Brightness Tint");
  auto radiance=[&](float v){return float(brightness*std::clamp(finite(v,1),0.0,1.0));};
  fog.color={radiance(rgb.x),radiance(rgb.y),radiance(rgb.z)};
  fog.start=float(std::clamp(finite(number(n,"DFV Matte Fog Start",0),0),0.0,1e9));
  fog.base_height=float(std::clamp(finite(number(n,"DFV Matte Fog Base Height",0),0),-1e9,1e9));
  fog.scale_height=float(std::clamp(finite(number(n,"DFV Matte Fog Scale Height",1000),1000),.001,1e9));
  return fog;
}
}
