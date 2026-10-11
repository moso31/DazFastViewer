#include "editor/extension_panel.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>
#include "editor/ground_panel.h"
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <QSpinBox>
static void extension_panel(QApplication &app){
  dfv::editor::ExtensionPanel panel;panel.resize(380,540);dfv::runtime::ObjectExtension value;value.kind=dfv::runtime::ExtensionKind::growth;panel.bind(value,true);panel.show();
  panel.changed=[&](auto v,bool,double){value=v;panel.bind(value);};app.processEvents();
  auto capture=[&](const char *suffix,QWidget *widget){const auto prefix=qEnvironmentVariable("DFV_EXTENSION_CAPTURE");if(!prefix.isEmpty())require(widget->grab().save(prefix+suffix),"无法保存面板截图");};capture("-initial.png",&panel);
  auto *age=panel.findChild<QDoubleSpinBox *>("GrowthAge");auto *step=panel.findChild<QDoubleSpinBox *>("GrowthAgeStep");
  QWheelEvent unselected(age->rect().center(),age->mapToGlobal(age->rect().center()),QPoint{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(age,&unselected);require(age->value()==3,"未点击选中的年龄被滚轮修改");
  require(age->minimum()==1&&age->maximum()==20&&age->singleStep()==1,"年龄固定范围或默认步进错误");
  auto arrows=[](QDoubleSpinBox *s){QStyleOptionSpinBox option;option.initFrom(s);option.rect={0,0,200,30};option.state=QStyle::State_Enabled|QStyle::State_Active;option.frame=true;option.buttonSymbols=s->buttonSymbols();option.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;QImage image(200,30,QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter painter(&image);s->style()->drawComplexControl(QStyle::CC_SpinBox,&option,&painter,s);painter.end();return image.copy(170,0,30,30);};
  dfv::editor::GroundPanel ground;ground.bind(true,0,0,false);const auto reference=arrows(ground.findChild<QDoubleSpinBox *>("GroundAlignmentOffset"));require(arrows(age)==reference,"年龄未选中时的箭头不同于地面对齐");
  for(const char *name:{"GrowthAge","GrowthAgeStep","GrowthStrength","GrowthSensitivity"})require(!panel.findChild<QWidget *>(QString(name)+"Row")->findChild<QToolButton *>("rangeButton")&&!panel.findChild<QWidget *>(QString(name)+"Row")->findChild<QToolButton *>("precisionButton"),"生长参数仍显示限／精");
  step->stepUp();require(step->value()==1.25,"步长控件的按钮增量不是 0.25");
  QTest::mouseClick(step,Qt::LeftButton);QWheelEvent step_wheel(step->rect().center(),step->mapToGlobal(step->rect().center()),QPoint{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(step,&step_wheel);require(step->value()==1,"步长控件的滚轮增量不是 0.25");
  step->setValue(.25);age->stepUp();require(age->value()==3.25,"年龄上下按钮不遵循步长");
  QTest::mouseClick(age,Qt::LeftButton);QWheelEvent wheel(age->rect().center(),age->mapToGlobal(age->rect().center()),QPoint{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(age,&wheel);require(age->value()==3.5,"年龄滚轮不遵循步长");
  require(arrows(age)==reference,"年龄选中后改变了原生箭头样式");
  step->setValue(2);age->stepDown();require(age->value()==1.5,"步长变更未立即生效");age->stepDown();require(age->value()==1,"年龄步进越过下界");
  auto *strength=panel.findChild<QDoubleSpinBox *>("GrowthStrength");auto *sense=panel.findChild<QDoubleSpinBox *>("GrowthSensitivity");app.processEvents();
  const double physique=strength->value(),sensitivity=sense->value();strength->stepUp();sense->stepUp();require(strength->value()==physique+.125&&sense->value()==sensitivity+2.5,"体格或每年增长的固定精度错误");
  strength->setValue(1.5);step->setValue(25);sense->setValue(200000);require(strength->value()==1.5&&step->value()==25&&sense->value()==200000,"生长参数仍被旧范围截断");
  require(age->mapTo(&panel,QPoint{}).y()==step->mapTo(&panel,QPoint{}).y()&&strength->mapTo(&panel,QPoint{}).y()==sense->mapTo(&panel,QPoint{}).y()&&strength->mapTo(&panel,QPoint{}).x()<sense->mapTo(&panel,QPoint{}).x(),"两行布局顺序错误");
  require(!panel.findChild<QPushButton *>("GrowthApply"),"应用当前年龄按钮未移除");
  auto *multiplier=panel.findChild<QDoubleSpinBox *>("GrowthMultiplier");auto *round=panel.findChild<QToolButton *>("GrowthRound");require(multiplier&&multiplier->value()==1&&multiplier->singleStep()==.05&&!round->isChecked(),"倍率初始设置错误");
  require(multiplier->mapTo(&panel,QPoint{}).y()>sense->mapTo(&panel,QPoint{}).y(),"倍率没有另起一行");
  QWheelEvent unused(multiplier->rect().center(),multiplier->mapToGlobal(multiplier->rect().center()),QPoint{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(multiplier,&unused);require(multiplier->value()==1,"倍率未选中就接受滚轮");
  QTest::mouseClick(multiplier,Qt::LeftButton);QWheelEvent selected(multiplier->rect().center(),multiplier->mapToGlobal(multiplier->rect().center()),QPoint{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(multiplier,&selected);require(multiplier->value()==1.05&&arrows(multiplier)==reference,"倍率步进或原生箭头错误");
  int multiply=0,divide=0;panel.multiply_scale=[&](bool divided){divided?++divide:++multiply;};panel.findChild<QPushButton *>("GrowthMultiply")->click();panel.findChild<QPushButton *>("GrowthDivide")->click();require(multiply==1&&divide==1,"乘除按钮未独立派发");
  capture("-selected.png",&panel);
  round->click();require(value.scale_round,"round 未启用");QTest::mouseMove(round,round->rect().center());QCursor::setPos(round->mapToGlobal(round->rect().center()));QEnterEvent enter_round(round->rect().center(),round->rect().center(),round->mapToGlobal(round->rect().center()));QApplication::sendEvent(round,&enter_round);QTest::qWait(350);auto *popup=panel.findChild<QWidget *>("GrowthRoundPopup");require(popup&&popup->isVisible(),"round 悬停未显示弹窗");auto *digits=popup->findChild<QSpinBox *>("GrowthRoundExponent");require(digits->value()==1&&digits->minimum()<=-4&&digits->maximum()==std::numeric_limits<int>::max(),"round 位数范围错误");capture("-round.png",popup);digits->setValue(-4);require(value.scale_round_exponent==1,"round 草稿提前生效");popup->findChild<QPushButton *>("GrowthRoundOK")->click();require(value.scale_round_exponent==-4,"round OK 未保存");round->click();require(!value.scale_round,"round 未关闭");
  dfv::runtime::WeightResult result;result.height_cm=175;result.kg=70;for(int i=0;i<13;++i){dfv::runtime::PartWeight part;part.kg=.5;part.triangles=1;result.parts.push_back(part);}panel.result(result);
  auto *summary=panel.findChild<QLabel *>("WeightSummary");auto *parts=panel.findChild<QLabel *>("WeightParts");auto *details=panel.findChild<QToolButton *>("WeightDetails");
  require(!details->isChecked()&&!parts->isVisible(),"部位详情未默认收回");details->click();require(parts->isVisible()&&parts->text().contains(QStringLiteral("<b>手臂：</b>"))&&parts->text().contains(QStringLiteral("<b>脚：</b>"))&&!parts->text().contains(QStringLiteral("左"))&&!parts->text().contains(QStringLiteral("右")),"部位粗体或单侧文字错误");
  panel.measurement_scale("2");require(summary->text().contains("350 cm")&&summary->text().contains("560 kg")&&parts->text().contains("4 kg"),"全局倍率未同步部位与汇总");
  require(!summary->text().contains(QStringLiteral("体积"))&&!panel.findChild<QLabel *>("WeightStatus")->isVisible(),"旧说明未移除");
  std::string scale="1";QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());if(!dialog)return;dialog->findChild<QLineEdit *>("MeasurementScale")->setText("1e2000");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
  require(dfv::editor::edit_measurement_scale(&panel,scale)&&scale=="1e2000","测量倍率对话框未接收大数");panel.measurement_scale(scale);require(summary->text().contains("1.75e1997 km"),"大数倍率未显示");
  panel.bind({},true);panel.bind(value);require(summary->text()==QStringLiteral("等待测量…")&&!details->isChecked(),"新注册对象继承了上一对象的测量显示");
}
