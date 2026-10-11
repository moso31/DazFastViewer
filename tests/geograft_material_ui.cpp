#include "editor/geograft_material_ui.h"
#include "editor/scene_extension.h"
#include <QTest>
#include <QTemporaryDir>
#include <fstream>
#include <iostream>
using namespace dfv::editor;using J=nlohmann::json;namespace fs=std::filesystem;
int main(int argc,char **argv){QApplication app(argc,argv);try{
  if(argc>1&&std::string(argv[1])=="--cancel-leaf"){Sleep(60000);return 0;}
  if(argc>2&&std::string(argv[1])=="--cancel-worker"){const auto executable=fs::path(QCoreApplication::applicationFilePath().toStdWString());std::wstring command=L"\""+executable.wstring()+L"\" --cancel-leaf";STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION p{};if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&start,&p))return 1;std::ofstream(fs::u8path(argv[2]))<<p.dwProcessId;CloseHandle(p.hProcess);CloseHandle(p.hThread);Sleep(60000);return 0;}
  if(argc>2&&(std::string(argv[1])=="--update"||std::string(argv[1])=="--update-create")){
    const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};
    std::cout<<"Loading real HD update scene"<<std::endl;auto loaded=load_scene_extension(fs::u8path(argv[2]),roots,1);auto &d=*loaded.document;auto &s=loaded.snapshot;
    size_t target=d.catalog.targets.size();for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].label=="Seo Hyun G8.1F (2)")for(auto graft:geograft_targets(d,t))if(geograft_material_strategy(d,graft)==GeograftMaterialStrategy::hd_nipples)target=graft;
    if(target==d.catalog.targets.size())throw std::runtime_error("缺少目标角色的 HD 附件");
    if(std::string(argv[1])=="--update-create"){
      const auto host=attachment_host(d,target);const auto instance=d.catalog.targets[host].instance;const auto &object=d.loaded.scene.instances[instance];const auto &body_slots=d.loaded.scene.meshes[object.mesh].material_slots;const auto body=std::find(body_slots.begin(),body_slots.end(),"Body");if(body==body_slots.end())throw std::runtime_error("角色缺少 Body");
      auto texture=copy_material(d.loaded.scene,s.material_overrides,{instance,size_t(body-body_slots.begin())}).at("parameters").at("color_texture");const auto original=fs::u8path(texture.at("file").get<std::string>());
      const auto renamed=fs::absolute("artifacts/geograft-update")/("uncached-body-"+std::to_string(QDateTime::currentMSecsSinceEpoch())+original.extension().string());fs::create_directories(renamed.parent_path());fs::copy_file(original,renamed);texture["file"]=renamed.generic_string();s.material_overrides[object.id]["Body"]["color_texture"]=texture;
    }
    const auto cache=fs::absolute("artifacts/geograft-update/hd-cache");int dialogs=0;QTimer accept;accept.setInterval(50);QObject::connect(&accept,&QTimer::timeout,[&]{if(auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());dialog&&dialog->objectName()=="HDBakeOptions"){++dialogs;dialog->accept();}});accept.start();
    std::cout<<"Preparing HD textures"<<std::endl;const auto directory=prepare_hd_update(nullptr,d,s,target,roots,cache);accept.stop();if(!directory)throw std::runtime_error("更新意外取消");
    if(std::string(argv[1])=="--update-create"&&dialogs!=1)throw std::runtime_error("新皮肤没有进入创建纹理流程");
    const auto result=update_geograft_materials(d,s,target,roots,*directory);if(result.updated!=5||result.fallback||!result.unresolved.empty())throw std::runtime_error("真实 HD 更新未完成："+result.strategy_error);
    const auto cached=prepare_hd_update(nullptr,d,s,target,roots,cache);if(cached!=directory)throw std::runtime_error("第二次更新没有复用当前皮肤缓存");
    const auto again=update_geograft_materials(d,s,target,roots,*cached);if(again.updated!=5||again.fallback)throw std::runtime_error("重复应用 HD 纹理失败");
    const auto saved=fs::absolute("artifacts/geograft-update/hd-updated.dufex");save_scene_extension(saved,d,s);auto restored=load_scene_extension(saved,roots,2);const auto instance=d.catalog.targets[target].instance;
    for(size_t slot=0;slot<d.loaded.scene.instances[instance].materials.size();++slot)if(copy_material(d.loaded.scene,s.material_overrides,{instance,slot})!=copy_material(restored.document->loaded.scene,restored.snapshot.material_overrides,{instance,slot}))throw std::runtime_error("HD 更新保存重载不一致");
    std::ofstream(fs::absolute("artifacts/geograft-update/hd-result.json"))<<J{{"status","PASS"},{"updated",result.updated},{"bake_dialogs",dialogs},{"directory",directory->generic_string()},{"cache_reused",true},{"save_reload",true}}.dump(2);std::cout<<"PASS real HD update / cache / repeat / save-reload\n";return 0;
  }
  if(argc>1){std::ifstream in(fs::u8path(argv[1]));J j;in>>j;HDBakeJob job;job.directory=fs::u8path(j.at("bake_directory").get<std::string>());job.executable=fs::u8path(j.at("executable").get<std::string>());job.identity=j.at("identity");for(const auto &p:j.at("inputs"))job.inputs.push_back(fs::u8path(p.get<std::string>()));for(const auto &p:j.at("arguments"))job.arguments.push_back(fs::u8path(p.get<std::string>()));if(!run_hd_bake(nullptr,job))throw std::runtime_error("烘焙取消");std::cout<<"PASS bake\n";return 0;}
  QImage source(2,2,QImage::Format_ARGB32);source.setPixel(0,0,qRgb(255,0,0));source.setPixel(1,0,qRgb(0,255,0));source.setPixel(1,1,qRgb(0,0,255));source.setPixel(0,1,qRgb(255,255,255));const std::vector<QPointF> coordinates={{0,0},{4096,0},{4096,4096},{0,4096}};auto mapped=reproject_hd_map(source,coordinates,coordinates);if(mapped.pixel(0,0)!=source.pixel(0,0)||mapped.pixel(4095,4095)!=source.pixel(1,1)||std::abs(qRed(mapped.pixel(2048,2048))-128)>1)throw std::runtime_error("附加贴图 UV 方向或双线性采样错误");if(!reproject_hd_map(source,coordinates,coordinates,[]{return false;}).isNull())throw std::runtime_error("取消后仍继续转换贴图");
  QTemporaryDir temporary;HDBakeJob cancel_job;cancel_job.directory=fs::path(temporary.path().toStdWString());cancel_job.executable=fs::path(QCoreApplication::applicationFilePath().toStdWString());const auto pidfile=cancel_job.directory/"child.txt";cancel_job.arguments={"--cancel-worker",pidfile};QTimer cancel_timer;cancel_timer.setInterval(50);int ticks=0;QObject::connect(&cancel_timer,&QTimer::timeout,[&]{if(fs::exists(pidfile)||++ticks>100)for(auto *widget:QApplication::topLevelWidgets())if(auto *progress=qobject_cast<QProgressDialog *>(widget))progress->cancel();});cancel_timer.start();if(run_hd_bake(nullptr,cancel_job))throw std::runtime_error("取消后烘焙仍被提交");cancel_timer.stop();DWORD child=0;std::ifstream(pidfile)>>child;if(!child)throw std::runtime_error("取消测试没有创建子进程");HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,child);if(process){const auto state=WaitForSingleObject(process,5000);CloseHandle(process);if(state!=WAIT_OBJECT_0)throw std::runtime_error("取消后遗留烘焙子进程");}
  bool passed=false;QString error;
  QTimer::singleShot(100,[&]{auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog){app.exit(1);return;}
    auto *size=dialog->findChild<QComboBox *>("HDBakeSize");auto *color=dialog->findChild<QComboBox *>("HDBakeColor");auto *glands=dialog->findChild<QComboBox *>("HDBakeGlands");
    if(size&&color&&glands&&size->currentIndex()==1&&glands->currentIndex()==1&&color->currentIndex()==0){
      auto wheel=[&](int delta){QWheelEvent event(QPointF(5,5),size->mapToGlobal(QPoint(5,5)),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(size,&event);};wheel(-120);
      if(size->currentIndex()!=1)error=QStringLiteral("未选择时滚轮修改选项");else {auto *label=size->parentWidget()->findChild<QLabel *>();QTest::mouseClick(label,Qt::LeftButton);wheel(-120);passed=size->currentIndex()==2;if(!passed)error=QStringLiteral("选中后滚轮未调节选项");}
    }else error=QStringLiteral("默认烘焙选项错误");
    if(qEnvironmentVariableIsSet("DFV_HD_CAPTURE"))dialog->grab().save(qEnvironmentVariable("DFV_HD_CAPTURE"));dialog->accept();
  });auto options=hd_bake_options(nullptr,qEnvironmentVariableIsSet("DFV_HD_CAPTURE")?fs::path("H:/G3"):fs::path{});if(!passed||!options||options->size!="Large")throw std::runtime_error(error.toStdString());
  QTimer::singleShot(100,[]{qobject_cast<QDialog *>(QApplication::activeModalWidget())->reject();});if(hd_bake_options(nullptr))throw std::runtime_error("取消仍提交选项");std::cout<<"PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
