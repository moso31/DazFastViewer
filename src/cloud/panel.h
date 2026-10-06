#pragma once
#include "cloud/model.h"
#include <QWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
#include <functional>
#include <map>
namespace dfv::cloud {
class Panel final:public QWidget {
  Config config_;
  std::map<std::string,QDoubleSpinBox *> fields_;
  QCheckBox *collisions_;
  QTreeWidget *sources_;
  QPushButton *sort_;
  QLabel *message_;
  QString selected_;
  std::string bound_id_;
  bool binding_=false;
  bool have_bounds_=false;
  void select(const QString &);
  void submit(const std::string &field={});
  bool eventFilter(QObject *,QEvent *) override;
public:
  explicit Panel(QWidget *parent=nullptr);
  void bind(const Cloud *,const std::vector<Candidate> &);
  std::function<void(Config)> changed;
  std::function<void()> refresh_candidates;
};
}
