#include "editor/powerpose_drag.h"
#include "editor/pose_drag.h"

namespace dfv::editor {
void PowerPoseDrag::begin(const runtime::Skin &skin,const ir::Mesh &mesh,const std::vector<ir::Vec3> &source,
  const std::vector<runtime::JointPose> &input,const std::vector<runtime::JointPose> &effective,
  ir::Transform matrix,runtime::TransformValues value,runtime::BoundPosePoint binding,
  runtime::PowerPoseInput pointer,CameraState view,int w,int h,std::vector<runtime::IkGoal> pins,const runtime::Target *figure,ir::Transform loaded_world) {
  active=false;moved=false;event_=0;press=pointer;bound=std::move(binding);initial_=input;effective_=solved_=effective;proposed_=input;
  base_world_=world=matrix;initial_transform_=transform=value;camera=view;width=w;height=h;result={};chain_.clear();bones.clear();
  loaded_world_=loaded_world;figure_frame_={};
  if(figure) {figure_frame_.has_edit_frame=figure->has_edit_frame;figure_frame_.edit_frame=figure->edit_frame;figure_frame_.translation_frame=figure->translation_frame;figure_frame_.base_rotation_degrees=figure->base_rotation_degrees;figure_frame_.rotation_order=figure->rotation_order;}
  if(!bound.editable||(bound.slots[pointer.right?2:0].empty()&&bound.slots[pointer.right?3:1].empty())) return;
  pins_=bound.hip_translation?std::move(pins):std::vector<runtime::IkGoal>{};limits_=runtime::ik_limits(skin,input,effective);
  for(size_t j=0;j<input.size();++j) for(int c=0;c<3;++c) {const float offset=runtime::joint_value(effective[j],c)-runtime::joint_value(input[j],c);limits_.joints[j].channels[c].minimum+=offset;limits_.joints[j].channels[c].maximum+=offset;}
  for(const auto &pin:pins_) for(int j:runtime::ik_chain(skin,pin.joint)) if(std::find(chain_.begin(),chain_.end(),j)==chain_.end()) chain_.push_back(j);
  build_pose_proxy(skin,mesh,source,proxy_skin_,proxy,source_);proxy.positions=runtime::deform(proxy_skin_,effective,source_);
  active=bound.editable;
}
bool PowerPoseDrag::update(const runtime::Skin &skin,const runtime::PowerPoseInput &pointer) {
  if(!active||pointer.event==event_||!pointer.moved) return false;event_=pointer.event;result={};
  if(bound.figure) {
    // Figure 使用编辑器现有的实例变换；不把整体移动伪装成根骨姿势。
    auto values=initial_;values.front().translation_cm=initial_transform_.translation_cm;values.front().rotation_degrees=initial_transform_.rotation_degrees;
    const auto changed=runtime::apply_powerpose(skin,bound,values,press.right,pointer.dx,pointer.dy,press.precision);
    transform=initial_transform_;transform.translation_cm=changed.front().translation_cm;transform.rotation_degrees=changed.front().rotation_degrees;
    if(!this->changed()&&!moved) return false;
    const auto before=runtime::parameter_transform(initial_transform_,figure_frame_,loaded_world_);
    const auto after=runtime::parameter_transform(transform,figure_frame_,loaded_world_);
    world=base_world_*ir::inverse(before*loaded_world_)*after*loaded_world_;
  } else {
    proposed_=runtime::apply_powerpose(skin,bound,initial_,press.right,pointer.dx,pointer.dy,press.precision);
    if(!changed()&&!moved) return false;
    solved_=effective_;
    for(size_t j=0;j<solved_.size();++j) for(int c=0;c<6;++c) {
      const float delta=runtime::joint_value(proposed_[j],c)-runtime::joint_value(initial_[j],c);
      if(delta!=0) runtime::set_joint_value(limits_,solved_,j,c,runtime::joint_value(effective_[j],c)+delta);
    }
    if(!pins_.empty()&&changed()) {
      const auto start=solved_;result=runtime::solve_ik(limits_,solved_,chain_,pins_,24);
      if(result.error>.001) {
        // 直腿压缩存在前屈／反屈两个分支；反屈会在膝盖下限停住。
        // 只在残差较大时尝试原生 Bend 正向种子，并只接受更好的约束结果。
        auto candidate=start;bool seeded=false;
        for(int j:chain_) {const auto &id=skin.joints[j].id;
          const float bend=(id=="lShin"||id=="rShin")?10.f:(id=="lThigh"||id=="rThigh")?-5.f:0;
          if(bend!=0&&runtime::editable_channel(limits_,j,3)) {runtime::set_joint_value(limits_,candidate,j,3,runtime::joint_value(candidate[j],3)+bend);seeded=true;}
        }
        if(seeded) {const auto alternative=runtime::solve_ik(limits_,candidate,chain_,pins_,24);
          auto score=[](const auto &r){return r.error*r.error+std::pow(r.angle_error_degrees*.0034906585,2);};
          if(score(alternative)<score(result)) {solved_=std::move(candidate);result=alternative;}
        }
      }
    }
    auto corrected=runtime::ik_input(initial_,effective_,solved_);
    for(size_t j=0;j<corrected.size();++j) corrected[j].translation_cm=proposed_[j].translation_cm;
    proposed_=std::move(corrected);
    proxy.positions=runtime::deform(proxy_skin_,solved_,source_);
  }
  moved=true;bones.clear();const auto matrices=runtime::joint_transforms(skin,solved_);
  for(int j:bound.joints) if(j>0) {
    auto at=[&](bool end){const auto a=end?skin.joints[j].end_cm:skin.joints[j].center_cm,b=end?solved_[j].end_offset_cm:solved_[j].center_offset_cm;return world.point(matrices[j].point({(a.x+b.x)*.01f,-(a.z+b.z)*.01f,(a.y+b.y)*.01f}));};
    bones.push_back({at(false),at(true)});
  }return true;
}
std::vector<runtime::JointPose> PowerPoseDrag::commit(const runtime::Skin &skin,const std::function<std::vector<runtime::JointPose>(const std::vector<runtime::JointPose>&)> &resolve) {
  if(!pins_.empty()) result=runtime::refine_ik_input(skin,proposed_,chain_,pins_,resolve);return proposed_;
}
}
