#pragma once
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <functional>
#include <vector>

namespace dfv::editor {
struct GeograftItem {size_t target;QString label;bool enabled;};
class GeograftPanel final:public QWidget {
  QWidget *body_;
  QVBoxLayout *rows_;
  std::vector<size_t> targets_;
  std::vector<QCheckBox *> switches_;
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    if(event->type()==QEvent::Wheel) {
      for(auto *parent=parentWidget();parent;parent=parent->parentWidget())if(auto *scroll=qobject_cast<QScrollArea *>(parent)) {
        auto *wheel=static_cast<QWheelEvent *>(event);QWheelEvent forwarded(scroll->viewport()->mapFromGlobal(wheel->globalPosition()),wheel->globalPosition(),wheel->pixelDelta(),wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),wheel->phase(),wheel->inverted());QApplication::sendEvent(scroll->viewport(),&forwarded);return true;
      }
    }
    return QWidget::eventFilter(object,event);
  }
public:
  std::function<void(size_t,bool)> changed;
  explicit GeograftPanel(QWidget *parent=nullptr):QWidget(parent) {
    setObjectName("GeograftPanel");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *header=new QToolButton;header->setObjectName("GeograftCollapse");header->setText(QStringLiteral("Geograft"));header->setCheckable(true);header->setChecked(true);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header->setArrowType(Qt::DownArrow);layout->addWidget(header);
    body_=new QWidget;rows_=new QVBoxLayout(body_);rows_->setContentsMargins(4,0,4,4);layout->addWidget(body_);
    installEventFilter(this);body_->installEventFilter(this);header->installEventFilter(this);
    connect(header,&QToolButton::toggled,this,[this,header](bool open){body_->setVisible(open);header->setArrowType(open?Qt::DownArrow:Qt::RightArrow);});
    hide();
  }
  void bind(const std::vector<GeograftItem> &items) {
    std::vector<size_t> targets;for(const auto &item:items)targets.push_back(item.target);
    if(targets!=targets_) {
      while(auto *item=rows_->takeAt(0)){delete item->widget();delete item;}switches_.clear();targets_=std::move(targets);
      for(const auto &item:items) {
        auto *box=new QCheckBox;box->setObjectName("GeograftEnabled/"+QString::number(qulonglong(item.target)));box->setToolTip(QStringLiteral("勾选启用；取消勾选停用并恢复宿主对应部位。"));rows_->addWidget(box);switches_.push_back(box);
        box->installEventFilter(this);
        connect(box,&QCheckBox::toggled,this,[this,target=item.target](bool enabled){if(changed)changed(target,enabled);});
      }
      if(!items.empty()){auto *note=new QLabel(QStringLiteral("停用后恢复宿主对应部位的渲染。"));note->setWordWrap(true);note->installEventFilter(this);rows_->addWidget(note);}
    }
    for(size_t i=0;i<items.size();++i){const QSignalBlocker block(switches_[i]);switches_[i]->setText(items[i].label);switches_[i]->setChecked(items[i].enabled);}
    setVisible(!items.empty());
  }
};
}
