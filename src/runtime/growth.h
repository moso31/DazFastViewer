#pragma once
#include <array>
#include <string>
#include <vector>

namespace dfv::runtime {
enum class ExtensionKind {none,growth,density};
struct ObjectExtension {
  ExtensionKind kind=ExtensionKind::none;
  double age=3,age_step=1,sensitivity=5,strength=.5,density=1000;
  bool operator==(const ObjectExtension &) const = default;
};
void validate_extension(const ObjectExtension &value);
bool legacy_extension_channel(const std::string &name);
struct GrowthValue {std::string label;double value;};
std::vector<GrowthValue> growth_values(double age,double strength);
struct Target;
struct Properties;
// 一次写入全部年龄形变，返回未提供或不可求值的参数名。
std::vector<std::string> apply_growth(const Target &target,Properties &values);
}
