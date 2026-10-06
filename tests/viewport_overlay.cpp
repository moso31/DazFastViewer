#include "viewport/window.h"
#include "viewport/overlay.h"
#include "viewport/display.h"
#include "util/half.h"
#include <QImage>
#include <filesystem>
#include <iostream>
using namespace dfv;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
static ir::Mesh plane(float left,float right){ir::Mesh m;m.positions={{left,0,-.5f},{right,0,-.5f},{right,0,.5f},{left,0,.5f}};m.triangles={{{0,1,2},0},{{0,2,3},0}};return m;}
int main(){
  try{
    std::filesystem::create_directories("artifacts/experience-feedback/overlay");Window window(400,240,false,nullptr,GetSystemMetrics(SM_CMONITORS)>1?2:0);window.automated_pointer=true;window.present_context.activate();
    glViewport(0,0,400,240);glDrawBuffer(GL_BACK);GLint bits=0;glGetIntegerv(GL_STENCIL_BITS,&bits);check(bits>=8,"选择遮罩缺少 stencil 缓冲");
    auto picture=[&](const char *name){QImage image(400,240,QImage::Format_RGBA8888);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());image=image.flipped(Qt::Vertical);image.save(QString("artifacts/experience-feedback/overlay/")+name+".png");return image;};
    CameraState camera;camera.target={0,0,0};camera.yaw=camera.pitch=0;camera.distance=3;
    ir::Scene scene;scene.meshes={plane(-1,-.1f),plane(.1f,1)};scene.instances.resize(2);scene.instances[1].mesh=1;
    HoverOverlay overlay;overlay.update(scene,{});overlay.draw_pose(camera,400,240,scene.meshes[0],{}, {},{100,100,100},{0});
    auto white=picture("all-proxies");check(white.pixelColor(150,120).red()>40&&white.pixelColor(250,120).red()>40,"拖动代理没有同时显示活动模型及另一模型");
    scene.materials.resize(1);scene.materials[0].cloud.emplace();scene.instances[1].materials={0};overlay.update(scene,{});overlay.draw_pose(camera,400,240,{},{},{},{100,100,100},{});
    auto volume=picture("cloud-no-solid-proxy");check(volume.pixelColor(150,120).red()>40&&volume.pixelColor(250,120).red()<40,"云层边界盒作为实心白模遮挡了视口");scene.instances[1].materials.clear();scene.materials.clear();
    scene.meshes={plane(-1,1)};scene.instances[1].mesh=0;overlay.update(scene,{});glClearColor(.2f,.2f,.2f,1);glClear(GL_COLOR_BUFFER_BIT);
    const std::vector<uint32_t> members{0,1};overlay.draw(camera,400,240,0,-1,&members);auto highlighted=picture("single-highlight");const auto yellow=highlighted.pixelColor(200,120);
    check(std::abs(yellow.red()-96)<=2&&std::abs(yellow.green()-89)<=2&&std::abs(yellow.blue()-58)<=2,"重叠表面重复叠加黄色标记");
    scene.instances[1].transform.value[7]=-.00002f;overlay.update(scene,{});glClear(GL_COLOR_BUFFER_BIT);overlay.draw(camera,400,240,0);
    const auto covered=picture("clothing-through-highlight").pixelColor(200,120);check(covered==yellow,"黄色选区没有透过紧贴身体的衣物显示");
    scene.instances[1].transform.value[7]=-.5f;overlay.update(scene,{});glClear(GL_COLOR_BUFFER_BIT);overlay.draw(camera,400,240,0);
    check(picture("occluder-through-highlight").pixelColor(200,120)==yellow,"有间距的遮挡物阻挡了黄色选区");
    runtime::JointRegions region;region.head=-1;region.detail={7,8};region.body={7,8};overlay.update(scene,{region});glClear(GL_COLOR_BUFFER_BIT);overlay.draw(camera,400,240,0,7);
    const auto part=picture("joint-through-highlight");check(part.pixelColor(230,140)==yellow&&part.pixelColor(170,100).red()==51,"穿透选区没有保持部位范围");
    scene.meshes[0].triangles[1].material_slot=1;overlay.update(scene,{});const std::vector<std::pair<size_t,size_t>> surfaces{{0,0},{1,0}};glClear(GL_COLOR_BUFFER_BIT);overlay.draw(camera,400,240,-1,-1,nullptr,&surfaces);
    const auto material=picture("material-through-highlight");check(material.pixelColor(230,140)==yellow&&material.pixelColor(170,100).red()==51&&overlay.surface_count(0,0)==1,"子材质遮罩范围错误或重复混色");
    scene.instances[0].visible=false;overlay.update(scene,{});glClear(GL_COLOR_BUFFER_BIT);overlay.draw(camera,400,240,0,-1,&members);
    // 隐藏对象不参与遮罩；同时取消选择前方物体。
    glClear(GL_COLOR_BUFFER_BIT);const std::vector<uint32_t> hidden{0};overlay.draw(camera,400,240,0,-1,&hidden);
    check(picture("hidden-no-highlight").pixelColor(200,120).red()==51,"隐藏对象仍显示黄色选区");
    check(glGetError()==GL_NO_ERROR,"覆盖渲染存在 OpenGL 错误");overlay.release();window.present_context.deactivate();
    {
      Telemetry telemetry("artifacts/experience-feedback/reconstruction");std::atomic<uint64_t> epoch=1;std::atomic<int> samples=1;
      Display display(window,telemetry,epoch,samples,true);
      ccl::DisplayDriver::Params params;params.size={200,120};params.full_size=params.size;
      check(display.update_begin(params,200,120),"无法写入升采样检查帧");auto *pixels=display.map_texture_buffer();check(pixels!=nullptr,"无法映射检查缓冲");
      for(int y=0;y<120;++y)for(int x=0;x<200;++x){const float v=x<100?0.f:1.f;pixels[y*200+x]=ccl::float4_to_half4(ccl::make_float4(v,v,v,1));}
      display.unmap_texture_buffer();display.update_end();window.present_context.activate();glFinish();glViewport(0,0,400,240);
      display.set_reconstruction(Reconstruction::bilinear);display.draw(params);auto linear=picture("upscale-bilinear");
      display.set_reconstruction(Reconstruction::bicubic);display.draw(params);auto cubic=picture("upscale-bicubic");
      check(linear.pixelColor(199,120).red()>20&&linear.pixelColor(199,120).red()<230,"低分辨率仍显示为最近邻方块");
      check(cubic.pixelColor(199,120).red()<linear.pixelColor(199,120).red()&&cubic.pixelColor(200,120).red()>linear.pixelColor(200,120).red(),"双三次未产生较清晰的边缘");
      check(cubic.pixelColor(399,239).red()==255&&cubic.pixelColor(0,0).red()==0,"升采样采到了纹理填充区或亮边振铃");
      for(auto filter:{Reconstruction::bilinear,Reconstruction::bicubic}) {
        display.set_reconstruction(filter);display.set_sharpen(0);display.draw(params);const auto normal=picture("sharpen-upscale-off");
        display.set_sharpen(1);display.draw(params);const auto sharp=picture("sharpen-upscale-on");
        check(sharp.pixelColor(199,120).red()<normal.pixelColor(199,120).red()&&sharp.pixelColor(200,120).red()>normal.pixelColor(200,120).red(),"升采样锐化未增强边缘对比度");
        check(sharp.pixelColor(399,239).red()==255&&sharp.pixelColor(0,0).red()==0,"锐化采到了无效边界");
        display.set_sharpen(0);display.draw(params);check(picture("sharpen-upscale-restored")==normal,"关闭锐化没有精确恢复图像");
      }
      window.present_context.deactivate();params.size={400,240};params.full_size=params.size;
      check(display.update_begin(params,400,240),"无法写入原分辨率检查帧");pixels=display.map_texture_buffer();check(pixels!=nullptr,"无法映射锐化检查缓冲");
      for(int y=0;y<240;++y)for(int x=0;x<400;++x){const float v=x<198?.125f:x==198?.25f:x==199?.375f:x==200?.625f:x==201?.75f:.875f;pixels[y*400+x]=ccl::float4_to_half4(ccl::make_float4(v,v,v,1));}
      display.unmap_texture_buffer();display.update_end();window.present_context.activate();glFinish();
      display.set_sharpen(0);display.draw(params);const auto normal=picture("sharpen-native-off");
      display.set_sharpen(.5f);display.draw(params);const auto medium=picture("sharpen-native-50");
      display.set_sharpen(1);display.draw(params);const auto sharp=picture("sharpen-native-100");
      check(normal.pixelColor(199,120).red()>medium.pixelColor(199,120).red()&&medium.pixelColor(199,120).red()>sharp.pixelColor(199,120).red(),"原分辨率锐化强度没有逐级生效");
      check(sharp.pixelColor(0,0)==normal.pixelColor(0,0)&&sharp.pixelColor(399,239)==normal.pixelColor(399,239),"锐化改变了平坦区域或图像边界");
      display.set_sharpen(10);display.draw(params);const auto maximum=picture("sharpen-native-strength-10");
      check(maximum.pixelColor(199,120).red()<sharp.pixelColor(199,120).red()&&maximum.pixelColor(200,120).red()>sharp.pixelColor(200,120).red(),"扩大强度没有超过原 100% 的锐化效果");
      check(maximum.pixelColor(0,0)==normal.pixelColor(0,0)&&maximum.pixelColor(399,239)==normal.pixelColor(399,239),"最大强度破坏了平坦区域或边界");
      display.set_sharpen(0);display.draw(params);check(picture("sharpen-native-restored")==normal,"原分辨率关闭锐化没有精确恢复");
      window.present_context.deactivate();check(display.update_begin(params,400,240),"无法写入高强度检查帧");pixels=display.map_texture_buffer();check(pixels!=nullptr,"无法映射高强度缓冲");
      for(int y=0;y<240;++y)for(int x=0;x<400;++x){const float v=x<198?.1f:x==198?.2f:x==199?.4f:x==200?.61f:x==201?.8f:.9f;pixels[y*400+x]=ccl::float4_to_half4(ccl::make_float4(v,v,v,1));}
      display.unmap_texture_buffer();display.update_end();window.present_context.activate();glFinish();
      display.set_sharpen(10);display.draw(params);const auto ten=picture("sharpen-range-10");
      display.set_sharpen(20);display.draw(params);const auto twenty=picture("sharpen-range-20");
      check(twenty.pixelColor(199,120).red()<ten.pixelColor(199,120).red()&&twenty.pixelColor(200,120).red()>ten.pixelColor(200,120).red(),"GPU 强度仍被截断在 10.00");
      check(twenty.pixelColor(0,0)==ten.pixelColor(0,0)&&twenty.pixelColor(399,239)==ten.pixelColor(399,239),"20.00 强度破坏了平坦区域或边界");
      check(glGetError()==GL_NO_ERROR&&!display.failed(),"升采样着色器失败");display.release_present_resources();window.present_context.deactivate();
    }
    std::cout<<"副屏 OpenGL：全场景代理、单次黄色合成、选区穿透衣物及部位范围、双线性/双三次升采样 PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
