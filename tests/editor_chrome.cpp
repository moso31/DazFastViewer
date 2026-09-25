#include "editor/chrome.h"
#include <QApplication>
#include <QTest>
#include <QToolButton>
#include <QMenu>
#include <QScreen>
#include <QMouseEvent>
#include <QLineEdit>
#include <windows.h>
#include <iostream>
using namespace dfv::editor;
static void check(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  try {
    const bool native=QGuiApplication::platformName()=="windows";
    EditorWindow window;window.resize(1000,600);window.install_chrome();auto *chrome=window.chrome;chrome->menus()->addMenu(QStringLiteral("文件"));chrome->menus()->addMenu(QStringLiteral("视图"));
    if(native) {QScreen *secondary=nullptr;for(auto *screen:QGuiApplication::screens()) if(screen!=QGuiApplication::primaryScreen()) secondary=screen;check(secondary,"原生窗口验证需要副屏");window.move(secondary->availableGeometry().topLeft()+QPoint(60,80));window.setAttribute(Qt::WA_ShowWithoutActivating);}
    window.show();QTest::qWait(80);
    // 后台启动测试进程时，消耗 STARTUPINFO 的隐藏状态，再显式显示副屏测试窗口。
    if(native) {ShowWindow(HWND(window.winId()),SW_SHOWNOACTIVATE);ShowWindow(HWND(window.winId()),SW_SHOWNOACTIVATE);QTest::qWait(80);}
    auto *menu=chrome->findChild<QToolBar *>("MenuModule"),*tools=chrome->findChild<QToolBar *>("TransformModule");check(menu&&tools,"模块没有独立身份");check(std::abs(menu->y()-tools->y())<3&&chrome->height()<50,"默认不是同一行");
    GizmoSettings last;int calls=0;chrome->changed=[&](auto settings){last=settings;++calls;};chrome->findChild<QAction *>("GizmoTool2")->trigger();check(calls==1&&last.tool==GizmoTool::rotate,"旋转按钮未接通");for(auto *action:tools->actions()) if(action->text()=="World") action->trigger();check(last.space==GizmoSpace::world,"World 没有切换");
    auto tool=[&](int i){chrome->findChild<QAction *>("GizmoTool"+QString::number(i))->trigger();};auto space=[&](const char *name){for(auto *a:tools->actions()) if(a->text()==name) a->trigger();};
    tool(1);check(last.space==GizmoSpace::local,"T 被 R 的世界模式污染");tool(3);tool(2);check(last.space==GizmoSpace::world,"R 切换回来丢失世界模式");
    space("Local");tool(1);space("World");tool(0);tool(2);check(last.space==GizmoSpace::local,"R 没有独立记住本地模式");tool(1);check(last.space==GizmoSpace::world,"T 没有独立记住世界模式");
    auto *functions=chrome->findChild<QToolBar *>("FunctionModule");auto *ratio=chrome->findChild<QDoubleSpinBox *>("GroundAlignmentRatio");check(functions&&ratio&&functions->x()>tools->x()&&functions->y()==tools->y(),"角色功能组没有默认位于 Local / World 右侧");
    check(!chrome->ground_action()->isEnabled()&&!ratio->isEnabled(),"没有角色时仍可执行对齐");double value=0;int ratio_calls=0;chrome->ground_ratio_changed=[&](double v){value=v;++ratio_calls;};chrome->bind_ground(true,.01);check(ratio->value()==.01&&ratio_calls==0,"绑定角色比例错误地写入参数");ratio->setValue(-.02);check(value==-.02&&ratio_calls==1,"地面对齐比例编辑未接通");chrome->bind_ground(true,0);chrome->bind_ground(true,.01);check(ratio->value()==.01&&ratio_calls==1,"切换角色比例引起额外编辑");
    check(chrome->ground_action()->shortcut()==QKeySequence(Qt::CTRL|Qt::Key_D)&&!chrome->ground_action()->icon().isNull(),"对齐图标或快捷键缺失");
    ratio->findChild<QLineEdit *>()->setText(ratio->prefix()+"0.035");chrome->ground_action()->trigger();check(value==.035,"点击对齐没有提交尚未回车的比例输入");
    functions->hide();auto function_state=chrome->save_modules();chrome->reset_modules();chrome->restore_modules(function_state);QTest::qWait(50);check(functions->isHidden(),"功能组显隐未保存");chrome->reset_modules();
    auto *host=chrome->findChild<QMainWindow *>("ToolbarModules");host->insertToolBarBreak(tools);QTest::qWait(100);check(tools->y()>menu->y()&&chrome->height()>50,"模块不能分行");auto saved=chrome->save_modules();chrome->reset_modules();QTest::qWait(50);check(tools->y()==menu->y(),"重置未合并行");check(chrome->restore_modules(saved),"模块布局未恢复");QTest::qWait(50);check(tools->y()>menu->y(),"模块顺序／换行未保存");
    check(chrome->findChild<QToolButton *>("ApplicationIcon")->y()==0&&chrome->findChild<QToolButton *>("WindowControl2")->y()==0,"分行后应用图标或窗口按钮离开顶部");
    tools->hide();saved=chrome->save_modules();chrome->reset_modules();chrome->restore_modules(saved);QTest::qWait(50);check(tools->isHidden()&&menu->isVisible(),"模块显隐未保存");chrome->reset_modules();
    check(chrome->findChild<QToolButton *>("ApplicationIcon")->x()<menu->mapTo(chrome,QPoint()).x(),"应用图标没有固定在左上角");
    // 通过实际鼠标事件拖出工具栏；状态序列化须保存浮动窗口，而非仅记下显隐。
    auto mouse=[&](QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons buttons) {QMouseEvent event(type,point,tools->mapToGlobal(point),button,buttons,{});QApplication::sendEvent(tools,&event);};
    QTest::qWait(60);mouse(QEvent::MouseButtonPress,{5,17},Qt::LeftButton,Qt::LeftButton);mouse(QEvent::MouseMove,{80,150},Qt::NoButton,Qt::LeftButton);mouse(QEvent::MouseButtonRelease,{80,150},Qt::LeftButton,{});QTest::qWait(60);check(tools->isFloating(),"拖出模块没有成为浮动工具栏");saved=chrome->save_modules();chrome->reset_modules();QTest::qWait(50);check(!tools->isFloating(),"重置没有重新停靠");chrome->restore_modules(saved);QTest::qWait(50);check(tools->isFloating(),"浮动状态未恢复");chrome->reset_modules();QTest::qWait(50);
    if(native) {
      const auto hwnd=HWND(window.winId());auto *grip=chrome->findChild<QWidget *>("WindowDragArea");const auto point=grip->mapTo(&window,grip->rect().center());POINT physical{qRound(point.x()*window.devicePixelRatioF()),qRound(point.y()*window.devicePixelRatioF())};ClientToScreen(hwnd,&physical);
      check(SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(physical.x,physical.y))==HTCAPTION,"顶部空白没有保留系统拖动");RECT rect;GetWindowRect(hwnd,&rect);check(SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(rect.left+1,rect.top+1))==HTTOPLEFT,"窗口边缘无法缩放");
      QTest::mouseClick(chrome->findChild<QToolButton *>("WindowControl1"),Qt::LeftButton);QTest::qWait(80);check(window.isMaximized(),"最大化按钮失败");MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&info);GetClientRect(hwnd,&rect);
      if(rect.right!=info.rcWork.right-info.rcWork.left||rect.bottom!=info.rcWork.bottom-info.rcWork.top) std::cerr<<"client="<<rect.right<<'x'<<rect.bottom<<" work="<<info.rcWork.right-info.rcWork.left<<'x'<<info.rcWork.bottom-info.rcWork.top<<" max="<<window.maximumWidth()<<'x'<<window.maximumHeight()<<" dpi="<<window.devicePixelRatioF()<<'\n';
      check(rect.right==info.rcWork.right-info.rcWork.left&&rect.bottom==info.rcWork.bottom-info.rcWork.top,"最大化覆盖任务栏或裁切客户区");
      QTest::mouseClick(chrome->findChild<QToolButton *>("WindowControl1"),Qt::LeftButton);QTest::qWait(80);check(!window.isMaximized(),"还原按钮失败");
      QTest::mouseClick(chrome->findChild<QToolButton *>("WindowControl0"),Qt::LeftButton);QTest::qWait(60);check(window.isMinimized(),"最小化按钮失败");window.showNormal();QTest::qWait(60);
      window.screen()->grabWindow(window.winId()).save(argc>1?QString::fromLocal8Bit(argv[1]):QStringLiteral("editor-chrome.png"));
      QTest::mouseClick(chrome->findChild<QToolButton *>("WindowControl2"),Qt::LeftButton);QTest::qWait(40);check(!window.isVisible(),"关闭按钮失败");
    }
    std::cout<<"editor chrome: actions, one row, split rows, persistence, visibility and fixed icon passed\n";return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
