#pragma once
#include <QLayout>
#include <QWidget>
#include <algorithm>
#include <vector>
namespace dfv::editor {
class FlowLayout final:public QLayout {
  std::vector<QLayoutItem *> items_;
  int arrange(const QRect &area,bool apply) const {
    const auto rect=area.marginsRemoved(contentsMargins());int y=rect.y();size_t first=0;
    while(first<items_.size()){
      size_t last=first;int width=0,height=0;
      do {const auto hint=items_[last]->sizeHint();const int w=std::min(rect.width(),hint.width());if(last>first&&width+spacing()+w>rect.width())break;width+=(last>first?spacing():0)+w;height=std::max(height,hint.height());++last;}while(last<items_.size());
      int x=rect.x(),extra=std::max(0,rect.width()-width);const int count=int(last-first);
      for(size_t i=first;i<last;++i){const int w=std::min(rect.width(),items_[i]->sizeHint().width())+extra/count+(int(i-first)<extra%count?1:0);if(apply)items_[i]->setGeometry({x,y,w,height});x+=w+spacing();}
      y+=height+spacing();first=last;
    }return y-area.y()-(items_.empty()?0:spacing())+contentsMargins().bottom();
  }
public:
  explicit FlowLayout(QWidget *parent):QLayout(parent){setContentsMargins(0,0,0,0);setSpacing(5);}
  ~FlowLayout(){while(auto *item=takeAt(0))delete item;}
  void addItem(QLayoutItem *item) override {items_.push_back(item);invalidate();}
  int count() const override {return int(items_.size());}
  QLayoutItem *itemAt(int i) const override {return i>=0&&i<count()?items_[size_t(i)]:nullptr;}
  QLayoutItem *takeAt(int i) override {if(i<0||i>=count())return nullptr;auto *item=items_[size_t(i)];items_.erase(items_.begin()+i);invalidate();return item;}
  Qt::Orientations expandingDirections() const override {return Qt::Horizontal;}
  bool hasHeightForWidth() const override {return true;}
  int heightForWidth(int width) const override {return arrange({0,0,std::max(1,width),0},false);}
  QSize minimumSize() const override {return {60,items_.empty()?0:items_.front()->minimumSize().height()};}
  QSize sizeHint() const override {return {300,heightForWidth(300)};}
  void setGeometry(const QRect &rect) override {QLayout::setGeometry(rect);arrange(rect,true);}
};
}
