#pragma once
#include "render_ir/options.h"
#include "render_ir/night_sky.h"
namespace dfv::ir {
inline OptionNode default_options(bool environment){
  OptionNode n;n.id=environment?"dfv-environment":"dfv-tonemapper";n.label=environment?"环境设置":"色调设置";
  auto add=[&](const char *id,const char *label,double value,double minimum,double maximum,double step=.01,const char *type="float") -> Option & {
    Option p;p.id=id;p.label=label;p.group=environment?"/Environment":"/色调";p.type=type;p.value={value};p.minimum=minimum;p.maximum=maximum;p.step=step;p.supported=true;n.parameters.push_back(p);return n.parameters.back();};
  if(environment){
    add("Environment Mode","环境模式",0,0,3,1,"enum").choices={"穹顶与场景","仅穹顶","太阳与天空","仅场景"};
    add("Environment Intensity","环境强度",1,0,1000);add("Environment Map","环境贴图强度",1,0,1000);
    add("Environment Tint","环境颜色",1,0,10,.01,"float_color").value={1,1,1};add("Draw Dome","显示穹顶",1,0,1,1,"bool");add("Dome Rotation","穹顶旋转",0,-360,360,1);
    add("SS Latitude","纬度",0,-90,90,1);add("SS Longitude","经度",0,-180,180,1);add("SS Day","日期（儒略日）",2457092,2000000,3000000,1);add("SS Time","当地时间",43200,0,86400,1800);add("SS UTC Offset","SS UTC Offset",0,-14,14,.5);
    add("SS Sun Disk Intensity","太阳强度",1,0,100);add("SS Sun Disk Scale","太阳大小",1,.01,100);add("SS Haze","雾霾",0,0,10);add("SS Multiplier","天空强度",1,0,100);
  }else{
    add("Tone Mapping Enable","启用色调映射",1,0,1,1,"bool");add("Exposure Value","曝光值",13,-20,30,.1);add("Film ISO","感光度 ISO",100,1,102400,10);
    add("Shutter Speed","快门速度倒数",128,.001,100000,1);add("Aperture","光圈",8,.001,128,.1);add("cm2 Factor","亮度系数",1,0,1000);
    add("White Point","白点",1,.001,100,.01,"float_color").value={1,1,1};add("White Point Scale","白点缩放",1,.001,100);
    add("Burn Highlights","高光压缩",.25,0,1);add("Burn Highlights Per Component","按分量处理高光",1,0,1,1,"bool");add("Crush Blacks","暗部压缩",.2,0,1);add("Saturation","饱和度",1,0,5);add("Gamma","伽马",2.2,.001,5,.1);add("Vignetting","暗角",0,0,10);
  }if(environment)ensure_night_options(n);return n;
}
}
