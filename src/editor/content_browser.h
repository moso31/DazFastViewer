#pragma once
#include <QWidget>
#include <QStringList>
#include <functional>
#include <memory>
#include "editor/content_locator.h"
namespace dfv::editor {
class ContentBrowser final:public QWidget {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool eventFilter(QObject *object,QEvent *event) override;
public:
  explicit ContentBrowser(QWidget *parent=nullptr,const QString &settings_file={},const QString &cache_directory={});
  ~ContentBrowser() override;
  std::function<void(const QString &)> open_asset;
  std::function<void(const QString &,bool)> apply_pose;
  void set_roots(const QStringList &roots);
  void record_use(const QString &path,const QString &category);
  void saved_scene(const QString &path);
  void show_recent();
  bool locate(const QString &path);
  void locate_asset(const ContentOrigin &origin);
  void save();
};
}
