#include "cycles/runtime_paths.h"
#include "editor/renderer.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <OpenImageIO/imageio.h>
#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QThread>
#include <fstream>
#include <iostream>

using namespace dfv;
using namespace dfv::editor;
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  const auto output=std::filesystem::absolute(argc>1?argv[1]:"artifacts/project-render-settings/gpu");
  std::filesystem::create_directories(output);
  try {
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);
    ccl::path_init(app.applicationDirPath().toStdString(),dfv::cycles_user_directory());
    const auto texture=output/"quality-4096.png";
    std::vector<unsigned char> pixels(4096*4096*4,255);
    for(size_t i=0;i<pixels.size();i+=4){pixels[i]=static_cast<unsigned char>((i/4)%251);pixels[i+1]=112;pixels[i+2]=64;}
    auto image=OIIO::ImageOutput::create(texture.string());
    check(image&&image->open(texture.string(),OIIO::ImageSpec(4096,4096,4,OIIO::TypeDesc::UINT8))&&image->write_image(OIIO::TypeDesc::UINT8,pixels.data())&&image->close(),"测试贴图创建失败");pixels.clear();pixels.shrink_to_fit();
    auto document=std::make_shared<Document>();document->generation=1;
    auto &scene=document->loaded.scene;scene.textures.push_back({"quality-texture",texture});
    ir::Material material;material.id="quality-material";material.color_texture=0;scene.materials.push_back(material);
    ir::Mesh mesh;mesh.id="quality-plane";mesh.material_slots={"surface"};mesh.positions={{-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1}};
    ir::Triangle a,b;a.vertices={0,1,2};a.uv={ir::Vec2{0,0},ir::Vec2{1,0},ir::Vec2{1,1}};b.vertices={0,2,3};b.uv={ir::Vec2{0,0},ir::Vec2{1,1},ir::Vec2{0,1}};mesh.triangles={a,b};scene.meshes.push_back(mesh);
    ir::Instance instance;instance.id="quality-plane";instance.mesh=0;instance.materials={0};scene.instances.push_back(instance);
    runtime::Target target;target.id=instance.id;target.label="quality-plane";document->catalog.targets.push_back(target);document->formulas.graphs.resize(1);
    auto snapshot=initial_snapshot(*document);snapshot.revision=2;snapshot.values[0].transform.translation_cm.x=12;
    QScreen *secondary=QGuiApplication::primaryScreen();for(auto *screen:QGuiApplication::screens())if(screen!=QGuiApplication::primaryScreen()){secondary=screen;break;}
    check(secondary,"No available display");QWidget host;host.setWindowTitle(QStringLiteral("纹理资源释放验证（自动关闭）"));host.resize(420,300);host.move(secondary->availableGeometry().topLeft()+QPoint(20,20));host.show();app.processEvents();
    SamplingSettings sampling;sampling.samples=32;
    Renderer renderer(reinterpret_cast<HWND>(host.winId()),qRound(host.width()*host.devicePixelRatioF()),qRound(host.height()*host.devicePixelRatioF()),output,sampling);
    renderer.automated_pointer();renderer.set_document(document,snapshot,false);renderer.camera_view({0,0,0,4,0,0});
    auto ready=[&](int sessions,const RenderQuality &quality) {
      const auto start=now();
      while(now()-start<90) {
        app.processEvents();const auto state=renderer.status();check(state.error.empty(),state.error.c_str());
        if(state.sessions==sessions&&static_cast<const RenderQuality &>(state.sampling)==quality&&state.presented_epoch==state.requested_epoch&&!state.preview&&state.samples>=8)return state;
        QThread::msleep(10);
      }
      throw std::runtime_error("GPU 画质切换等待超时");
    };
    nlohmann::json records=nlohmann::json::array();size_t original=0,reduced=0;int sessions=0;
    for(int limit:{0,512,1024,2048,4096,0}) {
      RenderQuality quality;quality.texture_limit=limit;renderer.render_quality(quality);
      const auto state=ready(++sessions,quality);
      check(state.generation==1&&state.applied_revision==2,"资源重建丢失未保存编辑版本");
      check(state.target_world.size()==1&&std::abs(state.target_world[0].value[3]-.12f)<1e-6,"纹理切换丢失对象变换");
      check(state.camera.target.x==0&&state.camera.target.y==0&&state.camera.target.z==0&&state.camera.distance==4&&state.camera.yaw==0&&state.camera.pitch==0,"纹理切换改变相机");
      const size_t bytes=state.gpu_device_bytes+state.gpu_host_bytes;
      if(sessions==1)original=bytes;if(limit==512)reduced=bytes;
      if(sessions==6)check(bytes<=original+1024*1024,"恢复原始精度后残留前几轮纹理");
      records.push_back({{"limit",limit},{"sessions",state.sessions},{"bytes",bytes},{"device_bytes",state.gpu_device_bytes},{"host_bytes",state.gpu_host_bytes},{"generation",state.generation},{"revision",state.applied_revision}});
      std::ofstream(output/"quality-checks.json")<<records.dump(2);
    }
    check(original>reduced+32*1024*1024,"降低到 512 后旧大纹理没有释放");
    const auto before=renderer.status().sessions;renderer.render_quality({});app.processEvents();QThread::msleep(100);check(renderer.status().sessions==before,"重复保存同一画质仍重建会话");
    const RenderQuality fast{1024,8,false,false};renderer.render_quality(fast);const auto fast_state=ready(++sessions,fast);
    check(!fast_state.sampling.subsurface&&!fast_state.sampling.bump_and_normal&&fast_state.sampling.transparent_bounces==8,"额外画质选项没有应用到渲染线程");
    check(scene.materials[0]==material&&scene.textures[0].file==texture,"渲染设置改写原场景资源");
    const auto stable=renderer.status();ViewportQuality viewport=stable.quality;viewport.sharpen=20.f;renderer.quality(viewport);
    const auto sharpen_start=now();RenderStatus sharpened;
    do {app.processEvents();QThread::msleep(10);sharpened=renderer.status();check(sharpened.error.empty(),sharpened.error.c_str());}while(sharpened.quality!=viewport&&now()-sharpen_start<10);
    check(sharpened.quality==viewport&&sharpened.sessions==stable.sessions&&sharpened.requested_epoch==stable.requested_epoch&&sharpened.samples>=stable.samples,"锐化没有应用，或导致渲染会话/采样重置");
    std::ofstream(output/"result.json")<<nlohmann::json{{"result","PASS"},{"released_bytes",original-reduced},{"sessions",sessions},{"final_transparent_bounces",8},{"final_subsurface",false},{"final_bump_and_normal",false},{"sharpen_strength",sharpened.quality.sharpen},{"sharpen_preserved_sampling",true}}.dump(2);
    std::cout<<"纹理全部档位、释放与恢复、编辑和相机保留：PASS\n";return 0;
  }catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}
}
