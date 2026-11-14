#include "runtime/growth.h"
#include "runtime/deformation.h"
#include "runtime/growth_tables.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace dfv::runtime {
void validate_extension(const ObjectExtension &v) {
  if(v.kind!=ExtensionKind::none&&v.kind!=ExtensionKind::growth&&v.kind!=ExtensionKind::density) throw std::runtime_error("未知对象扩展类型");
  if(!std::isfinite(v.age)||v.age<0||!std::isfinite(v.age_step)||
     !std::isfinite(v.baby_begin)||!std::isfinite(v.baby_end)||v.baby_begin<0||v.baby_end>20||v.baby_begin>=v.baby_end||
     !std::isfinite(v.baby_strength)||v.baby_strength<0||v.baby_strength>1||
     !std::isfinite(v.sensitivity)||!std::isfinite(v.strength)||!std::isfinite(v.fatness)||
     !std::isfinite(v.scale_multiplier)||v.scale_multiplier<=0||v.scale_round_exponent < -9||
     !std::isfinite(v.density)||v.density<0||v.density>100000) throw std::runtime_error("生长或密度参数超出范围");
  validate_growth_curve(v.curve);
}
bool legacy_extension_channel(const std::string &name) {
  return name=="Age"||name=="Age Step"||name=="Age Sensitivity"||name=="Body Strength"||name=="Density";
}
std::vector<GrowthValue> growth_values(double age,double strength) {
  ObjectExtension v;v.age=age;v.strength=strength;validate_extension(v);
  // 婴儿形体由 Baby 提供，旧表在 0～1 岁保持第一列，禁止负索引。
  const double a=std::clamp(age,1.0,20.0)-1;const auto a0=size_t(std::floor(a)),a1=size_t(std::ceil(a));
  // 表格外沿最近一段线性延伸，先限制索引再计算权重，负体格也不会越界。
  const auto s0=size_t(std::floor(std::clamp(strength,0.0,.75)*4)),s1=s0+1;
  std::vector<GrowthValue> result;
  for(const auto &table:growth_tables) {
    const auto at=[&](size_t row){return std::lerp(table.values[row][a0],table.values[row][a1],a-a0);};
    const double lo=at(s0)*.01,hi=at(s1)*.01;
    const double value=lo+(strength-double(s0)*.25)*((hi-lo)*4);
    if(!std::isfinite(value))throw std::runtime_error("体格计算结果超出有效数值范围");
    result.push_back({table.label,value});
  }
  return result;
}
std::vector<GrowthValue> growth_values(double age,double strength,double fatness) {
  auto result=growth_values(age,.5);
  const auto skeleton=growth_values(age,strength),soft=growth_values(age,fatness);
  for(size_t i=0;i<result.size();++i) {
    const std::string_view name=result[i].label;
    if(name=="GHD Width"||name=="Foot Propagating Scale"||name=="Hand Propagating Scale"||name=="Foot Length"||name=="All Toes Length")result[i].value=skeleton[i].value;
    else if(name=="Body Size"||name=="Voluptuous"||name=="Glutes Size"||name=="BGM Big Girl Pear Shape")result[i].value=soft[i].value;
    // 原年龄基线叠加胖瘦偏移；Weight 的 ERC 正向驱动 Heavy、负向驱动 Thin。
    // 中性值不做加减，避免快速浮点优化改变旧表的精确值。
    else if(name=="Weight"&&fatness!=.5)result[i].value+=fatness-.5;
    if(!std::isfinite(result[i].value))throw std::runtime_error("体格或肥胖计算结果超出有效数值范围");
  }
  return result;
}
double growth_baby_weight(const ObjectExtension &v) {
  validate_extension(v);
  if(!v.baby_enabled)return 0;
  const double age=v.curve.enabled?std::min(v.age,v.curve.planned_age):v.age;
  return v.baby_strength*(1-std::clamp((age-v.baby_begin)/(v.baby_end-v.baby_begin),0.0,1.0));
}
std::vector<uint32_t> growth_baby_scale_formulas(const Target &target,const FormulaGraph &graph) {
  std::vector<uint32_t> result;
  for(uint32_t f=0;f<graph.expressions.size();++f){const auto &e=graph.expressions[f];
    if(!e.enabled||e.owner>=target.morphs.size())continue;
    const auto &b=graph.channels.at(e.output).binding;
    if(b.property==Property::general_scale&&b.index==0&&target.morphs[e.owner].label=="G8Baby_Scale")result.push_back(f);
  }
  return result;
}
void configure_growth_formulas(FormulaRuntime &runtime,const std::vector<uint32_t> &formulas,const ObjectExtension &value) {
  for(auto f:formulas)runtime.set_enabled(f,!value.baby_scale_isolated);
}
std::vector<std::string> apply_growth(const Target &target,Properties &values) {
  validate_extension(values.extension);std::vector<std::string> missing;
  auto find=[&](const std::string &label){
    // 商业角色可能附带与原生同名的控制器。优先更新该实例实际引用的参数，
    // 避免向另一份控制器写值，与 DUF 已保存的 ERC 重复叠加。
    auto match=[&](const auto &m){return m.label==label&&m.alias_morph<0;};
    auto root=[&](const auto &m){return m.owner.empty()||m.owner.find('/')==std::string::npos;};
    auto found=target.morphs.end();int best=-1;
    for(auto i=target.morphs.begin();i!=target.morphs.end();++i)if(match(*i)) {
      const int rank=(i->scene_channel?4:0)+(root(*i)?2:0);if(rank>best){best=rank;found=i;}
    }
    return found;
  };
  const auto baby=find("Genesis 8 Baby");
  auto usable=[&](const auto &m){return m!=target.morphs.end()&&m->evaluable&&m->unsupported.empty()&&!m->locked;};
  const bool has_baby=usable(baby);const double weight=growth_baby_weight(values.extension);
  auto write=[&](const auto &found,double value){
    // 脚本写值也服从资产限位；ERC 仍由正常求值链处理。
    if(found->clamped) value=std::clamp(value,double(found->minimum),double(found->maximum));
    set_parameter(target,values,size_t(found-target.morphs.begin()),float(value));
  };
  const double age=values.extension.curve.enabled?std::min(values.extension.age,values.extension.curve.planned_age):values.extension.age;
  for(const auto &v:growth_values(age,values.extension.strength,values.extension.fatness)) {
    const auto found=find(v.label);
    if(!usable(found)){missing.push_back(v.label);continue;}
    write(found,v.value*(has_baby&&values.extension.baby_blend?1-weight:1));
  }
  // 未启用过的角色不接管手工 Baby；关闭已启用的衰减时清零其权重并保留缩放隔离。
  if(has_baby&&(values.extension.baby_enabled||values.extension.baby_scale_isolated)){write(baby,weight);values.extension.baby_scale_isolated=true;}
  else if(weight>0)missing.push_back("Genesis 8 Baby");
  return missing;
}
}
