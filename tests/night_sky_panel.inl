#include "render_ir/night_sky.h"
#include "editor/solar_location_picker.h"
#include <QCheckBox>
static void night_sky_panel(QApplication &app){
  using namespace dfv;
  ir::OptionNode n;n.id="night-test";ir::ensure_night_options(n);
  QScrollArea outer;outer.resize(660,480);outer.setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);
  auto *outside=new QPushButton("Outside");layout->addWidget(outside);auto *panel=new editor::ParameterPanel;panel->shared_scroll(&outer);layout->addWidget(panel);outer.setWidget(body);
  int location_commits=0;
  auto bind=[&]{panel->bind_options(&n,[&](size_t i,size_t k,double value){ir::set_option(n,i,k,value);},{},[&](const std::array<size_t,3> &indices,const std::array<double,3> &v){auto next=n;for(size_t k=0;k<3;++k)ir::set_option(next,indices[k],0,v[k]);n=std::move(next);++location_commits;});};bind();outer.show();app.processEvents();
  auto *rows=panel->findChild<QTreeWidget *>("parameterRows");
  auto item=[&](const char *id){for(int i=0;i<rows->topLevelItemCount();++i){auto *r=rows->topLevelItem(i);if(r->data(0,Qt::UserRole+1).toString()==QString::fromStdString(n.id+"/"+id+"0"))return r;}throw std::runtime_error(id);};
  auto row=[&](const char *id){auto *r=item(id);for(int attempt=0;attempt<5;++attempt){app.processEvents();rows->doItemsLayout();const auto pos=rows->viewport()->mapTo(body,rows->visualItemRect(r).center());outer.ensureVisible(pos.x(),pos.y(),0,100);app.processEvents();QTest::qWait(60);if(auto *w=rows->itemWidget(r,0))return w;}throw std::runtime_error(std::string("夜景行未挂载: ")+id);};
  auto wheel=[&](QWidget *w,int delta){const auto pos=w->rect().center();QWheelEvent event(pos,w->mapToGlobal(pos),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&event);app.processEvents();};
  auto *check=row("DFV Night Enabled")->findChild<QCheckBox *>("valueCheck");require(check&&!check->isChecked(),"夜景缺少默认关闭的勾选框");
  require(!row("DFV Night Sky Intensity")->findChild<QDoubleSpinBox *>("valueSpin")->isEnabled(),"夜景关闭后参数未禁用");
  check=row("DFV Night Enabled")->findChild<QCheckBox *>("valueCheck");QTest::mouseClick(check,Qt::LeftButton,Qt::NoModifier,QPoint(8,check->height()/2));require(ir::night_enabled(n),"勾选框未启用夜景");
  auto *spin=row("DFV Night Stars Intensity")->findChild<QDoubleSpinBox *>("valueSpin");require(spin&&spin->isEnabled(),"勾选后星光参数未恢复");
  const int scroll=outer.verticalScrollBar()->value();wheel(spin->findChild<QLineEdit *>(),-120);require(ir::number(n,"DFV Night Stars Intensity",0)==1&&outer.verticalScrollBar()->value()>scroll,"未选中夜景参数吞掉滚轮");
  spin=row("DFV Night Stars Intensity")->findChild<QDoubleSpinBox *>("valueSpin");auto *label=row("DFV Night Stars Intensity")->findChild<QLabel *>("valueLabel");
  auto arrows=[&](QDoubleSpinBox *s){QStyleOptionSpinBox o;o.initFrom(s);o.rect=s->rect();o.frame=s->hasFrame();o.buttonSymbols=s->buttonSymbols();return std::pair{s->style()->subControlRect(QStyle::CC_SpinBox,&o,QStyle::SC_SpinBoxUp,s),s->style()->subControlRect(QStyle::CC_SpinBox,&o,QStyle::SC_SpinBoxDown,s)};};
  const auto native=arrows(spin);QTest::mouseClick(label,Qt::LeftButton);wheel(spin,120);require(std::abs(ir::number(n,"DFV Night Stars Intensity",0)-1.1)<1e-6&&!label->styleSheet().isEmpty(),"选中夜景行后滚轮未调参");require(arrows(spin)==native&&spin->styleSheet().isEmpty(),"夜景选中状态改变了原生箭头");
  wheel(row("DFV Night Limiting Magnitude")->findChild<QDoubleSpinBox *>("valueSpin"),-120);require(ir::number(n,"DFV Night Limiting Magnitude",0)==6.5,"另一行被误改");
  bind();spin=row("DFV Night Stars Intensity")->findChild<QDoubleSpinBox *>("valueSpin");wheel(spin,120);require(std::abs(ir::number(n,"DFV Night Stars Intensity",0)-1.2)<1e-6,"同对象刷新丢失选中行");
  QTest::mouseClick(outside,Qt::LeftButton);wheel(spin,120);require(std::abs(ir::number(n,"DFV Night Stars Intensity",0)-1.2)<1e-6,"外部点击未取消选中");
  // 实际输入超出旧 0..100 的值，刷新后仍保留；共享“限”是唯一人为范围。
  auto *moon=row("DFV Night Moon Intensity")->findChild<QDoubleSpinBox *>("valueSpin");auto *range=row("DFV Night Moon Intensity")->findChild<QToolButton *>("rangeButton");
  require(moon&&range&&!range->isChecked()&&moon->maximum()>1000,"月光仍存在隐藏的 0..100 范围");
  QTest::mouseClick(moon,Qt::LeftButton);moon->selectAll();QTest::keyClicks(moon,"500");QTest::keyClick(moon,Qt::Key_Return);app.processEvents();require(ir::number(n,"DFV Night Moon Intensity",0)==500,"手动输入被第二套限幅截断");
  bind();moon=row("DFV Night Moon Intensity")->findChild<QDoubleSpinBox *>("valueSpin");require(moon->value()==500,"参数刷新截断了月光强度");
  auto *settings=dynamic_cast<editor::parameter_widgets::SettingsButtons *>(row("DFV Night Moon Intensity")->findChild<QWidget *>("parameterSettings"));require(settings,"月光缺少共享范围设置");
  settings->commit({true,200,800,.1});moon=row("DFV Night Moon Intensity")->findChild<QDoubleSpinBox *>("valueSpin");require(moon->minimum()==200&&moon->maximum()==800,"自定义范围被旧隐藏范围求交截断");
  auto *slider=row("DFV Night Moon Intensity")->findChild<QSlider *>("valueSlider");require(slider,"月光缺少滑轨");QTest::mouseClick(row("DFV Night Moon Intensity")->findChild<QLabel *>("valueLabel"),Qt::LeftButton);for(int k=0;k<5;++k)QTest::keyClick(slider,Qt::Key_PageUp);require(ir::number(n,"DFV Night Moon Intensity",0)==800,"滑轨与自定义范围不一致");
  range=row("DFV Night Moon Intensity")->findChild<QToolButton *>("rangeButton");QTest::mouseClick(range,Qt::LeftButton);moon=row("DFV Night Moon Intensity")->findChild<QDoubleSpinBox *>("valueSpin");moon->setValue(1200);require(ir::number(n,"DFV Night Moon Intensity",0)==1200,"关闭范围后仍受隐藏上限限制");
  auto *last=row("DFV Night Lighting Intensity");require(last->isVisible()&&rows->verticalScrollBar()->maximum()==0,"夜景有嵌套滚动条或最后一行不可达");
  auto *groups=panel->findChild<QTreeWidget *>("parameterGroups");
  auto group=[&](const QString &path){for(QTreeWidgetItemIterator i(groups);*i;++i)if((*i)->data(0,Qt::UserRole).toString()==path)return *i;throw std::runtime_error("分组缺失");};
  auto *night=group(QStringLiteral("/Environment/Dome/夜景")),*solar=group("/Environment/Dome/Sun-Sky");require(night->parent()==solar->parent()&&night->parent()->text(0)=="Dome","夜景与 Sun-Sky 不是 Dome 下的同级分组");
  groups->setCurrentItem(night);app.processEvents();require(!item("SS Day")->isHidden()&&!item("SS Time")->isHidden(),"夜景缺少共享日期和时间");
  require(!item("SS UTC Offset")->isHidden()&&row("SS UTC Offset")->findChild<QLabel *>("valueLabel")->text()=="SS UTC Offset"&&rows->indexOfTopLevelItem(item("SS UTC Offset"))==rows->indexOfTopLevelItem(item("SS Time"))+1,"原有 SS UTC Offset 未按原名显示在夜景 Time 旁边");
  auto *utc=row("SS UTC Offset")->findChild<QDoubleSpinBox *>("valueSpin");require(utc&&utc->singleStep()==.5,"UTC 默认精度不是半小时");QTest::mouseClick(outside,Qt::LeftButton);const auto utc_scroll=outer.verticalScrollBar()->value();wheel(utc,-120);require(ir::number(n,"SS UTC Offset",99)==0&&outer.verticalScrollBar()->value()>utc_scroll,"未选中的 UTC 吞掉滚轮");
  utc=row("SS UTC Offset")->findChild<QDoubleSpinBox *>("valueSpin");QTest::mouseClick(row("SS UTC Offset")->findChild<QLabel *>("valueLabel"),Qt::LeftButton);wheel(utc,120);require(ir::number(n,"SS UTC Offset",99)==.5,"UTC 没有按半小时调节");
  auto *utc_settings=dynamic_cast<editor::parameter_widgets::SettingsButtons *>(row("SS UTC Offset")->findChild<QWidget *>("parameterSettings"));require(utc_settings,"UTC 缺少共享精度设置");utc_settings->commit({false,0,1,.25});bind();night=group(QStringLiteral("/Environment/Dome/夜景"));solar=group("/Environment/Dome/Sun-Sky");require(row("SS UTC Offset")->findChild<QDoubleSpinBox *>("valueSpin")->singleStep()==.25,"刷新丢失自定义 UTC 精度");
  require(rows->indexOfTopLevelItem(item("DFV Night Star Density"))==rows->indexOfTopLevelItem(item("DFV Night Limiting Magnitude"))+1,"星等和密度没有相邻排列");
  require(rows->indexOfTopLevelItem(item("DFV Night Moon Lighting Gain"))==rows->indexOfTopLevelItem(item("DFV Night Moon Intensity"))+1,"月光照明增益没有与月亮强度放在一起");
  auto *time=row("SS Time")->findChild<QTimeEdit *>("valueTime");time->setTime(QTime(23,30));QTest::mouseClick(outside,Qt::LeftButton);
  const int old_scroll=outer.verticalScrollBar()->value();wheel(time,-120);require(ir::number(n,"SS Time",0)==84600&&outer.verticalScrollBar()->value()>old_scroll,"未选中的时间控件吞掉滚轮");
  time=row("SS Time")->findChild<QTimeEdit *>("valueTime");QTest::mouseClick(row("SS Time")->findChild<QLabel *>("valueLabel"),Qt::LeftButton);wheel(time,120);require(ir::number(n,"SS Time",-1)==0,"选中后的时间滚轮未跨午夜");
  groups->setCurrentItem(solar);app.processEvents();time=row("SS Time")->findChild<QTimeEdit *>("valueTime");require(time->time()==QTime(0,0)&&!item("SS Day")->isHidden(),"两个分组没有共享同一时间值");wheel(time,-120);require(ir::number(n,"SS Time",-1)==0,"切换分组后保留了旧滚轮选择");
  require(!item("SS UTC Offset")->isHidden()&&row("SS UTC Offset")->findChild<QLabel *>("valueLabel")->text()=="SS UTC Offset"&&row("SS UTC Offset")->findChild<QDoubleSpinBox *>("valueSpin")->value()==.5,"Sun-Sky 没有共享夜景修改后的原有 SS UTC Offset");
  groups->setCurrentItem(night);app.processEvents();
  require(!item("SS Latitude")->isHidden()&&!item("SS Longitude")->isHidden(),"夜景缺少共享经纬度");
  auto popup=[]()->QWidget *{for(auto *w:QApplication::topLevelWidgets())if(w->objectName()=="SolarLocationPopup")return w;return nullptr;};
  auto open_map=[&](const char *id){auto *w=row(id);require(!w->findChild<QToolButton *>("rangeButton")&&!w->findChild<QToolButton *>("precisionButton"),"经纬度仍保留限/精按钮");auto *button=w->findChild<QToolButton *>("solarMapButton");require(button&&button->text()=="Map","经纬度缺少 Map 按钮");QCursor::setPos(button->mapToGlobal(button->rect().center()));QTest::mouseClick(button,Qt::LeftButton);app.processEvents();auto *p=popup();require(p,"Map 未打开小窗");QCursor::setPos(p->mapToGlobal(p->rect().center()));return p;};
  auto *p=open_map("SS Latitude");auto *map=dynamic_cast<editor::SolarLocationMap *>(p->findChild<QWidget *>("solarLocationMap"));require(map,"小窗缺少世界地图");
  // 独立的已知墨卡托坐标：北纬 66.51326° 对应上方四分之一，赤道居中。
  const auto box=map->map_rect();const auto known=map->unproject({box.left()+box.width()*.75,box.top()+box.height()*.25});require(std::abs(known[0]-66.513260443)<1e-8&&std::abs(known[1]-90)<1e-8,"地图不是墨卡托投影");
  require(std::abs(map->project(0,0).y()-box.center().y())<1e-8&&std::isfinite(map->project(90,180).y()),"赤道或极区投影错误");
  const auto before_lat=ir::number(n,"SS Latitude",0),before_lon=ir::number(n,"SS Longitude",0);
  auto pick=map->project(35,110).toPoint();QTest::mouseClick(map,Qt::LeftButton,Qt::NoModifier,pick);require(location_commits==0&&ir::number(n,"SS Latitude",0)==before_lat&&ir::number(n,"SS Longitude",0)==before_lon,"地图草稿提前写入场景");
  QTest::keyClick(p->findChild<QPushButton *>("SolarLocationOK"),Qt::Key_Escape);app.processEvents();require(!popup()&&location_commits==0,"Esc 未取消地图草稿");
  p=open_map("SS Longitude");map=dynamic_cast<editor::SolarLocationMap *>(p->findChild<QWidget *>("solarLocationMap"));pick=map->project(-33,151).toPoint();QTest::mouseClick(map,Qt::LeftButton,Qt::NoModifier,pick);const auto expected=map->unproject(pick);
  const auto map_dir=QDir::currentPath()+"/artifacts/location-moon/ui";QDir().mkpath(map_dir);p->grab().save(map_dir+"/map.png");
  QTest::mouseClick(p->findChild<QPushButton *>("SolarLocationOK"),Qt::LeftButton);app.processEvents();require(location_commits==1&&std::abs(ir::number(n,"SS Latitude",0)-expected[0])<1e-9&&std::abs(ir::number(n,"SS Longitude",0)-expected[1])<1e-9,"OK 没有一次提交经纬度");
  require(ir::number(n,"SS UTC Offset",99)==10&&ir::number(n,"SS Time",99)==0,"Map 未设置最近时区或篡改了当地时间");
  groups->setCurrentItem(solar);app.processEvents();require(std::abs(row("SS Latitude")->findChild<QDoubleSpinBox *>("valueSpin")->value()-expected[0])<1e-6&&std::abs(row("SS Longitude")->findChild<QDoubleSpinBox *>("valueSpin")->value()-expected[1])<1e-6,"Sun-Sky 没有同步地图位置");
  p=open_map("SS Latitude");map=dynamic_cast<editor::SolarLocationMap *>(p->findChild<QWidget *>("solarLocationMap"));QTest::mouseClick(map,Qt::LeftButton,Qt::NoModifier,map->project(20,-70).toPoint());QCursor::setPos(p->mapToGlobal(QPoint(-40,-40)));QEvent leave(QEvent::Leave);QApplication::sendEvent(p,&leave);QTest::qWait(150);require(!popup()&&location_commits==1&&ir::number(n,"SS Longitude",0)==expected[1],"离开地图后仍提交了草稿");
  require(ir::number(n,"SS UTC Offset",99)==10,"取消地图草稿改变了 UTC");
  utc=row("SS UTC Offset")->findChild<QDoubleSpinBox *>("valueSpin");utc->setValue(3.5);p=open_map("SS Longitude");QTest::mouseClick(p->findChild<QPushButton *>("SolarLocationOK"),Qt::LeftButton);app.processEvents();require(location_commits==2&&ir::number(n,"SS UTC Offset",99)==10,"同一地点确认未重新匹配时区");
  auto *lat_spin=row("SS Latitude")->findChild<QDoubleSpinBox *>("valueSpin");const auto lat_value=lat_spin->value();QTest::mouseClick(outside,Qt::LeftButton);wheel(lat_spin,120);require(ir::number(n,"SS Latitude",0)==expected[0],"未选中经纬度时滚轮误改值");
  lat_spin=row("SS Latitude")->findChild<QDoubleSpinBox *>("valueSpin");QTest::mouseClick(row("SS Latitude")->findChild<QLabel *>("valueLabel"),Qt::LeftButton);wheel(lat_spin,120);require(std::abs(ir::number(n,"SS Latitude",0)-lat_value-1)<1e-5,"选中经纬度后滚轮不能调参");
  groups->setCurrentItem(night);app.processEvents();
  const auto dir=QDir::currentPath()+"/artifacts/night-sky/ui";QDir().mkpath(dir);outer.verticalScrollBar()->setValue(0);app.processEvents();QTest::qWait(160);outer.grab().save(dir+"/enabled.png");
  check=row("DFV Night Enabled")->findChild<QCheckBox *>("valueCheck");QTest::mouseClick(check,Qt::LeftButton,Qt::NoModifier,QPoint(8,check->height()/2));require(!ir::night_enabled(n)&&!row("DFV Night Stars Intensity")->findChild<QDoubleSpinBox *>("valueSpin")->isEnabled(),"取消勾选没有禁用参数");
  outer.grab().save(dir+"/disabled.png");std::cout<<"Night Qt checkbox / dependencies / wheel / native arrows / outer scroll PASS\n";
}
