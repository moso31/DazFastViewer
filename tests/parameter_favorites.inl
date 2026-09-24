// 覆盖真实 DUF 扩展结构、实例目录复用及 Qt 收藏操作，不接触用户设置或资产。
static void parameter_favorites(QApplication &app) {
  using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
  QTemporaryDir temp;require(temp.isValid(),"收藏测试目录创建失败");
  const fs::path root(temp.path().toStdWString());fs::create_directories(root/"data/Morphs");
  auto write=[&](const fs::path &path,const J &json){std::ofstream(path)<<json.dump();};
  auto favorite_extra=[](J favorites){return J::array({{{"type","studio_node_channels"},{"favorites",std::move(favorites)}}});};
  write(root/"data/figure.dsf",J::parse(R"({
    "node_library":[{"id":"figure","type":"figure"},{"id":"head","type":"bone","parent":"#figure"}],
    "geometry_library":[{"id":"geometry","type":"polygon_mesh","vertices":{"count":3,"values":[[0,0,0],[1,0,0],[0,1,0]]},
      "polygon_material_groups":{"count":1,"values":["surface"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],
    "uv_set_library":[{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}]
  })"));
  J shape={{"id","shape"},{"name","Shape Name"},{"parent","/data/figure.dsf#geometry"},{"channel",{{"type","float"},{"label","测试形态"},{"value",0},{"min",0},{"max",1}}},
    {"morph",{{"vertex_count",3},{"deltas",{{"count",1},{"values",{{0,0,0,1}}}}}}}};
  auto idle=shape;idle["id"]="idle";idle["name"]="未使用形态";idle["channel"]["label"]="未使用形态";
  write(root/"data/Morphs/shape.dsf",{{"modifier_library",{shape,idle}}});
  auto alias=shape;alias.erase("morph");alias["parent"]="/data/figure.dsf#head";alias["channel"]["type"]="alias";alias["channel"]["target_channel"]="figure:/data/Morphs/shape.dsf#shape?value";
  write(root/"data/Morphs/alias.dsf",{{"modifier_library",{alias}}});
  write(root/"data/Morphs/duplicate.dsf",{{"modifier_library",{shape}}});
  J nodes=J::array();
  for(const auto *id:{"A","B"}) {
    nodes.push_back({{"id",id},{"url","/data/figure.dsf#figure"},{"geometries",{{{"id",std::string(id)+"-mesh"},{"url","/data/figure.dsf#geometry"}}}}});
    nodes.push_back({{"id",std::string(id)+"-head"},{"url","/data/figure.dsf#head"},{"parent",std::string("#")+id}});
  }
  nodes[0]["extra"]=favorite_extra({"Shape%20Name/Value","Scale","%E6%9C%AA%E4%BD%BF%E7%94%A8%E5%BD%A2%E6%80%81","Unknown/Value"});
  nodes[1]["extra"]=favorite_extra({"XRotate"});nodes[2]["extra"]=favorite_extra(J::array());nodes[3]["extra"]=favorite_extra({"YRotate"});
  J modifiers=J::array();for(const auto *id:{"A","B"}) modifiers.push_back({{"url","/data/Morphs/shape.dsf#shape"},{"parent",std::string("#")+id},{"channel",{{"current_value",std::string(id)=="A"?.25:.75}}}});
  J materials=J::array();for(const auto *id:{"A","B"}) materials.push_back({{"id",std::string(id)+"-material"},{"geometry",std::string("#")+id+"-mesh"},{"groups",{"surface"}},{"diffuse",{{"channel",{{"value",{.5,.5,.5}}}}}}});
  const auto file=root/"scene.duf";write(file,{{"scene",{{"nodes",nodes},{"modifiers",modifiers},{"materials",materials}}}});
  auto loaded=daz::load(file,{{root},false});auto catalog=daz::discover_morphs(loaded,{root},{},true);
  require(catalog.targets.size()==2,"收藏测试场景没有两个角色");
  const auto &a=catalog.targets[0],&b=catalog.targets[1];
  require(a.favorites.at("").contains("Shape Name")&&a.favorites.at("").contains("未使用形态"),"收藏名称解码或 /Value 解析失败");
  require(b.favorites.at("").empty()&&a.favorites.at("head")==std::set<std::string>{"XRotate"}&&b.favorites.at("head")==std::set<std::string>{"YRotate"},"实例目录复用混淆了角色或骨骼收藏");
  runtime::Properties av,bv;for(const auto &m:a.morphs) av.morphs.push_back(m.initial);for(const auto &m:b.morphs) bv.morphs.push_back(m.initial);
  size_t shape_index=0;while(shape_index<a.morphs.size()&&(a.morphs[shape_index].channel_id!="shape"||!a.morphs[shape_index].scene_channel)) ++shape_index;
  require(shape_index<a.morphs.size()&&av.morphs[shape_index]==.25f&&bv.morphs[shape_index]==.75f,"收藏读取改变了实例保存值");
  editor::ParameterControl scale;scale.id="transform/general_scale";scale.favorite_name="Scale";scale.label="Scale";scale.read=[] {return 100.;};scale.write=[](double){};
  auto old=scale;old.id="old-global";old.favorite_name="Unused";
  QSettings().setValue("parameters/favorites",QStringList{"old-global"});
  editor::ParameterPanel panel;panel.resize(500,650);panel.set_extra({scale,old});panel.bind(&a,&av);panel.show();app.processEvents();
  auto group=[&](editor::ParameterPanel &p,const char *id){auto *tree=p.findChild<QTreeWidget *>("parameterGroups");for(QTreeWidgetItemIterator it(tree);*it;++it) if((*it)->data(0,Qt::UserRole).toString()==id) {tree->setCurrentItem(*it);app.processEvents();return;}throw std::runtime_error("缺少收藏分类");};
  auto count=[&](editor::ParameterPanel &p){auto *tree=p.findChild<QTreeWidget *>("parameterRows");int result=0;for(int i=0;i<tree->topLevelItemCount();++i) result+=!tree->topLevelItem(i)->isHidden();return result;};
  auto button=[&](editor::ParameterPanel &p,const std::string &id){auto *tree=p.findChild<QTreeWidget *>("parameterRows");for(int i=0;i<tree->topLevelItemCount();++i) if(auto *w=tree->itemWidget(tree->topLevelItem(i),0)) if(auto *spin=w->findChild<QDoubleSpinBox *>("valueSpin");spin&&spin->property("parameterId").toString().toStdString()==id) return w->findChild<QToolButton *>("favoriteButton");return static_cast<QToolButton *>(nullptr);};
  group(panel,"@favorites");require(count(panel)==3,"收藏页遗漏零值参数、缩放，或混入全局收藏 / 未知通道");
  const auto shape_id=a.morphs[shape_index].source+"#"+a.morphs[shape_index].id;
  auto *star=button(panel,shape_id);require(star&&star->text()==QStringLiteral("★"),"场景 Morph 没有亮星");
  star->click();app.processEvents();require(count(panel)==2,"取消收藏没有立即移除该行");
  panel.bind(&b,&bv);app.processEvents();require(count(panel)==0,"切换角色丢失收藏分类或污染空收藏列表");
  group(panel,"*");star=button(panel,shape_id);require(star,"第二角色缺少形态收藏按钮");star->click();app.processEvents();group(panel,"@favorites");require(count(panel)==1,"第二角色不能独立增加收藏");
  panel.bind(&a,&av);app.processEvents();require(count(panel)==2,"第二角色编辑改变了第一角色的收藏");
  auto x=scale,y=scale;x.id="joint/3";x.favorite_name="XRotate";y.id="joint/4";y.favorite_name="YRotate";
  panel.set_extra({x,y});panel.bind(&a,&av,"head");app.processEvents();require(count(panel)==1&&button(panel,x.id)&&button(panel,x.id)->text()==QStringLiteral("★"),"骨骼收藏没有映射真实旋转通道");
  panel.bind(&b,&bv,"head");app.processEvents();require(count(panel)==1&&button(panel,y.id)&&button(panel,y.id)->text()==QStringLiteral("★"),"同名骨骼继承了其他角色收藏");
  editor::ParameterPanel reopened;reopened.resize(500,650);reopened.set_extra({scale});reopened.bind(&a,&av);reopened.show();app.processEvents();group(reopened,"@favorites");require(count(reopened)==2,"重新创建面板丢失取消收藏覆盖");
  auto other_scene=a;other_scene.favorite_scope+="-other";reopened.bind(&other_scene,&av);app.processEvents();require(count(reopened)==3,"收藏覆盖泄漏到其他场景");
  require(QSettings().value("parameters/favorites").toStringList()==QStringList{"old-global"},"场景收藏被写入全局收藏");
  require(av.morphs[shape_index]==.25f&&bv.morphs[shape_index]==.75f,"加星操作改变了参数值");
  auto refreshed=daz::discover_morphs(loaded,{root},{},true);require(refreshed.targets[0].favorites==a.favorites,"刷新目录丢失场景收藏");
  require(daz::read_document_file(file).at("scene").at("nodes")==nodes,"收藏编辑改写源 DUF");
}
