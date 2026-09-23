#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <mutex>
#include <string>
#include "bench/camera.h"

namespace dfv {
class Telemetry;
class GLContext {
  HDC dc_{};
  HGLRC context_{};
  std::recursive_mutex mutex_;
  unsigned depth_=0;
public:
  void initialize(HDC dc,HGLRC share=nullptr);
  void activate();
  void deactivate();
  void destroy();
  HGLRC handle() const {return context_;}
};
class Window {
  static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
  int last_x_=0,last_y_=0;
  bool dragging_=false;
  bool middle_dragging_=false,back_pressed_=false,back_dragging_=false,back_click_=false;
  int back_x_=0,back_y_=0;
  bool keys_[6]{};
  ULONGLONG moved_=0;
  Telemetry *telemetry_=nullptr;
  void update_navigation();
  bool inside(int x,int y) const;
  bool side_combo(WPARAM buttons) const;
  void release_navigation_capture();
public:
  HWND hwnd{},hidden{};
  HDC dc{},render_dc{};
  GLContext present_context,render_context;
  std::atomic<bool> close{false},minimized{false},size_changed{false};
  std::atomic<int> pointer_x{-1},pointer_y{-1},click_x{-1},click_y{-1};
  std::atomic<bool> click_toggle{false},pointer_toggle{false};
  std::atomic<uint64_t> clicks{0},focus_requests{0};
  CameraState camera;
  CameraMailbox mailbox;
  std::atomic<int> width,height;
  int monitor_index=2;
  std::string monitor_device;
  bool embedded=false;
  Window(int width,int height,bool fullscreen,Telemetry *telemetry=nullptr,int monitor=2,HWND parent=nullptr);
  ~Window();
  void poll();
  void publish();
};
}
