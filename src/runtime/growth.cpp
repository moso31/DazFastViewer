#include "runtime/growth.h"
#include "runtime/deformation.h"
#include "runtime/growth_tables.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dfv::runtime {
void validate_extension(const ObjectExtension &v) {
  if(v.kind!=ExtensionKind::none&&v.kind!=ExtensionKind::growth&&v.kind!=ExtensionKind::density) throw std::runtime_error("未知对象扩展类型");
  if(!std::isfinite(v.age)||v.age<1||v.age>20||!std::isfinite(v.age_step)||
     !std::isfinite(v.sensitivity)||!std::isfinite(v.strength)||
     !std::isfinite(v.density)||v.density<0||v.density>100000) throw std::runtime_error("生长或密度参数超出范围");
}
bool legacy_extension_channel(const std::string &name) {
  return name=="Age"||name=="Age Step"||name=="Age Sensitivity"||name=="Body Strength"||name=="Density";
}
std::vector<GrowthValue> growth_values(double age,double strength) {
  ObjectExtension v;v.age=age;v.strength=strength;validate_extension(v);
  const double a=age-1;const auto a0=size_t(std::floor(a)),a1=size_t(std::ceil(a));
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
std::vector<std::string> apply_growth(const Target &target,Properties &values) {
  validate_extension(values.extension);std::vector<std::string> missing;
  for(const auto &v:growth_values(values.extension.age,values.extension.strength)) {
    // 商业角色可能附带与原生同名的控制器。优先更新该实例实际引用的参数，
    // 避免向另一份控制器写值，与 DUF 已保存的 ERC 重复叠加。
    auto match=[&](const auto &m){return m.label==v.label&&m.alias_morph<0;};
    auto root=[&](const auto &m){return m.owner.empty()||m.owner.find('/')==std::string::npos;};
    auto found=target.morphs.end();int best=-1;
    for(auto i=target.morphs.begin();i!=target.morphs.end();++i)if(match(*i)) {
      const int rank=(i->scene_channel?4:0)+(root(*i)?2:0);if(rank>best){best=rank;found=i;}
    }
    if(found==target.morphs.end()||!found->evaluable||!found->unsupported.empty()||found->locked) {missing.push_back(v.label);continue;}
    // 脚本写值也服从资产限位；ERC 仍由正常求值链处理。
    auto value=v.value;if(found->clamped) value=std::clamp(value,double(found->minimum),double(found->maximum));
    set_parameter(target,values,size_t(found-target.morphs.begin()),float(value));
  }
  return missing;
}
}
