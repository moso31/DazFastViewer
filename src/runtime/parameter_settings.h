#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <map>
#include <string>
#include <stdexcept>

namespace dfv::runtime {
// 编辑器数值规则，按稳定对象／通道身份保存；不依赖 Qt 或控件是否已创建。
struct ParameterSettings {
  bool limited=false;
  double minimum=0,maximum=1,step=.1;
  bool operator==(const ParameterSettings &) const=default;
};
using ParameterSettingsState=std::map<std::string,std::map<std::string,ParameterSettings>>;
inline void validate(const ParameterSettings &v) {
  if(!std::isfinite(v.minimum)||!std::isfinite(v.maximum)||v.minimum>v.maximum||
     !std::isfinite(v.step)||v.step<1e-9||v.step>1e12)
    throw std::runtime_error("参数范围或精度无效：最小值不能大于最大值，精度必须为正数");
}
inline void to_json(nlohmann::json &j,const ParameterSettings &v) {
  validate(v);j={{"limited",v.limited},{"min",v.minimum},{"max",v.maximum},{"step",v.step}};
}
inline void from_json(const nlohmann::json &j,ParameterSettings &v) {
  v={j.at("limited").get<bool>(),j.at("min").get<double>(),j.at("max").get<double>(),j.at("step").get<double>()};validate(v);
}
}
