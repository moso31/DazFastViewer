#pragma once
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>

namespace dfv::editor {
namespace hdr_color {
using Color=std::array<double,3>;
inline double linear(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
inline double srgb(double v){return v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;}
inline double peak(const Color &c){return std::max({c[0],c[1],c[2]});}
inline QColor preview(const Color &c){
  const double scale=std::max(1.,peak(c));
  return QColor::fromRgbF(std::clamp(srgb(c[0]/scale),0.,1.),std::clamp(srgb(c[1]/scale),0.,1.),std::clamp(srgb(c[2]/scale),0.,1.));
}
}

// The QColorDialog edits only the normalized color. Keep the original linear
// components separately so opening/accepting and exposure edits never round-trip
// through the picker's integer RGB/HSV controls.
class HdrColorDialog final:public QDialog {
  hdr_color::Color base_,color_;
  double exposure_=0,maximum_;
  QColorDialog *picker_;
  QDoubleSpinBox *exposure_input_;
  QSlider *slider_;
  QLabel *factor_,*preview_,*components_;
  QPushButton *brighter_,*dimmer_;

  double maximum_exposure() const {
    const double peak=hdr_color::peak(base_);
    return std::log2(maximum_/(peak>0?peak:1.));
  }
  void sync() {
    const double high=std::max(exposure_,maximum_exposure());
    {QSignalBlocker block(exposure_input_);exposure_input_->setRange(-16,high);exposure_input_->setValue(exposure_);}
    {QSignalBlocker block(slider_);slider_->setRange(-1600,int(std::floor(high*100)));slider_->setValue(qRound(exposure_*100));}
    factor_->setText(QStringLiteral("×%1").arg(std::exp2(exposure_),0,'g',6));
    const auto swatch=hdr_color::preview(color_);
    preview_->setStyleSheet("background-color: "+swatch.name()+"; border: 1px solid #888;");
    components_->setText(QStringLiteral("线性 RGB：%1, %2, %3").arg(color_[0],0,'g',7).arg(color_[1],0,'g',7).arg(color_[2],0,'g',7));
    brighter_->setEnabled(exposure_<maximum_exposure()-1e-9);
    dimmer_->setEnabled(exposure_>-16);
  }
  void set_exposure(double value) {
    exposure_=std::clamp(value,-16.,maximum_exposure());
    const double peak=hdr_color::peak(base_);
    const double scale=std::min(std::exp2(exposure_),maximum_/(peak>0?peak:1.));
    for(size_t i=0;i<3;++i)color_[i]=std::min(maximum_,base_[i]*scale);
    sync();
  }
public:
  HdrColorDialog(const hdr_color::Color &initial,double maximum,QWidget *parent=nullptr):QDialog(parent),base_(initial),color_(initial),maximum_(maximum) {
    setObjectName("materialHdrColorDialog");setWindowTitle(QStringLiteral("选择颜色与 HDR 强度"));
    const double scale=std::max(1.,hdr_color::peak(initial));exposure_=std::log2(scale);
    for(auto &component:base_)component/=scale;
    auto *layout=new QVBoxLayout(this);
    picker_=new QColorDialog(this);picker_->setObjectName("hdrBaseColor");
    picker_->setOptions(QColorDialog::DontUseNativeDialog|QColorDialog::NoButtons);
    picker_->setWindowFlags(Qt::Widget);picker_->setCurrentColor(hdr_color::preview(base_));layout->addWidget(picker_);
    auto *hint=new QLabel(QStringLiteral("选择基础色，再拖动曝光调整 HDR 强度；+1 EV = 强度翻倍。"));hint->setWordWrap(true);layout->addWidget(hint);
    auto *exposure_row=new QHBoxLayout;exposure_row->addWidget(new QLabel(QStringLiteral("颜色曝光")));
    slider_=new QSlider(Qt::Horizontal);slider_->setObjectName("hdrExposureSlider");slider_->setSingleStep(1);slider_->setPageStep(100);slider_->setMinimumWidth(160);exposure_row->addWidget(slider_,1);
    exposure_input_=new QDoubleSpinBox;exposure_input_->setObjectName("hdrExposure");exposure_input_->setDecimals(4);exposure_input_->setSingleStep(.1);exposure_input_->setSuffix(" EV");exposure_input_->setKeyboardTracking(false);exposure_row->addWidget(exposure_input_);layout->addLayout(exposure_row);
    auto *buttons=new QHBoxLayout;
    dimmer_=new QPushButton(QStringLiteral("÷2"));dimmer_->setObjectName("hdrHalf");buttons->addWidget(dimmer_);
    brighter_=new QPushButton(QStringLiteral("×2"));brighter_->setObjectName("hdrDouble");buttons->addWidget(brighter_);
    auto *zero=new QPushButton(QStringLiteral("0 EV"));zero->setObjectName("hdrZeroExposure");buttons->addWidget(zero);
    for(auto *button:{dimmer_,brighter_,zero})button->setAutoDefault(false);
    buttons->addStretch();buttons->addWidget(new QLabel(QStringLiteral("曝光倍率")));factor_=new QLabel;factor_->setObjectName("hdrMultiplier");buttons->addWidget(factor_);layout->addLayout(buttons);
    auto *preview_row=new QHBoxLayout;preview_=new QLabel;preview_->setFixedSize(42,24);preview_->setToolTip(QStringLiteral("HDR 色块按强度归一化显示，实际数值见右侧"));preview_row->addWidget(preview_);
    components_=new QLabel;components_->setObjectName("hdrLinearRgb");components_->setTextInteractionFlags(Qt::TextSelectableByMouse);preview_row->addWidget(components_,1);layout->addLayout(preview_row);
    auto *actions=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);actions->setObjectName("hdrDialogButtons");actions->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));actions->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));layout->addWidget(actions);
    connect(actions,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    // Escape handled by the embedded QDialog must close the whole editor.
    connect(picker_,&QDialog::rejected,this,&QDialog::reject);connect(picker_,&QDialog::accepted,this,&QDialog::accept);
    connect(picker_,&QColorDialog::currentColorChanged,this,[this](const QColor &chosen){if(!chosen.isValid())return;base_={hdr_color::linear(chosen.redF()),hdr_color::linear(chosen.greenF()),hdr_color::linear(chosen.blueF())};set_exposure(exposure_);});
    connect(slider_,&QSlider::valueChanged,this,[this](int value){set_exposure(value/100.);});
    connect(exposure_input_,&QDoubleSpinBox::valueChanged,this,[this](double value){set_exposure(value);});
    connect(brighter_,&QPushButton::clicked,this,[this]{set_exposure(exposure_+1);});
    connect(dimmer_,&QPushButton::clicked,this,[this]{set_exposure(exposure_-1);});
    connect(zero,&QPushButton::clicked,this,[this]{set_exposure(0);});
    sync();
  }
  const hdr_color::Color &color() const{return color_;}
};
}
