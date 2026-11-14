static void material_preset_uv() {
  const auto folder=fs::temp_directory_path()/("dfv-preset-uv-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(folder/"data");
  auto source=J::parse(R"({"node_library":[{"id":"root","type":"node"}],"geometry_library":[{"id":"mesh","vertices":{"count":4,"values":[[0,0,0],[100,0,0],[100,100,0],[0,100,0]]},"polygon_material_groups":{"count":2,"values":["Lace","Cloth"]},"polylist":{"count":2,"values":[[0,0,0,1,2],[0,1,0,2,3]]},"default_uv_set":"#default-target"}],"uv_set_library":[{"id":"default-target","name":"default","label":"Default UVs","vertex_count":4,"uvs":{"count":4,"values":[[0,0],[1,0],[1,1],[0,1]]}}],"scene":{"nodes":[],"materials":[]}})");
  for(const auto *name:{"dress","other"}){
    const auto shape=std::string(name)+"Shape";
    source["scene"]["nodes"].push_back({{"id",name},{"url","#root"},{"geometries",J::array({{{"id",shape},{"url","#mesh"}}})}});
    for(const auto *slot:{"Lace","Cloth"})source["scene"]["materials"].push_back({{"id",std::string(name)+slot},{"geometry","#"+shape},{"groups",{slot}}});
  }
  const auto scene_file=folder/"scene.duf",preset_file=folder/"gray.duf",saved_file=folder/"saved.dufex";
  std::ofstream(scene_file)<<source;
  auto foreign=source["uv_set_library"][0];foreign["id"]="default-foreign";foreign["vertex_count"]=5;
  foreign["uvs"]["count"]=5;foreign["uvs"]["values"].push_back({.5,.5});
  auto custom=foreign;custom["id"]="custom";custom["name"]="Custom";
  auto compatible=source["uv_set_library"][0];compatible["id"]="compatible";compatible["uvs"]["values"][0]={.25,.75};
  auto malformed=compatible;malformed["id"]="malformed";malformed["polygon_vertex_indices"]={{0,3,0}};
  std::ofstream(folder/"data/foreign.dsf")<<J{{"uv_set_library",J::array({foreign,custom,compatible,malformed})}};
  const auto preset_json=J::parse(R"({"asset_info":{"type":"preset_material"},"material_library":[{"id":"Lace","diffuse":{"channel":{"id":"diffuse","value":[1,1,1]}}}],"scene":{"materials":[{"url":"#Lace","groups":["Lace"]}],"animations":[{"url":"name://@selection#materials/Lace:?diffuse/value","keys":[[0,[0.5,0.5,0.5]]]},{"url":"name://@selection#materials/Lace:?uv_set","keys":[[0,"data/foreign.dsf#default-foreign"]]}]}})");
  std::ofstream(preset_file)<<preset_json;
  auto d=load(scene_file,{folder});const auto pristine=d;auto state=editor::initial_snapshot(d);const auto preset=daz::load(preset_file,{{folder},false});
  const auto original_uv=daz::material_uv_set(d.loaded.scene.materials[d.loaded.scene.instances[0].materials[0]]);
  auto request=preset.scene.materials[0];auto ref=daz::material_uv_set(request);
  auto alternate=d;ref.uri="data/foreign.dsf#compatible";daz::set_material_uv_set(request,ref);
  require(daz::apply_material_uv(alternate.loaded.scene,0,0,request,{{folder},false}),"UV 夹具未切换到另一套坐标");
  d=alternate;
  require(editor::apply_materials(d,0,preset,&state)==1,"跨版本默认 UV 材质未应用");
  const auto &scene=d.loaded.scene;const auto &instance=scene.instances[0];const auto &material=scene.materials[instance.materials[0]];
  require(material.base_color==preset.scene.materials[0].base_color&&material.base_color!=pristine.loaded.scene.materials[0].base_color,"零帧灰色没有应用");
  require(daz::material_uv_set(material)==original_uv,"兼容后仍记录外来 UV 引用");
  require(scene.meshes[instance.mesh].triangles==pristine.loaded.scene.meshes[pristine.loaded.scene.instances[0].mesh].triangles,"兼容预设没有恢复目标默认 UV");
  require(editor::copy_material(scene,{}, {0,1})==editor::copy_material(pristine.loaded.scene,{}, {0,1}),"局部预设修改了其他表面");
  require(editor::copy_material(scene,{}, {1,0})==editor::copy_material(pristine.loaded.scene,{}, {1,0}),"预设修改了另一件服装");
  editor::save_scene_extension(saved_file,d,state);
  fs::remove(scene_file);fs::remove(preset_file);
  auto restored=editor::load_scene_extension(saved_file,{folder},2);
  require(editor::copy_material(restored.document->loaded.scene,restored.snapshot.material_overrides,{0,0})==editor::copy_material(scene,state.material_overrides,{0,0}),"跨版本材质保存后不能独立重放");
  const auto &restored_scene=restored.document->loaded.scene;
  require(restored_scene.meshes[restored_scene.instances[0].mesh].triangles==scene.meshes[instance.mesh].triangles,"重开后默认 UV 坐标变化");
  {
    auto strict=pristine;
    rejects([&]{daz::apply_material_uv(strict.loaded.scene,0,0,preset.scene.materials[0],{{folder},false});},"手动切换错误地接受不兼容 UV");
  }
  auto variant=[&](const char *id){auto p=preset;auto uv=daz::material_uv_set(p.scene.materials[0]);uv.uri=std::string("data/foreign.dsf#")+id;daz::set_material_uv_set(p.scene.materials[0],uv);return p;};
  {
    auto exact=pristine;require(editor::apply_materials(exact,0,variant("compatible"))==1,"兼容 UV 应用失败");
    require(exact.loaded.scene.meshes[exact.loaded.scene.instances[0].mesh].triangles[0].uv[0]==ir::Vec2{.25f,.75f},"兼容外来 UV 被目标默认 UV 覆盖");
  }
  for(const auto *id:{"custom","malformed"}){auto rejected=pristine;rejects([&]{editor::apply_materials(rejected,0,variant(id));},"专用或损坏 UV 被默认 UV 隐藏");}
  auto undeclared=pristine;undeclared.loaded.objects.clear();
  rejects([&]{editor::apply_materials(undeclared,0,preset);},"没有几何声明仍猜测默认 UV");
  auto incompatible_geometry=pristine;std::swap(incompatible_geometry.loaded.scene.meshes[0].polygons[0].vertices[0],incompatible_geometry.loaded.scene.meshes[0].polygons[0].vertices[1]);
  rejects([&]{editor::apply_materials(incompatible_geometry,0,preset);},"默认 UV 回退绕过几何连接关系校验");
}

static void material_preset_roundtrip(const fs::path &scene_file,const fs::path &preset_file,const fs::path &output,const std::vector<fs::path> &roots) {
  auto d=load(scene_file,roots,true);auto state=editor::initial_snapshot(d);const auto before=d.loaded.scene;
  const auto preset=daz::load(preset_file,{roots,false});const auto applied=editor::apply_materials(d,0,preset,&state);
  require(applied>0,"实际材质未匹配服装表面");d.loaded.scene.validate();
  fs::create_directories(output.parent_path());auto saved_file=output;saved_file.replace_extension("dufex");
  editor::save_scene_extension(saved_file,d,state);auto restored=editor::load_scene_extension(saved_file,roots,2);
  const auto &scene=d.loaded.scene;const auto &reopened=restored.document->loaded.scene;
  J report={{"status","PASS"},{"applied_surfaces",applied},{"scene",scene_file.generic_string()},{"preset",preset_file.generic_string()},{"saved",saved_file.generic_string()},{"surfaces",J::array()}};
  for(size_t i=0;i<scene.instances.size();++i){const auto &instance=scene.instances[i];
    require(scene.meshes[instance.mesh].triangles==reopened.meshes[reopened.instances[i].mesh].triangles,"实际服装重开后 UV 变化");
    for(size_t slot=0;slot<instance.materials.size();++slot){
      require(editor::copy_material(scene,state.material_overrides,{i,slot})==editor::copy_material(reopened,restored.snapshot.material_overrides,{i,slot}),"实际服装重开后材质变化");
      const auto &m=scene.materials[instance.materials[slot]];const auto &old=before.materials[before.instances[i].materials[slot]];
      auto textures=J::array();for(const auto *index:ir::texture_indices(m))if(*index>=0){const auto &path=scene.textures.at(*index).file;require(fs::is_regular_file(path),"实际服装材质贴图丢失");textures.push_back(path.generic_string());}
      report["surfaces"].push_back({{"slot",scene.meshes[instance.mesh].material_slots[slot]},{"base_color",{m.base_color.x,m.base_color.y,m.base_color.z}},{"opacity",m.opacity},{"uv",daz::material_uv_set(m).uri},{"original_uv",daz::material_uv_set(old).uri},{"textures",textures}});
    }
  }
  std::ofstream(output)<<report.dump(2);std::cout<<report.dump(2)<<std::endl;
}
