#pragma once
#include "runtime/powerpose.h"
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPixmap>
#include <functional>

namespace dfv::editor {
class PowerPoseCanvas;
class PowerPosePanel final:public QWidget {
  friend class PowerPoseCanvas;
  QComboBox *sets_,*pages_;
  QLabel *source_,*details_,*controls_[4];
  PowerPoseCanvas *canvas_;
  std::vector<runtime::BoundPosePoint> points_;
  std::vector<runtime::PosePin> pins_;
  std::vector<QString> descriptions_;
  QPixmap backgrounds_[3];
  uint64_t generation_=0,revision_=0,serial_=0;
  int skin_=-1,target_=-1,page_=0,selected_=-1;
  std::string instance_;
  bool male_=false,ready_=false;
  bool automated_input_=false;
  runtime::PowerPoseInput gesture_;
  QPointF start_;
  Qt::MouseButton button_=Qt::NoButton;
  void show_controls(int index);
  void change_page(int page);
  void press(QPointF pos,Qt::MouseButton button,Qt::KeyboardModifiers modifiers);
  void move(QPointF pos);
  void release(QPointF pos,Qt::MouseButton button);
  void context(int index,QPoint global);
  void emit_input();
  bool eventFilter(QObject *object,QEvent *event) override;
public:
  explicit PowerPosePanel(QWidget *parent=nullptr);
  std::function<void(const runtime::BoundPosePoint&)> selection;
  std::function<void(runtime::PowerPoseInput)> input;
  // 0 定位参数，1 恢复本点，2 固定位置，3 固定角度。
  std::function<void(const runtime::BoundPosePoint&,int,bool)> action;
  void bind(const runtime::Skin *skin,int index,int target,uint64_t generation,uint64_t revision,
    const std::vector<std::filesystem::path> &roots,bool ready,const std::vector<runtime::PosePin> &pins);
  void cancel();
  void automated_input() {automated_input_=true;}
  bool dragging() const {return button_!=Qt::NoButton&&gesture_.moved;}
  // 同一坐标转换供绘制、命中及交互测试使用。
  QPointF point_position(const std::string &id) const;
  QWidget *canvas() const;
  void page(int index) {pages_->setCurrentIndex(index);}
};
}
