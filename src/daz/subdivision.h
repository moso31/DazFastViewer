#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>

namespace dfv::daz {
inline ir::SubdivisionSettings subdivision_settings(const nlohmann::json &asset,const nlohmann::json &instance) {
  ir::SubdivisionSettings settings;
  auto read=[&](const nlohmann::json &geometry) {
    if(geometry.contains("type")) settings.enabled=geometry.at("type")=="subdivision_surface";
    for(const auto &extra:geometry.value("extra",nlohmann::json::array())) if(extra.value("type","")=="studio_geometry_channels")
      for(const auto &entry:extra.value("channels",nlohmann::json::array())) {
        const auto &channel=entry.at("channel");const auto id=channel.value("id","");
        const auto raw=channel.value("current_value",channel.value("value",nlohmann::json()));if(!raw.is_number()) continue;const int value=raw.get<int>();
        if(id=="SubDIALevel") settings.level=value;
        else if(id=="SubDRenderLevel") settings.render_level=value;
        else if(id=="SubDAlgorithmControl") settings.algorithm=value;
        else if(id=="SubDEdgeInterpolateLevel") settings.edge_interpolation=value;
        else if(id=="SubDNormalSmoothing") settings.normal_smoothing=value;
      }
    if(geometry.contains("current_subdivision_level")) settings.level=geometry.at("current_subdivision_level").get<int>();
    if(geometry.contains("edge_interpolation_mode")) {
      const auto mode=geometry.at("edge_interpolation_mode").get<std::string>();
      settings.edge_interpolation=mode=="no_interpolation"?0:mode=="edges_and_corners"?1:mode=="edges_only"?2:-1;
    }
    if(geometry.contains("subdivision_algorithm")) {
      const auto mode=geometry.at("subdivision_algorithm").get<std::string>();
      settings.algorithm=mode=="catmark"?0:mode=="bilinear"?1:mode=="loop"?2:mode=="catmull_clark"?3:-1;
    }
    if(geometry.contains("subd_normal_smoothing_mode")) settings.normal_smoothing=geometry.at("subd_normal_smoothing_mode")=="smooth_all_normals"?0:1;
  };
  read(asset);read(instance);return settings;
}
}
