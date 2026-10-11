#pragma once
#include "editor/geograft_materials.h"
#include "editor/hd_auxiliary_maps.h"
#include "editor/parameter_widgets.h"
#include <QDialogButtonBox>
#include <QProgressDialog>
#include <QDateTime>
#include <QImageReader>
#include <QStandardPaths>
#include <QInputDialog>
#include <windows.h>

namespace dfv::editor {
inline std::filesystem::path hd_cache_directory(){return std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdWString())/"hd-textures";}
inline std::optional<HDBakeOptions> hd_bake_options(QWidget *parent,const std::filesystem::path &product_root={}){
  QDialog dialog(parent);dialog.setWindowTitle(QStringLiteral("创建 HD Nipples 纹理"));dialog.setObjectName("HDBakeOptions");
  auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;layout->addLayout(form);auto *context=new parameter_widgets::Context(&dialog);context->wheel_selection();
  auto add=[&](const char *name,const QString &label,const QStringList &choices){auto *row=new QWidget(&dialog);auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);auto *text=new QLabel(label,row);auto *combo=new QComboBox(row);combo->setObjectName(name);combo->addItems(choices);row->setProperty("numericRow",QString::fromLatin1(name));row->setProperty("numericLabel",QVariant::fromValue<QObject *>(text));line->addWidget(text);line->addWidget(combo,1);form->addRow(row);return combo;};
  auto *color=add("HDBakeColor",QStringLiteral("颜色"),{QStringLiteral("颜色 1"),QStringLiteral("颜色 2"),QStringLiteral("颜色 3")});
  auto *size=add("HDBakeSize",QStringLiteral("乳晕大小"),{QStringLiteral("小"),QStringLiteral("中"),QStringLiteral("大"),QStringLiteral("特大")});size->setCurrentIndex(1);
  auto *glands=add("HDBakeGlands",QStringLiteral("腺体"),{QStringLiteral("无"),QStringLiteral("不明显"),QStringLiteral("明显")});glands->setCurrentIndex(1);
  if(!product_root.empty()){
    auto *previews=new QHBoxLayout;layout->addLayout(previews);auto *color_image=new QLabel(&dialog),*size_image=new QLabel(&dialog),*gland_image=new QLabel(&dialog);for(auto *image:{color_image,size_image,gland_image}){image->setAlignment(Qt::AlignCenter);previews->addWidget(image);}
    const auto textures=product_root/"Runtime/Textures/3feetwolf/HDNipplesforG8F-2.0";
    auto update=[=]{const std::array<std::string,4> sizes={"Small","Medium","Large","Huge"};const std::array<std::string,3> types={"NoGlands","NotObviousGlands","ObviousGlands"};auto set=[](QLabel *label,const std::filesystem::path &p){QPixmap image(QString::fromStdWString(p.wstring()));label->setPixmap(image.scaled(110,110,Qt::KeepAspectRatio,Qt::SmoothTransformation));};set(color_image,textures/"Templates"/("DefaultD_"+std::to_string(color->currentIndex()+1)+"_Thumbnail.png"));set(size_image,textures/"Masks"/(sizes.at(size_t(size->currentIndex()))+"MaskThumbnail.png"));set(gland_image,textures/"Templates"/types.at(size_t(glands->currentIndex()))/"Thumbnail.png");};
    for(auto *combo:{color,size,glands})QObject::connect(combo,&QComboBox::currentIndexChanged,&dialog,[update]{update();});update();
  }
  auto *note=new QLabel(QStringLiteral("使用当前角色的皮肤生成左右各 7 张 4K 贴图。完成后可通过 Apply HD Textures 应用。"),&dialog);note->setWordWrap(true);layout->addWidget(note);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始烘焙"));layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  if(dialog.exec()!=QDialog::Accepted)return {};const std::array<std::string,4> sizes={"Small","Medium","Large","Huge"};const std::array<std::string,3> types={"NoGlands","NotObviousGlands","ObviousGlands"};return HDBakeOptions{color->currentIndex()+1,sizes.at(size_t(size->currentIndex())),types.at(size_t(glands->currentIndex()))};
}
inline bool run_hd_bake(QWidget *parent,const HDBakeJob &job){
  QProgressDialog progress(QStringLiteral("正在准备当前角色的皮肤贴图…"),QStringLiteral("取消"),0,14,parent);progress.setWindowTitle(QStringLiteral("烘焙 HD 纹理"));progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);progress.setAutoClose(false);progress.setAutoReset(false);progress.setValue(0);
  for(size_t i=0;i<job.inputs.size();++i){QApplication::processEvents();if(progress.wasCanceled())return false;QImageReader reader(QString::fromStdWString(job.inputs[i].wstring()));auto img=reader.read();if(img.isNull())throw std::runtime_error("无法读取烘焙源贴图："+job.inputs[i].string());if(img.size()!=QSize(4096,4096))img=img.scaled(4096,4096,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    if(!img.save(QString::fromStdWString((job.directory/("input-"+std::to_string(i)+".jpg")).wstring()),"JPG",100))throw std::runtime_error("无法写入烘焙输入贴图");}
  // Windows Job 在取消或异常时结束整个工具进程树；挂起启动避免子进程逃出 Job。
  struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
  Handle group{CreateJobObjectW(nullptr,nullptr)};if(!group.value)throw std::runtime_error("无法创建烘焙任务");JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;if(!SetInformationJobObject(group.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))throw std::runtime_error("无法设置烘焙任务");
  SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};Handle log{CreateFileW((job.directory/"process.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};if(log.value==INVALID_HANDLE_VALUE)throw std::runtime_error("无法写入烘焙日志");
  Handle input{CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
  STARTUPINFOW start{};start.cb=sizeof(start);start.dwFlags=STARTF_USESTDHANDLES;start.hStdOutput=log.value;start.hStdError=log.value;start.hStdInput=input.value;PROCESS_INFORMATION process{};
  auto quote=[](const std::filesystem::path &p){return L"\""+p.wstring()+L"\"";};std::wstring command=quote(job.executable);for(const auto &a:job.arguments)command+=L" "+quote(a);
  if(!CreateProcessW(job.executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,job.directory.c_str(),&start,&process))throw std::runtime_error("无法启动产品烘焙程序");Handle running{process.hProcess},thread{process.hThread};
  if(!AssignProcessToJobObject(group.value,running.value)){TerminateProcess(running.value,1);throw std::runtime_error("无法管理产品烘焙进程");}if(ResumeThread(thread.value)==DWORD(-1))throw std::runtime_error("无法运行产品烘焙程序");
  progress.setLabelText(QStringLiteral("正在生成 HD 纹理，请稍候…"));QEventLoop loop;QTimer timer;timer.setInterval(250);QObject::connect(&progress,&QProgressDialog::canceled,&loop,&QEventLoop::quit);
  QObject::connect(&timer,&QTimer::timeout,&loop,[&]{if(progress.wasCanceled()){loop.quit();return;}int count=0;for(const auto &item:std::filesystem::directory_iterator(job.directory))if(item.path().filename().string().starts_with("NipplesMap")&&item.path().extension()==".jpg")++count;progress.setValue(count);progress.setLabelText(QStringLiteral("正在生成 HD 纹理：%1 / 14 张").arg(count));if(WaitForSingleObject(running.value,0)==WAIT_OBJECT_0)loop.quit();});timer.start();loop.exec();timer.stop();
  if(progress.wasCanceled()){TerminateJobObject(group.value,1);WaitForSingleObject(running.value,5000);return false;}
  DWORD code=1;GetExitCodeProcess(running.value,&code);if(code!=0)throw std::runtime_error("产品烘焙程序失败，请查看任务目录中的 process.log");
  for(const auto &item:std::filesystem::directory_iterator(job.directory))if(item.path().filename().string().starts_with("NipplesMap")&&item.path().extension()==".jpg"){QImageReader reader(QString::fromStdWString(item.path().wstring()));if(reader.read().isNull()||reader.size()!=QSize(4096,4096))throw std::runtime_error("烘焙结果不是有效的 4K 贴图");}
  progress.setLabelText(QStringLiteral("正在转换附加皮肤贴图…"));if(!write_hd_auxiliary_maps(job,[&]{QApplication::processEvents();return !progress.wasCanceled();}))return false;finish_hd_bake(job);return true;
}
inline std::optional<std::filesystem::path> prepare_hd_update(QWidget *parent,const Document &d,const Snapshot &s,size_t target,const std::vector<std::filesystem::path> &roots,const std::filesystem::path &cache=hd_cache_directory()){
  const auto matches=find_hd_textures(d,s,target,roots,cache);
  const auto task_directory=[&](const char *suffix){return cache/(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz").toStdString()+suffix);};
  if(matches.empty()){
    const auto options=hd_bake_options(parent,hd_product_root(roots));if(!options)return {};
    const auto job=prepare_hd_bake(d,s,target,roots,task_directory("-update"),*options);
    if(!run_hd_bake(parent,job))return {};return job.directory;
  }
  const auto directory=matches.front();
  if(std::filesystem::is_regular_file(directory/"dfv-hd-textures.json"))return directory;
  // 兼容原脚本生成的纹理，同时补齐当前皮肤使用的附加通道。
  const auto job=prepare_hd_bake(d,s,target,roots,task_directory("-import"),{},false);
  for(const auto &item:std::filesystem::directory_iterator(directory))if(item.path().filename().string().starts_with("NipplesMap")&&item.path().extension()==".jpg")std::filesystem::copy_file(item.path(),job.directory/item.path().filename());
  QProgressDialog progress(QStringLiteral("正在转换附加皮肤贴图…"),QStringLiteral("取消"),0,0,parent);progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);
  if(!write_hd_auxiliary_maps(job,[&]{QApplication::processEvents();return !progress.wasCanceled();}))return {};
  finish_hd_bake(job);return job.directory;
}
}
