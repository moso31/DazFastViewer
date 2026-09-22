#include "editor/parameters.h"
#include "runtime/picking.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSlider>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QSplitter>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QSettings>
#include <algorithm>

namespace dfv::editor {
static QString text(const std::string &s) {return QString::fromUtf8(s.data(),qsizetype(s.size()));}
ParameterPanel::ParameterPanel(QWidget *parent):QWidget(parent) {
  auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
  search_=new QLineEdit;search_->setPlaceholderText(QStringLiteral("搜索参数名称、分组或 ID…"));layout->addWidget(search_);
  auto *line=new QHBoxLayout;hidden_=new QCheckBox(QStringLiteral("显示隐藏参数"));line->addWidget(hidden_);count_=new QLabel;line->addWidget(count_,1);layout->addLayout(line);
  auto *split=new QSplitter;groups_=new QTreeWidget;groups_->setHeaderHidden(true);groups_->setIndentation(12);groups_->setMinimumWidth(115);
  tree_=new QTreeWidget;tree_->setHeaderHidden(true);tree_->setRootIsDecorated(false);tree_->setUniformRowHeights(true);tree_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);tree_->setMinimumWidth(185);
  split->addWidget(groups_);split->addWidget(tree_);split->setStretchFactor(1,1);split->setSizes({140,250});layout->addWidget(split,1);
  for(const auto &id:QSettings().value("parameters/favorites").toStringList()) favorites_.insert(id.toStdString());
  connect(search_,&QLineEdit::textChanged,this,[this]{filter();});connect(hidden_,&QCheckBox::toggled,this,[this]{filter();});
  connect(groups_,&QTreeWidget::currentItemChanged,this,[this]{filter();});
  connect(tree_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item){current_=item?item->data(0,Qt::UserRole).toInt():-1;});
  connect(tree_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{mount();});
  auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{mount();});timer->start(150);
}
void ParameterPanel::bind(const runtime::Target *target,const runtime::Properties *values,const std::string &node) {
  target_=target;values_=values;node_=node;effective_.clear();controls_.clear();morph_rows_.clear();
  if(target_) {
    controls_=extra_;morph_rows_.resize(target_->morphs.size(),-1);
    for(size_t i=0;i<target_->morphs.size();++i) {
      const auto &m=target_->morphs[i];if(!runtime::parameter_on_node(m.owner,m.group,node_)) continue;
      ParameterControl c;c.morph=int(i);c.id=m.source+"#"+m.id;c.label=m.label;c.group=m.group.empty()?"/Morphs":m.group;c.detail=m.source+"\n"+m.unsupported+"\n"+m.limitation;
      c.minimum=m.clamped?std::min(m.minimum,values_->morphs[i]):std::min(-100.f,m.minimum);c.maximum=m.clamped?std::max(m.maximum,values_->morphs[i]):std::max(100.f,m.maximum);c.slider_minimum=m.minimum;c.slider_maximum=m.maximum;c.step=m.step;c.initial=m.initial;c.visible=m.visible;c.enabled=m.unsupported.empty()&&!m.locked;
      c.read=[this,i]{return double(values_->morphs.at(i));};c.write=[this,i](double v){if(changed) changed(i,v);};
      if(m.value_type=="bool") c.choices={"关闭","开启"};morph_rows_[i]=int(controls_.size());controls_.push_back(std::move(c));
    }
  }
  rebuild();
}
void ParameterPanel::bind_options(ir::OptionNode *node,std::function<void(size_t,size_t,double)> callback) {
  target_=nullptr;values_=nullptr;controls_.clear();morph_rows_.clear();
  if(node) for(size_t i=0;i<node->parameters.size();++i) {const auto &p=node->parameters[i];
    for(size_t k=0;k<p.value.size();++k) {
      ParameterControl c;c.id=node->id+"/"+p.id+std::to_string(k);c.label=p.label+(p.value.size()==3?std::string(" ")+"RGB"[k]:"");c.group=p.group;c.minimum=p.minimum;c.maximum=p.maximum;c.slider_minimum=p.minimum;c.slider_maximum=std::min(p.maximum,std::max(2.0,p.value[k]*2));c.step=p.step;c.initial=p.value[k];c.visible=p.visible;c.enabled=p.supported;c.choices=p.choices;
      if(p.type=="bool") c.choices={"关闭","开启"};c.detail=p.image_uri+(!p.supported?"\n已保留原值，此参数尚未参与渲染":"");
      if(p.id=="Environment Mode"&&c.choices.size()>2) {c.choices[2]+="（待支持）";c.disabled_choices.insert(2);c.detail+="\nSun-Sky Only 参数已保留；太阳天空模型尚未实现。";}
      c.read=[node,i,k]{return node->parameters.at(i).value.at(k);};c.write=[this,callback,i,k](double v){callback(i,k,v);update_rows();};controls_.push_back(std::move(c));
    }
  }
  rebuild();
  if(node) for(QTreeWidgetItemIterator it(groups_);*it;++it) {
    const auto path=(*it)->data(0,Qt::UserRole).toString();
    if(path=="/Tone Mapping"||path=="/Environment") {groups_->setCurrentItem(*it);break;}
  }
}
void ParameterPanel::rebuild() {
  QSignalBlocker a(groups_),b(tree_),scroll(tree_->verticalScrollBar());mounted_.clear();tree_->clear();groups_->clear();items_.clear();current_=-1;
  auto *all=new QTreeWidgetItem(groups_,{QStringLiteral("全部")});all->setData(0,Qt::UserRole,QString("*"));
  auto *fav=new QTreeWidgetItem(groups_,{QStringLiteral("收藏")});fav->setData(0,Qt::UserRole,QString("@favorites"));
  auto *used=new QTreeWidgetItem(groups_,{QStringLiteral("当前使用")});used->setData(0,Qt::UserRole,QString("@used"));
  std::map<QString,QTreeWidgetItem *> paths;
  for(size_t i=0;i<controls_.size();++i) {
    auto &c=controls_[i];if(c.group.empty()) c.group="/General";QTreeWidgetItem *parent=nullptr;QString path;
    for(const auto &part:text(c.group).split('/',Qt::SkipEmptyParts)) {
      path+="/"+part;auto &item=paths[path];if(!item) {item=parent?new QTreeWidgetItem(parent,{part}):new QTreeWidgetItem(groups_,{part});item->setData(0,Qt::UserRole,path);}parent=item;
    }
    auto *item=new QTreeWidgetItem(tree_);item->setData(0,Qt::UserRole,int(i));item->setSizeHint(0,QSize(180,58));items_.push_back(item);
  }
  groups_->setCurrentItem(all);groups_->expandToDepth(1);filter();
}
void ParameterPanel::filter() {
  QSignalBlocker scroll(tree_->verticalScrollBar());
  const auto q=search_->text().trimmed();const auto group=groups_->currentItem()?groups_->currentItem()->data(0,Qt::UserRole).toString():"*";int count=0;
  for(size_t i=0;i<controls_.size();++i) {
    const auto &c=controls_[i];const auto path=text(c.group);
    const bool category=group=="*"||(group=="@favorites"&&favorites_.contains(c.id))||(group=="@used"&&std::abs(c.read())>1e-6)||(path==group||path.startsWith(group+"/"));
    const bool shown=category&&(hidden_->isChecked()||c.visible)&&text(c.label+" "+c.id+" "+c.group).contains(q,Qt::CaseInsensitive);items_[i]->setHidden(!shown);if(shown) ++count;
  }
  count_->setText(QStringLiteral("%1 / %2 项").arg(count).arg(controls_.size()));mount();
}
void ParameterPanel::mount() {
  std::set<int> visible;const auto rect=tree_->viewport()->rect();
  for(int y=0;y<rect.height()+58;y+=20) if(auto *item=tree_->itemAt(2,std::min(y,std::max(0,rect.height()-1)))) visible.insert(item->data(0,Qt::UserRole).toInt());
  for(auto it=mounted_.begin();it!=mounted_.end();) {if(!visible.contains(it->first)) {tree_->removeItemWidget(items_.at(size_t(it->first)),0);it=mounted_.erase(it);}else ++it;}
  for(int i:visible) if(!mounted_.contains(i)) {
    const auto &c=controls_.at(size_t(i));auto *widget=new QWidget;auto *layout=new QVBoxLayout(widget);layout->setContentsMargins(5,2,5,3);layout->setSpacing(1);
    auto *title=new QHBoxLayout;title->setSpacing(2);auto *label=new QLabel(text(c.label));label->setObjectName("valueLabel");label->setToolTip(text(c.detail));title->addWidget(label,1);
    auto *favorite=new QToolButton;favorite->setText(favorites_.contains(c.id)?QStringLiteral("★"):QStringLiteral("☆"));favorite->setAutoRaise(true);favorite->setToolTip(QStringLiteral("收藏参数"));title->addWidget(favorite);layout->addLayout(title);
    connect(favorite,&QToolButton::clicked,this,[this,i,favorite]{const auto key=controls_[i].id;if(favorites_.contains(key)) favorites_.erase(key);else favorites_.insert(key);favorite->setText(favorites_.contains(key)?QStringLiteral("★"):QStringLiteral("☆"));QStringList ids;for(const auto &v:favorites_) ids<<text(v);QSettings().setValue("parameters/favorites",ids);});
    auto *line=new QHBoxLayout;line->setSpacing(4);layout->addLayout(line);
    if(!c.choices.empty()) {
      auto *combo=new QComboBox;combo->setObjectName("valueChoice");for(const auto &choice:c.choices) combo->addItem(text(choice));combo->setCurrentIndex(int(c.read()));combo->setEnabled(c.enabled);line->addWidget(combo);
      if(auto *model=qobject_cast<QStandardItemModel *>(combo->model())) for(int index:c.disabled_choices) if(auto *item=model->item(index)) item->setEnabled(false);
      connect(combo,&QComboBox::currentIndexChanged,this,[this,i](int value){current_=i;controls_[i].write(value);update_rows();});
    } else {
      auto *slider=new QSlider(Qt::Horizontal);slider->setObjectName("valueSlider");slider->setRange(0,1000);slider->setEnabled(c.enabled&&c.slider_maximum>c.slider_minimum);
      auto *spin=new QDoubleSpinBox;spin->setObjectName("valueSpin");spin->setDecimals(4);spin->setRange(c.minimum,c.maximum);spin->setSingleStep(std::max(.0001,c.step));spin->setKeyboardTracking(false);spin->setMaximumWidth(105);spin->setEnabled(c.enabled);
      line->addWidget(slider,1);line->addWidget(spin);spin->setValue(c.read());slider->setValue(c.slider_maximum>c.slider_minimum?qRound((c.read()-c.slider_minimum)/(c.slider_maximum-c.slider_minimum)*1000):0);
      connect(slider,&QSlider::valueChanged,this,[this,i](int value){current_=i;const auto &c=controls_[i];c.write(c.slider_minimum+(c.slider_maximum-c.slider_minimum)*value/1000);update_rows();});
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this,i](double value){current_=i;controls_[i].write(value);update_rows();});
    }
    widget->setToolTip(text(c.detail));tree_->setItemWidget(items_[size_t(i)],0,widget);mounted_[i]=widget;
  }
}
void ParameterPanel::update_rows() {
  for(const auto &[i,w]:mounted_) {const auto &c=controls_[i];
    if(target_&&c.morph>=0) {
      const auto &m=target_->morphs[size_t(c.morph)];QString detail=text(c.detail),status;
      if(!m.unsupported.empty()) status=QStringLiteral(" · 待支持");
      else if(m.payload) switch(m.payload->state()) {case runtime::PayloadState::unloaded:status=QStringLiteral(" · 按需加载");break;case runtime::PayloadState::loading:status=QStringLiteral(" · 加载中");break;case runtime::PayloadState::failed:status=QStringLiteral(" · 加载失败");detail+="\n"+text(m.payload->error());break;default:break;}
      if(size_t(c.morph)<effective_.size()) detail+=QStringLiteral("\n最终值：%1").arg(effective_[size_t(c.morph)]);
      if(auto *label=w->findChild<QLabel *>("valueLabel")) {label->setText(text(c.label)+status);label->setToolTip(detail);}w->setToolTip(detail);
    }
    if(auto *spin=w->findChild<QDoubleSpinBox *>("valueSpin")) {QSignalBlocker block(spin);spin->setValue(c.read());}
    if(auto *slider=w->findChild<QSlider *>("valueSlider")) {QSignalBlocker block(slider);slider->setValue(c.slider_maximum>c.slider_minimum?qRound((c.read()-c.slider_minimum)/(c.slider_maximum-c.slider_minimum)*1000):0);}
    if(auto *combo=w->findChild<QComboBox *>("valueChoice")) {QSignalBlocker block(combo);combo->setCurrentIndex(int(c.read()));}
  }
}
void ParameterPanel::refresh(size_t) {update_rows();}
void ParameterPanel::query(const QString &value) {search_->setText(value);}
void ParameterPanel::select_parameter(size_t index) {if(index<morph_rows_.size()&&morph_rows_[index]>=0) {current_=morph_rows_[index];auto *item=items_[size_t(current_)];tree_->setCurrentItem(item);tree_->scrollToItem(item);mount();}}
void ParameterPanel::set_slider(int value) {if(current_>=0) {const auto &c=controls_.at(size_t(current_));c.write(c.slider_minimum+(c.slider_maximum-c.slider_minimum)*value/1000);update_rows();}}
void ParameterPanel::evaluated(const std::vector<float> &values) {effective_=values;update_rows();}
void ParameterPanel::resource_states() {update_rows();}
}
