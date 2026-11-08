static void validation_preparation_checks() {
  ir::Mesh body;body.positions={{0,0,0},{1,0,0},{0,1,0}};body.triangles={{{0,1,2}}};auto cloth=body;for(auto &p:cloth.positions)p.z=.02f;
  ir::Scene base;base.meshes={body,cloth};base.instances.resize(2);base.instances[1].mesh=1;
  Target host;host.id="validation-host/mesh";host.morphs={morph("shape",{{0,{0,0,.1f}}})};Target shirt;shirt.id="validation-shirt/mesh";shirt.instance=1;shirt.conform_target="#validation-host";
  const std::vector<Target> targets={host,shirt};const std::vector<Skin> skins;const std::vector<FormulaGraph> graphs={graph(host,-1),graph(shirt,-1)};
  auto validated=base;DeformationRuntime validation(validated,targets,skins,graphs,nullptr,ConformPreparation::validation);
  require(validation.conform_stats().bindings==0&&validation.conform_links().front().surface.empty(),"穿戴校验仍构建逐顶点投射绑定");
  auto evaluated=base;DeformationRuntime full(evaluated,targets,skins,graphs);
  require(full.conform_stats().bindings==3&&full.conform_links().front().projected_morphs==validation.conform_links().front().projected_morphs,"轻量校验遗漏了形变关系");
  std::vector<Properties> values(2);values[0].morphs={1};rejects([&]{validation.evaluate(values,{});},"只校验的运行时被用于实际形变");
  // 两条准备路径必须拒绝同样的无效绑定，不能靠省略校验获得提速。
  auto rejected=[&](ir::Scene scene,std::vector<Target> inputs){std::string first;for(auto mode:{ConformPreparation::full,ConformPreparation::validation}){
    auto copy=scene;std::string error;try{DeformationRuntime runtime(copy,inputs,skins,graphs,nullptr,mode);}catch(const std::exception &e){error=e.what();}
    require(!error.empty(),"无效附件没有被拒绝");if(first.empty())first=error;else require(error==first,"轻量校验与完整准备拒绝原因不同");
  }};
  auto invalid=targets;invalid[0].conform_target="#validation-shirt";rejected(base,invalid);
  invalid=targets;invalid[1].conform_target="#missing";rejected(base,invalid);
  auto empty=base;empty.meshes[0].triangles.clear();rejected(empty,targets);
  auto graft=base;graft.meshes[1].graft_target_vertices=3;graft.meshes[1].graft_vertex_pairs={{{0,0}},{{1,1}}};
  auto graft_full=graft,graft_validation=graft;DeformationRuntime gf(graft_full,targets,skins,graphs),gv(graft_validation,targets,skins,graphs,nullptr,ConformPreparation::validation);
  require(graft_full.instances[1].graft_source==0&&graft_validation.instances[1].graft_source==0,"轻量校验没有更新 GeoGraft 宿主");
  graft.meshes[1].graft_target_vertices=4;rejected(graft,targets);
  invalid=targets;invalid[1].rigid_follow={"#validation-host",4,{0,1,2},true};rejected(base,invalid);
  invalid=targets;invalid[1].smoothing.enabled=true;invalid[1].smoothing.collision_target="#missing";rejected(base,invalid);
  invalid=targets;invalid[0].auto_fit_base="/Genesis 8/Female";invalid[1].preferred_base="/Genesis 8/Male";invalid[1].content_type="Follower/Wardrobe/Shirt";rejected(base,invalid);
}
