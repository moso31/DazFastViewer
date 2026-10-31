// 真实场景姿势提交计时；预热后独立记录 FK、IK、恢复和重复求值。
static void pose_profile(const fs::path &file,const fs::path &output,const fs::path &baseline={}) {
  fs::create_directories(output);
  auto loaded=editor::load_scene_extension(file,{"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"},1);
  auto &d=*loaded.document;const auto initial=loaded.snapshot;auto s=initial;auto scene=d.loaded.scene;
  size_t host=SIZE_MAX;for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].label=="SC Slipper") {
    for(size_t h=0;h<d.catalog.targets.size();++h)if(d.catalog.targets[t].conform_target=="#"+d.catalog.targets[h].id.substr(0,d.catalog.targets[h].id.rfind('/')))host=h;
  }
  if(host==SIZE_MAX)for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].character&&d.formulas.graphs[t].skin>=0){host=t;break;}
  check(host!=SIZE_MAX,"姿势性能测试没有找到人物");const auto sk=size_t(d.formulas.graphs[host].skin);const auto &skin=d.skeletons.skins[sk];
  auto joint=[&](const char *name){for(size_t j=0;j<skin.joints.size();++j)if(skin.joints[j].name==name)return j;throw std::runtime_error("姿势性能测试缺少骨骼");};
  const auto foot=joint("lFoot"),shin=joint("lShin");
  runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  J report={{"source",file.generic_string()},{"host",host},{"cases",J::array()}};
  report["targets"]=J::array();for(const auto &t:d.catalog.targets)report["targets"].push_back({{"id",t.id},{"label",t.label},{"mesh",scene.instances[t.instance].mesh},{"content_type",t.content_type}});
  auto run=[&](const char *name,const char *reference_name=nullptr) {
    diagnostics::LoadProfile profile;diagnostics::active=&profile;const auto begin=diagnostics::Clock::now();
    runtime.prepare(s.values,s.poses);const auto delta=runtime.evaluate(s.values,s.poses);
    const double ms=std::chrono::duration<double,std::milli>(diagnostics::Clock::now()-begin).count();diagnostics::active=nullptr;
    J entry={{"case",name},{"milliseconds",ms},{"changed_meshes",delta.meshes.size()},{"timings",J::object()}};
    for(const auto &[path,t]:profile.timings)entry["timings"][path]={{"milliseconds",t.seconds*1000},{"self_ms",t.self_seconds*1000},{"calls",t.calls}};
    std::ofstream stream(output/(std::string(name)+".bin"),std::ios::binary);std::ifstream previous;
    if(!baseline.empty())previous.open(baseline/(std::string(reference_name?reference_name:name)+".bin"),std::ios::binary);
    std::vector<bool> visible(scene.meshes.size());for(const auto &instance:scene.instances)if(instance.visible)visible[instance.mesh]=true;
    double difference=0,hidden_difference=0;for(size_t m=0;m<scene.meshes.size();++m){const auto &mesh=scene.meshes[m];const uint64_t count=mesh.positions.size();stream.write(reinterpret_cast<const char *>(&count),sizeof(count));stream.write(reinterpret_cast<const char *>(mesh.positions.data()),std::streamsize(count*sizeof(ir::Vec3)));
      if(!baseline.empty()){uint64_t n=0;previous.read(reinterpret_cast<char *>(&n),sizeof(n));check(n==count,"性能优化改变了网格顶点数");std::vector<ir::Vec3> points(count);previous.read(reinterpret_cast<char *>(points.data()),std::streamsize(count*sizeof(ir::Vec3)));check(bool(previous),"性能几何基线读取失败");auto &maximum=visible[m]?difference:hidden_difference;for(size_t v=0;v<count;++v)maximum=std::max(maximum,distance(points[v],mesh.positions[v]));}}
    entry["hidden_max_difference_m"]=hidden_difference;
    entry["baseline_max_difference_m"]=difference;check(difference<1e-6,"性能优化改变了已有几何结果");
    report["cases"].push_back(entry);std::ofstream(output/"profile.json")<<report.dump(2);std::cout<<name<<": "<<ms<<" ms, difference="<<difference<<std::endl;
  };
  run("initial");s.poses[sk][foot].rotation_degrees.x+=5;run("foot_fk");
  s=initial;s.poses[sk][shin].rotation_degrees.x+=8;run("shin_fk");
  s=initial;auto effective=runtime.resolve_poses(s.values,s.poses);auto solved=effective[sk];auto goal=runtime::IkGoal{int(foot),true,runtime::joint_point(skin,solved,int(foot),true)};goal.position.x+=.01f;goal.position.z+=.015f;
  const auto chain=runtime::ik_chain(skin,int(foot));const auto ik=runtime::solve_ik(runtime::ik_limits(skin,s.poses[sk],solved),solved,chain,std::span(&goal,1));check(ik.changed,"性能测试 IK 没有改变姿势");s.poses[sk]=runtime::ik_input(s.poses[sk],effective[sk],solved);run("leg_ik");
  s=initial;run("restore");run("unchanged");
  for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].label=="Flip Flops"&&!s.values[t].visible){s.values[t].visible=true;run("show_hidden","initial");s.values[t].visible=false;run("hide_restore","initial");break;}
  report["status"]="PASS";std::ofstream(output/"profile.json")<<report.dump(2);
}
