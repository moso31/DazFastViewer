#include "editor/special_controls.h"
#include <QStyleOptionSpinBox>
static void special_panel(QApplication &app) {
  using namespace dfv::editor;
  float strength=0;bool shadow=false;
  std::vector<ParameterControl> controls{
    forefoot_control([&]{return double(strength)*100;},[&](double v){strength=float(v/100);}),
    local_shadow_control([&]{return shadow?1.:0.;},[&](double v){shadow=v!=0;})};
  for(int i=0;i<30;++i){ParameterControl c;c.id="filler/"+std::to_string(i);c.label="其他参数 "+std::to_string(i);c.group="/特殊处理";c.read=[]{return 0.;};c.write=[](double){};controls.push_back(c);}
  QScrollArea outer;outer.resize(650,420);outer.setWidgetResizable(true);auto *page=new QWidget;auto *layout=new QVBoxLayout(page);auto *panel=new ParameterPanel;panel->shared_scroll(&outer);layout->addWidget(panel);outer.setWidget(page);
  auto settle=[&]{app.processEvents();QTest::qWait(180);};panel->bind_controls(controls);outer.show();settle();
  auto *groups=panel->findChild<QTreeWidget *>("parameterGroups");auto *rows=panel->findChild<QTreeWidget *>("parameterRows");auto *bar=outer.verticalScrollBar();
  QTreeWidgetItem *special=nullptr,*independent=nullptr;
  for(QTreeWidgetItemIterator it(groups);*it;++it){if((*it)->text(0)==QStringLiteral("特殊处理"))special=*it;if((*it)->text(0)==QStringLiteral("独立阴影结构"))independent=*it;}
  require(special&&independent&&independent->parent()==special,"特殊处理或独立阴影结构位置错误");
  auto top=[&]{bar->setValue(0);settle();};
  auto row=[&]{auto *w=rows->itemWidget(rows->topLevelItem(0),0);require(w,"前掌参数行未挂载");return w;};
  auto spin=[&]{auto *w=row()->findChild<QDoubleSpinBox *>("valueSpin");require(w,"前掌数值框未挂载");return w;};
  auto wheel=[&](QWidget *w,int delta){const QPointF p(w->rect().center());QWheelEvent event(p,w->mapToGlobal(p.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&event);settle();};
  require(rows->verticalScrollBar()->maximum()==0&&bar->maximum()>0,"特殊处理形成了内层滚动范围");
  wheel(spin()->findChild<QLineEdit *>(),-120);require(strength==0&&bar->value()>0,"未选中的前掌参数抢占滚轮");top();
  auto arrow=[&](QStyle::SubControl control){QStyleOptionSpinBox option;option.initFrom(spin());option.rect=spin()->rect();option.buttonSymbols=spin()->buttonSymbols();return spin()->style()->subControlRect(QStyle::CC_SpinBox,&option,control,spin());};
  const auto up=arrow(QStyle::SC_SpinBoxUp),down=arrow(QStyle::SC_SpinBoxDown);const auto style=spin()->styleSheet();
  QTest::mouseClick(row()->findChild<QLabel *>("valueLabel"),Qt::LeftButton);wheel(spin(),120);require(strength==.001f&&bar->value()==0,"选中后前掌滚轮调参失败");
  require(arrow(QStyle::SC_SpinBoxUp)==up&&arrow(QStyle::SC_SpinBoxDown)==down&&spin()->styleSheet()==style,"选中前掌后原生箭头布局改变");
  for(int v:{50,100,0}){spin()->selectAll();QTest::keyClicks(spin(),std::to_string(v).c_str());QTest::keyClick(spin(),Qt::Key_Return);require(strength==float(v/100.),"前掌百分比输入转换错误");}
  spin()->selectAll();QTest::keyClicks(spin(),"50.25");QTest::keyClick(spin(),Qt::Key_Return);require(strength==.5025f,"前掌百分比丢失小数精度");
  spin()->setValue(150);require(strength==1,"前掌数学定义域允许超过 100%");spin()->setValue(-1);require(strength==0,"前掌数学定义域允许负值");
  panel->hide();panel->show();settle();wheel(spin(),-120);require(strength==0&&bar->value()>0,"隐藏后前掌保留旧滚轮权限");top();
  panel->bind_controls(controls);settle();wheel(spin(),-120);require(strength==0&&bar->value()>0,"重新绑定保留旧滚轮权限");top();
  // 用真实组选择验证阴影入口仍可编辑，并保留既有通道身份。
  independent=nullptr;for(QTreeWidgetItemIterator it(groups);*it;++it)if((*it)->text(0)==QStringLiteral("独立阴影结构"))independent=*it;
  QTest::mouseClick(groups->viewport(),Qt::LeftButton,Qt::NoModifier,groups->visualItemRect(independent).center());settle();auto *shadow_row=rows->itemWidget(rows->topLevelItem(1),0);auto *choice=shadow_row?shadow_row->findChild<QComboBox *>("valueChoice"):nullptr;
  if(!choice||!choice->isVisible()){outer.grab().save("special-shadow-debug.png");std::cerr<<"shadow group="<<groups->currentItem()->data(0,Qt::UserRole).toString().toStdString()<<" hidden="<<rows->topLevelItem(1)->isHidden()<<" row="<<shadow_row<<" choice="<<choice<<" scroll="<<bar->value()<<"\n";}
  require(choice&&choice->isVisible(),"独立阴影结构没有显示原控件");choice->setCurrentIndex(1);require(shadow&&controls[1].id=="render/local_shadow","移动阴影分组改变旧通道语义");
  groups->setCurrentItem(independent->parent());settle();outer.grab().save("special-panel.png");
  bar->setValue(bar->maximum());settle();require(rows->itemWidget(rows->topLevelItem(rows->topLevelItemCount()-1),0),"特殊处理最后一行无法通过外层滚动访问");
  std::cout<<"Special controls / percentage / outer scroll / selection / native arrows / shadow group: PASS\n";
}
