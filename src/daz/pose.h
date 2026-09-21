#pragma once
#include "daz/loader.h"
#include "runtime/skeleton.h"
#include "runtime/morph.h"

namespace dfv::daz {
struct PoseChannel {std::string scheme,node,modifier,property;float value=0;};
struct PosePreset {std::string source;std::vector<PoseChannel> channels;};
PosePreset read_pose(const std::filesystem::path &file);
PosePreset parse_pose(const nlohmann::json &document,const std::string &source={});
struct AppliedPose {
  std::vector<runtime::JointPose> joints;
  runtime::Properties properties;
  nlohmann::json report;
};
AppliedPose apply_pose(const PosePreset &preset,const runtime::Skin &skin,const std::vector<runtime::JointPose> &current,
                       const runtime::Target &target,const runtime::Properties &properties);
}
