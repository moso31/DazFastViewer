#pragma once
#include "render_ir/options.h"
#include <array>
#include <memory>

namespace dfv::ir {
inline bool night_option(const std::string &id) {return id.starts_with("DFV Night ");}
void ensure_night_options(OptionNode &node);
inline bool night_enabled(const OptionNode &node) {return number(node,"DFV Night Enabled",0)!=0&&int(number(node,"Environment Mode",0))!=3;}

// 东 X、北 Y、天顶 Z，与现有太阳计算一致。J2000 星表另作岁差旋转。
struct NightAstronomy {
  double julian_day=0,sidereal=0,latitude=0,rotation=0;
  Vec3 moon{},sun{};
  double moon_distance=60,moon_radius=0,illuminated=0,darkness=0;
  Vec3 equatorial_to_world(Vec3 direction,bool j2000=true) const;
  Vec3 world_to_equatorial(Vec3 direction,bool j2000=true) const;
};
NightAstronomy night_astronomy(const OptionNode &node);

struct NightMap {
  int width=0,height=0;
  std::vector<float> pixels; // Cycles 的 bottom-up 经纬 RGBA；不作颜色空间转换。
};
struct NightAssets {
  // 两个最近恒星的查询表：RGB 是单位方向，A 打包星等和 B-V。不是星点图片。
  NightMap stars,stars_secondary;
  size_t resolved_stars=0;
  double prepare_ms=0;
};
inline constexpr float night_star_sigma_base=.00033f,night_star_sigma_gain=.00016f;
// 仅质量档决定星位查询表；日期、密度、亮度等日常调参绝不重建查询表。
std::shared_ptr<const NightAssets> night_assets(int quality);
std::array<Vec3,9> night_star_sh(double limiting_magnitude);
size_t night_star_count(double limiting_magnitude);
std::array<double,9> night_sh_basis(Vec3 direction);
Vec3 night_map_direction(double u,double v);
Vec3 sample_night_stars(const NightAssets &assets,Vec3 direction,double limiting_magnitude);
}
