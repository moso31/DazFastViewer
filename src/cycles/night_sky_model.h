#pragma once
// 根据用户提供的 Shadertoy wcGSDy 源码适配：球状云团、远近星光及分层吸收。
// 原参考：https://www.shadertoy.com/view/wcGSDy（At the edge of the Galaxy）。
// CPU 补光积分和 GPU 绘制共用；没有方向扭曲、天空图片或逐星光源。
#include "util/math.h"
#include "util/color.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline float dfv_night_madd(float a,float b,float c)
{
#if defined(__KERNEL_CUDA__) || defined(__KERNEL_OPTIX__)
  return __fadd_rn(__fmul_rn(a,b),c);
#else
  volatile float product=a*b;return product+c;
#endif
}
ccl_device_inline float3 dfv_night_hash(float3 p,float seed)
{
  uint x=__float_as_uint(dfv_night_madd(seed,-395.23f,p.x));
  uint y=__float_as_uint(dfv_night_madd(seed,705.23f,p.y));
  uint z=__float_as_uint(dfv_night_madd(seed,966.37f,p.z));
  for(int i=0;i<3;++i){const uint a=((x>>16)^y)*1111111111u,b=((y>>16)^z)*1111111111u,c=((z>>16)^x)*1111111111u;x=a;y=b;z=c;}
  return make_float3(float(x),float(y),float(z))*(1.0f/4294967296.0f);
}
ccl_device_inline float dfv_night_smooth(float a,float b,float x)
{
  const float t=clamp((x-a)/(b-a),0.0f,1.0f);return t*t*(3.0f-2.0f*t);
}
ccl_device_noinline float dfv_night_sphere(float3 p,float seed,float radius,float entropy,float softness)
{
  const float3 cell=make_float3(floorf(p.x),floorf(p.y),floorf(p.z));
  const float3 local=p-cell-make_float3(.5f,.5f,.5f);float result=0;
  for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
    const float3 neighbor=make_float3(float(x),float(y),float(z));
    const float3 center=dfv_night_hash(cell+neighbor,dfv_night_madd(seed,2.0f,.15f))-make_float3(.5f,.5f,.5f);
    const float3 delta=neighbor+center-local;const float distance2=dot(delta,delta);
    // 球支持区外不再求半径、开方和第三次哈希；不改变原球状噪声结果。
    const float reach=radius+.5f*entropy+softness;
    if(distance2>=reach*reach)continue;
    const float r=(dfv_night_hash(cell+neighbor,seed+13.17f).y-.5f)*entropy+radius;
    if(distance2>=(r+softness)*(r+softness))continue;
    const float id=dfv_night_hash(center,13.17f+seed).x;
    result=fmaxf(result,dfv_night_smooth(r+softness,r-softness,sqrtf(distance2))*id);
  }
  return result;
}
ccl_device_noinline float dfv_night_nebula(float3 p,float seed,int steps,float softness,float ratio_a,float ratio_f)
{
  float amplitude=1,frequency=1,result=0;const float curve=softness*.9f+.1f;
  for(int i=1;i<=steps;++i){result+=powf(dfv_night_sphere(p*frequency,dfv_night_madd(float(i),7.343f,seed),.5f,.5f,curve),curve)*amplitude;amplitude*=ratio_a;frequency*=ratio_f;}
  return result;
}
// J2000 银河坐标 → 原网页的轴向，保留原 seed 和云团坐标。
ccl_device_inline float3 dfv_night_reference_direction(float3 e)
{
  const float3 g=make_float3(dot(e,make_float3(-.05487556f,-.87343709f,-.48383502f)),dot(e,make_float3(.49410943f,-.44482963f,.74698224f)),dot(e,make_float3(-.86766615f,-.19807637f,.45598378f)));
  const float3 axis=normalize(make_float3(.4f,1,-.2f));
  const float3 center=normalize(cross(axis,cross(make_float3(0,0,1),axis)));
  return center*g.x+cross(axis,center)*g.y+axis*g.z;
}
struct DfvNightRaw {
  float3 galaxy;
  float3 transmission;
  float stars[6];
};
ccl_device_noinline DfvNightRaw dfv_night_raw(float3 direction,int quality,bool stars)
{
  DfvNightRaw out;for(int i=0;i<6;++i)out.stars[i]=0;
  const int steps=quality<=0?2:quality==1?4:5;
  const float3 axis=normalize(make_float3(.4f,1,-.2f)),orient=make_float3(0,0,1);
  const float3 projected=normalize(cross(axis,cross(orient,axis)));
  const float3 nx=normalize(cross(orient,axis)),ny=normalize(cross(nx,orient)),nz=normalize(cross(ny,nx));
  const float3 p=make_float3(dot(nx,direction),dot(ny,direction),dot(nz,direction));
  const float latitude=dot(direction,axis),longitude=dot(direction,projected);
  const float core_mask=dfv_night_smooth(0,.7f,-latitude+longitude*.7f+.22f)*dfv_night_smooth(0,.7f,latitude+longitude*.7f+.22f);
  const float glow_mask=dfv_night_smooth(0,.7f,-latitude+longitude*.3f+.47f)*dfv_night_smooth(0,.7f,latitude+longitude*.3f+.47f);
  const float periphery_mask=powf(dfv_night_smooth(0,.9f,-latitude+longitude*.19f+.22f)*dfv_night_smooth(0,.9f,latitude+longitude*.19f+.22f),.4f);
  const float nebula_mask=dfv_night_smooth(0,.8f,-latitude+longitude*.13f+.75f)*dfv_night_smooth(0,.8f,latitude+longitude*.13f+.75f);
  const float core=core_mask>0?fmaxf(0,powf(core_mask,3)*5-.5f*dfv_night_nebula(p*5,291.432f+107,steps,.15f,.7f,1.8f)):0;
  const float periphery=periphery_mask>0?fmaxf(0,periphery_mask*4-dfv_night_nebula(p*5,583.457f+107,steps,.1f,.6f,2.1f)):0;
  const float nebula=nebula_mask>0?fmaxf(0,nebula_mask*9-1.5f*dfv_night_nebula(p*4,1276.12f+107,steps,.2f,.6f,2.1f)):0;
  const float3 absorption=make_float3(.7f,.85f,.95f);
  const float3 outer=exp(-absorption*(powf(nebula*.7f,5.1f)*.0015f));
  const float3 inner=exp(-absorption*(periphery*.2f));
  out.transmission=outer*inner;
  out.galaxy=(make_float3(.99f,.95f,.9f)*(powf(core,3)*.002f+powf(glow_mask,2)*.07f)*inner+make_float3(.1f,.3f,.99f)*(periphery*.05f))*outer;
  if(stars){
    // 固定参考角尺度：原网页 800px / CameraZoom=.35。尺寸不随窗口分辨率跳变。
    const float s=500.0f/(.35f*800.0f);
    for(int layer=0;layer<2;++layer){const float seed=layer==0?435.34f:968.148f;
      out.stars[layer*3]=dfv_night_sphere(direction*175,seed+254.564f,.45f*s,.025f,.1f);
      out.stars[layer*3+1]=dfv_night_sphere(direction*175,seed+26.274f,.25f*s,.25f,.05f);
      out.stars[layer*3+2]=dfv_night_sphere(direction*225,seed+656.344f,.25f*s,.25f,.3f);
    }
  }
  return out;
}
ccl_device_inline float3 dfv_night_look(float3 c)
{
  // 原网页返回 1-exp(-c*7*1.5) 的显示值；先转回线性，交给场景统一曝光。
  // 这样保留网页的红褐色吸收层，而不会把显示色当线性值再提亮一次。
  c=make_float3(1,1,1)-exp(-c*10.5f);
  return make_float3(color_srgb_to_linear(c.x),color_srgb_to_linear(c.y),color_srgb_to_linear(c.z));
}
ccl_device_inline void dfv_night_finish(const DfvNightRaw &raw,float contrast,float density,ccl_private float3 *galaxy,ccl_private float3 *stars)
{
  const float3 base=dfv_night_look(raw.galaxy);
  const float exponent=fmaxf(contrast,.0001f);
  *galaxy=make_float3(powf(base.x,exponent),powf(base.y,exponent),powf(base.z,exponent));
  if(density<=0){*stars=zero_float3();return;}
  float light[2];
  for(int k=0;k<2;++k){const float curve=float(k+1)/fmaxf(density,.0001f);light[k]=(.196f)*(1.5f*powf(raw.stars[k*3],1000*curve)+.5f*powf(raw.stars[k*3+1],4*curve)+.25f*powf(raw.stars[k*3+2],curve));}
  const float3 total=raw.galaxy+raw.transmission*(light[0]*.13f)+make_float3(light[1]*.46f,light[1]*.46f,light[1]*.46f);
  *stars=max(dfv_night_look(total)-base,zero_float3());
}
CCL_NAMESPACE_END
