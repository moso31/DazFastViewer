#pragma once
#include <QTimeEdit>

namespace dfv::editor {
// 保留原生 Qt 时间输入和箭头；整行按半小时步进，不依赖当前选中的小时/分钟段。
class SolarTimeEdit final:public QTimeEdit {
public:
  SolarTimeEdit(){setDisplayFormat("HH:mm");setWrapping(true);setKeyboardTracking(false);}
  void stepBy(int steps) override {
    const int minute=time().hour()*60+time().minute();
    const int slot=steps>0?minute/30:(minute+29)/30;
    setTime(QTime(0,0).addSecs(((slot+steps)%48+48)%48*1800));
  }
protected:
  StepEnabled stepEnabled() const override {return StepUpEnabled|StepDownEnabled;}
};
}
