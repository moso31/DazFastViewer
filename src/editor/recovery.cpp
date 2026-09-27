#include "editor/recovery.h"
#include "editor/scene_extension.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUuid>
#include <cmath>

namespace dfv::editor {
namespace {
using J=nlohmann::json;
std::string path_text(const std::filesystem::path &p){auto v=p.generic_u8string();return {v.begin(),v.end()};}
J selection(const EditSelection &v){return {{"object",v.object},{"joint",v.joint},{"light",v.light},{"special",v.special}};}
EditSelection selection(const J &j){return {j.at("object"),j.at("joint"),j.at("light"),j.at("special")};}
void write(const QString &path,const J &data){
  const auto body=data.dump();QSaveFile output(path);output.setDirectWriteFallback(false);
  if(!output.open(QIODevice::WriteOnly)||output.write(body.data(),qint64(body.size()))!=qint64(body.size())||!output.commit())throw std::runtime_error((QStringLiteral("无法写入恢复文件：")+output.errorString()).toStdString());
}
}
J recovery_json(const EditState &s,const std::vector<std::filesystem::path> &roots){
  if(!s.document)throw std::runtime_error("没有可恢复的场景");
  auto snapshot=s.snapshot;snapshot.generation=s.document->generation;
  J j={{"schema","dfv-recovery"},{"version",1},{"scene",scene_extension_json(*s.document,snapshot)},
    {"save_file",path_text(s.save_file)},{"manual",s.manual},{"pending",J::array()},{"roots",J::array()},
    {"selection",J::array()},{"active",selection(s.context.active)},{"surfaces",s.context.surfaces},{"raw_morphs",J::object()}};
  for(const auto &root:roots)j["roots"].push_back(path_text(root));
  if(s.document->loaded.report.contains("content_roots"))for(const auto &root:s.document->loaded.report["content_roots"])if(std::find(j["roots"].begin(),j["roots"].end(),root)==j["roots"].end())j["roots"].push_back(root);
  for(const auto &[key,v]:s.pending)j["pending"].push_back({{"target",key.first},{"morph",key.second},{"value",v.value},{"unlimited",v.unlimited}});
  for(const auto &v:s.context.selection)j["selection"].push_back(selection(v));
  // DUFEX 面向可移植保存；恢复点额外保留原始通道，含尚未应用的输入。
  for(size_t t=0;t<s.snapshot.values.size();++t){const auto &target=s.document->catalog.targets.at(t);auto &m=j["raw_morphs"][target.id]=J::object();
    for(size_t i=0;i<target.morphs.size();++i){const float v=s.snapshot.values[t].morphs.at(i);if(!std::isfinite(v))throw std::runtime_error("恢复点包含无效参数");if(v!=target.morphs[i].initial)m[target.morphs[i].id]=v;}}
  return j;
}
EditState restore_recovery(const J &j,uint64_t generation,const std::function<void(const std::string &)> &progress){
  if(j.at("schema")!="dfv-recovery"||j.at("version")!=1)throw std::runtime_error("恢复文件版本不受支持");
  std::vector<std::filesystem::path> roots;for(const auto &v:j.at("roots"))roots.push_back(std::filesystem::u8path(v.get<std::string>()));
  auto restored=restore_scene_extension(j.at("scene"),{},roots,generation,progress);EditState s;s.document=restored.document;s.snapshot=std::move(restored.snapshot);
  s.save_file=std::filesystem::u8path(j.at("save_file").get<std::string>());s.manual=j.at("manual");
  for(const auto &[id,values]:j.at("raw_morphs").items()){
    const auto &targets=s.document->catalog.targets;auto t=std::find_if(targets.begin(),targets.end(),[&](const auto &v){return v.id==id;});if(t==targets.end())throw std::runtime_error("恢复对象已不存在");
    for(const auto &[name,value]:values.items()){auto m=std::find_if(t->morphs.begin(),t->morphs.end(),[&](const auto &v){return v.id==name;});if(m==t->morphs.end())throw std::runtime_error("恢复参数已不存在："+name);const float v=value.get<float>();if(!std::isfinite(v))throw std::runtime_error("恢复参数无效");s.snapshot.values[size_t(t-targets.begin())].morphs[size_t(m-t->morphs.begin())]=v;}
  }
  for(const auto &v:j.at("pending")){
    const auto target=v.at("target").get<std::string>(),morph=v.at("morph").get<std::string>();bool valid=false;
    for(const auto &t:s.document->catalog.targets)if(t.id==target)for(const auto &m:t.morphs)if(m.id==morph)valid=true;
    const float value=v.at("value");if(!valid||!std::isfinite(value))throw std::runtime_error("暂存参数无法恢复");s.pending[{target,morph}]={value,v.at("unlimited")};
  }
  for(const auto &v:j.at("selection"))s.context.selection.push_back(selection(v));s.context.active=selection(j.at("active"));
  s.context.surfaces=j.at("surfaces").get<decltype(s.context.surfaces)>();return s;
}
RecoverySession::RecoverySession(const QString &directory){
  if(!QDir().mkpath(directory))throw std::runtime_error("无法创建恢复目录");
  file_=QDir(directory).filePath("session-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".json");
  lock_=std::make_unique<QLockFile>(file_+".lock");lock_->setStaleLockTime(0);
  if(!lock_->tryLock())throw std::runtime_error("无法锁定恢复文件");worker_=std::thread([this]{run();});
}
RecoverySession::~RecoverySession(){
  {std::lock_guard lock(mutex_);stopping_=true;}ready_.notify_all();if(worker_.joinable())worker_.join();
}
J RecoverySession::read(const QString &file){QFile input(file);if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("无法读取恢复文件");const auto body=input.readAll();return J::parse(body.constData(),body.constData()+body.size());}
QStringList RecoverySession::candidates(const QString &directory){
  QStringList out;for(const auto &file:QDir(directory).entryInfoList({"session-*.json"},QDir::Files,QDir::Time)){
    QLockFile lock(file.absoluteFilePath()+".lock");lock.setStaleLockTime(0);if(!lock.tryLock())continue;
    try{const auto data=read(file.absoluteFilePath());if(!data.value("clean_exit",false)&&!data.value("dismissed",false))out.push_back(file.absoluteFilePath());}catch(...){/* 原子文件损坏时保留原文件，不覆盖其他可用恢复点。 */}
  }return out;
}
void RecoverySession::dismiss(const QString &file){QLockFile lock(file+".lock");lock.setStaleLockTime(0);if(!lock.tryLock())throw std::runtime_error("恢复文件仍在使用");auto data=read(file);data["dismissed"]=true;write(file,data);}
void RecoverySession::checkpoint(EditState state,std::vector<std::filesystem::path> roots,bool clean){
  if(!state.document)return;{std::lock_guard lock(mutex_);pending_=Request{std::move(state),std::move(roots),++serial_,clean};}ready_.notify_one();
}
bool RecoverySession::flush(){std::unique_lock lock(mutex_);const auto serial=serial_;written_.wait(lock,[&]{return completed_>=serial;});return error_.isEmpty();}
QString RecoverySession::error(){std::lock_guard lock(mutex_);return error_;}
void RecoverySession::run(){
  for(;;){Request request;{std::unique_lock lock(mutex_);ready_.wait(lock,[&]{return stopping_||pending_.has_value();});if(!pending_){if(stopping_)return;continue;}request=std::move(*pending_);pending_.reset();}
    QString error;try{auto data=recovery_json(request.state,request.roots);data["clean_exit"]=request.clean;data["updated"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();write(file_,data);
      if(request.clean)for(const auto &old:QDir(QFileInfo(file_).absolutePath()).entryInfoList({"session-*.json"},QDir::Files)){if(old.absoluteFilePath()==file_)continue;QLockFile lock(old.absoluteFilePath()+".lock");lock.setStaleLockTime(0);if(!lock.tryLock())continue;try{const auto previous=read(old.absoluteFilePath());if(previous.value("clean_exit",false))QFile::remove(old.absoluteFilePath());}catch(...){}}
    }catch(const std::exception &e){error=QString::fromUtf8(e.what());}
    {std::lock_guard lock(mutex_);completed_=request.serial;error_=error;}written_.notify_all();
  }
}
}
