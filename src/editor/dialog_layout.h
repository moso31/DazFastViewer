#pragma once
#include "editor/window_placement.h"
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QScreen>
#include <QSettings>
#include <memory>

namespace dfv::editor {
// 对话框的尺寸、所在屏幕和最大化状态属于界面偏好，取消操作也保存布局。
class DialogLayouts final:public QObject {
  QString settings_file_;
  bool applying_=false;
  QString key(QWidget *dialog) const {
    return "dialogs/"+(dialog->objectName().isEmpty()?QString::fromLatin1(dialog->metaObject()->className())+"/"+dialog->windowTitle():dialog->objectName());
  }
  QSettings *settings() const {return settings_file_.isEmpty()?new QSettings:new QSettings(settings_file_,QSettings::IniFormat);}
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    auto *dialog=qobject_cast<QWidget *>(object);if(!dialog||!dialog->isWindow()||applying_)return false;
    // 浮动停靠窗与工具栏由所属主窗口整体恢复，不能在 Show 中再次覆盖它们。
    if(!qobject_cast<QDialog *>(dialog))return false;
    if(event->type()==QEvent::Show&&!dialog->property("dfvLayoutRestored").toBool()) {
      applying_=true;dialog->setProperty("dfvLayoutRestored",true);std::unique_ptr<QSettings> saved(settings());const auto prefix=key(dialog);
      dialog->restoreGeometry(saved->value(prefix+"/geometry").toByteArray());
      const auto state=Qt::WindowStates(saved->value(prefix+"/state",0).toInt())&~Qt::WindowMinimized;
      if(state.testFlag(Qt::WindowMaximized))dialog->setWindowState(state);
      else if(!dialog->isMaximized()&&!dialog->isFullScreen()) {
        QList<QRect> areas;for(auto *screen:QGuiApplication::screens())areas.push_back(screen->availableGeometry());
        if(auto *screen=dialog->screen()){const auto bounds=visible_window_geometry(dialog->geometry(),areas,screen->availableGeometry());dialog->setGeometry(bounds);}
      }
      applying_=false;
    } else if(event->type()==QEvent::Hide&&dialog->property("dfvLayoutRestored").toBool()) {
      std::unique_ptr<QSettings> saved(settings());const auto prefix=key(dialog);saved->setValue(prefix+"/geometry",dialog->saveGeometry());saved->setValue(prefix+"/state",int(dialog->windowState()&~Qt::WindowMinimized));saved->sync();dialog->setProperty("dfvLayoutRestored",false);
    }
    return false;
  }
public:
  explicit DialogLayouts(const QString &settings_file={},QObject *parent=nullptr):QObject(parent?parent:qApp),settings_file_(settings_file){qApp->installEventFilter(this);}
};
}
