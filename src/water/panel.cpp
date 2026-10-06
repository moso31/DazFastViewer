#include "water/panel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <QSignalBlocker>
#include <QApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QHeaderView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QStyle>
#include <cmath>

namespace dfv::water {
class SourceItem final:public QTreeWidgetItem {
public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem &other)const override{
    if(treeWidget()->sortColumn()==2){const auto a=data(2,Qt::UserRole).toDouble(),b=other.data(2,Qt::UserRole).toDouble();if(a!=b)return a<b;return data(0,Qt::UserRole).toString()<other.data(0,Qt::UserRole).toString();}
    return QTreeWidgetItem::operator<(other);
  }
};
void Panel::select(const QString &key){
  selected_=key;
  for(auto *w:findChildren<QWidget *>())if(w->property("waterWheelKey").isValid()){
    const bool active=!key.isEmpty()&&w->property("waterWheelKey").toString()==key;
    if(w->property("waterSelected").toBool()!=active){w->setProperty("waterSelected",active);w->style()->unpolish(w);w->style()->polish(w);w->update();}
  }
}
bool Panel::eventFilter(QObject *object,QEvent *event){
  auto *widget=qobject_cast<QWidget *>(object);QString key;
  if(widget&&(widget==this||isAncestorOf(widget)))for(auto *w=widget;w&&w!=this;w=w->parentWidget())if(w->property("waterWheelKey").isValid()){key=w->property("waterWheelKey").toString();break;}
  if(event->type()==QEvent::MouseButtonPress&&static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton){
    if(widget==sources_->viewport()){const auto *item=sources_->itemAt(static_cast<QMouseEvent *>(event)->position().toPoint());if(item)key="source:"+item->data(0,Qt::UserRole).toString();}
    if(!key.isEmpty())select(key);
    else {bool propagated=false;if(widget)for(auto *w:findChildren<QWidget *>())if(!selected_.isEmpty()&&w->property("waterWheelKey").toString()==selected_&&widget->isAncestorOf(w)){propagated=true;break;}if(!propagated)select({});}
  }
  if(event->type()==QEvent::Hide&&object==this)select({});
  if(event->type()==QEvent::Wheel&&widget&&(widget==sources_->viewport()||!key.isEmpty())&&(key.isEmpty()||key!=selected_||busy_)){
    for(auto *p=parentWidget();p;p=p->parentWidget())if(auto *scroll=qobject_cast<QScrollArea *>(p)){
      auto *w=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(scroll->viewport()->mapFromGlobal(w->globalPosition()),w->globalPosition(),w->pixelDelta(),w->angleDelta(),w->buttons(),w->modifiers(),w->phase(),w->inverted());QApplication::sendEvent(scroll->viewport(),&forwarded);event->accept();return true;
    }event->ignore();return true;
  }
  return QWidget::eventFilter(object,event);
}
void Panel::expand_sources(){
  sources_->doItemsLayout();int height=sources_->header()->height()+2*sources_->frameWidth();
  for(int row=0;row<sources_->topLevelItemCount();++row)height+=sources_->sizeHintForRow(row);
  sources_->setFixedHeight(height);sources_->setColumnWidth(1,100);sources_->setColumnWidth(2,112);
}
Panel::Panel(QWidget *parent):QWidget(parent){
  setObjectName("WaterParameters");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);auto *form=new QFormLayout;layout->addLayout(form);
  // Preserve the same native spin-box frame/arrows as GroundPanel in every state.
  setStyleSheet(QStringLiteral("QLabel[waterSelected=\"true\"] { color: palette(highlight); }"));
  auto scalar=[&](const char *id,const QString &label,double min,double max,double step,int decimals=3){auto *spin=new QDoubleSpinBox;spin->setObjectName(QString::fromLatin1(id));spin->setDecimals(decimals);spin->setRange(min,max);spin->setSingleStep(step);spin->setKeyboardTracking(false);fields_[id]=spin;form->addRow(label,spin);connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{submit();});};
  scalar("x",QStringLiteral("位置 X（米）"),-1000000,1000000,1);scalar("y",QStringLiteral("位置 Y（米）"),-1000000,1000000,1);scalar("level",QStringLiteral("水位（米）"),-1000000,1000000,.1);
  scalar("width",QStringLiteral("宽度（米）"),1,1000000,100);scalar("length",QStringLiteral("长度（米）"),1,1000000,100);
  scalar("density",QStringLiteral("顶点密度（倍）"),.25,8,.25,2);
  fields_["density"]->setToolTip(QStringLiteral("1 倍保持原密度；上限为同一视角下原顶点数的 8 倍。实际数量受自适应分块与视口预算限制。交界细节由交界采样间距单独控制，无需重算。"));
  scalar("depth",QStringLiteral("水体深度（米）"),.01,100000,1);scalar("clarity",QStringLiteral("清澈距离（米）"),.01,100000,1);
  fields_["depth"]->setToolTip(QStringLiteral("整片水体的统一等效光学深度，用于水色与透射衰减；不会创建海底。"));
  scalar("wave_height",QStringLiteral("波高（米）"),0,1000,.1);scalar("wavelength",QStringLiteral("主波长（米）"),.1,100000,1);scalar("steepness",QStringLiteral("波峰陡峭度"),0,.9,.05);
  scalar("direction",QStringLiteral("风向（度）"),-36000,36000,5);scalar("time",QStringLiteral("时间（秒）"),-10000000,10000000,.1);
  fields_["time"]->setToolTip(QStringLiteral("仅计算指定时刻的静态水面；不会自动播放。改变时间后需要重算交界。"));
  scalar("ripples",QStringLiteral("细波纹强度"),0,2,.05);scalar("roughness",QStringLiteral("表面粗糙度"),.005,1,.01);
  scalar("foam_width",QStringLiteral("岸边泡沫宽度（米）"),.01,1000,.1);scalar("foam_strength",QStringLiteral("泡沫覆盖率"),0,1,.05);scalar("seed",QStringLiteral("随机种子"),0,4294967295.,1,0);
  scalar("foam_uv_scale",QStringLiteral("泡沫 UV 缩放"),.01,100,.1,2);
  fields_["foam_uv_scale"]->setToolTip(QStringLiteral("1 保持原纹理大小；数值越大，泡沫纹理越细密。仅调整泡沫纹理，不改变泡沫带宽度或细波纹，无需重算。"));
  color_=new QPushButton(QStringLiteral("选择水色…"));form->addRow(QStringLiteral("深水颜色"),color_);connect(color_,&QPushButton::clicked,this,[this]{const auto c=config_.color;const auto selected=QColorDialog::getColor(QColor::fromRgbF(c.x,c.y,c.z),this,QStringLiteral("水体颜色"));if(selected.isValid()){config_.color={float(selected.redF()),float(selected.greenF()),float(selected.blueF())};submit();}});
  coast_=new QCheckBox(QStringLiteral("海岸线计算"));coast_->setObjectName("WaterCoast");layout->addWidget(coast_);connect(coast_,&QCheckBox::toggled,this,[this]{submit();});
  scan_=new QCheckBox(QStringLiteral("扫描场景中的可见对象"));scan_->setObjectName("WaterScanScene");scan_->setToolTip(QStringLiteral("自动对象使用表面交界；人物、柱体和船体建议在列表中明确选择排水体积。"));layout->addWidget(scan_);connect(scan_,&QCheckBox::toggled,this,[this]{submit();});
  sources_=new QTreeWidget;sources_->setObjectName("WaterSources");sources_->setHeaderLabels({QStringLiteral("参与对象"),QStringLiteral("交界方式")});sources_->setRootIsDecorated(false);sources_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);sources_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);sources_->header()->setStretchLastSection(false);sources_->header()->setSectionResizeMode(0,QHeaderView::Stretch);sources_->header()->setSectionResizeMode(1,QHeaderView::Fixed);layout->addWidget(sources_);connect(sources_,&QTreeWidget::itemChanged,this,[this]{submit();});
  sources_->setColumnCount(3);sources_->headerItem()->setText(2,QStringLiteral("AABB 体积 (m³)"));sources_->headerItem()->setToolTip(2,QStringLiteral("点击按当前世界坐标 AABB 体积排序；组使用成员包围盒的并集。"));sources_->header()->setSectionResizeMode(2,QHeaderView::Fixed);sources_->setSortingEnabled(true);sources_->sortItems(2,Qt::DescendingOrder);
  auto *precision=new QFormLayout;layout->addLayout(precision);auto *spin=new QDoubleSpinBox;spin->setObjectName("precision");spin->setDecimals(3);spin->setRange(.02,1000);spin->setValue(.5);spin->setSingleStep(.1);spin->setKeyboardTracking(false);fields_["precision"]=spin;precision->addRow(QStringLiteral("交界采样间距（米）"),spin);spin->setToolTip(QStringLiteral("数值越小越精细，计算与网格开销越大；独立于远海 LOD。"));connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{submit();});
  auto *buttons=new QHBoxLayout;calculate_=new QPushButton(QStringLiteral("重算"));calculate_->setObjectName("WaterRecalculate");cancel_=new QPushButton(QStringLiteral("取消"));cancel_->setEnabled(false);buttons->addWidget(calculate_);buttons->addWidget(cancel_);layout->addLayout(buttons);
  connect(calculate_,&QPushButton::clicked,this,[this]{if(changed)changed(config_,true);});connect(cancel_,&QPushButton::clicked,this,[this]{if(cancel)cancel();});
  message_=new QLabel;message_->setWordWrap(true);message_->setObjectName("WaterStatus");layout->addWidget(message_);hide();
  for(const auto &[id,field]:fields_){const auto key=QString::fromStdString(id);field->setProperty("waterWheelKey",key);field->setFocusPolicy(Qt::StrongFocus);auto *row=field==spin?precision:form;if(auto *label=row->labelForField(field))label->setProperty("waterWheelKey",key);}
  qApp->installEventFilter(this);
  layout->removeWidget(sources_);layout->addWidget(sources_);
}
void Panel::submit(){
  if(binding_||busy_)return;
#define DFV_WATER_FIELD(n) config_.n=fields_.at(#n)->value()
  DFV_WATER_FIELD(density);DFV_WATER_FIELD(foam_uv_scale);
  DFV_WATER_FIELD(x);DFV_WATER_FIELD(y);DFV_WATER_FIELD(level);DFV_WATER_FIELD(width);DFV_WATER_FIELD(length);DFV_WATER_FIELD(depth);DFV_WATER_FIELD(clarity);DFV_WATER_FIELD(wave_height);DFV_WATER_FIELD(wavelength);DFV_WATER_FIELD(steepness);DFV_WATER_FIELD(direction);DFV_WATER_FIELD(time);DFV_WATER_FIELD(ripples);DFV_WATER_FIELD(roughness);DFV_WATER_FIELD(foam_width);DFV_WATER_FIELD(foam_strength);DFV_WATER_FIELD(precision);
#undef DFV_WATER_FIELD
  config_.seed=uint32_t(fields_.at("seed")->value());config_.coast=coast_->isChecked();config_.scan_scene=scan_->isChecked();std::map<std::string,bool> checked;
  for(int i=0;i<sources_->topLevelItemCount();++i){auto *item=sources_->topLevelItem(i);if(item->checkState(0)==Qt::Checked){auto *mode=qobject_cast<QComboBox *>(sources_->itemWidget(item,1));checked.emplace(item->data(0,Qt::UserRole).toString().toStdString(),mode&&mode->currentIndex()==1);}}
  // View sorting must not reorder overlapping group/object selection precedence.
  std::erase_if(config_.sources,[&](const auto &s){return !checked.contains(s.id);});for(auto &s:config_.sources){s.volume=checked.at(s.id);checked.erase(s.id);}for(const auto &[id,volume]:checked)config_.sources.push_back({id,volume});
  if(changed)changed(config_,false);
}
void Panel::bind(const Water *water,const std::vector<Candidate> &objects){
  if(!water||bound_id_!=water->id)select({});bound_id_=water?water->id:std::string{};
  setVisible(water!=nullptr);if(!water)return;binding_=true;config_=water->config;
#define DFV_WATER_FIELD(n) fields_.at(#n)->setValue(config_.n)
  DFV_WATER_FIELD(density);DFV_WATER_FIELD(foam_uv_scale);
  DFV_WATER_FIELD(x);DFV_WATER_FIELD(y);DFV_WATER_FIELD(level);DFV_WATER_FIELD(width);DFV_WATER_FIELD(length);DFV_WATER_FIELD(depth);DFV_WATER_FIELD(clarity);DFV_WATER_FIELD(wave_height);DFV_WATER_FIELD(wavelength);DFV_WATER_FIELD(steepness);DFV_WATER_FIELD(direction);DFV_WATER_FIELD(time);DFV_WATER_FIELD(ripples);DFV_WATER_FIELD(roughness);DFV_WATER_FIELD(foam_width);DFV_WATER_FIELD(foam_strength);DFV_WATER_FIELD(precision);DFV_WATER_FIELD(seed);
#undef DFV_WATER_FIELD
  coast_->setChecked(config_.coast);scan_->setChecked(config_.scan_scene);sources_->setSortingEnabled(false);sources_->clear();
  auto available=objects;for(const auto &s:config_.sources)if(std::none_of(available.begin(),available.end(),[&](const auto &v){return v.id==s.id;}))available.push_back({s.id,"[对象已移除] "+s.id,-1});
  for(const auto &[id,label,volume]:available){auto *item=new SourceItem(sources_,{QString::fromStdString(label)});item->setData(0,Qt::UserRole,QString::fromStdString(id));item->setToolTip(0,QString::fromStdString(id));item->setData(2,Qt::UserRole,volume);item->setText(2,volume<0?QStringLiteral("待更新"):QString::number(volume,'g',5));item->setToolTip(2,volume<0?QStringLiteral("等待视口几何"):QString::number(volume,'g',15)+QStringLiteral(" m³"));item->setTextAlignment(2,Qt::AlignRight|Qt::AlignVCenter);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);auto found=std::find_if(config_.sources.begin(),config_.sources.end(),[&](const auto &s){return s.id==id;});item->setCheckState(0,found!=config_.sources.end()?Qt::Checked:Qt::Unchecked);auto *mode=new QComboBox;mode->addItems({QStringLiteral("表面交界"),QStringLiteral("排水体积")});mode->setCurrentIndex(found!=config_.sources.end()&&found->volume?1:0);sources_->setItemWidget(item,1,mode);connect(mode,&QComboBox::currentIndexChanged,this,[this,item]{if(item->checkState(0)!=Qt::Checked)item->setCheckState(0,Qt::Checked);else submit();});}
  sources_->setSortingEnabled(true);
  for(int row=0;row<sources_->topLevelItemCount();++row){auto *item=sources_->topLevelItem(row);auto *mode=sources_->itemWidget(item,1);mode->setProperty("waterWheelKey","source:"+item->data(0,Qt::UserRole).toString());mode->setFocusPolicy(Qt::StrongFocus);}
  expand_sources();select(selected_);binding_=false;busy(busy_);
}
void Panel::busy(bool value){busy_=value;for(auto &[id,field]:fields_)field->setEnabled(!value);color_->setEnabled(!value);coast_->setEnabled(!value);scan_->setEnabled(!value);sources_->setEnabled(!value);calculate_->setEnabled(!value&&config_.coast);cancel_->setEnabled(value);}
}
