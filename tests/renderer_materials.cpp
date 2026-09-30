#include "cycles/runtime_paths.h"
#include "editor/renderer.h"
#include "editor/scene_extension.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QThread>
#include <QImage>
#include <fstream>
#include <iostream>
using namespace dfv;using namespace dfv::editor;
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(int argc,char **argv){
  QApplication app(argc,argv);const auto output=std::filesystem::absolute("artifacts/materials/gpu");std::filesystem::create_directories(output);
  try{
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);ccl::path_init(app.applicationDirPath().toStdString(),dfv::cycles_user_directory());
    auto d=std::make_shared<Document>();d->generation=1;auto &scene=d->loaded.scene;ir::Material material;material.id="surface";scene.materials={material};
    ir::Mesh mesh;mesh.id="plane";mesh.material_slots={"Surface"};mesh.positions={{-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1}};ir::Triangle a,b;a.vertices={0,1,2};a.uv={ir::Vec2{0,0},ir::Vec2{1,0},ir::Vec2{1,1}};b.vertices={0,2,3};b.uv={ir::Vec2{0,0},ir::Vec2{1,1},ir::Vec2{0,1}};mesh.triangles={a,b};scene.meshes={mesh};ir::Instance instance;instance.id="plane";instance.materials={0};scene.instances={instance};runtime::Target target;target.id="plane";target.label="plane";d->catalog.targets={target};d->formulas.graphs.resize(1);
    auto snapshot=initial_snapshot(*d);QScreen *secondary=QGuiApplication::primaryScreen();for(auto *screen:QGuiApplication::screens())if(screen!=QGuiApplication::primaryScreen()){secondary=screen;break;}check(secondary,"No available display");QWidget host;host.setWindowTitle(QStringLiteral("材质渲染验证（自动关闭）"));host.resize(420,300);host.move(secondary->availableGeometry().topLeft()+QPoint(30,30));host.show();app.processEvents();
    SamplingSettings settings;settings.samples=32;settings.adaptive_threshold=0;Renderer renderer(reinterpret_cast<HWND>(host.winId()),qRound(host.width()*host.devicePixelRatioF()),qRound(host.height()*host.devicePixelRatioF()),output,settings);renderer.automated_pointer();renderer.set_document(d,snapshot,false);renderer.camera_view({0,0,0,4,0,0});
    nlohmann::json records=nlohmann::json::array();
    auto ready=[&](const char *name){const auto start=now();while(now()-start<120){app.processEvents();auto s=renderer.status();check(s.error.empty(),s.error.c_str());check(s.edit_error.empty(),s.edit_error.c_str());if(s.applied_revision==snapshot.revision&&s.presented_revision==snapshot.revision&&s.presented_epoch==s.requested_epoch&&!s.preview&&s.samples>=16){check(s.sessions==1,"材质编辑重建了渲染会话");auto image=secondary->grabWindow(host.winId()).toImage();image.save(QString::fromStdWString((output/(std::string(name)+".png")).wstring()));records.push_back({{"stage",name},{"revision",s.applied_revision},{"sessions",s.sessions},{"materials",s.adapter.material_updates},{"samples",s.samples}});std::ofstream(output/"checks.json")<<records.dump(2);return image;}QThread::msleep(10);}throw std::runtime_error("材质 GPU 更新超时");};
    auto difference=[](const QImage &a,const QImage &b){size_t n=0;for(int y=0;y<std::min(a.height(),b.height());++y)for(int x=0;x<std::min(a.width(),b.width());++x)if(a.pixel(x,y)!=b.pixel(x,y))++n;return n;};
    auto initial=ready("initial");const auto before_hover=renderer.status();auto hover_ready=[&](size_t count){const auto start=now();while(now()-start<10){app.processEvents();auto s=renderer.status();if(s.material_hover_primitives==count&&now()-start>.1){check(s.sessions==before_hover.sessions&&s.requested_epoch==before_hover.requested_epoch&&s.adapter.material_updates==before_hover.adapter.material_updates,"悬浮高亮改变了材质或重启采样");return;}QThread::msleep(10);}throw std::runtime_error("材质悬浮状态未送达视口");};renderer.hover_materials(d->generation,{{0,0}});hover_ready(2);auto highlight=ready("material-hover");check(difference(initial,highlight)>100,"悬浮材质未在视口显示黄色");renderer.hover_materials(d->generation,{});hover_ready(0);ready("material-hover-cleared");
    auto &patch=snapshot.material_overrides["plane"]["Surface"];patch["base_color"]=nlohmann::json::array({1.,.005,.005});++snapshot.revision;renderer.edit(snapshot);auto red=ready("red");check(difference(initial,red)>100,"基础色编辑没有改变渲染图像");
    patch["base_color"]=nlohmann::json::array({.005,.005,1.});++snapshot.revision;renderer.edit(snapshot);auto blue=ready("blue");check(difference(red,blue)>100,"连续材质 Delta 没有改变渲染图像");
    auto texture=output/"green.png";QImage map(8,8,QImage::Format_RGB32);map.fill(Qt::green);check(map.save(QString::fromStdWString(texture.wstring())),"测试贴图保存失败");auto path=texture.generic_u8string();patch["base_color"]=nlohmann::json::array({1.,1.,1.});patch["color_texture"]={{"file",std::string(path.begin(),path.end())}};++snapshot.revision;renderer.edit(snapshot);auto green=ready("texture");check(difference(blue,green)>100,"贴图替换没有改变渲染图像");
    patch["overlay_weight"]=1.;patch["overlay_color"]=nlohmann::json::array({1.,.5,.005});++snapshot.revision;renderer.edit(snapshot);auto overlay=ready("overlay");check(difference(green,overlay)>100,"Diffuse Overlay 没有改变渲染图像");patch.erase("overlay_weight");patch.erase("overlay_color");
    patch["opacity"]=0.;++snapshot.revision;renderer.edit(snapshot);auto transparent=ready("transparent");check(difference(overlay,transparent)>100,"透明度编辑没有改变渲染图像");
    patch["opacity"]=1.;patch["emission_color"]=nlohmann::json::array({1.,.1,.01});patch["emission_luminance"]=10000.;++snapshot.revision;renderer.edit(snapshot);auto emission=ready("emission");check(difference(transparent,emission)>100,"自发光编辑没有改变渲染图像");
    auto saved=snapshot_json(*d,snapshot);snapshot=initial_snapshot(*d);snapshot.revision=10;renderer.edit(snapshot);auto reset=ready("reset");check(difference(initial,reset)<initial.width()*initial.height()/10,"材质还原没有恢复原画面");apply_snapshot_json(*d,snapshot,saved);snapshot.revision=11;renderer.edit(snapshot);auto restored=ready("restored");check(difference(reset,restored)>100,"DUFEX 材质恢复未实际渲染");
    check(scene.materials.size()==1&&scene.materials[0]==material&&scene.textures.empty(),"编辑改写了原始材质");std::ofstream(output/"result.json")<<nlohmann::json{{"result","PASS"},{"stages",records.size()},{"sessions",1}}.dump(2);std::cout<<"材质 GPU 九阶段：PASS\n";return 0;
  }catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}
}
