#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>
#include <memory>

namespace dfv::cloud {
inline constexpr size_t max_sources=16;
// Metres and seconds. Every sample is independent of wall time and edit history.
struct Config {
  double x=0,y=0,height=250,thickness=180,width=2000,length=2000;
  double density=.035,coverage=.65,scale=120,detail=2,wind_speed=12,direction=25,time=0;
  double padding=0,softness=1,velocity_x=0,velocity_y=0,velocity_z=-60,trail=8,recovery=5;
  int steps=64;
  uint32_t seed=1337;
  bool collisions=true;
  std::vector<std::string> sources;
  bool operator==(const Config &) const=default;
};
struct Cloud {std::string id;Config config;};
using Clouds=std::vector<std::shared_ptr<const Cloud>>;
struct Candidate {std::string id,label;double volume=-1;};
void validate(const Config &);
nlohmann::json json(const Cloud &);
std::shared_ptr<const Cloud> from_json(const nlohmann::json &);
std::shared_ptr<const Cloud> prefixed(const Cloud &,const std::string &);
const Cloud *find(const Clouds &,const std::string &);
ir::Mesh mesh(const Cloud &);
ir::Material material(const Cloud &);
// CPU reference for the analytic swept collision used by the volume shader.
float transmission(const ir::CloudCollider &,ir::Vec3 local_point);
}
