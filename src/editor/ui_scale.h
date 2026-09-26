#pragma once
#include <QApplication>
#include <QObject>
#include <memory>

class QMenu;
namespace dfv::editor {
inline double ui_scale() {const auto value=qApp->property("dfvUiScale");return value.isValid()?value.toDouble():1.;}
inline int ui_pixels(int value) {return qRound(value*ui_scale());}
// 独立于系统 DPI 和渲染分辨率的应用缩放；尺寸始终从基准值计算。
class UiScale final:public QObject {
  struct Impl;
  std::unique_ptr<Impl> impl_;
protected:
  bool eventFilter(QObject *,QEvent *) override;
public:
  explicit UiScale(bool persistent=true,const QString &settings_file={},QObject *parent=nullptr);
  ~UiScale() override;
  void set_percent(int percent);
  int percent() const;
  void add_menu(QMenu *menu);
};
}
