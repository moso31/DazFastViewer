#pragma once
#include <QString>
#include <QStringList>
class QWidget;
namespace dfv::editor {
struct ProjectSettings {
  QString file;
  QStringList content_roots;
  static ProjectSettings load(const QString &file);
  static QStringList normalize(const QStringList &roots);
  void save() const;
};
bool edit_project_settings(QWidget *parent,ProjectSettings &settings);
}
