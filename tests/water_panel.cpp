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
  tree->sortItems(2,Qt::AscendingOrder);QApplication::processEvents();check(tree->topLevelItem(79)==largest&&largest->checkState(0)==Qt::Checked&&tree->itemWidget(largest,1)==largest_mode&&largest_mode->currentIndex()==1,"sorting changed source identity or mode");
  check(tree->topLevelItem(2)->data(0,Qt::UserRole).toString()=="2","AABB sort used display text instead of numeric volume");
  x->setFocus();wheel(x);check(x->value()==0,"focus without click permitted wheel edit");check(outer.verticalScrollBar()->value()>0,"unselected spin swallowed outer scrolling");
  QTest::mouseClick(x,Qt::LeftButton);wheel(x,120);check(x->value()==1,"selected spin rejected wheel edit");
  check(arrows(x)==arrows(reference),"selected water arrows differ from GroundPanel");
  auto *density=panel->findChild<QDoubleSpinBox *>("density"),*foam=panel->findChild<QDoubleSpinBox *>("foam_uv_scale");water::Config edited;panel->changed=[&](water::Config c,bool){edited=c;};density->setValue(12);foam->setValue(2.5);check(edited.density==8&&edited.foam_uv_scale==2.5,"new parameters did not submit or density exceeded 8x");panel->changed={};
  panel->bind(&water,objects);wheel(x,120);check(x->value()==1,"same-object refresh lost selected row");
  wheel(y);check(y->value()==0,"wheel over another row changed value");
  panel->hide();panel->show();wheel(x);check(x->value()==1,"hidden panel retained selection");
  auto *combo=qobject_cast<QComboBox *>(tree->itemWidget(tree->topLevelItem(0),1));combo->setFocus();wheel(combo);check(combo->currentIndex()==0,"unselected combo changed mode");
  QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(tree->topLevelItem(0)).center());wheel(combo);check(combo->currentIndex()==1,"selected source row could not wheel-edit mode");
  water.id="two";panel->bind(&water,objects);combo=qobject_cast<QComboBox *>(tree->itemWidget(tree->topLevelItem(0),1));wheel(combo);check(combo->currentIndex()==0,"object switch retained old selected source");
  check(tree->verticalScrollBar()->maximum()==0&&tree->horizontalScrollBar()->maximum()==0,"table still has nested scrolling");
  check(tree->visualItemRect(tree->topLevelItem(79)).bottom()<=tree->viewport()->height(),"table clips last candidate");
  const auto before=outer.verticalScrollBar()->value();wheel(tree->viewport());check(outer.verticalScrollBar()->value()>before,"table swallowed outer scrolling");
  outer.ensureWidgetVisible(panel->findChild<QPushButton *>("WaterRecalculate"));QApplication::processEvents();check(outer.verticalScrollBar()->value()>0,"outer scrollbar cannot reach table end");
  std::cout<<"PASS: selected wheel, refresh, switch, hide, combo, expanded 80-row table\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
