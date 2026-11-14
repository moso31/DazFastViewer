#include "editor/material_channel_info.h"
#include <QStyleOptionSpinBox>

static void material_scope_ui(QApplication &app){
  auto d=fixture();auto &m=d->loaded.scene.materials[0];
  m.source_definition=R"({"channels":{"Metallic Weight":{"value":0,"min":0,"max":1,"clamped":true,"step_size":1},"Cutout Opacity":{"value":1,"step_size":1},"Glossy Roughness":{"step_size":0.01},"Luminance":{"step_size":50},"Share Glossy Inputs":{"value":true},"Refraction Color":{"value":[1,1,1]},"Top Coat Anisotropy":{"value":0.5},"Top Coat Weight":{"value":0},"Tags":{"value":"prop"},"Unknown Active":{"value":0.8}}})";
  const auto channels=J::parse(m.source_definition).at("channels");
  for(const auto &[id,c]:channels.items())m.source_channels.push_back({id,id,"float",c.value("value",J{}).dump(),{},id=="Metallic Weight"||id=="Cutout Opacity"||id=="Refraction Color"});
  auto snapshot=initial_snapshot(*d);MaterialPanel panel;panel.resize(720,700);panel.bind(d,&snapshot,0);panel.show();app.processEvents();
  auto *tree=panel.findChild<QTreeWidget *>("materialSurfaces");auto *skin=tree->topLevelItem(0)->child(0)->child(0);tree->setCurrentItem(skin);app.processEvents();
  auto spin=[&](const char *id){return panel.findChild<QDoubleSpinBox *>(QString("material/")+id);};
  check(spin("metallic")->singleStep()==.05&&spin("opacity")->singleStep()==.05,"源步长 1 仍使权重变成二元开关");
  check(spin("roughness")->singleStep()==.01&&spin("emission_luminance")->singleStep()==50,"更细源步长或物理单位步长被误改");
  check(spin("refraction_roughness")->singleStep()==.05,"原生材质步长没有使用参数定义");
  check(!panel.findChild<QToolButton *>("materialGroup/hair")->isChecked(),"物体毛发分类默认展开");
  auto quality=RenderQuality{};quality.subsurface=false;panel.render_quality(quality);app.processEvents();
  auto *sss=panel.findChild<QToolButton *>("materialGroup/subsurface");check(!sss->isChecked()&&sss->text().contains(QStringLiteral("设置已禁用")),"次表面散射面板没有同步画质开关");
  auto *scroll=panel.findChild<QScrollArea *>();scroll->verticalScrollBar()->setValue(0);app.processEvents();
  auto *metal=spin("metallic");const auto original=metal->value();
  auto wheel=[](QWidget *widget,int delta){const auto p=widget->rect().center();QWheelEvent e(p,widget->mapToGlobal(p),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(widget,&e);};
  wheel(metal,-120);app.processEvents();check(metal->value()==original&&scroll->verticalScrollBar()->value()>0,"未选择参数时滚轮没有滚动外层页面");
  scroll->ensureWidgetVisible(metal);app.processEvents();QStyleOptionSpinBox option;option.initFrom(metal);option.rect=metal->rect();option.buttonSymbols=metal->buttonSymbols();option.subControls=QStyle::SC_All;
  const auto arrow=metal->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxUp,metal);
  QTest::mouseClick(metal,Qt::LeftButton,Qt::NoModifier,QPoint(4,metal->height()/2));wheel(metal,120);
  check(std::abs(metal->value()-original-.05)<1e-6,"点击后滚轮没有平滑调节 0.05");
  option.initFrom(metal);check(arrow==metal->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxUp,metal)&&metal->styleSheet().isEmpty(),"选中改变了原生箭头样式或布局");
  const auto other=spin("opacity")->value();wheel(spin("opacity"),-120);check(spin("opacity")->value()==other,"滚轮串改未选中行");
  auto *edit=metal->findChild<QLineEdit *>();edit->selectAll();QTest::keyClicks(edit,"0.123456");QTest::keyClick(edit,Qt::Key_Return);check(std::abs(snapshot.material_overrides["figure"]["Skin"]["metallic"].get<double>()-.123456)<1e-7,"直接输入被步长量化");
  panel.hide();const auto hidden=metal->value();wheel(metal,120);check(metal->value()==hidden,"隐藏后残留滚轮选择");panel.show();app.processEvents();
  panel.findChild<QToolButton *>("materialSourceHeader")->click();app.processEvents();auto *sources=panel.findChild<QTreeWidget *>("materialSourceChannels");
  auto channel=[&](const char *id){for(int k=0;k<sources->topLevelItemCount();++k)if(sources->topLevelItem(k)->data(0,Qt::UserRole)==id)return sources->topLevelItem(k);return static_cast<QTreeWidgetItem *>(nullptr);};
  check(channel("Metallic Weight")->toolTip(2).contains("0.123456"),"源通道未展示转换后的编辑值");
  check(channel("Refraction Color")->text(2)==QStringLiteral("源输入未采用")&&channel("Top Coat Anisotropy")->text(2)==QStringLiteral("未启用层 / 未支持"),"未采用输入或未启用层被误报为生效");
  check(channel("Unknown Active")->text(2)==QStringLiteral("未支持")&&channel("Tags")->text(2)==QStringLiteral("说明 / 预览信息"),"未知通道或元数据被误报为已映射");
  scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());app.processEvents();check(sources->verticalScrollBar()->maximum()==0&&sources->visualItemRect(sources->topLevelItem(sources->topLevelItemCount()-1)).bottom()<sources->viewport()->height(),"源通道列表被截断或嵌套滚动");
  snapshot.parameter_settings["figure"]["material/Skin/metallic"]={true,.1,.7,.02};
  auto state=snapshot_json(*d,snapshot);auto reopened=initial_snapshot(*d);apply_snapshot_json(*d,reopened,state);panel.bind(d,&reopened,0);tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0));app.processEvents();
  check(spin("metallic")->singleStep()==.02&&spin("metallic")->minimum()==.1&&spin("metallic")->maximum()==.7,"保存的自定义编辑规则没有优先于源默认");
  auto next=std::make_shared<Document>(*d);next->loaded.scene.materials[0].hair=true;next->loaded.scene.materials[0].subsurface=.4f;panel.bind(next,&reopened,0);quality.subsurface=true;panel.render_quality(quality);app.processEvents();
  check(panel.findChild<QToolButton *>("materialGroup/hair")->isChecked()&&panel.findChild<QToolButton *>("materialGroup/subsurface")->isChecked(),"角色毛发或已启用 SSS 被物体折叠状态覆盖");
}

static void material_scope_scene(QApplication &app,const QStringList &args){
  check(args.size()==4,"--scope-scene scene.dufex output");
  auto restored=load_scene_extension(std::filesystem::path(args[2].toStdWString()),{"H:/g1","H:/g3"},1);auto &d=restored.document;auto &s=restored.snapshot;
  MaterialPanel panel;panel.resize(900,920);RenderQuality q;q.subsurface=false;panel.render_quality(q);panel.bind(d,&s,0);panel.restore_selection({{"SteelcoreCrate/SteelcoreCrate-1","M01"}});panel.show();app.processEvents();
  check(panel.selected_surfaces().size()==1,"未选中真实场景 M01");auto *metal=panel.findChild<QDoubleSpinBox *>("material/metallic");check(metal&&metal->singleStep()==.05,"真实场景金属度步长仍为 1");
  check(!panel.findChild<QToolButton *>("materialGroup/hair")->isChecked(),"真实道具毛发未折叠");
  const auto output=std::filesystem::path(args[3].toStdWString());std::filesystem::create_directories(output);panel.grab().save(QString::fromStdWString((output/"panel.png").wstring()));
  panel.findChild<QScrollArea *>()->ensureWidgetVisible(metal);app.processEvents();QTest::mouseClick(metal,Qt::LeftButton,Qt::NoModifier,QPoint(4,metal->height()/2));panel.grab().save(QString::fromStdWString((output/"selected.png").wstring()));
  panel.findChild<QLineEdit *>("materialSearch")->setText("Refraction");app.processEvents();panel.findChild<QScrollArea *>()->verticalScrollBar()->setValue(0);app.processEvents();panel.grab().save(QString::fromStdWString((output/"refraction.png").wstring()));
  panel.findChild<QLineEdit *>("materialSearch")->clear();panel.findChild<QToolButton *>("materialSourceHeader")->click();app.processEvents();auto *source=panel.findChild<QTreeWidget *>("materialSourceChannels");auto *outer=panel.findChild<QScrollArea *>();outer->ensureWidgetVisible(source,0,0);app.processEvents();panel.grab().save(QString::fromStdWString((output/"channels.png").wstring()));
  J rows=J::array();for(int n=0;n<source->topLevelItemCount();++n){const auto *row=source->topLevelItem(n);rows.push_back({{"id",row->data(0,Qt::UserRole).toString().toStdString()},{"value",row->text(1).toStdString()},{"status",row->text(2).toStdString()},{"detail",row->toolTip(2).toStdString()}});}std::ofstream(output/"channels.json")<<rows.dump(2);
}
