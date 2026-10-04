static void content_feedback(int argc,wchar_t **argv){
  using J=nlohmann::json;using namespace editor;namespace fs=std::filesystem;
  require(argc>=9,"--content-feedback scene pose02 pose04 material shell-scene report root...");
  std::vector<fs::path> roots;for(int i=8;i<argc;++i)roots.emplace_back(argv[i]);
  Document d;d.loaded=daz::load(argv[2],{roots,false});std::cout<<"Scene loaded"<<std::endl;
  // Preserve the original full scene for material checks; evaluate the reported
  // figure independently so this diagnostic does not require unrelated outfits.
  auto pose_document=d;std::erase_if(pose_document.loaded.objects,[](const auto &o){return o.label!="Lee 8 (2)";});
  auto &p=pose_document;p.catalog=daz::discover_morphs(p.loaded,roots,{},true);p.skeletons=daz::load_skeletons(p.loaded);p.formulas=daz::enable_formulas(p.catalog,p.skeletons);
  require(p.catalog.targets.size()==1&&p.skeletons.skins.size()==1,"Lee target not found");
  const auto initial=initial_snapshot(p);const auto &target=p.catalog.targets[0];const auto &skin=p.skeletons.skins[0];auto scene=p.loaded.scene;
  runtime::DeformationRuntime runtime(scene,p.catalog.targets,p.skeletons.skins,p.formulas.graphs);
  J report={{"poses",J::array()}};const auto mesh=scene.instances[target.instance].mesh;
  for(int pose_index:{3,4}){
    auto snapshot=initial;const auto preset=daz::read_pose(argv[pose_index]);auto applied=daz::apply_pose(preset,skin,snapshot.poses[0],target,snapshot.values[0]);snapshot.poses[0]=applied.joints;snapshot.values[0]=applied.properties;
    runtime.evaluate(snapshot.values,snapshot.poses);const auto correct=scene.meshes[mesh].positions;const auto corrected_weights=runtime.effective()[0];
    auto old=snapshot;
    // Negative control: reproduce the previous manual-edit limit policy.
    for(const auto &c:preset.channels)if(!c.modifier.empty())for(size_t m=0;m<target.morphs.size();++m){const auto &morph=target.morphs[m];if(morph.channel_name==c.modifier&&morph.unsupported.empty())old.values[0].unlimited_morphs.insert(target.morphs[morph.alias_morph>=0?size_t(morph.alias_morph):m].id);}
    runtime.evaluate(old.values,old.poses);J negative=J::array();
    for(size_t m=0;m<target.morphs.size();++m){const auto &morph=target.morphs[m];if(morph.channel_name.starts_with("pJCM")&&runtime.effective()[0][m]<0&&corrected_weights[m]>=0)negative.push_back({{"name",morph.channel_name},{"old",runtime.effective()[0][m]},{"fixed",corrected_weights[m]}});}
    double difference=0;for(size_t v=0;v<correct.size();++v){auto a=correct[v],b=scene.meshes[mesh].positions[v];difference=std::max(difference,double(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)));}
    // A subsequent rotation-only preset must not inherit disabled limits.
    auto follow=initial;follow.values=snapshot.values;runtime.evaluate(follow.values,follow.poses);auto fresh=scene.meshes[mesh].positions;
    runtime.evaluate(snapshot.values,snapshot.poses);runtime.evaluate(follow.values,follow.poses);require(scene.meshes[mesh].positions==fresh,"姿势切换残留形变");
    runtime.evaluate(initial.values,initial.poses);const auto reset=scene.meshes[mesh].positions;runtime.evaluate(snapshot.values,snapshot.poses);runtime.evaluate(initial.values,initial.poses);require(scene.meshes[mesh].positions==reset,"姿势撤销未恢复");
    report["poses"].push_back({{"file",fs::path(argv[pose_index]).filename().string()},{"negative_controls",negative},{"max_old_fixed_difference_m",difference},{"follow_pose_stable",true},{"restore_exact",true},{"applied",applied.report}});
    require(!negative.empty()&&difference>.001,"问题姿势未复现解除限位导致的形变");std::cout<<"Pose verified: "<<pose_index<<std::endl;
  }
  size_t big=0;for(const auto &o:d.loaded.objects){runtime::Target t;t.instance=o.instance;t.id=d.loaded.scene.instances[o.instance].id;t.label=o.label;d.catalog.targets.push_back(t);if(o.label=="big_01")big=d.catalog.targets.size()-1;}
  auto snapshot=initial_snapshot(d);const auto preset=daz::load(argv[5],{roots,false});const auto count=apply_materials(d,big,preset,&snapshot);const auto &instance=d.loaded.scene.instances[d.catalog.targets[big].instance];const auto &slots=d.loaded.scene.meshes[instance.mesh].material_slots;
  report["material"]={{"matched",count},{"slots",slots}};require(count==slots.size(),"Ruo Xi 没有覆盖 big_01 的全部表面");
  for(const auto *name:{"Body","Head"}){auto s=std::find(slots.begin(),slots.end(),name);require(s!=slots.end(),"缺少 G8.1 表面");const auto &material=d.loaded.scene.materials[instance.materials[size_t(s-slots.begin())]];require(material.color_texture>=0,"Torso 映射没有贴图");report["material"][name]=d.loaded.scene.textures[material.color_texture].file.string();}
  std::cout<<"Materials verified"<<std::endl;
  Document shells;shells.loaded=daz::load(argv[6],{roots,false});for(const auto &o:shells.loaded.objects){runtime::Target t;t.instance=o.instance;t.id=shells.loaded.scene.instances[o.instance].id;t.label=o.label;shells.catalog.targets.push_back(t);}
  const ObjectTargets objects(shells);size_t parts=0,rows=0;for(size_t t=0;t<shells.catalog.targets.size();++t)if(shells.catalog.targets[t].label=="GoldenPalace_Shell"){++parts;if(objects.primary[t]==t)++rows;}
  require(parts==3&&rows==1,"真实 Shell 没有合并为一个逻辑对象");report["shell"]={{"render_parts",parts},{"scene_rows",rows}};report["status"]="PASS";
  std::ofstream(fs::path(argv[7]))<<report.dump(2);std::cout<<"Content feedback: PASS"<<std::endl;
}
