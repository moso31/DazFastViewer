#include "editor/material_panel.h"
#include "editor/scene_extension.h"
#include <QApplication>
#include <QFontDatabase>
#include <QElapsedTimer>
#include <QTreeWidget>
#include <QThreadPool>
#include <fstream>
#include <iostream>
#include <windows.h>
#include <psapi.h>
using namespace dfv;
int main(int argc,char **argv){QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");app.setFont(QFont(QStringLiteral("Microsoft YaHei"),9));try{
  if(argc<3)throw std::runtime_error("需要场景路径和组标签");
  auto document=std::make_shared<editor::Document>();document->generation=1;
  daz::LoadOptions options;for(int a=4;a<argc;++a)options.content_roots.push_back(std::filesystem::u8path(argv[a]));
  if(std::string(argv[2])=="--roundtrip"){
    if(argc<4)throw std::runtime_error("保存重开验证需要输出报告路径");
    auto restored=editor::load_scene_extension(std::filesystem::u8path(argv[1]),options.content_roots,1);
    auto &state=restored.snapshot;const auto owner=restored.document->catalog.targets.empty()?"@environment":restored.document->catalog.targets[0].id;
    state.parameter_settings[owner]["ground/GroundAlignmentOffset"]={true,-20,40,.025};state.parameter_settings[owner]["parameters//probe"]={false,0,1,.5};
    state.parameter_settings["@environment"]["parameters//Exposure Value"]={true,-12,24,.25};
    auto saved=std::filesystem::u8path(argv[3]);saved.replace_extension("dufex");
    editor::save_scene_extension(saved,*restored.document,state);auto reopened=editor::load_scene_extension(saved,options.content_roots,2);
    if(reopened.snapshot.parameter_settings!=state.parameter_settings)throw std::runtime_error("范围或精度保存重开不一致");
    nlohmann::json result={{"result","PASS"},{"input",argv[1]},{"saved",saved.generic_string()},{"parameter_settings",reopened.snapshot.parameter_settings}};
    std::ofstream(std::filesystem::u8path(argv[3]))<<result.dump(2);std::cout<<result.dump(2)<<std::endl;return 0;
  }
  document->loaded=daz::load(std::filesystem::u8path(argv[1]),options);
  for(const auto &o:document->loaded.objects){runtime::Target t;t.id=o.id;t.label=o.label;t.instance=o.instance;t.parent=o.parent;document->catalog.targets.push_back(t);document->formulas.graphs.emplace_back();}
  std::string group;for(const auto &n:document->loaded.nodes)if(n.label==argv[2])group=n.id;
  if(group.empty())throw std::runtime_error("找不到组");auto snapshot=editor::initial_snapshot(*document);
  editor::MaterialPanel panel;panel.resize(760,850);panel.show();app.processEvents();
  PROCESS_MEMORY_COUNTERS_EX before{},after{};before.cb=after.cb=sizeof(before);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&before),sizeof(before));
  QElapsedTimer timer;timer.start();panel.bind(document,&snapshot,-4,group);const auto bind_ms=timer.nsecsElapsed()/1e6;
  app.processEvents();GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&after),sizeof(after));
  nlohmann::json result={{"group",group},{"selected_surfaces",panel.selected_surfaces().size()},{"bind_ms",bind_ms},{"resolved_materials",panel.property("resolvedMaterialCount").toULongLong()},{"texture_table_copies",panel.property("textureTableCopies").toInt()},{"private_bytes_delta",int64_t(after.PrivateUsage)-int64_t(before.PrivateUsage)}};
  std::cout<<result.dump(2)<<std::endl;if(argc>3){std::ofstream(std::filesystem::u8path(argv[3]))<<result.dump(2);panel.grab().save(QString::fromUtf8(argv[3])+".png");}
  QThreadPool::globalInstance()->waitForDone();return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
