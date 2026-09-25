#pragma once
#include "editor/gizmo.h"
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QDoubleSpinBox>
#include <functional>

namespace dfv::editor {
// 应用标识和系统按钮固定，菜单／工具由独立 QToolBar 承载，可移动、浮动、隐藏。
class EditorChrome final:public QWidget {
  QMainWindow *owner_,*modules_;
  QToolBar *menus_,*tools_,*functions_;
  QMenuBar *menu_;
  QWidget *grip_;
  QAction *local_,*world_,*ground_;
  QDoubleSpinBox *ground_ratio_;
  GizmoSettings settings_;
  GizmoSpace translation_space_=GizmoSpace::local,rotation_space_=GizmoSpace::local;
  QByteArray defaults_;
  void fit_height();
  void emit_settings();
protected:
  bool eventFilter(QObject *,QEvent *) override;
public:
  explicit EditorChrome(QMainWindow *owner);
  std::function<void(GizmoSettings)> changed;
  std::function<void(double)> ground_ratio_changed;
  QAction *ground_action() const {return ground_;}
  void bind_ground(bool enabled,double ratio);
  QMenuBar *menus() const {return menu_;}
  GizmoSettings settings() const {return settings_;}
  bool caption_at(QPoint position) const;
  QByteArray save_modules() const;
  bool restore_modules(const QByteArray &state);
  void reset_modules();
  void add_layout_actions(QMenu *menu);
};
class EditorWindow:public QMainWindow {
protected:
  bool nativeEvent(const QByteArray &,void *,qintptr *) override;
public:
  EditorChrome *chrome=nullptr;
  void install_chrome();
};
}
