#pragma once
#include <QWidget>
#include <QStringList>
#include <functional>
#include <memory>
namespace dfv::editor {
class ContentBrowser final:public QWidget {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool eventFilter(QObject *object,QEvent *event) override;
public:
  explicit ContentBrowser(QWidget *parent=nullptr,const QString &settings_file={},const QString &cache_directory={});
  ~ContentBrowser() override;
  std::function<void(const QString &)> open_asset;
  void set_roots(const QStringList &roots);
  void record_use(const QString &path,const QString &category);
  void show_recent();
  void save();
};
}
