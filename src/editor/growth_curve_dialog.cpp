#include "editor/growth_curve_dialog.h"
#include "editor/parameter_widgets.h"
#include "runtime/measurement_units.h"
#include "runtime/growth_height.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QGridLayout>
#include <QGroupBox>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <optional>

namespace dfv::editor {
namespace {
QString number(double x){return QString::number(x,'g',8);}
// 两图共享同一曲线草稿，节点拖动只改变年龄，不修改基础增长。
class GrowthPlot final:public QWidget {
  runtime::GrowthCurve curve_;
  std::shared_ptr<const std::vector<runtime::CurveKnot>> heights_;
  double cm_scale_=1;
  std::optional<double> probe_age_;std::optional<QPointF> hover_position_;
  QLabel *readout_;std::string units_;QString height_error_;
  bool velocity_;
  int dragging_=-1;
  double drag_offset_=0;
  struct Axis {double low=0,high=5,step=5;} axis_;
  QRectF area() const {return {58.,24.,std::max(1.,width()-80.),std::max(1.,height()-68.)};}
  bool cm()const{return velocity_?curve_.growth_in_cm:curve_.height_in_cm;}
  bool ready()const{return !cm()||(heights_&&std::isfinite(cm_scale_)&&cm_scale_>0);}
  double value(double age)const{
    if(!cm())return velocity_?runtime::growth_curve_rate(curve_,age):runtime::growth_curve_scale(curve_,age);
    return (velocity_?runtime::growth_curve_height_rate(curve_,*heights_,age):runtime::growth_curve_height(curve_,*heights_,age))*cm_scale_;
  }
  std::vector<double> samples()const{
    std::vector<double> ages;for(int i=0;i<=240;++i)ages.push_back(curve_.planned_age*(i/240.));
    for(const auto &p:curve_.shape.points)if(p.age<=curve_.planned_age)ages.push_back(p.age);
    ages.push_back(std::nextafter(curve_.planned_age,0.));
    if(heights_)for(const auto &p:*heights_)if(p.age<=curve_.planned_age)ages.push_back(p.age);
    std::sort(ages.begin(),ages.end());return ages;
  }
  Axis calculate_axis()const{
    if(!ready())return {};
    double low=0,high=0;for(double age:samples()){const auto v=value(age);if(std::isfinite(v)){low=std::min(low,v);high=std::max(high,v);}}
    auto round_up=[](double v){return std::ceil(v-1e-10*std::max(1.,std::abs(v)));};
    const double step=5*std::max(1.,round_up((high-low)/20));
    return {-round_up(-low/step)*step,std::max(step,round_up(high/step)*step),step};
  }
  QPointF point(double age,double v) const {const auto r=area();return {r.left()+r.width()*age/curve_.planned_age,r.bottom()-r.height()*(v-axis_.low)/(axis_.high-axis_.low)};}
  double age_at(QPointF p)const{const auto r=area();return std::clamp((p.x()-r.left())/r.width()*curve_.planned_age,0.,curve_.planned_age);}
protected:
  void paintEvent(QPaintEvent *)override{
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),palette().base());
    const auto r=area();p.setPen(palette().text().color());p.drawText(QRectF(r.left(),0,r.width(),20),Qt::AlignLeft,cm()?QStringLiteral("cm"):QStringLiteral("Scale"));
    QVariantList ticks;
    for(double tick=axis_.low;tick<=axis_.high+axis_.step*.01;tick+=axis_.step){ticks.append(tick);const double y=point(0,tick).y();p.setPen(palette().mid().color());p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));p.setPen(palette().text().color());p.drawText(QRectF(0,y-9,52,18),Qt::AlignRight|Qt::AlignVCenter,QString::number(tick,'f',0));}
    setProperty("yTicks",ticks);setProperty("cmReady",bool(heights_));
    for(int i=0;i<=6;++i){const double x=r.left()+r.width()*i/6;p.drawText(QRectF(x-22,r.bottom()+3,44,18),Qt::AlignCenter,QString::number(curve_.planned_age*i/6,'g',4));}
    if(ready()){
      QPainterPath path;bool first=true;
      for(const double t:samples()){const auto q=point(t,value(t));if(!std::isfinite(q.y()))continue;if(first){path.moveTo(q);first=false;}else path.lineTo(q);}
      p.save();p.setClipRect(r.adjusted(-1,-1,1,1));p.setPen(QPen(palette().highlight().color(),2));p.drawPath(path);p.restore();
      if(velocity_){p.setPen(palette().highlight().color());p.setBrush(palette().base());for(size_t i=1;i+1<curve_.shape.points.size();++i){const auto &n=curve_.shape.points[i];if(n.age<=curve_.planned_age)p.drawEllipse(point(n.age,value(n.age)),4,4);}}
    }else p.drawText(r,Qt::AlignCenter,QStringLiteral("cm 预览计算中或不可用"));
    if(probe_age_){p.setPen(QPen(palette().text().color(),1,Qt::DashLine));p.drawLine(point(*probe_age_,axis_.low),point(*probe_age_,axis_.high));}
    p.setPen(palette().text().color());p.drawText(QRectF(0,height()-20,width(),20),Qt::AlignCenter,QStringLiteral("年龄"));
  }
  void mousePressEvent(QMouseEvent *e)override{
    if(e->button()!=Qt::LeftButton)return;
    if(velocity_&&ready())for(size_t i=1;i+1<curve_.shape.points.size();++i)if(curve_.shape.points[i].age<=curve_.planned_age&&QLineF(e->position(),point(curve_.shape.points[i].age,value(curve_.shape.points[i].age))).length()<12){dragging_=int(i);drag_offset_=curve_.shape.points[i].age-age_at(e->position());return;}
  }
  void mouseMoveEvent(QMouseEvent *e)override{
    if(dragging_>=0&&(e->buttons()&Qt::LeftButton)){
      const auto i=size_t(dragging_);const double t=std::clamp(age_at(e->position())+drag_offset_,curve_.shape.points[i-1].age+.001,curve_.shape.points[i+1].age-.001);
      if(moved)moved(i,t);
    }
    if(area().contains(e->position())){hover_position_=e->position();if(hovered)hovered(std::clamp(std::round(age_at(e->position())),0.,std::floor(curve_.planned_age)));}
    else{hover_position_.reset();if(hovered)hovered({});}
  }
  void mouseReleaseEvent(QMouseEvent *)override{dragging_=-1;}
  void leaveEvent(QEvent *)override{hover_position_.reset();readout_->hide();if(hovered)hovered({});}
public:
  std::function<void(size_t,double)> moved;
  std::function<void(std::optional<double>)> hovered;
  explicit GrowthPlot(bool velocity,QWidget *parent=nullptr):QWidget(parent),velocity_(velocity){
    setObjectName(velocity?"GrowthCurveGrowthPlot":"GrowthCurveHeightPlot");setMinimumSize(420,210);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setMouseTracking(true);
    readout_=new QLabel(this);readout_->setObjectName("GrowthCurveHoverInfo");readout_->setAttribute(Qt::WA_TransparentForMouseEvents);readout_->setMargin(6);readout_->setFrameStyle(QFrame::StyledPanel);readout_->setAutoFillBackground(true);readout_->setMaximumWidth(320);readout_->setWordWrap(true);
    auto colors=palette();colors.setColor(QPalette::Window,colors.color(QPalette::ToolTipBase));colors.setColor(QPalette::WindowText,colors.color(QPalette::ToolTipText));readout_->setPalette(colors);readout_->hide();
  }
  void probe(std::optional<double> age){
    probe_age_=age;setProperty("hoverAge",age?QVariant(*age):QVariant{});update();
    if(!age||!hover_position_){readout_->hide();return;}
    const auto height_text=heights_?QString::fromStdString(runtime::measurement_height(runtime::growth_curve_height(curve_,*heights_,*age),units_)):height_error_.isEmpty()?QStringLiteral("计算中…"):height_error_;
    readout_->setText(QStringLiteral("年龄：%1\nScale：%2\n身高：%3").arg(number(*age),number(runtime::growth_curve_scale(curve_,*age)),height_text));readout_->setToolTip(height_error_);readout_->adjustSize();
    const auto cursor=hover_position_->toPoint();int x=cursor.x()+14,y=cursor.y()+14;if(y+readout_->height()>height())y=cursor.y()-readout_->height()-12;
    x=std::clamp(x,4,std::max(4,width()-readout_->width()-4));y=std::clamp(y,4,std::max(4,height()-readout_->height()-4));readout_->move(x,y);readout_->show();readout_->raise();
  }
  void bind(const runtime::GrowthCurve &c,std::shared_ptr<const std::vector<runtime::CurveKnot>> heights,double cm_scale,const std::string &units,const QString &error){curve_=c;heights_=std::move(heights);cm_scale_=cm_scale;units_=units;height_error_=error;axis_=calculate_axis();probe(probe_age_);}
};

class PreviewWorker {
public:
  struct Result {uint64_t serial=0;std::string error;std::shared_ptr<const std::vector<runtime::CurveKnot>> heights;};
private:
  std::mutex mutex_;std::condition_variable_any ready_;
  std::optional<std::pair<uint64_t,runtime::ObjectExtension>> pending_;Result result_;
  std::jthread worker_;
public:
  explicit PreviewWorker(GrowthCurvePreview preview):worker_([this,preview=std::move(preview)](std::stop_token stop){
    double cached_adult=-1;std::map<double,double> cache;
    while(!stop.stop_requested()){
      std::unique_lock lock(mutex_);ready_.wait(lock,stop,[this]{return pending_.has_value();});if(stop.stop_requested())break;
      auto [serial,value]=*pending_;pending_.reset();lock.unlock();Result result;result.serial=serial;
      auto canceled=[&]{std::lock_guard guard(mutex_);return stop.stop_requested()||pending_.has_value();};
      try{
        if(!preview)throw std::runtime_error("当前角色尚未提供身高预览");
        {
            const double adult=value.curve.planned_age;if(cached_adult!=adult){cache.clear();cached_adult=adult;}
            std::vector<double> ages{0,adult};const double limit=std::min(adult,20.);
            for(double age=.25;age<limit;age+=.25)ages.push_back(age);ages.push_back(limit);
            for(double age:{value.baby_begin,value.baby_end})if(age>0&&age<adult)ages.push_back(age);
            std::sort(ages.begin(),ages.end());ages.erase(std::unique(ages.begin(),ages.end()),ages.end());
            auto reference=value;reference.curve.birth_scale=100;reference.curve.minimum_growth=0;for(auto &p:reference.curve.shape.points)p.rate=0;
            for(double age:ages){if(canceled())break;if(!cache.contains(age)){reference.age=age;const double h=preview(reference,true).height_cm;if(!std::isfinite(h)||h<=0)throw std::runtime_error("参考身高无效");cache[age]=h;}}
            if(!canceled()){auto heights=std::make_shared<std::vector<runtime::CurveKnot>>();for(double age:ages)heights->push_back({age,cache.at(age)});result.heights=std::move(heights);}
        }
      }catch(const std::exception &e){result.error=e.what();}
      lock.lock();if(!pending_)result_=std::move(result);
    }
  }){}
  ~PreviewWorker(){worker_.request_stop();ready_.notify_all();}
  void request(uint64_t serial,const runtime::ObjectExtension &value){std::lock_guard lock(mutex_);pending_=std::pair{serial,value};ready_.notify_one();}
  Result result(){std::lock_guard lock(mutex_);return result_;}
};
}
bool edit_growth_curve(QWidget *parent,runtime::ObjectExtension &value,const std::string &units,
    GrowthCurvePreview preview,GrowthCurveSettings &settings,GrowthCurveSettingsReader read_settings,const std::string &title){
  auto draft=value;GrowthCurveSettings edited_settings=settings;
  QDialog dialog(parent?parent->window():nullptr);dialog.setObjectName("GrowthCurveDialog");dialog.setWindowTitle(QString::fromStdString(title));
  auto *layout=new QVBoxLayout(&dialog);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);layout->addWidget(scroll,1);
  auto *body=new QWidget;scroll->setWidget(body);auto *columns=new QHBoxLayout(body);columns->setSizeConstraint(QLayout::SetMinimumSize);
  auto *left=new QVBoxLayout;columns->addLayout(left);
  auto *main_group=new QGroupBox(QStringLiteral("主要参数"));main_group->setObjectName("GrowthCurveMainParameters");left->addWidget(main_group);
  auto *main_layout=new QVBoxLayout(main_group);auto *form=new QFormLayout;main_layout->addLayout(form);
  auto spin=[](const char *name,double step=.1){auto *s=parameter_widgets::number(name);s->setMinimumWidth(70);s->setSingleStep(step);return s;};
  auto combo_row=[](QFormLayout *fields,const QString &title,QComboBox *combo){
    fields->addRow(title,combo);auto *label=qobject_cast<QLabel *>(fields->labelForField(combo));
    if(label){combo->setProperty("numericLabel",QVariant::fromValue<QObject *>(label));label->setProperty("numericRow",QStringLiteral("extension/curve/")+combo->objectName());label->setProperty("numericLabel",QVariant::fromValue<QObject *>(label));}
  };
  auto *birth=spin("GrowthCurveBirth",5),*adult=spin("GrowthCurveAdult",5),*minimum=spin("GrowthCurveMinimum");
  auto *planned=spin("GrowthCurvePlannedAge",1),*puberty=spin("GrowthCurvePubertyWeight");
  puberty->setToolTip(QStringLiteral("青春期峰值与减速阶段的基础增长乘以该权重。越高，青春期增长占比越多；按端点拟合可重新满足目标。"));
  planned->setToolTip(QStringLiteral("到达此年龄后停止增长，两图横轴同步到此年龄。修改此值不会改变任何阶段年龄或曲线形状；范围外的阶段仍会保留。"));
  form->addRow(QStringLiteral("规划到年龄"),planned);form->addRow(QStringLiteral("出生 Scale"),birth);form->addRow(QStringLiteral("成年 Scale（拟合目标）"),adult);
  form->addRow(QStringLiteral("青春期发育权重"),puberty);form->addRow(QStringLiteral("最低增长"),minimum);
  auto *mode=new QComboBox;mode->setObjectName("GrowthCurveMinimumMode");mode->addItems({QStringLiteral("参与拟合"),QStringLiteral("拟合后叠加")});combo_row(form,QStringLiteral("最低增长模式"),mode);
  auto *shape_group=new QGroupBox(QStringLiteral("曲线形状"));shape_group->setObjectName("GrowthCurveShapeParameters");left->addWidget(shape_group);
  auto *shape_layout=new QVBoxLayout(shape_group);auto *shape_form=new QFormLayout;shape_layout->addLayout(shape_form);
  auto *reference=new QComboBox;reference->setObjectName("GrowthCurveTemplate");reference->addItems({QStringLiteral("中间发育"),QStringLiteral("较早发育"),QStringLiteral("较晚发育"),QStringLiteral("自定义")});
  reference->setToolTip(QStringLiteral("选择模板立即恢复全部阶段年龄、规划到年龄和基础增长。手动修改年龄后切换为自定义；按端点拟合保留当前形状。"));
  combo_row(shape_form,QStringLiteral("参考模板"),reference);
  auto *grid=new QGridLayout;shape_layout->addLayout(grid);grid->addWidget(new QLabel(QStringLiteral("阶段")),0,0);grid->addWidget(new QLabel(QStringLiteral("年龄")),0,1);
  auto *rate_title=new QLabel(QStringLiteral("每年增长"));rate_title->setAlignment(Qt::AlignRight|Qt::AlignVCenter);rate_title->setToolTip(QStringLiteral("该年龄的实际 Scale 每年增长，包含最低增长、青春期权重及拟合倍率，最多显示一位小数。"));grid->addWidget(rate_title,0,2);
  const QStringList names={QStringLiteral("出生"),QStringLiteral("婴儿期"),QStringLiteral("婴儿期结束"),QStringLiteral("儿童期"),QStringLiteral("青春期起点"),QStringLiteral("青春期峰值"),QStringLiteral("青春期减速"),QStringLiteral("成年")};
  std::array<QDoubleSpinBox *,8> ages{};std::array<QLabel *,8> rates{};
  for(size_t i=0;i<ages.size();++i){ages[i]=spin(("GrowthCurveStageAge"+std::to_string(i)).c_str(),.5);ages[i]->setProperty("fixedParameterSettings",true);rates[i]=new QLabel;rates[i]->setAlignment(Qt::AlignRight|Qt::AlignVCenter);rates[i]->setObjectName("GrowthCurveStageRate"+QString::number(i));rates[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);grid->addWidget(new QLabel(names[int(i)]),int(i)+1,0);grid->addWidget(ages[i],int(i)+1,1);grid->addWidget(rates[i],int(i)+1,2);}
  ages.front()->setEnabled(false);
  auto *hint=new QLabel(QStringLiteral("先设置曲线形状，再按端点拟合。\n拟合保留全部阶段年龄和基础增长。"));hint->setWordWrap(true);shape_layout->addWidget(hint);
  auto *fit=new QPushButton(QStringLiteral("按端点拟合"));fit->setObjectName("GrowthCurveFit");left->addWidget(fit);left->addStretch();
  auto *right=new QVBoxLayout;columns->addLayout(right,1);
  auto chart=[&](const QString &title,const char *name,bool velocity,QComboBox *&unit){
    auto *group=new QGroupBox(title);group->setObjectName(name);auto *stack=new QVBoxLayout(group);auto *fields=new QFormLayout;
    unit=new QComboBox;unit->setObjectName(velocity?"GrowthCurveGrowthUnit":"GrowthCurveHeightUnit");unit->addItems({QStringLiteral("Scale"),QStringLiteral("cm")});combo_row(fields,QStringLiteral("纵轴"),unit);stack->addLayout(fields);
    auto *plot=new GrowthPlot(velocity);stack->addWidget(plot,1);right->addWidget(group,1);return plot;
  };
  QComboBox *height_unit=nullptr,*growth_unit=nullptr;
  auto *height_plot=chart(QStringLiteral("身高图"),"GrowthCurveHeightChart",false,height_unit);
  auto *growth_plot=chart(QStringLiteral("增长图"),"GrowthCurveGrowthChart",true,growth_unit);
  auto *error=new QLabel;error->setObjectName("GrowthCurveError");error->setWordWrap(true);layout->addWidget(error);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  std::map<std::string,double> default_steps;
  for(auto *s:body->findChildren<QDoubleSpinBox *>())default_steps["extension/curve/"+s->objectName().toStdString()]=s->singleStep();
  auto *binding=parameter_widgets::context(body);
  binding->read=[&](const std::string &key,const runtime::ParameterSettings &defaults){
    const auto i=edited_settings.find(key);if(i!=edited_settings.end())return i->second;
    auto fallback=defaults;if(const auto step=default_steps.find(key);step!=default_steps.end())fallback.step=step->second;
    if(read_settings&&key=="extension/curve/GrowthCurvePlannedAge")fallback=read_settings("extension/curve/GrowthCurveAdultAge",fallback);
    return read_settings?read_settings(key,fallback):fallback;
  };
  binding->edit=[&](const std::string &key,const runtime::ParameterSettings &s,const std::function<void()> &apply){edited_settings[key]=s;apply();};
  parameter_widgets::decorate(body,"extension/curve/");
  PreviewWorker worker(std::move(preview));uint64_t serial=0,displayed=0;QTimer debounce,poll;debounce.setSingleShot(true);debounce.setInterval(160);poll.setInterval(60);
  std::shared_ptr<const std::vector<runtime::CurveKnot>> heights;double calibrated_age=-1,cm_scale=0;QString height_error;
  GrowthPlot *hover_plot=nullptr;std::optional<double> hover_age;
  try{cm_scale=std::stod(runtime::measurement_scale(units));}catch(const std::exception &){}
  auto update_probe=[&]{for(auto *plot:{height_plot,growth_plot})plot->probe(hover_age);};
  auto bind_plot=[&]{for(auto *plot:{height_plot,growth_plot})plot->bind(draft.curve,heights,cm_scale,units,height_error);update_probe();};
  auto refresh=[&]{
    const auto &c=draft.curve;auto set=[](QDoubleSpinBox *s,double v){QSignalBlocker block(s);s->setValue(v);};
    set(birth,c.birth_scale);set(adult,c.adult_scale);set(minimum,c.minimum_growth);set(planned,c.planned_age);set(puberty,c.puberty_weight);
    {QSignalBlocker a(mode),b(reference),h(height_unit),g(growth_unit);mode->setCurrentIndex(int(c.minimum_mode));reference->setCurrentIndex(c.shape.reference_template);height_unit->setCurrentIndex(c.height_in_cm?1:0);growth_unit->setCurrentIndex(c.growth_in_cm?1:0);}
    for(size_t i=0;i<ages.size();++i){set(ages[i],c.shape.points[i].age);auto rate=QString::number(runtime::growth_curve_rate(c,c.shape.points[i].age),'f',1);if(rate.endsWith(".0"))rate.chop(2);rates[i]->setText(rate);}
    if(calibrated_age!=c.planned_age)heights.reset();if(hover_age)hover_age=std::min(*hover_age,std::floor(c.planned_age));height_error.clear();bind_plot();++serial;debounce.start();
  };
  auto edit=[&](const std::function<void(runtime::GrowthCurve &)> &change){
    auto next=draft.curve;
    try{change(next);runtime::validate_growth_curve(next);draft.curve=std::move(next);error->clear();buttons->button(QDialogButtonBox::Ok)->setEnabled(true);refresh();return true;}
    catch(const std::exception &e){refresh();error->setText(QString::fromUtf8(e.what()));return false;}
  };
  QObject::connect(birth,&QDoubleSpinBox::valueChanged,&dialog,[&](double v){edit([&](auto &c){c.birth_scale=v;});});
  QObject::connect(adult,&QDoubleSpinBox::valueChanged,&dialog,[&](double v){edit([&](auto &c){c.adult_scale=v;});});
  QObject::connect(minimum,&QDoubleSpinBox::valueChanged,&dialog,[&](double v){edit([&](auto &c){c.minimum_growth=v;});});
  QObject::connect(planned,&QDoubleSpinBox::valueChanged,&dialog,[&](double v){edit([&](auto &c){runtime::resize_growth_curve(c,v);});});
  QObject::connect(puberty,&QDoubleSpinBox::valueChanged,&dialog,[&](double v){edit([&](auto &c){c.puberty_weight=v;});});
  QObject::connect(height_unit,&QComboBox::currentIndexChanged,&dialog,[&](int v){edit([&](auto &c){c.height_in_cm=v==1;});});
  QObject::connect(growth_unit,&QComboBox::currentIndexChanged,&dialog,[&](int v){edit([&](auto &c){c.growth_in_cm=v==1;});});
  QObject::connect(mode,&QComboBox::currentIndexChanged,&dialog,[&](int v){edit([&](auto &c){c.minimum_mode=runtime::GrowthMinimumMode(v);});});
  QObject::connect(reference,&QComboBox::currentIndexChanged,&dialog,[&](int v){edit([&](auto &c){if(v==runtime::GrowthCurveShape::custom_template)runtime::mark_growth_shape_custom(c);else runtime::apply_growth_template(c,v);});});
  QObject::connect(fit,&QPushButton::clicked,&dialog,[&]{if(!edit([](auto &c){runtime::fit_growth_curve(c);})){buttons->button(QDialogButtonBox::Ok)->setEnabled(false);return;}
    if(draft.curve.growth_factor==0)error->setText(QStringLiteral("当前为匀速曲线，不存在唯一峰值；曲线形状仍保留。"));
  });
  auto change_age=[&](size_t i,double age){if(draft.curve.shape.points[i].age==age)return;edit([&](auto &c){runtime::set_growth_stage_age(c,i,age);});};
  for(size_t i=1;i<ages.size();++i)QObject::connect(ages[i],&QDoubleSpinBox::valueChanged,&dialog,[&,i](double v){change_age(i,v);});
  for(auto *plot:{height_plot,growth_plot})plot->hovered=[&,plot](std::optional<double> age){
    if(age){hover_plot=plot;hover_age=age;}else if(hover_plot==plot){hover_plot=nullptr;hover_age.reset();}update_probe();
  };
  growth_plot->moved=change_age;
  QObject::connect(&debounce,&QTimer::timeout,&dialog,[&]{auto request=draft;request.curve.enabled=true;worker.request(serial,request);});
  QObject::connect(&poll,&QTimer::timeout,&dialog,[&]{const auto r=worker.result();if(r.serial!=serial||r.serial==displayed)return;displayed=r.serial;
    height_error=QString::fromStdString(r.error);if(r.heights){heights=r.heights;calibrated_age=draft.curve.planned_age;}bind_plot();
  });
  QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  refresh();poll.start();const auto screen=dialog.screen()->availableGeometry();dialog.resize(std::min(1080,screen.width()*9/10),std::min(860,screen.height()*9/10));
  if(dialog.exec()!=QDialog::Accepted)return false;
  value.curve=draft.curve;settings=std::move(edited_settings);return true;
}
int growth_curve_preview(){
  runtime::ObjectExtension example;example.kind=runtime::ExtensionKind::growth;example.curve.enabled=true;GrowthCurveSettings settings;
  edit_growth_curve(nullptr,example,"1",[](const runtime::ObjectExtension &v,bool){runtime::WeightResult result;result.height_cm=1.8*runtime::growth_curve_scale(v.curve,v.age);return result;},settings,{},"生长曲线调试（示例身高：Scale 100 = 180 cm）");
  return 0;
}

}
