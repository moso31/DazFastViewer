static void uv_unit(const fs::path &folder){
  auto d=update_fixture(folder);auto s=initial_snapshot(d);auto &object=d.loaded.objects[1];
  object.geometry_file=folder/"data/DAZ 3D/Genesis 8/Female Genitalia/graft.dsf";object.geometry_id="graft";
  const auto uv_root=object.geometry_file.parent_path()/"UV Sets";
  auto write_uv=[&](const fs::path &relative,const std::string &name,float offset,int vertices=3){
    const auto file=uv_root/relative;fs::create_directories(file.parent_path());
    std::ofstream(file)<<J{{"uv_set_library",J::array({{{"id",name},{"label",name=="default"?"Default UVs":name},{"vertex_count",vertices},{"uvs",{{"count",3},{"values",{{offset,0},{offset+.1f,0},{offset,.1f}}}}}}})}};
    return daz::MaterialUVSet{file.generic_string()+"#"+name,file.generic_string(),name=="default"?"Default UVs":name};
  };
  const auto defaults=write_uv("DAZ 3D/Base/default.dsf","default",0);
  const auto generic=write_uv("DAZ 3D/Base/Base Female.dsf","Base Female",.1f);
  const auto keicy=write_uv("Hamster/Keicy/HS Keicy.dsf","HS Keicy",.2f);
  const auto lurys=write_uv("Hamster/Lurys/HS Keicy.dsf","HS Keicy",.3f);
  const auto licy=write_uv("Hamster/HS Licy/HS Keicy.dsf","HS Keicy",.4f);
  const auto arm=write_uv("Artist/Arms/Arm Detail.dsf","Arm Detail",.5f);
  write_uv("Artist/Wrong/Wrong.dsf","Wrong",.6f,8);
  std::ofstream(object.geometry_file)<<J{{"geometry_library",J::array({{{"id","graft"},{"vertices",{{"count",3},{"values",{{0,0,0},{1,0,0},{0,1,0}}}}},{"polylist",{{"count",2},{"values",{{0,0,0,1,2},{0,1,0,1,2}}}}},{"default_uv_set",defaults.uri}}})}};
  auto &mesh=d.loaded.scene.meshes[1];mesh.material_slots={"Genitalia","Arms"};mesh.source_polygon_count=2;mesh.polygons.resize(2);mesh.triangles.resize(2);
  for(size_t i=0;i<2;++i){mesh.polygons[i].vertices={0,1,2};mesh.polygons[i].uv={{.9f,.9f},{.9f,.9f},{.9f,.9f}};mesh.polygons[i].material_slot=uint32_t(i);mesh.triangles[i].vertices={0,1,2};mesh.triangles[i].uv={{{.9f,.9f},{.9f,.9f},{.9f,.9f}}};mesh.triangles[i].source_polygon=uint32_t(i);mesh.triangles[i].material_slot=uint32_t(i);}
  auto set_source_uv=[&](size_t slot,const std::string &name){auto &scene=d.loaded.scene;daz::set_material_uv_set(scene.materials.at(scene.instances[0].materials[slot]),{(folder/"host.dsf").generic_string()+"#"+name,(folder/"host.dsf").generic_string(),name});};
  auto set_texture=[&](const std::string &path){s.material_overrides["object0"]["Body"]["color_texture"]={{"file",(folder/"Runtime/Textures"/path).generic_string()},{"colorspace",0},{"gamma",0},{"layers",J::array()}};};
  auto selected=[&](size_t slot){const auto &scene=d.loaded.scene;return daz::material_uv_set(scene.materials.at(scene.instances[1].materials[slot]));};
  auto update=[&]{const auto result=update_geograft_materials(d,s,1,{folder});check(result.updated==2&&result.unresolved.empty(),"逐表面复制及自动 UV 选择未完成");};
  set_source_uv(0,"Base Female");set_source_uv(1,"Arm Detail");set_texture("Hamster/Keicy/HS_Keicy_torso.jpg");
  const auto original=d;update();
  check(selected(0).owner==daz::entry_key(fs::u8path(keicy.owner))&&selected(0).label=="HS Keicy","未按当前皮肤来源区分同名 UV，或 Base Female 覆盖了专用皮肤 UV");
  check(selected(1).label=="Arm Detail","不同来源表面没有分别选择 UV");
  const auto &updated_mesh=d.loaded.scene.meshes.at(d.loaded.scene.instances[1].mesh);
  check(updated_mesh.triangles[0].uv[0].x==.2f&&updated_mesh.polygons[0].uv[0].x==.2f&&updated_mesh.triangles[1].uv[0].x==.5f,"自动 UV 只修改显示属性，没有更新实际坐标");
  check(original.loaded.scene.meshes[1].triangles[0].uv[0].x==.9f&&update_material(d,s,5,0)==update_material(original,s,5,0),"自动 UV 修改了撤销副本或其他角色");
  const auto same=d.loaded.scene.meshes.at(d.loaded.scene.instances[1].mesh).triangles;update();check(d.loaded.scene.meshes.at(d.loaded.scene.instances[1].mesh).triangles==same,"重复更新导致 UV 漂移");
  set_texture("Hamster/HS Licy/HS_Licy_torso.jpg");update();check(selected(0).owner==daz::entry_key(fs::u8path(licy.owner)),"切换皮肤后沿用了上一个同名 UV");
  set_source_uv(0,"Base 8.1 Female");set_texture("Unknown/Unknown/body.jpg");update();check(selected(0).label=="Default UVs"&&d.loaded.scene.meshes.at(d.loaded.scene.instances[1].mesh).triangles[0].uv[0].x==0,"无法判断时没有恢复附件真实默认 UV");
  set_source_uv(0,"Base Female");update();check(selected(0).label=="Base Female","没有匹配源表面正在使用的基础 UV");
  set_source_uv(0,"HS Keicy");update();check(selected(0).label=="Default UVs","同名 UV 存在歧义时没有回退默认值");
  set_texture("Hamster/Lurys/HS_Lurys_torso.jpg");update();check(selected(0).owner==daz::entry_key(fs::u8path(lurys.owner)),"同名源 UV 没有结合当前贴图区分产品");
  set_source_uv(0,"Wrong");set_texture("Artist/Wrong/Wrong_body.jpg");update();check(selected(0).label=="Default UVs","采用了与附件拓扑不兼容的 UV");
  set_source_uv(0,"Base 8.1 Female");set_texture("Chinese/中文皮肤/躯干.jpg");update();check(selected(0).label=="Default UVs","无法识别的中文贴图路径未安全回退");
}
static void uv_real(const fs::path &source){
  const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};
  std::cout<<"Loading genitalia UV scene"<<std::endl;auto loaded=load_scene_extension(source,roots,1);auto &d=*loaded.document;auto &s=loaded.snapshot;J report={{"updates",J::array()}};std::vector<size_t> updated;
  for(size_t target=0;target<d.catalog.targets.size();++target){
    const auto instance=d.catalog.targets[target].instance;bool genitalia=false;for(const auto &o:d.loaded.objects)if(o.instance==instance&&daz::entry_key(o.geometry_file).find("/genesis 8/female genitalia/")!=std::string::npos)genitalia=true;if(!genitalia)continue;
    const auto host=attachment_host(d,target);const auto host_instance=d.catalog.targets[host].instance;const auto &body_slots=d.loaded.scene.meshes.at(d.loaded.scene.instances[host_instance].mesh).material_slots;const auto body=std::find(body_slots.begin(),body_slots.end(),"Body");check(body!=body_slots.end(),"真实角色没有 Body");
    const auto skin=copy_material(d.loaded.scene,s.material_overrides,{host_instance,size_t(body-body_slots.begin())});const auto texture=skin.at("parameters").at("color_texture").at("file").get<std::string>();
    const auto result=update_geograft_materials(d,s,target,roots);check(result.updated==1&&result.unresolved.empty(),"真实 Genitalia 皮肤 UV 更新失败");
    const auto &object=d.loaded.scene.instances[instance];const auto uv=daz::material_uv_set(d.loaded.scene.materials.at(object.materials[0]));
    if(texture.find("/Hamster/Keicy/")!=std::string::npos)check(uv.label=="HS Keicy"&&uv.owner.find("/hamster/keicy/")!=std::string::npos,"Keicy 皮肤没有匹配到所属产品的 UV");
    else if(texture.find("/HerYun/Yujin/")!=std::string::npos)check(uv.label=="Default UVs","缺少 Yujin 专用 UV 时未回退默认值");
    else throw std::runtime_error("真实场景的测试皮肤已改变，请检查预期 UV");
    check(copy_material(d.loaded.scene,s.material_overrides,{instance,0}).at("parameters")==skin.at("parameters"),"选择 UV 时修改了应复制的皮肤通道");
    updated.push_back(instance);report["updates"].push_back({{"host",d.catalog.targets[host].label},{"graft",d.catalog.targets[target].label},{"texture",texture},{"uv",uv.label},{"uri",uv.uri}});std::cout<<d.catalog.targets[host].label<<" -> "<<uv.label<<std::endl;
  }
  check(updated.size()==2,"7.dufex 的 Female Genitalia 数量变化");const auto folder=fs::absolute("artifacts/geograft-uv");fs::create_directories(folder);const auto file=folder/"updated.dufex";save_scene_extension(file,d,s);auto restored=load_scene_extension(file,roots,2);
  for(auto i:updated){check(copy_material(d.loaded.scene,s.material_overrides,{i,0})==copy_material(restored.document->loaded.scene,restored.snapshot.material_overrides,{i,0}),"自动 UV 属性保存重载不一致");const auto &a=d.loaded.scene.meshes.at(d.loaded.scene.instances[i].mesh);const auto &b=restored.document->loaded.scene.meshes.at(restored.document->loaded.scene.instances[i].mesh);check(a.triangles==b.triangles&&a.polygons.size()==b.polygons.size(),"自动 UV 实际坐标保存重载不一致");for(size_t p=0;p<a.polygons.size();++p)check(a.polygons[p].uv==b.polygons[p].uv,"多边形 UV 保存重载不一致");}
  report["status"]="PASS";std::ofstream(folder/"result.json")<<report.dump(2);std::cout<<"PASS real genitalia UV / save-reload"<<std::endl;
}
