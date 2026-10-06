#pragma once
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <functional>
#include <map>

namespace dfv::editor {
class GroundPanel final:public QWidget {
  QDoubleSpinBox *ratio_,*offset_;
  QCheckBox *body_only_;
  QLabel *note_;
  std::map<QObject *,QWidget *> controls_;
  QWidget *selected_=nullptr;
  bool binding_=false;
  void publish(){if(!binding_&&changed)changed(ratio_->value()/100,offset_->value(),body_only_->isChecked());}
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    auto found=controls_.find(object);
    if(event->type()==QEvent::MouseButtonPress&&static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton){
      if(found!=controls_.end())selected_=found->second;
      else if(auto *widget=qobject_cast<QWidget *>(object);!widget||!selected_||!widget->isAncestorOf(selected_))selected_=nullptr;
    }
    if(event->type()==QEvent::Hide&&object==this)selected_=nullptr;
    if(event->type()==QEvent::Wheel&&found!=controls_.end()&&found->second!=selected_){
      for(auto *p=parentWidget();p;p=p->parentWidget())if(auto *scroll=qobject_cast<QScrollArea *>(p)){
        auto *w=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(scroll->viewport()->mapFromGlobal(w->globalPosition()),w->globalPosition(),w->pixelDelta(),w->angleDelta(),w->buttons(),w->modifiers(),w->phase(),w->inverted());QApplication::sendEvent(scroll->viewport(),&forwarded);return true;
      }
      event->ignore();return true;
    }
    return QWidget::eventFilter(object,event);
  }
public:
  std::function<void(double,double,bool)> changed;
  std::function<void()> align;
  explicit GroundPanel(QWidget *parent=nullptr):QWidget(parent) {
    setObjectName("GroundPanel");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *header=new QToolButton;header->setObjectName("GroundCollapse");header->setText(QStringLiteral("地面对齐"));header->setCheckable(true);header->setChecked(true);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header->setArrowType(Qt::DownArrow);layout->addWidget(header);
    auto *body=new QWidget;auto *form=new QFormLayout(body);form->setContentsMargins(4,0,4,4);layout->addWidget(body);
    connect(header,&QToolButton::toggled,this,[this,header,body](bool open){body->setVisible(open);header->setArrowType(open?Qt::DownArrow:Qt::RightArrow);selected_=nullptr;});
    auto number=[&](const char *id,const QString &name,double range,double step){
      auto *spin=new QDoubleSpinBox;spin->setObjectName(id);spin->setRange(-range,range);spin->setDecimals(5);spin->setSingleStep(step);spin->setKeyboardTracking(false);spin->setProperty("historyInput",true);
      auto *label=new QLabel(name);form->addRow(label,spin);auto children=spin->findChildren<QWidget *>();children.push_back(spin);children.push_back(label);for(auto *child:children)controls_[child]=spin;
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{publish();});return spin;
    };
    ratio_=number("GroundAlignmentRatio",QStringLiteral("向下对齐百分比（%）"),100000,1);
    offset_=number("GroundAlignmentOffset",QStringLiteral("固定世界偏移（cm）"),1e9,.1);
    body_only_=new QCheckBox(QStringLiteral("仅考虑角色本体"));body_only_->setObjectName("GroundBodyOnly");form->addRow(body_only_);connect(body_only_,&QCheckBox::toggled,this,[this]{publish();});
    note_=new QLabel;note_->setWordWrap(true);form->addRow(note_);
    auto *button=new QPushButton(QStringLiteral("向下对齐（Ctrl+D）"));button->setObjectName("GroundAlign");form->addRow(button);connect(button,&QPushButton::clicked,this,[this]{if(align)align();});
    qApp->installEventFilter(this);setEnabled(false);
  }
  void commit(){ratio_->interpretText();offset_->interpretText();}
  void bind(bool enabled,double ratio,double offset,bool body_only,bool collective=false,bool water=false){
    binding_=true;selected_=nullptr;setEnabled(enabled);const QSignalBlocker a(ratio_),b(offset_),c(body_only_);ratio_->setValue(ratio*100);offset_->setValue(offset);body_only_->setChecked(body_only);binding_=false;
    ratio_->setEnabled(!collective);offset_->setEnabled(!collective);body_only_->setEnabled(!collective);
    if(auto *form=qobject_cast<QFormLayout *>(ratio_->parentWidget()->layout())){form->setRowVisible(ratio_,!water);form->setRowVisible(body_only_,!water);}
    note_->setText(collective?QStringLiteral("组和多选对象按整体世界包围盒统一落地，忽略各对象的地面对齐设置，保持相对位置。") :QStringLiteral("底部高度 = 世界包围盒高度 × 百分比 + 固定偏移。正值离地，负值下沉；默认包含穿戴物。"));
    if(water)note_->setText(QStringLiteral("按未起伏的水面基准对齐到世界地面，保留固定偏移；对齐后需要重算海岸线。"));
  }
};
}
