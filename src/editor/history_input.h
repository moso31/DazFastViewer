#pragma once
#include "editor/edit_history.h"
#include "editor/numeric_slider.h"
#include <QApplication>
#include <QAbstractSpinBox>
#include <QKeyEvent>
#include <QPointer>
#include <QTimer>

namespace dfv::editor {
// 只给场景数值控件划分交互边界；搜索、导航和设置面板不会产生历史。
class HistoryInput final:public QObject {
  EditHistory &history_;
  QPointer<QWidget> root_,control_;
  QTimer timer_;
  bool held_=false;
public:
  HistoryInput(EditHistory &history,QWidget *root):QObject(root),history_(history),root_(root){
    timer_.setSingleShot(true);timer_.setInterval(400);connect(&timer_,&QTimer::timeout,this,[this]{finish();});qApp->installEventFilter(this);
  }
  void finish(){timer_.stop();held_=false;control_=nullptr;history_.finish_gesture();}
  bool eventFilter(QObject *object,QEvent *event) override {
    auto *widget=qobject_cast<QWidget *>(object);if(!widget||!root_||(!root_->isAncestorOf(widget)&&widget!=root_))return false;
    if(event->type()==QEvent::KeyPress){auto *key=static_cast<QKeyEvent *>(event);if(key->key()==Qt::Key_Shift||key->key()==Qt::Key_Control||key->key()==Qt::Key_Alt||key->key()==Qt::Key_Meta)return false;if(key->key()==Qt::Key_Escape&&history_.gesturing()){timer_.stop();held_=false;control_=nullptr;QPointer<NumericSlider> slider=dynamic_cast<NumericSlider *>(widget);const bool cancelled=history_.cancel_gesture();if(slider)slider->cancel_drag();return cancelled;}}
    QWidget *control=widget;while(control&&control!=root_&&!control->property("historyInput").toBool())control=control->parentWidget();
    if(!control||control==root_){if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::KeyPress)finish();return false;}
    auto begin=[&]{if(control_!=control){finish();control_=control;}history_.begin_gesture(quintptr(control));};
    auto step_key=[&](int key){return key==Qt::Key_Up||key==Qt::Key_Down||key==Qt::Key_PageUp||key==Qt::Key_PageDown||(dynamic_cast<NumericSlider *>(control)&&(key==Qt::Key_Left||key==Qt::Key_Right));};
    switch(event->type()){
      case QEvent::Wheel:begin();timer_.start();break;
      case QEvent::MouseButtonPress:begin();held_=true;timer_.stop();break;
      case QEvent::MouseButtonRelease:if(control_==control){held_=false;QTimer::singleShot(0,this,[this]{if(!held_)finish();});}break;
      case QEvent::KeyPress:{const auto key=static_cast<QKeyEvent *>(event)->key();if(step_key(key)){begin();held_=true;timer_.stop();}else finish();break;}
      case QEvent::KeyRelease:if(held_&&step_key(static_cast<QKeyEvent *>(event)->key())&&!static_cast<QKeyEvent *>(event)->isAutoRepeat()){held_=false;QTimer::singleShot(0,this,[this]{if(!held_)finish();});}break;
      case QEvent::FocusOut:QTimer::singleShot(0,this,[this]{finish();});break;
      default:break;
    }return false;
  }
};
}
