#include "water/model.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace dfv::water {
namespace {
constexpr double pi=3.14159265358979323846;
uint64_t mix(uint64_t x){x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
double random(uint64_t x){return double(mix(x)>>11)*0x1.0p-53;}
}
void validate(const Config &c){
  auto range=[](double v,double a,double b){return std::isfinite(v)&&v>=a&&v<=b;};
  if(!range(c.width,1,1000000)||!range(c.length,1,1000000)||!range(c.x,-1000000,1000000)||!range(c.y,-1000000,1000000)||!range(c.level,-1000000,1000000)||
     !range(c.depth,.01,100000)||!range(c.clarity,.01,100000)||!range(c.wave_height,0,1000)||!range(c.wavelength,.1,100000)||!range(c.steepness,0,.9)||
     !range(c.direction,-36000,36000)||!range(c.time,-10000000,10000000)||!range(c.ripples,0,2)||!range(c.roughness,.005,1)||
     !range(c.foam_width,.01,1000)||!range(c.foam_strength,0,1)||!range(c.precision,.02,1000)||!range(c.density,.25,8)||!range(c.foam_uv_scale,.01,100)||
     !range(c.color.x,0,1)||!range(c.color.y,0,1)||!range(c.color.z,0,1))throw std::runtime_error("水体参数超出有效范围");
  std::set<std::string> ids;for(const auto &s:c.sources)if(s.id.empty()||!ids.insert(s.id).second)throw std::runtime_error("海岸线对象身份无效或重复");
  if(c.lod_level<0||c.lod_level>26)throw std::runtime_error("水体 LOD 等级无效");
}
LodLayout lod_layout(const Config &c){
  validate(c);const double spacing=std::max(c.precision,c.wavelength/8.);
  const double exponent=std::log2(std::max(c.width,c.length)/spacing);
  const int base=std::clamp(int(std::ceil(exponent)),0,26);
  auto vertices=[](int level){const uint64_t n=uint64_t(1)<<level;return n*n+(n+1)*(n+1);};
  int finest=std::clamp(int(std::ceil(exponent+std::log2(c.density)/2)),0,26);
  while(c.density>1&&finest>0&&double(vertices(finest))>double(vertices(base))*c.density)--finest;
  LodLayout layout;layout.maximum_level=finest;layout.depth=std::max(0,finest-c.lod_level);layout.side=uint64_t(1)<<layout.depth;
  layout.vertices=vertices(layout.depth);layout.triangles=4*layout.side*layout.side;layout.bytes=layout.vertices*(sizeof(ir::Vec3)+sizeof(float))+layout.triangles*sizeof(ir::Triangle);return layout;
}
Wave wave(const Config &c,double x,double y){
  Wave out{x,y,0,0};double compression=0;
  // Fixed spectrum: random access in time has no simulation history or RNG state.
  for(int i=0;i<6;++i){
    const double r=random(uint64_t(c.seed)*17+i*7),angle=(c.direction+(r-.5)*72)*pi/180;
    const double length=c.wavelength*std::pow(.61,i)*( .85+.3*random(c.seed+i*23+9));
    const double k=2*pi/length,a=c.wave_height*.24*std::pow(.55,i),dx=std::cos(angle),dy=std::sin(angle);
    const double phase=std::remainder(k*(dx*x+dy*y)-std::sqrt(9.81*k)*c.time+random(c.seed+i*31+11)*2*pi,2*pi);
    const double q=c.steepness/6,displacement=std::min(a,q/k);
    out.x+=dx*displacement*std::cos(phase);out.y+=dy*displacement*std::cos(phase);out.z+=a*std::sin(phase);
    compression+=k*displacement*std::sin(phase);
  }
  out.foam=std::clamp((compression-.23)*3,0.,1.);return out;
}
bool same_shape(const Config &a,const Config &b){return a.width==b.width&&a.length==b.length&&a.x==b.x&&a.y==b.y&&a.level==b.level&&a.wave_height==b.wave_height&&a.wavelength==b.wavelength&&a.steepness==b.steepness&&a.direction==b.direction&&a.time==b.time&&a.seed==b.seed&&a.precision==b.precision&&a.foam_width==b.foam_width&&a.sources==b.sources&&a.scan_scene==b.scan_scene&&a.coast==b.coast;}
nlohmann::json json(const Water &w){
  const auto &c=w.config;validate(c);using J=nlohmann::json;
  J j={{"version",1},{"id",w.id},{"width",c.width},{"length",c.length},{"x",c.x},{"y",c.y},{"level",c.level},
    {"depth",c.depth},{"clarity",c.clarity},{"wave_height",c.wave_height},{"wavelength",c.wavelength},{"steepness",c.steepness},{"direction",c.direction},
    {"time",c.time},{"ripples",c.ripples},{"roughness",c.roughness},{"foam_width",c.foam_width},{"foam_strength",c.foam_strength},{"precision",c.precision},
    {"density",c.density},{"foam_uv_scale",c.foam_uv_scale},{"color",{c.color.x,c.color.y,c.color.z}},{"seed",c.seed},{"coast",c.coast},{"scan_scene",c.scan_scene},{"sources",J::array()}};
  j["manual_lod"]=c.manual_lod;j["lod_level"]=c.lod_level;
  for(const auto &s:c.sources)j["sources"].push_back({{"id",s.id},{"volume",s.volume}});
  if(w.cache){auto &cache=j["cache"];cache={{"stamp",w.cache->stamp},{"objects",w.cache->objects},{"sampling_spacing",w.cache->sampling_spacing},{"warnings",w.cache->warnings},{"cells",J::array()}};for(const auto &v:w.cache->cells)cache["cells"].push_back({v.x,v.y,v.level,v.clearance});}
  return j;
}
std::shared_ptr<const Water> from_json(const nlohmann::json &j){
  if(j.at("version")!=1)throw std::runtime_error("不支持的水体版本");auto w=std::make_shared<Water>();w->id=j.at("id");auto &c=w->config;
  if(w->id.empty())throw std::runtime_error("水体身份为空");
#define DFV_WATER_READ(n) c.n=j.at(#n).get<decltype(c.n)>()
  DFV_WATER_READ(width);DFV_WATER_READ(length);DFV_WATER_READ(x);DFV_WATER_READ(y);DFV_WATER_READ(level);DFV_WATER_READ(depth);DFV_WATER_READ(clarity);
  DFV_WATER_READ(wave_height);DFV_WATER_READ(wavelength);DFV_WATER_READ(steepness);DFV_WATER_READ(direction);DFV_WATER_READ(time);DFV_WATER_READ(ripples);DFV_WATER_READ(roughness);
  DFV_WATER_READ(foam_width);DFV_WATER_READ(foam_strength);DFV_WATER_READ(precision);DFV_WATER_READ(seed);DFV_WATER_READ(coast);DFV_WATER_READ(scan_scene);
#undef DFV_WATER_READ
  c.density=j.value("density",1.);c.foam_uv_scale=j.value("foam_uv_scale",1.);
  c.manual_lod=j.value("manual_lod",false);c.lod_level=j.value("lod_level",0);
  auto rgb=j.at("color").get<std::array<float,3>>();c.color={rgb[0],rgb[1],rgb[2]};for(const auto &s:j.at("sources"))c.sources.push_back({s.at("id"),s.at("volume")});validate(c);
  if(j.contains("cache")){const auto &v=j.at("cache");auto cache=std::make_shared<Cache>();cache->stamp=v.at("stamp");cache->objects=v.at("objects");cache->warnings=v.at("warnings").get<std::vector<std::string>>();
    cache->sampling_spacing=v.value("sampling_spacing",c.precision);if(!std::isfinite(cache->sampling_spacing)||cache->sampling_spacing<0||cache->sampling_spacing>1000000)throw std::runtime_error("海岸线采样间距无效");
    if(v.at("cells").size()>150000)throw std::runtime_error("海岸线缓存超出预算");std::set<std::tuple<int,uint32_t,uint32_t>> keys;
    for(const auto &a:v.at("cells")){Cell cell;cell.x=a.at(0);cell.y=a.at(1);cell.level=a.at(2);cell.clearance=a.at(3).get<std::array<double,5>>();if(cell.level<0||cell.level>26||cell.x>=(1u<<cell.level)||cell.y>=(1u<<cell.level)||!keys.emplace(cell.level,cell.x,cell.y).second)throw std::runtime_error("海岸线网格身份无效");for(double f:cell.clearance)if(!std::isfinite(f))throw std::runtime_error("海岸线包含无效数据");cache->cells.push_back(cell);}w->cache=std::move(cache);
  }return w;
}
std::shared_ptr<const Water> prefixed(const Water &w,const std::string &prefix){auto copy=std::make_shared<Water>(w);copy->id=prefix+w.id;for(auto &s:copy->config.sources)s.id=prefix+s.id;return copy;}
const Water *find(const Waters &waters,const std::string &id){for(const auto &w:waters)if(w->id==id||w->id+"/surface"==id)return w.get();return nullptr;}
ir::Material material(const Water &w){
  ir::Material m;m.id=w.id+"/material";m.base_color=w.config.color;m.roughness=float(w.config.roughness);m.ior=1.333f;m.transmission=1;
  m.water=ir::WaterSurface{float(w.config.depth),float(w.config.clarity),float(w.config.ripples),float(w.config.foam_strength),float(w.config.time),w.config.seed,float(w.config.foam_uv_scale)};return m;
}
ir::Bounds reference_bounds(const Water &w,const ir::Transform &world){
  ir::Bounds bounds;for(double x:{-w.config.width*.5,w.config.width*.5})for(double y:{-w.config.length*.5,w.config.length*.5})bounds.add(world.point({float(x),float(y),0}));return bounds;
}
}
