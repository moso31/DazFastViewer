#pragma once
#include "runtime/morph.h"
#include <QWidget>
#include <functional>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QLabel;
class QSlider;
class QDoubleSpinBox;
namespace dfv::editor {
class ParameterPanel final:public QWidget {
  const runtime::Target *target_=nullptr;
  const runtime::Properties *values_=nullptr;
  QTreeWidget *tree_;
  QLineEdit *search_;
  QCheckBox *hidden_;
  QLabel *count_,*details_;
  QSlider *slider_;
  QDoubleSpinBox *spin_;
  int current_=-1;
  std::vector<float> effective_;
  std::vector<QTreeWidgetItem *> items_;
  void rebuild();
  void filter();
  void select(QTreeWidgetItem *item);
public:
  explicit ParameterPanel(QWidget *parent=nullptr);
  std::function<void(size_t,double)> changed;
  void bind(const runtime::Target *target,const runtime::Properties *values);
  void refresh(size_t index);
  void query(const QString &text);
  void select_parameter(size_t index);
  void set_slider(int value);
  void evaluated(const std::vector<float> &values);
};
}
