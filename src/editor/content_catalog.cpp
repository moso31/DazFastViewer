#include "editor/content_catalog.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <filesystem>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>

namespace dfv::editor {
QString content_path(const QString &path) {return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));}
QStringList content_categories() {return {"all","scene","shape","pose","material","character","wearable","light","camera","properties","other"};}
QString category_label(const QString &category) {
  static const QMap<QString,QString> labels={{"all",QStringLiteral("所有（ALL）")},{"scene",QStringLiteral("场景（Scene）")},{"shape",QStringLiteral("形态（Shape）")},{"pose",QStringLiteral("姿势（Pose）")},{"material",QStringLiteral("材质（Material）")},{"character",QStringLiteral("角色（Character）")},{"wearable",QStringLiteral("穿戴（Wearable）")},{"light",QStringLiteral("灯光（Light）")},{"camera",QStringLiteral("相机（Camera）")},{"properties",QStringLiteral("参数（Properties）")},{"other",QStringLiteral("其他（Other）")}};
  return labels.value(category,labels["other"]);
}
QString content_category(const nlohmann::json &document) {
  using J=nlohmann::json;
  const auto info=document.find("asset_info");std::string type;
  if(info!=document.end()&&info->is_object()&&info->contains("type")&&(*info)["type"].is_string()) type=(*info)["type"].get<std::string>();
  const auto t=QString::fromStdString(type).toLower();
  if(t=="scene"||t=="preset_scene"||t=="preset_scene_subset") return "scene";
  if(t.contains("shape")||t.contains("morph")) return "shape";
  if(t.contains("pose")) return "pose";
  if(t.contains("material")||t.contains("shader")) return "material";
  if(t.contains("character")) return "character";
  if(t.contains("wearable")) return "wearable";
  if(t.contains("light")) return "light";
  if(t.contains("camera")) return "camera";
  if(t.contains("propert")||t.contains("render")) return "properties";
  // 无类型声明时仅使用实际内容；不根据目录名误判用户移动过的资产。
  const auto scene=document.find("scene");if(scene==document.end()||!scene->is_object()) return "other";
  auto populated=[&](const char *key) {auto v=scene->find(key);return v!=scene->end()&&v->is_array()&&!v->empty();};
  if(populated("nodes")) for(const auto &node:(*scene)["nodes"]) if(node.is_object()) {
    if(node.contains("geometries")&&!node["geometries"].empty()) return "scene";
    if(node.value("type","")=="light") return "light";
    if(node.value("type","")=="camera") return "camera";
  }
  if(populated("materials")) return "material";
  if(populated("animations")) return "properties";
  return "other";
}
QStringList preview_candidates(const QString &asset,bool tooltip) {
  const QFileInfo file(asset);const auto base=file.path()+"/"+file.completeBaseName();
  const QStringList icons={asset+".png",base+".png"},tips={base+".tip.png",asset+".tip.png"};
  return tooltip?tips+icons:icons+tips;
}
ContentHistory::ContentHistory(const QString &file):settings_(file.isEmpty()?std::make_unique<QSettings>():std::make_unique<QSettings>(file,QSettings::IniFormat)) {
  searches_=settings_->value("content/searches").toStringList();searches_=searches_.mid(0,10);
  const auto array=QJsonDocument::fromJson(settings_->value("content/recent").toByteArray()).array();
  for(const auto &v:array) {const auto o=v.toObject();const auto p=o["path"].toString(),c=o["category"].toString();if(QDir::isAbsolutePath(p)&&content_categories().contains(c)&&c!="all") recent_.push_back({content_path(p),c,qint64(o["used"].toDouble())});}
  std::stable_sort(recent_.begin(),recent_.end(),[](const auto &a,const auto &b){return a.used>b.used;});
}
void ContentHistory::searched(const QString &query) {
  const auto q=query.trimmed();if(q.isEmpty()) return;
  for(qsizetype i=searches_.size();i-->0;) if(searches_[i].compare(q,Qt::CaseInsensitive)==0) searches_.removeAt(i);
  searches_.prepend(q);searches_=searches_.mid(0,10);settings_->setValue("content/searches",searches_);settings_->sync();
}
void ContentHistory::used(const QString &path,const QString &category,qint64 time) {
  const auto p=content_path(path);auto c=category;if(c=="all"||!content_categories().contains(c)) c="other";
  if(!time) time=QDateTime::currentMSecsSinceEpoch();if(!recent_.empty()) time=std::max(time,recent_.front().used+1);
  std::erase_if(recent_,[&](const auto &r){return r.path.compare(p,Qt::CaseInsensitive)==0;});recent_.insert(recent_.begin(),{p,c,time});
  // 保留全局最近 100 项与每类最近 20 项的并集，避免热门分类挤掉其他分类。
  std::map<QString,int> counts;size_t rank=0;
  std::erase_if(recent_,[&](const auto &r){const int in_category=++counts[r.category];return rank++>=100&&in_category>20;});save_recent();
}
void ContentHistory::save_recent() {
  QJsonArray array;for(const auto &r:recent_) array.append(QJsonObject{{"path",r.path},{"category",r.category},{"used",double(r.used)}});
  settings_->setValue("content/recent",QJsonDocument(array).toJson(QJsonDocument::Compact));settings_->sync();
}
std::vector<RecentContent> ContentHistory::recent(const QString &category) const {
  std::vector<RecentContent> result;for(const auto &r:recent_) if(category=="all"||r.category==category) {result.push_back(r);if(result.size()==(category=="all"?100:20)) break;}return result;
}

ContentIndex::ContentIndex(const QString &cache,QObject *parent):QObject(parent),cache_directory_(cache) {scanner_.setMaxThreadCount(1);queries_.setMaxThreadCount(1);}
ContentIndex::~ContentIndex() {++scan_generation_;++query_generation_;scanner_.clear();queries_.clear();scanner_.waitForDone();queries_.waitForDone();}
void ContentIndex::cancel_search() {++query_generation_;queries_.clear();}
ContentSearch ContentIndex::match(const ContentEntries &entries,const QString &query,const QString &scope,const std::function<bool()> &cancel) {
  QElapsedTimer timer;timer.start();ContentSearch result;const auto q=QDir::fromNativeSeparators(query.trimmed()).toCaseFolded();
  auto prefix=scope.isEmpty()?QString():content_path(scope).toCaseFolded();if(!prefix.isEmpty()&&!prefix.endsWith('/')) prefix+='/';
  for(size_t i=0;i<entries.size();++i) {if((i&1023)==0&&cancel&&cancel()) return {};const auto &e=entries[i];if((prefix.isEmpty()||e.folded.startsWith(prefix))&&e.folded.contains(q)) result.paths.append(e.path);}
  result.milliseconds=timer.nsecsElapsed()/1000000.0;return result;
}
void ContentIndex::search(const QString &query,const QString &scope,std::function<void(ContentSearch)> complete) {
  cancel_search();const auto generation=query_generation_.load();const auto entries=entries_;
  queries_.start([this,entries,query,scope,generation,complete=std::move(complete)] {
    auto result=match(*entries,query,scope,[&]{return generation!=query_generation_;});if(generation!=query_generation_) return;
    QMetaObject::invokeMethod(this,[this,generation,result=std::move(result),complete] {if(generation==query_generation_) complete(result);},Qt::QueuedConnection);
  });
}
void ContentIndex::refresh(const QStringList &input,bool read_cache) {
  QStringList roots;for(const auto &root:input) {const auto p=content_path(root);if(!roots.contains(p,Qt::CaseInsensitive)) roots.append(p);}
  const auto generation=++scan_generation_;scanner_.clear();scanning_=true;
  if(read_cache) {cancel_search();entries_=std::make_shared<ContentEntries>();if(changed) changed();}
  const auto key=QCryptographicHash::hash(roots.join('\n').toCaseFolded().toUtf8(),QCryptographicHash::Sha256).toHex();
  const auto cache=cache_directory_+"/"+key+".index";
  if(progress) progress(QStringLiteral("正在后台扫描内容库…"));
  scanner_.start([this,roots,generation,cache,read_cache] {
    auto cancelled=[&]{return generation!=scan_generation_;};
    auto publish=[&](std::shared_ptr<ContentEntries> entries,bool done,const QString &message) {
      QMetaObject::invokeMethod(this,[this,generation,entries,done,message] {if(generation!=scan_generation_) return;entries_=entries;scanning_=!done;if(progress) progress(message);if(changed) changed();},Qt::QueuedConnection);
    };
    if(read_cache) {
      QFile file(cache);if(file.size()<256*1024*1024&&file.open(QIODevice::ReadOnly)) {
        QDataStream stream(&file);stream.setVersion(QDataStream::Qt_6_10);quint32 magic=0,count=0;QStringList saved_roots;stream>>magic>>saved_roots>>count;
        if(magic==0x44465631&&saved_roots==roots&&count<2000000) {auto cached=std::make_shared<ContentEntries>();cached->reserve(count);
          for(quint32 i=0;i<count&&!cancelled()&&stream.status()==QDataStream::Ok;++i) {QString p;stream>>p;cached->push_back({p,p.toCaseFolded()});}
          if(!cancelled()&&stream.status()==QDataStream::Ok&&cached->size()==count) publish(cached,false,QStringLiteral("已载入 %1 个资源，正在检查目录变化…").arg(count));
        }
      }
    }
    auto entries=std::make_shared<ContentEntries>();QSet<QString> seen;QStringList unavailable;QElapsedTimer timer;timer.start();qint64 last_progress=0;
    namespace fs=std::filesystem;
    for(const auto &root:roots) {
      if(cancelled()) return;std::error_code error;const fs::path path(root.toStdWString());
      if(!fs::is_directory(path,error)) {unavailable.append(root);continue;}
      fs::recursive_directory_iterator it(path,fs::directory_options::skip_permission_denied,error),end;
      while(it!=end&&!cancelled()) {
        const auto item=*it;const auto qpath=QString::fromStdWString(item.path().wstring());
        if(item.is_directory(error)) {if(QFileInfo(qpath).isSymbolicLink()||QFileInfo(qpath).isJunction()) it.disable_recursion_pending();}
        else if(QString::fromStdWString(item.path().extension().wstring()).compare(".duf",Qt::CaseInsensitive)==0&&item.is_regular_file(error)) {
          auto p=QDir::fromNativeSeparators(qpath),folded=p.toCaseFolded();if(!seen.contains(folded)) {seen.insert(folded);entries->push_back({std::move(p),std::move(folded)});}
        }
        error.clear();it.increment(error);if(error) {unavailable.append(root);break;}
        if(timer.elapsed()-last_progress>1500) {last_progress=timer.elapsed();const auto count=entries->size();QMetaObject::invokeMethod(this,[this,generation,count]{if(generation==scan_generation_&&progress) progress(QStringLiteral("后台扫描中：已发现 %1 个资源").arg(count));},Qt::QueuedConnection);}
      }
    }
    if(cancelled()) return;
    std::sort(entries->begin(),entries->end(),[](const auto &a,const auto &b){return a.folded<b.folded;});
    if(cancelled()) return;
    QDir().mkpath(QFileInfo(cache).path());QSaveFile file(cache);
    if(file.open(QIODevice::WriteOnly)) {QDataStream stream(&file);stream.setVersion(QDataStream::Qt_6_10);stream<<quint32(0x44465631)<<roots<<quint32(entries->size());for(const auto &e:*entries) {if(cancelled()) {file.cancelWriting();return;}stream<<e.path;}if(stream.status()==QDataStream::Ok) file.commit();}
    auto message=QStringLiteral("%1 个资源 · 索引已更新").arg(entries->size());if(!unavailable.empty()) message+=QStringLiteral(" · 部分目录不可访问：")+unavailable.join("；");
    publish(entries,true,message);
  });
}
}
