struct NightSH {std::array<dfv::ir::Vec3,9> galaxy{},stars{};};
NightSH procedural_night_sh(int quality,float detail,float density){
  struct Sample {ccl::DfvNightRaw raw;std::array<double,9> basis;double solid;};
  struct CachedSH {bool valid=false;float detail=0,density=0;NightSH value;};
  static std::mutex mutex;static std::array<std::vector<Sample>,3> cache;
  static std::array<CachedSH,3> final_cache;
  std::lock_guard lock(mutex);quality=std::clamp(quality,0,2);auto &samples=cache[quality];
  auto &last=final_cache[quality];if(last.valid&&last.detail==detail&&last.density==density)return last.value;
  if(samples.empty()){
    samples.reserve(128*64);
    for(int y=0;y<64;++y)for(int x=0;x<128;++x){const auto d=dfv::ir::night_map_direction((x+.5)/128,(y+.5)/64);const auto direction=ccl::dfv_night_reference_direction(ccl::make_float3(d.x,d.y,d.z));samples.push_back({ccl::dfv_night_raw(direction,quality,true),dfv::ir::night_sh_basis(d),2*M_PI*M_PI/(128*64)*std::sin(M_PI*(y+.5)/64)});}
  }
  NightSH result;
  // 密度/对比度只对缓存的球状噪声样本求值；不重新跑邻域搜索或生成天空图片。
  for(const auto &s:samples){ccl::float3 galaxy,stars;ccl::dfv_night_finish(s.raw,detail,density,&galaxy,&stars);for(int k=0;k<9;++k){const float a=float(s.solid*s.basis[k]);result.galaxy[k].x+=galaxy.x*a;result.galaxy[k].y+=galaxy.y*a;result.galaxy[k].z+=galaxy.z*a;result.stars[k].x+=stars.x*a;result.stars[k].y+=stars.y*a;result.stars[k].z+=stars.z*a;}}
  last={true,detail,density,result};return result;
}
