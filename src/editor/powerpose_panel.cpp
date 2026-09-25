#include "editor/powerpose_panel.h"
#include "bench/telemetry.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QKeyEvent>
#include <QToolTip>
#include <QSignalBlocker>

namespace dfv::editor {
namespace {
QString text(const std::string &s) {return QString::fromUtf8(s);}
class SingleLineLabel final:public QLabel {
public:
  explicit SingleLineLabel(const QString &value):QLabel(value) {setFixedHeight(fontMetrics().height()+2);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);}
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);p.setPen(palette().color(isEnabled()?QPalette::Active:QPalette::Disabled,QPalette::WindowText));
    p.drawText(contentsRect(),Qt::AlignLeft|Qt::AlignVCenter,fontMetrics().elidedText(text(),Qt::ElideRight,contentsRect().width()));
  }
};
}
class PowerPoseCanvas final:public QWidget {
  PowerPosePanel &panel_;
public:
  explicit PowerPoseCanvas(PowerPosePanel &p):QWidget(&p),panel_(p) {setObjectName("powerpose.canvas");setMinimumSize(220,280);setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);}
  QRectF area() const {const double scale=std::min(width()/480.,height()/720.);return {(width()-480*scale)*.5,(height()-720*scale)*.5,480*scale,720*scale};}
  QPointF position(const runtime::PosePoint &p) const {const auto a=area();return a.topLeft()+QPointF(panel_.male_?p.male_x:p.x,panel_.male_?p.male_y:p.y)*(a.width()/480.);}
  int hit(QPointF pos) const {int result=-1;double nearest=100;for(size_t i=0;i<panel_.points_.size();++i) {const auto d=position(*panel_.points_[i].point)-pos;const double distance=QPointF::dotProduct(d,d);if(distance<nearest) {nearest=distance;result=int(i);}}return result;}
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor(53,54,58));const auto a=area();
    const auto &bg=panel_.backgrounds_[panel_.page_];if(!bg.isNull()) p.drawPixmap(a,bg,bg.rect());
    else {p.setPen(QColor(130,135,140));p.drawText(a.adjusted(10,10,-10,-10),Qt::AlignBottom|Qt::TextWordWrap,QStringLiteral("内容库中未找到模板底图；控制点仍可使用。"));}
    const double radius=std::clamp(a.width()/480.*6.,4.,7.);
    for(size_t i=0;i<panel_.points_.size();++i) {const auto &b=panel_.points_[i];const auto &point=*b.point;const auto at=position(point);
      const bool enabled=!point.disabled&&(point.kind==runtime::PosePointKind::navigation||(panel_.ready_&&b.editable));
      p.setPen(QPen(i==panel_.selected_?QColor(255,212,104):QColor(25,35,39),i==panel_.selected_?2.:1.3));p.setBrush(enabled?QColor(0,232,181):QColor(106,115,119));
      if(point.kind==runtime::PosePointKind::navigation) p.drawPolygon(QPolygonF{at+QPointF(-radius,-radius),at+QPointF(radius+2,0),at+QPointF(-radius,radius)});
      else if(point.kind==runtime::PosePointKind::property) p.drawPolygon(QPolygonF{at+QPointF(0,-radius-1),at+QPointF(radius+1,0),at+QPointF(0,radius+1),at+QPointF(-radius-1,0)});
      else p.drawEllipse(at,radius,radius);
      if(point.kind==runtime::PosePointKind::group) {p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(50,225,185),1,Qt::DotLine));p.drawEllipse(at,radius+3,radius+3);}
      bool pinned=false;for(const auto &pin:panel_.pins_) if(pin.skin==panel_.skin_&&std::find(b.joints.begin(),b.joints.end(),pin.joint)!=b.joints.end()&&(pin.position||pin.angle)) pinned=true;
      if(pinned) {p.setPen(QPen(QColor(255,212,104),1.4));p.setBrush(QColor(55,55,55));const auto lock=at+QPointF(radius+2,-radius-5);p.drawRoundedRect(QRectF(lock,QSizeF(7,6)),1,1);p.drawArc(QRectF(lock+QPointF(1,-4),QSizeF(5,7)),0,180*16);}
    }
  }
  void mousePressEvent(QMouseEvent *e) override {if(!panel_.automated_input_||!e->spontaneous()) panel_.press(e->position(),e->button(),e->modifiers());e->accept();}
  void mouseMoveEvent(QMouseEvent *e) override {if(!panel_.automated_input_||!e->spontaneous()) panel_.move(e->position());e->accept();}
  void mouseReleaseEvent(QMouseEvent *e) override {if(!panel_.automated_input_||!e->spontaneous()) panel_.release(e->position(),e->button());e->accept();}
  void contextMenuEvent(QContextMenuEvent *e) override {e->accept();}
};
PowerPosePanel::PowerPosePanel(QWidget *parent):QWidget(parent) {
  setObjectName("powerpose.panel");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(6,6,6,6);layout->setSpacing(3);
  source_=new SingleLineLabel(QStringLiteral("请选择一个 Genesis 8 / 8.1 角色"));layout->addWidget(source_);
  auto *form=new QGridLayout;sets_=new QComboBox;sets_->setObjectName("powerpose.set");sets_->addItem(QStringLiteral("Base"));sets_->setEnabled(false);
  sets_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);sets_->setMinimumContentsLength(5);sets_->setMinimumWidth(60);sets_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
  pages_=new QComboBox;pages_->setObjectName("powerpose.template");pages_->addItems({"Body","Hands","Head"});
  form->setHorizontalSpacing(5);form->addWidget(new QLabel("Template Set"),0,0);form->addWidget(sets_,0,1);form->addWidget(new QLabel("Template"),0,2);form->addWidget(pages_,0,3);form->setColumnStretch(1,1);layout->addLayout(form);
  canvas_=new PowerPoseCanvas(*this);layout->addWidget(canvas_,1);details_=new SingleLineLabel(QStringLiteral("选择控制点查看操作；Shift 拖动精调。"));layout->addWidget(details_);
  auto *controls=new QGridLayout;controls->setVerticalSpacing(2);controls->setColumnStretch(0,1);controls->setColumnStretch(1,1);const QString names[]={QStringLiteral("左键 ↔"),QStringLiteral("左键 ↕"),QStringLiteral("右键 ↔"),QStringLiteral("右键 ↕")};
  for(int i=0;i<4;++i) {controls_[i]=new SingleLineLabel(names[i]+QStringLiteral("：—"));controls->addWidget(controls_[i],i%2,i/2);}layout->addLayout(controls);
  connect(pages_,&QComboBox::currentIndexChanged,this,[this](int p){change_page(p);});qApp->installEventFilter(this);
  for(const auto &p:runtime::powerpose_templates()[0].points) {runtime::BoundPosePoint b;b.point=&p;points_.push_back(b);}
}
QWidget *PowerPosePanel::canvas() const {return canvas_;}
QPointF PowerPosePanel::point_position(const std::string &id) const {for(const auto &b:points_) if(b.point->id==id) return canvas_->position(*b.point);return {-100,-100};}
void PowerPosePanel::bind(const runtime::Skin *skin,int index,int target,uint64_t generation,uint64_t revision,const std::vector<std::filesystem::path> &roots,bool ready,const std::vector<runtime::PosePin> &pins) {
  const bool changed=generation_!=generation||skin_!=index||target_!=target||instance_!=(skin?skin->id:"");
  if(changed||revision_!=revision||!ready) cancel();
  generation_=generation;revision_=revision;skin_=index;target_=target;ready_=ready&&skin;instance_=skin?skin->id:"";pins_=pins;
  if(changed) {
    male_=skin&&!skin->joints.empty()&&skin->joints.front().id.find("Male")!=std::string::npos;
    const bool g81=skin&&!skin->joints.empty()&&skin->joints.front().id.find("8_1")!=std::string::npos;
    const QString set=QStringLiteral("Base (Genesis %1 %2)").arg(g81?"8.1":"8",male_?"Male":"Female");sets_->setItemText(0,skin?set:QStringLiteral("Base"));
    sets_->setToolTip(sets_->currentText());
    for(int page=0;page<3;++page) {backgrounds_[page]={};const auto name=runtime::powerpose_templates()[page].name;
      // 优先所选角色对应世代，缺图时用同一性别的另一代原图。
      for(bool modern:{g81,!g81}) {for(const auto &root:roots) {
        const std::string folder=std::string(male_?"Male":"Female")+(modern?" 8_1":"");const std::string prefix=std::string(modern?"G8_1":"G8")+(male_?"M_":"F_");
        const auto file=root/"data"/"DAZ 3D"/"Genesis 8"/folder/"Tools"/"PowerPose"/(prefix+name+".png");
        if(backgrounds_[page].load(QString::fromStdWString(file.wstring()))) break;
      }if(!backgrounds_[page].isNull()) break;}
    }
    selected_=-1;
  }
  points_.clear();descriptions_.clear();
  for(const auto &p:runtime::powerpose_templates()[page_].points) {
    auto b=skin?runtime::bind_powerpose(*skin,p):runtime::BoundPosePoint{};b.point=&p;points_.push_back(b);
    QString desc;
    if(skin) for(size_t slot=0;slot<4;++slot) {QStringList bindings;for(const auto &c:b.slots[slot]) bindings<<text(skin->joints[c.joint].name+" · "+skin->joints[c.joint].channels[c.channel].label)+QStringLiteral(" %1%2 / px").arg(c.rate>=0?"+":"").arg(c.rate,0,'g',3);desc+=bindings.join(QStringLiteral("；"))+"\n";}
    descriptions_.push_back(desc);
  }
  source_->setText(skin?text(skin->id)+(ready_?QString{}:QStringLiteral(" · 等待场景就绪")):QStringLiteral("请选择同一 Genesis 8 / 8.1 角色的对象或骨骼"));
  source_->setToolTip(source_->text());
  show_controls(selected_);canvas_->update();
}
void PowerPosePanel::change_page(int page) {
  if(page<0||page>2||page==page_) return;cancel();page_=page;selected_=-1;points_.clear();descriptions_.clear();
  for(const auto &p:runtime::powerpose_templates()[page].points) {runtime::BoundPosePoint b;b.point=&p;points_.push_back(b);}
  show_controls(-1);canvas_->update(); // Editor 下一 tick 解析新页；导航保持角色选择。
}
void PowerPosePanel::show_controls(int index) {
  const QString names[]={QStringLiteral("左键 ↔"),QStringLiteral("左键 ↕"),QStringLiteral("右键 ↔"),QStringLiteral("右键 ↕")};
  if(index<0||size_t(index)>=points_.size()) {details_->setText(QStringLiteral("选择控制点查看操作；Shift 拖动精调。"));details_->setToolTip(details_->text());for(int i=0;i<4;++i) {controls_[i]->setText(names[i]+QStringLiteral("：—"));controls_[i]->setToolTip({});}return;}
  const auto &b=points_[index];const auto &p=*b.point;details_->setText(text(p.id+" · "+p.label)+(p.disabled?QStringLiteral(" · 不支持 Face"):QString{}));
  const auto lines=size_t(index)<descriptions_.size()?descriptions_[index].split('\n'):QStringList{};
  for(int i=0;i<4;++i) {const QString desc=lines.value(i);QStringList labels;
    for(const auto &entry:desc.split(QStringLiteral("；"),Qt::SkipEmptyParts)) {auto label=entry.section(QStringLiteral(" · "),1).section(' ',0,-4);if(!labels.contains(label)) labels<<label;}
    controls_[i]->setText(names[i]+QStringLiteral("：")+(labels.isEmpty()?QStringLiteral("无变化"):labels.join(" / ")));controls_[i]->setToolTip(desc);}
  QStringList unavailable;for(const auto &name:b.unavailable) unavailable<<text(name);details_->setToolTip(details_->text()+"\n"+(unavailable.isEmpty()?QStringLiteral("位移相对按下位置；Esc 取消。数值受角色自身锁定与限位约束。"):QStringLiteral("以下绑定不可编辑：\n")+unavailable.join('\n')));
}
void PowerPosePanel::emit_input() {++gesture_.event;gesture_.input_seconds=now();if(input) input(gesture_);}
void PowerPosePanel::press(QPointF pos,Qt::MouseButton button,Qt::KeyboardModifiers modifiers) {
  if(button_!=Qt::NoButton||(button!=Qt::LeftButton&&button!=Qt::RightButton)) return;
  const int i=canvas_->hit(pos);if(i<0) return;selected_=i;show_controls(i);canvas_->update();
  const auto b=points_[i];if(b.point->disabled) return;
  if(b.point->kind!=runtime::PosePointKind::navigation&&(!ready_||!b.editable)) return;
  if(b.point->kind!=runtime::PosePointKind::navigation&&selection) selection(b);
  start_=pos;button_=button;gesture_={};gesture_.serial=++serial_;gesture_.generation=generation_;gesture_.revision=revision_;gesture_.skin=skin_;gesture_.target=target_;gesture_.instance=instance_;gesture_.point=b.point->id;
  gesture_.held=true;gesture_.right=button==Qt::RightButton;gesture_.precision=modifiers.testFlag(Qt::ShiftModifier)?.1:1;
  if(!automated_input_) {canvas_->setFocus(Qt::MouseFocusReason);canvas_->grabMouse();}if(b.point->kind!=runtime::PosePointKind::navigation) emit_input();
}
void PowerPosePanel::move(QPointF pos) {
  if(button_==Qt::NoButton) {const int i=canvas_->hit(pos);canvas_->setToolTip(i<0?QString{}:text(points_[i].point->id+" · "+points_[i].point->label));return;}
  const auto delta=pos-start_;if(!gesture_.moved&&QPointF::dotProduct(delta,delta)<9) return;
  gesture_.moved=true;gesture_.dx=delta.x();gesture_.dy=delta.y();
  if(points_[selected_].point->kind!=runtime::PosePointKind::navigation) emit_input();
}
void PowerPosePanel::release(QPointF pos,Qt::MouseButton button) {
  if(button!=button_) return;move(pos);button_=Qt::NoButton;canvas_->releaseMouse();gesture_.held=false;
  if(selected_<0) return;const auto &p=*points_[selected_].point;
  if(p.kind==runtime::PosePointKind::navigation) {if(!gesture_.moved&&button==Qt::LeftButton&&canvas_->hit(pos)==selected_) {for(int i=0;i<3;++i) if(runtime::powerpose_templates()[i].name==p.target) pages_->setCurrentIndex(i);}return;}
  emit_input();if(!gesture_.moved&&button==Qt::RightButton) context(selected_,canvas_->mapToGlobal(pos.toPoint()));
}
void PowerPosePanel::cancel() {
  if(button_==Qt::NoButton) return;button_=Qt::NoButton;gesture_.held=false;gesture_.cancelled=true;canvas_->releaseMouse();emit_input();
}
bool PowerPosePanel::eventFilter(QObject *object,QEvent *event) {
  if(automated_input_&&event->spontaneous()) return false;
  if(button_!=Qt::NoButton) {
    if((event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape)) {cancel();return true;}
    if(event->type()==QEvent::ApplicationDeactivate||(object==window()&&event->type()==QEvent::WindowDeactivate)||(object==canvas_&&(event->type()==QEvent::Hide||event->type()==QEvent::FocusOut))) cancel();
  }return QWidget::eventFilter(object,event);
}
void PowerPosePanel::context(int index,QPoint global) {
  const auto b=points_.at(index);const auto generation=generation_;const auto revision=revision_;const auto instance=instance_;
  auto *menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
  auto add=[&](QString name,int operation,bool checkable=false,bool checked=false) {auto *a=menu->addAction(name);a->setCheckable(checkable);a->setChecked(checked);connect(a,&QAction::triggered,this,[this,b,generation,revision,instance,operation](bool value){if(generation_==generation&&revision_==revision&&instance_==instance&&ready_&&action) action(b,operation,value);});};
  add(QStringLiteral("定位到参数"),0);add(QStringLiteral("恢复加载时的本点参数"),1);
  if(b.pin_allowed) {bool position=false,angle=false;for(const auto &pin:pins_) if(pin.skin==skin_&&pin.joint==b.joints.front()) {position=pin.position;angle=pin.angle;}
    add(QStringLiteral("固定位置（IK）"),2,true,position);add(QStringLiteral("固定角度（IK）"),3,true,angle);}
  menu->popup(global);
}
}
