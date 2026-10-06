#include "cloud/model.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace dfv::cloud {
void validate(const Config &c){
  auto range=[](double v,double a,double b){return std::isfinite(v)&&v>=a&&v<=b;};
  if(!range(c.x,-1e6,1e6)||!range(c.y,-1e6,1e6)||!range(c.height,-1e6,1e6)||
     !range(c.width,1,1e6)||!range(c.length,1,1e6)||!range(c.thickness,1,1e5)||
     !range(c.density,0,10)||!range(c.coverage,0,1)||!range(c.scale,1,1e5)||!range(c.detail,0,4)||
     !range(c.wind_speed,0,10000)||!range(c.direction,-36000,36000)||!range(c.time,-1e6,1e6)||
     !range(c.padding,0,10000)||!range(c.softness,0,10000)||!range(c.trail,0,1000)||!range(c.recovery,.01,1000)||
     !range(c.velocity_x,-10000,10000)||!range(c.velocity_y,-10000,10000)||!range(c.velocity_z,-10000,10000)||
     c.steps<16||c.steps>192||c.sources.size()>max_sources)throw std::runtime_error("体积云参数超出范围（最多 16 个碰撞对象）");
  std::set<std::string> ids;for(const auto &id:c.sources)if(id.empty()||!ids.insert(id).second)throw std::runtime_error("体积云碰撞对象身份无效或重复");
}
nlohmann::json json(const Cloud &v){validate(v.config);const auto &c=v.config;nlohmann::json j={{"version",1},{"id",v.id}};
#define FIELD(n) j[#n]=c.n
  FIELD(x);FIELD(y);FIELD(height);FIELD(thickness);FIELD(width);FIELD(length);FIELD(density);FIELD(coverage);FIELD(scale);FIELD(detail);FIELD(wind_speed);FIELD(direction);FIELD(time);FIELD(padding);FIELD(softness);FIELD(velocity_x);FIELD(velocity_y);FIELD(velocity_z);FIELD(trail);FIELD(recovery);FIELD(steps);FIELD(seed);FIELD(collisions);FIELD(sources);
#undef FIELD
  return j;
}
std::shared_ptr<const Cloud> from_json(const nlohmann::json &j){
  if(j.at("version")!=1)throw std::runtime_error("不支持的体积云版本");auto v=std::make_shared<Cloud>();v->id=j.at("id");if(v->id.empty())throw std::runtime_error("体积云身份为空");auto &c=v->config;
#define FIELD(n) c.n=j.at(#n).get<decltype(c.n)>()
  FIELD(x);FIELD(y);FIELD(height);FIELD(thickness);FIELD(width);FIELD(length);FIELD(density);FIELD(coverage);FIELD(scale);FIELD(detail);FIELD(wind_speed);FIELD(direction);FIELD(time);FIELD(padding);FIELD(softness);FIELD(velocity_x);FIELD(velocity_y);FIELD(velocity_z);FIELD(trail);FIELD(recovery);FIELD(steps);FIELD(seed);FIELD(collisions);FIELD(sources);
#undef FIELD
  validate(c);return v;
}
std::shared_ptr<const Cloud> prefixed(const Cloud &v,const std::string &prefix){auto result=std::make_shared<Cloud>(v);result->id=prefix+v.id;for(auto &id:result->config.sources)id=prefix+id;return result;}
const Cloud *find(const Clouds &values,const std::string &id){for(const auto &v:values)if(v->id==id||v->id+"/volume"==id)return v.get();return nullptr;}
ir::Mesh mesh(const Cloud &v){
  validate(v.config);ir::Mesh m;m.id=v.id+"/mesh";m.material_slots={"cloud"};m.smooth=false;
  const float x=float(v.config.width*.5),y=float(v.config.length*.5),z=float(v.config.thickness*.5);
  m.positions={{-x,-y,-z},{x,-y,-z},{x,y,-z},{-x,y,-z},{-x,-y,z},{x,-y,z},{x,y,z},{-x,y,z}};
  // Watertight, outward-facing bounds, including when the camera is inside.
  for(auto face:std::initializer_list<std::array<uint32_t,3>>{{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}}){ir::Triangle t;t.vertices=face;m.triangles.push_back(t);}return m;
}
ir::Material material(const Cloud &v){
  validate(v.config);const auto &c=v.config;ir::Material m;m.id=v.id+"/material";m.cloud.emplace();auto &o=*m.cloud;
  o.half_extent={float(c.width*.5),float(c.length*.5),float(c.thickness*.5)};
  o.density=float(c.density);o.coverage=float(c.coverage);o.scale=float(c.scale);o.detail=float(c.detail);o.steps=c.steps;
  const double a=c.direction*3.141592653589793/180;
  // Noise is periodic only at very long distances; reduce in double precision.
  const double period=c.scale*10000;
  o.offset={float(std::remainder(-c.time*c.wind_speed*std::cos(a),period)+c.scale*(c.seed%997)),float(std::remainder(-c.time*c.wind_speed*std::sin(a),period)),float(c.scale*(c.seed%313))};
  return m;
}
float transmission(const ir::CloudCollider &c,ir::Vec3 p){
  const ir::Vec3 q{p.x-c.center.x,p.y-c.center.y,p.z-c.center.z};
  auto dot=[](ir::Vec3 a,ir::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
  auto inside=[&](float depth){if(c.softness==0)return depth>0?1.f:0.f;const float u=std::clamp(depth/c.softness,0.f,1.f);return u*u*(3-2*u);};
  float lo=-1e30f,hi=1e30f,side=1e30f;
  for(const auto &plane:c.planes){const float d=dot(plane.normal,q)-plane.distance-c.padding,b=dot(plane.normal,c.tail);
    if(std::abs(b)<1e-7f)side=std::min(side,-d);else if(b>0)lo=std::max(lo,d/b);else hi=std::min(hi,d/b);
  }
  // Clip the entire motion segment against every convex half-space. Feathering
  // stays INSIDE that intersection; padding is the only outward expansion.
  // Keep the ray interval unbounded until measuring inward depth. Clamping it
  // to [0,1] first makes short wakes thinner than the feather and incorrectly
  // restores cloud even deep inside the source as time leaves zero.
  const float depth=std::min({hi,1-lo,(hi-lo)*.5f})*c.tail_length;
  const float strength=inside(side)*(c.tail_length>0?inside(depth):1);
  return 1-strength*std::exp(-std::clamp(lo,0.f,1.f)*c.decay);
}
}
