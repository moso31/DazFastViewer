#include "editor/parameters.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSlider>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <map>

namespace dfv::editor {
static QString text(const std::string &s) {return QString::fromUtf8(s.data(),qsizetype(s.size()));}
ParameterPanel::ParameterPanel(QWidget *parent):QWidget(parent) {
  auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
  search_=new QLineEdit;search_->setPlaceholderText(QStringLiteral("搜索名称、ID、分组或子节点…"));layout->addWidget(search_);
  hidden_=new QCheckBox(QStringLiteral("显示隐藏参数"));layout->addWidget(hidden_);
  count_=new QLabel;count_->setWordWrap(true);layout->addWidget(count_);
  tree_=new QTreeWidget;tree_->setHeaderLabels({QStringLiteral("参数分组 / 名称"),QStringLiteral("状态")});tree_->setUniformRowHeights(true);tree_->setIndentation(12);tree_->setColumnWidth(0,210);layout->addWidget(tree_,1);
  details_=new QLabel(QStringLiteral("选择参数查看来源和支持状态"));details_->setWordWrap(true);details_->setTextInteractionFlags(Qt::TextSelectableByMouse);details_->setMinimumHeight(72);details_->setMaximumHeight(100);layout->addWidget(details_);
  auto *line=new QHBoxLayout;slider_=new QSlider(Qt::Horizontal);slider_->setRange(0,1000);spin_=new QDoubleSpinBox;spin_->setDecimals(4);spin_->setKeyboardTracking(false);line->addWidget(slider_,1);line->addWidget(spin_);layout->addLayout(line);slider_->setEnabled(false);spin_->setEnabled(false);
  connect(search_,&QLineEdit::textChanged,this,[this] {filter();});connect(hidden_,&QCheckBox::toggled,this,[this] {rebuild();});
  connect(tree_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item) {select(item);});
  connect(slider_,&QSlider::valueChanged,this,[this](int v) {if(current_>=0 && changed) {const auto &m=target_->morphs[size_t(current_)];changed(size_t(current_),m.minimum+(m.maximum-m.minimum)*v/1000.0);}});
  connect(spin_,&QDoubleSpinBox::valueChanged,this,[this](double v) {if(current_>=0 && changed) changed(size_t(current_),v);});
}
void ParameterPanel::bind(const runtime::Target *target,const runtime::Properties *values) {target_=target;values_=values;effective_.clear();rebuild();}
void ParameterPanel::rebuild() {
  QSignalBlocker block(tree_);tree_->clear();items_.clear();current_=-1;slider_->setEnabled(false);spin_->setEnabled(false);details_->setText(QStringLiteral("选择参数查看来源和支持状态"));
  if(!target_) {count_->clear();return;}
  items_.resize(target_->morphs.size());std::map<QString,QTreeWidgetItem *> groups;
  for(size_t i=0;i<target_->morphs.size();++i) {
    const auto &m=target_->morphs[i];if(!hidden_->isChecked() && !m.visible) continue;
    QString group=text(m.group);if(m.kind=="alias") group=QStringLiteral("/子节点/")+text(m.owner)+group;
    QTreeWidgetItem *parent=nullptr;QString path;
    for(const auto &part:group.split('/',Qt::SkipEmptyParts)) {
      path+="/"+part;auto &item=groups[path];
      if(!item) {item=parent?new QTreeWidgetItem(parent,{part}):new QTreeWidgetItem(tree_,{part});item->setData(0,Qt::UserRole,-1);}parent=item;
    }
    const auto state=m.unsupported.empty()?QStringLiteral("可编辑"):m.kind=="alias"?QStringLiteral("别名"):QStringLiteral("待支持");
    auto *item=parent?new QTreeWidgetItem(parent,{text(m.label),state}):new QTreeWidgetItem(tree_,{text(m.label),state});items_[i]=item;item->setData(0,Qt::UserRole,int(i));
    item->setData(0,Qt::UserRole+1,text(m.label+" "+m.channel_id+" "+m.group+" "+m.owner));
    item->setToolTip(0,text(m.group+"\n"+m.source+"\n"+m.unsupported));if(!m.unsupported.empty()) item->setForeground(1,Qt::darkGray);
  }
  filter();
}
void ParameterPanel::filter() {
  const auto query=search_->text().trimmed();size_t matched=0,supported=0;
  std::function<bool(QTreeWidgetItem *)> visit=[&](QTreeWidgetItem *item) {
    bool shown=false;
    if(item->childCount()) {for(int i=0;i<item->childCount();++i) shown=visit(item->child(i))||shown;}
    else {
      const int index=item->data(0,Qt::UserRole).toInt();
      shown=index>=0 && item->data(0,Qt::UserRole+1).toString().contains(query,Qt::CaseInsensitive);
      if(shown) {++matched;if(target_->morphs[size_t(index)].unsupported.empty()) ++supported;}
    }
    item->setHidden(!shown);if(!query.isEmpty() && shown) item->setExpanded(true);return shown;
  };
  for(int i=0;i<tree_->topLevelItemCount();++i) visit(tree_->topLevelItem(i));
  if(target_) count_->setText(QStringLiteral("已发现 %1 项 · 匹配 %2 项 · 可编辑 %3 项").arg(target_->morphs.size()).arg(matched).arg(supported));
}
void ParameterPanel::select(QTreeWidgetItem *item) {
  current_=item?item->data(0,Qt::UserRole).toInt():-1;slider_->setEnabled(false);spin_->setEnabled(false);
  if(current_<0 || !target_) return;
  const auto &m=target_->morphs[size_t(current_)];QSignalBlocker a(slider_),b(spin_);
  const auto support=m.unsupported.empty()?(m.kind=="alias"?"别名：与原参数共享编辑值":m.formula_count||m.offsets.empty()?"Formula / ERC 驱动":"直接 Morph"):m.unsupported;
  QString detail=text(m.label+"\n"+m.group+"\n"+support);
  if(m.unsupported.empty()&&size_t(current_)<effective_.size()) detail+=QStringLiteral(" · 最终值 %1").arg(effective_[size_t(current_)],0,'f',4);
  details_->setText(detail);details_->setToolTip(detail+QStringLiteral("\n来源：")+text(m.source)+QStringLiteral("\n所属节点：")+text(m.owner)+QStringLiteral("\n未解析引用：%1\n已恢复的资源引用：%2").arg(m.missing_dependencies).arg(m.repaired_references));
  spin_->setRange(m.clamped?m.minimum:std::min(-100.0f,m.minimum),m.clamped?m.maximum:std::max(100.0f,m.maximum));spin_->setSingleStep(std::max(.001,double(m.step)));
  slider_->setEnabled(m.unsupported.empty() && m.maximum>m.minimum);spin_->setEnabled(m.unsupported.empty());refresh(size_t(current_));
}
void ParameterPanel::refresh(size_t index) {
  if(current_!=int(index) || !target_ || !values_) return;const auto &m=target_->morphs[index];
  const double value=m.unsupported.empty()?values_->morphs[index]:m.initial;QSignalBlocker a(slider_),b(spin_);spin_->setValue(value);
  slider_->setValue(m.maximum>m.minimum?qRound((value-m.minimum)/(m.maximum-m.minimum)*1000):0);
}
void ParameterPanel::query(const QString &text) {search_->setText(text);}
void ParameterPanel::select_parameter(size_t index) {if(index<items_.size() && items_[index]) {tree_->setCurrentItem(items_[index]);tree_->scrollToItem(items_[index]);}}
void ParameterPanel::set_slider(int value) {slider_->setValue(value);}
void ParameterPanel::evaluated(const std::vector<float> &values) {if(effective_==values) return;effective_=values;if(current_>=0) select(tree_->currentItem());}
}
