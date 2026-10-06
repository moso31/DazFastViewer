#include "render_ir/matte_fog.h"
#include "editor/ground_panel.h"
#include "editor/hdr_color_dialog.h"
#include <QTimer>
#include <QDir>
#include <QStyleOptionSpinBox>
#include <QStyle>
static void matte_fog_panel(QApplication &app) {
  using namespace dfv;
  ir::RenderOptions options;options.environment.id="fog-environment";ir::ensure_matte_fog_options(options.environment);
  QScrollArea outer;outer.resize(620,440);outer.setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);
  auto *outside=new QPushButton("Outside parameter rows");layout->addWidget(outside);
  auto *ground=new editor::GroundPanel;ground->bind(true,0,0,false);layout->addWidget(ground);
  auto *panel=new editor::ParameterPanel;panel->shared_scroll(&outer);layout->addWidget(panel);outer.setWidget(body);
  panel->bind_options(&options.environment,[&](size_t i,size_t k,double value){ir::set_option(options.environment,i,k,value);});outer.show();app.processEvents();
  auto *rows=panel->findChild<QTreeWidget *>("parameterRows");
  auto row=[&](int index){rows->doItemsLayout();const auto pos=rows->viewport()->mapTo(body,rows->visualItemRect(rows->topLevelItem(index)).center());outer.ensureVisible(pos.x(),pos.y(),0,90);app.processEvents();QTest::qWait(180);auto *w=rows->itemWidget(rows->topLevelItem(index),0);require(w,"Fog row not mounted");return w;};
  auto spin=[&](int index){auto *s=row(index)->findChild<QDoubleSpinBox *>("valueSpin");require(s&&s->isEnabled(),"Fog value unavailable");return s;};
  auto wheel=[&](QWidget *w,int delta){const auto pos=w->rect().center();QWheelEvent event(pos,w->mapToGlobal(pos),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&event);app.processEvents();};
  auto *visibility=spin(1);const int before=outer.verticalScrollBar()->value();wheel(visibility->findChild<QLineEdit *>(),-120);
  require(ir::number(options.environment,"Matte Fog Visibility",0)==10000&&outer.verticalScrollBar()->value()>before,"Unselected fog editor swallowed page wheel");
  visibility=spin(1);auto *label=row(1)->findChild<QLabel *>("valueLabel");QTest::mouseClick(label,Qt::LeftButton);wheel(visibility,120);
  require(visibility->value()==10010&&!label->styleSheet().isEmpty(),"Fog click selection / wheel edit missing");
  panel->evaluated({});wheel(visibility,120);require(visibility->value()==10020,"Data refresh cleared fog selection");
  panel->bind_options(&options.environment,[&](size_t i,size_t k,double value){ir::set_option(options.environment,i,k,value);});visibility=spin(1);wheel(visibility,120);require(visibility->value()==10030,"Same environment rebind cleared fog selection");
  auto *other=spin(3);wheel(other,-120);require(other->value()==1&&ir::number(options.environment,"Matte Fog Visibility",0)==10030,"Hovering another fog row changed values");visibility=spin(1);
  const auto current=visibility->value();outside->click();QTest::mouseClick(outside,Qt::LeftButton);wheel(visibility,-120);require(visibility->value()==current,"Outside click retained fog wheel selection");
  visibility=spin(1);QTest::mouseClick(row(1)->findChild<QLabel *>("valueLabel"),Qt::LeftButton);panel->hide();panel->show();app.processEvents();wheel(visibility,-120);require(visibility->value()==current,"Hidden fog panel retained wheel permission");
  require(rows->topLevelItemCount()==9,"Color channels were not grouped into palette rows");
  auto *last=spin(8);require(last->isVisible()&&rows->verticalScrollBar()->maximum()==0,"Fog rows nested or last row unreachable");
  QTest::mouseClick(row(8)->findChild<QLabel *>("valueLabel"),Qt::LeftButton);wheel(last,120);require(last->value()==1010,"Last fog parameter not editable");
  visibility=spin(1);auto &fog=options.environment;
  // Compare native subcontrol layout to the GroundPanel, before and after click.
  auto subcontrols=[](QDoubleSpinBox *s){QStyleOptionSpinBox opt;opt.initFrom(s);opt.rect=QRect(0,0,125,26);opt.frame=s->hasFrame();opt.buttonSymbols=s->buttonSymbols();opt.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;return std::pair{s->style()->subControlRect(QStyle::CC_SpinBox,&opt,QStyle::SC_SpinBoxUp,s),s->style()->subControlRect(QStyle::CC_SpinBox,&opt,QStyle::SC_SpinBoxDown,s)};};
  const auto ground_arrows=subcontrols(ground->findChild<QDoubleSpinBox *>("GroundAlignmentRatio"));
  require(subcontrols(visibility)==ground_arrows&&visibility->styleSheet().isEmpty(),"Fog spinbox differs from native GroundPanel arrows");
  const auto dir=QString::fromStdString((std::filesystem::path(__FILE__).parent_path().parent_path()/"artifacts/matte-fog/ui").string());QDir().mkpath(dir);
  outer.grab().save(dir+"/before.png");QTest::mouseClick(row(1)->findChild<QLabel *>("valueLabel"),Qt::LeftButton);app.processEvents();outer.grab().save(dir+"/selected.png");
  require(subcontrols(visibility)==ground_arrows&&visibility->styleSheet().isEmpty(),"Fog selection changed native arrow layout");
  ground->grab().save(dir+"/ground-reference.png");
  auto replacement=fog;replacement.id="another-environment";panel->bind_options(&replacement,[&](size_t i,size_t k,double value){ir::set_option(replacement,i,k,value);});visibility=spin(1);wheel(visibility,-120);require(ir::number(replacement,"Matte Fog Visibility",0)==current,"New environment inherited wheel selection");
  int commits=0;
  panel->bind_options(&fog,[&](size_t i,size_t k,double value){ir::set_option(fog,i,k,value);},[&](size_t i,const auto &v){++commits;for(size_t k=0;k<3;++k)ir::set_option(fog,i,k,v[k]);});
  const auto initial=ir::color(fog,"Matte Fog Brightness Tint");
  auto palette=[&](bool change,bool accept){auto *button=row(5)->findChild<QPushButton *>("valueColor");require(button,"Fog color palette missing");
    QTimer::singleShot(0,[&]{auto *dialog=dynamic_cast<editor::HdrColorDialog *>(QApplication::activeModalWidget());require(dialog,"Material color dialog not reused");
      auto *exposure=dialog->findChild<QDoubleSpinBox *>("hdrExposure");const auto before=exposure->value();wheel(exposure,-120);require(exposure->value()==before,"Unselected palette exposure swallowed wheel");
      QTest::mouseClick(exposure,Qt::LeftButton);wheel(exposure,-120);require(exposure->value()<before,"Selected palette exposure rejected wheel");exposure->setValue(before);
      if(change)dialog->findChild<QColorDialog *>("hdrBaseColor")->setCurrentColor(QColor::fromRgbF(.2,.4,.8));
      dialog->grab().save(dir+"/palette.png");if(accept)dialog->accept();else dialog->reject();});
    QTest::mouseClick(button,Qt::LeftButton);
  };
  palette(false,true);require(commits==0,"Opening color dialog changed exact stored RGB");
  palette(true,false);require(commits==0&&ir::color(fog,"Matte Fog Brightness Tint").x==initial.x,"Cancelled palette changed color");
  palette(true,true);const auto edited=ir::color(fog,"Matte Fog Brightness Tint");
  require(commits==1&&std::abs(edited.x-editor::hdr_color::linear(.2))<1e-4&&std::abs(edited.z-editor::hdr_color::linear(.8))<1e-4,"Palette did not commit all linear RGB components together");
  std::cout<<"Matte fog Qt controls / native arrows / outer scroll: PASS\n";
}
