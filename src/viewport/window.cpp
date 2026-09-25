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
  pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cAlphaBits=8;pfd.cDepthBits=24;
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
  SetTimer(hwnd,1,16,nullptr);moved_=GetTickCount64();publish();
}
Window::~Window() {
  render_context.destroy();present_context.destroy();
  if(hidden) {ReleaseDC(hidden,render_dc);DestroyWindow(hidden);}
  if(hwnd) {ReleaseDC(hwnd,dc);DestroyWindow(hwnd);}
}
void Window::publish() {
  ++camera.epoch;camera.input_seconds=now();
  if(!camera.navigating) camera.preview_until=camera.input_seconds+.15;
  if(telemetry_) {Frame frame;frame.epoch=camera.epoch;telemetry_->event("camera_input",frame);}
  mailbox.publish(camera);
}
void Window::update_navigation() {
  camera.navigating=dragging_||middle_dragging_||back_dragging_||std::any_of(std::begin(keys_),std::end(keys_),[](bool k){return k;});
  mailbox.publish(camera);
}
bool Window::inside(int x,int y) const {
  RECT rect{};GetClientRect(hwnd,&rect);return PtInRect(&rect,POINT{x,y})!=0;
}
bool Window::side_combo(WPARAM buttons) const {
  if((LOWORD(buttons)&~MK_XBUTTON1)||dragging_||middle_dragging_||std::any_of(std::begin(keys_),std::end(keys_),[](bool k){return k;})) return true;
  for(int key=VK_BACK;key<256;++key) if(GetKeyState(key)&0x8000) return true;
  return false;
}
void Window::release_navigation_capture() {
  update_navigation();
  if(!dragging_&&!middle_dragging_&&!back_pressed_&&GetCapture()==hwnd) ReleaseCapture();
}
void Window::cancel_pose() {std::lock_guard lock(pose_mutex_);if(pose_pointer_.held) {pose_pointer_.held=false;pose_pointer_.cancelled=true;++pose_pointer_.revision;}}
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
    case WM_LBUTTONDOWN: {
      self->back_click_=false;if(!self->automated_pointer) {SetFocus(hwnd);SetCapture(hwnd);}
      std::lock_guard lock(self->pose_mutex_);auto &p=self->pose_pointer_;++p.serial;++p.revision;p.selection=self->pose_selection;
      p.start_x=p.x=GET_X_LPARAM(l);p.start_y=p.y=GET_Y_LPARAM(l);p.held=true;p.moved=p.cancelled=false;
      p.input_seconds=now();
      p.modified=(w&(MK_CONTROL|MK_SHIFT|MK_RBUTTON|MK_MBUTTON))||(GetKeyState(VK_MENU)&0x8000);return 0;
    }
    case WM_LBUTTONUP: {
      bool cancelled;{std::lock_guard lock(self->pose_mutex_);auto &p=self->pose_pointer_;p.x=GET_X_LPARAM(l);p.y=GET_Y_LPARAM(l);p.held=false;cancelled=p.cancelled;++p.revision;}
      self->click_x=GET_X_LPARAM(l);self->click_y=GET_Y_LPARAM(l);self->pointer_toggle=self->click_toggle=(w&MK_CONTROL)!=0;if(!cancelled) ++self->clicks;
      if(GetCapture()==hwnd) ReleaseCapture();return 0;
    }
    case WM_MOUSELEAVE:if(!self->automated_pointer) {self->back_click_=false;self->pointer_x=-1;self->pointer_y=-1;}return 0;
    case WM_CLOSE:self->close=true;return 0;
    case WM_KEYDOWN:
      if(w=='D'&&self->pointer_toggle&&!(GetKeyState(VK_SHIFT)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)) {
        self->cancel_pose();self->keys_[3]=false;self->update_navigation();if(!(l&(1LL<<30))) ++self->ground_requests;return 0;
      }
      if(w==VK_ESCAPE) self->cancel_pose();
      if(w==VK_CONTROL||w==VK_SHIFT||w==VK_MENU) self->cancel_pose();
      self->back_click_=false;
      if(w==VK_CONTROL) self->pointer_toggle=true;
      if(w==VK_ESCAPE&&!self->embedded) self->close=true;
      if(w=='F'&&!self->back_pressed_&&!(l&(1LL<<30))) ++self->focus_requests;
      for(int i=0;i<6;++i) if(w=="WASDQE"[i]) self->keys_[i]=true;
      self->update_navigation();return 0;
    case WM_KEYUP:
      if(w==VK_CONTROL) self->pointer_toggle=false;
      for(int i=0;i<6;++i) if(w=="WASDQE"[i]) self->keys_[i]=false;
      self->update_navigation();return 0;
    case WM_SYSKEYDOWN:self->cancel_pose();self->back_click_=false;break;
    case WM_MOUSEHWHEEL:self->back_click_=false;return 0;
    case WM_SETFOCUS:self->pointer_toggle=(GetKeyState(VK_CONTROL)&0x8000)!=0;return 0;
    case WM_KILLFOCUS:
    case WM_CANCELMODE:
      self->cancel_pose();
      if(self->telemetry_) self->telemetry_->event(msg==WM_KILLFOCUS?"viewport_focus_lost":"viewport_cancel_input");
      std::fill(std::begin(self->keys_),std::end(self->keys_),false);
      self->pointer_toggle=false;
      self->dragging_=self->middle_dragging_=self->back_pressed_=self->back_dragging_=self->back_click_=false;
      self->camera.preview_until=0;self->update_navigation();
      if(GetCapture()==hwnd) ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:
      self->cancel_pose();
      if(self->telemetry_) self->telemetry_->event("viewport_capture_changed");
      self->dragging_=self->middle_dragging_=self->back_pressed_=self->back_dragging_=self->back_click_=false;
      self->update_navigation();return 0;
    case WM_TIMER: {
      const auto now=GetTickCount64();const float seconds=std::min(float(now-self->moved_)*.001f,.1f);self->moved_=now;
      if(GetFocus()==hwnd&&std::any_of(std::begin(self->keys_),std::end(self->keys_),[](bool k){return k;})) {
        self->camera.move(float(self->keys_[0])-self->keys_[2],float(self->keys_[3])-self->keys_[1],float(self->keys_[5])-self->keys_[4],seconds,(GetKeyState(VK_SHIFT)&0x8000)!=0);self->publish();
      }return 0;
    }
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
      self->cancel_pose();
      self->back_click_=false;
      if(!self->inside(GET_X_LPARAM(l),GET_Y_LPARAM(l))) return 0;
      SetFocus(hwnd);if(msg==WM_RBUTTONDOWN) self->dragging_=true;else self->middle_dragging_=true;
      self->update_navigation();self->last_x_=GET_X_LPARAM(l);self->last_y_=GET_Y_LPARAM(l);SetCapture(hwnd);return 0;
    case WM_RBUTTONUP:self->dragging_=false;self->release_navigation_capture();return 0;
    case WM_MBUTTONUP:self->middle_dragging_=false;self->release_navigation_capture();return 0;
    case WM_XBUTTONDOWN:
      if(GET_XBUTTON_WPARAM(w)!=XBUTTON1) {self->back_click_=false;return TRUE;}
      if(!self->inside(GET_X_LPARAM(l),GET_Y_LPARAM(l))) return TRUE;
      SetFocus(hwnd);self->back_pressed_=true;self->back_dragging_=false;self->back_click_=!self->side_combo(w);
      if(self->telemetry_) self->telemetry_->event(self->back_click_?"side_press_candidate":"side_press_combination");
      self->back_x_=self->last_x_=GET_X_LPARAM(l);self->back_y_=self->last_y_=GET_Y_LPARAM(l);
      SetCapture(hwnd);return TRUE;
    case WM_XBUTTONUP: {
      if(GET_XBUTTON_WPARAM(w)!=XBUTTON1) return TRUE;
      const int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
      if(self->telemetry_) self->telemetry_->event(!self->back_pressed_?"side_release_without_press":self->back_dragging_?"side_release_drag":!self->back_click_?"side_release_cancelled":self->side_combo(w)?"side_release_combination":!self->inside(x,y)?"side_release_outside":"side_release_click_candidate");
      const bool click=self->back_pressed_&&self->back_click_&&!self->back_dragging_&&!self->side_combo(w)&&self->inside(x,y)&&
        std::abs(x-self->back_x_)<GetSystemMetrics(SM_CXDRAG)&&std::abs(y-self->back_y_)<GetSystemMetrics(SM_CYDRAG);
      self->back_pressed_=self->back_dragging_=self->back_click_=false;self->release_navigation_capture();
      if(click) ++self->focus_requests;return TRUE;
    }
    case WM_MOUSEMOVE:
      {std::lock_guard lock(self->pose_mutex_);auto &p=self->pose_pointer_;if(p.held) {
        p.x=GET_X_LPARAM(l);p.y=GET_Y_LPARAM(l);p.input_seconds=now();++p.revision;
        p.moved|=std::abs(p.x-p.start_x)>=GetSystemMetrics(SM_CXDRAG)||std::abs(p.y-p.start_y)>=GetSystemMetrics(SM_CYDRAG);
      }}
      self->pointer_x=GET_X_LPARAM(l);self->pointer_y=GET_Y_LPARAM(l);
      self->pointer_toggle=(w&MK_CONTROL)!=0;
      {TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT),TME_LEAVE,hwnd,0};TrackMouseEvent(&track);}
      if(self->dragging_||self->middle_dragging_||self->back_pressed_) {
        const int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
        if(!self->inside(x,y)) {self->back_click_=false;self->last_x_=x;self->last_y_=y;return 0;}
        if(self->back_pressed_) {
          if(self->side_combo(w)) {if(self->back_click_&&self->telemetry_) self->telemetry_->event("side_move_combination");self->back_click_=false;}
          if(!self->back_dragging_&&(std::abs(x-self->back_x_)>=GetSystemMetrics(SM_CXDRAG)||std::abs(y-self->back_y_)>=GetSystemMetrics(SM_CYDRAG))) {
            self->back_dragging_=true;self->back_click_=false;self->update_navigation();
          }
        }
        const float dx=float(x-self->last_x_),dy=float(y-self->last_y_);
        if(self->back_dragging_||(self->dragging_&&(w&MK_CONTROL))) self->camera.look(dx,dy);
        else if(self->middle_dragging_||(self->dragging_&&(w&MK_SHIFT))) self->camera.pan(dx,dy);
        else if(self->dragging_) self->camera.orbit(dx,dy);
        else return 0;
        self->last_x_=x;self->last_y_=y;if(dx||dy) self->publish();
      }
      return 0;
    case WM_MOUSEWHEEL: {
      self->back_click_=false;
      POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(hwnd,&p);
      if(self->inside(p.x,p.y)) {self->camera.dolly(float(GET_WHEEL_DELTA_WPARAM(w))/WHEEL_DELTA);self->publish();}return 0;
    }
    case WM_SIZE:
      self->minimized=w==SIZE_MINIMIZED;
      if(w!=SIZE_MINIMIZED && (LOWORD(l)!=self->width || HIWORD(l)!=self->height)) self->size_changed=true;
      return 0;
  }
  return DefWindowProcW(hwnd,msg,w,l);
}
}
