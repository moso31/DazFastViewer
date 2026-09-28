#pragma once
#include "runtime/physics_settings.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include "editor/flow_layout.h"
#include <QApplication>
#include <QSpinBox>
#include <QPushButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <limits>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>
#include <map>
#include <vector>

namespace dfv::editor {
class PhysicsPanel final:public QWidget {
  runtime::PhysicsObjectSettings value_;
  QWidget *body_,*settings_;
  QCheckBox *enabled_,*joined_,*select_all_;
  QSpinBox *rounds_;
  QPushButton *run_,*reset_;
  QComboBox *kind_;
  QWidget *surfaces_;
  FlowLayout *flow_;
  std::vector<std::pair<std::string,QCheckBox *>> surface_checks_;
  std::map<QObject *,QWidget *> wheel_controls_;
  QWidget *wheel_selected_=nullptr;
  int hovered_slot_=-1;
  void hover(int slot){if(slot!=hovered_slot_){hovered_slot_=slot;if(hovered)hovered(slot);}}
  void watch(QWidget *control,QWidget *label=nullptr){auto children=control->findChildren<QWidget *>();children.push_back(control);if(label)children.push_back(label);for(auto *child:children)wheel_controls_[child]=control;}
  struct Number {QDoubleSpinBox *spin;float runtime::PhysicsObjectSettings::*member;double scale;};
  std::vector<Number> numbers_;
  bool binding_=false;
  void sync_surface_selection(){
    size_t checked=0;for(const auto &[id,check]:surface_checks_)checked+=check->isChecked();
    const QSignalBlocker block(select_all_);select_all_->setEnabled(!surface_checks_.empty());
    select_all_->setCheckState(checked==0?Qt::Unchecked:checked==surface_checks_.size()?Qt::Checked:Qt::PartiallyChecked);
  }
  void publish(){if(!binding_&&changed){runtime::validate_physics(value_);run_->setEnabled(value_.enabled);reset_->setEnabled(value_.enabled);changed(value_);}}
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    const auto found=wheel_controls_.find(object);
    if(event->type()==QEvent::MouseButtonPress&&static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton){if(found!=wheel_controls_.end())wheel_selected_=found->second;else if(auto *widget=qobject_cast<QWidget *>(object);!widget||!wheel_selected_||!widget->isAncestorOf(wheel_selected_))wheel_selected_=nullptr;}
    if(event->type()==QEvent::Wheel&&found!=wheel_controls_.end()&&found->second!=wheel_selected_){
      for(auto *p=parentWidget();p;p=p->parentWidget())if(auto *scroll=qobject_cast<QScrollArea *>(p)){auto *w=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(scroll->viewport()->mapFromGlobal(w->globalPosition()),w->globalPosition(),w->pixelDelta(),w->angleDelta(),w->buttons(),w->modifiers(),w->phase(),w->inverted());QCoreApplication::sendEvent(scroll->viewport(),&forwarded);return true;}
      event->ignore();return true;
    }
    if(object->property("physicsSurfaceSlot").isValid()){
      if(event->type()==QEvent::Enter)hover(object->property("physicsSurfaceSlot").toInt());
      if(event->type()==QEvent::Leave||event->type()==QEvent::Hide)hover(-1);
    }
    if(object==this&&event->type()==QEvent::Hide){hover(-1);wheel_selected_=nullptr;}
    return QWidget::eventFilter(object,event);
  }
public:
  std::function<void(bool)> simulate;
  std::function<void(int)> hovered;
  std::function<void(runtime::PhysicsObjectSettings)> changed;
  explicit PhysicsPanel(QWidget *parent=nullptr):QWidget(parent){
    setObjectName("PhysicsPanel");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *header=new QToolButton;header->setObjectName("PhysicsCollapse");header->setText(QStringLiteral("物理"));header->setCheckable(true);header->setChecked(true);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header->setArrowType(Qt::DownArrow);layout->addWidget(header);
    body_=new QWidget;auto *body=new QVBoxLayout(body_);body->setContentsMargins(4,0,4,4);
    layout->addWidget(body_);
    connect(header,&QToolButton::toggled,this,[this,header](bool on){body_->setVisible(on);header->setArrowType(on?Qt::DownArrow:Qt::RightArrow);if(!on)hover(-1);});
    enabled_=new QCheckBox(QStringLiteral("启用快速物理"));enabled_->setObjectName("PhysicsEnabled");body->addWidget(enabled_);
    settings_=new QWidget;auto *form=new QFormLayout(settings_);form->setContentsMargins(0,0,0,0);body->addWidget(settings_);
    auto *controls=new QHBoxLayout;run_=new QPushButton(QStringLiteral("模拟"));run_->setObjectName("PhysicsRun");rounds_=new QSpinBox;rounds_->setObjectName("PhysicsRounds");rounds_->setRange(1,std::numeric_limits<int>::max());rounds_->setSuffix(QStringLiteral(" 轮"));rounds_->setKeyboardTracking(false);rounds_->setProperty("historyInput",true);watch(rounds_);
    reset_=new QPushButton(QStringLiteral("重置"));reset_->setObjectName("PhysicsReset");controls->addWidget(run_);controls->addWidget(rounds_,1);controls->addWidget(reset_);form->addRow(controls);
    kind_=new QComboBox;kind_->setObjectName("PhysicsKind");kind_->addItems({QStringLiteral("自动"),QStringLiteral("刚体／刚性穿戴物"),QStringLiteral("服装布料"),QStringLiteral("贴片发束（含多组件）"),QStringLiteral("Strand Based Hair")});auto *kind_label=new QLabel(QStringLiteral("模拟方式"));form->addRow(kind_label,kind_);watch(kind_,kind_label);
    form->addRow(new QLabel(QStringLiteral("组合／约束")));
    joined_=new QCheckBox(QStringLiteral("勾选的子材质作为整体解算"));joined_->setObjectName("PhysicsJoined");joined_->setToolTip(QStringLiteral("刚体保持整体形状；布料和头发在分离部件之间建立连接约束。关闭后允许部件分别运动。"));form->addRow(joined_);
    auto *surfaces_header=new QHBoxLayout;surfaces_header->addWidget(new QLabel(QStringLiteral("参与的子材质")));surfaces_header->addStretch();select_all_=new QCheckBox(QStringLiteral("全选"));select_all_->setObjectName("PhysicsAllSurfaces");select_all_->setTristate(true);surfaces_header->addWidget(select_all_);form->addRow(surfaces_header);
    surfaces_=new QWidget;surfaces_->setObjectName("PhysicsSurfaces");flow_=new FlowLayout(surfaces_);form->addRow(surfaces_);
    connect(select_all_,&QCheckBox::clicked,this,[this]{
      const bool on=select_all_->checkState()!=Qt::Unchecked;
      // 批量修改只提交一次，避免逐个子材质触发重算。
      for(const auto &[id,check]:surface_checks_){const QSignalBlocker block(check);check->setChecked(on);if(on)value_.excluded_surfaces.erase(id);else value_.excluded_surfaces.insert(id);}
      sync_surface_selection();publish();
    });
    auto add=[&](const char *id,const QString &label,float runtime::PhysicsObjectSettings::*member,double lo,double hi,double step,double scale=1.){
      auto *spin=new QDoubleSpinBox;spin->setObjectName(id);spin->setRange(lo,hi);spin->setDecimals(3);spin->setSingleStep(step);spin->setKeyboardTracking(false);spin->setProperty("historyInput",true);auto *title=new QLabel(label);form->addRow(title,spin);watch(spin,title);numbers_.push_back({spin,member,scale});
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this,member,scale](double v){value_.*member=float(v/scale);publish();});
    };
    add("PhysicsMass",QStringLiteral("质量（kg）"),&runtime::PhysicsObjectSettings::mass,.001,1000,.1);
    add("PhysicsFriction",QStringLiteral("摩擦"),&runtime::PhysicsObjectSettings::friction,0,2,.05);
    add("PhysicsRestitution",QStringLiteral("弹性"),&runtime::PhysicsObjectSettings::restitution,0,1,.05);
    add("PhysicsDamping",QStringLiteral("阻尼"),&runtime::PhysicsObjectSettings::damping,0,20,.25);
    add("PhysicsStiffness",QStringLiteral("形状保持"),&runtime::PhysicsObjectSettings::stiffness,0,1,.05);
    add("PhysicsFixed",QStringLiteral("根部固定范围（%）"),&runtime::PhysicsObjectSettings::fixed_fraction,1,95,5,100);
    add("PhysicsDistance",QStringLiteral("最大活动距离（cm）"),&runtime::PhysicsObjectSettings::max_distance,.1,500,1,100);
    add("PhysicsThickness",QStringLiteral("碰撞厚度（mm）"),&runtime::PhysicsObjectSettings::thickness,.1,10,1,1000);
    auto *note=new QLabel(QStringLiteral("点击模拟后从当前状态运行指定轮数，每轮推进 1/60 秒。修改物理参数或重置会恢复初始状态，等待再次启动。画面刷新率可在项目设置中调整。"));note->setWordWrap(true);form->addRow(note);
    connect(enabled_,&QCheckBox::toggled,this,[this](bool v){value_.enabled=v;publish();});
    connect(rounds_,&QSpinBox::valueChanged,this,[this](int v){value_.rounds=v;publish();});
    connect(run_,&QPushButton::clicked,this,[this]{if(simulate)simulate(false);});connect(reset_,&QPushButton::clicked,this,[this]{if(simulate)simulate(true);});
    connect(joined_,&QCheckBox::toggled,this,[this](bool v){value_.joined=v;publish();});
    connect(kind_,&QComboBox::currentIndexChanged,this,[this](int v){value_.kind=runtime::PhysicsKind(v);publish();});
    qApp->installEventFilter(this);setVisible(false);
  }
  void bind(const runtime::PhysicsObjectSettings &value,const std::vector<std::string> &surfaces){
    binding_=true;hover(-1);wheel_selected_=nullptr;value_=value;const QSignalBlocker a(enabled_),b(rounds_),c(joined_),d(kind_);
    enabled_->setChecked(value.enabled);rounds_->setValue(value.rounds);joined_->setChecked(value.joined);kind_->setCurrentIndex(int(value.kind));run_->setEnabled(value.enabled);reset_->setEnabled(value.enabled);
    surface_checks_.clear();while(auto *item=flow_->takeAt(0)){delete item->widget();delete item;}
    for(size_t slot=0;slot<surfaces.size();++slot){const auto &id=surfaces[slot];auto *check=new QCheckBox(QString::fromStdString(id));check->setObjectName("PhysicsSurface");check->setToolTip(QString::fromStdString(id));check->setProperty("physicsSurfaceSlot",int(slot));check->setChecked(!value.excluded_surfaces.contains(id));flow_->addWidget(check);
      surface_checks_.emplace_back(id,check);connect(check,&QCheckBox::toggled,this,[this,id](bool on){if(on)value_.excluded_surfaces.erase(id);else value_.excluded_surfaces.insert(id);sync_surface_selection();publish();});}
    sync_surface_selection();
    for(const auto &n:numbers_){QSignalBlocker block(n.spin);n.spin->setValue(value.*n.member*n.scale);}binding_=false;
  }
};
}
