#pragma once
#include <QSlider>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>
#include <functional>
#include <QPainter>
#include <QStyleOptionSlider>

namespace dfv::editor {
// 滑轨只是数值标尺。拖动按相对距离累加，越过轨道边缘仍可继续改变值。
class NumericSlider final:public QSlider {
  double value_=0,span_=1,step_=.1,anchor_value_=0;
  double minimum_=0,maximum_=1;
  bool limited_=false;
  double anchor_x_=0,anchor_span_=1;
  bool dragging_=false;
  void commit(double v) {v=std::round(v/step_)*step_;if(limited_)v=std::clamp(v,minimum_,maximum_);if(std::isfinite(v)&&edited) edited(v);}
protected:
  void paintEvent(QPaintEvent *event) override {
    if(!limited_){QSlider::paintEvent(event);return;}
    QStyleOptionSlider option;initStyleOption(&option);QPainter painter(this);option.subControls=QStyle::SC_SliderGroove;style()->drawComplexControl(QStyle::CC_Slider,&option,&painter,this);
    const auto handle=style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderHandle,this);
    painter.setRenderHint(QPainter::Antialiasing);painter.setPen(palette().color(QPalette::Mid));painter.setBrush(palette().color(isEnabled()?QPalette::Highlight:QPalette::Mid));
    const QRectF capsule(handle.center().x()-5,handle.center().y()-9,10,18);painter.drawRoundedRect(capsule,5,5);
  }
  bool event(QEvent *e) override {
    if(e->type()==QEvent::UngrabMouse||e->type()==QEvent::FocusOut||e->type()==QEvent::Hide||e->type()==QEvent::WindowDeactivate) {dragging_=false;setSliderDown(false);}
    return QSlider::event(e);
  }
  void mousePressEvent(QMouseEvent *e) override {
    if(e->button()!=Qt::LeftButton) {QSlider::mousePressEvent(e);return;}
    setFocus();dragging_=true;setSliderDown(true);anchor_x_=e->position().x();anchor_value_=value_;anchor_span_=span_;e->accept();
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    if(!dragging_) return;
    const double precision=e->modifiers().testFlag(Qt::ShiftModifier)?.1:1;
    commit(anchor_value_+(e->position().x()-anchor_x_)*anchor_span_/std::max(1,width()-16)*precision);e->accept();
  }
  void mouseReleaseEvent(QMouseEvent *e) override {if(e->button()==Qt::LeftButton) {dragging_=false;setSliderDown(false);}e->accept();}
  // 面板将滚轮交给同一行的十进制数字框步进，避免 float 回读累加产生尾数。
  void wheelEvent(QWheelEvent *e) override {if(wheeled) wheeled(e);else e->ignore();}
  void keyPressEvent(QKeyEvent *e) override {
    const int key=e->key();
    if(key==Qt::Key_Left||key==Qt::Key_Down) commit(value_-step_);
    else if(key==Qt::Key_Right||key==Qt::Key_Up) commit(value_+step_);
    else if(key==Qt::Key_PageUp) commit(value_+span_*.1);
    else if(key==Qt::Key_PageDown) commit(value_-span_*.1);
    else {e->ignore();return;}e->accept();
  }
public:
  void cancel_drag() {dragging_=false;setSliderDown(false);}
  std::function<void(double)> edited;
  std::function<void(QWheelEvent *)> wheeled;
  NumericSlider():QSlider(Qt::Horizontal) {setRange(0,1000);setProperty("historyInput",true);}
  void sync(double value,double low,double high,double step,bool limited=false) {
    limited_=limited;minimum_=low;maximum_=high;value_=value;step_=std::max(1e-9,std::abs(step));span_=limited?std::max(step_,high-low):std::max({std::abs(high-low),step_*100,std::abs(value)*.5});
    const double center=value<low||value>high?value:(low+high)*.5;
    setValue(qRound(std::clamp((value-center)/span_+.5,0.0,1.0)*1000));
  }
};
}
