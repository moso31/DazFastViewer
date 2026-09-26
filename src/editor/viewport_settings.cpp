#include "editor/viewport_settings.h"
#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QWidgetAction>
#include <memory>

namespace dfv::editor {
static std::unique_ptr<QSettings> settings(const QString &file) {
  return file.isEmpty()?std::make_unique<QSettings>():std::make_unique<QSettings>(file,QSettings::IniFormat);
}
ViewportSettings::ViewportSettings(bool persistent,const QString &file,QObject *parent):QObject(parent),persistent_(persistent),settings_file_(file) {
  if(persistent_) {auto s=settings(file);value_.percent=std::clamp(s->value("viewport/renderPercent",100).toInt(),50,100);
    value_.reconstruction=s->value("viewport/reconstruction","bicubic").toString()=="bilinear"?Reconstruction::bilinear:Reconstruction::bicubic;}
}
void ViewportSettings::set(ViewportQuality value) {
  value.percent=std::clamp(value.percent,50,100);
  if(value.reconstruction!=Reconstruction::bilinear) value.reconstruction=Reconstruction::bicubic;
  const bool modified=value_!=value;value_=value;
  if(percent_) {QSignalBlocker block(percent_);percent_->setValue(value.percent);}
  if(reconstruction_) {QSignalBlocker block(reconstruction_);reconstruction_->setCurrentIndex(value.reconstruction==Reconstruction::bicubic?0:1);}
  if(menu_) menu_->setTitle(QStringLiteral("视口渲染（%1%）").arg(value.percent));
  if(modified&&persistent_) {auto s=settings(settings_file_);s->setValue("viewport/renderPercent",value.percent);s->setValue("viewport/reconstruction",value.reconstruction==Reconstruction::bicubic?"bicubic":"bilinear");}
  if(modified&&changed) changed(value_);
}
void ViewportSettings::add_menu(QMenu *parent) {
  menu_=parent->addMenu(QStringLiteral("视口渲染"));menu_->setObjectName("ViewportRenderMenu");
  auto *panel=new QWidget(menu_);auto *layout=new QFormLayout(panel);
  percent_=new QSpinBox(panel);percent_->setObjectName("ViewportRenderPercent");percent_->setRange(50,100);percent_->setSingleStep(5);percent_->setSuffix("%");
  percent_->setKeyboardTracking(false);
  percent_->setToolTip(QStringLiteral("按视口宽、高的此比例采样，再放大显示。50% 对应约四分之一像素；较细纹理可能丢失。"));
  reconstruction_=new QComboBox(panel);reconstruction_->setObjectName("ViewportReconstruction");
  reconstruction_->addItems({QStringLiteral("双三次（较清晰）"),QStringLiteral("双线性（较柔和）")});
  layout->addRow(QStringLiteral("渲染分辨率倍率"),percent_);layout->addRow(QStringLiteral("升采样"),reconstruction_);
  auto *action=new QWidgetAction(menu_);action->setDefaultWidget(panel);menu_->addAction(action);
  set(value_);
  connect(percent_,&QSpinBox::valueChanged,this,[this](int percent){auto value=value_;value.percent=percent;set(value);});
  connect(reconstruction_,&QComboBox::currentIndexChanged,this,[this](int index){auto value=value_;value.reconstruction=index==0?Reconstruction::bicubic:Reconstruction::bilinear;set(value);});
  connect(menu_->addAction(QStringLiteral("恢复 100%")),&QAction::triggered,this,[this]{auto value=value_;value.percent=100;set(value);});
}
}
