#pragma once
#include "session/output_driver.h"
#include <OpenImageIO/imageio.h>
#include <filesystem>
#include <vector>
#include <cmath>
#include <algorithm>

namespace dfv {
// 保存线性 EXR 与精确 sRGB PNG，并向调用者传播写出错误。
class Output final: public ccl::OutputDriver {
  std::filesystem::path directory_;
  bool albedo_=false;
public:
  std::vector<float> linear_pixels;
  std::string error;
  bool written=false;
  explicit Output(std::filesystem::path directory,bool albedo=false):directory_(std::move(directory)),albedo_(albedo) {}
  void write_render_tile(const Tile &tile) override {
    if(tile.size!=tile.full_size) {error="不支持分块输出";return;}
    const int w=tile.size.x,h=tile.size.y;
    std::vector<float> linear(size_t(w)*h*4),flipped(linear.size());
    if(!tile.get_pass_pixels("combined",4,linear.data())) {error="读取 Combined 失败";return;}
    for(int y=0;y<h;++y) std::copy_n(linear.data()+size_t(y)*w*4,size_t(w)*4,flipped.data()+size_t(h-1-y)*w*4);
    linear_pixels=flipped;
    std::vector<unsigned char> srgb(flipped.size());
    for(size_t i=0;i<flipped.size();++i) {
      float v=flipped[i];
      if(!std::isfinite(v)) {error="渲染结果存在 NaN/Inf";return;}
      if(i%4!=3) v=v<=0.0031308f?12.92f*v:1.055f*std::pow(v,1/2.4f)-0.055f;
      srgb[i]=static_cast<unsigned char>(std::clamp(v,0.0f,1.0f)*255+0.5f);
    }
    for(bool exr:{true,false}) {
      const auto path=(directory_/(exr?"smoke.exr":"smoke.png")).string();
      auto output=OIIO::ImageOutput::create(path);
      const OIIO::TypeDesc type=exr?OIIO::TypeDesc::FLOAT:OIIO::TypeDesc::UINT8;
      OIIO::ImageSpec spec(w,h,4,type);spec.attribute("oiio:ColorSpace",exr?"Linear":"sRGB");
      if(!output || !output->open(path,spec) ||
         !output->write_image(type,exr?static_cast<void *>(flipped.data()):static_cast<void *>(srgb.data())) || !output->close()) {
        error="写出失败: "+path;return;
      }
    }
    written=true;
    if(albedo_) {
      if(!tile.get_pass_pixels("diffuse_color",3,linear.data())) {error="读取 Diffuse Color 失败";return;}
      std::vector<unsigned char> albedo(size_t(w)*h*3);
      for(int y=0;y<h;++y) for(int x=0;x<w*3;++x) {
        float v=linear[size_t(y)*w*3+x];
        if(!std::isfinite(v)) {error="Diffuse Color 包含 NaN/Inf";return;}
        v=v<=.0031308f?12.92f*v:1.055f*std::pow(std::max(v,0.f),1/2.4f)-.055f;
        albedo[size_t(h-1-y)*w*3+x]=static_cast<unsigned char>(std::clamp(v,0.f,1.f)*255+.5f);
      }
      auto output=OIIO::ImageOutput::create((directory_/"albedo.png").string());
      if(!output || !output->open((directory_/"albedo.png").string(),OIIO::ImageSpec(w,h,3,OIIO::TypeDesc::UINT8)) ||
         !output->write_image(OIIO::TypeDesc::UINT8,albedo.data()) || !output->close()) error="写出 Diffuse Color 失败";
    }
  }
};
}
