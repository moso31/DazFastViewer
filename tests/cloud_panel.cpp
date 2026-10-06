#include "cloud/panel.h"
#include "editor/ground_panel.h"
#include <QApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTest>
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <iostream>
using namespace dfv;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
static void wheel(QWidget *w,int delta=-120){const QPointF p=w->rect().center();QWheelEvent e(p,w->mapToGlobal(p.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&e);QApplication::processEvents();}
static QImage arrows(QDoubleSpinBox *spin){QStyleOptionSpinBox option;option.initFrom(spin);option.rect={0,0,200,30};option.state=QStyle::State_Enabled|QStyle::State_Active;option.frame=true;option.buttonSymbols=spin->buttonSymbols();option.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;QImage image(200,30,QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter painter(&image);spin->style()->drawComplexControl(QStyle::CC_SpinBox,&option,&painter,spin);painter.end();return image.copy(170,0,30,30);}
int main(int argc,char **argv){QApplication app(argc,argv);try{
  QScrollArea outer;outer.setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);auto *panel=new cloud::Panel;layout->addWidget(panel);auto *outside=new QPushButton("outside");layout->addWidget(outside);outer.setWidget(body);outer.resize(500,500);
  cloud::Cloud v;v.id="cloud";std::vector<cloud::Candidate> candidates;for(int i=0;i<80;++i)candidates.push_back({std::to_string(i),"Meteor "+std::to_string(i)});panel->bind(&v,candidates);outer.show();QApplication::processEvents();
  auto *x=panel->findChild<QDoubleSpinBox *>("x"),*y=panel->findChild<QDoubleSpinBox *>("y");auto *tree=panel->findChild<QTreeWidget *>("CloudSources");editor::GroundPanel ground;ground.bind(true,0,0,false);auto *reference=ground.findChild<QDoubleSpinBox *>("GroundAlignmentOffset");
  check(arrows(x)==arrows(reference),"unselected arrows differ from ground");x->setFocus();wheel(x);check(x->value()==0&&outer.verticalScrollBar()->value()>0,"focus/hover wheel edited value or swallowed scroll");
  QTest::mouseClick(x->findChild<QLineEdit *>(),Qt::LeftButton);wheel(x,120);check(x->value()==10,"internal editor click did not select row");check(arrows(x)==arrows(reference),"selected arrows differ from ground");
  v.config.x=10;panel->bind(&v,candidates);wheel(x,120);check(x->value()==20,"same-object refresh lost selection");wheel(y);check(y->value()==0,"other row accepted wheel");
  QTest::mouseClick(outside,Qt::LeftButton);wheel(x,120);check(x->value()==20,"outside click retained selection");QTest::mouseClick(x,Qt::LeftButton);panel->hide();panel->show();wheel(x,120);check(x->value()==20,"hide retained selection");
  QTest::mouseClick(x,Qt::LeftButton);QTest::mouseClick(panel,Qt::LeftButton,Qt::NoModifier,QPoint(1,panel->height()-1));wheel(x,120);check(x->value()==20,"panel blank area retained selection");
  QTest::mouseClick(x,Qt::LeftButton);v.id="second";panel->bind(&v,candidates);wheel(x,120);check(x->value()==10,"object switch retained selection");
  cloud::Config submitted;int edits=0;panel->changed=[&](cloud::Config c){submitted=c;++edits;};for(int i=0;i<17;++i)tree->topLevelItem(i)->setCheckState(0,Qt::Checked);check(submitted.sources.size()==16&&edits==16&&tree->topLevelItem(16)->checkState(0)==Qt::Unchecked,"source budget UI failed");
  check(tree->verticalScrollBar()->maximum()==0&&tree->horizontalScrollBar()->maximum()==0,"nested cloud list scrolling");check(tree->visualItemRect(tree->topLevelItem(79)).bottom()<=tree->viewport()->height(),"last collision object clipped");
  outer.verticalScrollBar()->setValue(0);wheel(tree->viewport());check(outer.verticalScrollBar()->value()>0,"object list swallowed outer wheel");outer.ensureWidgetVisible(panel->findChild<QLabel *>("CloudStatus"));QApplication::processEvents();check(outer.verticalScrollBar()->value()>1000,"cannot reach final object via outer scroll");
  v.config.sources={"7","2"};for(size_t i=0;i<candidates.size();++i)candidates[i].volume=double(i*i*i);v.id="sorted";panel->bind(&v,candidates);auto *sort=panel->findChild<QPushButton *>("CloudSortSources");check(sort&&sort->y()<tree->y(),"AABB button missing above sources");
  check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="79","initial cloud AABB sort is not numeric descending");edits=0;
  candidates[2].volume=1e9;panel->bind(&v,candidates);check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="79","data refresh unexpectedly reordered cloud sources");
  panel->refresh_candidates=[&]{panel->bind(&v,candidates);};candidates[3].volume=2e9;QTest::mouseClick(sort,Qt::LeftButton);check(edits==0&&tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="3","sort click failed to fetch fresh AABBs or edited scene");
  int checked=0;for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->checkState(0)==Qt::Checked){++checked;check(tree->topLevelItem(i)->data(0,Qt::UserRole).toString()=="2"||tree->topLevelItem(i)->data(0,Qt::UserRole).toString()=="7","sort changed source identity");}check(checked==2,"sort lost checked sources");
  auto pending=candidates;for(auto &o:pending)o.volume=-1;panel->bind(&v,pending);check(!sort->isEnabled(),"sort allowed stale/unavailable bounds");panel->bind(&v,candidates);check(sort->isEnabled()&&tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="3","pending geometry refresh lost source order");
  panel->findChild<QDoubleSpinBox *>("density")->setValue(.1);check(submitted.sources==v.config.sources,"next parameter edit reordered authored sources after sorting");
  v.id="loading";auto unknown=candidates;for(auto &o:unknown)o.volume=-1;panel->bind(&v,unknown);panel->bind(&v,candidates);check(tree->topLevelItem(0)->data(0,Qt::UserRole).toString()=="3","first evaluated bounds failed to establish sort order");
  check(tree->verticalScrollBar()->maximum()==0&&tree->visualItemRect(tree->topLevelItem(79)).bottom()<=tree->viewport()->height(),"sorting introduced an inner scroll range or clipped last row");panel->refresh_candidates={};
  v.config.x=275.36309814453125;v.config.height=20.435169219970703;panel->bind(&v,candidates);panel->findChild<QDoubleSpinBox *>("time")->setValue(6.5);check(submitted.x==v.config.x&&submitted.height==v.config.height&&submitted.time==6.5,"time edit rounded untouched coordinates and caused a scene rebuild");
  std::cout<<"PASS: real Qt click/wheel, internal editor, outside/hide/switch, native arrows, 80-row expansion, collision budget\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
