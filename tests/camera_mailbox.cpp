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
    camera.navigating=true;
    require(camera.needs_preview(camera.epoch,1000),"持续按键或右键不能因超时恢复完整渲染");
    camera.navigating=false;camera.preview_until=.15;
    require(camera.needs_preview(camera.epoch,.1),"滚轮静止窗口内应保持预览");
    require(!camera.needs_preview(camera.epoch,.2),"输入停止后应恢复完整渲染");
    require(camera.needs_preview(camera.epoch-1,1000),"延迟处理的滚轮或聚焦也必须先输出预览");
    const auto origin=camera.eye();const auto target=camera.target;
    camera.look(40,-20);
    const auto moved=camera.eye()-origin;
    require(std::abs(moved.x)+std::abs(moved.y)+std::abs(moved.z)<.00001f,"第一人称转头改变了相机位置");
    require(std::abs(camera.target.x-target.x)>.01f,"第一人称转头没有改变观察方向");
    camera.look(0,10000);
    const auto clamped=camera.eye()-origin;
    require(std::abs(clamped.x)+std::abs(clamped.y)+std::abs(clamped.z)<.00001f,"俯仰限幅改变了第一人称位置");
    const auto before_move=camera.eye();camera.move(1,1,0,.1f);
    const auto after_move=camera.eye()-before_move;
    require(std::abs(after_move.x)+std::abs(after_move.y)+std::abs(after_move.z)>.01f,"第一人称转头后 WASD 未移动");
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
