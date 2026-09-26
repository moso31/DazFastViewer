#include "editor/content_locator.h"
#include "editor/content_catalog.h"
#include "daz/documents.h"
#include "daz/loader.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <map>

namespace dfv::editor {
namespace {
using J=nlohmann::json;
using Reference=std::pair<QString,QString>;
QString field(const J &j,const char *key) {const auto i=j.find(key);return i!=j.end()&&i->is_string()?QString::fromStdString(i->get<std::string>()):QString{};}
QString clean_name(QString name) {name.remove(QRegularExpression("\\s*\\(\\d+\\)$"));return name.toCaseFolded().remove(QRegularExpression("[^\\p{L}\\p{N}]"));}
Reference reference(const QString &uri,const QString &owner,const QStringList &roots) {
  const auto hash=uri.indexOf('#');if(hash<0)return {};
  const auto file=QString::fromStdString(daz::decode_uri(uri.left(hash).toStdString()));
  const auto id=QString::fromStdString(daz::decode_uri(uri.mid(hash+1).toStdString()));
  if(file.isEmpty())return {content_path(owner).toCaseFolded(),id};
  QStringList candidates;if(!file.startsWith('/'))candidates.append(QFileInfo(owner).path()+"/"+file);
  for(const auto &root:roots)candidates.append(root+"/"+(file.startsWith('/')?file.mid(1):file));
  for(const auto &candidate:candidates)if(QFileInfo(candidate).isFile())return {content_path(candidate).toCaseFolded(),id};
  return {};
}
std::shared_ptr<const J> read(const QString &file) {return daz::document_view(std::filesystem::path(file.toStdWString()));}
bool equal_reference(const Reference &a,const Reference &b) {return !a.first.isEmpty()&&a==b;}
}

QString resolve_content_entry(const ContentOrigin &origin,const QStringList &roots,const QStringList &entries,const std::function<bool()> &cancel) {
  auto cancelled=[&]{return cancel&&cancel();};
  std::vector<Reference> node_refs,geometry_refs;QStringList names{origin.label,origin.node};
  for(const auto &[file,id]:origin.geometries){geometry_refs.emplace_back(content_path(file).toCaseFolded(),id);names.append(QFileInfo(file).completeBaseName());names.append(QFileInfo(file).dir().dirName());}
  // 读取原始节点标签，仍能找到在场景中被重命名的对象和追加后的对象。
  try {
    auto scene=read(origin.scene_file);
    for(const auto &node:daz::array_member(daz::object_member(*scene,"scene"),"nodes"))if(field(node,"id")==origin.node){
      names.append(field(node,"label"));auto ref=reference(field(node,"url"),origin.scene_file,roots);
      for(int depth=0;!ref.first.isEmpty()&&depth<16;++depth){
        if(std::find(node_refs.begin(),node_refs.end(),ref)!=node_refs.end())break;node_refs.push_back(ref);
        const auto doc=read(ref.first);Reference next;
        for(const auto &base:daz::array_member(*doc,"node_library"))if(field(base,"id")==ref.second){names.append(field(base,"label"));names.append(field(base,"name"));next=reference(field(base,"source"),ref.first,roots);break;}
        ref=next;
      }
      break;
    }
  }catch(const std::exception &){}
  QSet<QString> exact,tokens;
  for(const auto &name:names){const auto cleaned=clean_name(name);if(!cleaned.isEmpty())exact.insert(cleaned);for(const auto &token:name.toCaseFolded().split(QRegularExpression("[^\\p{L}\\p{N}]+"),Qt::SkipEmptyParts))if(token.size()>=3)tokens.insert(token);}
  struct Candidate {QString path;int rank;};std::vector<Candidate> candidates;QSet<QString> seen;
  auto add=[&](const QString &path){const auto key=content_path(path).toCaseFolded();if(seen.contains(key)||!path.endsWith(".duf",Qt::CaseInsensitive))return;seen.insert(key);
    const auto stem=QFileInfo(path).completeBaseName();int rank=exact.contains(clean_name(stem))?1000:0;
    for(const auto &token:tokens)if(stem.contains(token,Qt::CaseInsensitive))rank+=20;else if(path.contains(token,Qt::CaseInsensitive))rank+=1;
    if(key==content_path(origin.scene_file).toCaseFolded())rank+=2000;if(rank)candidates.push_back({path,rank});};
  add(origin.scene_file);for(const auto &entry:entries)add(entry);
  std::stable_sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.rank>b.rank;});
  QString grouped;
  for(const auto &candidate:candidates){
    if(cancelled())return {};
    try {
      const auto doc=read(candidate.path);const auto type=field(daz::object_member(*doc,"asset_info"),"type");
      if(type.contains("material")||type.contains("shader")||type.contains("pose")||type.contains("properties")||type.contains("shape")||type.contains("layered_image"))continue;
      int models=0;bool matches=false;
      for(const auto &node:daz::array_member(daz::object_member(*doc,"scene"),"nodes")){
        const auto &geometries=daz::array_member(node,"geometries");if(geometries.empty()||daz::selection_reference(field(node,"url").toStdString()))continue;++models;
        const auto ref=reference(field(node,"url"),candidate.path,roots);
        bool match=std::any_of(node_refs.begin(),node_refs.end(),[&](const auto &r){return equal_reference(r,ref);});
        if(node_refs.empty())for(const auto &geometry:geometries){const auto g=reference(field(geometry,"url"),candidate.path,roots);match|=std::any_of(geometry_refs.begin(),geometry_refs.end(),[&](const auto &r){return equal_reference(r,g);});}
        matches|=match;
      }
      if(!matches)continue;
      // 用户保存的场景不是产品入口；仅独立内嵌模型可回到其唯一来源。
      const bool saved_scene=type=="scene"||type=="preset_scene"||type=="preset_scene_subset";
      if(saved_scene){if(models==1&&candidate.path.compare(origin.scene_file,Qt::CaseInsensitive)==0&&std::all_of(node_refs.begin(),node_refs.end(),[&](const auto &r){return r.first==content_path(origin.scene_file).toCaseFolded();}))return candidate.path;continue;}
      if(models==1)return candidate.path;if(grouped.isEmpty())grouped=candidate.path;
    }catch(const std::exception &){}
  }
  return grouped;
}
}
