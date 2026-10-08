#include "editor/parameter_widgets.h"
#include "cloud/panel.h"
#include <QApplication>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QStyle>
#include <set>

namespace dfv::cloud {
class SourceItem final:public QTreeWidgetItem {
public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem &other)const override{
    const auto a=data(0,Qt::UserRole+1).toDouble(),b=other.data(0,Qt::UserRole+1).toDouble();if(a!=b)return a<b;return data(0,Qt::UserRole).toString()<other.data(0,Qt::UserRole).toString();
  }
};
void Panel::select(const QString &key){
  selected_=key;for(auto *label:findChildren<QLabel *>())if(label->property("cloudWheelKey").isValid()){
    label->setProperty("cloudSelected",!key.isEmpty()&&label->property("cloudWheelKey").toString()==key);label->style()->unpolish(label);label->style()->polish(label);label->update();
  }
}
bool Panel::eventFilter(QObject *object,QEvent *event){
  auto *widget=qobject_cast<QWidget *>(object);QString key;const bool inside=widget&&(widget==this||isAncestorOf(widget));
  if(inside)for(auto *w=widget;w&&w!=this;w=w->parentWidget())if(w->property("cloudWheelKey").isValid()){key=w->property("cloudWheelKey").toString();break;}
  if(event->type()==QEvent::MouseButtonPress&&static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton){
    if(!key.isEmpty())select(key);else {bool propagated=false;const auto point=static_cast<QMouseEvent *>(event)->globalPosition().toPoint();if(widget&&!selected_.isEmpty())for(auto *row:findChildren<QWidget *>())if(row->property("cloudWheelKey").toString()==selected_&&widget->isAncestorOf(row)&&row->rect().contains(row->mapFromGlobal(point)))propagated=true;if(!propagated)select({});}
  }
  if(event->type()==QEvent::Hide&&object==this)select({});
  if(event->type()==QEvent::Wheel&&inside){
    auto *spin=qobject_cast<QDoubleSpinBox *>(widget);if(!spin&&widget)spin=qobject_cast<QDoubleSpinBox *>(widget->parentWidget());
    if(!key.isEmpty()&&key==selected_&&spin&&isEnabled())return QWidget::eventFilter(object,event);
    QScrollArea *outer=nullptr;for(auto *p=parentWidget();p;p=p->parentWidget())if(auto *s=qobject_cast<QScrollArea *>(p))outer=s;
    if(outer){auto *w=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(outer->viewport()->mapFromGlobal(w->globalPosition()),w->globalPosition(),w->pixelDelta(),w->angleDelta(),w->buttons(),w->modifiers(),w->phase(),w->inverted());QApplication::sendEvent(outer->viewport(),&forwarded);event->accept();}else event->ignore();return true;
  }
  return QWidget::eventFilter(object,event);
}
Panel::Panel(QWidget *parent):QWidget(parent){
  setObjectName("CloudParameters");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
  setStyleSheet(QStringLiteral("QLabel[cloudSelected=\"true\"] { color: palette(highlight); }"));
  auto *note=new QLabel(QStringLiteral("体积云 · 时间仅生成指定静态帧，不自动播放。点击参数行后可用滚轮调节。"));note->setWordWrap(true);layout->addWidget(note);
  auto *form=new QFormLayout;layout->addLayout(form);
  auto scalar=[&](const char *id,const QString &label,double min,double max,double step,int decimals=3){auto *spin=editor::parameter_widgets::number(id);spin->setRange(min,max);spin->setDecimals(decimals);spin->setSingleStep(step);spin->setKeyboardTracking(false);spin->setFocusPolicy(Qt::StrongFocus);spin->setProperty("historyInput",true);spin->setProperty("cloudWheelKey",id);fields_[id]=spin;auto *caption=new QLabel(label);caption->setProperty("cloudWheelKey",id);form->addRow(caption,spin);connect(spin,&QDoubleSpinBox::valueChanged,this,[this,id]{submit(id);});};
  scalar("x",QStringLiteral("位置 X（米）"),-1e6,1e6,10);scalar("y",QStringLiteral("位置 Y（米）"),-1e6,1e6,10);
  scalar("height",QStringLiteral("云底高度（米）"),-1e6,1e6,10);scalar("thickness",QStringLiteral("云层厚度（米）"),1,1e5,10);
  scalar("width",QStringLiteral("宽度（米）"),1,1e6,100);scalar("length",QStringLiteral("长度（米）"),1,1e6,100);
  scalar("density",QStringLiteral("体积密度（每米）"),0,10,.005,4);scalar("coverage",QStringLiteral("云量"),0,1,.05);
  scalar("scale",QStringLiteral("云团大小（米）"),1,1e5,10);scalar("detail",QStringLiteral("细节层数"),0,8,.5,1);
  scalar("warp",QStringLiteral("云团扭曲"),0,2,.1);scalar("erosion",QStringLiteral("细节侵蚀"),0,1,.1);
  distribution_=new QComboBox;distribution_->setObjectName("CloudDistribution");distribution_->addItems({QStringLiteral("方形"),QStringLiteral("圆形")});form->addRow(QStringLiteral("整体分布"),distribution_);connect(distribution_,&QComboBox::currentIndexChanged,this,[this]{submit("distribution");});
  scalar("edge_fade",QStringLiteral("边缘衰减比例"),.001,1,.1);
  distribution_noise_=editor::parameter_widgets::checkbox(QStringLiteral("整体分布噪声"),"CloudDistributionNoise");form->addRow(distribution_noise_);connect(distribution_noise_,&QCheckBox::toggled,this,[this]{submit("distribution_noise");});
  scalar("distribution_threshold",QStringLiteral("有云阈值"),0,1,.1,4);scalar("distribution_scale",QStringLiteral("分布尺度（米）"),1,1e6,10);scalar("distribution_detail",QStringLiteral("分布分形层数"),0,8,.5);
  distribution_noise_->setToolTip(QStringLiteral("在方形或圆形范围内叠加分形噪声，控制云层的整体疏密分布；关闭时保留噪声参数。"));
  fields_["distribution_threshold"]->setToolTip(QStringLiteral("整体分布噪声值大于此阈值时才有云（0～1）。数值越高，有云区域越少；云团边界仍平滑衰减。"));
  fields_["detail"]->setToolTip(QStringLiteral("控制静态云体的分形细节，最多 8 层。采样预算单独控制体积积分速度。"));
  scalar("wind_speed",QStringLiteral("风速（米/秒）"),0,10000,1);scalar("direction",QStringLiteral("风向（度）"),-36000,36000,5);
  scalar("time",QStringLiteral("时间（秒）"),-1e6,1e6,.1);scalar("seed",QStringLiteral("随机种子"),0,4294967295.,1,0);
  scalar("steps",QStringLiteral("体积采样预算"),16,192,16,0);fields_["steps"]->setToolTip(QStringLiteral("沿云层最长轴的步进数量。默认 64；降低可加快视口，提高可改善薄云和小型云洞。较大的云层请相应增大云团与碰撞范围。"));
  collisions_=new QCheckBox(QStringLiteral("体积碰撞与穿透尾迹"));collisions_->setObjectName("CloudCollisions");layout->addWidget(collisions_);connect(collisions_,&QCheckBox::toggled,this,[this]{submit();});
  auto *collision_form=new QFormLayout;layout->addLayout(collision_form);form=collision_form;
  scalar("padding",QStringLiteral("额外驱散范围（米）"),0,10000,1);scalar("softness",QStringLiteral("云洞边缘柔化（米）"),0,10000,1);
  fields_["padding"]->setToolTip(QStringLiteral("从对象实际网格的低面数凸包向外增加的距离。0 不外扩；不再使用外接椭球。"));
  fields_["softness"]->setToolTip(QStringLiteral("在凸包及额外驱散范围内部柔化，不向外扩大云洞。0 使用清晰边界。"));
  scalar("velocity_x",QStringLiteral("穿透速度 X（米/秒）"),-10000,10000,5);scalar("velocity_y",QStringLiteral("穿透速度 Y（米/秒）"),-10000,10000,5);scalar("velocity_z",QStringLiteral("穿透速度 Z（米/秒）"),-10000,10000,5);
  scalar("trail",QStringLiteral("尾迹保留时长（秒）"),0,1000,.5);scalar("recovery",QStringLiteral("云层恢复时间（秒）"),.01,1000,.5);
  auto *help=new QLabel(QStringLiteral("勾选陨石等大型对象（最多 16 个，可选择组）。碰撞使用实际网格的低面数凸包，额外范围为 0 时不外扩，柔化仅向内过渡。凸包会填平手臂间等凹陷。云洞跟随对象；速度只控制后方尾迹，不自动移动物体。时间从 0 形成尾迹，长度受保留时长限制，越旧的尾迹恢复越多。"));help->setWordWrap(true);layout->addWidget(help);
  sort_=new QPushButton(QStringLiteral("按AABB重新排序"));sort_->setObjectName("CloudSortSources");sort_->setToolTip(QStringLiteral("按当前对象 AABB 体积从大到小排列，保留参与碰撞的勾选。"));layout->addWidget(sort_);
  connect(sort_,&QPushButton::clicked,this,[this]{if(refresh_candidates)refresh_candidates();if(sort_->isEnabled())sources_->sortItems(0,Qt::DescendingOrder);});
  sources_=new QTreeWidget;sources_->setObjectName("CloudSources");sources_->setHeaderLabels({QStringLiteral("参与碰撞的对象")});sources_->setRootIsDecorated(false);sources_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);sources_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);sources_->setAutoScroll(false);sources_->header()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(sources_);connect(sources_,&QTreeWidget::itemChanged,this,[this]{submit();});
  message_=new QLabel;message_->setWordWrap(true);message_->setObjectName("CloudStatus");layout->addWidget(message_);qApp->installEventFilter(this);editor::parameter_widgets::decorate(this,"cloud/");update_distribution_fields();hide();
}
void Panel::update_distribution_fields(){
  const bool enabled=distribution_noise_->isChecked();
  for(const auto *id:{"distribution_threshold","distribution_scale","distribution_detail"}){
    // decorate 创建的整行包含数值框及范围、精度按钮，一起禁用。
    fields_.at(id)->parentWidget()->setEnabled(enabled);
    for(auto *label:findChildren<QLabel *>())if(label->property("cloudWheelKey").toString()==QLatin1String(id))label->setEnabled(enabled);
  }
}
void Panel::submit(const std::string &field){
  if(binding_)return;auto previous=config_;
  // Do not round untouched authored coordinates to the spin box's display precision.
#define FIELD(n) if(field==#n)config_.n=decltype(config_.n)(fields_.at(#n)->value())
  FIELD(x);FIELD(y);FIELD(height);FIELD(thickness);FIELD(width);FIELD(length);FIELD(density);FIELD(coverage);FIELD(scale);FIELD(detail);FIELD(edge_fade);FIELD(distribution_threshold);FIELD(distribution_scale);FIELD(distribution_detail);FIELD(warp);FIELD(erosion);FIELD(wind_speed);FIELD(direction);FIELD(time);FIELD(padding);FIELD(softness);FIELD(velocity_x);FIELD(velocity_y);FIELD(velocity_z);FIELD(trail);FIELD(recovery);FIELD(steps);FIELD(seed);
#undef FIELD
  if(field=="distribution")config_.distribution=distribution_->currentIndex();if(field=="distribution_noise")config_.distribution_noise=distribution_noise_->isChecked();update_distribution_fields();config_.collisions=collisions_->isChecked();std::set<std::string> checked;for(int row=0;row<sources_->topLevelItemCount();++row){auto *item=sources_->topLevelItem(row);if(item->checkState(0)==Qt::Checked)checked.insert(item->data(0,Qt::UserRole).toString().toStdString());}
  // A view-only sort must not change the authored collision order on the next edit.
  std::erase_if(config_.sources,[&](const auto &id){return !checked.contains(id);});for(const auto &id:config_.sources)checked.erase(id);for(const auto &id:checked)config_.sources.push_back(id);
  try{validate(config_);}catch(const std::exception &e){config_=previous;binding_=true;for(int row=0;row<sources_->topLevelItemCount();++row){auto *i=sources_->topLevelItem(row);const auto id=i->data(0,Qt::UserRole).toString().toStdString();i->setCheckState(0,std::find(config_.sources.begin(),config_.sources.end(),id)!=config_.sources.end()?Qt::Checked:Qt::Unchecked);}binding_=false;message_->setText(QString::fromUtf8(e.what()));return;}
  message_->setText(QStringLiteral("静态帧已更新 · 碰撞跟随参与对象的可见性、形变和位置。"));if(changed&&config_!=previous)changed(config_);
}
void Panel::bind(const Cloud *v,const std::vector<Candidate> &objects){
  const bool switched=!v||v->id!=bound_id_;const auto previous=config_;
  std::map<std::string,int> order;if(v&&v->id==bound_id_&&have_bounds_)for(int row=0;row<sources_->topLevelItemCount();++row)order.emplace(sources_->topLevelItem(row)->data(0,Qt::UserRole).toString().toStdString(),row);
  const bool ready=std::any_of(objects.begin(),objects.end(),[](const auto &o){return o.volume>=0;});
  have_bounds_=(v&&v->id==bound_id_&&have_bounds_)||ready;sort_->setEnabled(ready);
  if(!v||v->id!=bound_id_)select({});bound_id_=v?v->id:std::string{};setVisible(v!=nullptr);if(!v)return;binding_=true;editor::parameter_widgets::context(this)->bind(v->id+"/volume");config_=v->config;
  // 候选对象／渲染结果刷新时保留键盘草稿、光标及选区；外部值真正
  // 改变（包括撤销）或切换对象时才写回编辑器。
#define FIELD(n) if(switched||previous.n!=config_.n) fields_.at(#n)->setValue(config_.n)
  FIELD(x);FIELD(y);FIELD(height);FIELD(thickness);FIELD(width);FIELD(length);FIELD(density);FIELD(coverage);FIELD(scale);FIELD(detail);FIELD(edge_fade);FIELD(distribution_threshold);FIELD(distribution_scale);FIELD(distribution_detail);FIELD(warp);FIELD(erosion);FIELD(wind_speed);FIELD(direction);FIELD(time);FIELD(padding);FIELD(softness);FIELD(velocity_x);FIELD(velocity_y);FIELD(velocity_z);FIELD(trail);FIELD(recovery);FIELD(steps);FIELD(seed);
#undef FIELD
  distribution_->setCurrentIndex(config_.distribution);distribution_noise_->setChecked(config_.distribution_noise);update_distribution_fields();collisions_->setChecked(config_.collisions);auto available=objects;for(const auto &id:config_.sources)if(std::none_of(available.begin(),available.end(),[&](const auto &o){return o.id==id;}))available.push_back({id,"[已移除] "+id});
  if(!order.empty())std::stable_sort(available.begin(),available.end(),[&](const auto &a,const auto &b){const auto rank=[&](const auto &id){auto i=order.find(id);return i==order.end()?int(order.size()):i->second;};return rank(a.id)<rank(b.id);});
  sources_->clear();for(const auto &[id,label,volume]:available){auto *item=new SourceItem(sources_,{QString::fromStdString(label)});item->setData(0,Qt::UserRole,QString::fromStdString(id));item->setData(0,Qt::UserRole+1,volume);item->setToolTip(0,QString::fromStdString(id));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,std::find(config_.sources.begin(),config_.sources.end(),id)!=config_.sources.end()?Qt::Checked:Qt::Unchecked);}
  if(order.empty())sources_->sortItems(0,Qt::DescendingOrder);
  sources_->doItemsLayout();int height=sources_->header()->height()+2*sources_->frameWidth();for(int i=0;i<sources_->topLevelItemCount();++i)height+=sources_->sizeHintForRow(i);sources_->setFixedHeight(std::max(40,height));
  message_->setText(QStringLiteral("已选 %1 / 16 个碰撞对象 · 不需要逐帧烘焙。若小型云洞不明显，可提高采样预算或驱散范围。").arg(config_.sources.size()));select(selected_);binding_=false;
}
}
