static void outfit_copy_tests(const fs::path &folder,const editor::Document &original,const J &base,const J &preset) {
  auto clothing_asset=base;clothing_asset["node_library"][0]["presentation"]={{"type","Follower/Wardrobe/Shirt"},{"preferred_base","/Genesis 8.1/Female"}};
  fs::create_directories(folder/"data/Wear/Morphs");
  std::ofstream(folder/"data/Wear/Morphs/shirt-shape.dsf")<<R"({"modifier_library":[{"id":"ShirtShape","parent":"/data/Wear/shirt.dsf#mesh","channel":{"type":"float","value":0,"min":0,"max":1},"morph":{"vertex_count":3,"deltas":{"count":1,"values":[[1,10,0,0]]}}}]})";
  std::ofstream(folder/"data/Wear/shirt.dsf")<<clothing_asset;
  auto shirt=preset;shirt["scene"]["nodes"][0]["url"]="/data/Wear/shirt.dsf#figure";shirt["scene"]["nodes"][0]["geometries"][0]["url"]="/data/Wear/shirt.dsf#mesh";
  shirt["scene"]["nodes"][2]["parent"]="#cloth";shirt["scene"]["nodes"].erase(1);std::ofstream(folder/"shirt.duf")<<shirt;
  auto d=original;d.generation=7;d.loaded.objects[1].auto_fit_base="/Genesis 8/Female";d.loaded.objects[1].extended_bases.clear();
  editor::append_document(d,load(folder/"shirt.duf",{folder},true));editor::attach_import(d,2,0);
  require(editor::outfit_characters(d)==std::vector<size_t>({0,1}),"服装和饰品被误列为角色");
  require(editor::outfit_clothing(d,0)==std::vector<size_t>({2,3}),"没有按角色查找全部穿戴和附件");
  require(editor::outfit_roots(d,0)==std::vector<size_t>{2},"服装的子部件被拆成独立穿戴选项");
  {
    auto partial=d;auto values=editor::initial_snapshot(partial);
    require(editor::copy_outfit(partial,values,0,{3},{1}).copied==1,"不能单独复制普通附件");
    const auto count=partial.catalog.targets.size();
    require(editor::copy_outfit(partial,values,0,{2},{1}).copied==1&&partial.catalog.targets.size()==count+1,"主体复制重复创建了目标已有的附属单件");
    evaluate(partial,values);
  }
  auto snapshot=editor::initial_snapshot(d);snapshot.values[0].transform.translation_cm.x=35;snapshot.values[1].transform.translation_cm.x=-20;
  require(!snapshot.values[2].morphs.empty(),"服装 Morph 夹具为空");snapshot.values[2].morphs[0]=.25f;snapshot.poses[1][1].rotation_degrees.z=-43;
  const auto shirt_instance=d.loaded.scene.instances.at(d.catalog.targets[2].instance).id;snapshot.material_overrides[shirt_instance]["Skin"]["roughness"]=.72;
  const auto old=evaluate(d,snapshot);const auto old_properties=snapshot.values;const auto first=d.catalog.targets.size();
  const auto copied=editor::copy_outfit(d,snapshot,0,{2},{1});
  require(copied.copied==1&&copied.skipped==0&&d.catalog.targets.size()==first+2,"单件服装及附属饰品未复制完整");
  require(editor::attachment_host(d,first)==1&&editor::attachment_host(d,first+1)==1,"新服装没有绑定目标角色");
  require(snapshot.values[first].morphs==snapshot.values[2].morphs,"复制丢失服装 Morph 设置");
  require(snapshot.material_overrides.at(d.loaded.scene.instances[d.catalog.targets[first].instance].id)==snapshot.material_overrides.at(shirt_instance),"复制丢失当前服装材质修改");
  for(size_t t=0;t<old_properties.size();++t)require(snapshot.values[t]==old_properties[t],"复制修改了已有对象属性");
  auto scene=evaluate(d,snapshot);require(distance(point(old,0),point(scene,0))==0&&distance(point(old,2),point(scene,2))==0,"复制移动了来源角色或原服装");
  const auto original_mesh=d.loaded.scene.instances[d.catalog.targets[2].instance].mesh,copied_mesh=d.loaded.scene.instances[d.catalog.targets[first].instance].mesh;
  require(original_mesh!=copied_mesh,"复制服装共享可变几何");
  const auto original_mat=d.loaded.scene.instances[d.catalog.targets[2].instance].materials[0],copied_mat=d.loaded.scene.instances[d.catalog.targets[first].instance].materials[0];require(original_mat!=copied_mat,"复制服装共享可变材质");
  snapshot.values[first].morphs[0]=0;scene=evaluate(d,snapshot);
  auto reference=original;reference.generation=7;reference.loaded.objects[1].auto_fit_base="/Genesis 8/Female";reference.loaded.objects[1].extended_bases.clear();editor::append_document(reference,load(folder/"shirt.duf",{folder},true));editor::attach_import(reference,2,1);auto reference_state=editor::initial_snapshot(reference);reference_state.values[0]=snapshot.values[0];reference_state.values[1]=snapshot.values[1];reference_state.poses[0]=snapshot.poses[0];reference_state.poses[1]=snapshot.poses[1];auto reference_scene=evaluate(reference,reference_state);
  if(distance(point(reference_scene,reference.catalog.targets[2].instance),point(scene,d.catalog.targets[first].instance))>=1e-5)std::cerr<<"复制 / 直接穿戴坐标："<<J({point(scene,d.catalog.targets[first].instance).x,point(scene,d.catalog.targets[first].instance).y,point(scene,d.catalog.targets[first].instance).z})<<" / "<<J({point(reference_scene,reference.catalog.targets[2].instance).x,point(reference_scene,reference.catalog.targets[2].instance).y,point(reference_scene,reference.catalog.targets[2].instance).z})<<'\n';
  for(size_t v=0;v<3;++v)require(distance(point(reference_scene,reference.catalog.targets[2].instance,v),point(scene,d.catalog.targets[first].instance,v))<1e-5,"G8.1 复制穿戴与直接穿戴到 G8 的结果不一致");
  auto &new_object=*std::find_if(d.loaded.objects.begin(),d.loaded.objects.end(),[&](const auto &o){return o.instance==d.catalog.targets[first].instance;});new_object.label="改名后的衣服";d.catalog.targets[first].label=new_object.label;
  const auto count=d.catalog.targets.size(),operations=d.operations.size();const auto duplicate=editor::copy_outfit(d,snapshot,0,{2},{1,1});require(duplicate.copied==0&&duplicate.skipped==1&&d.catalog.targets.size()==count&&d.operations.size()==operations,"改名或重复目标绕过了单件查重");
  const auto saved=folder/"outfit.dufex";editor::save_scene_extension(saved,d,snapshot);auto restored=editor::load_scene_extension(saved,{folder},19);
  require(editor::snapshot_json(*restored.document,restored.snapshot)==editor::snapshot_json(d,snapshot),"服装复制保存重开丢失状态");
  const auto expected=evaluate(d,snapshot),actual=evaluate(*restored.document,restored.snapshot);for(size_t i=0;i<expected.instances.size();++i)require(distance(point(expected,i),point(actual,i))<1e-5,"服装复制重放后位置变化");
  auto multiple=d;const auto new_host=multiple.catalog.targets.size();editor::append_document(multiple,original);auto multiple_state=editor::initial_snapshot(multiple);
  const auto multi_result=editor::copy_outfit(multiple,multiple_state,0,{2},{1,new_host});require(multi_result.copied==1&&multi_result.skipped==1,"多目标复制没有逐角色查重");evaluate(multiple,multiple_state);
  auto backward=original;backward.generation=8;backward.loaded.objects[1].auto_fit_base="/Genesis 8/Female";
  clothing_asset["node_library"][0]["presentation"]["preferred_base"]="/Genesis 8/Female";std::ofstream(folder/"data/Wear/shirt.dsf")<<clothing_asset;
  editor::append_document(backward,load(folder/"shirt.duf",{folder},true));editor::attach_import(backward,2,1);auto backward_state=editor::initial_snapshot(backward);
  require(editor::copy_outfit(backward,backward_state,1,{2},{0}).copied==1,"G8 服装不能复制给 G8.1 角色");evaluate(backward,backward_state);
  auto geograft=original;geograft.generation=9;editor::append_document(geograft,load(folder/"wear.duf",{folder},true));editor::attach_import(geograft,2,0);
  require(editor::outfit_clothing(geograft,0)==std::vector<size_t>{3},"Geograft 未排除，或普通骨骼挂件被漏掉");
  auto rigid_state=editor::initial_snapshot(geograft);const auto rigid_first=geograft.catalog.targets.size();
  require(editor::copy_outfit(geograft,rigid_state,0,{3},{1}).copied==1,"骨骼挂件不能单独复制");
  auto rigid_scene=evaluate(geograft,rigid_state);
  require(editor::attachment_host(geograft,rigid_first)==1,"复制挂件仍依赖来源角色");
  require(distance(point(rigid_scene,geograft.catalog.targets[1].instance),point(rigid_scene,geograft.catalog.targets[rigid_first].instance))<1e-5,"骨骼挂件复制到新角色后位置错误");
  {
    J shell;std::ifstream(folder/"body.duf")>>shell;
    shell["geometry_library"]=J::parse(R"([{"id":"shell","vertices":{"count":0,"values":[]},"polylist":{"count":0,"values":[]},"polygon_material_groups":{"count":0,"values":[]},"extra":[{"type":"studio/geometry/shell"}]}])");
    shell["scene"]["nodes"].push_back(J::parse(R"({"id":"overlay","parent":"#body0","geometries":[{"id":"overlay-shape","url":"#shell"}],"extra":[{"type":"studio/node/shell"},{"type":"studio_node_channels","channels":[{"channel":{"id":"Shell Node","node":"#body0"}}]}]})"));
    std::ofstream(folder/"body-shell.duf")<<shell;
    auto shells=load(folder/"body-shell.duf",{folder});auto values=editor::initial_snapshot(shells);
    require(editor::outfit_clothing(shells,0)==std::vector<size_t>{2},"Geometry Shell 被错误排除");
    require(editor::copy_outfit(shells,values,0,{2},{1}).copied==1,"Geometry Shell 复制失败");
    require(shells.loaded.scene.instances[shells.catalog.targets[3].instance].shell_source==int(shells.catalog.targets[1].instance),"复制的 Shell 仍绑定来源角色");evaluate(shells,values);
  }
  std::cout<<"复制穿搭：G8 / G8.1 双向绑定、Morph、独立材质和网格、附属物、查重及保存重开：PASS\n";
}
