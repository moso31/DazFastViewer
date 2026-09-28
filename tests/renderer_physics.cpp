#include "editor/renderer.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <QApplication>
#include <QScreen>
#include <QThread>
#include <fstream>
#include <iostream>
#include <cstdlib>
using namespace dfv;using namespace dfv::editor;
static void check(bool ok,const char *s){if(!ok)throw std::runtime_error(s);}
static std::shared_ptr<Document> fixture(uint64_t generation){
  auto d=std::make_shared<Document>();d->generation=generation;auto &s=d->loaded.scene;ir::Material material;material.id="surface";material.base_color={.2f,.5f,.8f};s.materials={material};
  ir::Mesh m;m.id="cloth";m.material_slots={"surface"};const uint32_t n=std::getenv("DFV_PHYSICS_DENSE")?1025:129;for(uint32_t y=0;y<n;++y)for(uint32_t x=0;x<n;++x)m.positions.push_back({float(x)/(n-1)-.5f,float(y)/(n-1)-.5f,1+float(y)/(2*(n-1))});
  for(uint32_t y=0;y+1<n;++y)for(uint32_t x=0;x+1<n;++x){const auto a=y*n+x;m.triangles.push_back({{a,a+1,a+n}});m.triangles.push_back({{a+1,a+n+1,a+n}});}s.meshes={m};
  ir::Instance i;i.id="cloth/mesh";i.materials={0};s.instances={i};runtime::Target t;t.id=i.id;t.label="Cloth";d->catalog.targets={t};d->formulas.graphs.resize(1);return d;
}
int main(int argc,char **argv){
  QApplication app(argc,argv);auto output=std::filesystem::absolute(std::getenv("DFV_PHYSICS_DENSE")?"artifacts/physics-controls/gpu-dense":"artifacts/physics-controls/gpu");std::filesystem::create_directories(output);
  try{
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);ccl::path_init(app.applicationDirPath().toStdString(),DFV_CYCLES_SOURCE);
    QWidget host;host.resize(700,520);for(auto *s:QGuiApplication::screens())if(s!=QGuiApplication::primaryScreen())host.move(s->availableGeometry().topLeft()+QPoint(40,40));host.show();app.processEvents();
    SamplingSettings settings;settings.samples=16;settings.adaptive_threshold=0;settings.interaction_probe=true;Renderer renderer(HWND(host.winId()),host.width(),host.height(),output,settings);renderer.resize(host.width(),host.height());renderer.automated_pointer();
    auto d=fixture(1);auto snapshot=initial_snapshot(*d);renderer.set_document(d,snapshot,false);renderer.camera_view({0,0,1.2f,3,.2f,.5f});
    auto wait=[&](auto predicate,const char *message,double seconds=45){const auto start=now();while(now()-start<seconds){app.processEvents();auto s=renderer.status();check(s.error.empty(),s.error.c_str());check(s.edit_error.empty(),s.edit_error.c_str());check(s.physics_error.empty(),s.physics_error.c_str());if(predicate(s))return s;QThread::msleep(2);}throw std::runtime_error(message);};
    auto full=[&]{return wait([&](const auto &s){return s.generation==snapshot.generation&&s.presented_revision==snapshot.revision&&!s.pose_restoring&&!s.preview&&s.samples>=4;},"完整画面超时",120);};
    const auto source_vertices=d->loaded.scene.meshes[0].positions.size();auto base=full();auto hashes=base.mesh_hashes;check(!hashes.empty(),"缺少基础网格校验值");auto edit=[&]{++snapshot.revision;renderer.edit(snapshot);};
    snapshot.values[0].physics.enabled=true;snapshot.values[0].physics.kind=runtime::PhysicsKind::cloth;snapshot.values[0].physics.rounds=600;edit();
    auto enabled_idle=full();check(enabled_idle.requested_epoch==base.requested_epoch,"仅启用物理重置了 OptiX");const auto idle_begin=now();while(now()-idle_begin<.5){app.processEvents();QThread::msleep(5);}check(renderer.status().physics_applied==0,"仅启用物理就重置了渲染");
    runtime::PhysicsOptions options;options.refresh_hz=.5;renderer.physics_options(options);
    ++snapshot.values[0].physics.run_sequence;edit();wait([](const auto &s){return s.physics_busy;},"物理任务没有启动");
    double worst=0;const auto prepare_start=now();int camera_checks=0;
    do{check(now()-prepare_start<90,"后台画面准备超时");const auto begin=now();renderer.orbit(camera_checks%2?1:-1,0);auto c=renderer.input_camera();wait([&](const auto &s){return s.camera.epoch>=c.epoch;},"后台解算或画面准备阻塞相机",2);worst=std::max(worst,(now()-begin)*1000);++camera_checks;QThread::msleep(15);}while(!renderer.status().physics_prepared);
    wait([](const auto &s){return s.physics_applied>=1;},"首个物理帧未提交",120);const auto first_physics_time=now();
    auto solved=wait([](const auto &s){return s.physics_applied>=2&&s.physics.steps>=10&&s.max_displacement>.01;},"物理结果未提交",120);check(solved.max_displacement>.01,"物理没有改变衣摆");check(solved.mesh_hashes!=hashes,"物理几何校验未变化");
    check(now()-first_physics_time>=1.8,"项目物理刷新率没有约束提交间隔");options.refresh_hz=4;options.paused=true;renderer.physics_options(options);
    wait([](const auto &s){return !s.physics_busy;},"暂停没有结束画面提交",120);auto frozen=full();
    const auto freeze_begin=now();while(now()-freeze_begin<.4){app.processEvents();QThread::msleep(5);}check(renderer.status().mesh_hashes==frozen.mesh_hashes,"暂停时画面没有保持当前状态");
    host.screen()->grabWindow(host.winId()).save(QString::fromStdString((output/"settled.png").string()));
    ++snapshot.values[0].physics.reset_sequence;edit();wait([&](const auto &s){return s.mesh_hashes==hashes&&!s.physics_busy;},"重置按钮没有恢复初始物理状态",120);full();
    options.paused=false;renderer.physics_options(options);snapshot.values[0].physics.rounds=4;++snapshot.values[0].physics.run_sequence;const auto step_base=renderer.status().physics.steps;edit();
    wait([&](const auto &s){return s.physics.steps==step_base+4&&!s.physics_busy&&s.mesh_hashes!=hashes;},"模拟 N 轮未在指定轮数停止",120);const auto stopped=full();
    const auto stopped_begin=now();while(now()-stopped_begin<.8){app.processEvents();QThread::msleep(5);}check(renderer.status().requested_epoch==stopped.requested_epoch&&renderer.status().physics_applied==stopped.physics_applied,"停止后空物理帧仍重置渲染");
    snapshot.values[0].physics.damping+=1;edit();wait([&](const auto &s){return s.mesh_hashes==hashes&&!s.physics_busy;},"修改物理参数没有恢复初始状态",120);full();snapshot.values[0].physics.rounds=600;
    snapshot.values[0].physics.enabled=false;edit();wait([&](const auto &s){return s.mesh_hashes==hashes;},"关闭物理未呈现基础状态",120);auto restored=full();check(restored.mesh_hashes==hashes&&restored.max_displacement<1e-6,"关闭物理没有恢复基础跟随");
    options.paused=false;renderer.physics_options(options);snapshot.values[0].physics.enabled=true;++snapshot.values[0].physics.run_sequence;edit();wait([](const auto &s){return s.physics_committing;},"未进入异步提交",120);
    QThread::msleep(30);app.processEvents();const auto retained=host.screen()->grabWindow(host.winId()).toImage();const auto pixel=retained.pixelColor(retained.width()/2,retained.height()/2);
    check(pixel.blue()>pixel.red()+10,"静止相机下物理提交没有保留完整彩色画面");retained.save(QString::fromStdString((output/"retained-during-commit.png").string()));
    for(int k=0;k<8;++k){const auto begin=now();renderer.orbit(1,0);auto c=renderer.input_camera();wait([&](const auto &s){return s.camera.epoch>=c.epoch;},"提交物理阻塞相机",2);worst=std::max(worst,(now()-begin)*1000);}
    const auto click_count=renderer.status().clicks;renderer.pointer(host.width()/2,host.height()/2,true);wait([&](const auto &s){return s.clicks>click_count&&s.hit_target==0;},"提交期间不能选择物体",.4);
    snapshot.values[0].physics.enabled=false;snapshot.values[0].transform.translation_cm.x=10;edit();
    auto immediate=wait([&](const auto &s){return s.applied_revision==snapshot.revision;},"提交期间参数编辑被延迟",.4);check(immediate.physics_committing,"未在提交持锁期间验证编辑");
    wait([&](const auto &s){return s.mesh_hashes==hashes;},"取消过程中物理未恢复",120);restored=full();check(restored.mesh_hashes==hashes,"提交中取消留下过期结果");check(std::abs(restored.target_world[0].value[3]-.1f)<1e-5,"旧结果覆盖了新位置");
    options.paused=false;renderer.physics_options(options);snapshot.values[0].physics.enabled=true;++snapshot.values[0].physics.run_sequence;edit();wait([](const auto &s){return s.physics_committing;},"第二次异步提交未启动",120);
    d=fixture(2);snapshot=initial_snapshot(*d);renderer.set_document(d,snapshot,false);auto switched=full();check(switched.mesh_hashes==hashes&&switched.physics_applied==0,"场景切换被旧物理覆盖");
    check(worst<250,"物理期间相机延迟超过 250ms");
    std::ofstream(output/"result.json")<<nlohmann::json{{"result","PASS"},{"source_vertices",source_vertices},{"camera_checks",camera_checks},{"camera_max_ms",worst},{"particles",solved.physics.particles},{"prepare_ms",solved.physics.prepare_ms},{"solve_ms",solved.physics.solve_ms},{"max_displacement_m",solved.max_displacement},{"checks",{"manual_start_only","finite_rounds","reset_to_initial","parameter_change_resets","no_op_preserves_optix","configured_refresh_rate","continuous_frames","pause_freezes_state","retain_beauty_during_commit","camera_during_solve_prepare_and_commit","selection_during_commit","parameter_edit_during_commit","disable_restore","cancel_during_commit","switch_document_during_commit"}}}.dump(2);return 0;
  }catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}
}
