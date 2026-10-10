#include "water/panel.h"
#include "editor/ground_panel.h"
#include <QApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTest>
#include <QWheelEvent>
#include <QHeaderView>
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <iostream>
using namespace dfv;
static void check(bool v,const char *s){if(!v)throw std::runtime_error(s);}
static void wheel(QWidget *w,int delta=-120){const QPointF p=w->rect().center();QWheelEvent e(p,w->mapToGlobal(p.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&e);QApplication::processEvents();}
static QImage arrows(QDoubleSpinBox *spin){QStyleOptionSpinBox option;option.initFrom(spin);option.rect={0,0,200,30};option.state=QStyle::State_Enabled|QStyle::State_Active;option.frame=true;option.buttonSymbols=spin->buttonSymbols();option.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;QImage image(200,30,QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter painter(&image);spin->style()->drawComplexControl(QStyle::CC_SpinBox,&option,&painter,spin);painter.end();return image.copy(170,0,30,30);}
int main(int argc,char **argv){QApplication app(argc,argv);try{
  QScrollArea outer;outer.setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);auto *panel=new water::Panel;layout->addWidget(panel);outer.setWidget(body);outer.resize(450,380);
  water::Water water;water.id="one";std::vector<water::Candidate> objects;for(int i=0;i<80;++i)objects.push_back({std::to_string(i),"Candidate "+std::to_string(i),double(i*i*i)});panel->bind(&water,objects);outer.show();QApplication::processEvents();
  auto *x=panel->findChild<QDoubleSpinBox *>("x"),*y=panel->findChild<QDoubleSpinBox *>("y");auto *tree=panel->findChild<QTreeWidget *>("WaterSources");
  editor::GroundPanel ground;ground.bind(true,0,0,false);auto *reference=ground.findChild<QDoubleSpinBox *>("GroundAlignmentOffset");check(arrows(x)==arrows(reference),"unselected water arrows differ from GroundPanel");
  check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="79","AABB default sort is not descending numeric order");
  auto *largest=tree->topLevelItem(0);largest->setCheckState(0,Qt::Checked);auto *largest_mode=qobject_cast<QComboBox *>(tree->itemWidget(largest,1));largest_mode->setCurrentIndex(1);
  auto *sort=panel->findChild<QPushButton *>("WaterSortSources");check(tree->columnCount()==2&&sort&&sort->y()<tree->y(),"sort button missing above two-column table");
  tree->sortItems(0,Qt::AscendingOrder);QApplication::processEvents();check(tree->topLevelItem(79)==largest,"numeric sort failed");
  int submitted=0;panel->changed=[&](water::Config,bool){++submitted;};sort->click();check(submitted==0,"sorting submitted a scene edit");panel->changed={};
  check(tree->topLevelItem(0)==largest&&largest->checkState(0)==Qt::Checked&&tree->itemWidget(largest,1)==largest_mode&&largest_mode->currentIndex()==1,"sorting changed source identity or mode");
  objects[2].volume=1e9;panel->bind(&water,objects);check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="79","refresh reordered rows without a click");sort->click();check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="2","button did not use refreshed AABB volumes");
  panel->refresh_candidates=[&]{panel->bind(&water,objects);};objects[3].volume=2e9;sort->click();check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="3","button did not fetch current evaluated bounds");panel->refresh_candidates={};objects[3].volume=27;
  water.id="loading";auto unknown=objects;for(auto &o:unknown)o.volume=-1;panel->bind(&water,unknown);panel->bind(&water,objects);check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="2","first available bounds did not establish initial order");
  x->setFocus();wheel(x);check(x->value()==0,"focus without click permitted wheel edit");check(outer.verticalScrollBar()->value()>0,"unselected spin swallowed outer scrolling");
  QTest::mouseClick(x,Qt::LeftButton);wheel(x,120);check(x->value()==.1,"selected spin rejected wheel edit");
  check(arrows(x)==arrows(reference),"selected water arrows differ from GroundPanel");
  auto *density=panel->findChild<QDoubleSpinBox *>("density"),*foam=panel->findChild<QDoubleSpinBox *>("foam_uv_scale");water::Config edited;panel->changed=[&](water::Config c,bool){edited=c;};density->setValue(12);foam->setValue(2.5);check(edited.density==8&&edited.foam_uv_scale==2.5,"new parameters did not submit or density exceeded 8x");
  panel->changed={};
  panel->bind(&water,objects);wheel(x,120);check(x->value()==.1,"same-object refresh lost selected row");
  wheel(y);check(y->value()==0,"wheel over another row changed value");
  panel->hide();panel->show();wheel(x);check(x->value()==.1,"hidden panel retained selection");
  auto *combo=qobject_cast<QComboBox *>(tree->itemWidget(tree->topLevelItem(0),1));combo->setFocus();wheel(combo);check(combo->currentIndex()==0,"unselected combo changed mode");
  QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(tree->topLevelItem(0)).center());wheel(combo);check(combo->currentIndex()==1,"selected source row could not wheel-edit mode");
  water.id="two";panel->bind(&water,objects);combo=qobject_cast<QComboBox *>(tree->itemWidget(tree->topLevelItem(0),1));wheel(combo);check(combo->currentIndex()==0,"object switch retained old selected source");
  check(tree->verticalScrollBar()->maximum()==0&&tree->horizontalScrollBar()->maximum()==0,"table still has nested scrolling");
  check(tree->visualItemRect(tree->topLevelItem(79)).bottom()<=tree->viewport()->height(),"table clips last candidate");
  const auto before=outer.verticalScrollBar()->value();wheel(tree->viewport());check(outer.verticalScrollBar()->value()>before,"table swallowed outer scrolling");
  outer.ensureWidgetVisible(panel->findChild<QPushButton *>("WaterRecalculate"));QApplication::processEvents();check(outer.verticalScrollBar()->value()>0,"outer scrollbar cannot reach table end");
  auto *manual=panel->findChild<QCheckBox *>("WaterManualLod");auto *level=panel->findChild<QDoubleSpinBox *>("lod_level");
  check(manual&&!manual->isChecked()&&level&&level->value()==0&&level->isEnabled(),"Missing automatic/manual LOD defaults");
  panel->changed=[&](water::Config c,bool){edited=c;};outer.ensureWidgetVisible(manual);QApplication::processEvents();QTest::mouseClick(manual,Qt::LeftButton,Qt::NoModifier,QPoint(8,manual->height()/2));check(edited.manual_lod&&level->isEnabled(),"Manual LOD checkbox did not submit");
  const auto scroll=outer.verticalScrollBar()->value();level->setFocus();wheel(level);check(level->value()==0&&outer.verticalScrollBar()->value()>scroll,"Unselected LOD consumed wheel edit");
  QTest::mouseClick(level,Qt::LeftButton);wheel(level,120);check(edited.lod_level==1&&arrows(level)==arrows(reference),"Selected LOD failed native wheel edit");
  panel->busy(true);check(!manual->isEnabled()&&!level->isEnabled(),"Busy panel permits LOD edit");panel->busy(false);check(level->isEnabled(),"Manual LOD did not restore after work");
  auto small=water;small.id="small";small.config.width=small.config.length=32;panel->bind(&small,{});water.config.manual_lod=true;water.config.lod_level=10;panel->bind(&water,{});std::cout<<"Restored LOD "<<level->value()<<" / "<<level->maximum()<<" manual="<<manual->isChecked()<<'\n';check(level->value()==10&&manual->isChecked(),"Switching water clamped saved LOD to previous object's range");
  std::cout<<"PASS: selected wheel, refresh, switch, hide, combo, expanded 80-row table\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
