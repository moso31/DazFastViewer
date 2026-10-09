#include "render_ir/night_sky.h"
#include <QCheckBox>
static void night_sky_panel(QApplication &app){
  using namespace dfv;
  ir::OptionNode n;n.id="night-test";ir::ensure_night_options(n);
  QScrollArea outer;outer.resize(660,480);outer.setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);
  auto *outside=new QPushButton("Outside");layout->addWidget(outside);auto *panel=new editor::ParameterPanel;panel->shared_scroll(&outer);layout->addWidget(panel);outer.setWidget(body);
  auto bind=[&]{panel->bind_options(&n,[&](size_t i,size_t k,double value){ir::set_option(n,i,k,value);});};bind();outer.show();app.processEvents();
  auto *rows=panel->findChild<QTreeWidget *>("parameterRows");
  auto row=[&](int index){rows->doItemsLayout();const auto pos=rows->viewport()->mapTo(body,rows->visualItemRect(rows->topLevelItem(index)).center());outer.ensureVisible(pos.x(),pos.y(),0,100);app.processEvents();QTest::qWait(100);auto *w=rows->itemWidget(rows->topLevelItem(index),0);require(w,"夜景行未挂载");return w;};
  auto wheel=[&](QWidget *w,int delta){const auto pos=w->rect().center();QWheelEvent event(pos,w->mapToGlobal(pos),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&event);app.processEvents();};
  auto *check=row(0)->findChild<QCheckBox *>("valueCheck");require(check&&!check->isChecked(),"夜景缺少默认关闭的勾选框");
  require(!row(1)->findChild<QDoubleSpinBox *>("valueSpin")->isEnabled(),"夜景关闭后参数未禁用");
  check=row(0)->findChild<QCheckBox *>("valueCheck");QTest::mouseClick(check,Qt::LeftButton,Qt::NoModifier,QPoint(8,check->height()/2));require(ir::night_enabled(n),"勾选框未启用夜景");
  auto *spin=row(2)->findChild<QDoubleSpinBox *>("valueSpin");require(spin&&spin->isEnabled(),"勾选后星光参数未恢复");
  const int scroll=outer.verticalScrollBar()->value();wheel(spin->findChild<QLineEdit *>(),-120);require(ir::number(n,"DFV Night Stars Intensity",0)==1&&outer.verticalScrollBar()->value()>scroll,"未选中夜景参数吞掉滚轮");
  spin=row(2)->findChild<QDoubleSpinBox *>("valueSpin");auto *label=row(2)->findChild<QLabel *>("valueLabel");
  auto arrows=[&](QDoubleSpinBox *s){QStyleOptionSpinBox o;o.initFrom(s);o.rect=s->rect();o.frame=s->hasFrame();o.buttonSymbols=s->buttonSymbols();return std::pair{s->style()->subControlRect(QStyle::CC_SpinBox,&o,QStyle::SC_SpinBoxUp,s),s->style()->subControlRect(QStyle::CC_SpinBox,&o,QStyle::SC_SpinBoxDown,s)};};
  const auto native=arrows(spin);QTest::mouseClick(label,Qt::LeftButton);wheel(spin,120);require(std::abs(ir::number(n,"DFV Night Stars Intensity",0)-1.1)<1e-6&&!label->styleSheet().isEmpty(),"选中夜景行后滚轮未调参");require(arrows(spin)==native&&spin->styleSheet().isEmpty(),"夜景选中状态改变了原生箭头");
  wheel(row(3)->findChild<QDoubleSpinBox *>("valueSpin"),-120);require(ir::number(n,"DFV Night Limiting Magnitude",0)==6.5,"另一行被误改");
  bind();spin=row(2)->findChild<QDoubleSpinBox *>("valueSpin");wheel(spin,120);require(std::abs(ir::number(n,"DFV Night Stars Intensity",0)-1.2)<1e-6,"同对象刷新丢失选中行");
  QTest::mouseClick(outside,Qt::LeftButton);wheel(spin,120);require(std::abs(ir::number(n,"DFV Night Stars Intensity",0)-1.2)<1e-6,"外部点击未取消选中");
  // 实际输入超出旧 0..100 的值，刷新后仍保留；共享“限”是唯一人为范围。
  auto *moon=row(7)->findChild<QDoubleSpinBox *>("valueSpin");auto *range=row(7)->findChild<QToolButton *>("rangeButton");
  require(moon&&range&&!range->isChecked()&&moon->maximum()>1000,"月光仍存在隐藏的 0..100 范围");
  QTest::mouseClick(moon,Qt::LeftButton);moon->selectAll();QTest::keyClicks(moon,"500");QTest::keyClick(moon,Qt::Key_Return);app.processEvents();require(ir::number(n,"DFV Night Moon Intensity",0)==500,"手动输入被第二套限幅截断");
  bind();moon=row(7)->findChild<QDoubleSpinBox *>("valueSpin");require(moon->value()==500,"参数刷新截断了月光强度");
  auto *settings=dynamic_cast<editor::parameter_widgets::SettingsButtons *>(row(7)->findChild<QWidget *>("parameterSettings"));require(settings,"月光缺少共享范围设置");
  settings->commit({true,200,800,.1});moon=row(7)->findChild<QDoubleSpinBox *>("valueSpin");require(moon->minimum()==200&&moon->maximum()==800,"自定义范围被旧隐藏范围求交截断");
  auto *slider=row(7)->findChild<QSlider *>("valueSlider");require(slider,"月光缺少滑轨");QTest::mouseClick(row(7)->findChild<QLabel *>("valueLabel"),Qt::LeftButton);for(int k=0;k<5;++k)QTest::keyClick(slider,Qt::Key_PageUp);require(ir::number(n,"DFV Night Moon Intensity",0)==800,"滑轨与自定义范围不一致");
  range=row(7)->findChild<QToolButton *>("rangeButton");QTest::mouseClick(range,Qt::LeftButton);moon=row(7)->findChild<QDoubleSpinBox *>("valueSpin");moon->setValue(1200);require(ir::number(n,"DFV Night Moon Intensity",0)==1200,"关闭范围后仍受隐藏上限限制");
  auto *last=row(int(n.parameters.size()-1));require(last->isVisible()&&rows->verticalScrollBar()->maximum()==0,"夜景有嵌套滚动条或最后一行不可达");
  const auto dir=QDir::currentPath()+"/artifacts/night-sky/ui";QDir().mkpath(dir);outer.grab().save(dir+"/enabled.png");
  check=row(0)->findChild<QCheckBox *>("valueCheck");QTest::mouseClick(check,Qt::LeftButton,Qt::NoModifier,QPoint(8,check->height()/2));require(!ir::night_enabled(n)&&!row(2)->findChild<QDoubleSpinBox *>("valueSpin")->isEnabled(),"取消勾选没有禁用参数");
  outer.grab().save(dir+"/disabled.png");std::cout<<"Night Qt checkbox / dependencies / wheel / native arrows / outer scroll PASS\n";
}
