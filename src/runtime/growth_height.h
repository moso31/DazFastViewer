#pragma once
#include "runtime/growth_curve.h"

namespace dfv::runtime {
// 参考身高由实际角色在 Scale=100、各年龄对应 Morph 下测得。
inline double growth_curve_height(const GrowthCurve &c,std::span<const CurveKnot> heights,double age){
  const double scale=growth_curve_scale(c,age);
  return curve_value(heights,std::min(age,c.planned_age))*scale/100;
}
inline double growth_curve_height_rate(const GrowthCurve &c,std::span<const CurveKnot> heights,double age){
  const double rate=growth_curve_rate(c,age);if(age>=c.planned_age)return 0;
  return (curve_derivative(heights,age)*growth_curve_scale(c,age)+curve_value(heights,age)*rate)/100;
}
}
