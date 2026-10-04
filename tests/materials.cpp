#include "editor/material_panel.h"
#include "render_ir/emission.h"
#include <QFontDatabase>
#include "editor/ui_scale.h"
#include "editor/scene_extension.h"
#include "daz/documents.h"
#include "daz/material_animation.h"
#include "daz/material_uv.h"
#include <QApplication>
#include <QTreeWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMainWindow>
#include <QDockWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QScreen>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QToolButton>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QMenu>
#include <QTimer>
#include <QThreadPool>
#include <fstream>
#include <iostream>

using namespace dfv;using namespace dfv::editor;using J=nlohmann::json;
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static std::shared_ptr<Document> fixture(){
  auto d=std::make_shared<Document>();d->generation=7;auto &s=d->loaded.scene;
  ir::Material m;m.id="shared";s.materials={m};ir::Mesh mesh;mesh.id="mesh";mesh.material_slots={"Skin","Nails"};mesh.positions={{0,0,0},{1,0,0},{0,0,1}};ir::Triangle triangle;triangle.vertices={0,1,2};mesh.triangles={triangle};s.meshes={mesh};
  for(const auto *name:{"figure","dress","other"}){ir::Instance i;i.id=name;i.materials={0,0};s.instances.push_back(i);runtime::Target t;t.id=std::string(name)+"/geometry";t.label=name;t.instance=uint32_t(d->catalog.targets.size());if(t.instance==1){t.parent="#figure";t.conform_target="#figure";t.ancestors={"#figure"};}d->catalog.targets.push_back(t);daz::AssetObject o;o.id=name;o.instance=t.instance;o.figure=true;o.auto_fit_base=t.instance==1?"":"Genesis8Female";o.conform_target=t.conform_target;d->loaded.objects.push_back(o);}
  d->formulas.graphs.resize(3);s.validate();return d;
}
static void apply_library_sample(daz::LoadedScene preset){
  // 合成网格只验证参数替换；真实 UV 拓扑和层级身份由独立的精确夹具验证。
  for(auto &m:preset.scene.materials)if(!m.source_definition.empty()){auto j=J::parse(m.source_definition);j.erase("uv_set");j.erase("target_node");m.source_definition=j.dump();}
  auto d=fixture();auto &scene=d->loaded.scene;std::set<std::string> groups;for(const auto &m:preset.report.at("materials"))for(const auto &g:m.at("groups"))groups.insert(g=="*"?"TestSurface":g.get<std::string>());check(!groups.empty(),"预设没有可应用的材质组");scene.meshes[0].material_slots.assign(groups.begin(),groups.end());for(auto &i:scene.instances)i.materials.assign(groups.size(),0);
  auto snapshot=initial_snapshot(*d);for(const auto &g:groups){snapshot.material_overrides["figure"][g]["opacity"]=.37;snapshot.material_overrides["figure"][g]["u_offset"]=.123;}
  check(apply_materials(*d,0,preset,&snapshot)>0,"真实预设未匹配对象表面");scene.validate();auto serialized=snapshot_json(*d,snapshot);auto restored=initial_snapshot(*d);apply_snapshot_json(*d,restored,serialized);check(restored.material_overrides==snapshot.material_overrides,"真实预设替换后的参数不能保存恢复");
  for(size_t i=preset.report.value("hierarchical_material",false)?2:1;i<scene.instances.size();++i)for(auto m:scene.instances[i].materials)check(scene.materials[m].base_color==ir::Material{}.base_color,"真实预设修改泄漏至其他对象");
}
static void nested_props(QApplication &app) {
  auto d=fixture();auto &scene=d->loaded.scene;d->loaded.nodes={{"outer","","Outer",true},{"nested","#figure","Nested",true}};
  d->loaded.objects[0].figure=false;d->loaded.objects[0].auto_fit_base.clear();d->loaded.objects[0].parent="#outer";
  d->loaded.objects[1].figure=false;d->loaded.objects[1].parent="#nested";d->loaded.objects[1].conform_target.clear();
  d->catalog.targets[0].parent="#outer";d->catalog.targets[1].parent="#nested";d->catalog.targets[1].conform_target.clear();d->catalog.targets[1].ancestors={"#nested","#figure","#outer"};
  auto glow=scene.materials[0];glow.id="glow";glow.emission_luminance=25;scene.materials.push_back(glow);scene.instances[1].materials={1,1};
  auto snapshot=initial_snapshot(*d);MaterialPanel panel;panel.resize(850,650);panel.bind(d,&snapshot,0);panel.show();app.processEvents();
  auto *tree=panel.findChild<QTreeWidget *>("materialSurfaces");
  check(tree->topLevelItemCount()==1&&panel.selected_surfaces().size()==4,"选择 Prop 母组没有递归包含深层子 Prop 材质");
  auto *strength=panel.findChild<QDoubleSpinBox *>("material/emission_luminance");check(strength&&strength->value()==0,"混合自发光测试首项应为零");
  auto *input=strength->findChild<QLineEdit *>();input->setFocus();input->selectAll();QTest::keyClicks(input,"0");QTest::keyClick(input,Qt::Key_Return);app.processEvents();
  auto rendered=scene;apply_material_overrides(rendered,scene,snapshot.material_overrides);check(rendered.materials[rendered.instances[1].materials[0]].emission_luminance==0&&rendered.materials[rendered.instances[1].materials[1]].emission_luminance==0,"混合选择输入零没有关闭后代自发光");
  check(!snapshot.material_overrides.contains("other"),"批量自发光修改污染其他对象");
  panel.bind(d,&snapshot,-4,"outer");check(panel.selected_surfaces().size()==4&&tree->topLevelItem(0)->text(0)=="Outer","空组无法递归选择材质");
  panel.bind(d,&snapshot,-4,"nested");check(panel.selected_surfaces().size()==2,"切换空组后选中范围不正确");
}
static void file_roundtrip(const std::filesystem::path &folder){
  auto source=J::parse(R"({"node_library":[{"id":"root","type":"node"}],"geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[0,0,0],[100,0,0],[0,100,0]]},"polygon_material_groups":{"count":1,"values":["Skin"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],"uv_set_library":[{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}],"scene":{"nodes":[{"id":"object","url":"#root","geometries":[{"id":"shape","url":"#mesh"}]}],"materials":[{"id":"mat","geometry":"#shape","groups":["Skin"]}]}})");
  const auto file=folder/"scene.duf",preset_file=folder/"shader.duf";std::ofstream(file)<<source.dump();
  auto shader=J::parse(R"({"asset_info":{"type":"preset_shader"},"material_library":[{"id":"Default","diffuse":{"channel":{"id":"diffuse","value":[0.8,0.2,0.1]}},"extra":[{"type":"studio/material/uber_iray","channels":[{"channel":{"id":"Diffuse Overlay Weight","value":0.5}},{"channel":{"id":"Specular Lobe 1 Roughness","value":0.4}},{"channel":{"id":"Unsupported Test","type":"float","value":3}}]}]}],"scene":{"materials":[{"url":"#Default"}]}})");std::ofstream(preset_file)<<shader.dump();
  Document d;d.generation=1;d.source_file=file;d.loaded=daz::load(file,{{folder},false});d.catalog=daz::discover_morphs(d.loaded,{folder});d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);auto snapshot=initial_snapshot(d);auto preset=daz::load(preset_file,{{folder},false});
  check(preset.scene.materials[0].overlay_weight==.5f&&preset.report["materials"][0]["groups"][0]=="*","通用 Shader 引用或覆盖层导入失败");check(std::any_of(preset.scene.materials[0].source_channels.begin(),preset.scene.materials[0].source_channels.end(),[](const auto &c){return c.id=="Unsupported Test"&&!c.mapped;}),"未映射源通道丢失");
  apply_surface_materials(d,snapshot,preset,{{0,0}});const auto id=d.loaded.scene.instances[0].id;snapshot.material_overrides[id]["Skin"]["roughness"]=.72;save_scene_extension(extension_path(file),d,snapshot);auto restored=load_scene_extension(extension_path(file),{folder},2);check(restored.snapshot.material_overrides.at(id).at("Skin").at("roughness")==.72,"磁盘 DUFEX 未恢复材质参数");check(restored.document->loaded.scene.materials.at(restored.document->loaded.scene.instances[0].materials[0]).overlay_weight==.5f,"DUFEX 未重放所选表面 Shader 预设");
  J unchanged;std::ifstream(file)>>unchanged;check(unchanged==source,"编辑覆盖了源 DUF");
  // 只写贴图 / 某个参数的局部预设，不能清除其他手动修改；操作和覆盖共同写入 DUFEX。
  const auto partial_file=folder/"makeup.duf";const auto map_file=folder/"makeup.png";QImage map(4,4,QImage::Format_RGB32);map.fill(Qt::blue);check(map.save(QString::fromStdWString(map_file.wstring())),"妆容贴图写入失败");
  auto partial=J::parse(R"({"asset_info":{"type":"preset_material"},"scene":{"materials":[{"id":"makeup","groups":["Skin"],"diffuse":{"channel":{"id":"diffuse","image_file":"makeup.png"}}}]}})");std::ofstream(partial_file)<<partial.dump();
  auto &rd=*restored.document;auto &rs=restored.snapshot;rs.material_overrides[id]["Skin"]["base_color"]=J::array({3.,2.,1.});rs.material_overrides[id]["Skin"]["opacity"]=.65;
  apply_materials(rd,0,daz::load(partial_file,{{folder},false}),&rs);check(rs.material_overrides[id]["Skin"].contains("base_color")&&rs.material_overrides[id]["Skin"]["roughness"]==.72,"只换贴图覆盖了颜色或粗糙度");
  save_scene_extension(extension_path(file),rd,rs);auto replayed=load_scene_extension(extension_path(file),{folder},3);auto textures=replayed.document->loaded.scene.textures;auto effective=effective_material(replayed.document->loaded.scene,replayed.snapshot.material_overrides,0,0,textures);
  check(effective.base_color.x==3&&effective.roughness==.72f&&effective.opacity==.65f&&effective.overlay_weight==.5f&&effective.color_texture>=0&&textures[size_t(effective.color_texture)].file==map_file,"局部替换与 HDR 颜色在 DUFEX 重开后不一致");
  partial["scene"]["materials"][0]["diffuse"]["channel"]={{"id","diffuse"},{"value",{.1,.3,.8}},{"image_file",""}};const auto cleared_file=folder/"clear.duf";std::ofstream(cleared_file)<<partial.dump();
  apply_materials(rd,0,daz::load(cleared_file,{{folder},false}),&rs);check(!rs.material_overrides[id]["Skin"].contains("base_color")&&rs.material_overrides[id]["Skin"].contains("roughness")&&rd.loaded.scene.materials.at(rd.loaded.scene.instances[0].materials[0]).color_texture<0,"显式改色 / 清图未覆盖对应参数");
  auto edited=replayed.snapshot;remove_target(*replayed.document,edited,0);check(edited.material_overrides.empty(),"删除对象遗留材质覆盖");
  // 常见 DAZ 材质把默认值放进 material_library，真正的当前值位于零帧通道。
  auto animated=J::parse(R"({"asset_info":{"type":"preset_material"},"material_library":[{"id":"Skin","diffuse":{"channel":{"id":"diffuse","value":[0.75,0.75,0.75]}}}],"scene":{"materials":[{"url":"#Skin","groups":["Skin"]}],"animations":[{"url":"name://@selection#materials/Skin:?diffuse/value","keys":[[0,[0.2,0.4,0.6]]]},{"url":"name://@selection#materials/Skin:?diffuse/image_file","keys":[[0,"makeup.png"]]}]}})");
  animated["uv_set_library"]=J::parse(R"([{"id":"Alt","vertex_count":3,"uvs":{"count":3,"values":[[0.25,0.75],[0.5,0.5],[0.75,0.25]]}}])");animated["scene"]["animations"].push_back({{"url","name://@selection#materials/Skin:?uv_set"},{"keys",J::array({{0,"#Alt"}})}});
  animated["scene"]["animations"].push_back({{"url","name://@selection#simulation:?extra/studio_modifier_channels/channels/Friction/value"},{"keys",J::array({{0,.2}})}});
  const auto animated_file=folder/"animated.duf";std::ofstream(animated_file)<<animated.dump();const auto kind=daz::inspect_contents(animated);check(kind.materials&&!kind.properties&&!kind.instantiate,"材质零帧或附带模拟通道被误判为角色姿势");auto loaded=daz::load(animated_file,{{folder},false});check(loaded.scene.materials[0].base_color.x<.04f&&loaded.scene.materials[0].color_texture>=0,"零帧材质值或贴图未覆盖定义中的默认值");
  apply_materials(rd,0,loaded,&rs);check(rd.loaded.scene.meshes[rd.loaded.scene.instances[0].mesh].triangles[0].uv[0]==ir::Vec2{.25f,.75f},"预设指定的 UV Set 未应用");save_scene_extension(extension_path(file),rd,rs);auto uv_restored=load_scene_extension(extension_path(file),{folder},4);check(uv_restored.document->loaded.scene.meshes[uv_restored.document->loaded.scene.instances[0].mesh].triangles[0].uv[0]==ir::Vec2{.25f,.75f},"DUFEX 未重放 UV Set 替换");
  auto shared=fixture();auto shared_snapshot=initial_snapshot(*shared);apply_materials(*shared,0,loaded,&shared_snapshot);const auto &shared_scene=shared->loaded.scene;check(shared_scene.instances[0].mesh!=shared_scene.instances[1].mesh&&shared_scene.meshes[shared_scene.instances[1].mesh].triangles[0].uv[0]==ir::Vec2{},"替换 UV 修改了其他对象共享的网格");
  // 跨代材质链接可能保留旧的默认 UV 哈希；只接受文件内唯一同名 UV，仍检查目标拓扑。
  const auto uv_file=folder/"default.dsf";auto uv_data=animated["uv_set_library"];uv_data[0]["id"]="default-0x1234";
  auto write_uv=[&]{std::ofstream(uv_file)<<J{{"uv_set_library",uv_data}};};write_uv();auto linked=loaded.scene.materials[0];auto definition=J::parse(linked.source_definition);definition["uv_set"]="default.dsf#default-0x9876";linked.source_definition=definition.dump();
  auto compatible_uv=fixture();check(daz::apply_material_uv(compatible_uv->loaded.scene,0,0,linked,{{folder},false}),"旧 UV 哈希没有匹配同文件的唯一 UV");
  check(compatible_uv->loaded.scene.meshes[compatible_uv->loaded.scene.instances[0].mesh].triangles[0].uv[0]==ir::Vec2{.25f,.75f},"兼容 UV 使用了错误坐标");
  auto rejects_uv=[&](const char *message){bool failed=false;try{auto scene=fixture()->loaded.scene;daz::apply_material_uv(scene,0,0,linked,{{folder},false});}catch(const std::exception &){failed=true;}check(failed,message);};
  uv_data[0]["vertex_count"]=4;write_uv();rejects_uv("旧 UV 哈希绕过了顶点数检查");uv_data[0]["vertex_count"]=3;
  uv_data.push_back(uv_data[0]);uv_data[1]["id"]="default-0x5678";write_uv();rejects_uv("有歧义的同名 UV 被任意选取");
  definition["uv_set"]="default.dsf#default-0x1234";linked.source_definition=definition.dump();auto exact_uv=fixture();check(daz::apply_material_uv(exact_uv->loaded.scene,0,0,linked,{{folder},false}),"准确 UV ID 没有优先匹配");
  uv_data.erase(1);uv_data[0]["id"]="Other-0x1234";write_uv();definition["uv_set"]="default.dsf#default-0x9876";linked.source_definition=definition.dump();rejects_uv("不同名称 UV 被错误兼容");
  auto only=animated;only["scene"].erase("materials");only.erase("material_library");const auto only_file=folder/"only-animation.duf";std::ofstream(only_file)<<only.dump();check(daz::load(only_file,{{folder},false}).scene.materials.size()==1,"只有零帧通道的局部材质预设无法载入");
  const auto copied=copy_material(rd.loaded.scene,rs.material_overrides,{0,0});paste_material(rd,rs,copied,{{0,0}});save_scene_extension(extension_path(file),rd,rs);
  auto pasted=load_scene_extension(extension_path(file),{folder},5);check(copy_material(pasted.document->loaded.scene,pasted.snapshot.material_overrides,{0,0})==copied,"完整材质粘贴未随 DUFEX 保存重放");
  auto hierarchy=J::parse(R"({"asset_info":{"type":"preset_hierarchical_material"},"scene":{"nodes":[{"id":"figure","url":"name://@selection/figure:","geometries":[{"id":"figureShape","url":"name://@selection#geometries/figure:"}]},{"id":"dress","url":"name://@selection/dress:","parent":"#figure","geometries":[{"id":"dressShape","url":"name://@selection#geometries/dress:"}]}],"materials":[{"id":"skin","geometry":"#figureShape","groups":["Skin"]},{"id":"fabric","geometry":"#dressShape","groups":["Skin"]}],"animations":[{"url":"figure#materials/Skin:?diffuse/value","keys":[[0,[1,0,0]]]},{"url":"dress#materials/Skin:?diffuse/value","keys":[[0,[0,0,1]]]}]}})");
  const auto hierarchy_file=folder/"hierarchy.duf";std::ofstream(hierarchy_file)<<hierarchy.dump();check(!daz::inspect_contents(hierarchy).instantiate,"层级材质被误判为追加模型");auto hierarchy_preset=daz::load(hierarchy_file,{{folder},false});auto family=fixture();auto family_snapshot=initial_snapshot(*family);check(apply_materials(*family,0,hierarchy_preset,&family_snapshot)==2,"层级材质没有同时匹配角色和服装");const auto &family_scene=family->loaded.scene;check(family_scene.materials[family_scene.instances[0].materials[0]].base_color==ir::Vec3{1,0,0}&&family_scene.materials[family_scene.instances[1].materials[0]].base_color==ir::Vec3{0,0,1}&&family_scene.materials[family_scene.instances[2].materials[0]].base_color==ir::Material{}.base_color,"同名表面跨对象串用层级材质");
}
static void clipboard_ui(QApplication &app){
  auto d=fixture();auto snapshot=initial_snapshot(*d);auto &source=d->loaded.scene.materials[0];source.source_channels.push_back({"test","Source","float","0.4","",true});source.source_definition="{\"channels\":{}}";
  source.base_color={500,600,700};source.normal_strength=25;source.uv_scale={15000,20000}; // Valid imports may exceed editable ranges.
  snapshot.material_overrides["figure"]["Skin"]["roughness"]=.42;
  MaterialPanel panel;panel.resize(650,500);panel.bind(d,&snapshot,0);panel.show();app.processEvents();auto *tree=panel.findChild<QTreeWidget *>("materialSurfaces");
  auto *skin=tree->topLevelItem(0)->child(0)->child(0);auto *nails=skin->parent()->child(1);tree->setCurrentItem(skin);app.processEvents();
  auto *scroll=panel.findChild<QScrollArea *>()->verticalScrollBar();scroll->setValue(600);const auto position=scroll->value();check(position>0,"材质属性没有可滚动范围");
  tree->setCurrentItem(nails);tree->setCurrentItem(skin);tree->setCurrentItem(nails);app.processEvents();check(scroll->value()==position,"快速切换材质重置滚动位置");
  auto menu_action=[&](QTreeWidgetItem *item,const char *name){tree->setCurrentItem(item);tree->scrollToItem(item);app.processEvents();bool triggered=false;
    QTimer::singleShot(0,[&]{auto *menu=qobject_cast<QMenu *>(QApplication::activePopupWidget());if(!menu)return;auto *action=menu->findChild<QAction *>(name);if(action&&action->isEnabled()){triggered=true;menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);}else menu->close();});
    QMetaObject::invokeMethod(tree,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,tree->visualItemRect(item).center()));check(triggered,"复制粘贴菜单不可用");};
  menu_action(skin,"CopyMaterial");const auto expected=copy_material(d->loaded.scene,snapshot.material_overrides,{0,0});
  // Changing the source after copying must not alter the clipboard snapshot.
  snapshot.material_overrides["figure"]["Skin"]["roughness"]=.8;
  bool pasted=false;panel.paste_requested=[&](const auto &copy,const auto &surfaces){paste_material(*d,snapshot,copy,surfaces);pasted=true;};menu_action(nails,"PasteMaterial");
  check(pasted&&copy_material(d->loaded.scene,snapshot.material_overrides,{0,1})==expected,"粘贴未复制完整材质或引用了已改变的源");
  check(copy_material(d->loaded.scene,snapshot.material_overrides,{0,0})!=expected&&d->loaded.scene.materials[d->loaded.scene.instances[2].materials[1]].roughness==.5f,"粘贴修改了源或其他对象");
  auto cross=fixture();auto other=initial_snapshot(*cross);paste_material(*cross,other,expected,{{2,0},{2,1}});check(copy_material(cross->loaded.scene,other.material_overrides,{2,0})==expected&&copy_material(cross->loaded.scene,other.material_overrides,{2,1})==expected,"跨场景或多表面粘贴失败");
  auto compatible=fixture();compatible->loaded.scene.meshes[0].material_slots={"Body","Head"};compatible->loaded.objects[0].geometry_file="Genesis8_1Female.dsf";auto state=initial_snapshot(*compatible);daz::LoadedScene torso;
  torso.scene.materials.resize(1);torso.scene.materials[0].base_color={1,0,0};torso.report={{"materials",J::array({{{"groups",{"Torso"}}}})}};
  check(apply_materials(*compatible,0,torso,&state)==2,"G8 Torso 未覆盖 G8.1 Body 和 Head");
  torso.scene.materials.push_back({});torso.scene.materials[1].base_color={0,0,1};torso.report["materials"].push_back({{"groups",{"Head"}}});
  apply_materials(*compatible,0,torso,&state);const auto &s=compatible->loaded.scene;check(s.materials[s.instances[0].materials[1]].base_color==ir::Vec3{0,0,1},"Torso 别名覆盖了显式 Head 材质");
  bool rejected=false;try{apply_materials(*compatible,2,torso,&state);}catch(...){rejected=true;}check(!rejected,"显式 Head 名称未匹配普通物体");
  torso.scene.materials.resize(1);torso.report["materials"].erase(1);rejected=false;try{apply_materials(*compatible,2,torso,&state);}catch(...){rejected=true;}check(rejected,"G8.1 别名扩散到不相关模型");
}
#include "material_uv_selection.inl"
int main(int argc,char **argv){
  QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");app.setFont(QFont(QStringLiteral("Microsoft YaHei"),9));
  try{
    if(argc>1&&std::string(argv[1])=="--uv-selection"){real_uv_selection(app,app.arguments());return 0;}
    {QTemporaryDir uv_temp;check(uv_temp.isValid(),"UV 测试目录创建失败");material_uv_selection(app,std::filesystem::path(uv_temp.path().toStdWString()));}
    clipboard_ui(app);
    nested_props(app);
    if(argc>2&&std::string(argv[1])=="--group-scene"){
      const std::vector<std::filesystem::path> roots={"G:/G1","G:/G3"};auto d=std::make_shared<Document>();d->loaded=daz::load(std::filesystem::u8path(argv[2]),{roots,false});d->catalog=daz::discover_morphs(d->loaded,roots,{},true);auto snapshot=initial_snapshot(*d);
      int target=-1;for(size_t i=0;i<d->catalog.targets.size();++i)if(d->catalog.targets[i].label=="BodyPlaneT")target=int(i);check(target>=0,"真实场景缺少 BodyPlaneT");
      MaterialPanel panel;panel.resize(1050,760);panel.bind(d,&snapshot,target);panel.show();app.processEvents();const auto surfaces=panel.selected_surfaces();std::set<size_t> instances;J emitters=J::array();
      for(auto s:surfaces){instances.insert(s.instance);const auto &i=d->loaded.scene.instances[s.instance];const auto &m=d->loaded.scene.materials[i.materials[s.slot]];if(ir::emission_strength(m)>0)emitters.push_back({{"object",i.id},{"surface",d->loaded.scene.meshes[i.mesh].material_slots[s.slot]},{"luminance",m.emission_luminance}});}
      check(instances.size()>20&&surfaces.size()>50&&!emitters.empty(),"真实场景母组遗漏子 Prop 或自发光表面");
      auto *search=panel.findChild<QLineEdit *>("materialSearch");search->setText(QStringLiteral("自发光"));auto *spin=panel.findChild<QDoubleSpinBox *>("material/emission_luminance");auto *input=spin->findChild<QLineEdit *>();input->setFocus();input->selectAll();QTest::keyClicks(input,"0");QTest::keyClick(input,Qt::Key_Return);app.processEvents();
      for(auto s:surfaces){auto textures=d->loaded.scene.textures;check(effective_material(d->loaded.scene,snapshot.material_overrides,s.instance,s.slot,textures).emission_luminance==0,"真实场景仍有未关闭的后代自发光");}
      if(argc>3)panel.grab().save(QString::fromUtf8(argv[3]));std::cout<<J{{"objects",instances.size()},{"surfaces",surfaces.size()},{"emitters_before",emitters},{"all_luminance_zero",true}}.dump(2)<<'\n';return 0;
    }
    if(argc>1&&std::string(argv[1])=="--library"){
      J report=J::array();for(int i=2;i<argc;++i){auto loaded=daz::load(std::filesystem::u8path(argv[i]),{{"H:/g1","H:/g3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"},false});check(!loaded.scene.materials.empty(),"样本未导入材质");apply_library_sample(loaded);size_t channels=0,unmapped=0;for(auto &m:loaded.scene.materials){ir::validate(m,loaded.scene.textures.size());for(auto &c:m.source_channels){++channels;if(!c.mapped)++unmapped;}}report.push_back({{"file",argv[i]},{"materials",loaded.scene.materials.size()},{"channels",channels},{"unmapped",unmapped},{"warnings",loaded.report.value("warnings",J::array())}});}std::cout<<report.dump(2)<<'\n';return 0;
    }
    auto d=fixture();auto snapshot=initial_snapshot(*d);const auto original=d->loaded.scene;auto &patch=snapshot.material_overrides["figure"]["Skin"];patch["roughness"]=.83;patch["base_color"]=J::array({.7,.2,.1});patch["emission_units"]=4;patch["thin_walled"]=true;
    QTemporaryDir temp;check(temp.isValid(),"临时目录创建失败");file_roundtrip(std::filesystem::path(temp.path().toStdWString()));const auto texture=std::filesystem::path(temp.path().toStdWString())/L"纹理.png";QImage image(2,2,QImage::Format_RGB32);image.fill(Qt::red);check(image.save(QString::fromStdWString(texture.wstring())),"测试贴图创建失败");const auto path=texture.generic_u8string();patch["color_texture"]={{"file",std::string(path.begin(),path.end())}};
    auto scene=original;ir::Delta delta;check(apply_material_overrides(scene,original,snapshot.material_overrides,&delta),"首次独立覆盖未通知绑定变化");check(scene.materials.size()==2&&scene.instances[0].materials[0]==1&&scene.instances[0].materials[1]==0&&scene.instances[1].materials[0]==0,"共享材质修改串到其他表面/对象");check(scene.materials[1].roughness==.83f&&scene.materials[1].color_texture==0,"数值或贴图未应用");
    patch["roughness"]=.27;delta={};check(!apply_material_overrides(scene,original,snapshot.material_overrides,&delta)&&delta.materials.size()==1&&delta.materials[0].value.roughness==.27f,"连续修改没有形成材质 Delta");delta={};check(!apply_material_overrides(scene,original,snapshot.material_overrides,&delta)&&delta.materials.empty(),"同值重复更新");
    const auto state=snapshot_json(*d,snapshot);auto restored=initial_snapshot(*d);apply_snapshot_json(*d,restored,state);check(restored.material_overrides==snapshot.material_overrides,"DUFEX 材质往返丢失数据");auto reordered=original;std::swap(reordered.instances[0],reordered.instances[2]);auto result=reordered;apply_material_overrides(result,reordered,restored.material_overrides);check(result.materials.at(result.instances[2].materials[0]).roughness==.27f&&result.instances[0].materials[0]==0,"材质依赖实例索引");
    auto corrupt=state;corrupt["materials"]["figure"]["Skin"]["roughness"]=-2;bool rejected=false;try{apply_snapshot_json(*d,restored,corrupt);}catch(...){rejected=true;}check(rejected&&restored.material_overrides==snapshot.material_overrides,"无效保存文件留下部分修改");
    auto old=state;old.erase("materials");apply_snapshot_json(*d,restored,old);check(restored.material_overrides.empty(),"旧 DUFEX 不兼容");
    patch["color_texture"]=nullptr;apply_material_overrides(scene,original,snapshot.material_overrides);check(scene.textures.empty()&&scene.materials.back().color_texture==-1,"移除贴图残留绑定");
    for(const auto &p:material_parameters()){ir::Material m;auto textures=original.textures;if(p.kind==MaterialParameter::texture)continue;auto v=p.read(m);if(p.kind==MaterialParameter::number)v=(p.minimum+p.maximum)*.5;if(p.kind==MaterialParameter::choice)v=int(p.choices.size()-1);p.write(m,v);ir::validate(m,textures.size());}
    auto before=snapshot.material_overrides;daz::LoadedScene preset;preset.scene.materials.push_back({});preset.scene.materials[0].roughness=.95f;preset.report["materials"]=J::array({{{"groups",J::array({"Skin"})}}});apply_materials(*d,0,preset,&snapshot);check(snapshot.material_overrides.empty()&&d->loaded.scene.materials.at(d->loaded.scene.instances[0].materials[0]).roughness==.95f,"材质预设被旧编辑遮盖");
    d=fixture();d->loaded.scene.textures.push_back({"preview",texture,ir::ColorSpace::srgb});d->loaded.scene.materials[0].color_texture=0;snapshot=initial_snapshot(*d);UiScale ui_scale(false);MaterialPanel panel;panel.resize(560,700);int changes=0;panel.changed=[&]{++changes;};panel.bind(d,&snapshot,0);
    if(argc>1&&std::string(argv[1])=="--window"){QScreen *secondary=nullptr;for(auto *screen:QGuiApplication::screens())if(screen!=QGuiApplication::primaryScreen())secondary=screen;check(secondary,"缺少副屏");panel.move(secondary->availableGeometry().topLeft()+QPoint(50,50));}panel.show();app.processEvents();
    if(argc>1&&std::string(argv[1])=="--window"){QScreen *secondary=nullptr;for(auto *screen:QGuiApplication::screens())if(screen!=QGuiApplication::primaryScreen())secondary=screen;check(secondary,"缺少副屏");panel.move(secondary->availableGeometry().topLeft()+QPoint(50,50));QTest::qWait(300);panel.grab().save("artifacts/materials/panel.png");}
    auto *tree=panel.findChild<QTreeWidget *>("materialSurfaces");check(tree&&tree->topLevelItemCount()==1&&tree->topLevelItem(0)->childCount()==2,"角色范围缺少自身或服装 / 混入另一角色");auto *skin=tree->topLevelItem(0)->child(0)->child(0);auto *nails=tree->topLevelItem(0)->child(0)->child(1);tree->setCurrentItem(skin);app.processEvents();
    QString located;panel.locate_file=[&](const QString &file){located=file;};auto *locate=panel.findChild<QToolButton *>("materialLocate/color_texture");check(locate&&locate->isEnabled(),"纹理缺少定位按钮");locate->click();check(QFileInfo(located).absoluteFilePath()==QFileInfo(QString::fromStdWString(texture.wstring())).absoluteFilePath(),"纹理定位没有使用当前文件");
    auto *preview=panel.findChild<QWidget *>("materialRow/color_texture")->findChild<QLabel *>("materialThumbnail");for(int n=0;n<100&&preview->pixmap().isNull();++n)QTest::qWait(20);check(!preview->pixmap().isNull(),"贴图缩略图没有显示");
    auto *rough=panel.findChild<QDoubleSpinBox *>("material/roughness");check(rough,"没有材质数值控件");rough->setValue(.21);check(changes==1&&snapshot.material_overrides["figure"]["Skin"]["roughness"]==.21,"实际 UI 编辑未提交");check(snapshot.material_overrides.size()==1,"UI 串改其他角色");
    nails->setSelected(true);app.processEvents();rough=panel.findChild<QDoubleSpinBox *>("material/roughness");check(rough->suffix().contains(QStringLiteral("多值")),"多选缺少混合值提示");rough->setValue(.64);check(snapshot.material_overrides["figure"]["Nails"]["roughness"]==.64,"多选未修改所有表面");
    auto *units=panel.findChild<QComboBox *>("material/emission_units");units->setCurrentIndex(5);check(snapshot.material_overrides["figure"]["Skin"]["emission_units"]==5,"枚举参数未提交");
    auto *thin=panel.findChild<QCheckBox *>("material/thin_walled");thin->click();check(snapshot.material_overrides["figure"]["Skin"]["thin_walled"]==true,"开关参数未提交");
    auto *red=panel.findChild<QDoubleSpinBox *>("material/base_color/0");red->setValue(.5);check(std::abs(snapshot.material_overrides["figure"]["Skin"]["base_color"][0].get<double>()-.21404114)<1e-6,"颜色没有从 sRGB 转换至线性");
    red->setValue(10);check(red->value()==10&&snapshot.material_overrides["figure"]["Skin"]["base_color"][0].get<double>()>10,"HDR 颜色上限没有提高至 10");
    auto *row=panel.findChild<QWidget *>("materialRow/base_color");auto *revert=panel.findChild<QToolButton *>("materialRevert/base_color");check(revert&&revert->x()>red->mapTo(row,QPoint()).x()&&std::abs(revert->geometry().center().y()-red->mapTo(row,red->rect().center()).y())<3,"参数不是紧凑单行或还原按钮不在行尾");
    auto *search=panel.findChild<QLineEdit *>("materialSearch");search->setText("roughness");app.processEvents();rough=panel.findChild<QDoubleSpinBox *>("material/roughness");auto wheel=[&](QWidget *widget){QPointF p(widget->rect().center());QWheelEvent event(p,widget->mapToGlobal(p.toPoint()),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(widget,&event);};const auto unclicked=rough->value();wheel(rough);check(rough->value()==unclicked,"未点击属性就能滚轮修改");QTest::mouseClick(rough,Qt::LeftButton,Qt::NoModifier,QPoint(4,rough->height()/2));wheel(rough);check(rough->value()>unclicked,"点击属性后滚轮不能修改");search->clear();
    std::vector<MaterialSurface> hover;panel.hovered=[&](const auto &v){hover=v;};auto position=tree->visualItemRect(skin).center();auto move=[&](QPoint p){QMouseEvent event(QEvent::MouseMove,QPointF(p),QPointF(tree->viewport()->mapToGlobal(p)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(tree->viewport(),&event);};QEvent initial_leave(QEvent::Leave);QApplication::sendEvent(tree->viewport(),&initial_leave);move(position);QTest::qWait(100);check(hover.size()==1&&hover[0].slot==0,"子材质悬浮范围错误");QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,position);check(hover.empty(),"点击材质后未清除黄色高亮");move(position+QPoint(2,0));QTest::qWait(50);check(hover.empty(),"点击后在同一材质内移动又亮起");move(tree->visualItemRect(nails).center());QTest::qWait(50);check(hover.size()==1&&hover[0].slot==1,"切换悬浮材质未恢复高亮");QEvent leave(QEvent::Leave);QApplication::sendEvent(tree->viewport(),&leave);check(hover.empty(),"离开材质树仍有高亮");
    search->setText("Roughness");check(panel.findChild<QWidget *>("materialRow/opacity")->isHidden()&&!panel.findChild<QWidget *>("materialRow/roughness")->isHidden(),"参数搜索失败");search->clear();
    tree->setCurrentItem(skin->parent());for(auto *button:panel.findChildren<QPushButton *>())if(button->text()==QStringLiteral("还原所选材质")){button->click();break;}app.processEvents();check(snapshot.material_overrides.empty(),"还原材质没有清除覆盖");
    panel.findChild<QComboBox *>("materialScope")->setCurrentIndex(1);check(tree->topLevelItemCount()==2,"全部对象范围失败");
    QMainWindow window;window.move(panel.pos());auto *dock=new QDockWidget(QStringLiteral("材质"),&window);dock->setObjectName("Materials");dock->setWidget(new MaterialPanel);window.addDockWidget(Qt::LeftDockWidgetArea,dock);auto layout=window.saveState(1);dock->setFloating(true);check(window.restoreState(layout,1)&&!dock->isFloating(),"独立停靠布局无法恢复");
    std::cout<<"材质隔离、Delta、贴图、DUFEX、预设、多选、颜色与 Dock：PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
