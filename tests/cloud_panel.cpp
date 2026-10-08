#include "cloud/panel.h"
#include "editor/ground_panel.h"
#include "editor/ui_scale.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTest>
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <QToolButton>
#include <iostream>
using namespace dfv;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
static void wheel(QWidget *w,int delta=-120){const QPointF p=w->rect().center();QWheelEvent e(p,w->mapToGlobal(p.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&e);QApplication::processEvents();}
static QImage arrows(QDoubleSpinBox *spin){QStyleOptionSpinBox option;option.initFrom(spin);option.rect={0,0,200,30};option.state=QStyle::State_Enabled|QStyle::State_Active;option.frame=true;option.buttonSymbols=spin->buttonSymbols();option.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;QImage image(200,30,QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter painter(&image);spin->style()->drawComplexControl(QStyle::CC_SpinBox,&option,&painter,spin);painter.end();return image.copy(170,0,30,30);}
int main(int argc,char **argv){QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");app.setFont(QFont(QStringLiteral("Microsoft YaHei"),9));try{
  QScrollArea outer;outer.setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);auto *panel=new cloud::Panel;layout->addWidget(panel);auto *outside=new QPushButton("outside");layout->addWidget(outside);outer.setWidget(body);outer.resize(500,500);
  cloud::Cloud v;v.id="cloud";std::vector<cloud::Candidate> candidates;for(int i=0;i<80;++i)candidates.push_back({std::to_string(i),"Meteor "+std::to_string(i)});panel->bind(&v,candidates);outer.show();QApplication::processEvents();
  auto *x=panel->findChild<QDoubleSpinBox *>("x"),*y=panel->findChild<QDoubleSpinBox *>("y");auto *tree=panel->findChild<QTreeWidget *>("CloudSources");editor::GroundPanel ground;ground.bind(true,0,0,false);auto *reference=ground.findChild<QDoubleSpinBox *>("GroundAlignmentOffset");
  for(auto *spin:panel->findChildren<QDoubleSpinBox *>())check(QFontInfo(spin->font()).pixelSize()==QFontInfo(x->font()).pixelSize(),"cloud numbers have inconsistent font sizes");
  for(auto *label:panel->findChildren<QLabel *>())check(QFontInfo(label->font()).pixelSize()==QFontInfo(x->font()).pixelSize(),"cloud labels have inconsistent font sizes");
  if(argc>1)outer.grab().save(QString::fromLocal8Bit(argv[1]));
  check(arrows(x)==arrows(reference),"unselected arrows differ from ground");x->setFocus();wheel(x);check(x->value()==0&&outer.verticalScrollBar()->value()>0,"focus/hover wheel edited value or swallowed scroll");
  QTest::mouseClick(x->findChild<QLineEdit *>(),Qt::LeftButton);wheel(x,120);check(x->value()==.1,"internal editor click did not select row");check(arrows(x)==arrows(reference),"selected arrows differ from ground");
  v.config.x=.1;panel->bind(&v,candidates);wheel(x,120);check(x->value()==.2,"same-object refresh lost selection");wheel(y);check(y->value()==0,"other row accepted wheel");
  QTest::mouseClick(outside,Qt::LeftButton);wheel(x,120);check(x->value()==.2,"outside click retained selection");QTest::mouseClick(x,Qt::LeftButton);panel->hide();panel->show();wheel(x,120);check(x->value()==.2,"hide retained selection");
  QTest::mouseClick(x,Qt::LeftButton);QTest::mouseClick(panel,Qt::LeftButton,Qt::NoModifier,QPoint(1,panel->height()-1));wheel(x,120);check(x->value()==.2,"panel blank area retained selection");
  QTest::mouseClick(x,Qt::LeftButton);v.id="second";panel->bind(&v,candidates);wheel(x,120);check(x->value()==.1,"object switch retained selection");
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
  auto *distribution=panel->findChild<QComboBox *>("CloudDistribution");auto *noise=panel->findChild<QCheckBox *>("CloudDistributionNoise");auto *threshold=panel->findChild<QDoubleSpinBox *>("distribution_threshold");
  check(distribution&&distribution->count()==2&&distribution->itemText(0)==QStringLiteral("方形")&&distribution->itemText(1)==QStringLiteral("圆形"),"distribution must contain only square and circular");check(noise&&threshold&&!noise->isChecked(),"distribution noise checkbox/default missing");
  auto noise_fields=[&](bool enabled){for(const auto *id:{"distribution_threshold","distribution_scale","distribution_detail"}){auto *spin=panel->findChild<QDoubleSpinBox *>(id);check(spin->isEnabled()==enabled,"noise field enable state incorrect");auto *row=spin->parentWidget();for(const auto *button:{"rangeButton","precisionButton"}){auto *b=row->findChild<QToolButton *>(button);check(b&&b->isEnabled()==enabled,"disabled noise row retained active range/precision buttons");}bool found=false;for(auto *label:panel->findChildren<QLabel *>())if(label->property("cloudWheelKey").toString()==QLatin1String(id)){found=true;check(label->isEnabled()==enabled,"noise label enable state incorrect");}check(found,"noise parameter label missing");}for(const auto *id:{"edge_fade","detail","warp","erosion"})check(panel->findChild<QDoubleSpinBox *>(id)->isEnabled(),"noise switch disabled an independent cloud field");};
  noise_fields(false);edits=0;outer.ensureWidgetVisible(noise);QApplication::processEvents();QTest::mouseClick(noise,Qt::LeftButton,Qt::NoModifier,QPoint(8,noise->height()/2));noise_fields(true);check(edits==1&&submitted.distribution_noise,"noise click did not publish configuration");
  threshold->setValue(.63);check(submitted.distribution_threshold==.63,"threshold edit not published");distribution->setCurrentIndex(1);check(submitted.distribution==1&&submitted.distribution_noise,"switching to circular lost noise");
  QTest::mouseClick(noise,Qt::LeftButton,Qt::NoModifier,QPoint(8,noise->height()/2));noise_fields(false);check(!submitted.distribution_noise&&submitted.distribution_threshold==.63,"disabling noise erased authored threshold");
  const auto before=edits;wheel(threshold,120);check(edits==before&&threshold->value()==.63,"disabled threshold accepted wheel edit");
  QTest::mouseClick(noise,Qt::LeftButton,Qt::NoModifier,QPoint(8,noise->height()/2));noise_fields(true);check(submitted.distribution_noise&&submitted.distribution_threshold==.63,"re-enabling noise lost threshold");
  v.config=submitted;const auto restored=cloud::from_json(cloud::json(v));panel->bind(restored.get(),{});noise_fields(true);check(noise->isChecked()&&distribution->currentIndex()==1&&threshold->value()==.63,"bind did not restore shape/noise/threshold");
  if(argc>1){QApplication::processEvents();outer.ensureWidgetVisible(panel->findChild<QDoubleSpinBox *>("distribution_detail"));QApplication::processEvents();outer.grab().save(QString::fromLocal8Bit(argv[1])+"-noise.png");}
  v.config.distribution_noise=false;panel->bind(&v,{});noise_fields(false);
  // 使用实际应用的缩放器，并检查内部编辑器和点击高亮后的实际字体。
  editor::UiScale ui_scale(false);ui_scale.set_percent(150);QApplication::processEvents();
  auto fonts=[&]{const int expected=QFontInfo(app.font()).pixelSize();for(auto *w:panel->findChildren<QWidget *>())if(qobject_cast<QLabel *>(w)||qobject_cast<QDoubleSpinBox *>(w)||qobject_cast<QLineEdit *>(w)){
    if(QFontInfo(w->font()).pixelSize()!=expected)std::cerr<<"font mismatch: "<<w->metaObject()->className()<<" "<<w->objectName().toStdString()<<" actual="<<QFontInfo(w->font()).pixelSize()<<" expected="<<expected<<'\n';
    check(QFontInfo(w->font()).pixelSize()==expected,"scaled cloud font mismatch");}};
  fonts();panel->bind(&v,{});fonts();outer.ensureWidgetVisible(x);QApplication::processEvents();QTest::mouseClick(x->findChild<QLineEdit *>(),Qt::LeftButton);fonts();QTest::mouseClick(y->findChild<QLineEdit *>(),Qt::LeftButton);fonts();
  // 渲染器更新候选对象时不能覆盖尚未提交的键盘草稿。
  outer.activateWindow();QTest::qWait(50);QApplication::setActiveWindow(&outer);x->setFocus();x->selectAll();check(QApplication::focusWidget(),"cloud editor did not get focus");QTest::keyClicks(QApplication::focusWidget(),"12345.");const auto draft=x->text();panel->bind(&v,{});check(x->text()==draft,"cloud refresh overwrote keyboard draft");
  QTest::keyClicks(QApplication::focusWidget(),"678");QTest::keyClick(QApplication::focusWidget(),Qt::Key_Return);check(submitted.x==12345.678,"cloud keyboard edit failed after refresh");
  for(const auto &locale:{QLocale(QLocale::English,QLocale::UnitedStates),QLocale(QLocale::German),QLocale(QLocale::Chinese)}){
    x->setLocale(locale);x->setValue(12345.678);check(!x->text().contains(locale.groupSeparator()),"numeric text contains grouping separator");
  }
  x->setLocale(QLocale::c());
  for(int percent:{80,200,100,150}){ui_scale.set_percent(percent);QApplication::processEvents();fonts();panel->bind(&v,{});fonts();}
  outer.ensureWidgetVisible(x);outer.activateWindow();QTest::qWait(50);QApplication::setActiveWindow(&outer);x->setFocus();QApplication::processEvents();
  check(QApplication::activeWindow()==&outer&&x->hasFocus(),"cloud editor was not focused before hover");
  auto *precision=x->parentWidget()->findChild<QToolButton *>("precisionButton");
  QTest::mouseMove(precision,precision->rect().center());QCursor::setPos(precision->mapToGlobal(precision->rect().center()));QEnterEvent enter(precision->rect().center(),precision->rect().center(),precision->mapToGlobal(precision->rect().center()));QApplication::sendEvent(precision,&enter);QTest::qWait(350);
  auto *popup=panel->findChild<QWidget *>("ParameterSettingsPopup");check(popup&&popup->isVisible(),"hover settings popup did not open");
  // offscreen 插件忽略 WA_ShowWithoutActivating；原生伴随测试验证窗口激活和键盘路由。
  if(QGuiApplication::platformName()=="windows"){
    check(QApplication::activeWindow()==&outer&&x->hasFocus(),"hover settings popup stole keyboard focus");
    x->selectAll();QTest::keyClicks(QApplication::focusWidget(),"123.45");check(x->text()=="123.45","hover popup intercepted keyboard input");
    // 用户点击工具窗口后仍可输入，不能以禁止窗口获得焦点来规避抢焦点。
    popup->activateWindow();QTest::qWait(50);auto *step=popup->findChild<QDoubleSpinBox *>("precisionStep");QTest::mouseClick(step->findChild<QLineEdit *>(),Qt::LeftButton);step->selectAll();
    check(QApplication::focusWidget()&&(step->hasFocus()||step->isAncestorOf(QApplication::focusWidget())),"settings popup cannot receive keyboard focus after click");
    QTest::keyClicks(QApplication::focusWidget(),"0.25");QTest::mouseClick(popup->findChild<QPushButton *>("ParameterSettingsOK"),Qt::LeftButton);QApplication::processEvents();check(x->singleStep()==.25,"hover popup keyboard draft did not commit with OK");
  }
  if(argc>1){QApplication::processEvents();outer.ensureWidgetVisible(panel->findChild<QDoubleSpinBox *>("distribution_detail"));QApplication::processEvents();outer.grab().save(QString::fromLocal8Bit(argv[1])+"-disabled.png");}
  std::cout<<"PASS: real Qt click/wheel, internal editor, outside/hide/switch, native arrows, 80-row expansion, collision budget, shape/noise controls and restored threshold\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
