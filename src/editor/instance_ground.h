#pragma once
#include "runtime/picking.h"
#include <map>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dfv::editor {
struct InstanceGround {double offset_m=0,ratio=0,offset_cm=0;bool body_only=false;bool operator==(const InstanceGround &) const=default;};
using InstanceGrounds=std::map<std::string,InstanceGround>;
inline void prune_instance_ground(const ir::Scene &scene,InstanceGrounds &values){
  const runtime::InstanceGroups groups(scene);
  std::erase_if(values,[&](const auto &v){for(size_t i=0;i<scene.instances.size();++i)if(scene.instances[i].prototype>=0&&groups.roots[i]==i&&scene.instances[i].id==v.first)return false;return true;});
}
inline void validate_instance_ground(const ir::Scene &scene,const InstanceGrounds &values){
  auto known=values;prune_instance_ground(scene,known);if(known.size()!=values.size())throw std::runtime_error("地面对齐实例不存在");
  for(const auto &[id,v]:values)if(!std::isfinite(v.offset_m)||!std::isfinite(float(v.offset_m))||!std::isfinite(v.ratio)||!std::isfinite(v.offset_cm))throw std::runtime_error("实例地面对齐数值无效");
}
// DAZ Instance 的变换独立于原型形变；同一条目的全部渲染零件一起平移。
inline void apply_instance_ground(ir::Scene &scene,const ir::Scene &base,const InstanceGrounds &values,ir::Delta *delta=nullptr,const std::vector<ir::Transform> *evaluated_base=nullptr){
  validate_instance_ground(base,values);const runtime::InstanceGroups groups(base);
  for(uint32_t i=0;i<scene.instances.size();++i){auto &instance=scene.instances[i];if(instance.prototype<0)continue;
    const auto found=values.find(base.instances[groups.roots[i]].id);auto world=evaluated_base?evaluated_base->at(i):base.instances[i].transform;
    if(found!=values.end())world.value[11]=float(double(world.value[11])+found->second.offset_m);
    if(!std::isfinite(world.value[11]))throw std::runtime_error("实例地面对齐位置超出范围");
    if(world==instance.transform)continue;instance.transform=world;
    if(delta){auto edit=std::find_if(delta->instances.begin(),delta->instances.end(),[&](const auto &v){return v.index==i;});if(edit==delta->instances.end())delta->instances.push_back({i,world});else edit->transform=world;}
  }
}
}
