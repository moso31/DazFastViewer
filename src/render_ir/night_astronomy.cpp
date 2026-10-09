#include "render_ir/night_sky.h"
#include "render_ir/sun_sky.h"
#include <numbers>

namespace dfv::ir {
namespace {
constexpr double pi=std::numbers::pi,rad=pi/180;
double sind(double x){return std::sin(std::remainder(x,360.)*rad);}
double cosd(double x){return std::cos(std::remainder(x,360.)*rad);}
Vec3 mul(Vec3 p,double s){return {float(p.x*s),float(p.y*s),float(p.z*s)};}
double dot(Vec3 a,Vec3 b){return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;}
Vec3 unit(Vec3 v){return mul(v,1/std::sqrt(dot(v,v)));}
Vec3 rz(Vec3 p,double a){return {float(std::cos(a)*p.x-std::sin(a)*p.y),float(std::sin(a)*p.x+std::cos(a)*p.y),p.z};}
Vec3 ry(Vec3 p,double a){return {float(std::cos(a)*p.x+std::sin(a)*p.z),p.y,float(-std::sin(a)*p.x+std::cos(a)*p.z)};}
// IAU 1976 岁差；目标是可视化精度，不包含章动、光行差或恒星自行。
Vec3 precess(Vec3 p,double jd,bool inverse){
  const double t=(jd-2451545)/36525,t2=t*t,t3=t2*t;
  const double zeta=(2306.2181*t+.30188*t2+.017998*t3)*rad/3600;
  const double z=(2306.2181*t+1.09468*t2+.018203*t3)*rad/3600;
  const double theta=(2004.3109*t-.42665*t2-.041833*t3)*rad/3600;
  return inverse?rz(ry(rz(p,-z),theta),-zeta):rz(ry(rz(p,zeta),-theta),z);
}
}
Vec3 NightAstronomy::equatorial_to_world(Vec3 p,bool j2000) const {
  if(j2000)p=precess(p,julian_day,false);
  const double c=std::cos(sidereal),s=std::sin(sidereal),a=std::sin(latitude),b=std::cos(latitude);
  return rz({float(-s*p.x+c*p.y),float(-a*c*p.x-a*s*p.y+b*p.z),float(b*c*p.x+b*s*p.y+a*p.z)},rotation);
}
Vec3 NightAstronomy::world_to_equatorial(Vec3 p,bool j2000) const {
  p=rz(p,-rotation);
  const double c=std::cos(sidereal),s=std::sin(sidereal),a=std::sin(latitude),b=std::cos(latitude);
  Vec3 q{float(-s*p.x-a*c*p.y+b*c*p.z),float(c*p.x-a*s*p.y+b*s*p.z),float(b*p.y+a*p.z)};
  return j2000?precess(q,julian_day,true):q;
}
NightAstronomy night_astronomy(const OptionNode &n) {
  NightAstronomy a;
  a.julian_day=number(n,"SS Day",2457092)-.5+(number(n,"SS Time",43200)-3600*number(n,"SS UTC Offset",0))/86400;
  const double d=a.julian_day-2451543.5,t=(a.julian_day-2451545)/36525;
  a.sidereal=std::remainder(280.46061837+360.98564736629*(a.julian_day-2451545)+.000387933*t*t-t*t*t/38710000+number(n,"SS Longitude",0),360.)*rad;
  a.latitude=std::clamp(number(n,"SS Latitude",0),-90.,90.)*rad;
  a.rotation=number(n,"Dome Rotation",0)*rad;
  a.sun=solar_direction(n);
  // Schlyter 低阶月球轨道 + 主要摄动，使用地球半径作为距离单位。
  const double N=125.1228-.0529538083*d,w=318.0634+.1643573223*d,M=115.3654+13.0649929509*d,e=.0549;
  double E=std::remainder(M,360.)*rad;
  for(int i=0;i<8;++i)E-=(E-e*std::sin(E)-std::remainder(M,360.)*rad)/(1-e*std::cos(E));
  const double x=60.2666*(std::cos(E)-e),y=60.2666*std::sqrt(1-e*e)*std::sin(E),v=std::atan2(y,x)/rad;
  double r=std::hypot(x,y);
  const double xe=r*(cosd(N)*cosd(v+w)-sind(N)*sind(v+w)*cosd(5.1454));
  const double ye=r*(sind(N)*cosd(v+w)+cosd(N)*sind(v+w)*cosd(5.1454));
  const double ze=r*sind(v+w)*sind(5.1454);
  double lon=std::atan2(ye,xe)/rad,lat=std::atan2(ze,std::hypot(xe,ye))/rad;
  const double Ms=356.0470+.9856002585*d,ws=282.9404+4.70935e-5*d,Ls=Ms+ws,Lm=M+w+N,D=Lm-Ls,F=Lm-N;
  lon+=-1.274*sind(M-2*D)+.658*sind(2*D)-.186*sind(Ms)-.059*sind(2*M-2*D)-.057*sind(M-2*D+Ms)+.053*sind(M+2*D)+.046*sind(2*D-Ms)+.041*sind(M-Ms)-.035*sind(D)-.031*sind(M+Ms)-.015*sind(2*F-2*D)+.011*sind(M-4*D);
  lat+=-.173*sind(F-2*D)-.055*sind(M-F-2*D)-.046*sind(M+F-2*D)+.033*sind(F+2*D)+.017*sind(2*M+F);
  r+=-.58*cosd(M-2*D)-.46*cosd(2*D);
  const double ob=(23.4393-3.563e-7*d)*rad;
  Vec3 eq{float(r*cosd(lon)*cosd(lat)),float(r*(sind(lon)*cosd(lat)*std::cos(ob)-sind(lat)*std::sin(ob))),float(r*(sind(lon)*cosd(lat)*std::sin(ob)+sind(lat)*std::cos(ob)))};
  auto top=a.equatorial_to_world(eq,false);
  // 海平面椭球观测者视差，避免近地平线接近一度的地心位置误差。
  const double geocentric=std::atan(.99330562*std::tan(a.latitude));
  const double rho=.99833+.00167*std::cos(2*a.latitude);
  auto observer=rz({0,float(rho*std::sin(geocentric-a.latitude)),float(rho*std::cos(geocentric-a.latitude))},a.rotation);
  top={top.x-observer.x,top.y-observer.y,top.z-observer.z};
  a.moon_distance=std::sqrt(dot(top,top));a.moon=unit(top);
  a.moon_radius=std::asin(.2725076/a.moon_distance);
  a.illuminated=std::clamp((1-dot(a.moon,a.sun))*.5,0.,1.);
  const double elevation=std::asin(std::clamp(double(a.sun.z),-1.,1.))/rad;
  const double fade=std::clamp((-elevation-4)/14,0.,1.);a.darkness=fade*fade*(3-2*fade);
  return a;
}
}
