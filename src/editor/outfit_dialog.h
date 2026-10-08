#pragma once
#include "runtime/visibility.h"
#include "editor/document.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QMouseEvent>

namespace dfv::editor {
class OutfitDialog final:public QDialog {
  const Document &document_;
  const Snapshot &snapshot_;
  QComboBox *source_;
  QLabel *source_label_;
  QScrollArea *scroll_;
  QVBoxLayout *clothes_;
  QDialogButtonBox *buttons_;
  std::vector<std::pair<size_t,QCheckBox *>> clothing_,hosts_;
  bool source_selected_=false;
  bool forwarding_wheel_=false;
  QString label(size_t target) const {
    const auto &name=document_.catalog.targets.at(target).label;const auto text=QString::fromUtf8(name);
    return std::count_if(document_.catalog.targets.begin(),document_.catalog.targets.end(),[&](const auto &t){return t.label==name;})>1?text+QStringLiteral("（对象 %1）").arg(qulonglong(target+1)):text;
  }
  void validate() {
    const auto checked=[](const auto &items){return std::any_of(items.begin(),items.end(),[](const auto &item){return item.second->isEnabled()&&item.second->isChecked();});};
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(checked(clothing_)&&checked(hosts_));
  }
  void bind() {
    while(auto *item=clothes_->takeAt(0)){delete item->widget();delete item;}clothing_.clear();
    const auto visible=runtime::effective_visibility(document_.catalog.targets,runtime::visibility_children(document_.loaded.scene,document_.catalog.targets),snapshot_.values);
    for(auto t:outfit_roots(document_,source())) {
      auto *box=new QCheckBox(label(t)+(visible[t]?QString{}:QStringLiteral("（已隐藏）")));box->setObjectName("OutfitClothing/"+QString::number(qulonglong(t)));box->setToolTip(QString::fromUtf8(document_.catalog.targets[t].id));box->setChecked(visible[t]);clothes_->addWidget(box);clothing_.push_back({t,box});
      box->installEventFilter(this);
      connect(box,&QCheckBox::toggled,this,[this]{validate();});
    }
    if(clothing_.empty()){auto *note=new QLabel(QStringLiteral("当前角色没有可复制的服装。"));clothes_->addWidget(note);}
    for(const auto &[t,box]:hosts_){box->setEnabled(t!=source());if(t==source())box->setChecked(false);}
    validate();
  }
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    if(event->type()==QEvent::MouseButtonPress) {
      source_selected_=object==source_||object==source_label_;
      source_label_->setText(source_selected_?QStringLiteral("▶ 当前选中角色："):QStringLiteral("当前选中角色："));
      if(object==source_label_){source_->setFocus(Qt::MouseFocusReason);return true;}
    }
    if(event->type()==QEvent::Hide){source_selected_=false;source_label_->setText(QStringLiteral("当前选中角色："));}
    if(event->type()==QEvent::Wheel&&!forwarding_wheel_&&object!=scroll_->viewport()&&(object!=source_||!source_selected_)) {
      auto *wheel=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(scroll_->viewport()->mapFromGlobal(wheel->globalPosition()),wheel->globalPosition(),wheel->pixelDelta(),wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),wheel->phase(),wheel->inverted());forwarding_wheel_=true;QApplication::sendEvent(scroll_->viewport(),&forwarded);forwarding_wheel_=false;return true;
    }
    return QDialog::eventFilter(object,event);
  }
public:
  OutfitDialog(const Document &document,const Snapshot &snapshot,size_t selected,QWidget *parent=nullptr):QDialog(parent),document_(document),snapshot_(snapshot) {
    setObjectName("CopyOutfitDialog");setWindowTitle(QStringLiteral("复制穿搭"));setWindowFlag(Qt::WindowMaximizeButtonHint);resize(620,640);
    auto *layout=new QVBoxLayout(this);scroll_=new QScrollArea;scroll_->setObjectName("OutfitScroll");scroll_->setWidgetResizable(true);scroll_->setFrameShape(QFrame::NoFrame);layout->addWidget(scroll_,1);
    auto *page=new QWidget;auto *body=new QVBoxLayout(page);body->setAlignment(Qt::AlignTop);scroll_->setWidget(page);
    auto *row=new QHBoxLayout;source_label_=new QLabel(QStringLiteral("当前选中角色："));source_label_->setObjectName("OutfitSourceLabel");row->addWidget(source_label_);source_=new QComboBox;source_->setObjectName("OutfitSource");row->addWidget(source_,1);body->addLayout(row);
    for(auto t:outfit_characters(document_)){source_->addItem(label(t),qulonglong(t));source_->setItemData(source_->count()-1,QString::fromUtf8(document_.catalog.targets[t].id),Qt::ToolTipRole);if(t==selected)source_->setCurrentIndex(source_->count()-1);}
    auto *clothes=new QGroupBox(QStringLiteral("复制服装："));clothes_=new QVBoxLayout(clothes);body->addWidget(clothes);
    auto *targets=new QGroupBox(QStringLiteral("复制到："));auto *target_rows=new QVBoxLayout(targets);body->addWidget(targets);
    for(auto t:outfit_characters(document_)) {
      auto *box=new QCheckBox(label(t));box->setObjectName("OutfitHost/"+QString::number(qulonglong(t)));box->setToolTip(QString::fromUtf8(document_.catalog.targets[t].id));target_rows->addWidget(box);hosts_.push_back({t,box});connect(box,&QCheckBox::toggled,this,[this]{validate();});
    }
    auto *note=new QLabel(QStringLiteral("每项代表一整件穿戴，子部件随根节点一起复制。默认勾选当前可见的穿戴，包含发型、睫毛及附件，排除 Geograft。Genesis 8 与 8.1 同性别穿戴可互通；已有的单件自动跳过，保留原有显隐状态。"));note->setWordWrap(true);body->addWidget(note);
    buttons_=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons_->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));buttons_->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));layout->addWidget(buttons_);
    connect(buttons_,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(source_,&QComboBox::currentIndexChanged,this,[this]{bind();});bind();
    for(auto *widget:findChildren<QWidget *>())widget->installEventFilter(this);installEventFilter(this);
  }
  size_t source() const {return size_t(source_->currentData().toULongLong());}
  std::vector<size_t> clothing() const {std::vector<size_t> result;for(const auto &[t,box]:clothing_)if(box->isChecked())result.push_back(t);return result;}
  std::vector<size_t> hosts() const {std::vector<size_t> result;for(const auto &[t,box]:hosts_)if(box->isEnabled()&&box->isChecked())result.push_back(t);return result;}
};
}
