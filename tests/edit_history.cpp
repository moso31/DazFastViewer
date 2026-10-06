#include "editor/edit_history.h"
#include "editor/history_input.h"
#include "runtime/picking.h"
#include "editor/recovery.h"
#include "editor/scene_extension.h"
#include "water/document.h"
#include "editor/material_panel.h"
#include "editor/numeric_spinbox.h"
#include "editor/hdr_color_dialog.h"
#include "powerpose_fixture.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTest>
#include <QFile>
#include <QComboBox>
#include <QProcess>
#include <fstream>
#include <iostream>

using namespace dfv;using namespace dfv::editor;using J=nlohmann::json;
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static std::shared_ptr<Document> fixture(){
  auto d=std::make_shared<Document>();d->generation=1;auto &scene=d->loaded.scene;ir::Material material;material.id="shared";scene.materials={material};
  ir::Mesh mesh;mesh.id="mesh";mesh.positions={{0,0,0},{1,0,0},{0,0,1}};mesh.triangles={{{0,1,2}}};mesh.material_slots={"Skin","Nails"};scene.meshes={mesh};
  for(const auto *name:{"figure","dress","other"}){ir::Instance i;i.id=name;i.materials={0,0};scene.instances.push_back(i);runtime::Target target;target.id=std::string(name)+"/geometry";target.label=name;target.instance=uint32_t(d->catalog.targets.size());
    runtime::Morph m;m.id="morph";m.label="Morph";m.evaluable=true;target.morphs={m};if(target.instance==1){target.parent="#figure";target.conform_target="#figure";target.ancestors={"#figure"};}d->catalog.targets.push_back(target);
    daz::AssetObject object;object.id=name;object.instance=target.instance;object.parent=target.parent;object.conform_target=target.conform_target;d->loaded.objects.push_back(object);}
  auto skin=powerpose_fixture();skin.instance=0;d->skeletons.skins={skin};d->formulas.graphs.resize(3);d->formulas.graphs[0].skin=0;return d;
}
static EditState state_for(std::shared_ptr<const Document> document){EditState s;s.document=std::move(document);s.snapshot=initial_snapshot(*s.document);s.save_file="first.dufex";s.context.selection={{"figure/geometry"}};s.context.active=s.context.selection.front();return s;}
static void water_history(){
  auto d=std::make_shared<Document>();auto w=std::make_shared<water::Water>();w->id="history-water";w->config.width=w->config.length=32;water::install(*d,w);auto state=state_for(d);
  EditHistory history([&]{return state;},[&](const EditState &s){state=s;});
  for(int i=1;i<=20;++i)history.execute("Water time",[&]{auto next=std::make_shared<water::Water>(*w);next->config.time=i;state.snapshot.water_overrides={next};});
  check(history.stack().count()==20&&state.document==d,"water edits duplicated the scene document or failed to enter history");
  history.undo();check(water::effective(*d,state.snapshot).front()->config.time==19,"water time undo failed");history.redo();check(water::effective(*d,state.snapshot).front()->config.time==20,"water time redo failed");
  while(history.stack().canUndo())history.undo();check(state.snapshot.water_overrides.empty()&&state.document==d,"water undo did not restore the shared baseline");
}
static void subtree_visibility(){
  auto d=fixture();d->catalog.targets[0].character=true;
  d->catalog.targets[1].parent="#bone";d->catalog.targets[1].ancestors={"#bone","#group","#figure"};
  d->catalog.targets[2].parent="#figure";
  auto nested=d->catalog.targets[1];nested.id="nested/geometry";nested.instance=3;nested.parent="#dress";nested.ancestors={"#dress","#bone","#group","#figure"};d->catalog.targets.push_back(nested);
  auto instance=d->loaded.scene.instances[1];instance.id="nested";d->loaded.scene.instances.push_back(instance);
  const auto mesh=d->loaded.scene.meshes[0];d->loaded.scene.meshes.resize(4,mesh);
  for(size_t i=0;i<4;++i)d->loaded.scene.instances[i].mesh=uint32_t(i);
  auto current=state_for(d);auto scene=d->loaded.scene;runtime::MorphRuntime runtime(scene,d->catalog.targets);
  auto render=[&]{for(size_t i=0;i<4;++i)runtime.set_visible(i,current.snapshot.values[i].visible);const auto delta=runtime.evaluate();check(delta.meshes.empty()&&delta.instances.empty(),"Visibility edit recomputed geometry or transforms");return delta;};
  // All combinations include A=true with every child false, and mixed deep descendants.
  for(unsigned mask=0;mask<8;++mask){
    current.snapshot.values[0].visible=true;for(size_t i=1;i<4;++i)current.snapshot.values[i].visible=(mask&(1u<<(i-1)))!=0;
    render();const auto before=current;const auto rendered=scene.instances;
    EditHistory history([&]{return current;},[&](const EditState &s){current=s;});
    history.execute("hide parent",[&]{current.snapshot.values[0].visible=false;});render();
    check(history.stack().count()==1,"Parent visibility must be one history entry");
    for(size_t i=0;i<4;++i)check(!scene.instances[i].visible,"Hidden parent left a descendant rendered");
    for(size_t i=1;i<4;++i)check(current.snapshot.values[i]==before.snapshot.values[i],"Parent edit changed descendant properties");
    check(render().visibility.empty(),"Idle hidden state emitted another visibility delta");
    runtime::PickingScene picking;picking.update(scene);check(picking.ray({.25f,1,.25f},{0,-1,0}).instance<0,"Hidden descendants remained pickable");
    const auto hidden=current;history.undo();render();check(same_edit(current,before),"Undo changed authored child inputs");
    for(size_t i=0;i<4;++i)check(scene.instances[i].visible==rendered[i].visible,"Undo failed to restore effective visibility");
    history.redo();render();check(same_edit(current,hidden),"Redo changed authored child inputs");
    history.execute("show parent",[&]{current.snapshot.values[0].visible=true;});render();
    check(current.snapshot.values==before.snapshot.values,"Showing parent changed child visible flags");
    check(scene.instances[3].visible==(current.snapshot.values[1].visible&&current.snapshot.values[3].visible),"Deep descendant ignored its intermediate parent");
    auto restored=initial_snapshot(*d);apply_snapshot_json(*d,restored,snapshot_json(*d,hidden.snapshot));
    check(restored.values==hidden.snapshot.values,"Saved effective visibility instead of authored values");
  }
  current.snapshot.values[0].visible=false;current.snapshot.values[1].visible=false;current.snapshot.values[3].visible=true;render();
  current.snapshot.values[1].visible=true;render();check(!scene.instances[1].visible&&!scene.instances[3].visible,"Child bypassed a hidden ancestor");
  current.snapshot.values[0].visible=true;render();check(scene.instances[1].visible&&scene.instances[3].visible,"Lost child edits made while parent was hidden");
}
static void attached_visibility(){
  auto d=fixture();d->catalog.targets[0].character=d->catalog.targets[2].character=true;
  const auto mesh=d->loaded.scene.meshes[0];d->loaded.scene.meshes.resize(3,mesh);
  for(size_t i=0;i<3;++i)d->loaded.scene.instances[i].mesh=uint32_t(i);
  auto add=[&](const std::string &id,const std::string &parent={},const std::string &fit={}) {
    const auto index=d->catalog.targets.size();runtime::Target t;t.id=id+"/geometry";t.instance=uint32_t(index);t.parent=parent;t.conform_target=fit;d->catalog.targets.push_back(t);
    daz::AssetObject o;o.id=id;o.instance=t.instance;o.parent=parent;o.conform_target=fit;d->loaded.objects.push_back(o);
    auto instance=d->loaded.scene.instances[0];instance.id=t.id;instance.mesh=uint32_t(d->loaded.scene.meshes.size());d->loaded.scene.meshes.push_back(mesh);d->loaded.scene.instances.push_back(instance);return index;
  };
  const auto fitted=add("fitted","#wardrobe","#figure"),button=add("button","#fitted");
  const auto rigid=add("rigid","#wardrobe");d->catalog.targets[rigid].rigid_follow.target="#fitted";
  const auto graft=add("graft","#wardrobe");d->loaded.scene.instances[graft].graft_source=0;
  const auto shell=add("shell","#wardrobe");d->loaded.scene.instances[shell].shell_source=int(graft);
  const auto shell_child=add("shell-child","#shell");
  const auto hair=add("hair","#wardrobe","#scalp"),scalp=add("scalp","#hair","#figure"),hair_tip=add("hair-tip","#hair");
  const auto glasses=add("glasses","#head");d->catalog.targets[glasses].ancestors={"#head","#figure"};
  const auto other_fit=add("other-fit",{},"#other");
  const auto collision=add("collision-only","#wardrobe");d->catalog.targets[collision].smoothing.collision_target="#figure";
  const auto detached=add("detached","#wardrobe");
  const auto shell_part=add("shell");d->catalog.targets[shell_part].id="shell/extra-part";
  auto copy=d->loaded.scene.instances[0];copy.id="independent-copy";copy.prototype=0;copy.instance_node="independent-copy";d->loaded.scene.instances.push_back(copy);
  const std::set<size_t> related{0,1,fitted,button,rigid,graft,shell,shell_child,hair,scalp,hair_tip,glasses,shell_part};
  auto current=state_for(d);current.snapshot.values[1].visible=false;current.snapshot.values[scalp].visible=false;
  const auto original=current.snapshot.values;auto scene=d->loaded.scene;runtime::MorphRuntime runtime(scene,d->catalog.targets);
  auto render=[&]{for(size_t i=0;i<original.size();++i)runtime.set_visible(i,current.snapshot.values[i].visible);return runtime.evaluate();};
  render();check(scene.instances[hair].visible&&scene.instances[hair_tip].visible&&!scene.instances[scalp].visible,"Hidden scalp hid fitted hair");
  const auto initial=scene.instances;current.snapshot.values[0].visible=false;const auto delta=render();
  for(size_t i=0;i<original.size();++i){check(scene.instances[i].visible==!related.contains(i),"Character hide missed attachments or affected unrelated objects");if(i)check(current.snapshot.values[i]==original[i],"Character hide changed attachment properties");}
  check(delta.visibility.size()==related.size()-2&&delta.meshes.empty()&&delta.instances.empty(),"Inherited visibility emitted wrong deltas or geometry edits");
  check(scene.instances.back().visible,"Character visibility affected an independent Instance");
  current.snapshot.values[0].visible=true;render();
  for(size_t i=0;i<scene.instances.size();++i)check(scene.instances[i].visible==initial[i].visible,"Restoring character lost mixed visibility");
  current.snapshot.values[fitted].visible=false;render();
  check(!scene.instances[fitted].visible&&!scene.instances[button].visible&&scene.instances[0].visible&&scene.instances[rigid].visible,"Scene inheritance was confused with deformation dependencies");
  current.snapshot.values[fitted].visible=true;current.snapshot.values[hair].visible=false;render();
  check(!scene.instances[hair_tip].visible&&!scene.instances[scalp].visible&&scene.instances[0].visible,"Hair descendants failed to hide or hid the character");
  current.snapshot.values[hair].visible=true;current.snapshot.values[0].visible=false;
  d->catalog.targets[fitted].conform_target.clear();d->loaded.objects[fitted].conform_target.clear();
  runtime::MorphRuntime rebound(scene,d->catalog.targets);for(size_t i=0;i<original.size();++i)rebound.set_visible(i,current.snapshot.values[i].visible);rebound.evaluate();
  check(scene.instances[fitted].visible&&scene.instances[button].visible&&scene.instances[rigid].visible,"Detached accessory still inherited former character visibility");
  check(scene.instances[2].visible&&scene.instances[other_fit].visible&&scene.instances[collision].visible&&scene.instances[detached].visible,"Visibility affected unrelated objects");
}
static void commands(){
  EditState current=state_for(fixture());int changes=0,restores=0;EditHistory history([&]{return current;},[&](const EditState &s){current=s;++restores;});history.changed=[&]{++changes;};
  auto roundtrip=[&](const QString &label,auto change){auto before=current;const auto count=history.stack().count();history.execute(label,change);auto after=current;check(history.stack().count()==count+1,"操作没有产生一条历史");history.undo();check(same_edit(current,before),"撤销没有恢复完整输入");history.redo();check(same_edit(current,after),"重做没有恢复完整输入");};
  roundtrip("transform",[&]{current.snapshot.values[0].transform.translation_cm={1,2,3};current.snapshot.values[0].transform.general_scale=1.123456789;});
  roundtrip("group transform",[&]{current.snapshot.group_transforms["group"].translation_cm={10,20,30};current.snapshot.group_transforms["nested"].rotation_degrees.y=35;current.context.selection={{"group","","",-4}};current.context.active=current.context.selection.front();});
  roundtrip("visibility",[&]{current.snapshot.values[1].visible=false;});
  roundtrip("morph",[&]{current.snapshot.values[0].morphs[0]=1.7f;current.snapshot.values[0].unlimited_morphs.insert("morph");});
  roundtrip("IK / FK / PowerPose",[&]{for(size_t i=0;i<3;++i)current.snapshot.poses[0][i].rotation_degrees.z=float(i+2);});
  roundtrip("pins",[&]{current.snapshot.pose_pins.push_back({0,1,{.1f,.2f,.3f},true,true,ir::Transform::translate({1,2,3})});});
  roundtrip("growth",[&]{auto &v=current.snapshot.values[0];v.extension.kind=runtime::ExtensionKind::growth;v.extension.age=7.5;v.extension.age_step=.25;v.extension.sensitivity=2.5;v.extension.strength=.7;v.morphs[0]=.23f;v.transform.general_scale=.85;v.transform.translation_cm.y=2;});
  roundtrip("density",[&]{current.snapshot.values[2].extension.kind=runtime::ExtensionKind::density;current.snapshot.values[2].extension.density=500;});
  roundtrip("ground",[&]{current.snapshot.values[0].ground_alignment_ratio=.25;current.snapshot.values[0].transform.translation_cm.y=-20;current.snapshot.instance_ground["instance"]={.3,.25};});
  roundtrip("subdivision",[&]{current.snapshot.subdivision_levels["mesh"]=2;});
  roundtrip("lights",[&]{ir::AreaLight light;light.id="new";current.snapshot.lights.push_back(light);});
  roundtrip("options",[&]{current.snapshot.options.backdrop={.1f,.2f,.3f};});
  roundtrip("materials",[&]{current.snapshot.material_overrides["figure"]["Skin"]["roughness"]=.7;current.snapshot.material_overrides["dress"]["Nails"]["roughness"]=.4;});
  roundtrip("favorites",[&]{current.snapshot.values[0].favorites.emplace();current.snapshot.values[0].favorites->nodes[""]["morph"]=true;current.snapshot.control_favorites.emplace();current.snapshot.control_favorites->nodes["options/environment"]["x"]=false;});
  current.manual=true;roundtrip("draft",[&]{current.pending[{"figure/geometry","morph"}]={current.snapshot.values[0].morphs[0],true};current.snapshot.values[0].morphs[0]=.95f;});
  roundtrip("apply",[&]{current.pending.clear();});
  const auto count=history.stack().count();history.undo();auto before=current;history.execute("unchanged",[]{});check(history.stack().canRedo()&&history.stack().count()==count,"无变化操作破坏 redo");
  try{history.execute("failed",[&]{current.snapshot.values[2].visible=false;throw std::runtime_error("拒绝");});}catch(...){}
  check(same_edit(current,before)&&history.stack().canRedo(),"失败操作没有回滚或破坏 redo");history.redo();
  history.mark_saved();history.execute("edit after save",[&]{current.snapshot.values[2].visible=false;});check(!history.stack().isClean(),"保存点未变脏");history.undo();check(history.stack().isClean(),"撤销到保存点未变干净");
  history.redo();history.undo();history.execute("branch",[&]{current.snapshot.values[2].transform.translation_cm.x=4;});check(!history.stack().canRedo(),"新编辑保留了过期 redo");
  check(changes>20&&restores>20,"历史通知未发出");
}
static void gestures_and_limits(){
  auto current=state_for(fixture());EditHistory history([&]{return current;},[&](const EditState &s){current=s;});
  history.begin_gesture(1);for(int i=1;i<=20;++i)history.execute("drag",[&]{current.snapshot.values[0].transform.translation_cm.x=float(i);});check(history.stack().count()==0,"预览提前入栈");history.finish_gesture();check(history.stack().count()==1,"拖动没有合并");history.undo();check(current.snapshot.values[0].transform.translation_cm.x==0,"拖动撤销不是起点");
  history.begin_gesture(1);history.execute("cancel",[&]{current.snapshot.values[0].transform.translation_cm.x=5;});history.cancel_gesture();check(history.stack().canRedo()&&current.snapshot.values[0].transform.translation_cm.x==0,"取消拖动破坏 redo");
  history.begin_gesture(1);history.execute("out",[&]{current.snapshot.values[0].transform.translation_cm.x=5;});history.execute("back",[&]{current.snapshot.values[0].transform.translation_cm.x=0;});history.finish_gesture();check(history.stack().canRedo()&&history.stack().count()==1,"回到原点仍占历史");history.clear();
  for(int i=1;i<=51;++i)history.execute(QString::number(i),[&]{current.snapshot.values[0].transform.translation_cm.x=float(i);});check(history.stack().count()==50,"历史上限不是 50");
  for(int i=0;i<50;++i)history.undo();check(current.snapshot.values[0].transform.translation_cm.x==1&&!history.stack().canUndo(),"第 51 条淘汰边界错误");
  for(int i=0;i<30;++i)history.redo();const auto before=current;history.set_limit(7);check(same_edit(current,before)&&history.stack().count()==7&&history.stack().index()==7,"降低上限改变当前场景或条数");
  history.undo();history.undo();const auto middle=current;history.set_limit(20);check(same_edit(current,middle)&&history.stack().index()==5&&history.stack().canRedo(),"扩大上限破坏 redo 游标");
  history.redo();history.redo();check(same_edit(current,before),"调整上限后 redo 错误");
  current.save_file="new-save-as.dufex";history.undo();check(current.save_file=="new-save-as.dufex","普通撤销回退了另存为路径");
}
static void structure(){
  auto current=state_for(fixture());current.snapshot.values[0].morphs[0]=.6f;current.snapshot.values[1].visible=false;current.snapshot.material_overrides["figure"]["Skin"]["opacity"]=.3;
  EditHistory history([&]{return current;},[&](const EditState &s){current=s;});const auto before=current;
  history.execute("delete",[&]{auto d=std::make_shared<Document>(*current.document);remove_target(*d,current.snapshot,0);current.document=d;});const auto deleted=current;check(current.snapshot.values.size()==1,"删除未包括穿戴物");history.undo();check(same_edit(current,before),"删除撤销丢失资源或参数");history.redo();check(same_edit(current,deleted),"删除重做错误");history.undo();
  history.execute("catalog",[&]{auto d=std::make_shared<Document>(*current.document);runtime::Morph m;m.id="added";d->catalog.targets[0].morphs.push_back(m);++d->asset_revision;current.document=d;current.snapshot.values[0].morphs.push_back(.2f);});auto catalog=current;history.undo();check(same_edit(current,before),"目录刷新撤销未恢复通道对应");history.redo();check(same_edit(current,catalog),"目录刷新重做错误");
  auto empty=std::make_shared<Document>();empty->generation=2;const auto old=current;history.execute("new scene",[&]{current=state_for(empty);current.scene_id=2;current.save_file="second.dufex";});auto blank=current;history.undo();check(same_edit(current,old)&&current.save_file==old.save_file,"跨场景撤销丢失保存目标");history.redo();check(same_edit(current,blank)&&current.save_file=="second.dufex","跨场景 redo 错误");
  current.save_file="second-saved-elsewhere.dufex";history.undo();check(current.save_file==old.save_file,"跨场景撤销丢失旧保存路径");history.redo();check(current.save_file=="second-saved-elsewhere.dufex","跨场景重做丢失后来另存为的路径");
  history.set_limit(2);history.undo();history.undo();check(current.document==before.document,"结构命令裁剪后撤销错误");
}
static void controls(){
  auto current=state_for(fixture());EditHistory history([&]{return current;},[&](const EditState &s){current=s;});QWidget root;QVBoxLayout layout(&root);auto *spin=new NumericSpinBox(false);spin->setRange(-100,100);spin->setKeyboardTracking(false);layout.addWidget(spin);HistoryInput input(history,&root);
  QObject::connect(spin,&QDoubleSpinBox::valueChanged,[&](double value){history.execute("spin",[&]{current.snapshot.values[0].transform.translation_cm.x=float(value);});});root.show();QTest::qWait(20);
  QTest::keyPress(spin,Qt::Key_Up);QTest::keyRelease(spin,Qt::Key_Up);QApplication::processEvents();check(history.stack().count()==1,"键盘步进未入历史");
  const auto count=history.stack().count();const QPointF point=spin->rect().center(),global=spin->mapToGlobal(point.toPoint());
  for(int i=0;i<4;++i){QWheelEvent event(point,global,{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(spin,&event);}QTest::qWait(450);check(history.stack().count()==count+1,"连续滚轮没有合并");
  auto *slider=new NumericSlider;layout.addWidget(slider);slider->sync(0,-10,10,1);slider->edited=[&](double value){history.execute("slider",[&]{current.snapshot.values[0].transform.translation_cm.y=float(value);});slider->sync(value,-10,10,1);};
  QObject::connect(slider,&QSlider::sliderPressed,[&]{history.begin_gesture(quintptr(slider));});QObject::connect(slider,&QSlider::sliderReleased,[&]{history.finish_gesture();});
  const int keys=history.stack().count();QTest::keyPress(slider,Qt::Key_Right);for(int i=0;i<5;++i){QKeyEvent repeat(QEvent::KeyPress,Qt::Key_Right,{},QString{},true);QApplication::sendEvent(slider,&repeat);}QTest::keyRelease(slider,Qt::Key_Right);QApplication::processEvents();check(history.stack().count()==keys+1&&current.snapshot.values[0].transform.translation_cm.y==6,"滑块键盘长按未合并");
  history.undo();slider->sync(0,-10,10,1);const auto drag_before=current;QTest::mousePress(slider,Qt::LeftButton,{},QPoint(10,8));QTest::mouseMove(slider,QPoint(55,8));QTest::keyPress(slider,Qt::Key_Shift);QTest::keyRelease(slider,Qt::Key_Shift);check(history.gesturing(),"Shift 精细调整中断了拖动历史");QTest::keyClick(slider,Qt::Key_Escape);check(same_edit(current,drag_before)&&history.stack().canRedo()&&!slider->isSliderDown(),"Esc 取消拖动未恢复原值与 redo");QTest::mouseRelease(slider,Qt::LeftButton);QApplication::processEvents();
  MaterialPanel panel;layout.addWidget(&panel);panel.bind(current.document,&current.snapshot,0);panel.edit_requested=[&](const QString &name,const std::function<void()> &change){history.execute(name,change);};
  panel.findChild<QComboBox *>()->setCurrentIndex(1);panel.restore_selection({{"figure","Skin"},{"dress","Nails"}});check(panel.selected_surfaces().size()==2,"多选测试未选择两个表面");auto *roughness=panel.findChild<QDoubleSpinBox *>("material/roughness");check(roughness,"材质参数控件缺失");const auto before=current;roughness->setValue(.63);auto after=current;history.undo();check(same_edit(current,before),"实际材质面板多选撤销错误");history.redo();check(same_edit(current,after)&&current.snapshot.material_overrides.size()==2,"实际材质面板多选重做错误");
  const auto before_color=current;const int color_count=history.stack().count();bool opened=false;
  QTimer::singleShot(0,[&]{auto *dialog=dynamic_cast<HdrColorDialog *>(QApplication::activeModalWidget());if(!dialog){if(auto *modal=qobject_cast<QDialog *>(QApplication::activeModalWidget()))modal->reject();return;}opened=true;dialog->findChild<QPushButton *>("hdrDouble")->click();dialog->findChild<QPushButton *>("hdrDouble")->click();dialog->accept();});
  panel.findChild<QPushButton *>("material/base_color")->click();QApplication::processEvents();check(opened&&history.stack().count()==color_count+1,"HDR 弹窗多次调色未合并为一条历史");
  const auto after_color=current;check(!same_edit(before_color,after_color),"HDR 弹窗没有写入修改");history.undo();check(same_edit(current,before_color),"HDR 多选颜色撤销错误");history.redo();check(same_edit(current,after_color),"HDR 多选颜色重做错误");
}
static EditState disk_fixture(const std::filesystem::path &folder){
  auto data=J::parse(R"({"node_library":[{"id":"root","type":"node"}],"geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[0,0,0],[100,0,0],[0,100,0]]},"polygon_material_groups":{"count":1,"values":["Skin"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]}}],"scene":{"nodes":[{"id":"object","url":"#root","geometries":[{"id":"shape","url":"#mesh"}]}],"materials":[{"id":"mat","geometry":"#shape","groups":["Skin"]}]}})");
  data["geometry_library"][0]["default_uv_set"]="#uv";data["uv_set_library"]=J::parse(R"([{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}])");
  std::filesystem::create_directories(folder/"Morphs");std::ofstream(folder/"Morphs/shape.dsf")<<R"({"modifier_library":[{"id":"draft","parent":"/scene.duf#mesh","channel":{"type":"float","label":"Draft","value":0,"min":0,"max":1},"morph":{"vertex_count":3,"deltas":{"count":1,"values":[[0,1,0,0]]}}}]})";
  const auto file=folder/"scene.duf";std::ofstream(file)<<data.dump();auto d=std::make_shared<Document>();d->source_file=file;d->generation=3;d->loaded=daz::load(file,{{folder},false});d->catalog=daz::discover_morphs(d->loaded,{folder});d->skeletons=daz::load_skeletons(d->loaded);d->formulas=daz::enable_formulas(d->catalog,d->skeletons);auto state=state_for(d);state.snapshot.values[0].transform.translation_cm.x=12;state.snapshot.values[0].extension.kind=runtime::ExtensionKind::density;state.snapshot.values[0].extension.density=320;state.snapshot.material_overrides[d->loaded.scene.instances[0].id]["Skin"]["roughness"]=.78;state.manual=true;return state;
}
static void imported_visibility(){
  QTemporaryDir temp;check(temp.isValid(),"Cannot create visibility fixture");const std::filesystem::path folder(temp.path().toStdWString());
  const auto seed=disk_fixture(folder);const auto file=seed.document->source_file;auto data=daz::read_document_file(file);
  const auto hidden=J::parse(R"([{"channels":[{"channel":{"id":"Visible","current_value":false}}]}])");
  const auto original=data["scene"]["nodes"][0];data["scene"]["nodes"][0]["extra"]=hidden;
  auto add=[&](const char *id,const char *parent,bool visible){auto node=original;node["id"]=id;node["parent"]=parent;const auto shape=std::string(id)+"-shape";node["geometries"][0]["id"]=shape;if(!visible)node["extra"]=hidden;data["scene"]["nodes"].push_back(node);auto material=data["scene"]["materials"][0];material["id"]=std::string(id)+"-mat";material["geometry"]="#"+shape;data["scene"]["materials"].push_back(material);};
  add("child-on","#object",true);add("child-off","#object",false);add("grandchild","#child-off",true);
  data["scene"]["nodes"].push_back({{"id","hidden-group"},{"extra",hidden}});add("group-child","#hidden-group",true);
  std::ofstream(file)<<data.dump();Document d;d.source_file=file;d.generation=4;d.loaded=daz::load(file,{{folder},false});d.catalog=daz::discover_morphs(d.loaded,{folder});d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);
  auto index=[](const Document &doc,const std::string &node){for(size_t i=0;i<doc.catalog.targets.size();++i)if(doc.catalog.targets[i].id.starts_with(node+"/"))return i;throw std::runtime_error("Missing visibility fixture node");};
  const auto root=index(d,"object"),on=index(d,"child-on"),off=index(d,"child-off"),deep=index(d,"grandchild"),group=index(d,"group-child");
  auto s=initial_snapshot(d);check(!s.values[root].visible&&s.values[on].visible&&!s.values[off].visible&&s.values[deep].visible&&s.values[group].visible,"Import flattened local visibility into inherited state");
  auto verify=[&](const Document &doc,Snapshot input,size_t r,size_t c,size_t h,size_t g,size_t fixed){
    auto scene=doc.loaded.scene;runtime::DeformationRuntime runtime(scene,doc.catalog.targets,doc.skeletons.skins,doc.formulas.graphs);
    auto shown=[&](size_t t){return scene.instances[doc.catalog.targets[t].instance].visible;};
    runtime.evaluate(input.values,input.poses);check(!shown(r)&&!shown(c)&&!shown(h)&&!shown(g)&&!shown(fixed),"Loaded hidden ancestors were ignored");
    input.values[r].visible=true;runtime.evaluate(input.values,input.poses);check(shown(r)&&shown(c)&&!shown(h)&&!shown(g)&&!shown(fixed),"Showing imported parent lost child or group visibility");
    input.values[h].visible=true;runtime.evaluate(input.values,input.poses);check(shown(g),"Showing intermediate parent did not reveal its enabled descendant");
  };
  verify(d,s,root,on,off,deep,group);
  const auto saved=folder/"visibility.dufex";save_scene_extension(saved,d,s);auto reopened=load_scene_extension(saved,{folder},5);
  check(snapshot_json(d,s)==snapshot_json(*reopened.document,reopened.snapshot),"DUFEX failed to retain authored visibility");verify(*reopened.document,reopened.snapshot,root,on,off,deep,group);
  Document combined;append_document(combined,d,"a/");append_document(combined,d,"b/");auto merged=initial_snapshot(combined);merged.values[index(combined,"b/object")].visible=true;
  auto rendered=combined.loaded.scene;runtime::DeformationRuntime runtime(rendered,combined.catalog.targets,combined.skeletons.skins,combined.formulas.graphs);runtime.evaluate(merged.values,merged.poses);
  check(!rendered.instances[combined.catalog.targets[index(combined,"a/child-on")].instance].visible&&rendered.instances[combined.catalog.targets[index(combined,"b/child-on")].instance].visible,"Appended scenes shared visibility inheritance");
  const auto refreshed=refresh_parameters(combined,index(combined,"b/object"),{folder});
  check(initial_snapshot(*refreshed).values[index(*refreshed,"b/child-on")].visible,"Parameter refresh lost authored child visibility");
}
static void recovery(){
  QTemporaryDir temp;check(temp.isValid(),"无法创建恢复测试目录");const std::filesystem::path folder(temp.path().toStdWString());auto state=disk_fixture(folder);const auto directory=temp.filePath("recovery");QString abnormal;
  {RecoverySession session(directory);abnormal=session.file();session.checkpoint(state,{folder});check(session.flush(),"恢复点写入失败");check(RecoverySession::candidates(directory).empty(),"运行中的恢复点被误认为崩溃");}
  check(RecoverySession::candidates(directory)==QStringList{abnormal},"异常退出没有恢复候选");const auto data=RecoverySession::read(abnormal);auto restored=restore_recovery(data,20);
  check(snapshot_json(*state.document,state.snapshot)==snapshot_json(*restored.document,restored.snapshot)&&state.save_file==restored.save_file&&restored.manual,"恢复点未恢复完整场景");
  check(!state.document->catalog.targets[0].morphs.empty(),"恢复夹具缺少 Morph");const auto &target=state.document->catalog.targets[0];auto draft=state;draft.pending[{target.id,target.morphs[0].id}]={.2f,true};draft.snapshot.values[0].morphs[0]=.75f;draft.snapshot.values[0].unlimited_morphs.insert(target.morphs[0].id);draft.context.selection={{target.id}};draft.context.active=draft.context.selection.front();draft.context.surfaces={{state.document->loaded.scene.instances[0].id,"Skin"}};
  const auto recovered_draft=restore_recovery(recovery_json(draft,{folder}),21);check(recovered_draft.pending==draft.pending&&recovered_draft.snapshot.values[0].morphs[0]==.75f&&recovered_draft.snapshot.values[0].unlimited_morphs==draft.snapshot.values[0].unlimited_morphs&&recovered_draft.context.selection==draft.context.selection&&recovered_draft.context.surfaces==draft.context.surfaces,"恢复丢失暂存值、已应用值或选择上下文");
  auto blank=state_for(std::make_shared<Document>());auto recovered_blank=restore_recovery(recovery_json(blank,{}),22);check(recovered_blank.document->catalog.targets.empty()&&recovered_blank.snapshot.values.empty(),"新建空场景无法恢复");
  {RecoverySession session(directory);for(int i=0;i<25;++i){state.snapshot.values[0].transform.translation_cm.x=float(i);session.checkpoint(state,{folder});}check(session.flush(),"连续编辑写入失败");check(RecoverySession::read(session.file())["scene"]["state"]["objects"].begin().value()["transform"]["translation_cm"][0]==24,"旧恢复任务覆盖最新操作");
    state.snapshot.values[0].transform.translation_cm.x=42;session.checkpoint(state,{folder},true);check(session.flush()&&RecoverySession::read(session.file())["clean_exit"]==true,"正常关闭未保存最终状态和标记");}
  RecoverySession::dismiss(abnormal);check(RecoverySession::candidates(directory).empty(),"忽略恢复后仍重复提示");
  auto bad=data;bad["version"]=99;bool rejected=false;try{restore_recovery(bad,1);}catch(...){rejected=true;}check(rejected,"无效恢复版本未拒绝");
  const auto crash_directory=temp.filePath("crash");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--crash-writer",crash_directory,temp.path()});check(child.waitForFinished(30000)&&child.exitCode()==23,"崩溃恢复子进程失败");const auto crashed=RecoverySession::candidates(crash_directory);check(crashed.size()==1,"异常进程遗留锁未识别");check(restore_recovery(RecoverySession::read(crashed.front()),55).snapshot.values[0].transform.translation_cm.x==12,"真实异常退出后丢失场景");
  auto original=RecoverySession::read(abnormal);{RecoverySession session(directory);session.checkpoint(state,{folder});check(session.flush(),"有效恢复点失败");auto invalid=state;invalid.snapshot.values[0].transform.general_scale=std::numeric_limits<double>::quiet_NaN();session.checkpoint(invalid,{folder});check(!session.flush(),"无效恢复点没有报错");check(RecoverySession::read(session.file())["scene"]["state"]["objects"].begin().value()["transform"]["general_scale"]==1,"失败写入破坏上一个恢复点");}
}
int main(int argc,char **argv){QApplication app(argc,argv);try{if(argc==4&&std::string(argv[1])=="--crash-writer"){auto state=disk_fixture(std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString()));RecoverySession session(QString::fromLocal8Bit(argv[2]));session.checkpoint(state,{std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString())});check(session.flush(),"崩溃前恢复点写入失败");std::_Exit(23);}water_history();subtree_visibility();attached_visibility();imported_visibility();commands();gestures_and_limits();structure();controls();recovery();std::cout<<"PASS edit history, controls, limits, structure, recovery\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
