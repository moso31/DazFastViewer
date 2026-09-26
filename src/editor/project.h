#pragma once
#include <QString>
#include <QStringList>
#include <functional>
#include "editor/application_settings.h"
class QWidget;
namespace dfv::editor {
struct ProjectSettings {
  QString file;
  QStringList content_roots;
  static ProjectSettings load(const QString &file);
  static QStringList normalize(const QStringList &roots);
  void save() const;
};
bool edit_project_settings(QWidget *parent,ProjectSettings &settings,ApplicationSettings &application,const QString &application_file={},bool persistent=true,const std::function<void(bool saved)> &applied={});
}
