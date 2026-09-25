#pragma once
#include "runtime/powerpose.h"
#include <set>
inline dfv::runtime::Skin powerpose_fixture() {
  using namespace dfv::runtime;Skin skin;skin.id="figure-A";Joint root;root.id=root.name="Genesis8Female";skin.joints.push_back(root);
  std::set<std::string> names;
  for(const auto &page:powerpose_templates()) for(const auto &p:page.points) for(const auto &slot:p.slots) for(const auto &b:slot) if(b.node!="Figure") names.insert(b.node);
  for(const auto *name:{"lShldrTwist","rShldrTwist","lThighTwist","rThighTwist","lWrist","rWrist"}) names.insert(name);
  for(const auto &name:names) {Joint j;j.id=j.name=j.label=name;j.parent=0;if(name.find("Thigh")!=std::string::npos||name.find("Shin")!=std::string::npos) j.rotation_order="YZX";else if(name.find("Shldr")!=std::string::npos) j.rotation_order="XYZ";skin.joints.push_back(j);}
  for(auto &j:skin.joints) for(int i=0;i<6;++i) {auto &c=j.channels[i];c.present=true;c.clamped=true;c.minimum=i<3?-10000:-90;c.maximum=i<3?10000:90;c.label=std::string(1,"XYZ"[i%3])+(i<3?" Translate":" Rotate");}
  for(const auto *side:{"l","r"}) {
    auto &shoulder=skin.joints[powerpose_joint(skin,std::string(side)+"Shldr")];shoulder.channels[3].locked=true;shoulder.channels[5].label="Bend";
    auto &twist=skin.joints[powerpose_joint(skin,std::string(side)+"ShldrTwist")];twist.channels[3].label="Twist";
    auto &thigh=skin.joints[powerpose_joint(skin,std::string(side)+"Thigh")];thigh.channels[3].minimum=-115;thigh.channels[3].maximum=35;thigh.channels[4].locked=true;
    auto &shin=skin.joints[powerpose_joint(skin,std::string(side)+"Shin")];shin.channels[3].minimum=-11;shin.channels[3].maximum=155;
    skin.joints[powerpose_joint(skin,std::string(side)+"ForeArm")].channels[3].locked=true;
  }
  auto &pelvis=skin.joints[powerpose_joint(skin,"pelvis")];pelvis.channels[3].label="Bend";pelvis.channels[4].label="Twist";pelvis.channels[5].label="Side-Side";
  skin.initial.resize(skin.joints.size());return skin;
}
inline const dfv::runtime::PosePoint &powerpose_point(const std::string &id) {
  for(const auto &page:dfv::runtime::powerpose_templates()) for(const auto &point:page.points) if(point.id==id) return point;throw std::runtime_error("测试控制点不存在");
}
