#pragma once
#include <QList>
#include <QRect>
#include <algorithm>

namespace dfv::editor {
// All rectangles use Qt logical pixels, including mixed-DPI and negative screen coordinates.
inline QRect visible_window_geometry(QRect window,const QList<QRect> &screens,QRect preferred) {
  if(screens.isEmpty()) return window;
  QRect available=preferred.isValid()?preferred:screens.front();
  qint64 largest=0;
  for(const auto &screen:screens) {
    const QRect overlap=window.intersected(screen);
    const qint64 area=qint64(overlap.width())*overlap.height();
    if(area>largest) {largest=area;available=screen;}
  }
  window.setSize(window.size().boundedTo(available.size()));
  if(!largest) window.moveCenter(available.center());
  window.moveLeft(std::clamp(window.left(),available.left(),available.right()-window.width()+1));
  window.moveTop(std::clamp(window.top(),available.top(),available.bottom()-window.height()+1));
  return window;
}
}
