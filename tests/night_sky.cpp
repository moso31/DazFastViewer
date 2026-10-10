#include "render_ir/night_sky.h"
#include "render_ir/default_options.h"
#include "render_ir/options_json.h"
#include "render_ir/sun_sky.h"
#include <chrono>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace dfv;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
static void set(ir::OptionNode &n,const char *id,double v){for(size_t i=0;i<n.parameters.size();++i)if(n.parameters[i].id==id){ir::set_option(n,i,0,v);return;}throw std::runtime_error(id);}
static double distance(ir::Vec3 a,ir::Vec3 b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);}
int main(){try{
  auto n=ir::default_options(true);check(!ir::night_enabled(n),"新场景默认开启了夜景");
  check(ir::number(n,"DFV Night Star Distribution",-1)==0,"默认没有使用程序化星空");
  check(ir::number(n,"DFV Night Moon Scale",0)==6,"月亮默认大小不是 6");
  check(ir::option(n,"SS UTC Offset")->step==.5,"UTC 默认精度不是 0.5");
  {auto imported=n;for(auto &p:imported.parameters)if(p.id=="SS UTC Offset"){p.type="int";p.step=1;p.visible=false;}ir::ensure_night_options(imported);set(imported,"SS UTC Offset",5.5);check(ir::number(imported,"SS UTC Offset",0)==5.5&&ir::option(imported,"SS UTC Offset")->visible&&ir::option(imported,"SS UTC Offset")->step==.5,"旧时区通道未支持半小时或仍被隐藏");}
  for(const auto &[lon,offset]:std::array<std::pair<double,double>,7>{{{0,0},{120,8},{-111.8972,-7.5},{151,10},{82.5,5.5},{180,12},{-180,-12}}})check(ir::longitude_utc_offset(lon)==offset,"地图经度对应时区错误");
  check(ir::number(n,"DFV Night Moon Lighting Gain",0)==1,"月光照明增益默认改变了原光量");
  check(!ir::option(n,"DFV Night Lighting")&&!ir::option(n,"DFV Night Quality"),"已移除的档位仍出现在参数中");
  const auto count=n.parameters.size();ir::ensure_night_options(n);check(count==n.parameters.size(),"夜景选项重复追加");
  auto legacy=n;ir::Option removed;removed.id="DFV Night Quality";removed.value={0};legacy.parameters.push_back(removed);removed.id="DFV Night Lighting";legacy.parameters.push_back(removed);set(legacy,"DFV Night Moon Scale",4);
  ir::ensure_night_options(legacy);check(!ir::option(legacy,"DFV Night Quality")&&!ir::option(legacy,"DFV Night Lighting")&&ir::number(legacy,"DFV Night Moon Scale",0)==4,"旧档位未移除或覆盖了已保存的月亮大小");
  set(n,"DFV Night Enabled",1);set(n,"DFV Night Moon Intensity",3);set(n,"DFV Night Stars Intensity",2);set(n,"DFV Night Limiting Magnitude",4.2);
  ir::RenderOptions o;o.environment=n;ir::ensure_matte_fog_options(o.environment);check(ir::options_from_json(ir::options_json(o))==o,"夜景设置未完整往返");
  auto unbounded=o;set(unbounded.environment,"SS UTC Offset",-7.5);set(unbounded.environment,"DFV Night Moon Intensity",500);set(unbounded.environment,"DFV Night Moon Lighting Gain",250);set(unbounded.environment,"DFV Night Milky Way Detail",5);set(unbounded.environment,"DFV Night Rotation",1080);set(unbounded.environment,"DFV Night Moon Scale",40);
  ir::ensure_night_options(unbounded.environment);const auto restored=ir::options_from_json(ir::options_json(unbounded));check(restored==unbounded&&ir::number(restored.environment,"DFV Night Moon Intensity",0)==500&&ir::number(restored.environment,"DFV Night Moon Scale",0)==40,"夜景建议范围在写入或加载时被重复强制限制");
  auto old=ir::options_json(o);auto &parameters=old["environment"]["parameters"];for(auto it=parameters.begin();it!=parameters.end();)if(it->at("id").get<std::string>().starts_with("DFV Night "))it=parameters.erase(it);else ++it;check(!ir::night_enabled(ir::options_from_json(old).environment),"旧文件迁移开启了夜景");
  set(n,"Environment Mode",3);check(!ir::night_enabled(n),"Scene Only 仍启用天空");set(n,"Environment Mode",2);
  set(n,"SS Day",2448001);set(n,"SS Time",0);set(n,"SS UTC Offset",0);set(n,"SS Latitude",60);set(n,"SS Longitude",15);
  const auto a=ir::night_astronomy(n);const auto eq=a.world_to_equatorial(a.moon,false);
  const double ra=std::fmod(std::atan2(eq.y,eq.x)*180/std::numbers::pi+360,360),dec=std::asin(eq.z)*180/std::numbers::pi;
  // Schlyter 1990-04-19 00:00 UTC，60N/15E 的公开地表观测示例。
  check(std::abs(ra-310.0017)<.025&&std::abs(dec+19.8790)<.025,"月球摄动/观测视差与参考星历不符");
  for(auto v:{ir::Vec3{1,0,0},ir::Vec3{0,1,0},ir::Vec3{0,0,1}})check(distance(a.world_to_equatorial(a.equatorial_to_world(v)),v)<1e-6,"J2000/世界变换不正交");
  set(n,"SS Time",8*3600);set(n,"SS UTC Offset",8);check(distance(ir::night_astronomy(n).moon,a.moon)<1e-6,"时区没有还原同一 UTC 时刻");
  set(n,"SS Time",5.5*3600);set(n,"SS UTC Offset",5.5);check(distance(ir::night_astronomy(n).moon,a.moon)<1e-6,"半小时时区改变了同一 UTC 时刻的月亮");
  set(n,"SS Day",2448000);set(n,"SS Time",22.5*3600);set(n,"SS UTC Offset",-1.5);check(distance(ir::night_astronomy(n).moon,a.moon)<1e-6,"时区跨日改变了月亮位置");set(n,"SS Day",2448001);set(n,"SS Time",8*3600);set(n,"SS UTC Offset",8);
  set(n,"Dome Rotation",90);const auto rotated=ir::night_astronomy(n);check(distance(rotated.moon,{-a.moon.y,a.moon.x,a.moon.z})<1e-6,"穹顶旋转未同步月亮");
  set(n,"SS Day",2460409);set(n,"SS Time",18*3600);set(n,"SS UTC Offset",0);check(ir::night_astronomy(n).illuminated<.005,"2024-04-08 新月错误");
  set(n,"SS Day",2460335);set(n,"SS Time",18*3600);check(ir::night_astronomy(n).illuminated>.995,"2024-01-25 满月错误");
  set(n,"SS Latitude",90);check(std::isfinite(ir::night_astronomy(n).moon.z),"极区出现非有限值");
  check(ir::night_star_count(2)<ir::night_star_count(4)&&ir::night_star_count(4)<ir::night_star_count(6.5)&&ir::night_star_count(7)==15598,"星等密度没有保留亮星子集");
  auto stars=ir::night_star_sh(7),few=ir::night_star_sh(2);check(stars[0].x>few[0].x&&stars[0].z>few[0].z,"SH 光量不随可见星等变化");
  const auto assets=ir::night_assets(1);check(assets->stars.width==2048&&assets->stars_secondary.width==2048,"质量档尺寸错误");
  double energy=0;
  check(assets->resolved_stars>15598*.995,"查询表遗漏过多恒星");
  // 独立球面积分，与直接由星表累加的 SH 总光量比较，覆盖角距离星核、查询边界和极区。
  constexpr int integration_width=8192,integration_height=4096;
  for(int y=0;y<integration_height;++y)for(int x=0;x<integration_width;++x){const auto d=ir::night_map_direction((x+.5)/integration_width,(y+.5)/integration_height);energy+=ir::sample_night_stars(*assets,d,7).x*std::sin(std::numbers::pi*(y+.5)/integration_height)*2*std::numbers::pi*std::numbers::pi/(integration_width*integration_height);}
  check(std::abs(energy-stars[0].x/.2820947918)<.06*energy,"程序化星核与星表积分光量不符");
  // 改查询表尺寸不能改变已解析亮星的位置、星核大小或亮度。
  const double sr=101.287*std::numbers::pi/180,sd=-16.716*std::numbers::pi/180;
  const ir::Vec3 sirius{float(std::cos(sd)*std::cos(sr)),float(std::cos(sd)*std::sin(sr)),float(std::sin(sd))};
  const auto low=ir::night_assets(0),high=ir::night_assets(2);
  check(distance(ir::sample_night_stars(*low,sirius,7),ir::sample_night_stars(*high,sirius,7))<1e-4,"查询表尺寸改变了天狼星的绘制精度");
  check(low->resolved_stars>15598*.99&&high->resolved_stars>=assets->resolved_stars,"精细度未提高密集恒星的查询覆盖率");
  const auto start=std::chrono::steady_clock::now();for(int i=0;i<100;++i){set(n,"DFV Night Limiting Magnitude",i%7);set(n,"SS Time",i*600);check(ir::night_assets(1)==assets,"调参重新生成了星图");}
  const double cache_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"Night sky PASS; stars="<<ir::night_star_count(7)<<"; resolved_low/medium/high="<<low->resolved_stars<<'/'<<assets->resolved_stars<<'/'<<high->resolved_stars<<"; integrated_energy="<<energy<<"; catalog_energy="<<stars[0].x/.2820947918<<"; prepare_ms="<<assets->prepare_ms<<"; 100 cached edits_ms="<<cache_ms<<"; moon_RA="<<ra<<"; moon_Dec="<<dec<<"\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
