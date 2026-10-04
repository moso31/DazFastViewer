#pragma once
#include "city/runtime.h"
#include <QWidget>
class QComboBox;class QLineEdit;class QSpinBox;class QDoubleSpinBox;class QListWidget;class QCheckBox;class QLabel;class QPushButton;
namespace dfv::city {
class Panel final:public QWidget {
  QComboBox *cities_=nullptr,*lod_=nullptr;
  QLineEdit *directory_=nullptr;
  QSpinBox *seed_=nullptr,*x_=nullptr,*y_=nullptr,*lots_=nullptr;
  QDoubleSpinBox *size_=nullptr,*road_=nullptr,*density_=nullptr,*origin_[3]{},*distances_[3]{};
  QListWidget *assets_=nullptr;
  QCheckBox *lock_=nullptr,*enabled_=nullptr;
  QLabel *message_=nullptr,*stats_=nullptr;
  QPushButton *generate_=nullptr,*remove_=nullptr,*cancel_=nullptr;
  Cities source_;Views views_;bool updating_=false,busy_=false;
  void read_selection();void scan(const std::vector<std::string> &selected={});void change_view();
public:
  explicit Panel(const std::filesystem::path &,QWidget *parent=nullptr);
  std::function<void(Config,std::string)> generate;
  std::function<void(std::string)> remove,focus;
  std::function<void(std::string,View)> view_changed;
  std::function<void()> cancel;
  void bind(const Cities &,const Views &);
  void busy(bool);void message(const QString &);void status(const Stats &);
};
}
