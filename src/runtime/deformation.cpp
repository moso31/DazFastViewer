#include "runtime/deformation.h"
#include "diagnostics/load_profile.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dfv::runtime {
namespace {
float component(ir::Vec3 v,uint32_t axis) {return axis==0?v.x:axis==1?v.y:v.z;}
void component(ir::Vec3 &v,uint32_t axis,double value) {if(!std::isfinite(float(value))) throw std::runtime_error("骨骼通道超出有效数值范围");(axis==0?v.x:axis==1?v.y:v.z)=float(value);}
double bone_input(const Binding &b,const Skin &skin,const std::vector<JointPose> &poses) {
  const auto &p=poses.at(b.index);const auto &j=skin.joints.at(b.index);
  switch(b.property) {
    case Property::translation:return component(p.translation_cm,b.axis);
    case Property::rotation:return component(p.rotation_degrees,b.axis);
    case Property::scale:return component(p.scale,b.axis);
    case Property::general_scale:return p.general_scale*(b.index==0?skin.root_general_scale:1);
    case Property::center:return component(j.center_cm,b.axis)+component(p.center_offset_cm,b.axis);
    case Property::end:return component(j.end_cm,b.axis)+component(p.end_offset_cm,b.axis);
    case Property::orientation:return component(j.orientation_degrees,b.axis)+component(p.orientation_offset_degrees,b.axis);
    default:throw std::runtime_error("无效骨骼输入绑定");
  }
}
void bone_output(const Binding &b,const Skin &skin,std::vector<JointPose> &poses,double value) {
  auto &p=poses.at(b.index);const auto &j=skin.joints.at(b.index);
  switch(b.property) {
    case Property::translation:component(p.translation_cm,b.axis,value);break;
    case Property::rotation:component(p.rotation_degrees,b.axis,value);break;
    case Property::scale:component(p.scale,b.axis,value);break;
    case Property::general_scale:if(!std::isfinite(float(value))) throw std::runtime_error("无效总体缩放");p.general_scale=float(value)/(b.index==0?skin.root_general_scale:1);break;
    case Property::center:component(p.center_offset_cm,b.axis,value-component(j.center_cm,b.axis));break;
    case Property::end:component(p.end_offset_cm,b.axis,value-component(j.end_cm,b.axis));break;
    case Property::orientation:component(p.orientation_offset_degrees,b.axis,value-component(j.orientation_degrees,b.axis));break;
    default:throw std::runtime_error("无效骨骼输出绑定");
  }
}
}
void sync_aliases(const Target &target,Properties &values) {for(size_t m=0;m<target.morphs.size();++m) if(target.morphs[m].alias_morph>=0) values.morphs[m]=values.morphs.at(size_t(target.morphs[m].alias_morph));}
void set_parameter(const Target &target,Properties &values,size_t index,float value) {
  const auto &p=target.morphs.at(index);if(!p.unsupported.empty()) throw std::runtime_error(p.unsupported);if(!std::isfinite(value)) throw std::runtime_error("参数必须为有限值");
  if(p.value_type=="bool"||p.value_type=="int") value=std::round(value);
  values.morphs.at(p.alias_morph>=0?size_t(p.alias_morph):index)=value;sync_aliases(target,values);
}
DeformationRuntime::DeformationRuntime(ir::Scene &scene,const std::vector<Target> &targets,const std::vector<Skin> &skins,const std::vector<FormulaGraph> &graphs)
  :targets_(targets),skins_(skins),graphs_(graphs),morph_(scene,targets),skin_(scene,skins),conform_(scene,targets,skins,graphs),collision_(scene,targets) {
  if(graphs.size()!=targets.size()) throw std::runtime_error("角色和公式图数量不一致");
  for(const auto &g:graphs) formulas_.push_back(std::make_unique<FormulaRuntime>(g));
  for(const auto &target:targets) {Properties p;for(const auto &m:target.morphs) p.morphs.push_back(m.evaluable||m.unsupported.empty()?m.initial:0);sync_aliases(target,p);previous_.push_back(p);}
  for(const auto &skin:skins) previous_poses_.push_back(skin.initial);
  // 没有父对象的穿戴物仍跟随 Fit To 目标的交互变换；不改写场景层级。
  for(size_t t:conform_.order()) if(targets[t].parent.empty()) if(const auto *link=conform_.link(t)) morph_.bind_parent(t,link->source);
  for(size_t s=0;s<skins.size();++s) {
    const auto &skin=skins[s];size_t parent=0;while(parent<targets.size()&&targets[parent].instance!=skin.instance) ++parent;
    if(parent==targets.size()) continue;
    const auto figure=scene.instances.at(skin.instance).transform;const auto bind=joint_transforms(skin,skin.initial);
    for(size_t j=0;j<skin.joints.size();++j) if(!skin.joints[j].scene_id.empty()) for(size_t t=0;t<targets.size();++t)
      if(targets[t].conform_target.empty()&&targets[t].parent=="#"+skin.joints[j].scene_id) {
        morph_.bind_parent(t,parent);attachments_.push_back({t,s,j,figure,ir::inverse(figure*bind[j])});
      }
  }
}
void DeformationRuntime::feed(const std::vector<Properties> &values,const std::vector<std::vector<JointPose>> &poses,std::vector<std::vector<float>> &weights,std::vector<std::vector<JointPose>> &resolved) {
  diagnostics::Scope scope("formula_feed");
  if(values.size()!=targets_.size()||poses.size()!=skins_.size()) throw std::runtime_error("变形快照数量不一致");
  weights.resize(targets_.size());resolved=poses;
  for(size_t s=0;s<skins_.size();++s) validate_pose(skins_[s],poses[s]);
  for(size_t t:conform_.order()) {
    if(values[t].morphs.size()!=targets_[t].morphs.size()) throw std::runtime_error("Morph 快照数量不一致");const auto &g=graphs_[t];auto &runtime=*formulas_[t];
    const auto *link=conform_.link(t);std::vector<JointPose> inherited;
    if(link&&g.skin>=0) {inherited=conform_pose(*link,skins_,resolved,poses.at(size_t(g.skin)));resolved[size_t(g.skin)]=inherited;}
    for(uint32_t c=0;c<g.channels.size();++c) {const auto &binding=g.channels[c].binding;
      if(binding.property==Property::morph) {if(g.morph_channels[binding.index]!=int(c)) continue;double input=values[t].morphs[binding.index];
        if(link&&link->morph_sources.at(binding.index)>=0) input+=weights.at(link->source).at(size_t(link->morph_sources[binding.index]));runtime.set(c,input);}
      else runtime.set(c,bone_input(binding,skins_.at(size_t(g.skin)),resolved.at(size_t(g.skin))));}
    runtime.evaluate();
    const auto &result=runtime.values();auto &value=weights[t];value.resize(targets_[t].morphs.size());
    for(size_t m=0;m<value.size();++m) {const int c=g.morph_channels[m];if(c>=0&&targets_[t].morphs[m].evaluable) value[m]=float(result[size_t(c)]);if(!std::isfinite(value[m])) throw std::runtime_error("Morph 求值超出有效数值范围");}
    for(size_t c=0;c<g.channels.size();++c) if(g.channels[c].binding.property!=Property::morph) bone_output(g.channels[c].binding,skins_.at(size_t(g.skin)),resolved.at(size_t(g.skin)),result[c]);
    // 目标 ERC 已包含体型骨长 / 中心调整，服装不能再次叠加相同公式。
    if(link&&g.skin>=0) for(size_t j=0;j<link->joints.size();++j) if(link->joints[j]>=0) resolved[size_t(g.skin)][j]=inherited[j];
  }
}
PayloadProgress DeformationRuntime::prepare(const std::vector<Properties> &values,const std::vector<std::vector<JointPose>> &poses,bool retry) {
  std::vector<std::vector<float>> weights;std::vector<std::vector<JointPose>> resolved;
  try {feed(values,poses,weights,resolved);}
  catch(...) {std::vector<std::vector<float>> rollback_weights;std::vector<std::vector<JointPose>> rollback_poses;feed(previous_,previous_poses_,rollback_weights,rollback_poses);throw;}
  PayloadProgress progress;std::vector<std::shared_ptr<const OffsetBuffer>> leases;std::set<MorphPayload *> seen;
  for(size_t t=0;t<weights.size();++t) for(size_t m=0;m<weights[t].size();++m) if(weights[t][m]!=0) {
    const auto &p=targets_[t].morphs[m].payload;if(!p||!seen.insert(p.get()).second) continue;
    p->request(retry);if(auto data=p->acquire()) leases.push_back(std::move(data));
    else if(p->state()==PayloadState::failed) {if(progress.error.empty()) progress.error=targets_[t].morphs[m].label+"："+p->error();}
    else ++progress.pending;
  }
  payload_leases_=std::move(leases);trim_morph_cache();return progress;
}
ir::Delta DeformationRuntime::evaluate(const std::vector<Properties> &values,const std::vector<std::vector<JointPose>> &poses) {
  std::vector<std::vector<float>> weights;auto resolved=poses;
  try {
    for(const auto &p:values) validate_transform(p.transform);
    feed(values,poses,weights,resolved);
    // 命令行调用保持同步语义；编辑器先调用 prepare，资源就绪后才提交形变。
    std::vector<std::shared_ptr<const OffsetBuffer>> leases;
    for(size_t t=0;t<weights.size();++t) for(size_t m=0;m<weights[t].size();++m) if(weights[t][m]!=0&&targets_[t].morphs[m].payload) leases.push_back(targets_[t].morphs[m].payload->ensure());
    payload_leases_=std::move(leases);
    trim_morph_cache();
    for(size_t s=0;s<skins_.size();++s) validate_pose(skins_[s],resolved[s]);
  } catch(...) {std::vector<std::vector<float>> rollback_weights;std::vector<std::vector<JointPose>> rollback_poses;feed(previous_,previous_poses_,rollback_weights,rollback_poses);throw;}
  for(size_t t=0;t<weights.size();++t) {
    for(size_t m=0;m<weights[t].size();++m) if(targets_[t].morphs[m].evaluable||targets_[t].morphs[m].unsupported.empty()) morph_.set_morph(t,m,weights[t][m]);
    morph_.set_transform(t,values[t].transform);
    morph_.set_visible(t,values[t].visible);
  }
  for(size_t s=0;s<resolved.size();++s) skin_.set_pose(s,resolved[s]);
  std::vector<std::vector<ir::Transform>> joints(skins_.size());
  for(const auto &a:attachments_) {
    if(joints[a.skin].empty()) joints[a.skin]=joint_transforms(skins_[a.skin],resolved[a.skin]);
    morph_.set_attachment(a.target,a.figure*joints[a.skin][a.joint]*a.inverse_bind);
  }
  conform_.project(weights,morph_);
  effective_=std::move(weights);effective_poses_=std::move(resolved);previous_=values;previous_poses_=poses;return collision_.evaluate(skin_.evaluate(morph_.evaluate()));
}
FormulaStats DeformationRuntime::formula_stats() const {FormulaStats out;for(const auto &f:formulas_) {out.expressions+=f->stats().expressions;out.channels+=f->stats().channels;}return out;}
}
