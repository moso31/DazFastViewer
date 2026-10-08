#include "editor/window_placement.h"
#include "editor/dialog_layout.h"
#include "viewport/window.h"
#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QTemporaryDir>
#include <array>
#include <iostream>
#include <stdexcept>
#include <GL/gl.h>

static void check(bool value,const char *message) {if(!value) throw std::runtime_error(message);}

int main(int argc,char **argv) {
  QApplication app(argc,argv);
  using dfv::editor::visible_window_geometry;
  try {
    const QRect primary(0,0,1366,728),left(-1920,0,1920,1040),scaled(1366,-100,1280,680);
    check(primary.contains(visible_window_geometry({0,0,1580,920},{primary},primary)),"Single-screen window exceeds work area");
    const QRect saved(-1700,80,1000,700);
    check(visible_window_geometry(saved,{primary,left},primary)==saved,"Existing secondary-screen placement was lost");
    check(primary.contains(visible_window_geometry(saved,{primary},primary)),"Disconnected screen left window invisible");
    check(scaled.contains(visible_window_geometry({1400,-60,1580,920},{primary,scaled},primary)),"Mixed-DPI screen size not respected");
    check(primary.contains(visible_window_geometry({1300,700,400,300},{primary},primary)),"Partially hidden window was not clamped");
    check(primary.contains(visible_window_geometry({6000,4000,300,200},{primary},primary)),"Floating dock outside all screens was not recovered");
    QTemporaryDir preferences;dfv::editor::DialogLayouts dialogs(preferences.filePath("layout.ini"));
    QSize normal;QPoint position;
    {QDialog dialog;dialog.setObjectName("LayoutTest");dialog.resize(480,360);dialog.move(app.primaryScreen()->availableGeometry().topLeft()+QPoint(80,90));dialog.show();app.processEvents();normal=dialog.size();position=dialog.pos();dialog.reject();}
    {QDialog dialog;dialog.setObjectName("LayoutTest");dialog.resize(200,100);dialog.show();app.processEvents();check(dialog.size()==normal&&dialog.pos()==position,"取消后重开对话框没有继承尺寸和位置");dialog.showMaximized();app.processEvents();check(dialog.isMaximized(),"测试窗口没有最大化");dialog.reject();}
    {QDialog dialog;dialog.setObjectName("LayoutTest");dialog.show();app.processEvents();check(dialog.isMaximized(),"重建对话框没有继承最大化布局");dialog.showNormal();app.processEvents();check(dialog.size()==normal,"最大化还原丢失普通窗口尺寸");dialog.reject();}
    if(app.arguments().contains("--native")) {
      // Test real WGL creation without requiring CUDA/OptiX or a second monitor.
      QWidget host;host.setAttribute(Qt::WA_ShowWithoutActivating);host.resize(320,240);
      host.move(app.primaryScreen()->availableGeometry().topLeft()+QPoint(50,50));host.show();app.processEvents();
      const auto parent=reinterpret_cast<HWND>(host.winId());
      dfv::Window viewport(320,240,false,nullptr,0,parent);
      SetFocus(viewport.hwnd);
      auto key=[&](UINT message,WPARAM value){SendMessageW(viewport.hwnd,message,value,0);};
      auto tick=[&]{Sleep(20);SendMessageW(viewport.hwnd,WM_TIMER,1,0);};
      for(char movement:std::string("WASDQE")){const auto before=viewport.camera.matrix();key(WM_KEYDOWN,VK_CONTROL);key(WM_KEYDOWN,movement);tick();check(viewport.camera.matrix()==before,"Ctrl + WASDQE 移动了镜头");key(WM_KEYUP,movement);key(WM_KEYUP,VK_CONTROL);tick();check(viewport.camera.matrix()==before,"释放 Ctrl 后残留组合键移动");}
      key(WM_KEYDOWN,'W');tick();check(viewport.camera.navigating,"普通 W 没有进入镜头移动");const auto moved=viewport.camera.matrix();key(WM_KEYDOWN,VK_CONTROL);tick();check(viewport.camera.matrix()==moved&&!viewport.camera.navigating,"移动中按 Ctrl 没有停止镜头");key(WM_KEYUP,VK_CONTROL);key(WM_KEYUP,'W');
      int saves=0;viewport.save_requested=[&]{++saves;};const auto before_save=viewport.camera.matrix();
      key(WM_KEYDOWN,VK_CONTROL);key(WM_KEYDOWN,'S');SendMessageW(viewport.hwnd,WM_KEYDOWN,'S',1LL<<30);key(WM_KEYUP,'S');key(WM_KEYUP,VK_CONTROL);tick();
      check(saves==1&&viewport.camera.matrix()==before_save,"原生 Ctrl+S 没有触发一次保存，或移动了镜头");
      key(WM_KEYDOWN,'S');key(WM_KEYUP,'S');check(saves==1,"普通 S 错误触发保存");
      std::array<BYTE,256> original{},modifiers{};GetKeyboardState(original.data());modifiers=original;modifiers[VK_CONTROL]=0x80;modifiers[VK_SHIFT]=modifiers[VK_MENU]=0;SetKeyboardState(modifiers.data());key(WM_KEYDOWN,'S');key(WM_KEYUP,'S');SetKeyboardState(original.data());check(saves==2,"Ctrl 在视口获得焦点前按下时无法保存");
      for(auto modifier:{VK_SHIFT,VK_MENU}){modifiers[modifier]=0x80;SetKeyboardState(modifiers.data());key(WM_KEYDOWN,'S');key(WM_KEYUP,'S');SetKeyboardState(original.data());modifiers[modifier]=0;check(saves==2,"Ctrl+Shift/Alt+S 错误触发普通保存");}
      std::cout<<"Native Ctrl+S save / autorepeat / modifiers / camera: PASS\n";
      check(GetParent(viewport.hwnd)==parent,"Viewport is not embedded");
      check(MonitorFromWindow(viewport.hwnd,MONITOR_DEFAULTTONEAREST)==MonitorFromWindow(parent,MONITOR_DEFAULTTONEAREST),"Viewport does not follow its host monitor");
      {
        dfv::GLContext::Binding binding(viewport.render_context);
        check(wglGetCurrentContext()!=nullptr,"OpenGL context is missing");
        std::cout<<"OpenGL renderer: "<<glGetString(GL_RENDERER)<<'\n';
        if(app.arguments().contains("--cuda")) {
          const auto cuda=LoadLibraryExW(L"nvcuda.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
          check(cuda!=nullptr,"NVIDIA driver library is missing");
          const auto initialize=reinterpret_cast<int(WINAPI *)(unsigned)>(GetProcAddress(cuda,"cuInit"));
          const auto query=reinterpret_cast<int(WINAPI *)(unsigned *,int *,unsigned,unsigned)>(GetProcAddress(cuda,"cuGLGetDevices_v2"));
          check(initialize&&query&&initialize(0)==0,"CUDA driver initialization failed");
          int devices[32]{};unsigned count=0;
          check(query(&count,devices,32,1)==0&&count>0,"Viewport has no matching CUDA device; select the NVIDIA GPU in Windows graphics settings");
          std::cout<<"OpenGL/CUDA matching devices: "<<count<<", first ordinal: "<<devices[0]<<'\n';
          FreeLibrary(cuda);
        }
      }
      check(wglGetCurrentContext()==nullptr,"OpenGL context was not released");
      viewport.recreate_contexts();
      // Use physical Win32 pixels to exercise oversized children even at 150% DPI.
      MONITORINFO info{};info.cbSize=sizeof(info);
      check(GetMonitorInfoW(MonitorFromWindow(parent,MONITOR_DEFAULTTONEAREST),&info),"Monitor work area is missing");
      dfv::Window oversized(info.rcWork.right-info.rcWork.left+100,info.rcWork.bottom-info.rcWork.top+100,false,nullptr,0,parent);
      check(GetParent(oversized.hwnd)==parent,"Oversized viewport is not clipped by its host");
      std::cout<<"Native viewport and OpenGL contexts: PASS\n";
    }
    std::cout<<"Window placement: PASS\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
