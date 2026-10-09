#include "render_ir/night_sky.h"
#include "scene/image.h"
#include "cycles/night_sky_model.h"
#include <cstring>
#include <mutex>

namespace {
class NightImageLoader final:public ccl::ImageLoader {
  std::shared_ptr<const dfv::ir::NightAssets> assets_;
  int channel_;
public:
  NightImageLoader(std::shared_ptr<const dfv::ir::NightAssets> assets,int channel):assets_(std::move(assets)),channel_(channel){}
  const dfv::ir::NightMap &map() const{return channel_==0?assets_->stars:assets_->stars_secondary;}
  bool load_metadata(ccl::ImageMetaData &m,const ccl::ImageLoaderParams &,ccl::Progress &) override {
    m.width=map().width;m.height=map().height;m.channels=4;m.type=ccl::IMAGE_DATA_TYPE_FLOAT4;m.colorspace=ccl::u_colorspace_data;return true;
  }
  bool load_pixels(const ccl::ImageMetaData &m,void *pixels) override {std::memcpy(pixels,map().pixels.data(),map().pixels.size()*sizeof(float));m.conform_pixels(pixels);return true;}
  std::string name() const override{return channel_==0?"DFV J2000 star lookup A":"DFV J2000 star lookup B";}
  bool equals(const ccl::ImageLoader &other) const override {const auto *p=dynamic_cast<const NightImageLoader *>(&other);return p&&p->assets_==assets_&&p->channel_==channel_;}
};
// 日常调参仅重建小型着色图；ImageManager 按相同 assets/通道复用 GPU 纹理。
struct NightGraph {
  ccl::ShaderGraph &g;
  using Out=ccl::ShaderOutput;
  Out *math(ccl::NodeMathType op,Out *x=nullptr,Out *y=nullptr,float a=0,float b=0){auto *n=g.create_node<ccl::MathNode>();n->set_math_type(op);n->set_value1(a);n->set_value2(b);if(x)g.connect(x,n->input("Value1"));if(y)g.connect(y,n->input("Value2"));return n->output("Value");}
  Out *vector(ccl::NodeVectorMathType op,Out *x,Out *y=nullptr,ccl::float3 value=ccl::zero_float3()){auto *n=g.create_node<ccl::VectorMathNode>();n->set_math_type(op);n->set_vector2(value);if(x)g.connect(x,n->input("Vector1"));if(y)g.connect(y,n->input("Vector2"));return n->output(op==ccl::NODE_VECTOR_MATH_DOT_PRODUCT?"Value":"Vector");}
  Out *scale(Out *x,Out *factor,float f=1){auto *n=g.create_node<ccl::VectorMathNode>();n->set_math_type(ccl::NODE_VECTOR_MATH_SCALE);n->set_scale(f);g.connect(x,n->input("Vector1"));if(factor)g.connect(factor,n->input("Scale"));return n->output("Vector");}
  Out *rgb(dfv::ir::Vec3 color,Out *factor=nullptr){auto *n=g.create_node<ccl::VectorMathNode>();n->set_math_type(ccl::NODE_VECTOR_MATH_SCALE);n->set_vector1(ccl::make_float3(color.x,color.y,color.z));n->set_scale(1);if(factor)g.connect(factor,n->input("Scale"));return n->output("Vector");}
  Out *clamp(Out *x,float low,float high){return math(ccl::NODE_MATH_MINIMUM,math(ccl::NODE_MATH_MAXIMUM,x,nullptr,0,low),nullptr,0,high);}
  Out *background(Out *color,Out *strength=nullptr,float gain=1){auto *n=g.create_node<ccl::BackgroundNode>();g.connect(color,n->input("Color"));n->set_strength(gain);if(strength)g.connect(strength,n->input("Strength"));return n->output("Background");}
};
#include "cycles/night_galaxy.inl"
ccl::ShaderOutput *night_background(ccl::Scene &scene,ccl::ShaderGraph &graph,const dfv::ir::OptionNode &n) {
  using namespace ccl;using namespace dfv;NightGraph g{graph};
  const auto astronomy=ir::night_astronomy(n);
  auto value=[&](const char *id,float fallback){return float(ir::number(n,id,fallback));};
  const bool catalog=value("DFV Night Star Distribution",0)==1;
  const float stars_intensity=std::max(0.f,value("DFV Night Stars Intensity",1)),galaxy_intensity=std::max(0.f,value("DFV Night Milky Way Intensity",1));
  const auto assets=catalog?ir::night_assets(int(value("DFV Night Quality",1))):nullptr;
  const auto environment_tint=ir::color(n,"Environment Tint");
  auto tint=[&](ShaderOutput *rgb){return g.vector(NODE_VECTOR_MATH_MULTIPLY,rgb,nullptr,make_float3(environment_tint.x,environment_tint.y,environment_tint.z));};
  auto *coord=graph.create_node<TextureCoordinateNode>();auto *world=coord->output("Generated");
  auto *split=graph.create_node<SeparateXYZNode>();graph.connect(world,split->input("Vector"));auto *alt=split->output("Z");
  // 把世界方向变换到 J2000；仅更新三条基向量，日期变化不重新生成星图。
  const auto ex=astronomy.equatorial_to_world({1,0,0}),ey=astronomy.equatorial_to_world({0,1,0}),ez=astronomy.equatorial_to_world({0,0,1});
  auto *combine=graph.create_node<CombineXYZNode>();
  const std::array<ir::Vec3,3> basis={ex,ey,ez};const char *components[]={"X","Y","Z"};
  for(int i=0;i<3;++i)graph.connect(g.vector(NODE_VECTOR_MATH_DOT_PRODUCT,world,nullptr,make_float3(basis[i].x,basis[i].y,basis[i].z)),combine->input(components[i]));
  auto *rotate=graph.create_node<VectorRotateNode>();rotate->set_axis(make_float3(0,0,1));rotate->set_angle(-value("DFV Night Rotation",0)*float(M_PI/180));graph.connect(combine->output("Vector"),rotate->input("Vector"));auto *eq=rotate->output("Vector");
  auto texture=[&](int channel){auto *t=graph.create_node<EnvironmentTextureNode>();t->set_colorspace(u_colorspace_data);t->set_alpha_type(IMAGE_ALPHA_CHANNEL_PACKED);t->set_interpolation(INTERPOLATION_CLOSEST);t->handle=scene.image_manager->add_image(make_unique<NightImageLoader>(assets,channel),t->image_params());graph.connect(eq,t->input("Vector"));return t;};
  // 表内保存星位而非栅格化星斑。两次最近邻查询后按角距离画紧凑星核/微弱光翼，
  // 视角放大不暴露贴图像素；始终不遍历整个星表，也不创建逐星光源。
  const float limit=value("DFV Night Limiting Magnitude",6.5f);
  auto star=[&](int channel){
    auto *record=texture(channel);auto *packed=record->output("Alpha");
    auto *mag=g.math(NODE_MATH_SUBTRACT,g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_FLOOR,packed),nullptr,0,.01f),nullptr,0,2);
    auto *bv=g.math(NODE_MATH_SUBTRACT,g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_FRACTION,packed),nullptr,0,float(2.4/.99)),nullptr,0,.4f);
    auto *visibility=g.clamp(g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_SUBTRACT,nullptr,mag,limit+.15f),nullptr,0,2),0,1);
    visibility=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_MULTIPLY,visibility,visibility),g.math(NODE_MATH_SUBTRACT,nullptr,g.math(NODE_MATH_MULTIPLY,visibility,nullptr,0,2),3));
    visibility=g.math(NODE_MATH_MULTIPLY,visibility,g.math(NODE_MATH_GREATER_THAN,packed,nullptr,0,-.5f));
    auto *delta=g.vector(NODE_VECTOR_MATH_SUBTRACT,eq,record->output("Color"));
    auto *r2=g.vector(NODE_VECTOR_MATH_DOT_PRODUCT,delta,delta);
    auto *sigma=g.math(NODE_MATH_ADD,g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_POWER,nullptr,g.math(NODE_MATH_MULTIPLY,mag,nullptr,0,-.15f),10),nullptr,0,ir::night_star_sigma_gain),nullptr,0,ir::night_star_sigma_base);
    auto *sigma2=g.math(NODE_MATH_MULTIPLY,sigma,sigma);
    auto *exponent=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_DIVIDE,r2,sigma2),nullptr,0,-.5f);
    auto *core=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_EXPONENT,exponent),nullptr,0,.9f);
    auto *wing=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_EXPONENT,g.math(NODE_MATH_MULTIPLY,exponent,nullptr,0,1.f/9)),nullptr,0,.1f/9);
    auto *profile=g.math(NODE_MATH_DIVIDE,g.math(NODE_MATH_ADD,core,wing),sigma2);
    auto *flux=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_POWER,nullptr,g.math(NODE_MATH_MULTIPLY,mag,nullptr,0,-.4f),10),nullptr,0,float(.00012/(2*M_PI)));
    auto *strength=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_MULTIPLY,profile,flux),visibility);
    auto *bv_scaled=g.math(NODE_MATH_MULTIPLY,bv,nullptr,0,.92f);
    auto *temp=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_ADD,g.math(NODE_MATH_DIVIDE,nullptr,g.math(NODE_MATH_ADD,bv_scaled,nullptr,0,1.7f),1),g.math(NODE_MATH_DIVIDE,nullptr,g.math(NODE_MATH_ADD,bv_scaled,nullptr,0,.62f),1)),nullptr,0,4600);
    auto *cool=g.clamp(g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_SUBTRACT,temp,nullptr,0,2500),nullptr,0,1.f/4000),0,1);
    auto *hot=g.clamp(g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_SUBTRACT,temp,nullptr,0,6500),nullptr,0,1.f/20000),0,1);
    auto *color=g.vector(NODE_VECTOR_MATH_ADD,g.vector(NODE_VECTOR_MATH_ADD,g.rgb({1,.32f,.08f}),g.rgb({0,.68f,.92f},cool)),g.rgb({-.48f,-.28f,0},hot));
    return g.scale(color,strength);
  };
  auto *web=graph.create_node<DfvNightSkyNode>();web->set_quality(int(value("DFV Night Quality",1)));web->set_contrast(value("DFV Night Milky Way Detail",1));web->set_density(catalog?0:value("DFV Night Star Density",1));graph.connect(eq,web->input("Vector"));
  auto *star_rgb=g.scale(catalog?g.vector(NODE_VECTOR_MATH_ADD,star(0),star(1)):web->output("Stars"),nullptr,stars_intensity);
  auto *galaxy_rgb=g.scale(web->output("Galaxy"),nullptr,galaxy_intensity);
  auto *full=g.vector(NODE_VECTOR_MATH_ADD,star_rgb,galaxy_rgb);
  // 地平线与大气消光；避免把地平线以下的星空投射到地面。
  auto *horizon=g.clamp(g.math(NODE_MATH_MULTIPLY,alt,nullptr,0,35),0,1);
  const float haze=std::max(0.f,value("SS Haze",0));
  auto *airmass=g.math(NODE_MATH_DIVIDE,nullptr,g.math(NODE_MATH_MAXIMUM,alt,nullptr,0,.04f),-.08f*(1+.2f*haze));
  auto *extinction=g.math(NODE_MATH_EXPONENT,airmass);
  auto *gain=g.math(NODE_MATH_MULTIPLY,horizon,extinction);
  gain=g.math(NODE_MATH_MULTIPLY,gain,nullptr,0,float(astronomy.darkness)*std::max(0.f,value("Environment Intensity",1)));
  auto *sky=g.rgb({.0011f,.0017f,.0034f});sky=g.scale(sky,nullptr,std::max(0.f,value("DFV Night Sky Intensity",1)));
  auto *camera=g.background(tint(g.vector(NODE_VECTOR_MATH_ADD,sky,full)),gain);
  const int lighting=int(value("DFV Night Lighting",1));
  auto *illumination=full;
  if(lighting!=2){
    const auto sh=procedural_night_sh(int(value("DFV Night Quality",1)),value("DFV Night Milky Way Detail",1),catalog?0:value("DFV Night Star Density",1));
    auto coefficients=catalog?ir::night_star_sh(limit):sh.stars;
    const auto &galaxy_sh=sh.galaxy;
    for(int i=0;i<9;++i){auto &c=coefficients[i];const auto m=galaxy_sh[i];c={c.x*stars_intensity+m.x*galaxy_intensity,c.y*stars_intensity+m.y*galaxy_intensity,c.z*stars_intensity+m.z*galaxy_intensity};}
    auto *separate=graph.create_node<SeparateXYZNode>();graph.connect(eq,separate->input("Vector"));auto *x=separate->output("X"),*y=separate->output("Y"),*z=separate->output("Z");
    const auto c0=coefficients[0];illumination=g.rgb({c0.x*.2820947918f,c0.y*.2820947918f,c0.z*.2820947918f});
    if(lighting==1){
      std::array<ShaderOutput *,9> sh={nullptr,y,z,x,g.math(NODE_MATH_MULTIPLY,x,y),g.math(NODE_MATH_MULTIPLY,y,z),g.math(NODE_MATH_SUBTRACT,g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_MULTIPLY,z,z),nullptr,0,3),nullptr,0,1),g.math(NODE_MATH_MULTIPLY,x,z),g.math(NODE_MATH_SUBTRACT,g.math(NODE_MATH_MULTIPLY,x,x),g.math(NODE_MATH_MULTIPLY,y,y))};
      constexpr float scales[]={0,.4886025119f,.4886025119f,.4886025119f,1.0925484306f,1.0925484306f,.3153915653f,1.0925484306f,.5462742153f};
      for(int i=1;i<9;++i){const auto c=coefficients[i];illumination=g.vector(NODE_VECTOR_MATH_ADD,illumination,g.rgb({c.x*scales[i],c.y*scales[i],c.z*scales[i]},sh[i]));}
      illumination=g.vector(NODE_VECTOR_MATH_MAXIMUM,illumination);
    }
  }
  auto *lit=g.background(tint(g.scale(g.vector(NODE_VECTOR_MATH_ADD,sky,illumination),nullptr,std::max(0.f,value("DFV Night Lighting Intensity",1)))),gain);
  auto *path=graph.create_node<LightPathNode>();auto *mix=graph.create_node<MixClosureNode>();graph.connect(path->output("Is Camera Ray"),mix->input("Fac"));graph.connect(lit,mix->input("Closure1"));graph.connect(camera,mix->input("Closure2"));
  // 月盘用解析球面法线和太阳方向产生正确朝向的月相；不受星空补光档位影响。
  const auto moon=astronomy.moon,sun=astronomy.sun;const float moon_scale=std::max(0.f,value("DFV Night Moon Scale",1));const float radius=std::clamp(float(astronomy.moon_radius)*moon_scale,1e-6f,float(M_PI_2)-1e-5f);
  auto *dot=g.vector(NODE_VECTOR_MATH_DOT_PRODUCT,world,nullptr,make_float3(moon.x,moon.y,moon.z));
  auto *disc=g.math(NODE_MATH_GREATER_THAN,dot,nullptr,0,std::cos(radius));
  auto *tangent=g.scale(g.vector(NODE_VECTOR_MATH_SUBTRACT,world,g.rgb(moon,dot)),nullptr,1/std::sin(radius));
  auto *r2=g.vector(NODE_VECTOR_MATH_DOT_PRODUCT,tangent,tangent);
  auto *depth=g.math(NODE_MATH_SQRT,g.math(NODE_MATH_MAXIMUM,g.math(NODE_MATH_SUBTRACT,nullptr,r2,1),nullptr,0,0));
  auto *normal=g.vector(NODE_VECTOR_MATH_SUBTRACT,tangent,g.rgb(moon,depth));
  auto *phase=g.math(NODE_MATH_MAXIMUM,g.vector(NODE_VECTOR_MATH_DOT_PRODUCT,normal,nullptr,make_float3(sun.x,sun.y,sun.z)),nullptr,0,0);
  auto *moon_strength=g.math(NODE_MATH_MULTIPLY,g.math(NODE_MATH_MULTIPLY,phase,disc),g.math(NODE_MATH_MULTIPLY,horizon,extinction));
  float energy=moon_scale>0?80*std::max(0.f,value("DFV Night Moon Intensity",1))*std::max(0.f,value("Environment Intensity",1)):0;
  if(value("DFV Night Moon Physical",1)!=0)energy/=std::max(1e-12f,moon_scale*moon_scale);
  moon_strength=g.math(NODE_MATH_MULTIPLY,moon_strength,nullptr,0,energy);
  auto *lunar=g.background(tint(g.rgb(ir::color(n,"DFV Night Moon Color",{1,.94f,.82f}))),moon_strength);
  auto *sum=graph.create_node<MixClosureNode>();graph.connect(mix->output("Closure"),sum->input("Closure1"));graph.connect(lunar,sum->input("Closure2"));graph.connect(disc,sum->input("Fac"));return sum->output("Closure");
}
}
