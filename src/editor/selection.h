#pragma once
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <array>
#include <algorithm>
#include <vector>

namespace dfv::editor {
using Selection = std::array<int,3>; // 对象、骨骼、灯光。
struct DeletionSelection {
  std::vector<size_t> targets,lights;
  std::vector<std::string> nodes;
  bool empty() const {return targets.empty()&&lights.empty()&&nodes.empty();}
};
inline DeletionSelection tree_deletion_selection(QTreeWidget *tree) {
  DeletionSelection result;
  for(auto *item:tree->selectedItems()){
    const auto target=item->data(0,Qt::UserRole).toInt(),light=item->data(0,Qt::UserRole+2).toInt();
    if(light>=0)result.lights.push_back(size_t(light));
    else if(target>=0)result.targets.push_back(size_t(target));
    else if(target<=-4){const auto id=item->data(0,Qt::UserRole+3).toString().toStdString();if(!id.empty())result.nodes.push_back(id);}
  }
  auto unique=[](auto &values){std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());};unique(result.targets);unique(result.nodes);unique(result.lights);return result;
}
inline std::vector<Selection> tree_selection(QTreeWidget *tree) {
  std::vector<Selection> result;
  auto append=[&](auto &&self,QTreeWidgetItem *item)->void {
    Selection key{item->data(0,Qt::UserRole).toInt(),item->data(0,Qt::UserRole+1).toInt(),item->data(0,Qt::UserRole+2).toInt()};
    if(key[0]==-4) {for(int i=0;i<item->childCount();++i) self(self,item->child(i));return;}
    if(key[0]<0&&key[0]>-5&&key[2]<0) return;
    if(std::find(result.begin(),result.end(),key)==result.end()) result.push_back(key);
    if(key[1]<0)for(const auto &member:item->data(0,Qt::UserRole+4).toList()){
      Selection part{member.toInt(),-1,-1};if(std::find(result.begin(),result.end(),part)==result.end())result.push_back(part);
    }
  };
  for(auto *item:tree->selectedItems()) append(append,item);
  std::sort(result.begin(),result.end());return result;
}
inline void choose_item(QTreeWidget *tree,QTreeWidgetItem *item,bool toggle=false) {
  if(!item) {if(!toggle) {tree->clearSelection();tree->setCurrentItem(nullptr);}return;}
  if(toggle) {
    const bool selected=item->isSelected();tree->setCurrentItem(item,0,QItemSelectionModel::NoUpdate);item->setSelected(!selected);
  } else tree->setCurrentItem(item,0,QItemSelectionModel::ClearAndSelect);
}
inline QTreeWidgetItem *active_selection(QTreeWidget *tree) {
  if(auto *item=tree->currentItem();item&&item->isSelected()) return item;
  const auto selected=tree->selectedItems();return selected.empty()?nullptr:selected.back();
}
}
