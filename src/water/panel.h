#pragma once
#include "water/model.h"
#include <QWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QComboBox>
#include <map>
namespace dfv::water {
class Panel final:public QWidget {
  Config config_;
  std::map<std::string,QDoubleSpinBox *> fields_;
  QCheckBox *coast_,*scan_;
  QTreeWidget *sources_;
  QLabel *message_;
  QPushButton *calculate_,*cancel_,*color_;
  bool binding_=false,busy_=false;
  std::string bound_id_;
  QString selected_;
  void select(const QString &);
  void expand_sources();
  bool eventFilter(QObject *,QEvent *) override;
  void submit();
public:
  explicit Panel(QWidget *parent=nullptr);
  void bind(const Water *,const std::vector<Candidate> &objects);
  void busy(bool);
  void message(const QString &s){message_->setText(s);}
  std::function<void(Config,bool)> changed;
  std::function<void()> cancel;
};
}
