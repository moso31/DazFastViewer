#pragma once
#include "editor/parameter_widgets.h"
#include "editor/flow_layout.h"
#include "runtime/garment_deformation.h"
#include <functional>

namespace dfv::editor {
class GarmentPanel final:public QWidget {
  FlowLayout *flow_;
public:
  std::function<void(const std::string &,bool)> changed;
  explicit GarmentPanel(QWidget *parent=nullptr):QWidget(parent) {
    setObjectName("GarmentPanel");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *header=new QToolButton;header->setObjectName("GarmentCollapse");header->setText(QStringLiteral("形变"));header->setCheckable(true);header->setChecked(true);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header->setArrowType(Qt::DownArrow);layout->addWidget(header);
    auto *body=new QWidget;body->setObjectName("GarmentRegions");flow_=new FlowLayout(body);flow_->setContentsMargins(4,0,4,4);layout->addWidget(body);
    connect(header,&QToolButton::toggled,this,[header,body](bool open){body->setVisible(open);header->setArrowType(open?Qt::DownArrow:Qt::RightArrow);});
    parameter_widgets::decorate(this,"garment/");hide();
  }
  void bind(const std::vector<runtime::DeformationRegion> &regions,const std::set<std::string> &disabled) {
    while(auto *item=flow_->takeAt(0)){delete item->widget();delete item;}
    for(const auto &region:regions) {
      auto *check=parameter_widgets::checkbox(QString::fromStdString(region.label),"GarmentRegion");check->setProperty("region",QString::fromStdString(region.id));check->setChecked(!disabled.contains(region.id));
      check->setToolTip(QStringLiteral("勾选：正常跟随角色形变。取消：保持该部位的形状，整体跟随位置、朝向和均匀尺寸，抑制局部 Morph、骨骼拉伸和弯折。左右同名部位同步切换，相邻拼片自动过渡。保留衣物自身适配及手动 Morph，并进行防穿插修正。"));
      flow_->addWidget(check);connect(check,&QCheckBox::toggled,this,[this,id=region.id](bool enabled){if(changed)changed(id,enabled);});
    }
    setVisible(!regions.empty());
  }
};
}
