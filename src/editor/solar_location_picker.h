#pragma once
#include <QApplication>
#include <QCursor>
#include <QFrame>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include "render_ir/sun_sky.h"

namespace dfv::editor {
// SS 纬度/经度的局部交互；与“限 / 精”一样，只在 OK 时提交草稿。
class SolarLocationMap final:public QWidget {
  std::array<double,2> location_{}; // latitude, longitude
  static const QPainterPath &land() {
    static const auto path=[] {
#include "natural_earth/mercator_land.inl"
      QPainterPath result;
      for(size_t ring=1;ring<std::size(land_offsets);++ring){
        for(auto i=land_offsets[ring-1];i<land_offsets[ring];++i){const QPointF p(land_points[i][0]/65535.,land_points[i][1]/65535.);if(i==land_offsets[ring-1])result.moveTo(p);else result.lineTo(p);}
        result.closeSubpath();
      }
      return result;
    }();
    return path;
  }
protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const auto box=map_rect();
    const bool dark=palette().color(QPalette::Window).lightness()<128;
    p.fillRect(box,dark?QColor(29,44,55):QColor(218,233,242));
    QTransform transform;transform.translate(box.left(),box.top());transform.scale(box.width(),box.height());
    p.setPen(QPen(dark?QColor(149,167,172):QColor(119,145,148),.6));p.setBrush(dark?QColor(76,96,95):QColor(185,202,187));p.drawPath(transform.map(land()));
    p.setBrush(Qt::NoBrush);p.setPen(QPen(dark?QColor(220,230,240,40):QColor(70,100,120,45),1));
    for(int lon=-120;lon<=120;lon+=60){const auto x=project(0,lon).x();p.drawLine(QPointF(x,box.top()),QPointF(x,box.bottom()));}
    for(int lat=-60;lat<=60;lat+=30){const auto y=project(lat,0).y();p.drawLine(QPointF(box.left(),y),QPointF(box.right(),y));}
    p.setPen(palette().color(QPalette::Text));
    p.drawText(QRectF(box.left(),box.top()+4,box.width(),20),Qt::AlignHCenter|Qt::AlignTop,QStringLiteral("北 / N"));
    p.drawText(QRectF(box.left()+6,box.center().y()+4,85,20),Qt::AlignLeft,QStringLiteral("180° W"));
    p.drawText(QRectF(box.right()-91,box.center().y()+4,85,20),Qt::AlignRight,QStringLiteral("180° E"));
    const auto marker=project(location_[0],location_[1]);p.save();p.setClipRect(box);
    p.setPen(QPen(QColor(15,25,30),4));p.drawEllipse(marker,6,6);
    p.setPen(QPen(QColor(255,199,73),2));p.drawEllipse(marker,6,6);
    p.drawLine(marker+QPointF(-12,0),marker+QPointF(-3,0));p.drawLine(marker+QPointF(3,0),marker+QPointF(12,0));
    p.drawLine(marker+QPointF(0,-12),marker+QPointF(0,-3));p.drawLine(marker+QPointF(0,3),marker+QPointF(0,12));p.restore();
    p.setPen(palette().color(QPalette::Mid));p.drawRect(box);
  }
  void mousePressEvent(QMouseEvent *event) override {
    if(event->button()!=Qt::LeftButton||!map_rect().contains(event->position()))return;
    location_=unproject(event->position());update();if(changed)changed(location_);event->accept();
  }
public:
  static constexpr double latitude_limit=85.0511287798066;
  std::function<void(const std::array<double,2> &)> changed;
  explicit SolarLocationMap(std::array<double,2> location,QWidget *parent=nullptr):QWidget(parent),location_(location){setObjectName("solarLocationMap");setCursor(Qt::CrossCursor);setAccessibleName(QStringLiteral("墨卡托世界地图，经纬度选点"));}
  QRectF map_rect() const {return QRectF(rect()).adjusted(1,1,-1,-1);}
  QPointF project(double latitude,double longitude) const {
    const double phi=std::clamp(latitude,-latitude_limit,latitude_limit)*std::numbers::pi/180;
    const double lon=longitude>=-180&&longitude<=180?longitude:std::remainder(longitude,360.);
    const auto box=map_rect();return {box.left()+(lon+180)/360*box.width(),box.top()+(.5-std::asinh(std::tan(phi))/(2*std::numbers::pi))*box.height()};
  }
  std::array<double,2> unproject(QPointF point) const {
    const auto box=map_rect();const double u=std::clamp((point.x()-box.left())/box.width(),0.,1.),v=std::clamp((point.y()-box.top())/box.height(),0.,1.);
    return {std::atan(std::sinh(std::numbers::pi*(1-2*v)))*180/std::numbers::pi,u*360-180};
  }
  const std::array<double,2> &location() const{return location_;}
};

class SolarLocationButton final:public QToolButton {
  std::function<std::array<double,2>()> read_;
  std::function<void(const std::array<double,2> &)> write_;
  QPointer<QWidget> popup_;
  bool entered_=false;
  void cancel(){auto popup=popup_;popup_.clear();if(popup){popup->setObjectName({});popup->hide();popup->deleteLater();}}
  void leave_later(){QTimer::singleShot(100,this,[this]{if(!popup_)return;const auto p=QCursor::pos();if(popup_->frameGeometry().contains(p))return;if(!entered_&&QRect(mapToGlobal(QPoint()),size()).adjusted(-2,-2,2,2).contains(p))return;cancel();});}
  void show_map(){
    if(popup_)return;entered_=false;
    auto *popup=new QFrame(this,Qt::Tool|Qt::FramelessWindowHint);popup_=popup;popup->setObjectName("SolarLocationPopup");popup->setProperty("parameterSettingsPopup",true);popup->setAttribute(Qt::WA_DeleteOnClose);popup->setAutoFillBackground(true);popup->setFrameShape(QFrame::StyledPanel);
    auto *layout=new QVBoxLayout(popup);layout->setContentsMargins(10,8,10,8);
    layout->addWidget(new QLabel(QStringLiteral("地图选点 · 墨卡托投影")));
    auto *map=new SolarLocationMap(read_(),popup);const auto available=screen()->availableGeometry();
    const int side=std::min({460,std::max(120,available.width()-24),std::max(120,available.height()-155)});map->setFixedSize(side,side);layout->addWidget(map);
    auto *coordinates=new QLabel;coordinates->setObjectName("solarLocationDraft");layout->addWidget(coordinates);
    map->changed=[coordinates](const auto &v){const auto zone=ir::longitude_utc_offset(v[1]);coordinates->setText(QStringLiteral("纬度 %1°   经度 %2°   UTC%3%4").arg(v[0],0,'f',4).arg(v[1],0,'f',4).arg(zone>=0?"+":"").arg(zone,0,'f',1));};map->changed(map->location());
    auto *hint=new QLabel(QStringLiteral("点击地图选点，再点 OK；地图显示南北纬 85.05° 之间。"));hint->setWordWrap(true);layout->addWidget(hint);
    auto *ok=new QPushButton("OK");ok->setObjectName("SolarLocationOK");layout->addWidget(ok);
    connect(ok,&QPushButton::clicked,this,[this,map]{const auto value=map->location();const auto write=write_;cancel();write(value);});
    for(auto *child:popup->findChildren<QWidget *>())child->installEventFilter(this);
    popup->installEventFilter(this);popup->adjustSize();const auto top=mapToGlobal(QPoint());
    int x=std::clamp(top.x()+width()-popup->width(),available.left(),std::max(available.left(),available.right()-popup->width()+1));
    int y=top.y()+height()-1;if(y+popup->height()>available.bottom()+1)y=top.y()-popup->height()+1;
    y=std::clamp(y,available.top(),std::max(available.top(),available.bottom()-popup->height()+1));popup->move(x,y);popup->show();ok->setFocus();
  }
protected:
  void leaveEvent(QEvent *event) override {leave_later();QToolButton::leaveEvent(event);}
  void hideEvent(QHideEvent *event) override {cancel();QToolButton::hideEvent(event);}
  bool eventFilter(QObject *object,QEvent *event) override {
    if(popup_&&event->type()==QEvent::KeyPress&&static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape){cancel();return true;}
    if(object==popup_){if(event->type()==QEvent::Enter)entered_=true;if(event->type()==QEvent::Leave)leave_later();if(event->type()==QEvent::WindowDeactivate)cancel();}return false;
  }
public:
  SolarLocationButton(std::function<std::array<double,2>()> read,std::function<void(const std::array<double,2> &)> write,QWidget *parent=nullptr):QToolButton(parent),read_(std::move(read)),write_(std::move(write)){
    setObjectName("solarMapButton");setText("Map");setAutoRaise(true);setToolTip(QStringLiteral("在地图上同时选择经纬度，点 OK 提交；离开或按 Esc 取消。"));connect(this,&QToolButton::clicked,this,[this]{show_map();});
  }
  ~SolarLocationButton() override {cancel();}
};
}
