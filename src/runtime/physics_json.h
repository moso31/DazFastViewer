#pragma once
#include "runtime/physics_settings.h"
#include <nlohmann/json.hpp>
namespace dfv::runtime {
inline nlohmann::json physics_json(const PhysicsObjectSettings &s) {
  validate_physics(s);return {{"version",1},{"enabled",s.enabled},{"rounds",s.rounds},{"excluded_surfaces",s.excluded_surfaces},{"joined",s.joined},{"kind",int(s.kind)},{"mass",s.mass},{"friction",s.friction},
    {"restitution",s.restitution},{"damping",s.damping},{"stiffness",s.stiffness},{"fixed_fraction",s.fixed_fraction},{"max_distance",s.max_distance},{"thickness",s.thickness}};
}
inline PhysicsObjectSettings physics_object_from_json(const nlohmann::json &j) {
  PhysicsObjectSettings s;if(j.is_null())return s;if(j.value("version",1)!=1)throw std::runtime_error("不支持的物理配置版本");
  s.enabled=j.value("enabled",false);s.kind=PhysicsKind(j.value("kind",0));s.mass=j.value("mass",s.mass);s.friction=j.value("friction",s.friction);
  s.rounds=j.value("rounds",s.rounds);s.joined=j.value("joined",true);s.excluded_surfaces=j.value("excluded_surfaces",s.excluded_surfaces);
  s.restitution=j.value("restitution",s.restitution);s.damping=j.value("damping",s.damping);s.stiffness=j.value("stiffness",s.stiffness);
  s.fixed_fraction=j.value("fixed_fraction",s.fixed_fraction);s.max_distance=j.value("max_distance",s.max_distance);s.thickness=j.value("thickness",s.thickness);validate_physics(s);return s;
}
inline nlohmann::json physics_json(const PhysicsOptions &s) {validate_physics(s);return {{"version",1},{"paused",s.paused},{"ground",s.ground},{"gravity",s.gravity},{"ground_height",s.ground_height},{"quality",s.quality},{"refresh_hz",s.refresh_hz}};}
inline PhysicsOptions physics_options_from_json(const nlohmann::json &j) {
  PhysicsOptions s;if(j.is_null())return s;if(j.value("version",1)!=1)throw std::runtime_error("不支持的全局物理配置版本");
  s.refresh_hz=j.value("refresh_hz",s.refresh_hz);s.paused=j.value("paused",s.paused);s.ground=j.value("ground",s.ground);s.gravity=j.value("gravity",s.gravity);s.ground_height=j.value("ground_height",s.ground_height);s.quality=j.value("quality",s.quality);validate_physics(s);return s;
}
}
