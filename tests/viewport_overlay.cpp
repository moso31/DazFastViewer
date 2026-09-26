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
    std::filesystem::create_directories("artifacts/experience-feedback/overlay");Window window(400,240,false,nullptr,2);window.automated_pointer=true;window.present_context.activate();
    glViewport(0,0,400,240);glDrawBuffer(GL_BACK);GLint bits=0;glGetIntegerv(GL_STENCIL_BITS,&bits);check(bits>=8,"选择遮罩缺少 stencil 缓冲");
    auto picture=[&](const char *name){QImage image(400,240,QImage::Format_RGBA8888);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());image=image.flipped(Qt::Vertical);image.save(QString("artifacts/experience-feedback/overlay/")+name+".png");return image;};
    CameraState camera;camera.target={0,0,0};camera.yaw=camera.pitch=0;camera.distance=3;
    ir::Scene scene;scene.meshes={plane(-1,-.1f),plane(.1f,1)};scene.instances.resize(2);scene.instances[1].mesh=1;
    HoverOverlay overlay;overlay.update(scene,{});overlay.draw_pose(camera,400,240,scene.meshes[0],{}, {},{100,100,100},{0});
    auto white=picture("all-proxies");check(white.pixelColor(150,120).red()>40&&white.pixelColor(250,120).red()>40,"拖动代理没有同时显示活动模型及另一模型");
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
      check(glGetError()==GL_NO_ERROR&&!display.failed(),"升采样着色器失败");display.release_present_resources();window.present_context.deactivate();
    }
    std::cout<<"副屏 OpenGL：全场景代理、单次黄色合成、选区穿透衣物及部位范围、双线性/双三次升采样 PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
