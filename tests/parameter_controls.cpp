#include "editor/parameters.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QTest>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  try {
    double value=.5;dfv::editor::ParameterControl c;c.id="test";c.label="数值";c.minimum=0;c.maximum=1;c.read=[&]{return value;};c.write=[&](double v){value=v;};
    dfv::editor::ParameterPanel panel;panel.resize(380,480);panel.bind_controls({c});panel.show();app.processEvents();QTest::qWait(180);
    auto *spin=panel.findChild<QDoubleSpinBox *>("valueSpin");auto *slider=panel.findChild<QSlider *>("valueSlider");require(spin&&slider,"参数行未创建");
    require(slider->width()>=32,"无限数值范围的文本框挤占了滑轨");
    spin->setFocus();spin->selectAll();QTest::keyClicks(spin,"12345.");panel.evaluated({});QTest::qWait(200);panel.evaluated({});QTest::keyClicks(spin,"678");QTest::keyClick(spin,Qt::Key_Return);require(std::abs(value-12345.678)<1e-6,"右侧输入被范围限制或被后台刷新覆盖");
    spin->selectAll();QTest::keyClicks(spin,"-8.25");QTest::keyClick(spin,Qt::Key_Tab);require(value==-8.25,"失焦未提交负数文本");
    bool interacting=false;int released=0;panel.interaction_changed=[&](bool active){interacting=active;if(!active) ++released;};
    const auto center=slider->rect().center();QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,center);require(interacting,"拖动没有进入交互状态");
    const QPoint beyond(slider->width()*3,center.y());QMouseEvent move(QEvent::MouseMove,beyond,slider->mapToGlobal(beyond),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(slider,&move);
    QTest::mouseRelease(slider,Qt::LeftButton,Qt::NoModifier,beyond);require(value>1,"拖动越过标尺后被截断");
    require(!interacting&&released==1,"释放没有结束交互状态");
    QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,center);QEvent lost(QEvent::UngrabMouse);QApplication::sendEvent(slider,&lost);require(!interacting,"捕获丢失未结束交互状态");
    QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,center);QFocusEvent unfocus(QEvent::FocusOut);QApplication::sendEvent(slider,&unfocus);require(!interacting,"失焦未结束交互状态");
    std::cout<<"Numeric text / focus commit / unbounded slider: PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}
}
