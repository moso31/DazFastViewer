#include "editor/pose_drag.h"
#include <map>

namespace dfv::editor {
void PoseDrag::begin(const runtime::Skin &skeleton,const ir::Mesh &mesh,const std::vector<ir::Vec3> &source,
  const std::vector<runtime::JointPose> &input,const std::vector<runtime::JointPose> &effective,
  ir::Transform world,int bone,runtime::PosePointer pointer,CameraState camera,int width,int height,std::vector<runtime::IkGoal> pins) {
  active=false;moved=false;press=pointer;pointer_revision_=0;input_=input;effective_=solved_=effective;
  solver_=runtime::ik_limits(skeleton,input,effective);chain_=runtime::ik_chain(solver_,bone);if(chain_.empty()) return;
  pins_=std::move(pins);for(const auto &pin:pins_) {if(pin.joint==bone&&pin.fix_position) return;for(int j:runtime::ik_chain(solver_,pin.joint)) if(std::find(chain_.begin(),chain_.end(),j)==chain_.end()) chain_.push_back(j);}
  if(std::none_of(chain_.begin(),chain_.end(),[&](int j){for(int c=3;c<6;++c) if(runtime::editable_channel(solver_,j,c)) return true;return false;})) return;
  joint=bone;world_=world;inverse_=ir::inverse(world);camera_=camera;width_=width;height_=height;
  goal={bone,true,runtime::joint_point(solver_,effective,bone,true),1};start_=world.point(goal.position);
  const auto m=camera.matrix();const float depth=(start_.x-m[3])*m[2]+(start_.y-m[7])*m[6]+(start_.z-m[11])*m[10];if(depth<=0) return;
  pixel_scale_=2*std::tan(.4f)*depth/std::max(1,std::min(width,height));
  proxy={};proxy_skin_=solver_;source_.clear();std::map<uint32_t,uint32_t> indices;
  constexpr size_t triangle_budget=40000;const size_t stride=std::max(size_t(1),(mesh.triangles.size()+triangle_budget-1)/triangle_budget);
  for(size_t t=0;t<mesh.triangles.size();t+=stride) {auto triangle=mesh.triangles[t];
    for(auto &v:triangle.vertices) {const auto old=v;auto [it,inserted]=indices.emplace(old,uint32_t(indices.size()));if(inserted) {source_.push_back(source.at(old));proxy_skin_.weights.push_back(skeleton.weights.at(old));}v=it->second;}
    proxy.triangles.push_back(triangle);
  }
  proxy.positions=runtime::deform(proxy_skin_,effective,source_);active=true;result={};
}
bool PoseDrag::update(runtime::PosePointer pointer) {
  if(!active||pointer.revision==pointer_revision_||!pointer.moved) return false;pointer_revision_=pointer.revision;moved=true;
  const auto m=camera_.matrix();const float dx=(pointer.x-press.start_x)*pixel_scale_,dy=-(pointer.y-press.start_y)*pixel_scale_;
  goal.position=inverse_.point({start_.x+m[0]*dx+m[1]*dy,start_.y+m[4]*dx+m[5]*dy,start_.z+m[8]*dx+m[9]*dy});
  auto goals=pins_;goals.insert(goals.begin(),goal);result=runtime::solve_ik(solver_,solved_,chain_,goals,24);
  proxy.positions=runtime::deform(proxy_skin_,solved_,source_);bones.clear();
  const auto matrices=runtime::joint_transforms(solver_,solved_);
  auto at=[&](int j,bool end) {const auto a=end?solver_.joints[j].end_cm:solver_.joints[j].center_cm,b=end?solved_[j].end_offset_cm:solved_[j].center_offset_cm;return world_.point(matrices[j].point({(a.x+b.x)*.01f,-(a.z+b.z)*.01f,(a.y+b.y)*.01f}));};
  for(int j:chain_) bones.push_back({at(j,false),at(j,true)});
  return true;
}
std::vector<runtime::JointPose> PoseDrag::commit(const runtime::Skin &skin,const std::function<std::vector<runtime::JointPose>(const std::vector<runtime::JointPose>&)> &resolve) {
  auto proposed=input();if(pins_.empty()) return proposed;
  auto goals=pins_;goals.insert(goals.begin(),goal);result=runtime::refine_ik_input(skin,proposed,chain_,goals,resolve);return proposed;
}
}
