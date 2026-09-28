#pragma once
#include <QString>
#include <QStringList>
#include <functional>
#include "editor/application_settings.h"
#include "runtime/physics_settings.h"
class QWidget;
namespace dfv::editor {
struct ProjectSettings {
  QString file;
  QStringList content_roots;
  int history_limit=50;
  runtime::PhysicsOptions physics;
  static ProjectSettings load(const QString &file);
  static QStringList normalize(const QStringList &roots);
  void save() const;
};
bool edit_project_settings(QWidget *parent,ProjectSettings &settings,ApplicationSettings &application,const QString &application_file={},bool persistent=true,const std::function<void(bool saved)> &applied={});
}
