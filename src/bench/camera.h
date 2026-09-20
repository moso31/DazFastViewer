#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace dfv {
struct Vec3 {
  float x{}, y{}, z{};
  Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
  Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
  Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec3 normalized(Vec3 a) {
  const float length=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
  return a*(1.0f/std::max(length,1e-8f));
}
struct CameraState {
  Vec3 target{0,0,1};
  float yaw=0.5f, pitch=0.28f, distance=12.0f;
  uint64_t epoch=1;
  double input_seconds=0;
  Vec3 eye() const {
    return target+Vec3{std::sin(yaw)*std::cos(pitch),-std::cos(yaw)*std::cos(pitch),std::sin(pitch)}*distance;
  }
  // Cycles 相机局部 +Z 指向视线方向，局部 +Y 向上。
  std::array<float,12> matrix() const {
    const Vec3 origin=eye(), forward=normalized(target-origin);
    const Vec3 right=normalized(cross(forward,{0,0,1}));
    const Vec3 up=cross(right,forward);
    return {right.x,up.x,forward.x,origin.x,
            right.y,up.y,forward.y,origin.y,
            right.z,up.z,forward.z,origin.z};
  }
  void orbit(float dx,float dy) { yaw-=dx*0.005f; pitch=std::clamp(pitch+dy*0.005f,-1.45f,1.45f); }
  void pan(float dx,float dy) {
    const Vec3 forward=normalized(target-eye()),right=normalized(cross(forward,{0,0,1}));
    target=target+right*(-dx*distance*0.001f)+cross(right,forward)*(dy*distance*0.001f);
  }
  void dolly(float ticks) { distance=std::clamp(distance*std::exp(-ticks*0.12f),0.3f,200.0f); }
};
class CameraMailbox {
  std::mutex mutex_;
  CameraState latest_;
public:
  void publish(CameraState state) { std::lock_guard lock(mutex_); latest_=state; }
  CameraState latest() { std::lock_guard lock(mutex_); return latest_; }
};
}
