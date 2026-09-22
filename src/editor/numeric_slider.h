#pragma once
#include <QSlider>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>
#include <functional>

namespace dfv::editor {
// 滑轨只是数值标尺。拖动按相对距离累加，越过轨道边缘仍可继续改变值。
class NumericSlider final:public QSlider {
  double value_=0,span_=1,step_=.01,anchor_value_=0;
  double anchor_x_=0,anchor_span_=1;
  bool dragging_=false;
  void commit(double v) {if(std::isfinite(v)&&edited) edited(v);}
protected:
  void mousePressEvent(QMouseEvent *e) override {
    if(e->button()!=Qt::LeftButton) {QSlider::mousePressEvent(e);return;}
    setFocus();dragging_=true;anchor_x_=e->position().x();anchor_value_=value_;anchor_span_=span_;e->accept();
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    if(!dragging_) return;
    const double precision=e->modifiers().testFlag(Qt::ShiftModifier)?.1:1;
    commit(anchor_value_+(e->position().x()-anchor_x_)*anchor_span_/std::max(1,width()-16)*precision);e->accept();
  }
  void mouseReleaseEvent(QMouseEvent *e) override {dragging_=false;e->accept();}
  void wheelEvent(QWheelEvent *e) override {commit(value_+e->angleDelta().y()/120.0*step_);e->accept();}
  void keyPressEvent(QKeyEvent *e) override {
    const int key=e->key();
    if(key==Qt::Key_Left||key==Qt::Key_Down) commit(value_-step_);
    else if(key==Qt::Key_Right||key==Qt::Key_Up) commit(value_+step_);
    else if(key==Qt::Key_PageUp) commit(value_+span_*.1);
    else if(key==Qt::Key_PageDown) commit(value_-span_*.1);
    else {e->ignore();return;}e->accept();
  }
public:
  std::function<void(double)> edited;
  NumericSlider():QSlider(Qt::Horizontal) {setRange(0,1000);}
  void sync(double value,double low,double high,double step) {
    value_=value;step_=std::max(.000001,std::abs(step));span_=std::max({std::abs(high-low),step_*100,std::abs(value)*.5});
    const double center=value<low||value>high?value:(low+high)*.5;
    setValue(qRound(std::clamp((value-center)/span_+.5,0.0,1.0)*1000));
  }
};
}
