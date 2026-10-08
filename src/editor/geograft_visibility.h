#pragma once
#include "editor/document.h"
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QSignalBlocker>
#include <QStyleOptionViewItem>

namespace dfv::editor {
inline constexpr int graft_visibility_role=Qt::UserRole+7;
class GeograftVisibilityDelegate final:public QStyledItemDelegate {
protected:
  void initStyleOption(QStyleOptionViewItem *option,const QModelIndex &index) const override {
    QStyledItemDelegate::initStyleOption(option,index);
    if(index.data(graft_visibility_role).isValid()&&!index.data(graft_visibility_role).toBool())option->state&=~QStyle::State_Enabled;
  }
public:
  using QStyledItemDelegate::QStyledItemDelegate;
};
inline void sync_geograft_visibility(QTreeWidget *tree,const Document &document,const Snapshot &snapshot) {
  QSignalBlocker block(tree);
  for(QTreeWidgetItemIterator it(tree);*it;++it) {
    auto *item=*it;const int target=item->data(0,Qt::UserRole).toInt();if(target<0||size_t(target)>=snapshot.values.size()||item->data(0,Qt::UserRole+1).toInt()>=0)continue;
    const bool editable=geograft_visibility_editable(document,snapshot,size_t(target));item->setData(0,graft_visibility_role,editable);
    auto flags=item->flags();if(editable)flags|=Qt::ItemIsUserCheckable;else flags&=~Qt::ItemIsUserCheckable;item->setFlags(flags);
    item->setToolTip(0,editable?QString{}:QStringLiteral("Geograft 已停用或所属角色已隐藏；请在角色的 Geograft 组中启用。"));
    item->setCheckState(0,snapshot.values[size_t(target)].visible?Qt::Checked:Qt::Unchecked);
  }
}
}
