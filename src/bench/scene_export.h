#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>

namespace dfv {
inline nlohmann::json scene_json(const ir::Scene &scene,int samples) {
  using J=nlohmann::json;
  auto vec=[](ir::Vec3 v) {return J::array({v.x,v.y,v.z});};
  J result={{"schema","dfv-material-reference-1"},{"coordinate_system","meters-Z-up"},
    {"environment",vec(scene.environment)},
    {"camera",{{"transform",scene.camera.transform.value},{"width",scene.camera.width},
               {"height",scene.camera.height},{"fov_short_axis",scene.camera.fov}}},
    {"render",{{"samples",samples},{"seed",1337},{"max_bounces",8},{"diffuse_bounces",4},
               {"glossy_bounces",4},{"transmission_bounces",8},{"transparent_max_bounces",8},
               {"denoise",false},{"adaptive_sampling",false},{"working_space","linear-Rec709"}}}};
  for(const char *key:{"textures","materials","meshes","instances","lights"}) result[key]=J::array();
  for(const auto &t:scene.textures) {
    auto path=t.file.generic_u8string();
    result["textures"].push_back({{"id",t.id},{"file",std::string(path.begin(),path.end())},
      {"colorspace",t.colorspace==ir::ColorSpace::srgb?"srgb":"linear"}});
  }
  for(const auto &m:scene.materials)
    result["materials"].push_back({{"id",m.id},{"base_color",vec(m.base_color)},
      {"roughness",m.roughness},{"metallic",m.metallic},{"opacity",m.opacity},
      {"transmission",m.transmission},{"ior",m.ior},{"normal_strength",m.normal_strength},
      {"color_texture",m.color_texture},{"roughness_texture",m.roughness_texture},
      {"opacity_texture",m.opacity_texture},{"normal_texture",m.normal_texture},
      {"bump_texture",m.bump_texture},{"bump_strength",m.bump_strength},{"bump_distance",m.bump_distance}});
  for(const auto &mesh:scene.meshes) {
    J m={{"id",mesh.id},{"smooth",mesh.smooth},{"material_slots",mesh.material_slots},
         {"positions",J::array()},{"triangles",J::array()}};
    for(auto p:mesh.positions) m["positions"].push_back(vec(p));
    for(const auto &t:mesh.triangles) m["triangles"].push_back({{"vertices",t.vertices},
      {"material_slot",t.material_slot},{"uv",{{t.uv[0].x,t.uv[0].y},{t.uv[1].x,t.uv[1].y},{t.uv[2].x,t.uv[2].y}}}});
    result["meshes"].push_back(std::move(m));
  }
  for(const auto &i:scene.instances) result["instances"].push_back({{"id",i.id},{"mesh",i.mesh},
    {"transform",i.transform.value},{"materials",i.materials}});
  for(const auto &l:scene.lights) result["lights"].push_back({{"id",l.id},{"transform",l.transform.value},
    {"power",vec(l.power)},{"width",l.width},{"height",l.height}});
  return result;
}
}
