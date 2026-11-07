#pragma once
#include <array>
#include "runtime/curve_interpolation.h"

namespace dfv::runtime {
struct GrowthCurvePoint {
  double age=0,rate=0;
  bool operator==(const GrowthCurvePoint &) const = default;
};
enum class GrowthMinimumMode { fit, additive };
struct GrowthCurveShape {
  static constexpr int custom_template=3;
  int reference_template=0;
  // 阶段形状独立于拟合倍率；模板只负责提供这一组基础参数。
  std::array<GrowthCurvePoint,8> points{{{0,24},{1,12},{2,8},{5,5.5},{8,4.5},{12.5,8.6},{15,3},{18,0}}};
  bool operator==(const GrowthCurveShape &) const = default;
};
struct GrowthCurve {
  bool enabled=false;
  double birth_scale=30,adult_scale=100,minimum_growth=0;
  double planned_age=18,puberty_weight=1,growth_factor=1;
  bool height_in_cm=false,growth_in_cm=false;
  GrowthMinimumMode minimum_mode=GrowthMinimumMode::fit;
  GrowthCurveShape shape;
  GrowthCurve();
  bool operator==(const GrowthCurve &) const = default;
};
void validate_growth_curve(const GrowthCurve &curve);
void fit_growth_curve(GrowthCurve &curve);
void apply_growth_template(GrowthCurve &curve,int reference_template);
void mark_growth_shape_custom(GrowthCurve &curve);
void set_growth_stage_age(GrowthCurve &curve,size_t stage,double age);
void move_growth_peak(GrowthCurve &curve,double age);
void resize_growth_curve(GrowthCurve &curve,double adult_age);
void migrate_growth_curve(GrowthCurve &curve);
bool same_growth_scale(const GrowthCurve &a,const GrowthCurve &b);
double growth_stage_weight(const GrowthCurve &curve,size_t stage);
double growth_curve_scale(const GrowthCurve &curve,double age);
double growth_curve_rate(const GrowthCurve &curve,double age);
}
