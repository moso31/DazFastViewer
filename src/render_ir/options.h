#pragma once
#include "render_ir/scene.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dfv::ir {
inline const Option *option(const OptionNode &node,const std::string &id) {
  for(const auto &p:node.parameters) if(p.id==id) return &p;return nullptr;
}
inline double number(const OptionNode &node,const std::string &id,double fallback) {
  const auto *p=option(node,id);return p&&!p->value.empty()?p->value[0]:fallback;
}
inline Vec3 color(const OptionNode &node,const std::string &id,Vec3 fallback={1,1,1}) {
  const auto *p=option(node,id);return p&&p->value.size()==3?Vec3{float(p->value[0]),float(p->value[1]),float(p->value[2])}:fallback;
}
inline bool scene_lights(const RenderOptions &o) {const int mode=int(number(o.environment,"Environment Mode",0));return o.environment.id.empty()||mode==0||mode==3;}
// 相对 HDRI 曝光基准为 EV 13；绝对测光尚需跨渲染器校准。
inline float exposure(const RenderOptions &o) {
  const auto &n=o.tonemapper;if(n.id.empty()||!number(n,"Tone Mapping Enable",1)) return 1;
  const auto iso=number(n,"Film ISO",100),factor=number(n,"cm2 Factor",1);
  return float(std::clamp(factor*(iso==0?1:std::exp2(13-number(n,"Exposure Value",13))),0.0,1e12));
}
inline void set_option(OptionNode &node,size_t index,size_t component,double value) {
  auto &p=node.parameters.at(index);if(!p.supported||!std::isfinite(value)) throw std::runtime_error("无效或尚不支持的渲染参数");
  if(p.type=="bool"||p.type=="enum"||p.type=="int") value=std::round(value);
  value=std::clamp(value,p.minimum,p.maximum);p.value.at(component)=value;
  auto update=[&](const std::string &id,double v) {for(auto &q:node.parameters) if(q.id==id&&!q.value.empty()) q.value[0]=v;};
  if(p.id=="Exposure Value") update("Shutter Speed",std::exp2(value)*std::max(number(node,"Film ISO",100),1.0)/(100*std::pow(std::max(number(node,"Aperture",8),.001),2)));
  if(p.id=="Shutter Speed"||p.id=="Aperture"||p.id=="Film ISO") {
    const auto iso=number(node,"Film ISO",100);if(iso>0) update("Exposure Value",std::log2(std::max(number(node,"Shutter Speed",128),.001)*std::pow(std::max(number(node,"Aperture",8),.001),2)*100/iso));
  }
}
}
