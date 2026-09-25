#pragma once
#include "runtime/growth.h"
#include "runtime/weight.h"
#include <QWidget>
#include <functional>
class QToolButton;
class QDoubleSpinBox;
class QLabel;
namespace dfv::editor {
class ExtensionPanel final:public QWidget {
  QToolButton *header_,*details_;
  QWidget *body_,*growth_,*density_row_;
  QDoubleSpinBox *age_,*step_,*sense_,*strength_,*density_;
  QLabel *summary_,*status_,*parts_;
  QWidget *parts_scroll_;
  runtime::ObjectExtension value_;
  runtime::WeightResult measured_;
  std::string measurement_scale_="1",error_;
  bool has_result_=false;
  void render_result();
public:
  explicit ExtensionPanel(QWidget *parent=nullptr);
  std::function<void(runtime::ObjectExtension,bool,double)> changed;
  void bind(const runtime::ObjectExtension &value,bool new_selection=false);
  void expand();
  void pending();
  void result(const runtime::WeightResult &value,const std::string &error={});
  void measurement_scale(const std::string &value);
};
bool edit_measurement_scale(QWidget *parent,std::string &scale);
}
