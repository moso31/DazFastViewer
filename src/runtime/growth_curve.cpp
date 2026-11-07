#include "runtime/growth_curve.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dfv::runtime {
namespace {
auto knots(const GrowthCurve &c){std::array<CurveKnot,8> out;for(size_t i=0;i<out.size();++i)out[i]={c.shape.points[i].age,c.shape.points[i].rate*growth_stage_weight(c,i)};return out;}
double integral(const GrowthCurve &c,double age){return curve_integral(knots(c),age);}
double bounded_age(const GrowthCurve &c,double age){
  if(!std::isfinite(age)||age<0)throw std::runtime_error("年龄必须是非负有限数值");
  return std::min(age,c.planned_age);
}
}
GrowthCurve::GrowthCurve(){fit_growth_curve(*this);}
double growth_stage_weight(const GrowthCurve &c,size_t i){return i==5||i==6?c.puberty_weight:1.;}
void validate_growth_curve(const GrowthCurve &c){
  if(!std::isfinite(c.birth_scale)||c.birth_scale<=0||!std::isfinite(c.adult_scale)||c.adult_scale<=0||
     !std::isfinite(c.minimum_growth)||c.minimum_growth<0||c.shape.reference_template<0||c.shape.reference_template>GrowthCurveShape::custom_template||
     !std::isfinite(c.planned_age)||c.planned_age<=0||!std::isfinite(c.growth_factor)||c.growth_factor<0||
     !std::isfinite(c.puberty_weight)||c.puberty_weight<0||
     (c.minimum_mode!=GrowthMinimumMode::fit&&c.minimum_mode!=GrowthMinimumMode::additive))
    throw std::runtime_error("生长曲线需要正数 Scale 和年龄边界、非负增长与青春期权重，以及有效模板");
  if(c.shape.points.front().age!=0||c.shape.points.back().rate!=0)
    throw std::runtime_error("生长曲线形状必须从 0 开始，最后阶段的额外增长为 0");
  for(size_t i=0;i<c.shape.points.size();++i){const auto &p=c.shape.points[i];
    if(!std::isfinite(p.age)||!std::isfinite(p.rate)||p.rate<0||(i&&p.age-c.shape.points[i-1].age<.001)||!std::isfinite(p.rate*growth_stage_weight(c,i)))
      throw std::runtime_error("阶段年龄必须依次增加至少 0.001，阶段额外增长必须是非负有限数值");
  }
  if(c.shape.points[5].rate<c.shape.points[4].rate||c.shape.points[5].rate<c.shape.points[6].rate)
    throw std::runtime_error("青春期峰值的基础增长不能低于青春期起点或减速阶段");
  const double end=c.birth_scale+c.planned_age*c.minimum_growth+c.growth_factor*integral(c,c.planned_age);
  if(!std::isfinite(end)||end<=0)throw std::runtime_error("生长曲线 Scale 超出有效数值范围");
}
void fit_growth_curve(GrowthCurve &curve){
  auto c=curve;
  if(!std::isfinite(c.birth_scale)||c.birth_scale<=0||!std::isfinite(c.adult_scale)||c.adult_scale<c.birth_scale||
     !std::isfinite(c.minimum_growth)||c.minimum_growth<0||c.shape.reference_template<0||c.shape.reference_template>GrowthCurveShape::custom_template||
     !std::isfinite(c.planned_age)||c.planned_age<=0||!std::isfinite(c.puberty_weight)||c.puberty_weight<0)
    throw std::runtime_error("拟合需要正数出生 Scale 和成年边界，成年 Scale 不低于出生 Scale，最低增长与青春期权重不小于 0");
  double budget=c.adult_scale-c.birth_scale;
  if(c.minimum_mode==GrowthMinimumMode::fit)budget-=c.planned_age*c.minimum_growth;
  if(budget<0)throw std::runtime_error("拟合无解：成年边界 × 最低增长超过成年与出生 Scale 的差值。请减小最低增长或增大成年 Scale");
  validate_growth_curve(c);const double area=integral(c,c.planned_age);
  if(area==0&&budget>0)throw std::runtime_error("拟合无解：当前曲线形状没有额外增长，无法分配端点差值。请选择有增长的参考模板");
  c.growth_factor=area>0?budget/area:0;
  validate_growth_curve(c);curve=std::move(c);
}
void apply_growth_template(GrowthCurve &curve,int reference){
  if(reference<0||reference>2)throw std::runtime_error("请选择有效的参考模板");
  auto c=curve;const double peak=reference==1?11.7:reference==2?13.9:12.5;
  const double peak_rate=reference==1?7.9:reference==2?9.3:8.6;
  c.shape.reference_template=reference;c.planned_age=18;
  c.shape.points={{{0,24},{1,12},{2,8},{5,5.5},{8,4.5},{peak,peak_rate},{reference==0?15:(peak+18)/2,3},{18,0}}};
  validate_growth_curve(c);curve=std::move(c);
}
void mark_growth_shape_custom(GrowthCurve &c){c.shape.reference_template=GrowthCurveShape::custom_template;}

void set_growth_stage_age(GrowthCurve &curve,size_t stage,double age){
  if(stage==0||stage>=curve.shape.points.size())throw std::runtime_error("请选择可编辑的年龄阶段");
  auto c=curve;c.shape.points[stage].age=age;mark_growth_shape_custom(c);validate_growth_curve(c);curve=std::move(c);
}
void move_growth_peak(GrowthCurve &curve,double age){set_growth_stage_age(curve,5,age);}
void resize_growth_curve(GrowthCurve &curve,double age){
  if(!std::isfinite(age)||age<=0)throw std::runtime_error("规划到年龄必须是正数");
  auto c=curve;c.planned_age=age;validate_growth_curve(c);curve=std::move(c);
}
void migrate_growth_curve(GrowthCurve &c){
  validate_growth_curve(c);double old_area=0;
  for(size_t i=1;i<c.shape.points.size();++i)old_area+=(c.shape.points[i].age-c.shape.points[i-1].age)*(c.shape.points[i].rate+c.shape.points[i-1].rate)/2;
  const double new_area=integral(c,c.planned_age);if(new_area>0)for(auto &p:c.shape.points)p.rate*=old_area/new_area;
  validate_growth_curve(c);
}
bool same_growth_scale(const GrowthCurve &a,const GrowthCurve &b){
  return a.enabled==b.enabled&&a.birth_scale==b.birth_scale&&a.minimum_growth==b.minimum_growth&&a.planned_age==b.planned_age&&a.puberty_weight==b.puberty_weight&&a.growth_factor==b.growth_factor&&a.shape.points==b.shape.points;
}
double growth_curve_scale(const GrowthCurve &c,double age){
  validate_growth_curve(c);const double t=bounded_age(c,age);return c.birth_scale+t*c.minimum_growth+c.growth_factor*integral(c,t);
}
double growth_curve_rate(const GrowthCurve &c,double age){
  validate_growth_curve(c);const double t=bounded_age(c,age);if(t>=c.planned_age)return 0;
  return c.minimum_growth+c.growth_factor*curve_value(knots(c),t);
}
}
