#include "editor/parameter_widgets.h"
#include "editor/extension_panel.h"
#include "editor/numeric_spinbox.h"
#include "runtime/measurement_units.h"
#include <QToolButton>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>

namespace dfv::editor {
namespace {
QString text(const std::string &s){return QString::fromUtf8(s.data(),qsizetype(s.size()));}
class RoundButton final:public QToolButton {
  QPointer<QWidget> popup_;
  bool entered_=false;
  void leave_later(){QTimer::singleShot(100,this,[this]{if(popup_&&!popup_->frameGeometry().contains(QCursor::pos())&&(entered_||!underMouse()))cancel();});}
  void show_editor(){
    if(popup_)return;entered_=false;
    auto *popup=new QFrame(this,Qt::Tool|Qt::FramelessWindowHint);popup_=popup;popup->setObjectName("GrowthRoundPopup");popup->setProperty("parameterSettingsPopup",true);popup->setFrameShape(QFrame::StyledPanel);popup->setAttribute(Qt::WA_ShowWithoutActivating);popup->setAutoFillBackground(true);
    auto *layout=new QVBoxLayout(popup);auto *row=new QHBoxLayout;auto *label=new QLabel(QStringLiteral("对齐到 10 的幂"));auto *exponent=new QSpinBox;exponent->setObjectName("GrowthRoundExponent");exponent->setRange(-9,std::numeric_limits<int>::max());exponent->setValue(digits);exponent->setKeyboardTracking(false);row->addWidget(label);row->addWidget(exponent);layout->addLayout(row);
    for(auto *w:std::vector<QWidget *>{label,exponent}){w->setProperty("numericRow","roundExponent");w->setProperty("numericLabel",QVariant::fromValue<QObject *>(label));}parameter_widgets::context(popup)->wheel_selection();
    auto *hint=new QLabel(QStringLiteral("1：十位（175 → 180）\n0：整数；−4：小数点后 4 位\n支持 −9 及以上整数，正方向不设业务上限。"));layout->addWidget(hint);
    auto *ok=new QPushButton(QStringLiteral("OK"));ok->setObjectName("GrowthRoundOK");layout->addWidget(ok);
    connect(ok,&QPushButton::clicked,this,[this,exponent]{exponent->interpretText();digits=exponent->value();if(edited)edited(digits);cancel();});
    popup->installEventFilter(this);popup->adjustSize();const auto area=screen()->availableGeometry();const auto top=mapToGlobal(QPoint());int x=std::clamp(top.x()+width()-popup->width(),area.left(),std::max(area.left(),area.right()-popup->width()+1));int y=top.y()+height()-1;if(y+popup->height()>area.bottom()+1)y=top.y()-popup->height()+1;popup->move(x,std::clamp(y,area.top(),std::max(area.top(),area.bottom()-popup->height()+1)));popup->show();
  }
protected:
  bool eventFilter(QObject *o,QEvent *e) override {
    if(o==this&&e->type()==QEvent::Enter)QTimer::singleShot(260,this,[this]{if(underMouse()&&isEnabled())show_editor();});
    if((o==this||o==popup_)&&e->type()==QEvent::Leave)leave_later();
    if(o==popup_&&e->type()==QEvent::Enter)entered_=true;
    if(o==popup_&&e->type()==QEvent::WindowDeactivate)cancel();
    return QToolButton::eventFilter(o,e);
  }
  void hideEvent(QHideEvent *e) override {cancel();QToolButton::hideEvent(e);}
public:
  int digits=1;std::function<void(int)> edited;
  RoundButton(){setObjectName("GrowthRound");setText("round");setCheckable(true);setAutoRaise(true);setToolTip(QStringLiteral("点击启用／禁用 Scale 四舍五入；悬停设置取整位数"));installEventFilter(this);}
  void cancel(){auto p=popup_;popup_.clear();if(p){p->setObjectName({});p->hide();p->deleteLater();}}
};
}
ExtensionPanel::ExtensionPanel(QWidget *parent):QWidget(parent){
  setObjectName("ObjectExtensionPanel");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(4);
  header_=new QToolButton;header_->setObjectName("ExtensionCollapse");header_->setCheckable(true);header_->setChecked(true);header_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header_->setArrowType(Qt::DownArrow);layout->addWidget(header_);
  body_=new QWidget;layout->addWidget(body_);auto *body=new QVBoxLayout(body_);body->setContentsMargins(4,0,0,0);body->setSpacing(5);
  connect(header_,&QToolButton::toggled,this,[this](bool on){body_->setVisible(on);header_->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});
  growth_=new QWidget;body->addWidget(growth_);auto *form=new QVBoxLayout(growth_);form->setContentsMargins(0,0,0,0);form->setSpacing(4);
  auto spin=[&](const char *name,double lo,double hi,double step){auto *s=new NumericSpinBox(false);s->setObjectName(name);s->setDecimals(9);s->setRange(lo,hi);s->setSingleStep(step);s->setKeyboardTracking(false);s->setFocusPolicy(Qt::StrongFocus);s->setMinimumWidth(48);s->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);return s;};
  const auto unlimited=std::numeric_limits<float>::max();
  age_=spin("GrowthAge",1,20,1);step_=spin("GrowthAgeStep",-unlimited,unlimited,.25);sense_=spin("GrowthSensitivity",-unlimited,unlimited,2.5);strength_=spin("GrowthStrength",-unlimited,unlimited,.125);density_=spin("ObjectDensity",0,100000,100);
  for(auto *s:{age_,step_,sense_,strength_})s->setProperty("fixedParameterSettings",true);
  auto *age_row=new QHBoxLayout;age_row->setSpacing(4);age_row->addWidget(new QLabel(QStringLiteral("年龄")));age_row->addWidget(age_,1);age_row->addSpacing(6);age_row->addWidget(new QLabel(QStringLiteral("步长")));age_row->addWidget(step_,1);form->addLayout(age_row);
  auto *shape_row=new QHBoxLayout;shape_row->setSpacing(4);shape_row->addWidget(new QLabel(QStringLiteral("体格")));shape_row->addWidget(strength_,1);shape_row->addSpacing(6);shape_row->addWidget(new QLabel(QStringLiteral("每年增长")));shape_row->addWidget(sense_,1);
  sense_->setToolTip(QStringLiteral("每增加一岁，实际等比缩放增加的百分点"));
  auto *smaller=new QPushButton(QStringLiteral("−"));auto *larger=new QPushButton(QStringLiteral("+"));smaller->setObjectName("GrowthScaleDown");larger->setObjectName("GrowthScaleUp");
  for(auto *button:{smaller,larger}){button->setFixedWidth(24);shape_row->addWidget(button);button->setToolTip(QStringLiteral("按步长 × 每年增长调整缩放，年龄保持当前值"));}form->addLayout(shape_row);
  multiplier_=spin("GrowthMultiplier",1e-9,unlimited,.05);multiplier_->setProperty("fixedParameterSettings",true);multiplier_->setToolTip(QStringLiteral("仅在点击 ×／÷ 时乘除当前 Scale；不随其他参数联动"));
  auto *multiplier_row=new QHBoxLayout;multiplier_row->setSpacing(4);multiplier_row->addWidget(new QLabel(QStringLiteral("倍率")));multiplier_row->addWidget(multiplier_,1);
  auto *round=new RoundButton;round_=round;multiplier_row->addWidget(round_);auto *times=new QPushButton(QStringLiteral("×"));auto *divide=new QPushButton(QStringLiteral("÷"));times->setObjectName("GrowthMultiply");divide->setObjectName("GrowthDivide");for(auto *b:{times,divide}){b->setFixedWidth(24);multiplier_row->addWidget(b);}form->addLayout(multiplier_row);
  auto notify=[this](bool shape,double scale=0){if(changed)changed(value_,shape,scale);};
  connect(multiplier_,&QDoubleSpinBox::valueChanged,this,[this,notify](double v){value_.scale_multiplier=v;notify(false);});
  connect(round_,&QToolButton::clicked,this,[this,notify](bool on){value_.scale_round=on;notify(false);});round->edited=[this,notify](int digits){value_.scale_round_exponent=digits;notify(false);};
  connect(times,&QPushButton::clicked,this,[this]{if(multiply_scale)multiply_scale(false);});connect(divide,&QPushButton::clicked,this,[this]{if(multiply_scale)multiply_scale(true);});
  connect(age_,&QDoubleSpinBox::valueChanged,this,[this,notify](double v){value_.age=v;notify(true);});
  connect(step_,&QDoubleSpinBox::valueChanged,this,[this,notify](double v){value_.age_step=v;age_->setSingleStep(std::abs(v));notify(false);});
  connect(sense_,&QDoubleSpinBox::valueChanged,this,[this,notify](double v){value_.sensitivity=v;notify(false);});
  connect(strength_,&QDoubleSpinBox::valueChanged,this,[this,notify](double v){value_.strength=v;notify(true);});
  connect(smaller,&QPushButton::clicked,this,[this,notify]{notify(false,-value_.age_step);});connect(larger,&QPushButton::clicked,this,[this,notify]{notify(false,value_.age_step);});
  density_row_=new QWidget;auto *df=new QFormLayout(density_row_);df->setContentsMargins(0,0,0,0);df->addRow(QStringLiteral("密度（kg/m³）"),density_);body->addWidget(density_row_);
  connect(density_,&QDoubleSpinBox::valueChanged,this,[this,notify](double v){value_.density=v;notify(false);});
  summary_=new QLabel(QStringLiteral("等待测量…"));summary_->setObjectName("WeightSummary");summary_->setWordWrap(true);summary_->setTextFormat(Qt::RichText);auto font=summary_->font();font.setPointSizeF(font.pointSizeF()+1);summary_->setFont(font);body->addWidget(summary_);
  status_=new QLabel;status_->setWordWrap(true);status_->setObjectName("WeightStatus");status_->hide();body->addWidget(status_);
  details_=new QToolButton;details_->setObjectName("WeightDetails");details_->setText(QStringLiteral("部位重量详情"));details_->setCheckable(true);details_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);details_->setArrowType(Qt::RightArrow);body->addWidget(details_);
  parts_=new QLabel;parts_->setObjectName("WeightParts");parts_->setTextFormat(Qt::RichText);parts_->setWordWrap(true);parts_->setFont(font);parts_->setAlignment(Qt::AlignTop|Qt::AlignLeft);
  parts_->hide();parts_scroll_=parts_;body->addWidget(parts_);
  connect(details_,&QToolButton::toggled,this,[this](bool on){parts_scroll_->setVisible(on);details_->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});parameter_widgets::decorate(this,"extension/");hide();
}
void ExtensionPanel::bind(const runtime::ObjectExtension &v,bool selection){
  const bool reset=selection||v.kind!=value_.kind;value_=v;
  if(reset){has_result_=false;summary_->setText(QStringLiteral("等待测量…"));summary_->setToolTip({});status_->clear();status_->hide();parts_->clear();details_->setChecked(false);}
  setVisible(v.kind!=runtime::ExtensionKind::none);if(v.kind==runtime::ExtensionKind::none)return;
  const bool character=v.kind==runtime::ExtensionKind::growth;header_->setText(character?QStringLiteral("生长与体重"):QStringLiteral("密度与重量"));growth_->setVisible(character);density_row_->setVisible(!character);details_->setVisible(character);
  for(const auto &[s,x]:std::vector<std::pair<QDoubleSpinBox *,double>>{{age_,v.age},{step_,v.age_step},{sense_,v.sensitivity},{strength_,v.strength},{density_,v.density},{multiplier_,v.scale_multiplier}}){QSignalBlocker block(s);s->setValue(x);}
  round_->setChecked(v.scale_round);auto *round=static_cast<RoundButton *>(round_);if(reset||round->digits!=v.scale_round_exponent)round->cancel();round->digits=v.scale_round_exponent;
  age_->setSingleStep(std::abs(v.age_step));
  parts_scroll_->setVisible(character&&details_->isChecked());
}
void ExtensionPanel::expand(){header_->setChecked(true);}
void ExtensionPanel::pending(){status_->clear();status_->hide();}
void ExtensionPanel::measurement_scale(const std::string &s){measurement_scale_=runtime::measurement_scale(s);if(has_result_)render_result();}
void ExtensionPanel::result(const runtime::WeightResult &v,const std::string &error){measured_=v;error_=error;has_result_=true;render_result();}
void ExtensionPanel::render_result(){
  if(!error_.empty()){summary_->setText(QStringLiteral("测量未完成"));status_->setText(text(error_));status_->show();parts_->clear();return;}
  const auto &v=measured_;const bool character=value_.kind==runtime::ExtensionKind::growth;
  auto mass=[&](double kg){return text(runtime::measurement_mass(kg,measurement_scale_));};
  summary_->setText((character?QStringLiteral("<b>身高：</b>%1　").arg(text(runtime::measurement_height(v.height_cm,measurement_scale_))):QString{})+QStringLiteral("<b>体重：</b>%1").arg(mass(v.kg)));
  status_->clear();status_->hide();QStringList missing;for(const auto &name:v.missing_morphs)missing<<text(name);summary_->setToolTip(missing.isEmpty()?QString{}:QStringLiteral("未提供的生长参数：\n")+missing.join('\n'));
  QStringList lines;if(v.parts.size()>=13){
    auto part=[&](size_t index,const QString &label,bool bold=false){const auto &p=v.parts[index];return (bold?QStringLiteral("<b>%1：</b>"):QStringLiteral("%1：")).arg(label)+(p.triangles?mass(p.kg):QStringLiteral("—"));};
    lines<<part(3,QStringLiteral("手臂"),true)+QStringLiteral("，")+part(2,QStringLiteral("上臂"))+QStringLiteral("，")+part(1,QStringLiteral("前臂"));
    lines<<part(7,QStringLiteral("腿"),true)+QStringLiteral("，")+part(6,QStringLiteral("大腿"))+QStringLiteral("，")+part(5,QStringLiteral("小腿"));
    lines<<part(0,QStringLiteral("手"),true)+QStringLiteral("，")+part(8,QStringLiteral("拇指"))+QStringLiteral("，")+part(9,QStringLiteral("食指"));
    lines<<part(10,QStringLiteral("中指"))+QStringLiteral("，")+part(11,QStringLiteral("无名指"))+QStringLiteral("，")+part(12,QStringLiteral("小指"));
    lines<<part(4,QStringLiteral("脚"),true);
  }parts_->setText(lines.join(QStringLiteral("<br>")));
}
bool edit_measurement_scale(QWidget *parent,std::string &scale){
  QDialog dialog(parent);dialog.setObjectName("MeasurementUnitsDialog");dialog.setWindowTitle(QStringLiteral("测量单位"));auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;auto *input=new QLineEdit(text(scale));input->setObjectName("MeasurementScale");input->setMaxLength(128);auto *row=new QHBoxLayout;row->addWidget(input);row->addWidget(new QLabel(QStringLiteral("×")));form->addRow(QStringLiteral("全局缩放倍率"),row);layout->addLayout(form);
  auto *description=new QLabel(QStringLiteral("身高按倍率缩放，体重按倍率的立方缩放。\n支持小数和科学计数法，例如 0.1、10、1e6。"));layout->addWidget(description);auto *error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults);layout->addWidget(buttons);std::string next;
  QObject::connect(buttons->button(QDialogButtonBox::RestoreDefaults),&QPushButton::clicked,&dialog,[&]{input->setText("1");});QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{try{next=runtime::measurement_scale(input->text().trimmed().toStdString());dialog.accept();}catch(const std::exception &e){error->setText(text(e.what()));}});
  if(dialog.exec()!=QDialog::Accepted)return false;scale=next;return true;
}
}
