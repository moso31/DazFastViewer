#pragma once
#include "viewport/quality.h"
#include "viewport/render_quality.h"
#include <QString>
#include <QMap>

namespace dfv::editor {
struct ApplicationSettings {
  int ui_percent=100;
  ViewportQuality viewport;
  RenderQuality render;
  int settings_tab=0;
  QMap<QString,bool> expanded;
  static ApplicationSettings load(const QString &file={});
  void save(const QString &file={}) const;
  bool operator==(const ApplicationSettings &) const=default;
};
}
