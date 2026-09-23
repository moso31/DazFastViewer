#pragma once
#include "render_ir/options.h"
#include <numbers>

namespace dfv::ir {
// 低阶太阳历：儒略日 + 当地秒数/UTC 时差。坐标为东 X、北 Y、天顶 Z。
// 与 NOAA 的赤纬/时角定义一致；不包括地形遮挡和近地平线大气折射。
inline Vec3 solar_direction(const OptionNode &n) {
  constexpr double rad=std::numbers::pi/180;
  const double jd=number(n,"SS Day",2457092)-.5+(number(n,"SS Time",43200)-3600*number(n,"SS UTC Offset",0))/86400;
  const double days=jd-2451545;
  const double anomaly=(357.528+.9856003*days)*rad;
  const double longitude=(280.460+.9856474*days+1.915*std::sin(anomaly)+.020*std::sin(2*anomaly))*rad;
  const double obliquity=(23.439-.0000004*days)*rad;
  const double ascension=std::atan2(std::cos(obliquity)*std::sin(longitude),std::cos(longitude));
  const double declination=std::asin(std::sin(obliquity)*std::sin(longitude));
  const double hour=(280.46061837+360.98564736629*days+number(n,"SS Longitude",0))*rad-ascension;
  const double latitude=std::clamp(number(n,"SS Latitude",0),-90.0,90.0)*rad;
  Vec3 direction{float(-std::cos(declination)*std::sin(hour)),
    float(std::sin(declination)*std::cos(latitude)-std::cos(declination)*std::cos(hour)*std::sin(latitude)),
    float(std::sin(declination)*std::sin(latitude)+std::cos(declination)*std::cos(hour)*std::cos(latitude))};
  // Dome Rotation 沿 DAZ 的竖轴转动太阳方位。
  const double rotation=number(n,"Dome Rotation",0)*rad,c=std::cos(rotation),s=std::sin(rotation);
  return {float(c*direction.x-s*direction.y),float(s*direction.x+c*direction.y),direction.z};
}
}
