static QTreeWidgetItem *selection_set_item(MaterialPanel &panel,size_t instance,const QString &name){
  auto *tree=panel.findChild<QTreeWidget *>("materialSurfaces");for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole+3).isValid()&&(*it)->data(0,Qt::UserRole).toULongLong()==instance&&(*it)->text(0)==name)return *it;return nullptr;
}
static void click_selection_set(QApplication &app,MaterialPanel &panel,QTreeWidgetItem *item,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
  check(item,"缺少原生材质选择集");auto *tree=panel.findChild<QTreeWidget *>("materialSurfaces");for(auto *parent=item->parent();parent;parent=parent->parent())parent->setExpanded(true);tree->scrollToItem(item);app.processEvents();QTest::mouseClick(tree->viewport(),Qt::LeftButton,modifiers,tree->visualItemRect(item).center());app.processEvents();
}
static void wait_uv_choices(QApplication &app,MaterialPanel &panel){
  for(int wait=0;wait<500;++wait){app.processEvents();auto *combo=panel.findChild<QComboBox *>("material/uv_set");if(combo&&combo->isEnabled())return;QTest::qWait(10);}throw std::runtime_error("UV Set 列表未就绪");
}
static void material_uv_selection(QApplication &app,const std::filesystem::path &folder){
  namespace fs=std::filesystem;const auto root=folder/"uv-selection",extra_root=folder/"uv-extra";fs::create_directories(root/"data/Figure/UV Sets");fs::create_directories(extra_root/"data/Figure/UV Sets");
  const J uv={{"id","base"},{"label","Base UV"},{"vertex_count",4},{"uvs",{{"count",4},{"values",{{0,0},{1,0},{1,1},{0,1}}}}}};
  J geometry={{"id","mesh"},{"vertices",{{"count",4},{"values",{{0,0,0},{100,0,0},{100,100,0},{0,100,0}}}}},{"polygon_material_groups",{{"count",4},{"values",{"Body","Head","Fingernails","Toenails"}}}},
    {"polylist",{{"count",4},{"values",{{0,0,0,1,2},{0,1,0,2,3},{0,2,0,1,3},{0,3,1,2,3}}}}},{"default_uv_set","#base"},
    {"extra",J::array({{{"type","material_selection_sets"},{"material_selection_sets",J::array({{{"name","Nails"},{"materials",{"Fingernails","Toenails","Missing"}}},{{"name","Legacy"}},{{"name","Torso"},{"parent","Legacy"},{"materials",{"Body","Head"}}}})}}})}};
  const auto model_file=root/"data/Figure/model.dsf",scene_file=root/"scene.duf";
  std::ofstream(model_file)<<J{{"node_library",J::array({{{"id","root"},{"type","node"}}})},{"geometry_library",J::array({geometry})},{"uv_set_library",J::array({uv})}};
  J source={{"scene",{{"nodes",J::array()},{"materials",J::array()}}}};
  for(const auto *name:{"figure","other"}){const auto shape=std::string(name)+"Shape";source["scene"]["nodes"].push_back({{"id",name},{"url","data/Figure/model.dsf#root"},{"geometries",J::array({{{"id",shape},{"url","data/Figure/model.dsf#mesh"}}})}});
    for(const auto &slot:geometry["polygon_material_groups"]["values"])source["scene"]["materials"].push_back({{"id",std::string(name)+slot.get<std::string>()},{"geometry","#"+shape},{"groups",J::array({slot})}});
  }
  std::ofstream(scene_file)<<source;auto alternative=uv;alternative["id"]="alternate";alternative["label"]="Alternate UV";alternative["uvs"]["values"][0]={.2,.3};alternative["polygon_vertex_indices"]={{0,0,4}};alternative["uvs"]["count"]=5;alternative["uvs"]["values"].push_back({.7,.8});
  std::ofstream(extra_root/"data/Figure/UV Sets/alternate.dsf")<<J{{"uv_set_library",J::array({alternative})}};
  auto invalid=alternative;invalid["id"]="bad";invalid["polygon_vertex_indices"]={{0,3,4}};std::ofstream(root/"data/Figure/UV Sets/bad.dsf")<<J{{"uv_set_library",J::array({invalid})}};
  fs::create_directories(root/"data/Unrelated/UV Sets");std::ofstream(root/"data/Unrelated/UV Sets/wrong.dsf")<<J{{"uv_set_library",J::array({alternative})}};
  const std::vector<fs::path> roots{root,extra_root};auto d=std::make_shared<Document>();d->generation=1;d->source_file=scene_file;d->loaded=daz::load(scene_file,{roots,false});d->catalog=daz::discover_morphs(d->loaded,roots);d->skeletons=daz::load_skeletons(d->loaded);d->formulas=daz::enable_formulas(d->catalog,d->skeletons);auto snapshot=initial_snapshot(*d);
  check(d->loaded.objects[0].material_selection_sets.size()==3,"原生选择集没有加载");const auto catalog=daz::discover_material_uv_sets(d->loaded,0,{roots,false});check(catalog.sets.size()==2&&!catalog.warnings.empty(),"UV 目录未合并多内容库或错误接缝未过滤");
  const auto original=daz::material_uv_set(d->loaded.scene.materials.at(d->loaded.scene.instances[0].materials[0]));check(original.label=="Base UV","默认 UV 没有解析名称");
  const auto alt=*std::find_if(catalog.sets.begin(),catalog.sets.end(),[](const auto &set){return set.label=="Alternate UV";});
  const auto before=d->loaded.scene;check(change_material_uv(*d,alt,{{0,0}}),"UV 切换未生效");const auto &scene=d->loaded.scene;const auto &mesh=scene.meshes.at(scene.instances[0].mesh);
  check(mesh.triangles[0].uv[0]==ir::Vec2{.7f,.8f}&&mesh.polygons[0].uv[0]==ir::Vec2{.7f,.8f},"UV 接缝未更新三角形与原始多边形");
  check(mesh.triangles[1].uv==before.meshes[before.instances[0].mesh].triangles[1].uv,"UV 切换影响了未选表面");check(scene.meshes[scene.instances[1].mesh].triangles==before.meshes[before.instances[1].mesh].triangles,"UV 切换影响了共享网格的另一对象");
  check(daz::material_uv_set(scene.materials.at(scene.instances[0].materials[0])).label=="Alternate UV","显示名称没有更新");check(!change_material_uv(*d,alt,{{0,0}}),"同值 UV 切换产生多余历史");
  save_scene_extension(extension_path(scene_file),*d,snapshot);auto restored=load_scene_extension(extension_path(scene_file),roots,2);const auto &saved=restored.document->loaded.scene;
  check(saved.meshes[saved.instances[0].mesh].triangles[0].uv[0]==ir::Vec2{.7f,.8f}&&daz::material_uv_set(saved.materials[saved.instances[0].materials[0]]).label=="Alternate UV","UV 修改未随 DUFEX 保存恢复");
  check(reset_material_uv(*d,{{0,0}})&&!daz::material_uv_baseline(d->loaded.scene.materials[d->loaded.scene.instances[0].materials[0]]),"UV 还原未清除修改状态");
  check(d->loaded.scene.meshes[d->loaded.scene.instances[0].mesh].triangles[0].uv[0]==ir::Vec2{0,0},"UV 还原未恢复原坐标");
  change_material_uv(*d,alt,{{0,0}});const auto copied=copy_material(d->loaded.scene,snapshot.material_overrides,{0,0});check(paste_material(*d,snapshot,copied,{{1,0}}).empty(),"兼容拓扑粘贴错误地跳过 UV");
  check(d->loaded.scene.meshes[d->loaded.scene.instances[1].mesh].triangles[0].uv[0]==ir::Vec2{.7f,.8f},"材质粘贴没有应用复制的 UV");
  reset_material_uv(*d,{{0,0}});auto incompatible=copied;incompatible["uv_topology"]="different";incompatible["parameters"]["roughness"]=.17;
  check(!paste_material(*d,snapshot,incompatible,{{0,0}}).empty(),"不兼容 UV 粘贴未提示");check(daz::material_uv_set(d->loaded.scene.materials[d->loaded.scene.instances[0].materials[0]]).uri==original.uri&&d->loaded.scene.materials[d->loaded.scene.instances[0].materials[0]].roughness==.17f,"跳过 UV 时没有保留目标 UV 或未粘贴其他参数");
  MaterialPanel panel;panel.resize(920,750);panel.uv_requested=[&](const auto &set,const auto &surfaces){auto next=std::make_shared<Document>(*d);if(change_material_uv(*next,set,surfaces)){d=next;const auto selected=panel.selection_ids();panel.bind(d,&snapshot,0);panel.restore_selection(selected);}};panel.bind(d,&snapshot,0);panel.show();app.processEvents();
  click_selection_set(app,panel,selection_set_item(panel,0,"Nails"));const auto nails=panel.selected_surfaces();check(nails.size()==2&&nails[0].instance==0&&nails[0].slot==2&&nails[1].slot==3,"Nails 未选中同一对象的两片指甲");
  auto *tree=panel.findChild<QTreeWidget *>("materialSurfaces");check(tree->selectedItems().size()==2&&tree->selectedItems()[0]->data(0,Qt::UserRole+1).isValid(),"选择集没有高亮实际材质条目");
  click_selection_set(app,panel,selection_set_item(panel,0,"Torso"),Qt::ControlModifier);check(panel.selected_surfaces().size()==4,"Ctrl 没有追加选择集");
  click_selection_set(app,panel,selection_set_item(panel,0,"Nails"),Qt::ControlModifier);check(panel.selected_surfaces().size()==2&&panel.selected_surfaces()[0].slot==0,"Ctrl 没有取消选择集");
  click_selection_set(app,panel,selection_set_item(panel,0,"Legacy"));check(panel.selected_surfaces().size()==2,"父选择集没有包含子集合");
  wait_uv_choices(app,panel);auto *combo=panel.findChild<QComboBox *>("material/uv_set");const auto index=combo->findText("Alternate UV");check(index>=0,"UV 下拉框缺少跨库选项");combo->setCurrentIndex(index);QMetaObject::invokeMethod(combo,"activated",Qt::DirectConnection,Q_ARG(int,index));app.processEvents();
  for(size_t slot:{0,1})check(daz::material_uv_set(d->loaded.scene.materials[d->loaded.scene.instances[0].materials[slot]]).uri==alt.uri,"UV 下拉框没有批量切换");
  panel.restore_selection({{d->loaded.scene.instances[0].id,"Body"},{d->loaded.scene.instances[0].id,"Fingernails"}});wait_uv_choices(app,panel);check(panel.findChild<QComboBox *>("material/uv_set")->currentText()==QStringLiteral("多值"),"多选 UV 没有显示多值");
  auto *scroll=panel.findChild<QScrollArea *>()->verticalScrollBar();scroll->setValue(350);const int position=scroll->value();click_selection_set(app,panel,selection_set_item(panel,0,"Nails"));check(scroll->value()==position,"切换选择集重置滚动位置");
  // Malformed hierarchy must not recurse forever or select another object.
  d->loaded.objects[0].material_selection_sets={{"A","B",{"Body"}},{"B","A",{"Head"}}};auto cyclic=std::make_shared<Document>(*d);panel.bind(cyclic,&snapshot,0);check(selection_set_item(panel,0,"A"),"循环分组未安全显示");
  QThreadPool::globalInstance()->waitForDone();app.processEvents();
}
static void real_uv_selection(QApplication &app,const QStringList &args){
  namespace fs=std::filesystem;check(args.size()>=6,"--uv-selection scene material output root...");const auto file=fs::path(args[2].toStdWString()),preset_file=fs::path(args[3].toStdWString()),output=fs::path(args[4].toStdWString());std::vector<fs::path> roots;for(int n=5;n<args.size();++n)roots.emplace_back(args[n].toStdWString());
  auto d=std::make_shared<Document>();d->generation=1;d->source_file=file;d->loaded=daz::load(file,{roots,false});size_t lee=0,big=0;
  for(const auto &o:d->loaded.objects){runtime::Target t;t.instance=o.instance;t.id=d->loaded.scene.instances[o.instance].id;t.label=o.label;d->catalog.targets.push_back(t);if(o.label=="Lee 8 (2)")lee=d->catalog.targets.size()-1;if(o.label=="big_01")big=d->catalog.targets.size()-1;}
  auto snapshot=initial_snapshot(*d);const auto li=d->catalog.targets[lee].instance,bi=d->catalog.targets[big].instance;J report;MaterialPanel panel;panel.resize(1080,820);panel.uv_requested=[](const auto &,const auto &){};panel.bind(d,&snapshot,int(lee));panel.show();app.processEvents();
  click_selection_set(app,panel,selection_set_item(panel,li,"Nails"));auto nails=panel.selected_surfaces();check(nails.size()==2,"Lee Nails 不含两片指甲");report["Lee_Nails"]=J::array();for(auto s:nails){check(s.instance==li,"Lee Nails 跨对象选择");report["Lee_Nails"].push_back(d->loaded.scene.meshes[d->loaded.scene.instances[s.instance].mesh].material_slots[s.slot]);}
  wait_uv_choices(app,panel);report["Lee_current"]=panel.findChild<QComboBox *>("material/uv_set")->currentText().toStdString();check(report["Lee_current"]=="Base Male","Lee 当前 UV 显示错误");
  const auto lc=daz::discover_material_uv_sets(d->loaded,li,{roots,false});report["Lee_choices"]=J::array();for(const auto &set:lc.sets)report["Lee_choices"].push_back(set.label);
  panel.bind(d,&snapshot,int(big));click_selection_set(app,panel,selection_set_item(panel,bi,"Torso"));check(panel.selected_surfaces().size()==2,"G8.1 Legacy Torso 没有选中 Body/Head");wait_uv_choices(app,panel);check(panel.findChild<QComboBox *>("material/uv_set")->currentText()=="Base 8.1 Female","G8.1 当前 UV 显示错误");
  const auto bc=daz::discover_material_uv_sets(d->loaded,bi,{roots,false});report["big_choices"]=J::array();for(const auto &set:bc.sets)report["big_choices"].push_back(set.label);auto base=std::find_if(bc.sets.begin(),bc.sets.end(),[](const auto &set){return set.label=="Base Female";});check(base!=bc.sets.end(),"G8.1 没有继承兼容的 Base Female UV");
  const auto selected=panel.selected_surfaces();check(change_material_uv(*d,*base,selected),"真实 G8.1 UV 切换没有生效");for(auto s:selected)check(daz::material_uv_set(d->loaded.scene.materials[d->loaded.scene.instances[s.instance].materials[s.slot]]).label=="Base Female","真实多选切换没有更新名称");check(reset_material_uv(*d,selected),"真实 UV 无法还原");
  const auto changed=apply_materials(*d,big,daz::load(preset_file,{roots,false}),&snapshot);report["Ruo_matched"]=changed;check(changed==17,"Ruo Xi 材质未完整匹配");
  report["Ruo_uvs"]=J::object();const auto &i=d->loaded.scene.instances[bi];const auto &slots=d->loaded.scene.meshes[i.mesh].material_slots;for(size_t s=0;s<slots.size();++s){const auto current=daz::material_uv_set(d->loaded.scene.materials[i.materials[s]]);report["Ruo_uvs"][slots[s]]=current.label;check(current.label=="Base Female","应用 Ruo Xi 后 UV 元数据不正确");}
  auto updated=std::make_shared<Document>(*d);panel.bind(updated,&snapshot,int(big));click_selection_set(app,panel,selection_set_item(panel,bi,"Torso"));wait_uv_choices(app,panel);check(panel.findChild<QComboBox *>("material/uv_set")->currentText()=="Base Female","预设后界面仍显示旧 UV");panel.findChild<QLineEdit *>("materialSearch")->setText("UV");app.processEvents();panel.grab().save(QString::fromStdWString((output.parent_path()/"uv-selection-panel.png").wstring()));
  report["status"]="PASS";std::ofstream(output)<<report.dump(2);QThreadPool::globalInstance()->waitForDone();std::cout<<report.dump(2)<<std::endl;
}
