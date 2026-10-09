#include "render_ir/night_sky.h"
#include <chrono>
#include <mutex>
#include <numbers>

namespace dfv::ir {
void ensure_night_options(OptionNode &node) {
  if(node.id.empty())return;
  auto add=[&](const char *id,const char *label,const char *type,std::vector<double> value,double low,double high,double step=.1)->Option & {
    auto it=std::find_if(node.parameters.begin(),node.parameters.end(),[&](const auto &p){return p.id==id;});
    if(it==node.parameters.end()){Option p;p.id=id;p.value=std::move(value);node.parameters.push_back(std::move(p));it=std::prev(node.parameters.end());}
    auto &p=*it;p.label=label;p.group="/Environment/夜景";p.type=type;p.minimum=low;p.maximum=high;p.step=step;p.visible=true;p.supported=p.value.size()==(p.type=="float_color"?3:1);p.clamped=false;p.authored=false;
    // min/max 仅提供初始滑轨尺度；范围限制统一来自 ParameterSettings。
    // 导入和刷新也不能再次把用户已保存的数值截回建议范围。
    for(auto &v:p.value){if(!std::isfinite(v))v=low;if(p.type=="bool"||p.type=="enum")v=std::clamp(std::round(v),low,high);}return p;
  };
  add("DFV Night Enabled","是否启用夜景","bool",{0},0,1,1);
  add("DFV Night Sky Intensity","夜空底色强度","float",{1},0,100);
  add("DFV Night Stars Intensity","星光强度","float",{1},0,100);
  add("DFV Night Limiting Magnitude","可见极限星等（越大星越多）","float",{6.5},-1.5,7);
  add("DFV Night Milky Way Intensity","银河强度","float",{1},0,100);
  add("DFV Night Milky Way Detail","银河细节对比度","float",{1},.25,3);
  add("DFV Night Rotation","星空附加旋转（°）","float",{0},-360,360,1);
  add("DFV Night Moon Intensity","月亮强度","float",{1},0,100);
  add("DFV Night Moon Scale","月亮大小","float",{1},.1,20);
  add("DFV Night Moon Physical","月亮大小保持总光量","bool",{1},0,1,1);
  add("DFV Night Moon Color","月光颜色","float_color",{1,.94,.82},0,100);
  add("DFV Night Lighting","星空补光方式","enum",{1},0,2,1).choices={"均色补光（最快）","SH 补光（平滑方向）","完整环境采样（保留星点反射）"};
  add("DFV Night Lighting Intensity","星空补光强度","float",{1},0,100);
  add("DFV Night Quality","夜空精细度","enum",{1},0,2,1).choices={"低（快速预览）","标准（网页效果）","高（更多细节）"};
  add("DFV Night Star Distribution","星空分布","enum",{0},0,1,1).choices={"网页式程序化星空","真实天文星表"};
  add("DFV Night Star Density","程序化星空密度","float",{1},0,2);
  // 原生 HDRI 预设可能省略或隐藏 Sun-Sky 的时间通道。共享原通道，避免用户
  // 启用夜景后找不到日期；已创作的数值、范围、精度及分组保持不变。
  auto time=[&](const char *id,const char *label,double initial,double low,double high,double step){
    for(auto &p:node.parameters)if(p.id==id){p.visible=true;return;}
    auto &p=add(id,label,"float",{initial},low,high,step);p.group="/Environment/日期与位置";
  };
  time("SS Day","日期（儒略日）",2457092,2000000,3000000,1);time("SS Time","当地时间（秒）",43200,0,86400,60);
  time("SS UTC Offset","UTC 时差",0,-14,14,1);time("SS Latitude","纬度",0,-90,90,1);time("SS Longitude","经度",0,-180,180,1);
}
namespace {
constexpr double pi=std::numbers::pi;
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,double f){return {float(a.x*f),float(a.y*f),float(a.z*f)};}
float smooth(float a,float b,float x){x=std::clamp((x-a)/(b-a),0.f,1.f);return x*x*(3-2*x);}
struct Star {float ra,dec,mag,bv;};
constexpr Star catalog[]={
#include "hyg/bright_stars.inl"
};
Vec3 star_direction(const Star &s){const double ra=s.ra*pi/12,dec=s.dec*pi/180;return {float(std::cos(dec)*std::cos(ra)),float(std::cos(dec)*std::sin(ra)),float(std::sin(dec))};}
Vec3 star_color(double color_index){
  // B-V → 色温 → 平滑暖白/蓝白近似，使用线性 RGB。
  const double bv=std::clamp(color_index,-.4,2.),temp=4600*(1/(.92*bv+1.7)+1/(.92*bv+.62));
  if(temp<6500){const float t=float(std::clamp((temp-2500)/4000,0.,1.));return {1,.32f+.68f*t,.08f+.92f*t};}
  const float t=float(std::clamp((temp-6500)/20000,0.,1.));return {1-.48f*t,1-.28f*t,1};
}
double star_flux(const Star &s){return .00012*std::pow(10.,-.4*s.mag);}
void put(NightMap &map,int x,int y,Vec3 p,float a=1){const auto i=(size_t(y)*map.width+x)*4;map.pixels[i]=p.x;map.pixels[i+1]=p.y;map.pixels[i+2]=p.z;map.pixels[i+3]=a;}
}
Vec3 night_map_direction(double u,double v){const double phi=pi-2*pi*u,theta=pi-pi*v;return {float(std::sin(theta)*std::cos(phi)),float(std::sin(theta)*std::sin(phi)),float(std::cos(theta))};}
std::array<double,9> night_sh_basis(Vec3 p){return {.2820947918,.4886025119*p.y,.4886025119*p.z,.4886025119*p.x,1.0925484306*p.x*p.y,1.0925484306*p.y*p.z,.3153915653*(3*p.z*p.z-1),1.0925484306*p.x*p.z,.5462742153*(p.x*p.x-p.y*p.y)};}
std::array<Vec3,9> night_star_sh(double limit){
  std::array<Vec3,9> out{};for(const auto &s:catalog){const double visibility=1-smooth(float(limit-.35),float(limit+.15),s.mag);if(visibility<=0)continue;const auto basis=night_sh_basis(star_direction(s));const auto rgb=mul(star_color(s.bv),star_flux(s)*visibility);for(int k=0;k<9;++k)out[k]=add(out[k],mul(rgb,basis[k]));}return out;
}
size_t night_star_count(double limit){return std::count_if(std::begin(catalog),std::end(catalog),[&](const auto &s){return s.mag<=limit;});}
Vec3 sample_night_stars(const NightAssets &assets,Vec3 d,double limit){
  const int w=assets.stars.width,h=assets.stars.height;
  const int x=((int(std::floor((.5-std::atan2(d.y,d.x)/(2*pi))*w))%w)+w)%w;
  const int y=std::clamp(int(std::floor((.5+std::asin(std::clamp(double(d.z),-1.,1.))/pi)*h)),0,h-1);
  const size_t i=(size_t(y)*w+x)*4;Vec3 result{};
  for(const auto *map:{&assets.stars,&assets.stars_secondary}){
    const auto &p=map->pixels;const float packed=p[i+3];if(packed<0)continue;
    const float whole=std::floor(packed),mag=whole*.01f-2,bv=(packed-whole)*float(2.4/.99)-.4f;
    const double sigma=night_star_sigma_base+night_star_sigma_gain*std::pow(10.,-.15*mag);
    const double r2=std::pow(d.x-p[i],2)+std::pow(d.y-p[i+1],2)+std::pow(d.z-p[i+2],2);
    const double profile=(.9*std::exp(-.5*r2/(sigma*sigma))+.1/9*std::exp(-.5*r2/(9*sigma*sigma)))/(2*pi*sigma*sigma);
    const double flux=.00012*std::pow(10.,-.4*mag)*(1-smooth(float(limit-.35),float(limit+.15),mag));
    result=add(result,mul(star_color(bv),profile*flux));
  }
  return result;
}
std::shared_ptr<const NightAssets> night_assets(int quality){
  static std::mutex mutex;static std::array<std::shared_ptr<const NightAssets>,3> cache;
  std::lock_guard lock(mutex);quality=std::clamp(quality,0,2);if(cache[quality])return cache[quality];
  const auto start=std::chrono::steady_clock::now();auto result=std::make_shared<NightAssets>();
  const int w=1024<<quality,h=w/2;
  // 查询表保存最近的两颗恒星。每条光线恒定查询两次，再按真实角距离绘制星核，
  // 避免 2K 辐亮度贴图的模糊光斑；近邻双星也不会被合并为同一个星等门限。
  const size_t count=size_t(w)*h;
  std::vector<double> longitude_cos(w),longitude_sin(w),latitude_cos(h),latitude_sin(h);
  for(int x=0;x<w;++x){const double phi=pi-2*pi*(x+.5)/w;longitude_cos[x]=std::cos(phi);longitude_sin[x]=std::sin(phi);}
  for(int y=0;y<h;++y){const double theta=pi-pi*(y+.5)/h;latitude_cos[y]=std::cos(theta);latitude_sin[y]=std::sin(theta);}
  std::array<std::vector<float>,2> distances={std::vector<float>(count,1e10f),std::vector<float>(count,1e10f)};
  std::array<std::vector<int>,2> ids={std::vector<int>(count,-1),std::vector<int>(count,-1)};
  for(size_t id=0;id<std::size(catalog);++id){const auto &s=catalog[id];const auto dir=star_direction(s);
    const double px=(.5-s.ra/24.)*w-.5,py=(.5+s.dec/180.)*h-.5;
    const double radius=.015+2*pi/w,coslat=std::max(.002,std::cos(s.dec*pi/180));
    const int rx=std::min(w/2,int(std::ceil(radius*w/(2*pi*coslat)))+1),ry=int(std::ceil(radius*h/pi))+1;
    const int ix=int(std::floor(px)),iy=int(std::floor(py));
    for(int y=std::max(0,iy-ry);y<=std::min(h-1,iy+ry+1);++y)for(int x=ix-rx;x<ix+rx+1;++x){
      const int xx=(x%w+w)%w;const size_t p=size_t(y)*w+xx;const Vec3 v{float(latitude_sin[y]*longitude_cos[xx]),float(latitude_sin[y]*longitude_sin[xx]),float(latitude_cos[y])};
      const float distance=(v.x-dir.x)*(v.x-dir.x)+(v.y-dir.y)*(v.y-dir.y)+(v.z-dir.z)*(v.z-dir.z);
      if(distance<distances[0][p]){distances[1][p]=distances[0][p];ids[1][p]=ids[0][p];distances[0][p]=distance;ids[0][p]=int(id);}
      else if(distance<distances[1][p]&&ids[0][p]!=int(id)){distances[1][p]=distance;ids[1][p]=int(id);}
    }
  }
  for(int layer=0;layer<2;++layer){auto &map=layer?result->stars_secondary:result->stars;map={w,h,std::vector<float>(count*4)};
    for(size_t p=0;p<count;++p){const int id=ids[layer][p];if(id<0){map.pixels[p*4+3]=-1;continue;}const auto &s=catalog[id];const auto dir=star_direction(s);put(map,int(p%w),int(p/w),dir,float(std::round((s.mag+2)*100)+.99*(std::clamp(double(s.bv),-.4,2.)+.4)/2.4));}
  }
  for(size_t id=0;id<std::size(catalog);++id){const auto &s=catalog[id];const int x=((int(std::floor((.5-s.ra/24)*w))%w)+w)%w,y=std::clamp(int(std::floor((.5+s.dec/180)*h)),0,h-1);const size_t p=size_t(y)*w+x;if(ids[0][p]==int(id)||ids[1][p]==int(id))++result->resolved_stars;}
  result->prepare_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();cache[quality]=result;return result;
}
}
