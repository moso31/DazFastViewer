#pragma once
#include "runtime/growth.h"
#include "runtime/parameter_settings.h"
#include "runtime/weight.h"
#include <functional>
class QWidget;
namespace dfv::editor {
using GrowthCurvePreview=std::function<runtime::WeightResult(const runtime::ObjectExtension &,bool force_shape)>;
using GrowthCurveSettings=std::map<std::string,runtime::ParameterSettings>;
using GrowthCurveSettingsReader=std::function<runtime::ParameterSettings(const std::string &,const runtime::ParameterSettings &)>;
bool edit_growth_curve(QWidget *parent,runtime::ObjectExtension &value,const std::string &units,
  GrowthCurvePreview preview,GrowthCurveSettings &settings,GrowthCurveSettingsReader read_settings={},const std::string &title="编辑生长曲线");
int growth_curve_preview();
}
