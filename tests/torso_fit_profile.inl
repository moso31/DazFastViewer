// 真实上衣诊断：在独立副本中分离作者 Morph、自动投射、蒙皮与碰撞。
static void torso_profile(const fs::path &file,const fs::path &output) {
  const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library"};
  fs::create_directories(output);std::cout<<"Loading torso scene"<<std::endl;
  auto loaded=editor::load_scene_extension(file,roots,1);auto &d=*loaded.document;auto s=loaded.snapshot;
  size_t host=SIZE_MAX,gym=SIZE_MAX;
  for(size_t t=0;t<d.catalog.targets.size();++t){if(d.catalog.targets[t].id=="Genesis8_1Female-3/Genesis81FemaleGeom-3")host=t;if(d.catalog.targets[t].id.find("GymClass TShirt")!=std::string::npos)gym=t;}
  std::cout<<"Loaded host="<<host<<", gym="<<gym<<std::endl;
  check(host!=SIZE_MAX&&gym!=SIZE_MAX,"未找到截图角色和 Gym Class 上衣");
  const auto sue=d.catalog.targets.size();
  editor::append_document(d,load_wear("H:/G1/People/Genesis 8 Female/Clothing/Sue Yee/dForce Sue Yee School Uniform/Sue Yee SU Shirt.duf",roots));editor::attach_import(d,sue,host);
  const auto defaults=editor::initial_snapshot(d);s.values.insert(s.values.end(),defaults.values.begin()+s.values.size(),defaults.values.end());s.poses.insert(s.poses.end(),defaults.poses.begin()+s.poses.size(),defaults.poses.end());s.generation=d.generation;
  for(auto &v:s.values)v.visible=false;s.values[host].visible=true;s.values[gym].visible=true;s.values[sue].visible=true;
  J report={{"host",host},{"gym",gym},{"sue",sue},{"targets",J::array()}};
  auto raw=d.loaded.scene;runtime::DeformationRuntime uncorrected(raw,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);uncorrected.defer_collision();uncorrected.evaluate(s.values,s.poses);
  auto final=d.loaded.scene;runtime::DeformationRuntime corrected(final,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);corrected.evaluate(s.values,s.poses);
  editor::apply_subdivision_levels(raw,s.subdivision_levels);editor::apply_subdivision_levels(final,s.subdivision_levels);
  for(auto t:{host,gym,sue}){
    const auto &target=d.catalog.targets[t];const auto name=t==host?"host":t==gym?"gym":"sue";const auto folder=output/name;fs::create_directories(folder);
    const runtime::ConformLink *link=nullptr;for(const auto &l:uncorrected.conform_links())if(l.follower==t)link=&l;
    J entry={{"target",t},{"label",target.label},{"morphs",J::array()},{"projection",J::array()}};
    auto authored=d.loaded.scene;auto &authored_mesh=authored.meshes[authored.instances[target.instance].mesh];
    for(size_t m=0;m<target.morphs.size();++m){const auto &p=target.morphs[m];const auto value=uncorrected.effective()[t][m];if(value!=0||s.values[t].morphs[m]!=0||(link&&link->morph_sources[m]>=0))entry["morphs"].push_back({{"id",p.channel_id},{"value",value},{"input",s.values[t].morphs[m]},{"min",p.minimum},{"max",p.maximum},{"clamped",p.clamped},{"unlimited",s.values[t].unlimited_morphs.contains(p.id)},{"source",link?link->morph_sources[m]:-1},{"offsets",p.offset_count()}});
      if(value!=0&&p.has_offsets())for(const auto &o:p.data()){auto &v=authored_mesh.positions.at(o.vertex);v.x+=value*o.delta.x;v.y+=value*o.delta.y;v.z+=value*o.delta.z;}}
    if(link)for(auto m:link->projected_morphs)if(uncorrected.effective()[host][m]!=0)entry["projection"].push_back({{"id",d.catalog.targets[host].morphs[m].channel_id},{"value",uncorrected.effective()[host][m]}});
    std::ofstream(folder/"morphs.json")<<entry.dump(2);report["targets"].push_back(entry);
    // 只导出衣物表面供视觉检查；角色几何保留在数值 fixture 中。
    if(t==host)continue;
    dump(d,d.loaded.scene,t,folder/"0-base.obj",true);dump(d,authored,t,folder/"1-authored.obj",true);
    auto morphed=raw;morphed.meshes[morphed.instances[target.instance].mesh].positions=uncorrected.skin_source(size_t(d.formulas.graphs[t].skin));dump(d,morphed,t,folder/"2-projected.obj",true);
    dump(d,raw,t,folder/"3-skinned.obj",true);dump(d,final,t,folder/"4-collision.obj",true);
    J fixture={{"body",collision_mesh(mesh(d,d.loaded.scene,host))},{"cloth",collision_mesh(mesh(d,d.loaded.scene,t))},{"body_input",points_json(mesh(d,raw,host).positions)},{"cloth_input",points_json(mesh(d,raw,t).positions)},{"morphed",points_json(mesh(d,morphed,t).positions)},{"authored",points_json(authored_mesh.positions)},{"expected",points_json(mesh(d,final,t).positions)},{"content_type",target.content_type}};
    std::vector<ir::Vec3> projected(mesh(d,d.loaded.scene,host).positions.size());
    for(auto m:link->projected_morphs)if(uncorrected.effective()[host][m]!=0)for(const auto &o:d.catalog.targets[host].morphs[m].data()){auto &v=projected[o.vertex];const auto w=uncorrected.effective()[host][m];v.x+=w*o.delta.x;v.y+=w*o.delta.y;v.z+=w*o.delta.z;}
    fixture["source_offsets"]=points_json(projected);
    fixture["source_frame"]=d.loaded.scene.instances[d.catalog.targets[host].instance].transform.value;fixture["cloth_frame"]=d.loaded.scene.instances[target.instance].transform.value;
    const auto k=size_t(d.formulas.graphs[t].skin);const auto &skin=d.skeletons.skins[k];fixture["skin"]={{"dual_quaternion",skin.method==runtime::SkinMethod::dual_quaternion},{"joints",J::array()},{"poses",J::array()},{"weights",J::array()}};
    for(const auto &joint:skin.joints)fixture["skin"]["joints"].push_back({{"name",joint.name},{"parent",joint.parent},{"center",points_json({joint.center_cm})[0]},{"end",points_json({joint.end_cm})[0]},{"orientation",points_json({joint.orientation_degrees})[0]},{"order",joint.rotation_order},{"inherits_scale",joint.inherits_scale}});
    for(const auto &p:uncorrected.effective_poses()[k])fixture["skin"]["poses"].push_back({{"translation",points_json({p.translation_cm})[0]},{"rotation",points_json({p.rotation_degrees})[0]},{"scale",points_json({p.scale})[0]},{"general_scale",p.general_scale},{"center",points_json({p.center_offset_cm})[0]},{"end",points_json({p.end_offset_cm})[0]},{"orientation",points_json({p.orientation_offset_degrees})[0]}});
    for(const auto &weights:skin.weights){J w=J::array();for(const auto &v:weights)w.push_back({v.joint,v.weight});fixture["skin"]["weights"].push_back(w);}
    std::ofstream(folder/"fixture.json")<<fixture.dump();
    auto isolated=s;isolated.values[gym].visible=t==gym;for(size_t v=sue;v<isolated.values.size();++v)isolated.values[v].visible=t==sue;
    editor::save_scene_extension(folder/"isolated.dufex",d,isolated);
    std::cout<<name<<": collision max "<<difference(mesh(d,raw,t).positions,mesh(d,final,t).positions)*1000<<" mm"<<std::endl;
  }
  std::ofstream(output/"report.json")<<report.dump(2);std::cout<<"Torso diagnostic complete"<<std::endl;
}

static void torso_replay(const fs::path &file,const fs::path &output) {
  J fixture;std::ifstream(file)>>fixture;fs::create_directories(output);
  editor::Document d;auto &scene=d.loaded.scene;scene.meshes={read_collision_mesh(fixture["body"]),read_collision_mesh(fixture["cloth"])};scene.instances.resize(2);scene.instances[1].mesh=1;
  scene.instances[0].transform.value=fixture.at("source_frame").get<std::array<float,12>>();scene.instances[1].transform.value=fixture.at("cloth_frame").get<std::array<float,12>>();
  runtime::Target host;host.id="body/mesh";host.auto_fit_base="/Genesis 8.1/Female";
  runtime::Morph shape;shape.id=shape.channel_id="auto";shape.evaluable=shape.auto_follow=true;auto offsets=read_points(fixture["source_offsets"]);for(uint32_t v=0;v<offsets.size();++v)shape.offsets.push_back({v,offsets[v]});host.morphs.push_back(shape);
  runtime::Target cloth;cloth.id="cloth/mesh";cloth.instance=1;cloth.conform_target="#body";cloth.preferred_base="/Genesis 8/Female";cloth.content_type=fixture["content_type"];
  runtime::Morph own;own.id=own.channel_id="own";own.evaluable=true;auto authored=read_points(fixture["authored"]);const auto &base=scene.meshes[1].positions;for(uint32_t v=0;v<base.size();++v)own.offsets.push_back({v,{authored[v].x-base[v].x,authored[v].y-base[v].y,authored[v].z-base[v].z}});cloth.morphs.push_back(own);
  d.catalog.targets={host,cloth};d.formulas.graphs={graph(host),graph(cloth)};
  runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.defer_collision();std::vector<runtime::Properties> values(2);values[0].morphs={1};values[1].morphs={1};runtime.evaluate(values,{});
  dump(d,scene,1,output/"2-projected.obj",true);std::ofstream(output/"positions.json")<<points_json(scene.meshes[1].positions).dump();
  std::cout<<"Replay difference "<<difference(scene.meshes[1].positions,read_points(fixture["morphed"]))<<std::endl;
}

static void torso_collision_replay(const fs::path &file,const fs::path &output) {
  J fixture;std::ifstream(file)>>fixture;fs::create_directories(output);editor::Document d;auto &scene=d.loaded.scene;
  scene.meshes={read_collision_mesh(fixture["body"]),read_collision_mesh(fixture["cloth"])};scene.instances.resize(2);scene.instances[1].mesh=1;
  scene.instances[0].transform.value=fixture.at("source_frame").get<std::array<float,12>>();scene.instances[1].transform.value=fixture.at("cloth_frame").get<std::array<float,12>>();
  runtime::Target body;body.id="body/mesh";body.auto_fit_base="/Genesis 8.1/Female";
  runtime::Target cloth;cloth.id="cloth/mesh";cloth.instance=1;cloth.conform_target="#body";cloth.preferred_base="/Genesis 8/Female";cloth.content_type=fixture["content_type"];
  cloth.smoothing.enabled=true;cloth.smoothing.collision_target="#body";cloth.smoothing.collision_iterations=3;cloth.smoothing.smoothing_iterations=2;cloth.smoothing.weight=.5f;d.catalog.targets={body,cloth};
  runtime::CollisionRuntime runtime(scene,d.catalog.targets);scene.meshes[0].positions=read_points(fixture["body_input"]);scene.meshes[1].positions=read_points(fixture["cloth_input"]);ir::Delta delta;delta.meshes.push_back({1,scene.meshes[1].positions});runtime.evaluate(std::move(delta));
  dump(d,scene,1,output/"4-collision.obj",true);std::cout<<"Collision difference "<<difference(scene.meshes[1].positions,read_points(fixture["expected"]))<<std::endl;
  fixture["expected"]=points_json(scene.meshes[1].positions);std::ofstream(output/"fixture.json")<<fixture.dump();check(runtime.evaluate({}).meshes.empty(),"层间碰撞重复求值发生漂移");
}
