// NASA LROC 颜色 + LOLA 高程。仅首次加载计算法线，日期和亮度变化复用同一纹理。
struct MoonSurface {dfv::ir::NightMap color,normal;};
std::shared_ptr<const MoonSurface> moon_surface() {
  static const auto cached=[] {
    auto result=std::make_shared<MoonSurface>();
    auto read=[](const char *name,int channels){
      auto input=OIIO::ImageInput::open(ccl::path_get(std::string("data/moon/")+name));
      if(!input)throw std::runtime_error(std::string("无法读取月表数据: ")+name);
      const auto spec=input->spec();if(spec.nchannels!=channels)throw std::runtime_error("月表数据通道数错误");
      dfv::ir::NightMap map{spec.width,spec.height,std::vector<float>(size_t(spec.width)*spec.height*channels)};
      if(!input->read_image(0,0,0,channels,OIIO::TypeDesc::FLOAT,map.pixels.data()))throw std::runtime_error("月表数据读取失败");input->close();return map;
    };
    const auto albedo=read("lroc_color_2k.jpg",3),height=read("ldem_4_uint.tif",1);
    auto &color=result->color;color={albedo.width,albedo.height,std::vector<float>(size_t(albedo.width)*albedo.height*4)};
    for(int y=0;y<color.height;++y)for(int x=0;x<color.width;++x){const auto dst=(size_t(color.height-1-y)*color.width+x)*4,src=(size_t(y)*color.width+x)*3;for(int k=0;k<3;++k)color.pixels[dst+k]=ccl::color_srgb_to_linear(albedo.pixels[src+k]);color.pixels[dst+3]=1;}
    auto &normal=result->normal;normal={height.width,height.height,std::vector<float>(size_t(height.width)*height.height*4)};
    auto elevation=[&](int x,int y){return height.pixels[size_t(std::clamp(y,0,height.height-1))*height.width+(x+height.width)%height.width]*32767.5f;};
    for(int y=0;y<height.height;++y)for(int x=0;x<height.width;++x){
      const float lon=float(2*M_PI*((x+.5)/height.width-.5)),lat=float(M_PI*(.5-(y+.5)/height.height));
      const float c=std::cos(lat),s=std::sin(lat),cl=std::cos(lon),sl=std::sin(lon);
      const auto radial=ccl::make_float3(c*cl,c*sl,s),east=ccl::make_float3(-sl,cl,0),north=ccl::make_float3(-s*cl,-s*sl,c);
      const float dx=(elevation(x+1,y)-elevation(x-1,y))/float(2*2*M_PI/height.width*1737400*std::max(.01f,c));
      const float dy=(elevation(x,y-1)-elevation(x,y+1))/float(2*M_PI/height.height*1737400);
      const auto n=ccl::normalize(radial-east*dx-north*dy);const auto dst=(size_t(height.height-1-y)*height.width+x)*4;
      normal.pixels[dst]=n.x;normal.pixels[dst+1]=n.y;normal.pixels[dst+2]=n.z;normal.pixels[dst+3]=1;
    }
    return result;
  }();return cached;
}
class MoonImageLoader final:public ccl::ImageLoader {
  std::shared_ptr<const MoonSurface> surface_=moon_surface();bool normal_;
  const dfv::ir::NightMap &map()const{return normal_?surface_->normal:surface_->color;}
public:
  explicit MoonImageLoader(bool normal):normal_(normal){}
  bool load_metadata(ccl::ImageMetaData &m,const ccl::ImageLoaderParams &,ccl::Progress &)override {m.width=map().width;m.height=map().height;m.channels=4;m.type=ccl::IMAGE_DATA_TYPE_FLOAT4;m.colorspace=ccl::u_colorspace_data;return true;}
  bool load_pixels(const ccl::ImageMetaData &m,void *pixels)override {std::memcpy(pixels,map().pixels.data(),map().pixels.size()*sizeof(float));m.conform_pixels(pixels);return true;}
  std::string name()const override{return normal_?"DFV LOLA lunar normals":"DFV LROC lunar albedo";}
  bool equals(const ccl::ImageLoader &other)const override {const auto *p=dynamic_cast<const MoonImageLoader *>(&other);return p&&p->normal_==normal_;}
};
