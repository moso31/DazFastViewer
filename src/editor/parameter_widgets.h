#pragma once
#include "editor/numeric_slider.h"
#include "editor/numeric_spinbox.h"
#include "editor/hdr_color_dialog.h"
#include "runtime/parameter_settings.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QCursor>
#include <QTreeWidget>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace dfv::editor::parameter_widgets {
using Settings=runtime::ParameterSettings;
class SettingsButtons;
// 一个面板一个绑定上下文。数据保存在场景中；虚拟化控件的销毁不丢失编辑规则。
class Context final:public QObject {
  QWidget *root_;
  QString selected_;
  QPointer<QLabel> selected_label_;
  bool wheel_=false;
  void select(const QString &key,QLabel *label=nullptr) {
    if(selected_label_)selected_label_->setStyleSheet({});selected_=key;selected_label_=label;
    if(label&&!key.isEmpty())label->setStyleSheet("QLabel { color: palette(highlight); }");
  }
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    if(!wheel_)return false;
    const auto type=event->type();
    if(type!=QEvent::MouseButtonPress&&type!=QEvent::MouseButtonDblClick&&type!=QEvent::Wheel&&type!=QEvent::Hide)return false;
    auto *w=qobject_cast<QWidget *>(object);if(!w)return false;
    if(w->window()->property("parameterSettingsPopup").toBool())return false;
    const bool inside=w==root_||root_->isAncestorOf(w);
    QString key;QWidget *row=nullptr;
    if(inside)for(auto *p=w;p&&p!=root_;p=p->parentWidget())if(p->property("numericRow").isValid()){key=p->property("numericRow").toString();row=p;break;}
    if(type==QEvent::Hide&&w==root_)select({});
    if(inside&&(type==QEvent::MouseButtonPress||type==QEvent::MouseButtonDblClick))if(auto *tree=qobject_cast<QTreeWidget *>(w->parentWidget());tree&&w==tree->viewport())if(auto *item=tree->itemAt(static_cast<QMouseEvent *>(event)->position().toPoint())){const auto value=item->data(0,Qt::UserRole+200);if(value.isValid())key=value.toString();}
    if((type==QEvent::MouseButtonPress||type==QEvent::MouseButtonDblClick)&&static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton){
      if(!key.isEmpty())select(key,row?qobject_cast<QLabel *>(row->property("numericLabel").value<QObject *>()):nullptr);
      else if(!inside||!w->rect().contains(w->mapFromGlobal(static_cast<QMouseEvent *>(event)->globalPosition().toPoint()))||!row) {
        // 忽略向父级传播的同一次点击，空白和面板外部点击清除选择。
        auto *under=root_->childAt(root_->mapFromGlobal(static_cast<QMouseEvent *>(event)->globalPosition().toPoint()));
        bool same=false;for(auto *p=under;p&&p!=root_;p=p->parentWidget())if(p->property("numericRow").toString()==selected_&&!selected_.isEmpty())same=true;
        if(!same)select({});
      }
    }
    if(type==QEvent::Wheel&&inside){
      auto *control=w;while(control&&control!=root_&&!qobject_cast<QAbstractSpinBox *>(control)&&!qobject_cast<QComboBox *>(control))control=control->parentWidget();
      if(!key.isEmpty()&&key==selected_&&control&&control!=root_&&control->isEnabled())return false;
      QScrollArea *outer=nullptr;for(auto *p=root_->parentWidget();p;p=p->parentWidget())if(auto *s=qobject_cast<QScrollArea *>(p))outer=s;
      if(outer){auto *e=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(outer->viewport()->mapFromGlobal(e->globalPosition()),e->globalPosition(),e->pixelDelta(),e->angleDelta(),e->buttons(),e->modifiers(),e->phase(),e->inverted(),e->source());QApplication::sendEvent(outer->viewport(),&forwarded);event->accept();}else event->ignore();return true;
    }
    return false;
  }
public:
  std::string owner;
  runtime::ParameterSettingsState local;
  std::function<Settings(const std::string &,const Settings &)> read;
  std::function<void(const std::string &,const Settings &,const std::function<void()> &)> edit;
  explicit Context(QWidget *root):QObject(root),root_(root){setObjectName("ParameterWidgetContext");}
  void wheel_selection(){wheel_=true;qApp->installEventFilter(this);}
  void bind(const std::string &id);
  Settings get(const std::string &key,const Settings &defaults) const {
    if(read)return read(key,defaults);auto o=local.find(owner);if(o!=local.end())if(auto p=o->second.find(key);p!=o->second.end())return p->second;return defaults;
  }
  void commit(const std::string &key,const Settings &s,const std::function<void()> &apply){
    runtime::validate(s);if(edit)edit(key,s,apply);else {local[owner][key]=s;apply();}
  }
};
inline Context *context(QWidget *root){
  for(auto *c:root->children())if(auto *v=dynamic_cast<Context *>(c))return v;
  return new Context(root);
}
inline NumericSpinBox *number(const QString &id,bool float_backed=false){
  auto *s=new NumericSpinBox(float_backed);s->setObjectName(id);s->setDecimals(9);s->setRange(-std::numeric_limits<float>::max(),std::numeric_limits<float>::max());s->setSingleStep(.1);s->setKeyboardTracking(false);s->setFocusPolicy(Qt::StrongFocus);return s;
}
inline QCheckBox *checkbox(const QString &label,const QString &id={}){auto *w=new QCheckBox(label);w->setObjectName(id);return w;}
inline QPushButton *color(const QString &id,const hdr_color::Color &v){auto *w=new QPushButton;w->setObjectName(id);hdr_color::swatch(w,v,false,true);return w;}

class SettingsButtons final:public QWidget {
  Context *context_;
  std::string key_;
  Settings defaults_,settings_;
  QDoubleSpinBox *spin_;
  QPointer<NumericSlider> slider_;
  double hard_min_,hard_max_,scale_min_,scale_max_;
  QToolButton *range_,*precision_;
  QPointer<QWidget> popup_;
  QPointer<QToolButton> anchor_;
  bool popup_entered_=false,ready_=false;
  void cancel(){auto popup=popup_;popup_.clear();anchor_.clear();if(popup){popup->setObjectName({});popup->hide();popup->deleteLater();}}
  void leave_later(){QTimer::singleShot(100,this,[this]{if(!popup_)return;const auto p=QCursor::pos();if(popup_->frameGeometry().contains(p))return;if(!popup_entered_&&anchor_&&QRect(anchor_->mapToGlobal(QPoint()),anchor_->size()).adjusted(-2,-2,2,2).contains(p))return;cancel();});}
  void show_editor(QToolButton *button,bool activate=true){
    if(popup_&&anchor_==button)return;cancel();anchor_=button;popup_entered_=false;
    auto *popup=new QFrame(this,Qt::Tool|Qt::FramelessWindowHint);popup->setFrameShape(QFrame::StyledPanel);popup_=popup;popup->setObjectName("ParameterSettingsPopup");popup->setProperty("parameterSettingsPopup",true);popup->setAttribute(Qt::WA_DeleteOnClose);popup->setAutoFillBackground(true);
    // 悬停只展示草稿，不能激活工具窗口并抢走正在输入的参数焦点。
    popup->setAttribute(Qt::WA_ShowWithoutActivating,!activate);
    auto *layout=new QVBoxLayout(popup);layout->setContentsMargins(10,8,10,8);auto *form=new QFormLayout;layout->addLayout(form);
    auto *first=number(button==range_?"rangeMinimum":"precisionStep");auto *second=button==range_?number("rangeMaximum"):nullptr;
    first->setMinimumWidth(145);first->setValue(button==range_?settings_.minimum:settings_.step);if(button==precision_)first->setRange(1e-9,1e12);
    form->addRow(button==range_?QStringLiteral("最小值"):QStringLiteral("调节步长"),first);
    if(second){second->setValue(settings_.maximum);form->addRow(QStringLiteral("最大值"),second);}
    auto *message=new QLabel;message->setWordWrap(true);layout->addWidget(message);
    auto *ok=new QPushButton(QStringLiteral("OK"));ok->setObjectName("ParameterSettingsOK");layout->addWidget(ok);
    connect(ok,&QPushButton::clicked,this,[this,first,second,message]{
      first->interpretText();if(second)second->interpretText();auto next=settings_;if(second){next.minimum=first->value();next.maximum=second->value();}else next.step=first->value();
      try{runtime::validate(next);if(next.limited&&std::max(hard_min_,next.minimum)>std::min(hard_max_,next.maximum))throw std::runtime_error("范围与该参数的有效数值区间没有交集");commit(next);cancel();}catch(const std::exception &e){message->setText(QString::fromUtf8(e.what()));popup_->adjustSize();}
    });
    popup->installEventFilter(this);popup->adjustSize();const auto available=button->screen()->availableGeometry();const auto top=button->mapToGlobal(QPoint(0,0));
    int x=std::clamp(top.x()+button->width()-popup->width(),available.left(),std::max(available.left(),available.right()-popup->width()+1));
    int y=top.y()+button->height()-1;if(y+popup->height()>available.bottom()+1)y=top.y()-popup->height()+1;
    y=std::clamp(y,available.top(),std::max(available.top(),available.bottom()-popup->height()+1));popup->move(x,y);popup->show();
  }
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    if(object==range_||object==precision_){
      if(event->type()==QEvent::Enter){auto *button=static_cast<QToolButton *>(object);QTimer::singleShot(260,this,[this,button]{if(button->underMouse()&&button->isEnabled())show_editor(button,false);});}
      if(event->type()==QEvent::Leave)leave_later();
    }
    if(object==popup_){if(event->type()==QEvent::Enter)popup_entered_=true;if(event->type()==QEvent::Leave)leave_later();if(event->type()==QEvent::WindowDeactivate)cancel();}
    if(event->type()==QEvent::Hide&&object==this)cancel();return false;
  }
  void hideEvent(QHideEvent *e) override {cancel();QWidget::hideEvent(e);}
public:
  std::function<void(const Settings &)> changed;
  SettingsButtons(Context *binding,std::string key,QDoubleSpinBox *spin,Settings defaults={},NumericSlider *slider=nullptr,
                  double scale_min=0,double scale_max=1,QWidget *parent=nullptr):QWidget(parent),context_(binding),key_(std::move(key)),defaults_(defaults),spin_(spin),slider_(slider),hard_min_(spin->minimum()),hard_max_(spin->maximum()),scale_min_(scale_min),scale_max_(scale_max){
    setObjectName("parameterSettings");setProperty("parameterKey",QString::fromStdString(key_));auto *row=new QHBoxLayout(this);row->setContentsMargins(0,0,0,0);row->setSpacing(1);
    range_=new QToolButton;range_->setObjectName("rangeButton");range_->setText(QStringLiteral("限"));range_->setAccessibleName(QStringLiteral("限制范围"));range_->setCheckable(true);range_->setToolTip(QStringLiteral("限制范围：点击启用／关闭，悬停设置最小值和最大值"));
    precision_=new QToolButton;precision_->setObjectName("precisionButton");precision_->setText(QStringLiteral("精"));precision_->setAccessibleName(QStringLiteral("调节精度"));precision_->setToolTip(QStringLiteral("调节精度：悬停设置步长"));
    for(auto *b:{range_,precision_}){b->setAutoRaise(true);b->setFixedSize(23,22);b->installEventFilter(this);row->addWidget(b);}
    connect(range_,&QToolButton::clicked,this,[this](bool on){auto next=settings_;next.limited=on;try{if(on&&std::max(hard_min_,next.minimum)>std::min(hard_max_,next.maximum)){range_->setChecked(settings_.limited);show_editor(range_);return;}commit(next);}catch(const std::exception &){range_->setChecked(settings_.limited);}});
    connect(precision_,&QToolButton::clicked,this,[this]{show_editor(precision_);});
    refresh();
  }
  ~SettingsButtons() override {cancel();}
  const Settings &settings() const{return settings_;}
  void defaults(const Settings &value){defaults_=value;refresh();}
  void refresh(bool discard=false){const auto v=context_->get(key_,defaults_);if(discard)cancel();if(!ready_||v!=settings_){cancel();apply(v,false);ready_=true;}}
  void sync_slider(){if(slider_)slider_->sync(spin_->value(),settings_.limited?settings_.minimum:scale_min_,settings_.limited?settings_.maximum:scale_max_,settings_.step,settings_.limited);}
  void apply(const Settings &v,bool edit_value){
    settings_=v;const double previous=spin_->value();double lo=hard_min_,hi=hard_max_;if(v.limited){lo=std::max(lo,v.minimum);hi=std::min(hi,v.maximum);}
    if(lo>hi){lo=hard_min_;hi=hard_max_;}
    {QSignalBlocker block(spin_);if(spin_->decimals()!=0)spin_->setDecimals(std::max(spin_->decimals(),9));spin_->setRange(lo,hi);spin_->setSingleStep(v.step);spin_->setValue(previous);}
    range_->setChecked(v.limited);precision_->setToolTip(QStringLiteral("调节步长：%1；悬停修改").arg(v.step,0,'g',9));sync_slider();if(edit_value&&changed)changed(v);
    if(edit_value&&spin_->value()!=previous)QMetaObject::invokeMethod(spin_,"valueChanged",Qt::DirectConnection,Q_ARG(double,spin_->value()));
  }
  void commit(const Settings &v){if(v==settings_)return;context_->commit(key_,v,[this,v]{apply(v,true);});}
};
inline void Context::bind(const std::string &id){const bool switched=id!=owner;owner=id;if(switched)select({});for(auto *w:root_->findChildren<QWidget *>())if(auto *b=dynamic_cast<SettingsButtons *>(w))b->refresh(switched);}

// 单行数值框保持原生 Qt 外观，只在右侧增加两个按钮。保留原有数值对象和信号。
inline QWidget *decorate_number(QWidget *root,QDoubleSpinBox *spin,const std::string &key,QLabel *label=nullptr){
  auto *row=new QWidget;row->setObjectName(spin->objectName()+"Row");row->setProperty("numericRow",QString::fromStdString(key));row->setProperty("numericLabel",QVariant::fromValue<QObject *>(label));
  auto *layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);layout->addWidget(spin,1);
  const auto font=QApplication::font("QWidget");spin->setFont(font);spin->setFocusPolicy(Qt::StrongFocus);spin->setProperty("historyInput",true);
  if(label){label->setFont(font);label->setProperty("numericRow",QString::fromStdString(key));label->setProperty("numericLabel",QVariant::fromValue<QObject *>(label));}
  auto *buttons=new SettingsButtons(context(root),key,spin,{},nullptr,0,1,row);layout->addWidget(buttons);return row;
}
inline void decorate(QWidget *root,const std::string &prefix){
  auto *binding=context(root);qApp->removeEventFilter(root);binding->wheel_selection();
  const auto spins=root->findChildren<QDoubleSpinBox *>();
  for(auto *label:root->findChildren<QLabel *>())label->setFont(QApplication::font("QWidget"));
  for(auto *spin:spins){if(spin->property("numericDecorated").toBool()||spin->objectName().isEmpty()||spin->decimals()==0)continue;
    auto *parent=spin->parentWidget();auto *layout=parent?parent->layout():nullptr;if(!layout)continue;
    QLabel *label=nullptr;if(auto *form=qobject_cast<QFormLayout *>(layout))label=qobject_cast<QLabel *>(form->labelForField(spin));
    if(!label){std::function<QLabel *(QLayout *)> preceding=[&](QLayout *l)->QLabel *{QLabel *last=nullptr;for(int n=0;n<l->count();++n){auto *item=l->itemAt(n);if(item->widget()==spin)return last;if(auto *caption=qobject_cast<QLabel *>(item->widget()))last=caption;if(item->layout())if(auto *found=preceding(item->layout()))return found;}return nullptr;};label=preceding(layout);}
    auto *placeholder=new QWidget;auto *item=layout->replaceWidget(spin,placeholder);if(!item){delete placeholder;continue;}delete item;
    spin->setProperty("numericDecorated",true);auto *row=decorate_number(root,spin,prefix+spin->objectName().toStdString(),label);delete layout->replaceWidget(placeholder,row);delete placeholder;
  }
  for(auto *spin:spins)if(!spin->property("numericDecorated").toBool()){spin->setFont(QApplication::font("QWidget"));spin->setProperty("numericRow",QString::fromStdString(prefix)+spin->objectName());}
  for(auto *spin:root->findChildren<QSpinBox *>())spin->setProperty("numericRow",QString::fromStdString(prefix)+spin->objectName());
  for(auto *combo:root->findChildren<QComboBox *>()){combo->setProperty("numericRow",QString::fromStdString(prefix)+combo->objectName());}
}
}
