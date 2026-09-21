#include "runtime/morph.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <functional>

namespace dfv::runtime {
static void finite(ir::Vec3 p) {
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) throw std::runtime_error("属性包含非有限数值");
}
static bool same(ir::Vec3 a,ir::Vec3 b) {return a.x==b.x && a.y==b.y && a.z==b.z;}
MorphRuntime::MorphRuntime(ir::Scene &scene,const std::vector<Target> &targets):scene_(scene),targets_(targets) {
  parents_.resize(targets.size(),-1);follow_offsets_.resize(targets.size());attachments_.resize(targets.size());
  for(size_t i=0;i<targets.size();++i) if(targets[i].parent.starts_with('#')) for(size_t j=0;j<targets.size();++j) if(i!=j&&targets[j].id.substr(0,targets[j].id.rfind('/'))==targets[i].parent.substr(1)) parents_[i]=int(j);
  for(size_t i=0;i<parents_.size();++i) {std::set<int> seen;for(int p=int(i);p>=0;p=parents_[size_t(p)]) if(!seen.insert(p).second) throw std::runtime_error("编辑对象的父子关系存在循环");}
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
ir::Transform make_transform(const TransformValues &v) {
  validate_transform(v);
    ir::Transform scale;scale.value[0]=v.scale.x;scale.value[5]=v.scale.y;scale.value[10]=v.scale.z;
    ir::Transform rotation;
    for(int axis=0;axis<3;++axis) {
      const float angle=(axis==0?v.rotation_degrees.x:axis==1?v.rotation_degrees.y:v.rotation_degrees.z)*std::numbers::pi_v<float>/180;
      ir::Transform r;const int j=(axis+1)%3,k=(axis+2)%3;
      r.value[j*4+j]=r.value[k*4+k]=std::cos(angle);r.value[j*4+k]=-std::sin(angle);r.value[k*4+j]=std::sin(angle);rotation=r*rotation;
    }
    ir::Transform conversion;conversion.value={.01f,0,0,0,0,0,-.01f,0,0,.01f,0,0};
    ir::Transform inverse;inverse.value={100,0,0,0,0,0,100,0,0,-100,0,0};
    return conversion*ir::Transform::translate(v.translation_cm)*rotation*scale*inverse;
}
bool MorphRuntime::set_transform(size_t target,const TransformValues &v) {
  validate_transform(v);
  auto &old=values_.at(target).transform;
  if(same(old.translation_cm,v.translation_cm)&&same(old.rotation_degrees,v.rotation_degrees)&&same(old.scale,v.scale)) return false;
  old=v;dirty_transform(target);return true;
}
void MorphRuntime::dirty_transform(size_t target) {
  dirty_transforms_.insert(target);
  for(size_t i=0;i<parents_.size();++i) for(int p=parents_[i];p>=0;p=parents_[size_t(p)]) if(size_t(p)==target) {dirty_transforms_.insert(i);break;}
}
void MorphRuntime::bind_parent(size_t target,size_t parent) {
  if(target>=parents_.size()||parent>=parents_.size()) throw std::runtime_error("附件父节点越界");
  for(int p=int(parent);p>=0;p=parents_[size_t(p)]) if(size_t(p)==target) throw std::runtime_error("骨骼附件父子关系存在循环");
  parents_[target]=int(parent);dirty_transform(target);
}
void MorphRuntime::set_attachment(size_t target,const ir::Transform &delta) {
  auto &old=attachments_.at(target);if(old.value==delta.value) return;
  old=delta;dirty_transform(target);
}
bool MorphRuntime::set_follow_offsets(size_t target,const std::vector<ir::Vec3> &offsets) {
  if(offsets.size()!=bases_.at(target).size()) throw std::runtime_error("服装跟随顶点数量不一致");
  for(auto p:offsets) finite(p);
  auto &old=follow_offsets_.at(target);
  if(old.size()==offsets.size()&&std::equal(old.begin(),old.end(),offsets.begin(),same)) return false;
  // 初次绑定的全零位移不使静止网格变脏。
  const bool changed=!old.empty()||std::any_of(offsets.begin(),offsets.end(),[](ir::Vec3 p) {return !same(p,{});});
  old=offsets;if(changed) dirty_meshes_.insert(target);return changed;
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
    for(size_t v=0;v<follow_offsets_[target].size();++v) {const auto d=follow_offsets_[target][v];positions[v].x+=d.x;positions[v].y+=d.y;positions[v].z+=d.z;}
    const auto mesh=scene_.instances.at(targets_[target].instance).mesh;
    for(auto p:positions) finite(p);
    scene_.meshes[mesh].positions=positions;delta.meshes.push_back({mesh,std::move(positions)});++stats_.morph_evaluations;
  }
  std::function<ir::Transform(size_t)> edit_transform=[&](size_t target) {
    const auto &v=values_[target].transform;
    auto result=attachments_[target]*make_transform(v);
    if(parents_[target]>=0) result=edit_transform(size_t(parents_[target]))*result;
    return result;
  };
  for(auto target:dirty_transforms_) {
    const auto transform=edit_transform(target)*transforms_[target];
    const auto instance=targets_[target].instance;scene_.instances[instance].transform=transform;
    delta.instances.push_back({instance,transform});++stats_.transform_evaluations;
  }
  dirty_meshes_.clear();dirty_transforms_.clear();return delta;
}
}
