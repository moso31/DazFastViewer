static Document update_fixture(const fs::path &folder){
  auto d=fixture(folder);d.catalog.targets[0].character=true;d.catalog.targets[4].character=true;
  for(size_t t:{1,3,5}){d.loaded.scene.instances[t].graft_source=t==5?4:0;d.loaded.scene.meshes[t].graft_vertex_pairs={{{0,0}}};}
  auto &mesh=d.loaded.scene.meshes[3];mesh.material_slots.push_back("Inner");d.loaded.scene.instances[3].materials.push_back(d.loaded.scene.instances[3].materials[0]);
  for(size_t slot=1;slot<mesh.material_slots.size();++slot){auto face=mesh.triangles.front();face.material_slot=uint32_t(slot);mesh.triangles.push_back(face);}
  return d;
}
static J update_material(const Document &d,const Snapshot &s,size_t instance,size_t slot){return copy_material(d.loaded.scene,s.material_overrides,{instance,slot}).at("parameters");}
static void update_unit(const fs::path &folder){
  auto d=update_fixture(folder);auto s=initial_snapshot(d);s.material_overrides["object0"]["Body"]["roughness"]=.91;s.material_overrides["object0"]["Arms"]["roughness"]=.12;
  const auto other=update_material(d,s,5,0),hd=update_material(d,s,3,0);
  // 同一角色可以有同产品的独立附件，更新入口必须只处理指定行。
  auto duplicate=d.loaded.scene.instances[1];duplicate.id="duplicate";d.loaded.scene.instances.push_back(duplicate);
  auto object=d.loaded.objects[1];object.id="duplicate";object.instance=6;d.loaded.objects.push_back(object);
  auto target=d.catalog.targets[1];target.id="duplicate/geometry";target.instance=6;d.catalog.targets.push_back(target);
  const auto native=update_geograft_materials(d,s,1,{});
  check(native.updated==3&&native.fallback==0&&native.unresolved.empty(),"GP 专用更新遗漏表面或覆盖层");
  check(update_material(d,s,1,0)==update_material(d,s,0,0)&&update_material(d,s,5,0)==other&&update_material(d,s,3,0)==hd&&update_material(d,s,6,0)==other,"GP 更新串改角色或同类附件");
  const auto skin=update_geograft_materials(d,s,3,{}, {},"烘焙资源缺失");
  check(skin.updated==5&&skin.fallback==5&&skin.unresolved.empty(),"HD 通用回退没有覆盖五个表面");
  for(size_t slot:{0,2,3})check(update_material(d,s,3,slot)==update_material(d,s,0,0),"HD 身体映射错误");
  auto inner=update_material(d,s,0,0);inner["opacity"]=0;inner["opacity_texture"]=nullptr;
  check(update_material(d,s,3,4)==inner,"HD 回退把隐藏的 Inner 变成可见皮肤");
  check(update_material(d,s,3,1)==update_material(d,s,0,1),"HD Arms 未匹配当前手臂皮肤");
  d.loaded.scene.meshes[1].material_slots={"Genitalia","Unknown region"};d.loaded.objects[1].geometry_file=folder/"data/DAZ 3D/Genesis 8/Female Genitalia/mesh.dsf";
  const auto untouched=update_material(d,s,1,1);s.material_overrides["object0"]["Body"]["roughness"]=.23;
  const auto partial=update_geograft_materials(d,s,1,{});
  check(partial.updated==1&&partial.unresolved.size()==1&&partial.unresolved[0].starts_with("Unknown region")&&update_material(d,s,1,1)==untouched,"部分匹配没有保留成功结果与未匹配表面");
  auto missing=update_fixture(folder);auto state=initial_snapshot(missing);missing.loaded.scene.meshes[0].material_slots[0]="Missing body";
  const auto arms=update_geograft_materials(missing,state,3,{});
  check(arms.updated==1&&arms.fallback==1&&arms.unresolved.size()==4&&!arms.strategy_error.empty(),"专用策略失败或缺少身体材质时，没有继续可执行的 Arms 更新");
  auto hd_scene=update_fixture(folder);auto hd_state=initial_snapshot(hd_scene);const auto baked=folder/"cache/new";
  const auto applied=update_geograft_materials(hd_scene,hd_state,3,{folder},baked);
  check(applied.updated==5&&applied.fallback==0&&applied.unresolved.empty(),"缓存 HD 更新没有覆盖五个表面");
  auto &left=hd_scene.loaded.scene.materials.at(hd_scene.loaded.scene.instances[3].materials[2]);check(daz::material_uv_set(left).label=="HD UVs","HD 更新没有应用局部 UV");
  const auto local_uv=hd_scene.loaded.scene.meshes.at(hd_scene.loaded.scene.instances[3].mesh).triangles[2].uv;
  check(local_uv[0]==ir::Vec2{.2f,.2f},"HD 更新没有改变实际顶点 UV");
  check(update_material(hd_scene,hd_state,3,4).at("opacity")==0,"HD 专用策略把 Inner 变成可见皮肤");
  const auto base=folder/"data/3feetwolf/HD Nipples for G8F - 2.0/HD Nipples - 2.0/UV Sets/3feetwolf/Base/default.dsf";
  const auto hd_file=base.parent_path()/"HD UVs.dsf";
  change_material_uv(hd_scene,{hd_file.generic_string()+"#HD%20UVs",hd_file.generic_string(),"HD UVs"},{{3,0},{3,1},{3,4}});
  hd_state.material_overrides["object3"]["Inner"]["opacity"]=1;
  const auto corrected=update_geograft_materials(hd_scene,hd_state,3,{folder},baked);
  check(corrected.updated==5&&corrected.fallback==0&&corrected.unresolved.empty(),"纠正手动误设 UV 时未完成专用策略");
  for(size_t slot:{0,1,4}){
    const auto &scene=hd_scene.loaded.scene;const auto &instance=scene.instances[3];
    check(daz::material_uv_set(scene.materials.at(instance.materials[slot])).label=="default"&&scene.meshes.at(instance.mesh).triangles[slot].uv[0]==ir::Vec2{0,0},"外围表面未恢复实际基础 UV");
  }
  check(update_material(hd_scene,hd_state,3,4).at("opacity")==0&&hd_scene.loaded.scene.meshes.at(hd_scene.loaded.scene.instances[3].mesh).triangles[2].uv==local_uv,"修复 Inner 没有隐藏或破坏了左右 HD 坐标");
  fs::remove(base);
  const auto partial_uv=update_geograft_materials(hd_scene,hd_state,3,{}, {},"测试缺失基础 UV");
  check(partial_uv.updated==3&&partial_uv.unresolved.size()==2,"UV 恢复失败时没有保留其余三个成功表面");
  check(hd_scene.loaded.scene.meshes.at(hd_scene.loaded.scene.instances[3].mesh).triangles[2].uv==local_uv,"失败表面的 UV 被部分修改");
  // 即使原场景没有保存 UV baseline，也要用产品基础 UV 恢复整身纹理。
  std::ofstream(base)<<J{{"uv_set_library",J::array({{{"id","default"},{"vertex_count",3},{"uvs",{{"count",3},{"values",{{0,0},{1,0},{0,1}}}}}}})}};
  const auto fallback=update_geograft_materials(hd_scene,hd_state,3,{}, {},"测试回退");
  check(fallback.updated==5&&fallback.unresolved.empty(),"已应用 HD UV 后无法回退");
  check(daz::material_uv_set(hd_scene.loaded.scene.materials.at(hd_scene.loaded.scene.instances[3].materials[2])).label=="default","整身皮肤仍使用 HD 局部 UV");
  const auto base_uv=hd_scene.loaded.scene.meshes.at(hd_scene.loaded.scene.instances[3].mesh).triangles[2].uv;
  check(base_uv[0]==ir::Vec2{0,0}&&base_uv[1]==ir::Vec2{1,0}&&base_uv!=local_uv,"回退只改变 UV 名称，未恢复实际坐标");
  check(update_material(hd_scene,hd_state,3,2)==update_material(hd_scene,hd_state,0,0),"UV 恢复后没有完整复制皮肤");
  // 在专用策略已经写入部分皮肤与 UV 后故意失败，不能残留半成品。
  hd_state.material_overrides["object0"]["Body"]["roughness_texture"]={{"file",(folder/"missing-extra.png").generic_string()},{"colorspace",1},{"gamma",0},{"layers",J::array()}};
  const auto failed_native=update_geograft_materials(hd_scene,hd_state,3,{folder},baked);
  check(failed_native.updated==5&&failed_native.fallback==5&&!failed_native.strategy_error.empty(),"专用策略执行中失败没有回退");
  check(daz::material_uv_set(hd_scene.loaded.scene.materials.at(hd_scene.loaded.scene.instances[3].materials[2])).label=="default","失败的专用策略污染了回退 UV");
  HDBakeJob incomplete;incomplete.directory=baked;incomplete.identity=hd_texture_identity(hd_scene,hd_state,0);
  std::ofstream(baked/"dfv-hd-textures.json")<<J{{"identity",incomplete.identity},{"version",2}};
  check(find_hd_textures(hd_scene,hd_state,0,{},folder/"cache").empty(),"附加通道缺失的缓存仍被自动匹配");
  for(char side:{'L','R'})std::ofstream(baked/(std::string("Channel_roughness_texture_")+side+".png"))<<"channel";
  check(find_hd_textures(hd_scene,hd_state,0,{},folder/"cache")==std::vector<fs::path>{baked},"完整附加通道缓存未被匹配");
  rejects([&]{update_geograft_materials(d,s,0,{});},"角色本体被当作附件更新");
}
static void update_real(const fs::path &source){
  const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};
  std::cout<<"Loading "<<source.string()<<std::endl;auto loaded=load_scene_extension(source,roots,1);auto d=*loaded.document;auto s=loaded.snapshot;
  size_t host=d.catalog.targets.size();for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].label=="Seo Hyun G8.1F (2)")host=t;
  check(host<d.catalog.targets.size(),"7.dufex 缺少目标角色");const auto grafts=geograft_targets(d,host);check(grafts.size()==2,"目标角色没有两个 Geograft");
  const auto original=snapshot_json(d,s);const auto original_document=loaded.document;
  const auto host_instance=d.catalog.targets[host].instance;const auto host_id=d.loaded.scene.instances[host_instance].id;
  s.material_overrides[host_id]["Body"]["roughness"]=.456;
  J report={{"source",source.generic_string()},{"host",d.catalog.targets[host].label},{"updates",J::array()}};
  for(auto target:grafts){
    const auto result=update_geograft_materials(d,s,target,roots,{},geograft_material_strategy(d,target)==GeograftMaterialStrategy::hd_nipples?"验证整身皮肤回退":"");
    check(result.updated>0&&result.unresolved.empty(),"真实附件存在未更新部位");
    report["updates"].push_back({{"label",d.catalog.targets[target].label},{"updated",result.updated},{"fallback",result.fallback},{"strategy_error",result.strategy_error}});
    std::cout<<d.catalog.targets[target].label<<": "<<result.updated<<" surfaces, fallback="<<result.fallback<<std::endl;
  }
  check(snapshot_json(*original_document,loaded.snapshot)==original,"材质更新破坏撤销文档");
  const auto folder=fs::absolute("artifacts/geograft-update");fs::create_directories(folder);const auto file=folder/"updated.dufex";save_scene_extension(file,d,s);
  auto restored=load_scene_extension(file,roots,2);
  check(restored.document->loaded.scene.instances.size()==d.loaded.scene.instances.size(),"保存重载对象数变化");
  for(size_t i=0;i<d.loaded.scene.instances.size();++i)for(size_t slot=0;slot<d.loaded.scene.instances[i].materials.size();++slot){
    check(copy_material(restored.document->loaded.scene,restored.snapshot.material_overrides,{i,slot})==copy_material(d.loaded.scene,s.material_overrides,{i,slot}),"保存重载材质或 UV 不一致");
    if(i!=host_instance&&std::none_of(grafts.begin(),grafts.end(),[&](size_t t){return d.catalog.targets[t].instance==i;}))check(update_material(d,s,i,slot)==update_material(*original_document,loaded.snapshot,i,slot),"更新串改其他角色或附件");
  }
  d.loaded.scene.validate();report["status"]="PASS";std::ofstream(folder/"result.json")<<report.dump(2);
}
