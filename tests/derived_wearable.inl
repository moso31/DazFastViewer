// 对应内嵌骨架、外部静态道具及失效源几何 ID 的穿戴资源。
static void derived_wearable_tests(const fs::path &folder,const J &base,const editor::Document &host) {
  const auto directory=folder/"data/Derived";fs::create_directories(directory/"Morphs");
  const auto source_file=directory/"model.dsf",preset_file=folder/"derived.duf";
  auto source=base;source["asset_info"]={{"type","prop"}};source.erase("modifier_library");
  source["node_library"]={{{"id","figure"},{"type","node"}}};
  auto preset=base;preset["asset_info"]={{"type","wearable"}};
  preset["geometry_library"][0]["id"]="embedded";
  preset["geometry_library"][0]["source"]="/data/Derived/model.dsf#stale-skin-id";
  preset["modifier_library"][0]["skin"]["geometry"]="#embedded";
  preset["node_library"][0]["source"]="/data/Derived/model.dsf#figure";
  preset["node_library"][0]["presentation"]={{"type","Follower/Attachment"},{"preferred_base","/Genesis 8/Female"}};
  preset["scene"]={{"nodes",{{{"id","derived"},{"url","#figure"},{"parent","name://@selection:"},{"conform_target","name://@selection:"},
    {"geometries",{{{"id","shape"},{"url","#embedded"}}}}}}},
    {"materials",{{{"id","material"},{"geometry","#shape"},{"groups",{"Skin"}}}}}};
  const J morph=J::parse(R"({"id":"FitForBody","parent":"/data/Derived/model.dsf#figure",
    "channel":{"type":"float","value":1,"min":0,"max":1},
    "morph":{"vertex_count":3,"deltas":{"count":1,"values":[[1,20,0,0]]}}})");
  std::ofstream(directory/"Morphs/fit.dsf")<<J{{"modifier_library",{morph}}};
  auto save=[&] {std::ofstream(source_file)<<source;std::ofstream(preset_file)<<preset;};save();
  auto imported=load(preset_file,{folder},true);const auto &geometry=imported.loaded.objects[0].geometry_sources.at(0);
  require(geometry.id=="mesh"&&geometry.node=="figure","失效源几何或内嵌蒙皮的来源节点没有按证据恢复");
  require(imported.catalog.targets[0].morphs.size()==1&&imported.catalog.targets[0].morphs[0].has_offsets()&&
    imported.catalog.targets[0].morphs[0].intrinsic_error.empty(),"外部道具节点的适配 Morph 没有启用");
  auto document=host;const auto first=document.catalog.targets.size();editor::append_document(document,std::move(imported));editor::attach_import(document,first,0);
  auto snapshot=editor::initial_snapshot(document);const auto fitted=evaluate(document,snapshot);
  snapshot.values[first].morphs[0]=0;const auto neutral=evaluate(document,snapshot);
  require(distance(point(fitted,first,1),point(neutral,first,1))>.05,"适配 Morph 只被登记，没有实际改变穿戴几何");
  snapshot.values[first].morphs[0]=1;editor::fit_attachment(document,first,-1);evaluate(document,snapshot);
  editor::fit_attachment(document,first,0);const auto rebound=evaluate(document,snapshot);
  require(distance(point(rebound,first,1),point(fitted,first,1))<1e-6,"重新绑定改变了派生服装的形态");
  const auto archive=folder/"derived.dufex";editor::save_scene_extension(archive,document,snapshot);
  fs::remove(preset_file);auto restored=editor::load_scene_extension(archive,{folder},12);const auto reopened=evaluate(*restored.document,restored.snapshot);
  require(distance(point(reopened,first,1),point(fitted,first,1))<1e-6,"冻结底稿重开丢失来源节点或适配 Morph");
  save();
  // 普通实例的 url 不参与容错；只在派生几何 source 上恢复。
  auto ordinary=preset;ordinary["scene"]["nodes"][0]["geometries"][0]["url"]="/data/Derived/model.dsf#stale-skin-id";
  const auto ordinary_file=folder/"ordinary-stale.duf";std::ofstream(ordinary_file)<<ordinary;
  rejects([&]{load(ordinary_file,{folder},true);},"普通几何引用被拓扑猜测替换");
  auto duplicate=source["geometry_library"][0];duplicate["id"]="another";source["geometry_library"].push_back(duplicate);save();
  rejects([&]{load(preset_file,{folder},true);},"多几何资产的失效 ID 被猜测恢复");
  source["geometry_library"].erase(1);source["geometry_library"][0]["polylist"]["values"][0]={0,0,0,2,1};save();
  rejects([&]{load(preset_file,{folder},true);},"相同顶点数量但不同面索引的源几何被继承");
  source["geometry_library"]=base["geometry_library"];
  preset["node_library"][0]["source"]="/data/Derived/model.dsf#unrelated";save();
  const auto unproven=load(preset_file,{folder},true);
  require(unproven.loaded.objects[0].geometry_sources[0].node.empty()&&!unproven.catalog.targets[0].morphs[0].intrinsic_error.empty(),"节点来源不符时仍套用了外部 Morph");
  preset["node_library"][0]["source"]="/data/Absent/model.dsf#figure";save();
  const auto missing_source=load(preset_file,{folder},true);
  require(missing_source.loaded.objects[0].geometry_sources[0].node.empty()&&!missing_source.catalog.targets[0].morphs[0].intrinsic_error.empty(),"可选节点来源缺失应保留几何并拒绝推断 Morph 归属");
  preset["node_library"][0]["source"]="/data/Derived/model.dsf#figure";
  // 同一几何的两个实例有不同来源证明时，参数缓存不能将已验证的结果复制给另一实例。
  auto mixed=preset;auto unverified_node=mixed["scene"]["nodes"][0];unverified_node["id"]="unverified-instance";
  unverified_node["source"]="/data/Derived/model.dsf#unrelated";unverified_node["geometries"][0]["id"]="unverified-shape";
  mixed["scene"]["nodes"].push_back(unverified_node);auto material=mixed["scene"]["materials"][0];material["id"]="unverified-material";material["geometry"]="#unverified-shape";
  mixed["scene"]["materials"].push_back(material);const auto mixed_file=folder/"mixed-derived.duf";std::ofstream(mixed_file)<<mixed;
  const auto mixed_document=load(mixed_file,{folder},true);
  require(mixed_document.catalog.targets[0].morphs[0].intrinsic_error.empty()&&!mixed_document.catalog.targets[1].morphs[0].intrinsic_error.empty(),"参数缓存跨实例传播了来源节点证明");
  auto other_binding=preset["modifier_library"][0];other_binding["skin"]["geometry"]="#other";preset["modifier_library"].push_back(other_binding);save();
  const auto ambiguous=daz::load(preset_file,{{folder},false,true});
  require(ambiguous.objects[0].geometry_sources[0].node.empty(),"内嵌 Figure 的多几何绑定歧义被忽略");
  std::cout<<"Derived wearable / verified source repair / external node morph / detach / rebind / frozen replay / rejection boundaries: PASS\n";
}
