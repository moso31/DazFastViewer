#include "editor/powerpose_panel.h"
#include "powerpose_fixture.h"
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QMenu>
#include <QPointer>
#include <QTest>
#include <iostream>

using namespace dfv;
static void check(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
static void mouse(QWidget *widget,QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons held,Qt::KeyboardModifiers modifiers={}) {
  QMouseEvent event(type,point,widget->mapToGlobal(point.toPoint()),button,held,modifiers);QApplication::sendEvent(widget,&event);
}
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  try {
    auto skin=powerpose_fixture();editor::PowerPosePanel panel;panel.resize(390,860);panel.show();app.processEvents();
    std::vector<runtime::PowerPoseInput> inputs;std::vector<std::string> selections;int operations=0;
    panel.input=[&](auto input){inputs.push_back(input);};panel.selection=[&](const auto &p){selections.push_back(p.point->id);};panel.action=[&](const auto &,int,bool){++operations;};
    auto bind=[&](bool ready=true,uint64_t revision=1){panel.bind(&skin,0,0,1,revision,{},ready,{});};bind();auto *canvas=panel.canvas();
    auto event=[&](QEvent::Type type,QPointF p,Qt::MouseButton b,Qt::MouseButtons held,Qt::KeyboardModifiers m=Qt::NoModifier){mouse(canvas,type,p,b,held,m);};
    auto head=panel.point_position("B01");
    event(QEvent::MouseButtonPress,head,Qt::LeftButton,Qt::LeftButton);event(QEvent::MouseMove,head+QPointF(1,1),Qt::NoButton,Qt::LeftButton);event(QEvent::MouseButtonRelease,head+QPointF(1,1),Qt::LeftButton,{});
    check(selections.back()=="B01"&&!inputs.back().moved&&!inputs.back().held,"轻点没有仅选择");
    size_t cases=0;
    for(int page=0;page<3;++page) {panel.page(page);bind();
      for(const auto &point:runtime::powerpose_templates()[page].points) if(point.kind!=runtime::PosePointKind::navigation) {
        const auto p=panel.point_position(point.id);
        for(bool right:{false,true}) for(const QPointF delta:{QPointF(0,-24),QPointF(0,24),QPointF(-24,0),QPointF(24,0)}) {
          const auto button=right?Qt::RightButton:Qt::LeftButton;
          event(QEvent::MouseButtonPress,p,button,button);event(QEvent::MouseMove,p+delta,Qt::NoButton,button);event(QEvent::MouseButtonRelease,p+delta,button,{});
          const auto &last=inputs.back();if(!(last.point==point.id&&last.moved&&!last.held&&!last.cancelled&&last.right==right&&std::abs(last.dx-delta.x())<.001&&std::abs(last.dy-delta.y())<.001)) throw std::runtime_error("控制点命中或方向手势错误："+point.id+" / "+last.point+" moved="+std::to_string(last.moved)+" cancelled="+std::to_string(last.cancelled)+" dx="+std::to_string(last.dx)+" dy="+std::to_string(last.dy)+" expected="+std::to_string(delta.x())+","+std::to_string(delta.y())+" right="+std::to_string(last.right)+" expectedRight="+std::to_string(right)+" held="+std::to_string(last.held)+" x="+std::to_string(p.x())+" y="+std::to_string(p.y()));++cases;
          check(panel.findChildren<QMenu *>().empty(),"右键拖动误弹菜单");
        }
      }
    }check(cases==680,"没有遍历 680 个手势");
    panel.page(0);bind();
    event(QEvent::MouseButtonPress,head,Qt::RightButton,Qt::RightButton,Qt::ShiftModifier);event(QEvent::MouseButtonPress,head,Qt::LeftButton,Qt::LeftButton|Qt::RightButton);event(QEvent::MouseMove,head+QPointF(20,30),Qt::NoButton,Qt::RightButton|Qt::LeftButton);
    check(inputs.back().right&&inputs.back().precision==.1,"第二键或移动改变了原始模式");
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,{});QApplication::sendEvent(canvas,&escape);check(inputs.back().cancelled&&!inputs.back().held,"Esc 没有取消");
    event(QEvent::MouseButtonRelease,head,Qt::RightButton,{});
    event(QEvent::MouseButtonPress,head,Qt::LeftButton,Qt::LeftButton);event(QEvent::MouseMove,head+QPointF(20,20),Qt::NoButton,Qt::LeftButton);bind(true,2);check(inputs.back().cancelled,"版本变化没有取消");
    bind(false);const auto count=inputs.size();event(QEvent::MouseButtonPress,head,Qt::LeftButton,Qt::LeftButton);check(inputs.size()==count,"未就绪仍开始拖动");bind();
    event(QEvent::MouseButtonPress,head,Qt::LeftButton,Qt::LeftButton);QFocusEvent focus(QEvent::FocusOut);QApplication::sendEvent(canvas,&focus);check(inputs.back().cancelled,"失焦没有取消");
    const auto hands=panel.point_position("BT01");event(QEvent::MouseButtonPress,hands,Qt::LeftButton,Qt::LeftButton);event(QEvent::MouseButtonRelease,hands,Qt::LeftButton,{});check(panel.findChild<QComboBox*>("powerpose.template")->currentText()=="Hands","导航没有切页");bind();
    panel.page(2);bind();const auto face=panel.point_position("NT01");const auto before=inputs.size();event(QEvent::MouseButtonPress,face,Qt::LeftButton,Qt::LeftButton);event(QEvent::MouseButtonRelease,face,Qt::LeftButton,{});check(inputs.size()==before&&panel.findChild<QComboBox*>("powerpose.template")->count()==3,"Face 被启用");
    panel.page(0);bind();event(QEvent::MouseButtonPress,head,Qt::RightButton,Qt::RightButton);event(QEvent::MouseButtonRelease,head,Qt::RightButton,{});app.processEvents();auto menus=panel.findChildren<QMenu*>();check(menus.size()==1&&menus.front()->actions().size()==4,"右键点菜单不完整");QPointer<QMenu> first_menu=menus.front();menus.front()->actions()[1]->trigger();check(operations==1,"恢复动作没有触发");if(first_menu) first_menu->close();app.processEvents();
    // 已弹出的菜单不能应用到另一个角色。
    head=panel.point_position("B01");
    event(QEvent::MouseButtonPress,head,Qt::RightButton,Qt::RightButton);event(QEvent::MouseButtonRelease,head,Qt::RightButton,{});auto second_menus=panel.findChildren<QMenu*>();check(!second_menus.empty(),"第二次右键未命中控制点");QPointer<QMenu> menu=second_menus.back();skin.id="figure-B";panel.bind(&skin,1,1,1,1,{},true,{});menu->actions()[1]->trigger();check(operations==1,"旧菜单跨角色提交");if(menu) menu->close();
    std::cout<<"PowerPose Qt: 680 directions, click, navigation, menu, Shift, cancel, identity: PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
