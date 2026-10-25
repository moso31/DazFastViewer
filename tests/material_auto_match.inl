static void material_auto_match() {
  editor::Document d;auto &scene=d.loaded.scene;ir::Material old;old.id="old";scene.materials={old};
  auto add=[&](std::string id,std::vector<std::string> slots,std::string parent={}) {
    ir::Mesh mesh;mesh.material_slots=slots;mesh.positions={{0,0,0},{1,0,0},{0,1,0}};mesh.triangles={{{0,1,2}}};
    ir::Instance instance;instance.id=id;instance.mesh=uint32_t(scene.meshes.size());instance.materials.assign(slots.size(),0);
    runtime::Target target;target.id=id+"/geometry";target.instance=uint32_t(scene.instances.size());target.parent=parent;
    if(!parent.empty())target.ancestors={parent};else target.character=true;
    scene.meshes.push_back(mesh);scene.instances.push_back(instance);d.catalog.targets.push_back(target);
  };
  add("body",{"Skin"});add("top",{"Shirt","Trim"},"#body");add("shorts",{"Shorts","Trim","Belt","Ribbon"},"#body");
  add("other-body",{"Skin"});add("other-top",{"Shirt","Trim"},"#other-body");
  daz::LoadedScene preset;ir::Material fresh;fresh.id="fresh";fresh.base_color={.2f,.3f,.4f};preset.scene.materials={fresh,fresh};
  preset.report={{"materials",J::array({{{"groups",{"Shirt"}}},{{"groups",{"Trim"}}}})}};
  auto unchanged=[&](size_t t){for(auto m:d.loaded.scene.instances[t].materials)require(d.loaded.scene.materials[m].base_color==old.base_color,"自动材质修改了无关对象");};
  require(editor::apply_materials(d,0,preset)==2,"角色未自动匹配上衣整份材质");unchanged(0);unchanged(2);unchanged(3);unchanged(4);
  preset.scene.materials.resize(4,fresh);preset.report["materials"]=J::array();for(const auto *group:{"Shorts","Trim","Belt","Ribbon"})preset.report["materials"].push_back({{"groups",{group}}});
  require(editor::apply_materials(d,0,preset)==4,"角色未自动匹配短裤整份材质");unchanged(4);
  preset.scene.materials.resize(1);preset.report["materials"]=J::array({{{"groups",{"Trim"}}}});
  const auto before=d.loaded.scene.materials;rejects([&]{editor::apply_materials(d,0,preset);},"共有 Trim 表面歧义被静默套给多件服装");
  require(d.loaded.scene.materials.size()==before.size(),"歧义失败仍修改材质");
  require(editor::apply_materials(d,1,preset)==1,"显式选择服装不能应用局部表面材质");
  preset.report["materials"][0]["groups"]={"Missing"};rejects([&]{editor::apply_materials(d,0,preset);},"未穿戴对应服装时仍套用材质");
  preset.report["materials"][0]["groups"]={"*"};require(editor::apply_materials(d,0,preset)==1,"通用 Shader 不再作用于当前选择");
}
