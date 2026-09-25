#pragma once
#include <string>
namespace dfv::runtime {
// 全局测量倍率以十进制字符串保存，不经过 double；不改变场景几何。
std::string measurement_scale(const std::string &value);
std::string measurement_height(double centimeters,const std::string &scale="1");
std::string measurement_mass(double kilograms,const std::string &scale="1");
}
