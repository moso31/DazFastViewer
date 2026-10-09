#include "cycles/night_sky_model.h"
#include <OpenImageIO/imageio.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ppl.h>
#include <nlohmann/json.hpp>
using namespace ccl;
static float original_sphere(float3 p,float seed,float radius,float entropy,float softness){
  const float3 cell=make_float3(floorf(p.x),floorf(p.y),floorf(p.z));const float3 local=p-cell-make_float3(.5f,.5f,.5f);float result=0;
  for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){const float3 n=make_float3(float(x),float(y),float(z));const auto offset=dfv_night_hash(cell+n,dfv_night_madd(seed,2,.15f))-make_float3(.5f,.5f,.5f);const float id=dfv_night_hash(offset,13.17f+seed).x;const float r=(dfv_night_hash(cell+n,seed+13.17f).y-.5f)*entropy+radius;result=ccl::fmaxf(result,dfv_night_smooth(r+softness,r-softness,len(n+offset-local))*id);}return result;
}
int main(int argc,char **argv){try{
  const std::filesystem::path dir=argc>1?argv[1]:"artifacts/night-sky/web-reference";std::filesystem::create_directories(dir);
  float max_error=0;
  for(int i=0;i<256;++i){const float3 p=make_float3(i*.1837f-21,i*.9123f-70,i*.237f-32);for(int k=0;k<3;++k){const float r=k==0?.5f:.44642857f,e=k==0?.5f:.25f,s=k==0?.235f:k==1?.05f:.3f;const float seed=398.432f+i*.37f;max_error=ccl::fmaxf(max_error,fabsf(original_sphere(p,seed,r,e,s)-dfv_night_sphere(p,seed,r,e,s)));}}
  if(max_error>1e-6f)throw std::runtime_error("球支持区优化改变了网页球状噪声");
  constexpr int width=800,height=450;std::vector<unsigned char> pixels(width*height*3);std::vector<float> raw_rgb(width*height*3);
  const float a=.2f,b=.25f;const float3 position=make_float3(-10*cosf(a)*sinf(b),10*sinf(a),-10*cosf(a)*cosf(b));const float3 forward=normalize(-position),right=normalize(cross(forward,make_float3(0,1,0))),up=normalize(cross(right,forward));
  const auto start=std::chrono::steady_clock::now();
  concurrency::parallel_for(0,height,[&](int y){for(int x=0;x<width;++x){const float u=(x+.5f)/width-.5f,v=(height-y-.5f)/width-float(height)/(2*width);const auto ray=normalize(forward*.35f+right*u+up*v);const auto raw=dfv_night_raw(ray,1,true);float3 galaxy,stars;dfv_night_finish(raw,1,1,&galaxy,&stars);const auto color=galaxy+stars;const size_t i=(size_t(y)*width+x)*3;raw_rgb[i]=color.x;raw_rgb[i+1]=color.y;raw_rgb[i+2]=color.z;pixels[i]=(unsigned char)(clamp(color_linear_to_srgb(color.x),0.f,1.f)*255+.5f);pixels[i+1]=(unsigned char)(clamp(color_linear_to_srgb(color.y),0.f,1.f)*255+.5f);pixels[i+2]=(unsigned char)(clamp(color_linear_to_srgb(color.z),0.f,1.f)*255+.5f);}});
  auto out=OIIO::ImageOutput::create((dir/"reference.png").string());if(!out||!out->open((dir/"reference.png").string(),OIIO::ImageSpec(width,height,3,OIIO::TypeDesc::UINT8))||!out->write_image(OIIO::TypeDesc::UINT8,pixels.data())||!out->close())throw std::runtime_error("参考图输出失败");
  std::ofstream(dir/"reference-linear.bin",std::ios::binary).write(reinterpret_cast<const char *>(raw_rgb.data()),std::streamsize(raw_rgb.size()*sizeof(float)));
  const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();std::ofstream(dir/"result.json")<<nlohmann::json{{"result","PASS"},{"sphere_max_error",max_error},{"width",width},{"height",height},{"render_ms",ms}}.dump(2);std::cout<<"Reference shader PASS; max sphere error="<<max_error<<"; render_ms="<<ms<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
