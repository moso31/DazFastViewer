#include "runtime/morph.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace dfv::runtime {
static void finite(ir::Vec3 p) {
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) throw std::runtime_error("属性包含非有限数值");
}
static bool same(ir::Vec3 a,ir::Vec3 b) {return a.x==b.x && a.y==b.y && a.z==b.z;}
MorphRuntime::MorphRuntime(ir::Scene &scene,const std::vector<Target> &targets):scene_(scene),targets_(targets) {
  std::set<uint32_t> used_meshes;
  for(const auto &target:targets) {
    const auto &instance=scene.instances.at(target.instance);const auto &mesh=scene.meshes.at(instance.mesh);
    if(!used_meshes.insert(instance.mesh).second) throw std::runtime_error("可编辑对象必须拥有独立网格实例");
    bases_.push_back(mesh.positions);transforms_.push_back(instance.transform);active_.emplace_back();
    Properties property;
    for(const auto &m:target.morphs) {
      if(!std::isfinite(m.minimum)||!std::isfinite(m.maximum)||!std::isfinite(m.initial)||m.minimum>m.maximum)
        throw std::runtime_error("Morph 范围无效");
      std::set<uint32_t> vertices;
      for(const auto &offset:m.offsets) {
        finite(offset.delta);
        if(offset.vertex>=mesh.positions.size() || !vertices.insert(offset.vertex).second) throw std::runtime_error("Morph 顶点索引越界或重复");
      }
      property.morphs.push_back(0);
    }
    values_.push_back(std::move(property));
    const auto index=values_.size()-1;
    for(size_t m=0;m<target.morphs.size();++m) if(target.morphs[m].unsupported.empty()||target.morphs[m].evaluable) set_morph(index,m,target.morphs[m].initial);
  }
}
bool MorphRuntime::set_morph(size_t target,size_t index,float value) {
  const auto &m=targets_.at(target).morphs.at(index);auto &current=values_.at(target).morphs.at(index);
  if(!m.unsupported.empty()&&!m.evaluable) throw std::runtime_error(m.unsupported);
  if(!std::isfinite(value)) throw std::runtime_error("Morph 权重必须为有限数值");
  if(m.clamped) value=std::clamp(value,m.minimum,m.maximum);
  if(current==value) return false;
  current=value;
  if(value==0) active_[target].erase(index);else active_[target].insert(index);
  if(!m.offsets.empty()) dirty_meshes_.insert(target);return true;
}
void validate_transform(const TransformValues &v) {
  finite(v.translation_cm);finite(v.rotation_degrees);finite(v.scale);
  if(v.scale.x<=0 || v.scale.y<=0 || v.scale.z<=0) throw std::runtime_error("缩放必须大于零");
}
bool MorphRuntime::set_transform(size_t target,const TransformValues &v) {
  validate_transform(v);
  auto &old=values_.at(target).transform;
  if(same(old.translation_cm,v.translation_cm)&&same(old.rotation_degrees,v.rotation_degrees)&&same(old.scale,v.scale)) return false;
  old=v;dirty_transforms_.insert(target);return true;
}
ir::Delta MorphRuntime::evaluate() {
  ir::Delta delta;
  for(auto target:dirty_meshes_) {
    auto positions=bases_[target];
    for(auto index:active_[target]) {
      const float weight=values_[target].morphs[index];
      for(const auto &offset:targets_[target].morphs[index].offsets) {
        auto &p=positions[offset.vertex];p.x+=weight*offset.delta.x;p.y+=weight*offset.delta.y;p.z+=weight*offset.delta.z;
        ++stats_.offsets_visited;
      }
    }
    const auto mesh=scene_.instances.at(targets_[target].instance).mesh;
    for(auto p:positions) finite(p);
    scene_.meshes[mesh].positions=positions;delta.meshes.push_back({mesh,std::move(positions)});++stats_.morph_evaluations;
  }
  for(auto target:dirty_transforms_) {
    const auto &v=values_[target].transform;
    ir::Transform scale;scale.value[0]=v.scale.x;scale.value[5]=v.scale.y;scale.value[10]=v.scale.z;
    ir::Transform rotation;
    for(int axis=0;axis<3;++axis) {
      const float angle=(axis==0?v.rotation_degrees.x:axis==1?v.rotation_degrees.y:v.rotation_degrees.z)*std::numbers::pi_v<float>/180;
      ir::Transform r;const int j=(axis+1)%3,k=(axis+2)%3;
      r.value[j*4+j]=r.value[k*4+k]=std::cos(angle);r.value[j*4+k]=-std::sin(angle);r.value[k*4+j]=std::sin(angle);rotation=r*rotation;
    }
    ir::Transform conversion;conversion.value={.01f,0,0,0,0,0,-.01f,0,0,.01f,0,0};
    ir::Transform inverse;inverse.value={100,0,0,0,0,0,100,0,0,-100,0,0};
    const auto transform=conversion*ir::Transform::translate(v.translation_cm)*rotation*scale*inverse*transforms_[target];
    const auto instance=targets_[target].instance;scene_.instances[instance].transform=transform;
    delta.instances.push_back({instance,transform});++stats_.transform_evaluations;
  }
  dirty_meshes_.clear();dirty_transforms_.clear();return delta;
}
}
