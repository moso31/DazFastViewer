#pragma once
#include "render_ir/scene.h"
#include <cmath>
#include <limits>

namespace dfv::ir {
inline bool emits(const Material &m) {
  return m.emission_luminance>0&&(m.emission_color.x>0||m.emission_color.y>0||m.emission_color.z>0);
}
// 与现有 Diffeomorphic 材质对照及 EV13 预览基准一致；非 Iray 绝对测光标定。
inline float emission_strength(const Material &m,double area_m2=1) {
  if(!emits(m)) return 0;
  constexpr double factors[]={1,1000,1/.09290304,10000,1,1};
  double strength=m.emission_luminance*factors[std::clamp(m.emission_units,0,5)]/30000;
  if(m.emission_units>=4) strength=area_m2>1e-12?strength/area_m2:0;
  if(m.emission_units==5) strength*=m.emission_efficacy;
  return float(std::min(strength,double(std::numeric_limits<float>::max())));
}
// 每个源表面绑定的实际世界面积。散布实例复用原型的亮度，不能把复制数计入原型功率。
inline std::vector<float> emission_strengths(const Scene &scene) {
  std::vector<double> areas(scene.materials.size());
  const bool power=std::any_of(scene.materials.begin(),scene.materials.end(),[](const auto &m){return emits(m)&&m.emission_units>=4;});
  if(power) for(const auto &i:scene.instances) if(i.prototype<0) {
    const auto &mesh=scene.meshes.at(i.mesh);
    for(const auto &t:mesh.triangles) {
      const auto m=i.materials.at(t.material_slot);if(!emits(scene.materials[m])||scene.materials[m].emission_units<4||!mesh.draws(t)) continue;
      const auto a=i.transform.point(mesh.positions[t.vertices[0]]),b=i.transform.point(mesh.positions[t.vertices[1]]),c=i.transform.point(mesh.positions[t.vertices[2]]);
      const double x=b.x-a.x,y=b.y-a.y,z=b.z-a.z,u=c.x-a.x,v=c.y-a.y,w=c.z-a.z;
      areas[m]+=.5*std::hypot(y*w-z*v,z*u-x*w,x*v-y*u);
    }
  }
  std::vector<float> result;for(size_t m=0;m<scene.materials.size();++m) result.push_back(emission_strength(scene.materials[m],areas[m]));return result;
}
}
