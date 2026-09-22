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
// 离线 PNG 与视口使用同一套色调参数；线性 EXR 保留原始辐亮度。
inline Vec3 display_color(const RenderOptions &o,Vec3 input,float u=.5f,float v=.5f,float aspect=1) {
  float x[3]={std::max(0.f,input.x),std::max(0.f,input.y),std::max(0.f,input.z)};const auto &n=o.tonemapper;
  if(n.id.empty()||!number(n,"Tone Mapping Enable",1)) {
    for(auto &a:x) a=a<=.0031308f?12.92f*a:1.055f*std::pow(a,1/2.4f)-.055f;
  } else {
    const auto white=color(n,"White Point");const float w[]={white.x,white.y,white.z};
    const float px=(u*2-1)*std::max(aspect,1.f)*.422793f,py=(v*2-1)*std::max(1/aspect,1.f)*.422793f;
    const float gain=exposure(o)*std::pow(1+px*px+py*py,-float(number(n,"Vignetting",0)));
    for(int i=0;i<3;++i) x[i]*=gain/std::max(.0001f,w[i]*float(number(n,"White Point Scale",1)));
    const float l=x[0]*.2126f+x[1]*.7152f+x[2]*.0722f,burn=float(number(n,"Burn Highlights",.25));
    for(auto &a:x) {const float level=number(n,"Burn Highlights Per Component",1)?a:l;a*= (1+burn*level)/(1+level);
      const float mix=1-std::clamp(a,0.f,1.f);a=a*(1-mix)+std::pow(std::max(0.f,a),1+2*float(number(n,"Crush Blacks",.2)))*mix;}
    const float gray=x[0]*.2126f+x[1]*.7152f+x[2]*.0722f;
    for(auto &a:x) a=std::pow(std::max(0.f,gray+(a-gray)*float(number(n,"Saturation",1))),1/std::max(.001f,float(number(n,"Gamma",2.2))));
  }return {x[0],x[1],x[2]};
}
inline void set_option(OptionNode &node,size_t index,size_t component,double value) {
  auto &p=node.parameters.at(index);if(!p.supported||!std::isfinite(value)) throw std::runtime_error("无效或尚不支持的渲染参数");
  if(p.type=="bool"||p.type=="enum"||p.type=="int") value=std::round(value);
  if(p.type=="bool"||p.type=="enum") value=std::clamp(value,p.minimum,p.maximum);
  if((p.id=="Gamma"||p.id=="Aperture"||p.id=="Shutter Speed"||p.id=="White Point Scale"||p.id=="White Point")&&value<=0) throw std::runtime_error("此参数必须大于零");
  p.value.at(component)=value;
  auto update=[&](const std::string &id,double v) {for(auto &q:node.parameters) if(q.id==id&&!q.value.empty()) q.value[0]=v;};
  if(p.id=="Exposure Value") update("Shutter Speed",std::exp2(value)*std::max(number(node,"Film ISO",100),1.0)/(100*std::pow(std::max(number(node,"Aperture",8),.001),2)));
  if(p.id=="Shutter Speed"||p.id=="Aperture"||p.id=="Film ISO") {
    const auto iso=number(node,"Film ISO",100);if(iso>0) update("Exposure Value",std::log2(std::max(number(node,"Shutter Speed",128),.001)*std::pow(std::max(number(node,"Aperture",8),.001),2)*100/iso));
  }
}
}
