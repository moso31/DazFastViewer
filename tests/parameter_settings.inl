#include "editor/parameter_widgets.h"
#include "editor/ground_panel.h"
static void parameter_settings(QApplication &app){
  using namespace dfv::editor;using namespace dfv::editor::parameter_widgets;
  ParameterPanel panel;panel.resize(580,460);double value=.5;ParameterControl c;c.id="precision";c.label="精度与范围";c.read=[&]{return value;};c.write=[&](double v){value=v;};
  panel.bind_controls({c});panel.show();app.processEvents();QTest::qWait(180);
  auto *spin=panel.findChild<QDoubleSpinBox *>("valueSpin");auto *range=panel.findChild<QToolButton *>("rangeButton");auto *precision=panel.findChild<QToolButton *>("precisionButton");
  require(spin&&range&&precision&&!range->isChecked()&&spin->singleStep()==.1,"原生数值默认范围／精度错误");
  QTest::mouseClick(range,Qt::LeftButton);require(range->isChecked()&&spin->minimum()==0&&spin->maximum()==1,"范围开关未限制输入");spin->setValue(8);require(value==1,"启用范围后数值越界");
  QTest::mouseClick(precision,Qt::LeftButton);app.processEvents();
  auto *popup=panel.findChild<QWidget *>("ParameterSettingsPopup");require(popup&&popup->isVisible(),"精度编辑窗未显示");
  auto *step=popup->findChild<QDoubleSpinBox *>("precisionStep");step->setValue(.025);QTest::mouseClick(popup->findChild<QPushButton *>("ParameterSettingsOK"),Qt::LeftButton);app.processEvents();require(spin->singleStep()==.025,"OK 未提交精度");
  QTest::mouseClick(precision,Qt::LeftButton);app.processEvents();popup=panel.findChild<QWidget *>("ParameterSettingsPopup");require(popup,"取消测试缺少弹窗");popup->findChild<QDoubleSpinBox *>("precisionStep")->setValue(.7);
  QTest::mouseMove(popup,QPoint(15,15));QTest::mouseMove(&panel,QPoint(5,panel.height()-5));QCursor::setPos(panel.mapToGlobal(QPoint(5,panel.height()-5)));QEvent leave(QEvent::Leave);QApplication::sendEvent(popup,&leave);QTest::qWait(180);require(spin->singleStep()==.025,"离开弹窗提交了待取消精度");
  QTest::mouseMove(range,range->rect().center());QCursor::setPos(range->mapToGlobal(range->rect().center()));QEnterEvent enter(range->rect().center(),range->rect().center(),range->mapToGlobal(range->rect().center()));QApplication::sendEvent(range,&enter);QTest::qWait(320);popup=panel.findChild<QWidget *>("ParameterSettingsPopup");require(popup&&popup->isVisible(),"悬停范围按钮未打开弹窗");
  const auto button_rect=QRect(range->mapToGlobal(QPoint()),range->size());require(popup->frameGeometry().adjusted(-2,-2,2,2).intersects(button_rect),"按钮和弹窗之间存在鼠标无法跨越的间隙");
  popup->findChild<QDoubleSpinBox *>("rangeMinimum")->setValue(-2);popup->findChild<QDoubleSpinBox *>("rangeMaximum")->setValue(3);QTest::mouseClick(popup->findChild<QPushButton *>("ParameterSettingsOK"),Qt::LeftButton);app.processEvents();require(spin->minimum()==-2&&spin->maximum()==3,"范围 OK 未生效");
  panel.bind_controls({c});app.processEvents();QTest::qWait(180);spin=panel.findChild<QDoubleSpinBox *>("valueSpin");require(spin->singleStep()==.025&&spin->minimum()==-2&&spin->maximum()==3,"控件重建丢失范围或精度");
  context(&panel)->bind("another");require(spin->singleStep()==.1&&spin->maximum()>1e10,"不同对象之间泄漏设置");context(&panel)->bind("");require(spin->singleStep()==.025&&spin->maximum()==3,"切回对象未恢复设置");
  c.id="authored";c.settings=dfv::runtime::ParameterSettings{true,-15,75,.25};panel.bind_controls({c});app.processEvents();QTest::qWait(180);spin=panel.findChild<QDoubleSpinBox *>("valueSpin");
  require(spin->minimum()==-15&&spin->maximum()==75&&spin->singleStep()==.25,"DUF 通道范围和精度未优先继承");
  c.settings.reset();c.id="native-again";panel.bind_controls({c});app.processEvents();QTest::qWait(180);spin=panel.findChild<QDoubleSpinBox *>("valueSpin");require(spin->minimum()<-1e10&&spin->singleStep()==.1,"DUF 元数据泄漏至原生参数");
  panel.hide();
  GroundPanel ground;ground.bind(true,0,0,false);ground.show();app.processEvents();auto *offset=ground.findChild<QDoubleSpinBox *>("GroundAlignmentOffset");require(offset&&offset->parentWidget()->findChild<QToolButton *>("rangeButton"),"单行浮点框没有范围按钮");
  auto *buttons=offset->parentWidget()->findChild<QToolButton *>("precisionButton");require(buttons&&std::abs(buttons->mapTo(&ground,QPoint()).y()-offset->mapTo(&ground,QPoint()).y())<10,"普通浮点参数被拆成两行");
}
