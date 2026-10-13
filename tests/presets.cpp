#include "editor/presets.h"
#include "editor/scene_extension.h"
#include "daz/pose.h"
#include <fstream>
#include <iostream>
using namespace dfv;using namespace dfv::editor;using J=nlohmann::json;namespace fs=std::filesystem;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
int main(){const auto folder=fs::temp_directory_path()/"dfv-preset-test";fs::create_directories(folder);try{
  const auto file=folder/"source.duf";J native=J::parse(R"({"asset_info":{"type":"scene"},"node_library":[{"id":"figure","type":"figure"},{"id":"bone","name":"test bone","type":"bone","parent":"#figure"}],"geometry_library":[{"id":"mesh","vertices":{"count":4,"values":[[0,0,0],[100,0,0],[100,100,0],[0,100,0]]},"polygon_material_groups":{"count":2,"values":["Skin","Toenails"]},"polylist":{"count":2,"values":[[0,0,0,1,2],[0,1,0,2,3]]},"default_uv_set":"#uv"}],"uv_set_library":[{"id":"uv","vertex_count":4,"uvs":{"count":4,"values":[[0,0],[1,0],[1,1],[0,1]]}}],"modifier_library":[{"id":"skin","skin":{"node":"#figure","geometry":"#mesh","vertex_count":4,"joints":[{"node":"#bone","node_weights":{"count":4,"values":[[0,1],[1,1],[2,1],[3,1]]}}]}},{"id":"Raise","name":"Raise Shape","parent":"#mesh","channel":{"id":"value","type":"float","value":0,"min":-1,"max":1},"morph":{"vertex_count":4,"deltas":{"count":1,"values":[[1,20,0,0]]}}}],"scene":{"nodes":[{"id":"figure","url":"#figure","geometries":[{"id":"shape","url":"#mesh"}]},{"id":"bone","url":"#bone","parent":"#figure"}],"modifiers":[{"id":"raise","url":"#Raise","parent":"#figure","channel":{"current_value":0.1}}],"materials":[{"id":"skin","geometry":"#shape","groups":["Skin"],"diffuse":{"channel":{"value":[0.2,0.4,0.6]}}},{"id":"nails","geometry":"#shape","groups":["Toenails"],"diffuse":{"channel":{"value":[0.7,0.2,0.1]}},"extra":[{"type":"studio/material/uber_iray"},{"type":"studio_material_channels","channels":[{"channel":{"id":"Refraction Weight","value":0.1}},{"channel":{"id":"Refraction Color","value":[1,1,1]}},{"channel":{"id":"Thin Walled","value":true}}]}]}]}})");
  {std::ofstream stream(file);stream<<native.dump();}Document d;d.source_file=file;d.loaded=daz::load(file,{{folder},false});d.catalog=daz::discover_morphs(d.loaded,{folder},{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);auto s=initial_snapshot(d);
  auto &scene=d.loaded.scene;check(d.catalog.targets.size()==1&&d.skeletons.skins.size()==1,"测试角色加载失败");
  const auto &nails=scene.materials.at(scene.instances[0].materials[1]);check(nails.base_color.x<.5f&&nails.base_color.y<.05f&&nails.refraction_color.x==1&&nails.separate_refraction,"部分折射抹掉了趾甲颜色");
  const auto texture=folder/"texture with spaces.ppm";{std::ofstream image(texture);image<<"P3\n1 1\n255\n120 30 20\n";}
  scene.textures.push_back({"test-texture",texture});scene.materials[scene.instances[0].materials[0]].color_texture=int(scene.textures.size()-1);
  const auto instance=scene.instances[0].id;s.material_overrides[instance]["Skin"]["base_color"]=J::array({2.,.3,.1});s.material_overrides[instance]["Skin"]["bump_distance"]=3.;s.material_overrides[instance]["Skin"]["bump_from_texel_density"]=false;s.material_overrides[instance]["Toenails"]["roughness"]=.67;
  const auto a=copy_material(scene,s.material_overrides,{0,0}),b=copy_material(scene,s.material_overrides,{0,1});const auto saved=folder/"材料 preset.duf";save_preset(saved,d,s,0,PresetKind::material);
  const auto encoded=daz::read_document_file(saved);check(encoded.at("asset_info").at("type")=="preset_material"&&encoded.at("scene").at("materials").size()==2,"材质未保存整对象表面");check(encoded.at("uv_set_library").size()==1,"内嵌 UV 没有保存或未去重");
  check(encoded.at("scene").at("materials")[0].at("diffuse").at("channel").at("image_file")=="/texture%20with%20spaces.ppm","贴图未保存为内容库相对路径");
  check(encoded.at("material_library")[0].at("id")!=encoded.at("scene").at("materials")[0].at("id"),"原生 DUF 定义与实例 ID 重复");
  auto preset=daz::load(saved,{{folder},false});s.material_overrides[instance]["Skin"]["opacity"]=.25;apply_materials(d,0,preset,&s);
  check(copy_material(d.loaded.scene,s.material_overrides,{0,0}).at("parameters")==a.at("parameters")&&copy_material(d.loaded.scene,s.material_overrides,{0,1}).at("parameters")==b.at("parameters"),"材质保存应用未精确还原当前参数");
  save_scene_extension(folder/"roundtrip.dufex",d,s);fs::remove(saved);auto restored=load_scene_extension(folder/"roundtrip.dufex",{folder},2);check(copy_material(restored.document->loaded.scene,restored.snapshot.material_overrides,{0,1}).at("parameters")==b.at("parameters"),"源预设删除后 DUFEX 丢失材质");
  s.poses[0][1].rotation_degrees={21,-12,38};s.poses[0][1].translation_cm={3,4,5};s.values[0].morphs[0]=.75f;
  const auto pose=folder/"pose.duf",morph=folder/"morph.duf";save_preset(pose,d,s,0,PresetKind::pose);save_preset(morph,d,s,0,PresetKind::morph);
  auto initial=initial_snapshot(d);auto applied=daz::apply_pose(daz::read_pose(pose),d.skeletons.skins[0],initial.poses[0],d.catalog.targets[0],initial.values[0]);
  check(applied.joints==s.poses[0]&&applied.properties.morphs==initial.values[0].morphs&&applied.report.at("fully_applied")==true,"Pose 骨骼回读失败或改动 Morph");
  auto shaped=daz::apply_pose(daz::read_pose(morph),d.skeletons.skins[0],applied.joints,d.catalog.targets[0],initial.values[0]);check(shaped.properties.morphs==s.values[0].morphs&&shaped.joints==applied.joints&&shaped.report.at("fully_applied")==true,"Morph 回读失败或改动姿势");
  s.values[0].morphs[0]=0;save_preset(morph,d,s,0,PresetKind::morph);auto zero=daz::apply_pose(daz::read_pose(morph),d.skeletons.skins[0],shaped.joints,d.catalog.targets[0],shaped.properties);check(zero.properties.morphs[0]==0,"零值 Morph 未保存，不能重置已有形态");
  bool protected_source=false;try{save_preset(file,d,s,0,PresetKind::pose);}catch(const std::exception &){protected_source=true;}check(protected_source&&daz::read_document_file(file)==native,"预设覆盖了源场景");
  auto duplicate=d.catalog.targets[0].morphs[0];duplicate.id+="-other-asset";d.catalog.targets[0].morphs.push_back(duplicate);s.values[0].morphs.push_back(.42f);
  save_preset(morph,d,s,0,PresetKind::morph);auto unset=s.values[0];unset.morphs={.9f,.8f};auto distinct=daz::apply_pose(daz::read_pose(morph),d.skeletons.skins[0],s.poses[0],d.catalog.targets[0],unset);
  check(distinct.properties.morphs==s.values[0].morphs&&distinct.report.at("fully_applied")==true,"同名的不同 Morph 资产未按身份恢复");
  std::cout<<"预设材质 / UV / Pose / Morph 往返：PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
