#include "cycles/runtime_paths.h"
#include "editor/renderer.h"
#include "powerpose_fixture.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <QApplication>
#include <QScreen>
#include <QThread>
#include <fstream>
#include <iostream>
using namespace dfv;using namespace dfv::editor;
static void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
#include "joint_region_renderer.inl"
int main(int argc,char **argv){
  QApplication app(argc,argv);if(app.arguments().contains("--joint-regions"))return joint_region_renderer(app);
  const auto output=std::filesystem::absolute("artifacts/content-interaction/gpu");std::filesystem::create_directories(output);
  try{
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);ccl::path_init(app.applicationDirPath().toStdString(),dfv::cycles_user_directory());
    auto d=std::make_shared<Document>();d->generation=1;auto &scene=d->loaded.scene;ir::Material material;material.id="surface";scene.materials={material};
    ir::Mesh mesh;mesh.id="mesh";mesh.material_slots={"Surface"};mesh.positions={{.75f,0,-.2f},{1.2f,0,-.2f},{1.2f,0,.2f},{.75f,0,.2f}};mesh.triangles={{{0,1,2}},{{0,2,3}}};scene.meshes={mesh};
    ir::Instance instance;instance.id="figure/mesh";instance.materials={0};scene.instances={instance};runtime::Target target;target.id=instance.id;target.label="figure";d->catalog.targets={target};
    auto skin=powerpose_fixture();const auto original=skin.joints;skin.joints={original[0]};for(const auto *name:{"lShldr","lForeArm","lHand"})for(const auto &joint:original)if(joint.name==name)skin.joints.push_back(joint);
    for(size_t i=1;i<original.size();++i)if(original[i].name!="lShldr"&&original[i].name!="lForeArm"&&original[i].name!="lHand")skin.joints.push_back(original[i]);const int shoulder=1,fore=2,hand=3;
    skin.joints[fore].parent=shoulder;skin.joints[hand].parent=fore;skin.joints[shoulder].end_cm={40,0,0};skin.joints[fore].center_cm={40,0,0};skin.joints[fore].end_cm={80,0,0};skin.joints[hand].center_cm={80,0,0};skin.joints[hand].end_cm={120,0,0};
    skin.weights.assign(4,{{uint32_t(hand),1}});d->skeletons.skins={skin};d->formulas.graphs.resize(1);d->formulas.graphs[0].skin=0;
    daz::AssetObject object;object.id="figure";object.figure=true;object.content_type="Actor/Character";object.auto_fit_base="/Genesis 8/Female";d->loaded.objects={object};
    auto snapshot=initial_snapshot(*d);QScreen *secondary=QGuiApplication::primaryScreen();for(auto *screen:QGuiApplication::screens())if(screen!=QGuiApplication::primaryScreen())secondary=screen;check(secondary,"No available display");
    QWidget host;host.resize(700,520);host.move(secondary->availableGeometry().topLeft()+QPoint(30,30));host.show();app.processEvents();
    SamplingSettings settings;settings.samples=16;settings.adaptive_threshold=0;settings.interaction_probe=true;
    Renderer renderer(reinterpret_cast<HWND>(host.winId()),host.width(),host.height(),output,settings);renderer.resize(host.width(),host.height());renderer.automated_pointer();renderer.set_document(d,snapshot,false);renderer.camera_view({.95f,0,0,2.5f,0,0});renderer.select(1,0,-1,{{0,-1}},true);
    auto wait=[&](auto predicate,const char *message,double seconds=10){const auto start=now();while(now()-start<seconds){app.processEvents();auto s=renderer.status();check(s.error.empty(),s.error.c_str());check(s.edit_error.empty(),s.edit_error.c_str());if(predicate(s))return s;QThread::msleep(1);}throw std::runtime_error(message);};
    auto full=[&]{return wait([&](const auto &s){return s.generation==1&&s.applied_revision==snapshot.revision&&s.presented_revision==snapshot.revision&&!s.pose_restoring&&!s.preview&&s.samples>=4;},"最终画面未恢复",90);};
    full();nlohmann::json checks=nlohmann::json::array();
    auto changed=[&]{++snapshot.revision;renderer.edit(snapshot);};
    auto cpu=[&]{return wait([&](const auto &s){return s.applied_revision==snapshot.revision&&s.pose_restoring;},"首帧等待期间参数没有应用");};
    // 模拟 Morph / ERC 同一次求值改变网格、姿势及层次可见性。
    renderer.interaction(true);snapshot.values[0].visible=false;snapshot.poses[0][hand].rotation_degrees.z=10;changed();auto hidden=cpu();check(!hidden.visible[0],"白模显隐没有更新");
    renderer.interaction(false);snapshot.values[0].visible=true;changed();cpu();
    const auto start=now();renderer.interaction(true);snapshot.poses[0][hand].rotation_degrees.z=20;changed();auto latest=cpu();check(latest.visible[0]&&latest.presented_revision<snapshot.revision,"等待期复编辑错误显示旧帧");
    checks.push_back({{"case","visibility_and_fk_reedit"},{"latency_ms",(now()-start)*1000},{"revision",snapshot.revision}});renderer.interaction(false);full();
    uint64_t serial=0;
    auto powerpose=[&](bool expect_wait){
      const auto before=renderer.status();if(expect_wait)check(before.pose_restoring,"未在首帧等待期间重新拖动");
      runtime::PowerPoseInput input;input.serial=++serial;input.event=1;input.generation=1;input.revision=snapshot.revision;input.skin=input.target=0;input.instance=skin.id;input.point="B09";input.held=input.moved=true;input.dy=8;input.input_seconds=now();renderer.powerpose(input);
      const auto proxy=wait([&](const auto &s){return s.pose_dragging&&s.pose_powerpose&&s.pose_previews>before.pose_previews;},"等待首帧时无法再次拖动 PowerPose");
      checks.push_back({{"case",expect_wait?"powerpose_reedit":"powerpose"},{"latency_ms",proxy.pose_latency_ms}});
      input.held=false;++input.event;renderer.powerpose(input);const auto committed=wait([&](const auto &s){return s.pose_commit>before.pose_commit;},"姿态未提交");check(committed.pose_revision==snapshot.revision,"姿态提交版本过期");snapshot.poses[0]=committed.pose_input;changed();cpu();
    };
    powerpose(false);powerpose(true);full();
    auto hwnd=FindWindowExW(HWND(host.winId()),nullptr,L"DfvCyclesBench",nullptr);check(hwnd,"缺少原生视口");
    auto mouse=[&](UINT message,int x,int y){SendMessageW(hwnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,MAKELPARAM(x,y));};
    renderer.gizmo({GizmoTool::rotate,GizmoSpace::local});renderer.select(1,0,hand,{{0,hand}},true);
    auto gizmo=[&](bool expect_wait){
      auto before=wait([](const auto &s){return s.gizmo_available&&!s.gizmo_shape.lines.empty();},"没有 FK 手柄");if(expect_wait)check(before.pose_restoring,"FK 未在首帧等待期间验证");
      const auto line=before.gizmo_shape.lines.front();int x=int((line.a.x+line.b.x)/2),y=int((line.a.y+line.b.y)/2);mouse(WM_LBUTTONDOWN,x,y);QThread::msleep(20);mouse(WM_MOUSEMOVE,x+24,y+20);
      auto proxy=wait([&](const auto &s){return s.pose_dragging&&s.pose_gizmo&&s.pose_previews>before.pose_previews;},"首帧等待时无法再次拖动 FK");checks.push_back({{"case",expect_wait?"fk_reedit":"fk"},{"latency_ms",proxy.pose_latency_ms}});
      mouse(WM_LBUTTONUP,x+24,y+20);const auto committed=wait([&](const auto &s){return s.pose_commit>before.pose_commit;},"FK 未提交");snapshot.poses[0]=committed.pose_input;changed();cpu();
    };
    gizmo(false);gizmo(true);full();
    renderer.gizmo({});renderer.select(1,0,-1,{{0,-1}},true);snapshot.poses[0]=skin.initial;changed();full();
    auto ik=[&](bool expect_wait){
      const auto before=renderer.status();if(expect_wait)check(before.pose_restoring,"IK 未在首帧等待期间验证");
      const int x=host.width()/2,y=host.height()/2;mouse(WM_LBUTTONDOWN,x,y);QThread::msleep(20);mouse(WM_MOUSEMOVE,x+8,y+12);
      const auto proxy=wait([&](const auto &s){return s.pose_dragging&&!s.pose_gizmo&&!s.pose_powerpose&&s.pose_previews>before.pose_previews;},"首帧等待时无法再次拖动 IK");checks.push_back({{"case",expect_wait?"ik_reedit":"ik"},{"latency_ms",proxy.pose_latency_ms}});
      mouse(WM_LBUTTONUP,x+8,y+12);const auto committed=wait([&](const auto &s){return s.pose_commit>before.pose_commit;},"IK 未提交");snapshot.poses[0]=committed.pose_input;changed();cpu();
    };
    ik(false);ik(true);const auto final=full();check(final.sessions==1,"连续编辑重建了会话");check(final.presented_revision==snapshot.revision,"显示了过期编辑帧");
    std::ofstream(output/"result.json")<<nlohmann::json{{"result","PASS"},{"checks",checks},{"sessions",final.sessions},{"revision",snapshot.revision}}.dump(2);return 0;
  }catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}
}
