#include "editor/extension_panel.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>
static void extension_panel(QApplication &app){
  dfv::editor::ExtensionPanel panel;panel.resize(380,540);dfv::runtime::ObjectExtension value;value.kind=dfv::runtime::ExtensionKind::growth;panel.bind(value,true);panel.show();
  panel.changed=[&](auto v,bool,double){value=v;panel.bind(value);};app.processEvents();
  auto *age=panel.findChild<QDoubleSpinBox *>("GrowthAge");auto *step=panel.findChild<QDoubleSpinBox *>("GrowthAgeStep");
  step->stepUp();require(step->value()==1.1,"步长控件的按钮增量不是 0.25");
  QTest::mouseClick(step,Qt::LeftButton);QWheelEvent step_wheel(step->rect().center(),step->mapToGlobal(step->rect().center()),QPoint{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(step,&step_wheel);require(step->value()==1,"步长控件的滚轮增量不是 0.25");
  step->setValue(.25);age->stepUp();require(age->value()==3.25,"年龄上下按钮不遵循步长");
  QTest::mouseClick(age,Qt::LeftButton);QWheelEvent wheel(age->rect().center(),age->mapToGlobal(age->rect().center()),QPoint{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(age,&wheel);require(age->value()==3.5,"年龄滚轮不遵循步长");
  step->setValue(2);age->stepDown();require(age->value()==1.5,"步长变更未立即生效");age->stepDown();require(age->value()==1,"年龄步进越过下界");
  auto *strength=panel.findChild<QDoubleSpinBox *>("GrowthStrength");auto *sense=panel.findChild<QDoubleSpinBox *>("GrowthSensitivity");app.processEvents();
  require(age->mapTo(&panel,QPoint{}).y()==step->mapTo(&panel,QPoint{}).y()&&strength->mapTo(&panel,QPoint{}).y()==sense->mapTo(&panel,QPoint{}).y()&&strength->mapTo(&panel,QPoint{}).x()<sense->mapTo(&panel,QPoint{}).x(),"两行布局顺序错误");
  require(!panel.findChild<QPushButton *>("GrowthApply"),"应用当前年龄按钮未移除");
  dfv::runtime::WeightResult result;result.height_cm=175;result.kg=70;for(int i=0;i<13;++i){dfv::runtime::PartWeight part;part.kg=.5;part.triangles=1;result.parts.push_back(part);}panel.result(result);
  auto *summary=panel.findChild<QLabel *>("WeightSummary");auto *parts=panel.findChild<QLabel *>("WeightParts");auto *details=panel.findChild<QToolButton *>("WeightDetails");
  require(!details->isChecked()&&!parts->isVisible(),"部位详情未默认收回");details->click();require(parts->isVisible()&&parts->text().contains(QStringLiteral("<b>手臂：</b>"))&&parts->text().contains(QStringLiteral("<b>脚：</b>"))&&!parts->text().contains(QStringLiteral("左"))&&!parts->text().contains(QStringLiteral("右")),"部位粗体或单侧文字错误");
  panel.measurement_scale("2");require(summary->text().contains("350 cm")&&summary->text().contains("560 kg")&&parts->text().contains("4 kg"),"全局倍率未同步部位与汇总");
  require(!summary->text().contains(QStringLiteral("体积"))&&!panel.findChild<QLabel *>("WeightStatus")->isVisible(),"旧说明未移除");
  std::string scale="1";QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());if(!dialog)return;dialog->findChild<QLineEdit *>("MeasurementScale")->setText("1e2000");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
  require(dfv::editor::edit_measurement_scale(&panel,scale)&&scale=="1e2000","测量倍率对话框未接收大数");panel.measurement_scale(scale);require(summary->text().contains("1.75e1997 km"),"大数倍率未显示");
  panel.bind({},true);panel.bind(value);require(summary->text()==QStringLiteral("等待测量…")&&!details->isChecked(),"新注册对象继承了上一对象的测量显示");
}
