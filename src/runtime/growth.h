#pragma once
#include "runtime/growth_curve.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dfv::runtime {
enum class ExtensionKind {none,growth,density};
struct ObjectExtension {
  ExtensionKind kind=ExtensionKind::none;
  double age=3,age_step=1,sensitivity=5,strength=.5,fatness=.5,density=1000;
  double scale_multiplier=1;
  double baby_begin=0,baby_end=7,baby_strength=.6;
  bool baby_enabled=false,baby_blend=true;
  // 首次启用婴儿期后隔离 Baby 的根缩放 ERC；关闭后保留隔离，避免重新触发负缩放。
  bool baby_scale_isolated=false;
  bool scale_round=false;
  int scale_round_exponent=1;
  GrowthCurve curve;
  bool operator==(const ObjectExtension &) const = default;
};
void validate_extension(const ObjectExtension &value);
bool legacy_extension_channel(const std::string &name);
struct GrowthValue {std::string label;double value;};
// 原单轴表保留用于旧规则对照；正式生长使用带肥胖参数的重载。
std::vector<GrowthValue> growth_values(double age,double strength);
std::vector<GrowthValue> growth_values(double age,double strength,double fatness);
double growth_baby_weight(const ObjectExtension &value);
struct Target;
struct Properties;
struct FormulaGraph;
class FormulaRuntime;
std::vector<uint32_t> growth_baby_scale_formulas(const Target &target,const FormulaGraph &graph);
void configure_growth_formulas(FormulaRuntime &runtime,const std::vector<uint32_t> &formulas,const ObjectExtension &value);
// 一次写入全部年龄形变，返回未提供或不可求值的参数名。
std::vector<std::string> apply_growth(const Target &target,Properties &values);
}
