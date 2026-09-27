#pragma once
#include <QGuiApplication>
#include <QIcon>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>

namespace dfv::editor {
// 与顶部工具栏共用 24 单位画布、圆角线帽和 1.6 单位线宽。
class FolderIconEngine final:public QIconEngine {
public:
  QIconEngine *clone() const override {return new FolderIconEngine;}
  void paint(QPainter *painter,const QRect &rect,QIcon::Mode mode,QIcon::State) override {
    const auto palette=QGuiApplication::palette();
    const auto ink=palette.color(mode==QIcon::Disabled?QPalette::Disabled:QPalette::Active,QPalette::ButtonText);
    const qreal scale=qMin(rect.width(),rect.height())/24.;
    painter->save();painter->setRenderHint(QPainter::Antialiasing);
    painter->translate(rect.x()+(rect.width()-24*scale)/2,rect.y()+(rect.height()-24*scale)/2);painter->scale(scale,scale);
    painter->setPen(QPen(ink,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter->setBrush(Qt::NoBrush);
    painter->drawPolygon(QPolygonF{{3,5},{9,5},{11,8},{21,8},{21,20},{3,20}});painter->restore();
  }
  QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override {
    QPixmap image(size);image.fill(Qt::transparent);QPainter painter(&image);paint(&painter,QRect(QPoint{},size),mode,state);return image;
  }
};
inline QIcon folder_icon(){return QIcon(new FolderIconEngine);}
}
