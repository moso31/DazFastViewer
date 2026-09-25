#pragma once
#include <charconv>
#include <cmath>
#include <limits>
namespace dfv::runtime {
// 将底层 float 还原为最短可往返的十进制值，再以 double 参与编辑运算。
inline double decimal_float(double value) {
  char buffer[64];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),float(value));
  if(result.ec!=std::errc{})return value;
  double decimal=value;std::from_chars(buffer,result.ptr,decimal);return decimal;
}
// ERC 与保存缩放相乘后的派生值可能偏移一个 float ULP。
// 仅清理派生基值；用户编辑的 double 倍率不参与此近似。
inline double decimal_derived_float(double value) {
  if(value==0||!std::isfinite(value))return value;
  const double tolerance=std::abs(value)*std::numeric_limits<float>::epsilon();
  for(int precision=1;precision<=std::numeric_limits<float>::max_digits10;++precision){
    char buffer[64];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::general,precision);
    if(r.ec!=std::errc{})break;double candidate=value;std::from_chars(buffer,r.ptr,candidate);
    if(std::abs(candidate-value)<=tolerance)return candidate;
  }return decimal_float(value);
}
}
