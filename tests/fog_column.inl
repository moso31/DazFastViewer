#include "render_ir/fog_column.h"
static void fog_columns() {
  // Independent numerical quadrature, including rays that cross the base,
  // near-horizontal rays, a displaced atmosphere, and a camera inside it.
  for(float height:{100.f,1000.f,10000.f})for(float z:{-200.f,0.f,338.f,3000.f})
    for(float dz:{-1.f,-.01f,0.f,1e-8f,.1f,1.f})for(float distance:{1.f,2841.75f,10000.f}) {
      constexpr int n=8192;const double step=double(distance)/n;double reference=0;
      for(int i=0;i<n;++i)reference+=std::exp(-std::max(0.,double(z)+dz*((i+.5)*step))/height)*step;
      const double actual=dfv_fog_column(z,dz,distance,0,0,height,false);
      require(std::isfinite(actual)&&std::abs(actual-reference)<2e-4*std::max(1.,reference),"Atmosphere integral disagrees with numerical density integration");
      require(std::abs(actual-dfv_fog_column(z+8192,dz,distance,0,8192,height,false))<1e-4*std::max(1.,actual),"Moving atmosphere and camera together changed optical depth");
      const double a=dfv_fog_column(z,dz,distance*.4f,0,0,height,false);
      const double b=dfv_fog_column(z+dz*distance*.4f,dz,distance*.6f,0,0,height,false);
      require(std::abs(actual-a-b)<2e-4*std::max(1.,actual),"Atmosphere segments do not compose");
    }
  require(dfv_fog_column(1000,0,100,200,0,1000,false)==0,"Fog starts before near exclusion");
  for(float dz:{1e-20f,.000001f,.1f,1.f}) {
    const auto sky=dfv_fog_column(338,dz,0,50,0,1000,true);
    const double expected=1000./dz*std::exp(-(338.+dz*50)/1000.);
    require(std::isfinite(sky)&&std::abs(sky/expected-1)<1e-5,"Upward background column does not integrate to infinity");
  }
  for(float z:{-1e9f,0.f,1e9f})for(float dz:{-1.f,0.f,1e-30f,1.f})
    require(std::isfinite(dfv_fog_column(z,dz,0,0,0,.001f,true)),"Extreme atmosphere produced NaN/inf");
}
