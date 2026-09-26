static void native_extension_tests() {
  using namespace dfv;const auto root=fs::absolute("native-extension-data");
  const auto asset=root/"data/figure.dsf",scene=root/"scene.duf";
  write(asset,{{"node_library",{{{"id","figure"},{"type","figure"}}}},{"geometry_library",{{{"id","geometry"},{"vertices",{{"count",2}}},{"polylist",{{"values",J::array()}}}}}}});
  J library=J::array(),instances=J::array();
  auto add=[&](const char *owner,const char *id,const char *name,double value){auto m=control(id,name,"/data/figure.dsf#figure");m["channel"]["current_value"]=value;library.push_back(m);instances.push_back({{"id",std::string(id)+"-instance"},{"url",std::string("#")+id},{"parent",std::string("#")+owner}});};
  add("partial","partial-age","Age",4.5);add("partial","partial-strength","Body Strength",.75);
  add("full","full-age","Age",12);add("full","full-step","Age Step",.25);add("full","full-sense","Age Sensitivity",10);add("full","full-strength","Body Strength",0);
  add("invalid","bad-age","Age",22);add("invalid","zero-sense","Age Sensitivity",0);
  add("conflict","age-one","Age",2);add("conflict","age-two","Age",5);
  add("prop","density","Density",7500);
  instances[0]["channel"]={{"current_value",6.25}};
  J document={{"modifier_library",library},{"scene",{{"nodes",J::array()},{"modifiers",instances}}}};write(scene,document);
  daz::LoadedScene loaded;ir::Mesh mesh;mesh.positions={{0,0,0},{1,1,1}};loaded.scene.meshes={mesh};loaded.report["input"]=scene.generic_string();
  for(const char *id:{"none","partial","full","invalid","conflict","prop"}) {const auto index=uint32_t(loaded.scene.instances.size());ir::Instance instance;instance.id=id;loaded.scene.instances.push_back(instance);daz::AssetObject object;object.id=object.label=id;object.instance=index;object.geometry_file=asset;object.geometry_id="geometry";object.geometry_instance_id=std::string(id)+"-geometry";object.figure=std::string(id)!="prop";loaded.objects.push_back(object);}
  auto catalog=daz::discover_morphs(loaded,{root});auto &targets=catalog.targets;
  require(targets[0].native_extension.kind==runtime::ExtensionKind::none,"未匹配角色被自动注册");
  const auto &partial=targets[1].native_extension;require(partial.kind==runtime::ExtensionKind::growth&&partial.age==6.25&&partial.strength==.75&&partial.age_step==1&&partial.sensitivity==5,"部分匹配或实例覆盖、缺失默认值错误");
  const auto &full=targets[2].native_extension;require(full.age==12&&full.age_step==.25&&full.sensitivity==10&&full.strength==0&&targets[2].native_extension_channels.size()==4,"完整匹配或零值丢失");
  require(targets[3].native_extension.age==3&&targets[3].native_extension.sensitivity==0&&targets[3].native_extension_channels.size()==1,"无效字段没有独立回退");
  require(targets[4].native_extension.kind==runtime::ExtensionKind::none,"冲突的原生参数被错误猜测");
  require(targets[5].native_extension.kind==runtime::ExtensionKind::density&&targets[5].native_extension.density==7500,"道具原生密度没有继承");
  require(targets[1].morphs.empty()&&targets[2].morphs.empty(),"原生扩展重复进入 Morph 列表");
  editor::Document d;d.loaded=std::move(loaded);d.catalog=std::move(catalog);auto snapshot=editor::initial_snapshot(d);
  require(snapshot.values[1].extension==partial,"初始快照丢失原生扩展");
  auto saved=editor::snapshot_json(d,snapshot);snapshot.values[1].extension.age=8.5;saved=editor::snapshot_json(d,snapshot);auto restored=editor::initial_snapshot(d);editor::apply_snapshot_json(d,restored,saved);
  require(restored.values[1].extension.age==8.5,"DUFEX 未覆盖原生初值");
}
