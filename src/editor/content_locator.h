#pragma once
#include <QStringList>
#include <functional>
#include <utility>
#include <vector>

namespace dfv::editor {
// 场景容器、节点身份和几何来源分别保留；不能把场景文件当作模型入口。
struct ContentOrigin {
  QString scene_file,node,label;
  std::vector<std::pair<QString,QString>> geometries;
};
QString resolve_content_entry(const ContentOrigin &origin,const QStringList &roots,const QStringList &entries,const std::function<bool()> &cancel={});
}
