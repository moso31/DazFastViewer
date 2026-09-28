#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <set>
#include <string>

namespace dfv::runtime {
enum class PhysicsKind {automatic,rigid,cloth,hair_cards,hair_curves};
struct PhysicsObjectSettings {
  bool enabled=false;
  bool paused=false;
  int rounds=20;
  // 编辑器命令序号不写入场景文件；重新打开场景从初始状态运行。
  uint64_t run_sequence=0,reset_sequence=0;
  // 空的排除集合表示全部参与；材质身份只用于划定几何范围，不读取着色参数。
  std::set<std::string> excluded_surfaces;
  bool joined=true;
  PhysicsKind kind=PhysicsKind::automatic;
  float mass=.3f,friction=.4f,restitution=.05f,damping=2,stiffness=.8f;
  float fixed_fraction=.2f,max_distance=.25f,thickness=.004f;
  bool operator==(const PhysicsObjectSettings &) const=default;
};
struct PhysicsOptions {
  bool paused=false,ground=true;
  float gravity=9.81f,ground_height=0;
  int quality=1;
  double refresh_hz=4;
  bool operator==(const PhysicsOptions &) const=default;
};
inline void validate_physics(const PhysicsObjectSettings &s) {
  auto in=[](float x,float lo,float hi){return std::isfinite(x)&&x>=lo&&x<=hi;};
  if(s.rounds<1||int(s.kind)<0||int(s.kind)>4||!in(s.mass,.001f,100000.f)||!in(s.friction,0,2)||!in(s.restitution,0,1)||
     !in(s.damping,0,20)||!in(s.stiffness,0,1)||!in(s.fixed_fraction,.01f,.95f)||!in(s.max_distance,.001f,5)||!in(s.thickness,.0001f,.1f))
    throw std::runtime_error("物理参数超出范围");
}
inline void validate_physics(const PhysicsOptions &s) {
  if(!std::isfinite(s.refresh_hz)||s.refresh_hz<=0||!std::isfinite(s.gravity)||s.gravity<0||s.gravity>100||!std::isfinite(s.ground_height)||std::abs(s.ground_height)>10000||s.quality<0||s.quality>2)
    throw std::runtime_error("全局物理参数超出范围");
}
inline bool same_physics_parameters(PhysicsObjectSettings a,PhysicsObjectSettings b) {
  a.rounds=b.rounds=1;a.run_sequence=b.run_sequence=0;a.reset_sequence=b.reset_sequence=0;a.paused=b.paused=false;
  return a==b;
}
inline bool same_physics_environment(const PhysicsOptions &a,const PhysicsOptions &b) {
  return a.ground==b.ground&&a.ground_height==b.ground_height&&a.gravity==b.gravity&&a.quality==b.quality;
}
}
