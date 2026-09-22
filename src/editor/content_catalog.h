#pragma once
#include <QObject>
#include <QSettings>
#include <QThreadPool>
#include <QStringList>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

namespace dfv::editor {
QString content_path(const QString &path);
QString content_category(const nlohmann::json &document);
QStringList content_categories();
QString category_label(const QString &category);
QStringList preview_candidates(const QString &asset,bool tooltip=false);

struct RecentContent {QString path,category;qint64 used=0;};
class ContentHistory {
  std::unique_ptr<QSettings> settings_;
  std::vector<RecentContent> recent_;
  QStringList searches_;
  void save_recent();
public:
  explicit ContentHistory(const QString &settings_file={});
  QSettings &settings() {return *settings_;}
  const QStringList &searches() const {return searches_;}
  void searched(const QString &query);
  void used(const QString &path,const QString &category,qint64 time=0);
  std::vector<RecentContent> recent(const QString &category="all") const;
};

struct ContentEntry {QString path,folded;};
using ContentEntries=std::vector<ContentEntry>;
struct ContentSearch {QStringList paths;double milliseconds=0;};
// 查询只读取不可变快照；扫描和查询使用独立线程池，取消旧请求不会阻塞界面。
class ContentIndex final:public QObject {
  QThreadPool scanner_,queries_;
  std::atomic<uint64_t> scan_generation_=0,query_generation_=0;
  std::shared_ptr<const ContentEntries> entries_=std::make_shared<ContentEntries>();
  QString cache_directory_;
  bool scanning_=false;
public:
  explicit ContentIndex(const QString &cache_directory,QObject *parent=nullptr);
  ~ContentIndex() override;
  std::function<void()> changed;
  std::function<void(const QString &)> progress;
  void refresh(const QStringList &roots,bool read_cache=false);
  void search(const QString &query,const QString &scope,std::function<void(ContentSearch)> complete);
  void cancel_search();
  size_t size() const {return entries_->size();}
  bool scanning() const {return scanning_;}
  static ContentSearch match(const ContentEntries &entries,const QString &query,const QString &scope={},const std::function<bool()> &cancel={});
};
}
