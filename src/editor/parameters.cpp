#include "editor/parameters.h"
#include "editor/parameter_widgets.h"
#include "render_ir/matte_fog.h"
#include "render_ir/night_sky.h"
#include <QDateEdit>
#include <QApplication>
#include <QTimeEdit>
#include "editor/numeric_slider.h"
#include "editor/numeric_spinbox.h"
#include "editor/hdr_color_dialog.h"
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
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QSettings>
#include <QCoreApplication>
#include <algorithm>
#include <limits>

namespace dfv::editor {
static QString text(const std::string &s) {return QString::fromUtf8(s.data(),qsizetype(s.size()));}
static std::string favorite_id(const runtime::Target &target,const runtime::Morph &m) {
  auto path=m.source;std::replace(path.begin(),path.end(),'\\','/');for(auto &c:path)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
  auto scope=target.favorite_scope;std::replace(scope.begin(),scope.end(),'\\','/');for(auto &c:scope)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
  if(path==scope)path="scene";else if(const auto at=path.find("/data/");at!=std::string::npos)path=path.substr(at);
  const auto at=m.id.rfind('#');return "morph:"+path+"#"+(!m.channel_id.empty()?m.channel_id:at==std::string::npos?m.id:m.id.substr(at+1));
}
static void forward_wheel(QWidget *target,QWheelEvent *event) {
  QWheelEvent forwarded(target->mapFromGlobal(event->globalPosition()),event->globalPosition(),event->pixelDelta(),event->angleDelta(),
    event->buttons(),event->modifiers(),event->phase(),event->inverted(),event->source(),event->pointingDevice());
  QCoreApplication::sendEvent(target,&forwarded);event->accept();
}
ParameterPanel::ParameterPanel(QWidget *parent):QWidget(parent) {
  auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
  search_=new QLineEdit;search_->setPlaceholderText(QStringLiteral("搜索参数名称、分组或 ID…"));layout->addWidget(search_);
  auto *line=new QHBoxLayout;hidden_=new QCheckBox(QStringLiteral("显示隐藏参数"));line->addWidget(hidden_);count_=new QLabel;line->addWidget(count_,1);layout->addLayout(line);
  auto *split=new QSplitter;groups_=new QTreeWidget;groups_->setHeaderHidden(true);groups_->setIndentation(12);groups_->setMinimumWidth(115);
  tree_=new QTreeWidget;tree_->setHeaderHidden(true);tree_->setRootIsDecorated(false);tree_->setUniformRowHeights(true);tree_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);tree_->setMinimumWidth(185);
  groups_->setObjectName("parameterGroups");tree_->setObjectName("parameterRows");
  qApp->installEventFilter(this);
  split->addWidget(groups_);split->addWidget(tree_);split->setStretchFactor(1,1);split->setSizes({140,250});layout->addWidget(split,1);
  for(const auto &id:QSettings().value("parameters/favorites").toStringList()) favorites_.insert(id.toStdString());
  const auto overrides=QSettings().value("parameters/favoriteOverrides").toMap();
  for(auto it=overrides.cbegin();it!=overrides.cend();++it) favorite_overrides_[it.key().toStdString()]=it.value().toBool();
  connect(search_,&QLineEdit::textChanged,this,[this]{filter();});connect(hidden_,&QCheckBox::toggled,this,[this]{filter();});
  connect(groups_,&QTreeWidget::currentItemChanged,this,[this]{filter();});
  connect(tree_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item){current_=item?item->data(0,Qt::UserRole).toInt():-1;if(current_!=wheel_selected_) wheel_selection(-1);});
  connect(tree_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{mount();});
  auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{mount();});timer->start(150);
}
void ParameterPanel::shared_scroll(QScrollArea *scroll) {
  shared_scroll_=scroll;setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
  for(auto *tree:{groups_,tree_}){tree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);tree->setAutoScroll(false);}
  connect(scroll->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{mount();});
  connect(groups_,&QTreeWidget::itemExpanded,this,[this]{shared_height();});connect(groups_,&QTreeWidget::itemCollapsed,this,[this]{shared_height();});shared_height();
}
void ParameterPanel::shared_height() {
  if(!shared_scroll_)return;int rows=0,groups=0;
  for(auto *item:items_)if(!item->isHidden())++rows;
  for(QTreeWidgetItemIterator it(groups_);*it;++it){bool shown=true;for(auto *p=(*it)->parent();p;p=p->parent())shown&=p->isExpanded();if(shown)++groups;}
  const int group_height=groups_->topLevelItemCount()?groups_->sizeHintForIndex(groups_->indexFromItem(groups_->topLevelItem(0))).height():22;
  tree_->parentWidget()->setFixedHeight(std::max({60,rows*58+4,groups*std::max(20,group_height)+4}));tree_->verticalScrollBar()->setValue(0);groups_->verticalScrollBar()->setValue(0);QTimer::singleShot(0,this,[this]{mount();});
}
void ParameterPanel::show_item(QTreeWidgetItem *item) {
  if(!shared_scroll_){tree_->scrollToItem(item);return;}
  tree_->doItemsLayout();const auto point=tree_->viewport()->mapTo(shared_scroll_->widget(),tree_->visualItemRect(item).center());shared_scroll_->ensureVisible(point.x(),point.y(),0,50);
}
void ParameterPanel::wheel_selection(int row) {
  wheel_selected_=row;
  for(const auto &[i,w]:mounted_)if(auto *label=w->findChild<QLabel *>("valueLabel"))
    label->setStyleSheet(i==row?"QLabel { color: palette(highlighted-text); font-weight: bold; }":"");
}
bool ParameterPanel::eventFilter(QObject *object,QEvent *event) {
  if(event->type()!=QEvent::MouseButtonPress&&event->type()!=QEvent::MouseButtonDblClick&&event->type()!=QEvent::Wheel&&event->type()!=QEvent::Hide)return QWidget::eventFilter(object,event);
  if(event->type()==QEvent::Hide&&object==this)wheel_selection(-1);
  const auto *widget=qobject_cast<QWidget *>(object);
  if(widget&&widget->window()->property("parameterSettingsPopup").toBool())return false;
  const bool owned=widget&&(widget==this||isAncestorOf(widget));
  if(!owned) {
    if(event->type()==QEvent::MouseButtonPress&&static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton)wheel_selection(-1);
    return QWidget::eventFilter(object,event);
  }
  if(shared_scroll_&&event->type()==QEvent::Wheel&&(object==tree_->viewport()||object==groups_->viewport())){forward_wheel(shared_scroll_->viewport(),static_cast<QWheelEvent *>(event));return true;}
  if(event->type()!=QEvent::MouseButtonPress&&event->type()!=QEvent::MouseButtonDblClick&&event->type()!=QEvent::Wheel) return QWidget::eventFilter(object,event);
  const bool viewport=object==tree_->viewport();
  const auto row=object->property("parameterRow");
  if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick) {
    const auto *mouse=static_cast<QMouseEvent *>(event);
    if(mouse->button()==Qt::LeftButton) {
      auto *item=viewport?tree_->itemAt(mouse->position().toPoint()):row.isValid()?items_.at(size_t(row.toInt())):nullptr;
      wheel_selection(item?item->data(0,Qt::UserRole).toInt():-1);
      if(item) tree_->setCurrentItem(item,0,QItemSelectionModel::ClearAndSelect);
    }
  } else if(event->type()==QEvent::Wheel&&!viewport&&row.isValid()) {
    const int index=row.toInt();const auto *item=items_.at(size_t(index));
    // 焦点、键盘选择或程序设置 currentItem 都不能开启滚轮调参；只认左键选中的行。
    if(index!=wheel_selected_||!item->isSelected()||item->isHidden()||!controls_.at(size_t(index)).enabled) {
      forward_wheel(tree_->viewport(),static_cast<QWheelEvent *>(event));return true;
    }
  }
  return QWidget::eventFilter(object,event);
}
void ParameterPanel::import_favorites(const runtime::Target *target,std::optional<runtime::FavoriteState> &state) const {
  if(state)return;state.emplace();std::map<std::string,std::string> ids;
  if(favorites_.empty()&&favorite_overrides_.empty())return;
  if(target)for(const auto &m:target->morphs)ids[m.source+"#"+m.id]=favorite_id(*target,m);
  auto portable=[&](const std::string &id){const auto it=ids.find(id);return it==ids.end()?id:it->second;};
  for(const auto &id:favorites_)if(ids.contains(id)||id.find('#')==std::string::npos)state->fallback.insert(portable(id));
  if(!target)return;const auto prefix=target->favorite_scope+"\n"+target->id+"\n";
  for(const auto &[key,value]:favorite_overrides_)if(key.starts_with(prefix)){
    const auto at=key.find('\n',prefix.size());if(at==std::string::npos)continue;
    state->nodes[key.substr(prefix.size(),at-prefix.size())][portable(key.substr(at+1))]=value;
  }
}
bool ParameterPanel::is_favorite(const ParameterControl &control) const {
  if(saved_favorites_){const auto &id=control.favorite_id.empty()?control.id:control.favorite_id;
    if(const auto node=saved_favorites_->nodes.find(saved_favorite_node_);node!=saved_favorites_->nodes.end())if(const auto found=node->second.find(id);found!=node->second.end())return found->second;
    return control.favorite||(!scene_favorites_&&saved_favorites_->fallback.contains(id));
  }
  if(!favorite_scope_.empty()) if(const auto it=favorite_overrides_.find(favorite_scope_+"\n"+control.id);it!=favorite_overrides_.end()) return it->second;
  return control.favorite||(!scene_favorites_&&favorites_.contains(control.id));
}
void ParameterPanel::toggle_favorite(size_t index) {
  const auto &control=controls_.at(index);const bool enabled=!is_favorite(control);
  if(saved_favorites_&&favorite_changed){favorite_changed(saved_favorite_node_,control.favorite_id.empty()?control.id:control.favorite_id,enabled);}
  else if(favorite_scope_.empty()) {
    if(enabled) favorites_.insert(control.id);else favorites_.erase(control.id);
    QStringList ids;for(const auto &id:favorites_) ids<<text(id);QSettings().setValue("parameters/favorites",ids);
  } else {
    const auto key=favorite_scope_+"\n"+control.id;favorite_overrides_[key]=enabled;
    // 只保存用户覆盖，导入的收藏不写进全局收藏，也不改写源 DUF。
    auto saved=QSettings().value("parameters/favoriteOverrides").toMap();saved[text(key)]=enabled;QSettings().setValue("parameters/favoriteOverrides",saved);
  }
  if(auto it=mounted_.find(int(index));it!=mounted_.end()) if(auto *button=it->second->findChild<QToolButton *>("favoriteButton")) button->setText(enabled?QStringLiteral("★"):QStringLiteral("☆"));
  // 收藏页取消后立即移除该行；延后到点击信号返回，避免销毁正在发信号的按钮。
  QTimer::singleShot(0,this,[this]{filter();});
}
void ParameterPanel::bind(const runtime::Target *target,const runtime::Properties *values,const std::string &node) {
  const auto selected=target&&target_==target&&node_==node&&wheel_selected_>=0?controls_.at(size_t(wheel_selected_)).id:std::string{};
  option_node_=nullptr;
  if(!target)bind_favorites(nullptr);
  target_=target;values_=values;node_=node;effective_.clear();controls_.clear();morph_rows_.clear();
  favorite_scope_.clear();scene_favorites_=false;
  if(target_) {
    favorite_scope_=target_->favorite_scope+"\n"+target_->id+"\n"+node_;
    const auto saved=target_->favorites.find(node_);scene_favorites_=saved!=target_->favorites.end();
    auto saved_favorite=[&](const std::string &name) {return !name.empty()&&scene_favorites_&&saved->second.contains(name);};
    auto favorite_owner=[&](const runtime::Morph &m) {return node_.empty()?m.kind!="alias":m.owner==node_;};
    std::set<std::string> scene_names;
    for(const auto &m:target_->morphs) if(m.scene_channel&&favorite_owner(m)) {if(!m.channel_name.empty()) scene_names.insert(m.channel_name);if(!m.channel_id.empty()) scene_names.insert(m.channel_id);}
    controls_=extra_;morph_rows_.resize(target_->morphs.size(),-1);
    for(auto &c:controls_) c.favorite=saved_favorite(c.favorite_name);
    for(size_t i=0;i<target_->morphs.size();++i) {
      const auto &m=target_->morphs[i];if(runtime::legacy_extension_channel(m.label)||!runtime::parameter_on_node(m.owner,m.group,node_)) continue;
      ParameterControl c;c.morph=int(i);c.id=m.source+"#"+m.id;c.label=m.label;c.group=m.group.empty()?"/Morphs":m.group;c.detail=m.source+"\n"+m.unsupported+"\n"+m.limitation;
      c.favorite_id=favorite_id(*target_,m);
      // Figure 列表不能同时点亮头部的同名别名；骨骼列表只属于该骨骼自身。
      c.favorite=favorite_owner(m)&&(saved_favorite(m.channel_name)||saved_favorite(m.channel_id));
      // 多个资源定义同名属性时，优先 DUF 实际引用的资源，避免重复收藏另一产品的通道。
      if(!m.scene_channel&&(scene_names.contains(m.channel_name)||scene_names.contains(m.channel_id))) c.favorite=false;
      c.minimum=m.clamped?std::min(m.minimum,values_->morphs[i]):std::min(-100.f,m.minimum);c.maximum=m.clamped?std::max(m.maximum,values_->morphs[i]):std::max(100.f,m.maximum);c.slider_minimum=m.minimum;c.slider_maximum=m.maximum;c.step=m.step;c.initial=m.initial;c.visible=m.visible;c.enabled=m.unsupported.empty()&&!m.locked;
      c.float_backed=true;c.read=[this,i]{return double(values_->morphs.at(i));};c.write=[this,i](double v){if(changed) changed(i,v);};
      c.settings=runtime::ParameterSettings{m.clamped,decimal_float(m.minimum),decimal_float(m.maximum),std::max(1e-9,decimal_float(m.step))};
      if(m.value_type=="bool") c.choices={"关闭","开启"};morph_rows_[i]=int(controls_.size());controls_.push_back(std::move(c));
    }
  }
  rebuild();
  if(!selected.empty())for(size_t i=0;i<controls_.size();++i)if(controls_[i].id==selected&&!items_[i]->isHidden()){wheel_selection(int(i));tree_->setCurrentItem(items_[i]);break;}
}
void ParameterPanel::bind_options(ir::OptionNode *node,std::function<void(size_t,size_t,double)> callback,
                                  std::function<void(size_t,const std::array<double,3> &)> color_callback) {
  const bool same=node&&node==option_node_&&node->id==option_node_id_;
  const auto selected=same&&wheel_selected_>=0?controls_.at(size_t(wheel_selected_)).id:std::string{};
  node_.clear();option_node_=node;option_node_id_=node?node->id:std::string{};
  target_=nullptr;values_=nullptr;controls_.clear();morph_rows_.clear();
  favorite_scope_.clear();scene_favorites_=false;
  if(node) for(size_t i=0;i<node->parameters.size();++i) {const auto &p=node->parameters[i];
    const bool color=(p.type=="float_color"||p.type=="color")&&p.value.size()==3;
    for(size_t k=0;k<(color?1:p.value.size());++k) {
      ParameterControl c;c.id=node->id+"/"+p.id+std::to_string(k);c.label=p.label+(!color&&p.value.size()==3?std::string(" ")+"RGB"[k]:"");c.group=p.group;c.minimum=p.minimum;c.maximum=p.maximum;c.slider_minimum=p.minimum;c.slider_maximum=std::min(p.maximum,std::max(2.0,p.value[k]*2));c.step=p.step;c.initial=p.value[k];c.visible=p.visible;c.enabled=p.supported;c.choices=p.choices;
      if(p.type=="bool") c.choices={"关闭","开启"};c.detail=p.image_uri+(!p.supported?"\n已保留原值，此参数尚未参与渲染":"");
      if(p.authored)c.settings=runtime::ParameterSettings{p.clamped,p.minimum,p.maximum,std::max(1e-9,p.step)};
      if(ir::matte_fog_option(p.id)) {
        c.enforce_limits=true;c.visible=true;
        c.detail="模型与天空穿过同一片雾层。能见度是在基准高度处，物体对比度降至 2% 的距离；数值越小，雾越浓。距离单位均为米。";
        if(p.id=="DFV Matte Fog Base Height")c.detail="雾层的世界基准高度。此高度以下密度保持恒定，以上指数衰减；默认 0 m 对应场景地面。";
        if(p.id=="DFV Matte Fog Scale Height")c.detail="每升高此距离，雾密度降为约 37%。较小值形成低空薄雾，较大值形成厚重大气；模型和天空使用相同的密度分布。";
        if(p.id=="DFV Matte Fog Start")c.detail="跳过相机附近此距离内的雾，同时作用于模型与天空。0 m 使用连续雾层，最适合自然大气。";
        if(p.id=="Matte Fog Brightness Relative to Environment")c.detail="开启时随环境平均亮度变化；关闭时使用独立雾亮度。Scene Only 下可关闭此项来制作可见的远景雾。";
      }
      if(p.id=="SS Day") {c.format=ParameterControl::Format::date;c.label="SS Day（年月日）";}
      if(ir::night_option(p.id)){
        c.enforce_limits=p.type=="bool"||p.type=="enum";
        if(p.type=="bool")c.format=ParameterControl::Format::checkbox;
        if(p.id!="DFV Night Enabled")c.read_enabled=[node,supported=p.supported]{return supported&&ir::night_enabled(*node);};
        c.detail="夜景关闭时不参与渲染。使用 SS 日期、当地时间、UTC 时差与经纬度；仅场景模式下不启用天空。";
        if(p.id=="DFV Night Enabled")c.detail="启用日期驱动的日夜天空，替代环境贴图；关闭后恢复原有环境。原生 DAZ DUF 默认关闭。";
        if(p.id=="DFV Night Limiting Magnitude"){c.detail="仅用于真实星表模式。根据星等保留亮星、隐藏暗星；目录最暗为 7 等，超过目录不会凭空增加恒星。";c.read_enabled=[node,supported=p.supported]{return supported&&ir::night_enabled(*node)&&ir::number(*node,"DFV Night Star Distribution",0)==1;};}
        if(p.id=="DFV Night Star Density"){c.detail="仅用于网页式星空。1 对应参考网页，减小逐渐隐藏暗星，增大显示更多暗星；星位保持稳定。";c.read_enabled=[node,supported=p.supported]{return supported&&ir::night_enabled(*node)&&ir::number(*node,"DFV Night Star Distribution",0)==0;};}
        if(p.id=="DFV Night Star Distribution")c.detail="网页式星空使用三维球面上的稳定程序化分布，包含远近星光和尘埃遮挡；真实星表保留 HYG 恒星位置及 B-V 色彩。两者均随日期系统旋转。";
        if(p.id=="DFV Night Lighting")c.detail="所有档位只有一个环境光，不创建逐星点光源。均色与 SH 将非相机光线中的星空近似为平滑补光；完整环境保留星点反射，可能需要更多采样降噪。月光始终独立。";
        if(p.id=="DFV Night Quality")c.detail="控制银河球状云团的噪声层数；中档与网页默认层数一致。日常调参不烘焙天空纹理。真实星表模式同时调整星位查询表的精细度。";
        if(p.id=="DFV Night Rotation")c.detail="在日期驱动的天球上附加艺术旋转，不移动月亮。Dome Rotation 同时旋转太阳、月亮和星空。";
        if(c.read_enabled)c.enabled=c.read_enabled();
      }
      if(p.id=="SS Time") {c.format=ParameterControl::Format::time;c.label="SS Time（时分秒）";}
      c.read=[node,i,k]{return node->parameters.at(i).value.at(k);};c.write=[this,callback,i,k](double v){callback(i,k,v);update_rows();};controls_.push_back(std::move(c));
      if(color) {
        auto &control=controls_.back();control.format=ParameterControl::Format::color;
        control.read_color=[node,i]{const auto &v=node->parameters.at(i).value;return std::array<double,3>{v[0],v[1],v[2]};};
        if(ir::night_option(p.id))control.maximum=std::numeric_limits<float>::max();
        control.write_color=[this,callback,color_callback,i,minimum=p.minimum,maximum=p.maximum,night=ir::night_option(p.id)](auto v){if(!night)for(auto &c:v)c=std::clamp(c,minimum,maximum);if(color_callback)color_callback(i,v);else for(size_t k=0;k<3;++k)callback(i,k,v[k]);update_rows();};
      }
    }
  }
  rebuild();
  if(same&&!selected.empty())for(size_t i=0;i<controls_.size();++i)if(controls_[i].id==selected&&!items_[i]->isHidden()) {wheel_selection(int(i));tree_->setCurrentItem(items_[i]);break;}
  if(node&&!same) for(QTreeWidgetItemIterator it(groups_);*it;++it) {
    const auto path=(*it)->data(0,Qt::UserRole).toString();
    if(path=="/Tone Mapping"||path=="/Environment") {groups_->setCurrentItem(*it);break;}
  }
}
void ParameterPanel::rebuild() {
  const auto selected_group=groups_->currentItem()?groups_->currentItem()->data(0,Qt::UserRole).toString():QString("*");
  QSignalBlocker a(groups_),b(tree_),scroll(tree_->verticalScrollBar());mounted_.clear();tree_->clear();groups_->clear();items_.clear();current_=wheel_selected_=-1;
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
  groups_->setCurrentItem(all);
  for(QTreeWidgetItemIterator it(groups_);*it;++it) if((*it)->data(0,Qt::UserRole).toString()==selected_group) {groups_->setCurrentItem(*it);break;}
  groups_->expandToDepth(1);filter();
}
void ParameterPanel::filter() {
  QSignalBlocker scroll(tree_->verticalScrollBar());
  const auto q=search_->text().trimmed();const auto group=groups_->currentItem()?groups_->currentItem()->data(0,Qt::UserRole).toString():"*";int count=0;
  for(size_t i=0;i<controls_.size();++i) {
    const auto &c=controls_[i];const auto path=text(c.group);
    const bool used=c.read_color?hdr_color::peak(c.read_color())>1e-6:std::abs(c.read())>1e-6;
    const bool category=group=="*"||(group=="@favorites"&&is_favorite(c))||(group=="@used"&&used)||(path==group||path.startsWith(group+"/"));
    const bool shown=category&&(hidden_->isChecked()||c.visible)&&text(c.label+" "+c.id+" "+c.group).contains(q,Qt::CaseInsensitive);items_[i]->setHidden(!shown);if(shown) ++count;
  }
  if(wheel_selected_>=0&&items_[size_t(wheel_selected_)]->isHidden()) wheel_selection(-1);
  count_->setText(QStringLiteral("%1 / %2 项").arg(count).arg(controls_.size()));shared_height();mount();
}
void ParameterPanel::mount() {
  std::set<int> visible;auto rect=tree_->viewport()->rect();
  if(shared_scroll_){rect=rect.intersected(QRect(tree_->viewport()->mapFromGlobal(shared_scroll_->viewport()->mapToGlobal(QPoint(0,0))),shared_scroll_->viewport()->size()));if(!isVisible())rect={};}
  for(int y=rect.top();!rect.isEmpty()&&y<rect.bottom()+58;y+=20) if(auto *item=tree_->itemAt(2,std::min(y,rect.bottom()))) visible.insert(item->data(0,Qt::UserRole).toInt());
  for(auto it=mounted_.begin();it!=mounted_.end();) {if(!visible.contains(it->first)) {tree_->removeItemWidget(items_.at(size_t(it->first)),0);it=mounted_.erase(it);}else ++it;}
  for(int i:visible) if(!mounted_.contains(i)) {
    const auto &c=controls_.at(size_t(i));auto *widget=new QWidget;auto *layout=new QVBoxLayout(widget);layout->setContentsMargins(5,2,5,3);layout->setSpacing(1);
    auto *title=new QHBoxLayout;title->setSpacing(2);auto *label=new QLabel(text(c.label));label->setObjectName("valueLabel");label->setToolTip(text(c.detail));title->addWidget(label,1);
    auto *favorite=new QToolButton;favorite->setObjectName("favoriteButton");favorite->setText(is_favorite(c)?QStringLiteral("★"):QStringLiteral("☆"));favorite->setAutoRaise(true);favorite->setToolTip(QStringLiteral("收藏参数"));title->addWidget(favorite);layout->addLayout(title);
    connect(favorite,&QToolButton::clicked,this,[this,i]{toggle_favorite(size_t(i));});
    auto *line=new QHBoxLayout;line->setSpacing(4);layout->addLayout(line);
    if(c.format==ParameterControl::Format::color) {
      auto *button=new QPushButton;button->setObjectName("valueColor");button->setProperty("parameterId",text(c.id));button->setEnabled(c.enabled);line->addWidget(button,1);
      hdr_color::swatch(button,c.read_color(),false,true);
      connect(button,&QPushButton::clicked,this,[this,i]{
        const auto control=controls_.at(size_t(i));const auto initial=control.read_color();
        HdrColorDialog dialog(initial,std::max(1e-6,control.maximum),this);dialog.setWindowTitle(text(control.label));
        if(dialog.exec()==QDialog::Accepted&&dialog.color()!=initial)control.write_color(dialog.color());
      });
    } else if(c.format==ParameterControl::Format::checkbox) {
      auto *check=parameter_widgets::checkbox(QStringLiteral("启用"),"valueCheck");check->setChecked(c.read()!=0);check->setEnabled(c.enabled);line->addWidget(check);
      connect(check,&QCheckBox::toggled,this,[this,i](bool checked){current_=i;controls_[i].write(checked?1:0);update_rows();});
    } else if(c.format==ParameterControl::Format::date) {
      auto *date=new QDateEdit;date->setObjectName("valueDate");date->setProperty("historyInput",true);date->setDisplayFormat("yyyy-MM-dd");date->setCalendarPopup(true);date->setDateRange(QDate(1,1,1),QDate(9999,12,31));date->setDate(QDate::fromJulianDay(qRound64(c.read())));date->setKeyboardTracking(false);date->setEnabled(c.enabled);line->addWidget(date);
      connect(date,&QDateEdit::dateChanged,this,[this,i](QDate value){current_=i;controls_[i].write(double(value.toJulianDay()));update_rows();});
    } else if(c.format==ParameterControl::Format::time) {
      auto *time=new QTimeEdit;time->setObjectName("valueTime");time->setProperty("historyInput",true);time->setDisplayFormat("HH:mm:ss");time->setTime(QTime(0,0).addSecs(std::clamp(qRound(c.read()),0,86399)));time->setKeyboardTracking(false);time->setEnabled(c.enabled);line->addWidget(time);
      connect(time,&QTimeEdit::timeChanged,this,[this,i](QTime value){current_=i;controls_[i].write(QTime(0,0).secsTo(value));update_rows();});
    } else if(!c.choices.empty()) {
      auto *combo=new QComboBox;combo->setObjectName("valueChoice");for(const auto &choice:c.choices) combo->addItem(text(choice));combo->setCurrentIndex(int(c.read()));combo->setEnabled(c.enabled);line->addWidget(combo);
      if(auto *model=qobject_cast<QStandardItemModel *>(combo->model())) for(int index:c.disabled_choices) if(auto *item=model->item(index)) item->setEnabled(false);
      connect(combo,&QComboBox::currentIndexChanged,this,[this,i](int value){current_=i;controls_[i].write(value);update_rows();});
    } else {
      auto *slider=new NumericSlider;slider->setObjectName("valueSlider");slider->setEnabled(c.enabled);
      connect(slider,&QSlider::sliderPressed,this,[this]{if(interaction_changed) interaction_changed(true);});
      connect(slider,&QSlider::sliderReleased,this,[this]{if(interaction_changed) interaction_changed(false);});
      slider->setToolTip(QStringLiteral("左右拖动可越过标尺范围；Shift 精细调整。左键选中参数后，滚轮可调值；未选中时滚轮翻页。右侧可直接输入数值。"));
      auto *spin=new NumericSpinBox(c.float_backed);spin->setObjectName("valueSpin");spin->setDecimals(c.enforce_limits&&c.step==1?0:6);spin->setRange(c.enforce_limits?c.minimum:-std::numeric_limits<float>::max(),c.enforce_limits?c.maximum:std::numeric_limits<float>::max());spin->setSingleStep(std::max(.000001,c.step));spin->setKeyboardTracking(false);spin->setFixedWidth(125);spin->setEnabled(c.enabled);
      spin->setProperty("parameterId",text(c.id));
      line->addWidget(slider,1);line->addWidget(spin);spin->sync(c.read());
      const auto key="parameters/"+node_+"/"+(c.favorite_id.empty()?c.id:c.favorite_id);
      auto *settings=new parameter_widgets::SettingsButtons(parameter_widgets::context(this),key,spin,c.settings.value_or(runtime::ParameterSettings{false,0,1,c.float_backed?decimal_float(float(c.step)):c.step}),slider,c.slider_minimum,c.slider_maximum,widget);
      settings->setEnabled(c.enabled);title->insertWidget(title->count()-1,settings);
      slider->wheeled=[spin](QWheelEvent *event){forward_wheel(spin,event);};
      slider->edited=[spin](double value){spin->setValue(value);};
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this,i](double value){current_=i;controls_[i].write(value);update_rows();});
      connect(spin,&QDoubleSpinBox::editingFinished,this,[this,spin]{if(auto *line=spin->findChild<QLineEdit *>()) line->setModified(false);update_rows();});
    }
    if(c.read_enabled){
      for(auto *child:widget->findChildren<QWidget *>())if(qobject_cast<QDoubleSpinBox *>(child)||qobject_cast<QSlider *>(child)||qobject_cast<QComboBox *>(child)||qobject_cast<QPushButton *>(child)||qobject_cast<QCheckBox *>(child)||dynamic_cast<parameter_widgets::SettingsButtons *>(child))child->setEnabled(true);
      widget->setEnabled(c.enabled);
    }
    widget->setToolTip(text(c.detail));
    auto watched=widget->findChildren<QWidget *>();watched.push_back(widget);
    for(auto *child:watched) child->setProperty("parameterRow",i);
    if(i==wheel_selected_)label->setStyleSheet("QLabel { color: palette(highlighted-text); font-weight: bold; }");
    tree_->setItemWidget(items_[size_t(i)],0,widget);mounted_[i]=widget;
  }
}
void ParameterPanel::update_rows() {
  for(auto &c:controls_)if(c.read_enabled)c.enabled=c.read_enabled();
  if(wheel_selected_>=0&&!controls_[wheel_selected_].enabled)wheel_selection(-1);
  for(const auto &[i,w]:mounted_) {const auto &c=controls_[i];
    if(c.read_enabled)w->setEnabled(c.enabled);
    if(auto *check=w->findChild<QCheckBox *>("valueCheck")){QSignalBlocker block(check);check->setChecked(c.read()!=0);}
    if(auto *button=w->findChild<QPushButton *>("valueColor"))hdr_color::swatch(button,c.read_color(),false,true);
    if(target_&&c.morph>=0) {
      const auto &m=target_->morphs[size_t(c.morph)];QString detail=text(c.detail),status;
      if(!m.unsupported.empty()) status=QStringLiteral(" · 待支持");
      else if(m.payload) switch(m.payload->state()) {case runtime::PayloadState::unloaded:status=QStringLiteral(" · 按需加载");break;case runtime::PayloadState::loading:status=QStringLiteral(" · 加载中");break;case runtime::PayloadState::failed:status=QStringLiteral(" · 加载失败");detail+="\n"+text(m.payload->error());break;default:break;}
      if(size_t(c.morph)<effective_.size()) detail+=QStringLiteral("\n最终值：%1").arg(effective_[size_t(c.morph)]);
      if(auto *label=w->findChild<QLabel *>("valueLabel")) {label->setText(text(c.label)+status);label->setToolTip(detail);}w->setToolTip(detail);
    }
    if(auto *spin=w->findChild<QDoubleSpinBox *>("valueSpin")) {
      const auto *line=spin->findChild<QLineEdit *>();
      // 后台渲染状态持续刷新时，不能覆盖尚未按 Enter / 失焦提交的输入文本。
      if(!(spin->hasFocus()&&line&&line->isModified())&&spin->value()!=c.read()) {QSignalBlocker block(spin);static_cast<NumericSpinBox *>(spin)->sync(c.read());}
    }
    for(auto *child:w->findChildren<QWidget *>())if(auto *settings=dynamic_cast<parameter_widgets::SettingsButtons *>(child))settings->sync_slider();
    if(auto *combo=w->findChild<QComboBox *>("valueChoice")) {QSignalBlocker block(combo);combo->setCurrentIndex(int(c.read()));}
    if(auto *date=w->findChild<QDateEdit *>("valueDate");date&&!date->hasFocus()) {QSignalBlocker block(date);date->setDate(QDate::fromJulianDay(qRound64(c.read())));}
    if(auto *time=w->findChild<QTimeEdit *>("valueTime");time&&!time->hasFocus()) {QSignalBlocker block(time);time->setTime(QTime(0,0).addSecs(std::clamp(qRound(c.read()),0,86399)));}
  }
}
void ParameterPanel::refresh(size_t) {update_rows();}
bool ParameterPanel::edit_control(const std::string &id,double value) {
  auto found=std::find_if(controls_.begin(),controls_.end(),[&](const auto &c){return c.id==id;});if(found==controls_.end()||!found->enabled) return false;
  hidden_->setChecked(true);search_->clear();groups_->setCurrentItem(groups_->topLevelItem(0));filter();current_=int(found-controls_.begin());tree_->setCurrentItem(items_[current_]);show_item(items_[current_]);mount();
  const auto row=mounted_.find(current_);if(row==mounted_.end()) return false;
  if(auto *spin=row->second->findChild<QDoubleSpinBox *>("valueSpin")) {spin->setValue(value);return true;}
  if(auto *combo=row->second->findChild<QComboBox *>("valueChoice")) {combo->setCurrentIndex(int(value));return true;}return false;
}
void ParameterPanel::query(const QString &value) {search_->setText(value);}
void ParameterPanel::select_parameter(size_t index) {if(index<morph_rows_.size()&&morph_rows_[index]>=0) {current_=morph_rows_[index];auto *item=items_[size_t(current_)];tree_->setCurrentItem(item);show_item(item);mount();}}
void ParameterPanel::set_slider(int value) {if(current_>=0) {
  const auto &c=controls_.at(size_t(current_));if(c.read_color)return;
  const auto key="parameters/"+node_+"/"+(c.favorite_id.empty()?c.id:c.favorite_id);
  const auto settings=parameter_widgets::context(this)->get(key,c.settings.value_or(runtime::ParameterSettings{false,0,1,c.step}));
  const double low=settings.limited?settings.minimum:c.slider_minimum,high=settings.limited?settings.maximum:c.slider_maximum;
  double next=low+(high-low)*value/1000.;if(settings.limited)next=std::clamp(next,low,high);if(c.enforce_limits)next=std::clamp(next,c.minimum,c.maximum);
  c.write(next);update_rows();
}}
void ParameterPanel::evaluated(const std::vector<float> &values) {effective_=values;update_rows();}
void ParameterPanel::resource_states() {update_rows();}
}
