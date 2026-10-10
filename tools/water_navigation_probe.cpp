#include "cycles/runtime_paths.h"
#include "editor/renderer.h"
#include "editor/scene_extension.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <QApplication>
#include <QThread>
#include <QScreen>
#include <fstream>
#include <iostream>
using namespace dfv;using namespace dfv::editor;using J=nlohmann::json;
// 真实场景只读诊断：同一路径比较原地转向、平移及冻结网格/简化材质。
int main(int argc,char **argv){QApplication app(argc,argv);try{
  if(argc<4)throw std::runtime_error("WaterNavigationProbe scene.dufex project.json output");
  std::cout.setf(std::ios::unitbuf);const auto output=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(output);
  J project;std::ifstream(argv[2])>>project;std::vector<std::filesystem::path> roots;for(const auto &v:project.at("content_roots"))roots.push_back(std::filesystem::u8path(v.get<std::string>()));
  std::cout<<"Loading scene\n";auto restored=load_scene_extension(std::filesystem::u8path(argv[1]),roots,1);auto &d=restored.document;auto snapshot=restored.snapshot;
  if(d->waters.empty())throw std::runtime_error("Scene has no native water");
  auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);ccl::path_init(app.applicationDirPath().toStdString(),cycles_user_directory());
  QWidget host;host.resize(960,600);host.show();app.processEvents();
  SamplingSettings sampling;sampling.samples=128;sampling.adaptive_threshold=0;sampling.update_interval_seconds=.03;
  Renderer renderer(reinterpret_cast<HWND>(host.winId()),host.width(),host.height(),output,sampling);renderer.resize(host.width(),host.height());renderer.automated_pointer();renderer.set_document(d,snapshot,false);
  auto view=snapshot.view.value_or(std::array<float,6>{0,0,0,4,0,.4f});view[5]=.6f;renderer.camera_view(view);
  auto pump=[&](double seconds){const auto start=now();while(now()-start<seconds){app.processEvents();QThread::msleep(1);}};
  auto ready=[&]{const auto start=now();double stable=0;uint64_t epoch=0;for(;;){pump(.005);const auto s=renderer.status();if(!s.error.empty()||!s.edit_error.empty())throw std::runtime_error(s.error+s.edit_error);if(s.generation==1&&!s.preview&&s.samples>=4&&s.presented_epoch==s.requested_epoch&&s.camera.epoch==renderer.input_camera().epoch){if(epoch!=s.requested_epoch){epoch=s.requested_epoch;stable=now();}if(now()-stable>.5)return s;}if(now()-start>240)throw std::runtime_error("Frame timeout");}};
  auto s=ready();std::cout<<"Scene ready; triangles="<<s.adapter.triangles<<"\n";
  J report;for(const auto &w:water::effective(*d,snapshot))report["water"].push_back(water::json(*w));report["source"]=argv[1];report["view"]=view;
  auto run=[&](const std::string &name,bool turn,bool continuous){
    renderer.camera_view(view);ready();CameraState camera=renderer.input_camera();const auto before=renderer.status();const auto begin=now();renderer.trace((name+"_begin").c_str());std::cout<<name<<"\n";
    J gaps=J::array();double last=now();uint64_t epoch=before.presented_epoch;const int steps=continuous?100:12;const double interval=continuous?.03:.45;
    for(int i=0;i<steps;++i){if(turn)camera.look(i%2==0?3.f:-3.f,0);else camera.target.y+=i<steps/2?.25f:-.25f;
      renderer.camera_view({camera.target.x,camera.target.y,camera.target.z,camera.distance,camera.yaw,camera.pitch});const auto requested=now();
      while(now()-requested<interval){pump(.001);const auto next=renderer.status();if(next.presented_epoch!=epoch){gaps.push_back((now()-last)*1000);last=now();epoch=next.presented_epoch;}}
    }
    const auto moved=now();const auto end=ready();renderer.trace((name+"_end").c_str());
    report["cases"].push_back({{"name",name},{"begin",begin},{"motion_end",moved},{"end",now()},{"gaps_ms",gaps},{"settle_ms",(now()-moved)*1000},{"topology_updates",end.adapter.topology_updates-before.adapter.topology_updates},{"geometry_updates",end.adapter.geometry_updates-before.adapter.geometry_updates},{"origin_updates",end.adapter.origin_updates-before.adapter.origin_updates},{"gpu_bytes",end.gpu_device_bytes}});
    std::ofstream(output/"report.json")<<report.dump(2);host.screen()->grabWindow(host.winId()).save(QString::fromStdString((output/(name+".png")).string()));
    if(name.starts_with("manual")&&end.adapter.topology_updates!=before.adapter.topology_updates)throw std::runtime_error("Manual water changed topology during camera navigation");
  };
  for(int mode=0;mode<3;++mode){RenderProbe probe;probe.serial=mode+1;probe.freeze_water_lod=mode!=0;probe.matte_water=mode==2;renderer.render_probe(probe);pump(.3);ready();const std::string name=mode==0?"auto":mode==1?"frozen":"frozen-matte";run(name+"-turn",true,true);run(name+"-move",false,true);run(name+"-steps",false,false);}
  {RenderProbe probe;probe.serial=4;renderer.render_probe(probe);pump(.3);ready();
    auto waters=water::effective(*d,snapshot);snapshot.water_overrides.clear();
    for(const auto &w:waters){auto manual=std::make_shared<water::Water>(*w);manual->config.manual_lod=true;manual->config.lod_level=0;
      const auto layout=water::lod_layout(manual->config);report["manual_lod0"]={{"triangles",layout.triangles},{"raw_bytes",layout.bytes}};
      try{water::check_manual_lod_budget(*manual);report["manual_lod0"]["budget"]="available";}catch(const std::exception &e){report["manual_lod0"]["budget"]=e.what();}
      manual->config.lod_level=4;snapshot.water_overrides.push_back(manual);}
    const auto topology=renderer.status().adapter.topology_updates;auto moving=view;moving[1]+=.25f;renderer.camera_view(moving);
    ++snapshot.revision;renderer.edit(snapshot);pump(.025);renderer.camera_view(view);pump(.3);const auto changed=ready();
    if(changed.adapter.topology_updates==topology)throw std::runtime_error("Manual LOD edit during navigation was dropped");
    run("manual4-turn",true,true);run("manual4-move",false,true);run("manual4-steps",false,false);
  }
  std::cout<<"Water navigation profile complete\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
