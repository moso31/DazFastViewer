#pragma once
#include "editor/geograft_materials.h"
#include <QImageReader>
#include <QPointF>
#include <fstream>
#include <cmath>

namespace dfv::editor {
// 附加数据贴图使用相同的产品 UV 对应关系，只重采样、不叠加颜色模板。
inline QImage reproject_hd_map(const QImage &input,const std::vector<QPointF> &source,const std::vector<QPointF> &destination,const std::function<bool()> &proceed={}){
  if(source.size()!=destination.size()||source.size()%4||source.empty())throw std::runtime_error("HD UV 对应关系无效");
  auto pixels=input.convertToFormat(QImage::Format_ARGB32);QImage out(4096,4096,QImage::Format_ARGB32);out.fill(pixels.pixel(0,0));
  auto cross=[](QPointF a,QPointF b){return a.x()*b.y()-a.y()*b.x();};
  auto sample=[&](QPointF uv){const double x=std::clamp(uv.x()/4096*pixels.width()-.5,0.,double(pixels.width()-1)),y=std::clamp(uv.y()/4096*pixels.height()-.5,0.,double(pixels.height()-1));const int ix=int(x),iy=int(y),jx=std::min(ix+1,pixels.width()-1),jy=std::min(iy+1,pixels.height()-1);const QRgb a=pixels.pixel(ix,iy),b=pixels.pixel(jx,iy),c=pixels.pixel(ix,jy),d=pixels.pixel(jx,jy);const double fx=x-ix,fy=y-iy;auto channel=[&](int shift){auto value=[&](QRgb p){return double((p>>shift)&255);};return int(std::lround((value(a)*(1-fx)+value(b)*fx)*(1-fy)+(value(c)*(1-fx)+value(d)*fx)*fy));};return qRgba(channel(16),channel(8),channel(0),channel(24));};
  for(size_t face=0;face<source.size();face+=4){if(proceed&&!proceed())return {};for(int part=0;part<2;++part){const std::array<size_t,3> indexes={face,face+size_t(part+1),face+size_t(part+2)};std::array<QPointF,3> a,b;for(size_t n=0;n<3;++n){a[n]=destination[indexes[n]];b[n]=source[indexes[n]];}const auto ab=a[1]-a[0],ac=a[2]-a[0];const double det=cross(ab,ac);if(std::abs(det)<1e-8)continue;
      const int left=std::max(0,int(std::floor(std::min({a[0].x(),a[1].x(),a[2].x()})))),right=std::min(4095,int(std::ceil(std::max({a[0].x(),a[1].x(),a[2].x()}))));const int top=std::max(0,int(std::floor(std::min({a[0].y(),a[1].y(),a[2].y()})))),bottom=std::min(4095,int(std::ceil(std::max({a[0].y(),a[1].y(),a[2].y()}))));
      for(int y=top;y<=bottom;++y){auto *line=reinterpret_cast<QRgb *>(out.scanLine(y));for(int x=left;x<=right;++x){const QPointF p=QPointF(x+.5,y+.5)-a[0];const double u=cross(p,ac)/det,v=cross(ab,p)/det;if(u>=-1e-7&&v>=-1e-7&&u+v<=1+1e-7)line[x]=sample(b[0]+(b[1]-b[0])*u+(b[2]-b[0])*v);}}
    }}return out;
}
inline bool write_hd_auxiliary_maps(const HDBakeJob &job,const std::function<bool()> &proceed={}){
  auto read=[&](const std::string &name){std::ifstream in(job.directory/(name+".DAT"));if(!in)throw std::runtime_error("缺少 HD UV 数据");std::vector<QPointF> points;std::string line;while(std::getline(in,line)){const auto comma=line.find(',');if(comma==std::string::npos)throw std::runtime_error("HD UV 数据无效");points.push_back({std::stod(line.substr(0,comma)),4096-std::stod(line.substr(comma+1))});}return points;};
  const std::array<std::vector<QPointF>,2> source={read("7a"),read("7b")},destination={read("6a"),read("6b")};
  for(const auto &p:material_parameters())if(p.kind==MaterialParameter::texture&&!hd_primary_channel(p.id)){const auto &value=job.identity.at("body").at(p.id);if(value.is_null())continue;auto path=std::filesystem::u8path(value.at("file").get<std::string>());QImageReader reader(QString::fromStdWString(path.wstring()));const auto input=reader.read();if(input.isNull())throw std::runtime_error("无法读取附加皮肤贴图："+p.label);
    for(size_t side=0;side<2;++side){auto out=reproject_hd_map(input,source[side],destination[side],proceed);if(out.isNull())return false;auto file=job.directory/("Channel_"+p.id+"_"+(side?"R":"L")+".png");if(!out.save(QString::fromStdWString(file.wstring()),"PNG"))throw std::runtime_error("无法写入附加 HD 贴图："+p.label);}
  }return true;
}
}
