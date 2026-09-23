#pragma once
#include "render_ir/options_json.h"
#include <nlohmann/json.hpp>

namespace dfv {
inline nlohmann::json scene_json(const ir::Scene &scene,int samples) {
  using J=nlohmann::json;
  auto vec=[](ir::Vec3 v) {return J::array({v.x,v.y,v.z});};
  J result={{"schema","dfv-material-reference-1"},{"coordinate_system","meters-Z-up"},
    {"environment",vec(scene.environment)},{"render_options",ir::options_json(scene.options)},
    {"camera",{{"transform",scene.camera.transform.value},{"width",scene.camera.width},
               {"height",scene.camera.height},{"fov_short_axis",scene.camera.fov}}},
    {"render",{{"samples",samples},{"seed",1337},{"max_bounces",8},{"diffuse_bounces",4},
               {"glossy_bounces",4},{"transmission_bounces",8},{"transparent_max_bounces",32},
               {"denoise",false},{"adaptive_sampling",false},{"working_space","linear-Rec709"}}}};
  for(const char *key:{"textures","materials","meshes","instances","lights"}) result[key]=J::array();
  for(const auto &t:scene.textures) {
    auto path=t.file.generic_u8string();
    result["textures"].push_back({{"id",t.id},{"file",std::string(path.begin(),path.end())},
      {"colorspace",t.colorspace==ir::ColorSpace::srgb?"srgb":"linear"},{"gamma",t.gamma},{"layers",J::array()}});
    for(const auto &l:t.layers) {const auto p=l.file.generic_u8string();result["textures"].back()["layers"].push_back({{"file",std::string(p.begin(),p.end())},{"operation",l.operation},{"opacity",l.opacity},{"color",vec(l.color)},{"invert",l.invert},{"rotation",l.rotation},{"scale",{l.scale.x,l.scale.y}},{"offset",{l.offset.x,l.offset.y}},{"mirror_x",l.mirror_x},{"mirror_y",l.mirror_y}});}
  }
  for(const auto &m:scene.materials)
    result["materials"].push_back({{"id",m.id},{"base_color",vec(m.base_color)},
      {"roughness",m.roughness},{"metallic",m.metallic},{"opacity",m.opacity},
      {"transmission",m.transmission},{"ior",m.ior},{"normal_strength",m.normal_strength},
      {"color_texture",m.color_texture},{"roughness_texture",m.roughness_texture},
      {"opacity_texture",m.opacity_texture},{"normal_texture",m.normal_texture},
      {"bump_texture",m.bump_texture},{"bump_strength",m.bump_strength},{"bump_distance",m.bump_distance},{"bump_invert",m.bump_invert},
      {"bump_from_texel_density",m.bump_from_texel_density},
      {"displacement_texture",m.displacement_texture},{"displacement_strength",m.displacement_strength},{"displacement_min_m",m.displacement_min},{"displacement_max_m",m.displacement_max},
      {"thin_walled",m.thin_walled},{"hair",m.hair},{"weighted_glossy",m.weighted_glossy},{"subsurface",m.subsurface},{"subsurface_radius",vec(m.subsurface_radius)},{"subsurface_anisotropy",m.subsurface_anisotropy},
      {"subsurface_color",vec(m.subsurface_color)},{"separate_subsurface_color",m.separate_subsurface_color},
      {"translucency",m.translucency},{"translucency_color",vec(m.translucency_color)},{"translucency_texture",m.translucency_texture},
      {"translucency_color_texture",m.translucency_color_texture},{"specular",m.specular},{"specular_texture",m.specular_texture},{"specular_color",vec(m.specular_color)},
      {"specular_color_texture",m.specular_color_texture},{"coat_color_texture",m.coat_color_texture},
      {"anisotropy",m.anisotropy},{"anisotropy_rotation",m.anisotropy_rotation},{"roughness_from_glossiness",m.roughness_from_glossiness},
      {"coat",m.coat},{"coat_roughness",m.coat_roughness},{"coat_ior",m.coat_ior},{"coat_color",vec(m.coat_color)},{"coat_texture",m.coat_texture},{"coat_roughness_texture",m.coat_roughness_texture},
      {"dual_weight",m.dual_weight},{"dual_texture",m.dual_texture},{"dual_ratio",m.dual_ratio},{"dual_roughness1",m.dual_roughness1},{"dual_roughness2",m.dual_roughness2},{"dual_specular",m.dual_specular},
      {"metallic_texture",m.metallic_texture},{"transmission_texture",m.transmission_texture},
      {"hair_root_radius",m.hair_root_radius},{"hair_tip_radius",m.hair_tip_radius},{"hair_tip_color",vec(m.hair_tip_color)},
      {"hair_radial_roughness",m.hair_radial_roughness},{"hair_melanin",m.hair_melanin},{"hair_redness",m.hair_redness},
      {"uv_scale",{m.uv_scale.x,m.uv_scale.y}},{"uv_offset",{m.uv_offset.x,m.uv_offset.y}}});
  for(const auto &mesh:scene.meshes) {
    J m={{"id",mesh.id},{"smooth",mesh.smooth},{"material_slots",mesh.material_slots},
         {"positions",J::array()},{"triangles",J::array()},{"curves",J::array()}};
    for(auto p:mesh.positions) m["positions"].push_back(vec(p));
    for(const auto &t:mesh.triangles) if(mesh.draws(t)) m["triangles"].push_back({{"vertices",t.vertices},
      {"material_slot",t.material_slot},{"uv",{{t.uv[0].x,t.uv[0].y},{t.uv[1].x,t.uv[1].y},{t.uv[2].x,t.uv[2].y}}}});
    for(const auto &c:mesh.curves) m["curves"].push_back({{"vertices",c.vertices},{"material_slot",c.material_slot},{"uv",{c.uv.x,c.uv.y}}});
    result["meshes"].push_back(std::move(m));
  }
  for(const auto &i:scene.instances) result["instances"].push_back({{"id",i.id},{"mesh",i.mesh},
    {"transform",i.transform.value},{"materials",i.materials},{"visible",i.visible},{"instance_group",i.instance_group},{"instance_label",i.instance_label}});
  for(const auto &l:scene.lights) result["lights"].push_back({{"id",l.id},{"transform",l.transform.value},
    {"power",vec(l.power)},{"width",l.width},{"height",l.height}});
  return result;
}
}
