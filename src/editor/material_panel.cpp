#include "editor/material_panel.h"
#include "editor/object_hierarchy.h"
#include "editor/folder_icon.h"
#include "editor/numeric_slider.h"
#include "editor/numeric_spinbox.h"
#include <QTreeWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QApplication>
#include <QTimer>
#include <QSignalBlocker>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QImageReader>
#include <QPixmapCache>
#include <QPointer>
#include <QThreadPool>
#include <QDateTime>
#include <QPainter>
#include <set>

namespace dfv::editor {
namespace {
using J=nlohmann::json;using P=MaterialParameter;
class ParameterLabel final:public QLabel {
public:using QLabel::QLabel;
protected:void paintEvent(QPaintEvent *)override{QPainter painter(this);painter.setPen(palette().color(isEnabled()?QPalette::Active:QPalette::Disabled,QPalette::WindowText));painter.drawText(rect(),Qt::AlignVCenter,fontMetrics().elidedText(text(),Qt::ElideRight,width()));}
};
QString text(const std::string &s){return QString::fromUtf8(s.data(),qsizetype(s.size()));}
double linear(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
double srgb(double v){return v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;}
std::string utf8(const QString &s){return s.toUtf8().toStdString();}
void thumbnail(QLabel *label,const QString &file){
  label->setFixedSize(30,30);label->setAlignment(Qt::AlignCenter);label->setObjectName("materialThumbnail");
  if(file.isEmpty()){label->setText(QStringLiteral("—"));return;}
  const auto key="material-thumbnail/"+file+"/"+QString::number(QFileInfo(file).lastModified().toMSecsSinceEpoch());QPixmap cached;
  if(QPixmapCache::find(key,&cached)){label->setPixmap(cached);return;}label->setText(QStringLiteral("…"));label->setToolTip(file);
  QPointer<QLabel> guard(label);
  QThreadPool::globalInstance()->start([guard,file,key]{QImageReader reader(file);reader.setAutoTransform(true);auto size=reader.size();if(size.isValid())reader.setScaledSize(size.scaled(30,30,Qt::KeepAspectRatio));auto image=reader.read();
    QMetaObject::invokeMethod(qApp,[guard,key,image]{if(!image.isNull()){auto pixmap=QPixmap::fromImage(image).scaled(30,30,Qt::KeepAspectRatio,Qt::SmoothTransformation);QPixmapCache::insert(key,pixmap);if(guard)guard->setPixmap(pixmap);}else if(guard){guard->setText(QStringLiteral("?"));guard->setToolTip(guard->toolTip()+QStringLiteral("\n无法读取缩略图"));}},Qt::QueuedConnection);
  });
}
}
#include "editor/material_selection_sets.inl"
#include "editor/material_panel_uv.inl"
MaterialPanel::MaterialPanel(QWidget *parent):QWidget(parent){
  setObjectName("MaterialPanel");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);
  auto *toolbar=new QHBoxLayout;scope_=new QComboBox;scope_->setObjectName("materialScope");scope_->addItems({QStringLiteral("当前角色 / 对象"),QStringLiteral("全部场景对象")});toolbar->addWidget(scope_);
  auto *reset_all=new QPushButton(QStringLiteral("还原所选材质"));reset_all->setToolTip(QStringLiteral("还原至加载场景或最近应用的材质预设"));toolbar->addWidget(reset_all);toolbar->addStretch();layout->addLayout(toolbar);
  auto *preset=new QPushButton(QStringLiteral("应用材质 / Shader 预设…"));layout->addWidget(preset);connect(preset,&QPushButton::clicked,this,[this]{const auto selected=surfaces();if(selected.empty()||!preset_requested)return;auto file=QFileDialog::getOpenFileName(this,QStringLiteral("应用到所选表面"),{},QStringLiteral("DAZ 材质预设 (*.duf *.djl)"));if(!file.isEmpty())preset_requested(std::filesystem::path(file.toStdWString()),selected);});
  auto *splitter=new QSplitter(Qt::Horizontal);splitter->setObjectName("materialSplitter");splitter->setChildrenCollapsible(false);layout->addWidget(splitter,1);
  tree_=new QTreeWidget;tree_->setObjectName("materialSurfaces");tree_->setHeaderLabel(QStringLiteral("对象与子材质"));tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);tree_->setMinimumWidth(100);tree_->setTextElideMode(Qt::ElideMiddle);splitter->addWidget(tree_);
  tree_->setIndentation(14);tree_->setMouseTracking(true);tree_->viewport()->installEventFilter(this);installEventFilter(this);
  auto *right=new QWidget;auto *details=new QVBoxLayout(right);details->setContentsMargins(3,0,0,0);
  heading_=new QLabel(QStringLiteral("选择左侧子材质查看参数"));heading_->setWordWrap(true);details->addWidget(heading_);
  auto *filter_row=new QHBoxLayout;search_=new QLineEdit;search_->setObjectName("materialSearch");search_->setPlaceholderText(QStringLiteral("搜索属性 / DAZ 参数名"));search_->setClearButtonEnabled(true);filter_row->addWidget(search_);
  modified_=new QCheckBox(QStringLiteral("已修改"));modified_->setObjectName("materialModifiedOnly");filter_row->addWidget(modified_);details->addLayout(filter_row);
  properties_=new QScrollArea;properties_->setWidgetResizable(true);properties_->setMinimumWidth(310);details->addWidget(properties_,1);splitter->addWidget(right);splitter->setStretchFactor(1,1);splitter->setSizes({165,380});
  status_=new QLabel;status_->setWordWrap(true);layout->addWidget(status_);
  connect(properties_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value){if(!restoring_scroll_)scroll_position_=value;});
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(tree_,&QWidget::customContextMenuRequested,this,[this](const QPoint &point){
    auto *item=tree_->itemAt(point);if(!item||(!item->data(0,Qt::UserRole+1).isValid()&&!item->data(0,Qt::UserRole+3).isValid())||!document_||!snapshot_)return;
    if(!item->isSelected())tree_->setCurrentItem(item,0,QItemSelectionModel::ClearAndSelect);
    QMenu menu(this);auto *copy=menu.addAction(QStringLiteral("复制材质"));copy->setObjectName("CopyMaterial");copy->setEnabled(item->data(0,Qt::UserRole+1).isValid());
    auto *paste=menu.addAction(QStringLiteral("粘贴材质"));paste->setObjectName("PasteMaterial");paste->setEnabled(!clipboard_.is_null()&&bool(paste_requested));
    auto *action=menu.exec(tree_->viewport()->mapToGlobal(point));
    if(action==copy){try{const auto instance=size_t(item->data(0,Qt::UserRole).toULongLong());clipboard_=copy_material(document_->loaded.scene,snapshot_->material_overrides,{instance,size_t(item->data(0,Qt::UserRole+1).toULongLong())});auto textures=document_->loaded.scene.textures;const auto uv=daz::material_uv_set(read_copied_material(clipboard_,textures));if(!uv.uri.empty())if(auto archive=material_archive(*document_,{instance,size_t(item->data(0,Qt::UserRole+1).toULongLong())},uv.owner))clipboard_["source_archive"]=archive->json();status_->setText(QStringLiteral("已复制材质：")+item->text(0));}catch(const std::exception &e){status_->setText(text(e.what()));}}
    else if(action==paste)paste_requested(clipboard_,surfaces());
  });
  connect(tree_,&QTreeWidget::itemSelectionChanged,this,[this]{wheel_selected_.clear();rebuild_properties();});
  connect(scope_,&QComboBox::currentIndexChanged,this,[this]{rebuild_tree();});
  connect(search_,&QLineEdit::textChanged,this,[this]{filter();});connect(modified_,&QCheckBox::toggled,this,[this]{filter();});
  connect(reset_all,&QPushButton::clicked,this,[this]{reset({});});
}
void MaterialPanel::clear_hover(){hovered_.clear();if(hovered)hovered({});}
bool MaterialPanel::eventFilter(QObject *object,QEvent *event){
  if(object==this&&event->type()==QEvent::Hide){clear_hover();hover_suppressed_.clear();}
  if(object==tree_->viewport()){
    if(event->type()==QEvent::Leave){clear_hover();hover_suppressed_.clear();}
    if(event->type()==QEvent::MouseMove){auto *item=tree_->itemAt(static_cast<QMouseEvent *>(event)->position().toPoint());const auto key=item?item->data(0,Qt::UserRole+2).toString():QString{};
      if(key!=hover_suppressed_)hover_suppressed_.clear();if(key!=hovered_){clear_hover();hovered_=key;if(item&&key!=hover_suppressed_){if(hovered)hovered(item_surfaces(item));}}
    }
    if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick){auto *item=tree_->itemAt(static_cast<QMouseEvent *>(event)->position().toPoint());hover_suppressed_=item?item->data(0,Qt::UserRole+2).toString():QString{};clear_hover();hovered_=hover_suppressed_;
      auto *mouse=static_cast<QMouseEvent *>(event);if(item&&item->data(0,Qt::UserRole+3).isValid()&&mouse->button()==Qt::LeftButton&&mouse->position().x()>=tree_->visualItemRect(item).left()){
        if(event->type()==QEvent::MouseButtonPress)choose_selection_set(item,mouse->modifiers().testFlag(Qt::ControlModifier));return true;
      }}
  }else if(const auto id=object->property("materialWheelRow").toString();!id.isEmpty()){
    if((event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick)&&static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton){wheel_selected_=id;filter();}
    if(event->type()==QEvent::Wheel&&wheel_selected_!=id){auto *wheel=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(properties_->viewport()->mapFromGlobal(wheel->globalPosition().toPoint()),wheel->globalPosition(),wheel->pixelDelta(),wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),wheel->phase(),wheel->inverted(),wheel->source());QApplication::sendEvent(properties_->viewport(),&forwarded);return true;}
  }
  return QWidget::eventFilter(object,event);
}
void MaterialPanel::bind(std::shared_ptr<const Document> document,Snapshot *snapshot,int target,const std::string &group){
  if(document_==document&&snapshot_==snapshot&&target_==target&&group_==group)return;
  if(target_!=target||group_!=group){QSignalBlocker block(tree_);tree_->clearSelection();}
  document_=std::move(document);snapshot_=snapshot;target_=target;group_=group;rebuild_tree();
}
std::vector<MaterialPanel::Surface> MaterialPanel::surfaces() const{
  std::set<std::pair<size_t,size_t>> selected;
  for(auto *item:tree_->selectedItems())for(auto surface:item_surfaces(item))selected.emplace(surface.instance,surface.slot);
  std::vector<Surface> result;for(auto [i,s]:selected)result.push_back({i,s});return result;
}
void MaterialPanel::rebuild_tree(){
  clear_hover();hover_suppressed_.clear();wheel_selected_.clear();
  // 使用稳定身份恢复选中项，切换骨骼不会改变当前表面。
  std::set<QString> selected,expanded;for(QTreeWidgetItemIterator it(tree_);*it;++it){if((*it)->isSelected())selected.insert((*it)->data(0,Qt::UserRole+2).toString());if((*it)->isExpanded())expanded.insert((*it)->data(0,Qt::UserRole+2).toString());}
  QSignalBlocker blocker(tree_);tree_->clear();
  if(!document_||!snapshot_){rebuild_properties();return;}const auto &d=*document_;const auto &scene=d.loaded.scene;
  ObjectHierarchy hierarchy(d);
  std::string focus=group_;
  if(target_>=0&&size_t(target_)<hierarchy.targets.size()) {
    size_t host=size_t(target_);try{host=attachment_host(d,host);}catch(const std::exception &){}
    focus=hierarchy.targets[host];
  }
  std::map<size_t,size_t> targets;for(size_t t=0;t<d.catalog.targets.size();++t)targets[d.catalog.targets[t].instance]=t;
  std::set<std::string> represented=hierarchy.groups;for(const auto &id:hierarchy.instances)represented.insert(id);
  std::map<std::string,QTreeWidgetItem *> families;
  std::set<std::string> building;
  std::function<QTreeWidgetItem *(const std::string &)> family=[&](const std::string &id)->QTreeWidgetItem * {
    if(auto f=families.find(id);f!=families.end())return f->second;
    if(!building.insert(id).second)return nullptr;
    QTreeWidgetItem *parent=nullptr;auto p=hierarchy.parents[id];std::set<std::string> seen{id};
    if(scope_->currentIndex()!=0||id!=focus)while(!p.empty()&&seen.insert(p).second){if(represented.contains(p)){parent=family(p);break;}p=hierarchy.parents[p];}
    auto *item=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree_);item->setText(0,text(hierarchy.labels[id].empty()?id:hierarchy.labels[id]));
    item->setData(0,Qt::UserRole+2,"family/"+text(id));item->setToolTip(0,text(id));item->setExpanded(true);families[id]=item;building.erase(id);return item;
  };
  QTreeWidgetItem *first=nullptr;
  for(size_t i=0;i<scene.instances.size();++i){const auto &instance=scene.instances[i];if(instance.materials.empty()||!city::material_entry(document_->cities,instance.id))continue;
    auto target=targets.find(i);const auto &node=hierarchy.instances[i];
    if(scope_->currentIndex()==0&&!focus.empty()&&!hierarchy.contains(focus,node))continue;
    auto *parent=family(node);
    auto label=target==targets.end()?text(instance.instance_label.empty()?instance.id:instance.instance_label):text(d.catalog.targets[target->second].label);
    label=QStringLiteral("自身 · ")+label;
    if(instance.graft_source>=0)label=QStringLiteral("GeoGraft · ")+label;
    auto *object=parent?new QTreeWidgetItem(parent,{label}):new QTreeWidgetItem(tree_,{label});object->setData(0,Qt::UserRole+2,"object/"+text(instance.id));object->setToolTip(0,text(instance.id));
    const auto &slots=scene.meshes.at(instance.mesh).material_slots;
    add_selection_sets(object,i);
    for(size_t slot=0;slot<instance.materials.size();++slot){auto *surface=new QTreeWidgetItem(object,{text(slots.at(slot))});surface->setData(0,Qt::UserRole,qulonglong(i));surface->setData(0,Qt::UserRole+1,qulonglong(slot));surface->setData(0,Qt::UserRole+2,text(instance.id)+"\n"+text(slots[slot]));surface->setToolTip(0,text(scene.materials.at(instance.materials[slot]).id));
      if(!first||(target!=targets.end()&&int(target->second)==target_&&slot==0))first=surface;
    }
    object->setExpanded(expanded.contains(object->data(0,Qt::UserRole+2).toString())||(target!=targets.end()&&int(target->second)==target_));
  }
  bool restored=false;for(QTreeWidgetItemIterator it(tree_);*it;++it){const auto key=(*it)->data(0,Qt::UserRole+2).toString();if(expanded.contains(key))(*it)->setExpanded(true);if(selected.contains(key)){(*it)->setSelected(true);restored=true;}}
  if(!restored&&scope_->currentIndex()==0&&families.contains(focus))tree_->setCurrentItem(families.at(focus));
  else if(!restored&&first){tree_->setCurrentItem(first);first->parent()->setExpanded(true);}rebuild_properties();
}
J MaterialPanel::value(Surface s,const P &p)const{auto textures=document_->loaded.scene.textures;auto m=effective_material(document_->loaded.scene,snapshot_->material_overrides,s.instance,s.slot,textures);return material_value(m,textures,p);}
std::vector<std::pair<std::string,std::string>> MaterialPanel::selection_ids() const {
  std::vector<std::pair<std::string,std::string>> result;if(!document_)return result;
  const auto &scene=document_->loaded.scene;for(auto s:surfaces()){const auto &i=scene.instances.at(s.instance);result.emplace_back(i.id,scene.meshes.at(i.mesh).material_slots.at(s.slot));}return result;
}
void MaterialPanel::restore_selection(const std::vector<std::pair<std::string,std::string>> &selection) {
  if(!document_)return;
  {QSignalBlocker block(tree_);tree_->clearSelection();for(QTreeWidgetItemIterator it(tree_);*it;++it){auto *item=*it;if(!item->data(0,Qt::UserRole+1).isValid())continue;
    const auto &scene=document_->loaded.scene;const auto &i=scene.instances.at(size_t(item->data(0,Qt::UserRole).toULongLong()));const auto &slot=scene.meshes.at(i.mesh).material_slots.at(size_t(item->data(0,Qt::UserRole+1).toULongLong()));
    if(std::find(selection.begin(),selection.end(),std::make_pair(i.id,slot))!=selection.end()){item->setSelected(true);for(auto *p=item->parent();p;p=p->parent())p->setExpanded(true);}}}
  if(scope_->currentIndex()==0&&selection_ids().size()<selection.size()){scope_->setCurrentIndex(1);restore_selection(selection);return;}
  rebuild_properties();
}
void MaterialPanel::schedule_properties(){if(refresh_pending_)return;refresh_pending_=true;QTimer::singleShot(0,this,[this]{refresh_pending_=false;rebuild_properties();});}
void MaterialPanel::commit(const P &p,const J &v,int component){
  if(!document_||!snapshot_)return;
  try{auto next=snapshot_->material_overrides;const auto &scene=document_->loaded.scene;
    for(auto s:surfaces()){auto textures=scene.textures;auto m=effective_material(scene,next,s.instance,s.slot,textures);auto input=v;if(component>=0){input=material_value(m,textures,p);input.at(size_t(component))=v;}set_material_value(m,textures,p,input);ir::validate(m,textures.size());const auto &i=scene.instances.at(s.instance);next[i.id][scene.meshes.at(i.mesh).material_slots.at(s.slot)][p.id]=input;}
    if(next==snapshot_->material_overrides)return;
    auto apply=[&]{snapshot_->material_overrides=std::move(next);status_->clear();if(changed)changed();};
    if(edit_requested)edit_requested(QStringLiteral("修改材质：")+text(p.label),apply);else apply();
    // 数字输入期间保留焦点和拖动状态；仅更新筛选状态。
    filter();
  }catch(const std::exception &e){status_->setText(text(e.what()));schedule_properties();}
}
void MaterialPanel::reset(const std::string &parameter){
  if(edit_requested){auto callback=std::move(edit_requested);callback(parameter.empty()?QStringLiteral("还原所选表面"):QStringLiteral("还原材质参数"),[&]{reset(parameter);});edit_requested=std::move(callback);return;}
  if(!document_||!snapshot_)return;
  if((parameter.empty()||parameter=="uv_set")&&uv_reset_requested)uv_reset_requested(surfaces());if(parameter=="uv_set")return;
  auto previous=snapshot_->material_overrides;const auto &scene=document_->loaded.scene;
  for(auto s:surfaces()){const auto &i=scene.instances.at(s.instance);auto object=snapshot_->material_overrides.find(i.id);if(object==snapshot_->material_overrides.end())continue;const auto &slot=scene.meshes.at(i.mesh).material_slots.at(s.slot);if(parameter.empty())object->second.erase(slot);else if(auto patch=object->second.find(slot);patch!=object->second.end())patch->second.erase(parameter);}
  prune_material_overrides(scene,snapshot_->material_overrides);if(previous!=snapshot_->material_overrides&&changed)changed();schedule_properties();
}
void MaterialPanel::rebuild_properties(){
  if(interaction_changed)interaction_changed(false);
  const auto generation=++properties_generation_;restoring_scroll_=true;
  // setWidget destroys the old controls and temporarily collapses the range.
  // Restore only after the new layout has settled, including rapid selection changes.
  QTimer::singleShot(0,this,[this,generation]{if(generation!=properties_generation_)return;auto *body=properties_->widget();if(body&&body->layout()){body->layout()->activate();body->resize(body->width(),std::max(properties_->viewport()->height(),body->sizeHint().height()));}properties_->verticalScrollBar()->setValue(scroll_position_);restoring_scroll_=false;});
  auto *body=new QWidget;auto *layout=new QVBoxLayout(body);layout->setContentsMargins(6,6,6,6);layout->setSpacing(5);properties_->setWidget(body);status_->clear();
  const auto selected=surfaces();if(!document_||!snapshot_||selected.empty()){heading_->setText(QStringLiteral("选择左侧子材质查看参数"));layout->addStretch();return;}
  const auto &scene=document_->loaded.scene;const auto first=selected.front();const auto &instance=scene.instances.at(first.instance);
  heading_->setText(selected.size()==1?QStringLiteral("表面：%1").arg(text(scene.meshes.at(instance.mesh).material_slots.at(first.slot))):QStringLiteral("已选 %1 个表面 · 不同数值标记为“多值”").arg(selected.size()));
  std::vector<std::pair<ir::Material,std::vector<ir::Texture>>> resolved;
  for(auto s:selected){auto textures=scene.textures;auto m=effective_material(scene,snapshot_->material_overrides,s.instance,s.slot,textures);resolved.emplace_back(std::move(m),std::move(textures));}
  std::string group;QWidget *group_body=nullptr;QVBoxLayout *rows=nullptr;
  for(const auto &p:material_parameters()){
    if(group!=p.group){group=p.group;auto *header=new QToolButton;header->setText(text(group).section('/',0,0).trimmed());header->setToolTip(text(group));header->setCheckable(true);header->setChecked(true);header->setArrowType(Qt::DownArrow);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);header->setProperty("materialGroupHeader",true);layout->addWidget(header);
      group_body=new QWidget;group_body->setProperty("materialGroup",true);rows=new QVBoxLayout(group_body);rows->setContentsMargins(0,0,0,4);rows->setSpacing(4);layout->addWidget(group_body);header->setChecked(!collapsed_[group]);group_body->setVisible(header->isChecked());header->setArrowType(header->isChecked()?Qt::DownArrow:Qt::RightArrow);connect(header,&QToolButton::toggled,group_body,[this,header,group_body,group](bool open){collapsed_[group]=!open;group_body->setVisible(open);header->setArrowType(open?Qt::DownArrow:Qt::RightArrow);});}
    const auto current=material_value(resolved.front().first,resolved.front().second,p);bool mixed=false;for(const auto &r:resolved)mixed|=material_value(r.first,r.second,p)!=current;
    auto *row=new QWidget;row->setObjectName("materialRow/"+text(p.id));row->setProperty("materialParameter",text(p.id));row->setProperty("materialSearchText",text(p.label+" "+p.id+" "+p.group));auto *horizontal=new QHBoxLayout(row);horizontal->setContentsMargins(3,1,3,1);horizontal->setSpacing(4);
    auto *label=new ParameterLabel(text(p.label).section(QStringLiteral(" · "),0,0));label->setFixedWidth(84);label->setToolTip(text(p.label));horizontal->addWidget(label);auto *reset_button=new QToolButton;reset_button->setObjectName("materialRevert/"+text(p.id));reset_button->setText(QStringLiteral("↺"));reset_button->setToolTip(QStringLiteral("还原此参数"));reset_button->setFixedWidth(22);connect(reset_button,&QToolButton::clicked,this,[this,id=p.id]{reset(id);});
    auto *controls=new QHBoxLayout;controls->setSpacing(3);horizontal->addLayout(controls,1);horizontal->addWidget(reset_button);const auto *parameter=&p;
    if(p.kind==P::number){auto *spin=new NumericSpinBox(true);spin->setButtonSymbols(QAbstractSpinBox::NoButtons);spin->setFixedWidth(74);spin->setObjectName("material/"+text(p.id));spin->setDecimals(6);spin->setRange(std::min(p.minimum,current.get<double>()),std::max(p.maximum,current.get<double>()));spin->setSingleStep(p.step);spin->setKeyboardTracking(false);spin->sync(current.get<double>());if(mixed)spin->setSuffix(QStringLiteral("（多值）"));auto *slider=new NumericSlider;slider->setMinimumWidth(30);slider->sync(spin->value(),p.minimum,std::min(p.maximum,std::max(1.,std::abs(spin->value())*2)),p.step);controls->addWidget(slider,1);controls->addWidget(spin);
      auto sync=[=]{slider->sync(spin->value(),p.minimum,std::min(p.maximum,std::max(1.,std::abs(spin->value())*2)),p.step);};
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this,parameter,spin,sync](double v){spin->setSuffix({});commit(*parameter,v);sync();});connect(spin->findChild<QLineEdit *>(),&QLineEdit::textEdited,spin,[spin]{spin->setProperty("materialTyped",true);});
      connect(spin,&QDoubleSpinBox::editingFinished,this,[this,parameter,spin,sync]{if(!spin->property("materialTyped").toBool())return;spin->setProperty("materialTyped",false);spin->setSuffix({});commit(*parameter,spin->value());sync();});
      slider->edited=[spin](double v){spin->setValue(v);};slider->wheeled=[spin](QWheelEvent *event){QApplication::sendEvent(spin,event);};
      connect(slider,&QSlider::sliderPressed,this,[this]{if(interaction_changed)interaction_changed(true);});connect(slider,&QSlider::sliderReleased,this,[this]{if(interaction_changed)interaction_changed(false);});
    }else if(p.kind==P::boolean){auto *check=new QCheckBox(mixed?QStringLiteral("多值"):QStringLiteral("启用"));check->setObjectName("material/"+text(p.id));check->setTristate(mixed);check->setCheckState(mixed?Qt::PartiallyChecked:(current.get<bool>()?Qt::Checked:Qt::Unchecked));controls->addWidget(check);connect(check,&QCheckBox::checkStateChanged,this,[this,parameter,check](Qt::CheckState state){if(state==Qt::PartiallyChecked)return;check->setTristate(false);check->setText(QStringLiteral("启用"));commit(*parameter,state==Qt::Checked);});
    }else if(p.kind==P::choice){auto *combo=new QComboBox;combo->setObjectName("material/"+text(p.id));for(const auto &name:p.choices)combo->addItem(text(name));combo->setCurrentIndex(mixed?-1:current.get<int>());combo->setPlaceholderText(QStringLiteral("多值"));controls->addWidget(combo,1);connect(combo,&QComboBox::currentIndexChanged,this,[this,parameter](int index){if(index>=0)commit(*parameter,index);});
    }else if(p.kind==P::color||p.kind==P::vector){
      QPushButton *swatch=nullptr;
      if(p.kind==P::color){swatch=new QPushButton(mixed?QStringLiteral("多值"):QStringLiteral("颜色"));swatch->setFixedWidth(24);swatch->setObjectName("material/"+text(p.id));controls->addWidget(swatch);auto color=QColor::fromRgbF(std::clamp(srgb(current[0]),0.,1.),std::clamp(srgb(current[1]),0.,1.),std::clamp(srgb(current[2]),0.,1.));swatch->setStyleSheet("background-color: "+color.name()+"; border: 1px solid #888;");connect(swatch,&QPushButton::clicked,this,[this,parameter]{const auto selected=surfaces();if(selected.empty())return;const auto c=value(selected.front(),*parameter);const auto latest=QColor::fromRgbF(std::clamp(srgb(c[0]),0.,1.),std::clamp(srgb(c[1]),0.,1.),std::clamp(srgb(c[2]),0.,1.));const auto chosen=QColorDialog::getColor(latest,this,QStringLiteral("选择颜色（sRGB）"));if(chosen.isValid()){commit(*parameter,J::array({linear(chosen.redF()),linear(chosen.greenF()),linear(chosen.blueF())}));schedule_properties();}});}
      for(int component=0;component<3;++component){auto *spin=new NumericSpinBox(true);spin->setButtonSymbols(QAbstractSpinBox::NoButtons);spin->setMinimumWidth(42);spin->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);spin->setObjectName("material/"+text(p.id)+"/"+QString::number(component));spin->setDecimals(6);const double shown=p.kind==P::color?srgb(current[size_t(component)]):current[size_t(component)].get<double>();spin->setRange(0,std::max(p.maximum,shown));spin->setMinimumWidth(42);spin->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);spin->setSingleStep(p.step);spin->setKeyboardTracking(false);spin->setPrefix(QString("%1 ").arg("RGB"[component]));spin->sync(shown);if(mixed)spin->setSuffix(QStringLiteral(" *"));controls->addWidget(spin,1);connect(spin,&QDoubleSpinBox::valueChanged,this,[this,parameter,component,spin,swatch](double v){spin->setSuffix({});commit(*parameter,parameter->kind==P::color?linear(v):v,component);if(swatch){const auto selected=surfaces();if(!selected.empty()){auto c=value(selected.front(),*parameter);auto color=QColor::fromRgbF(std::clamp(srgb(c[0]),0.,1.),std::clamp(srgb(c[1]),0.,1.),std::clamp(srgb(c[2]),0.,1.));swatch->setStyleSheet("background-color: "+color.name()+"; border: 1px solid #888;");swatch->setText({});}}});}
    }else if(p.kind==P::texture){auto *button=new QPushButton;button->setObjectName("material/"+text(p.id));button->setText(mixed?QStringLiteral("多张贴图"):current.is_null()?QStringLiteral("无贴图 · 点击选择"):QFileInfo(text(current.at("file").get<std::string>())).fileName());button->setToolTip(current.is_null()?QStringLiteral("选择场景贴图或图像文件"):text(current.dump(2)));controls->addWidget(button,1);
      connect(button,&QPushButton::clicked,this,[this,parameter,button]{QMenu menu;auto *browse=menu.addAction(QStringLiteral("浏览图像文件…"));auto *clear=menu.addAction(QStringLiteral("移除贴图"));menu.addSeparator();std::map<QAction *,J> available;const auto &scene=document_->loaded.scene;for(size_t i=0;i<scene.textures.size();++i){ir::Material m;m.*parameter->texture_member=int(i);auto v=material_value(m,scene.textures,*parameter);auto *action=menu.addAction(QFileInfo(text(v["file"].get<std::string>())).fileName());action->setToolTip(text(v["file"].get<std::string>()));available.emplace(action,std::move(v));}auto *action=menu.exec(button->mapToGlobal(QPoint(0,button->height())));if(!action)return;
        if(action==clear)commit(*parameter,J{});else if(action==browse){auto file=QFileDialog::getOpenFileName(this,QStringLiteral("选择材质贴图"),{},QStringLiteral("图像 (*.png *.jpg *.jpeg *.tif *.tiff *.exr *.hdr *.bmp *.tga *.webp);;所有文件 (*)"));if(file.isEmpty())return;commit(*parameter,J{{"file",utf8(QFileInfo(file).absoluteFilePath())}});}else commit(*parameter,available.at(action));schedule_properties();});
    }
    if(p.kind==P::texture){auto *preview=new QLabel;thumbnail(preview,!mixed&&!current.is_null()?text(current.at("file").get<std::string>()):QString{});controls->addWidget(preview);if(auto *button=row->findChild<QPushButton *>()){button->setMinimumWidth(45);button->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);}}
    if(p.kind==P::texture){auto *locate=new QToolButton;locate->setObjectName("materialLocate/"+text(p.id));locate->setIcon(folder_icon());locate->setToolTip(QStringLiteral("在内容库中定位"));locate->setFixedWidth(22);locate->setEnabled(!mixed&&!current.is_null());horizontal->insertWidget(horizontal->count()-1,locate);connect(locate,&QToolButton::clicked,this,[this,parameter]{const auto selected=surfaces();if(selected.empty()||!locate_file)return;const auto v=value(selected.front(),*parameter);if(!v.is_null())locate_file(text(v.at("file").get<std::string>()));});}
    if(p.kind==P::color)if(auto *button=row->findChild<QPushButton *>()){button->setFixedWidth(24);button->setText(mixed?QStringLiteral("*"):QString{});button->setToolTip(QStringLiteral("选择颜色；HDR 分量可在右侧直接输入"));}
    if(p.kind==P::choice)if(auto *combo=row->findChild<QComboBox *>()){combo->setMinimumWidth(40);combo->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);}
    auto children=row->findChildren<QWidget *>();children.push_back(row);for(auto *child:children){child->setProperty("materialWheelRow",text(p.id));child->installEventFilter(this);}
    rows->addWidget(row);
  }
  auto *uv_row=new QWidget;uv_row->setObjectName("materialRow/uv_set");uv_row->setProperty("materialParameter","uv_set");uv_row->setProperty("materialSearchText",QStringLiteral("UV Set UV ? ???? ??"));
  auto *uv_layout=new QHBoxLayout(uv_row);uv_layout->setContentsMargins(3,1,3,1);auto *uv_label=new ParameterLabel(QStringLiteral("UV Set"));uv_label->setFixedWidth(84);uv_layout->addWidget(uv_label);
  auto *uv_combo=new QComboBox;uv_combo->setObjectName("material/uv_set");uv_combo->setMinimumWidth(40);uv_combo->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);uv_layout->addWidget(uv_combo,1);rows->insertWidget(0,uv_row);populate_uv_combo(uv_combo);
  auto *uv_revert=new QToolButton;uv_revert->setObjectName("materialRevert/uv_set");uv_revert->setText(QStringLiteral("↺"));uv_revert->setFixedWidth(22);uv_revert->setToolTip(QStringLiteral("还原至加载场景或最近应用的材质 UV Set"));uv_layout->addWidget(uv_revert);connect(uv_revert,&QToolButton::clicked,this,[this]{reset("uv_set");});
  connect(uv_combo,&QComboBox::activated,this,[this,uv_combo](int index){const auto data=uv_combo->itemData(index).toString();if(data.isEmpty()||!uv_requested)return;const auto j=J::parse(utf8(data));uv_requested({j.at("uri"),j.at("owner"),j.at("label")},surfaces());});
  for(auto *widget:{uv_row,static_cast<QWidget *>(uv_combo),static_cast<QWidget *>(uv_label)}){widget->setProperty("materialWheelRow","uv_set");widget->installEventFilter(this);}
  auto *source=new QTreeWidget;source->setObjectName("materialSourceChannels");source->setHeaderLabels({QStringLiteral("源材质通道"),QStringLiteral("原值 / 状态")});source->setMinimumHeight(150);source->setMaximumHeight(260);source->setColumnWidth(0,180);
  for(const auto &c:scene.materials.at(instance.materials.at(first.slot)).source_channels){auto *item=new QTreeWidgetItem(source,{text(c.label),text(c.value)+(c.mapped?QStringLiteral(" · 已转换"):QStringLiteral(" · 未映射"))});item->setToolTip(0,text(c.id+"\n"+c.type));item->setToolTip(1,text(c.image));if(!c.mapped)item->setForeground(1,QColor(210,150,70));}
  auto *source_header=new QToolButton;source_header->setText(QStringLiteral("源通道与兼容信息（只读）"));source_header->setCheckable(true);source_header->setArrowType(Qt::RightArrow);source_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);layout->addWidget(source_header);layout->addWidget(source);source->hide();connect(source_header,&QToolButton::toggled,source,[source_header,source](bool on){source->setVisible(on);source_header->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});
  source_header->setToolTip(QStringLiteral("显示第一个所选表面的原始通道。上方参数为 Cycles 转换后的可编辑值；未映射通道不参与渲染。"));layout->addStretch();filter();
}
void MaterialPanel::filter(){
  if(!properties_->widget()||!document_||!snapshot_)return;const auto query=search_->text().trimmed();const auto selected=surfaces();const auto &scene=document_->loaded.scene;
  for(auto *row:properties_->widget()->findChildren<QWidget *>()){auto id=row->property("materialParameter");if(!id.isValid())continue;bool edited=false;
    if(id.toString()=="uv_set")for(auto s:selected)edited|=daz::material_uv_baseline(scene.materials.at(scene.instances.at(s.instance).materials.at(s.slot))).has_value();
    for(auto s:selected){const auto &i=scene.instances.at(s.instance);auto object=snapshot_->material_overrides.find(i.id);if(object==snapshot_->material_overrides.end())continue;auto slot=object->second.find(scene.meshes.at(i.mesh).material_slots.at(s.slot));edited|=slot!=object->second.end()&&slot->second.contains(utf8(id.toString()));}
    row->setVisible((!modified_->isChecked()||edited)&&(query.isEmpty()||row->property("materialSearchText").toString().contains(query,Qt::CaseInsensitive)));
    if(auto *label=row->findChild<QLabel *>()){auto font=label->font();font.setBold(edited);label->setFont(font);}
    auto palette=row->palette();const bool active=id.toString()==wheel_selected_;palette.setColor(QPalette::Window,palette.color(QPalette::Highlight).darker(200));for(auto group:{QPalette::Active,QPalette::Inactive})palette.setColor(group,QPalette::WindowText,active?palette.color(group,QPalette::HighlightedText):properties_->palette().color(group,QPalette::WindowText));row->setPalette(palette);row->setAutoFillBackground(active);
  }
  auto *layout=properties_->widget()->layout();for(int i=1;i<layout->count();++i){auto *group=layout->itemAt(i)->widget();if(!group||!group->property("materialGroup").toBool())continue;auto *header=qobject_cast<QToolButton *>(layout->itemAt(i-1)->widget());if(!header)continue;bool any=false;for(auto *row:group->findChildren<QWidget *>(QString{},Qt::FindDirectChildrenOnly))if(row->property("materialParameter").isValid()&&!row->isHidden())any=true;header->setVisible(any);group->setVisible(any&&(header->isChecked()||!query.isEmpty()));}
}
}
