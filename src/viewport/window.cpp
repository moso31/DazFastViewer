#include "viewport/window.h"
#include "bench/telemetry.h"
#include <windowsx.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <algorithm>

namespace dfv {
void GLContext::initialize(HDC dc,HGLRC share) {
  dc_=dc;context_=wglCreateContext(dc_);
  if(!context_ || (share && !wglShareLists(share,context_)))
    throw std::runtime_error("创建或共享 OpenGL context 失败");
}
void GLContext::activate() {
  mutex_.lock();
  if(depth_++==0 && !wglMakeCurrent(dc_,context_)) {
    --depth_;mutex_.unlock();throw std::runtime_error("激活 OpenGL context 失败");
  }
}
void GLContext::deactivate() {
  if(--depth_==0) wglMakeCurrent(nullptr,nullptr);
  mutex_.unlock();
}
void GLContext::destroy() {if(context_) {wglDeleteContext(context_);context_=nullptr;}}

static void pixel_format(HDC dc) {
  PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;
  pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
  pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cAlphaBits=8;
  const int format=ChoosePixelFormat(dc,&pfd);
  if(!format || !SetPixelFormat(dc,format,&pfd)) throw std::runtime_error("设置 OpenGL pixel format 失败");
}
struct Monitor {HMONITOR handle;MONITORINFOEXW info;};
static BOOL CALLBACK enumerate_monitors(HMONITOR monitor,HDC,LPRECT,LPARAM data) {
  Monitor row{};row.handle=monitor;row.info.cbSize=sizeof(row.info);
  if(GetMonitorInfoW(monitor,&row.info)) reinterpret_cast<std::vector<Monitor> *>(data)->push_back(row);
  return TRUE;
}
Window::Window(int w,int h,bool fullscreen,Telemetry *telemetry,int monitor,HWND parent):telemetry_(telemetry),width(w),height(h),monitor_index(monitor),embedded(parent!=nullptr) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  std::vector<Monitor> monitors;EnumDisplayMonitors(nullptr,nullptr,enumerate_monitors,reinterpret_cast<LPARAM>(&monitors));
  std::sort(monitors.begin(),monitors.end(),[](const Monitor &a,const Monitor &b) {
    const bool ap=a.info.dwFlags&MONITORINFOF_PRIMARY,bp=b.info.dwFlags&MONITORINFOF_PRIMARY;
    return ap!=bp?ap>bp:std::wstring(a.info.szDevice)<std::wstring(b.info.szDevice);
  });
  if(monitor<1 || size_t(monitor)>monitors.size()) throw std::runtime_error("请求的显示器不存在；不会回退到主屏");
  const auto &selected=monitors[monitor-1];const auto area=fullscreen?selected.info.rcMonitor:selected.info.rcWork;
  for(const wchar_t *c=selected.info.szDevice;*c;++c) monitor_device+=char(*c);
  WNDCLASSW cls{};cls.style=CS_OWNDC;cls.lpfnWndProc=procedure;
  cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"DfvCyclesBench";
  cls.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&cls);
  const DWORD style=parent?(WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN):(fullscreen?WS_POPUP:(WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX));
  RECT rect{0,0,w,h};AdjustWindowRect(&rect,style,FALSE);
  const int outer_width=rect.right-rect.left,outer_height=rect.bottom-rect.top;
  if(outer_width>area.right-area.left || outer_height>area.bottom-area.top) throw std::runtime_error("窗口尺寸超出目标显示器，请减小 --width/--height");
  const int left=area.left+(area.right-area.left-outer_width)/2,top=area.top+(area.bottom-area.top-outer_height)/2;
  hwnd=CreateWindowW(cls.lpszClassName,L"DazFastViewer - Cycles",style,parent?0:left,parent?0:top,
                     rect.right-rect.left,rect.bottom-rect.top,parent,nullptr,cls.hInstance,this);
  hidden=CreateWindowW(cls.lpszClassName,L"Cycles render context",WS_POPUP,area.left,area.top,1,1,nullptr,nullptr,cls.hInstance,nullptr);
  if(!hwnd || !hidden) throw std::runtime_error("创建窗口失败");
  if(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONULL)!=selected.handle) throw std::runtime_error("窗口未位于指定显示器，拒绝显示");
  dc=GetDC(hwnd);render_dc=GetDC(hidden);pixel_format(dc);pixel_format(render_dc);
  present_context.initialize(dc);render_context.initialize(render_dc,present_context.handle());
  ShowWindow(hwnd,SW_SHOWNOACTIVATE);
  SetWindowPos(hwnd,HWND_TOP,parent?0:left,parent?0:top,outer_width,outer_height,SWP_NOACTIVATE);
  std::cout<<"Window monitor "<<monitor<<" "<<monitor_device<<" at "<<left<<","<<top<<" client "<<w<<"x"<<h<<std::endl;
  publish();
}
Window::~Window() {
  render_context.destroy();present_context.destroy();
  if(hidden) {ReleaseDC(hidden,render_dc);DestroyWindow(hidden);}
  if(hwnd) {ReleaseDC(hwnd,dc);DestroyWindow(hwnd);}
}
void Window::publish() {
  ++camera.epoch;camera.input_seconds=now();
  if(telemetry_) {Frame frame;frame.epoch=camera.epoch;telemetry_->event("camera_input",frame);}
  mailbox.publish(camera);
}
void Window::poll() {
  MSG msg{};
  while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageW(&msg);}
}
LRESULT CALLBACK Window::procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
  auto *self=reinterpret_cast<Window *>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
  if(msg==WM_NCCREATE) {
    self=static_cast<Window *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
  }
  if(!self) return DefWindowProcW(hwnd,msg,w,l);
  switch(msg) {
    case WM_CLOSE:self->close=true;return 0;
    case WM_KEYDOWN:if(w==VK_ESCAPE && !self->embedded) self->close=true;return 0;
    case WM_RBUTTONDOWN:
      self->dragging_=true;self->last_x_=GET_X_LPARAM(l);self->last_y_=GET_Y_LPARAM(l);SetCapture(hwnd);return 0;
    case WM_RBUTTONUP:self->dragging_=false;ReleaseCapture();return 0;
    case WM_MOUSEMOVE:
      if(self->dragging_) {
        const int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
        if(w&MK_SHIFT) self->camera.pan(float(x-self->last_x_),float(y-self->last_y_));
        else self->camera.orbit(float(x-self->last_x_),float(y-self->last_y_));
        self->last_x_=x;self->last_y_=y;self->publish();
      }
      return 0;
    case WM_MOUSEWHEEL:self->camera.dolly(float(GET_WHEEL_DELTA_WPARAM(w))/WHEEL_DELTA);self->publish();return 0;
    case WM_SIZE:
      self->minimized=w==SIZE_MINIMIZED;
      if(w!=SIZE_MINIMIZED && (LOWORD(l)!=self->width || HIWORD(l)!=self->height)) self->size_changed=true;
      return 0;
  }
  return DefWindowProcW(hwnd,msg,w,l);
}
}
