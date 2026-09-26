#pragma once
#include "viewport/quality.h"
#include <QObject>
#include <QString>
#include <functional>
class QMenu;
class QSpinBox;
class QComboBox;
namespace dfv::editor {
class ViewportSettings final:public QObject {
  bool persistent_;
  QString settings_file_;
  ViewportQuality value_;
  QSpinBox *percent_=nullptr;
  QComboBox *reconstruction_=nullptr;
  QMenu *menu_=nullptr;
public:
  explicit ViewportSettings(bool persistent=true,const QString &settings_file={},QObject *parent=nullptr);
  ViewportQuality value() const {return value_;}
  void set(ViewportQuality value);
  void add_menu(QMenu *menu);
  std::function<void(ViewportQuality)> changed;
};
}
