#pragma once
#include "runtime/growth.h"

// 预览与正式版共用同一算法；0.5 / 0.5 精确复用旧表中档。
namespace dfv::preview {
inline std::vector<runtime::GrowthValue> composition_values(double age,double frame,double fat) {
  return runtime::growth_values(age,frame,fat);
}
}
