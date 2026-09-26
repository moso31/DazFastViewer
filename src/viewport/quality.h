#pragma once
#include <algorithm>
#include <utility>

namespace dfv {
enum class Reconstruction {bilinear,bicubic};
struct ViewportQuality {
  int percent=100;
  Reconstruction reconstruction=Reconstruction::bicubic;
  bool operator==(const ViewportQuality &) const=default;
};
inline std::pair<int,int> render_size(int width,int height,int percent,bool moving=false) {
  const double factor=std::clamp(percent,50,100)/100.;const int divisor=moving?4:1;
  return {std::max(1,int(width*factor+.5)/divisor),std::max(1,int(height*factor+.5)/divisor)};
}
}
