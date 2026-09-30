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

using namespace dfv;
using namespace dfv::editor;
static void check(bool value,const std::string &message) {if(!value) throw std::runtime_error(message);}

int main(int argc,char **argv) {
  QApplication app(argc,argv);
  const auto output=std::filesystem::absolute("artifacts/gl-recovery/gpu");
  std::filesystem::create_directories(output);
  try {
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();
    config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);
    ccl::path_init(app.applicationDirPath().toStdString(),dfv::cycles_user_directory());
    QScreen *secondary=QGuiApplication::primaryScreen();
    for(auto *screen:QGuiApplication::screens()) if(screen!=QGuiApplication::primaryScreen()) {secondary=screen;break;}
    check(secondary,"No available display");
    QWidget host;host.setWindowTitle(QStringLiteral("渲染故障恢复验证（自动关闭）"));
    host.setAttribute(Qt::WA_ShowWithoutActivating);host.resize(420,300);
    host.move(secondary->availableGeometry().topLeft()+QPoint(30,30));host.show();app.processEvents();

    // 真实 WGL 绑定验证：嵌套及异常退出不能留下递归锁或 current context。
    {
      Telemetry telemetry(output/"bindings");
      Window window(200,150,false,&telemetry,2,reinterpret_cast<HWND>(host.winId()));
      try {
        GLContext::Binding first(window.present_context);
        {GLContext::Binding nested(window.present_context);}
        check(wglGetCurrentContext()==window.present_context.handle(),"嵌套释放提前解除上下文");
        throw std::runtime_error("作用域退出测试");
      } catch(const std::runtime_error &e) {check(std::string(e.what())=="作用域退出测试",e.what());}
      check(wglGetCurrentContext()==nullptr,"异常退出残留 current context");
      window.present_context.inject(GLContext::Fault::activate_once);
      try {GLContext::Binding binding(window.present_context);check(false,"故障注入未触发");}
      catch(const GraphicsError &) {}
      window.recreate_contexts();
      {GLContext::Binding binding(window.present_context);check(wglGetCurrentContext()!=nullptr,"重建后不能绑定");}
    }

    auto document=std::make_shared<Document>();document->generation=17;
    auto &scene=document->loaded.scene;
    ir::Material material;material.id="surface";scene.materials={material};
    ir::Mesh mesh;mesh.id="plane";mesh.material_slots={"Surface"};
    mesh.positions={{-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1}};
    ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};mesh.triangles={a,b};scene.meshes={mesh};
    ir::Instance instance;instance.id="plane";instance.materials={0};scene.instances={instance};
    runtime::Target target;target.id="plane";target.label="plane";document->catalog.targets={target};document->formulas.graphs.resize(1);
    const std::array<float,6> view{.1f,0,.1f,4,.1f,.05f};
    nlohmann::json results=nlohmann::json::array();
    auto pump=[&](double seconds) {const auto end=now()+seconds;while(now()<end) {app.processEvents();QThread::msleep(10);}};
    auto different=[](const QImage &a,const QImage &b) {
      size_t count=0;for(int y=0;y<std::min(a.height(),b.height());++y) for(int x=0;x<std::min(a.width(),b.width());++x)
        if(a.pixel(x,y)!=b.pixel(x,y)) ++count;
      return count;
    };
    for(const std::string name:{"present-lost","upload-lost","release-failed","create-retry","persistent","shutdown-lost"}) {
      const auto directory=output/name;std::filesystem::create_directories(directory);
      auto snapshot=initial_snapshot(*document);snapshot.revision=7;
      snapshot.material_overrides["plane"]["Surface"]["base_color"]=nlohmann::json::array({.01,.05,1.});
      const auto original=snapshot_json(*document,snapshot);
      SamplingSettings settings;settings.samples=32;settings.adaptive_threshold=0;
      {
        Renderer renderer(reinterpret_cast<HWND>(host.winId()),host.width(),host.height(),directory,settings);
        renderer.automated_pointer();renderer.quality({80,Reconstruction::bicubic});
        renderer.set_document(document,snapshot,false);renderer.camera_view(view);renderer.select(document->generation,0,-1,{{0,-1}});
        auto ready=[&](uint64_t recoveries) {
          const auto start=now();
          while(now()-start<90) {
            app.processEvents();const auto state=renderer.status();
            check(!state.graphics_blocked,name+": "+state.error);
            check(state.error.empty()||state.graphics_recovering,name+": "+state.error);
            if(!state.graphics_recovering&&state.graphics_recoveries>=recoveries&&state.generation==document->generation&&
               state.applied_revision==snapshot.revision&&state.presented_revision==snapshot.revision&&
               state.requested_epoch==state.presented_epoch&&!state.preview&&state.samples==32) {
              check(state.adapter.triangles==2,"恢复丢失几何");
              check(state.selected_target==0,"恢复丢失选择");
              check(state.quality.percent==80&&state.render_width==336&&state.render_height==240,"恢复丢失分辨率设置");
              const auto &c=state.camera;
              check(std::abs(c.target.x-view[0])<1e-5&&std::abs(c.target.z-view[2])<1e-5&&std::abs(c.distance-view[3])<1e-5&&std::abs(c.yaw-view[4])<1e-5&&std::abs(c.pitch-view[5])<1e-5,"恢复丢失相机");
              pump(.08);return state;
            }
            QThread::msleep(10);
          }
          throw std::runtime_error(name+": 恢复超时");
        };
        ready(0);const auto before=secondary->grabWindow(host.winId()).toImage();
        before.save(QString::fromStdWString((directory/"before.png").wstring()));
        if(name=="shutdown-lost") {
          renderer.inject_graphics_failure(true,GLContext::Fault::context_lost);
          results.push_back({{"case",name},{"shutdown","pending"}});
        } else {
          if(name=="present-lost") renderer.inject_graphics_failure(false,GLContext::Fault::context_lost);
          if(name=="upload-lost") {renderer.inject_graphics_failure(true,GLContext::Fault::context_lost);renderer.camera_view(view);}
          if(name=="release-failed") renderer.inject_graphics_failure(false,GLContext::Fault::release_once);
          if(name=="create-retry") {
            renderer.inject_graphics_failure(true,GLContext::Fault::create_once);
            renderer.inject_graphics_failure(false,GLContext::Fault::activate_once);
          }
          if(name=="persistent") {
            renderer.inject_graphics_failure(false,GLContext::Fault::persistent);
            const auto start=now();while(!renderer.status().graphics_blocked&&now()-start<30) pump(.02);
            auto blocked=renderer.status();check(blocked.graphics_blocked&&blocked.graphics_attempts==2,"持续故障未限制到两次自动恢复");
            pump(.6);check(renderer.status().sessions==blocked.sessions&&renderer.status().graphics_attempts==2,"持续故障仍在循环重建");
            snapshot.material_overrides["plane"]["Surface"]["base_color"]=nlohmann::json::array({1.,.01,.01});
            ++snapshot.revision;renderer.edit(snapshot);pump(.1);
            check(renderer.status().graphics_blocked,"普通编辑绕过了自动恢复次数限制");
            renderer.inject_graphics_failure(false,GLContext::Fault::none);renderer.restart_render();
            // 重启请求由呈现线程消费；先等它接收请求，避免把上一轮 blocked 状态当作重启失败。
            const auto restart_begin=now();while(renderer.status().graphics_blocked&&now()-restart_begin<2)pump(.01);
          }
          auto state=ready(1);
          check(state.sessions==2,"恢复未正确销毁并重建 Cycles 会话");
          if(name=="create-retry") check(state.graphics_attempts==2,"上下文创建失败后未再次重试");
          const auto after=secondary->grabWindow(host.winId()).toImage();
          after.save(QString::fromStdWString((directory/"after.png").wstring()));
          const auto changed=different(before,after);
          if(name=="persistent") check(changed>100,"手动恢复未应用故障期间的最新材质修改");
          else {
            check(snapshot_json(*document,snapshot)==original,"恢复改写了场景快照");
            check(changed<size_t(before.width()*before.height()/100),"恢复后画面或选区与原图不一致");
          }
          results.push_back({{"case",name},{"sessions",state.sessions},{"recoveries",state.graphics_recoveries},{"attempts",state.graphics_attempts},{"changed_pixels",changed}});
        }
      }
      if(name=="shutdown-lost") results.back()["shutdown"]="PASS";
      std::ofstream(output/"checks.json")<<results.dump(2);pump(.1);
    }
    check(scene.materials[0]==material,"恢复修改了源场景材质");
    std::ofstream(output/"result.json")<<nlohmann::json{{"result","PASS"},{"cases",results.size()}}.dump(2);
    std::cout<<"GL 故障恢复 GPU 验证：PASS\n";return 0;
  } catch(const std::exception &e) {
    std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;
  }
}
