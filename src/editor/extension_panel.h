#pragma once
#include "runtime/growth.h"
#include "runtime/weight.h"
#include "editor/growth_curve_dialog.h"
#include <QWidget>
#include <functional>
class QToolButton;
class QDoubleSpinBox;
class QLabel;
class QCheckBox;
namespace dfv::editor {
class ExtensionPanel final:public QWidget {
  QToolButton *header_,*details_,*round_;
  QWidget *body_,*growth_,*density_row_,*baby_details_;
  QDoubleSpinBox *age_,*step_,*sense_,*strength_,*fatness_,*density_,*multiplier_;
  QDoubleSpinBox *baby_begin_,*baby_end_,*baby_strength_;
  QCheckBox *baby_enabled_,*baby_blend_,*curve_enabled_;
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
  std::function<void(bool)> multiply_scale;
  std::function<GrowthCurvePreview()> prepare_curve_preview;
  std::function<void(runtime::ObjectExtension,const GrowthCurveSettings &)> curve_edited;
  void bind(const runtime::ObjectExtension &value,bool new_selection=false);
  void expand();
  void pending();
  void result(const runtime::WeightResult &value,const std::string &error={});
  void measurement_scale(const std::string &value);
};
bool edit_measurement_scale(QWidget *parent,std::string &scale);
}
