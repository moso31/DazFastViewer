#include "bench/camera.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <stdexcept>

static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
int main() {
  try {
    dfv::CameraMailbox mailbox;
    std::atomic<bool> finished=false;
    std::jthread producer([&] {
      for(uint64_t i=2;i<=1001;++i) {dfv::CameraState s;s.epoch=i;s.target.x=float(i);mailbox.publish(s);}
      finished=true;
    });
    uint64_t previous=0;
    do {
      const auto s=mailbox.latest();
      require(s.epoch>=previous,"相机版本倒退");previous=s.epoch;
      if(s.epoch>1) require(s.target.x==float(s.epoch),"读到了撕裂的相机快照");
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while(!finished.load());
    producer.join();
    require(mailbox.latest().epoch==1001,"消费者停顿后没有取得最新状态");
    dfv::CameraState camera;
    for(int i=0;i<10000;++i) {camera.orbit(0.3f,1);camera.dolly(i%2?2:-2);}
    const auto m=camera.matrix();
    for(float v:m) require(std::isfinite(v),"相机矩阵出现非有限值");
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) {
      float dot=0;for(int k=0;k<3;++k) dot+=m[k*4+i]*m[k*4+j];
      require(std::abs(dot-(i==j?1.0f:0.0f))<0.0001f,"相机坐标系不正交");
    }
    std::cout<<"latest snapshot / coherent epoch / stable camera basis: PASS\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
