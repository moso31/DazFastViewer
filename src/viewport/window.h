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
#include <functional>
#include <stdexcept>
#include "bench/camera.h"
#include "runtime/pose_edit.h"

namespace dfv {
class Telemetry;
class GraphicsError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
class GLContext {
  HDC dc_{};
  HGLRC context_{};
  std::recursive_mutex mutex_;
  unsigned depth_=0;
  DWORD owner_=0;
  Telemetry *telemetry_=nullptr;
  const char *name_="unknown";
  std::atomic<bool> failed_{false};
  mutable std::mutex error_mutex_;
  std::string error_;
  std::string failure(const char *operation,DWORD code);
#ifdef DFV_GL_RECOVERY_TEST
public:
  enum class Fault {none,activate_once,context_lost,persistent,create_once,release_once};
  void inject(Fault value) {fault_=value;}
private:
  std::atomic<Fault> fault_{Fault::none};
#endif
public:
  // 同一线程可嵌套使用；异常路径和正常路径都准确解除一次绑定。
  class Binding {
    GLContext *context_;
  public:
    explicit Binding(GLContext &context):context_(&context) {context_->activate();}
    Binding(const Binding &)=delete;
    Binding &operator=(const Binding &)=delete;
    ~Binding() {release();}
    void release() noexcept {if(context_) {context_->deactivate();context_=nullptr;}}
  };
  void diagnostics(Telemetry *telemetry,const char *name) {telemetry_=telemetry;name_=name;}
  void initialize(HDC dc,HGLRC share=nullptr);
  void activate();
  void deactivate() noexcept;
  bool destroy() noexcept;
  bool failed() const {return failed_.load();}
  std::string error() const {std::lock_guard lock(error_mutex_);return error_;}
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
  std::mutex pose_mutex_;
  runtime::PosePointer pose_pointer_;
  void cancel_pose();
public:
  std::atomic<uint64_t> pose_selection{0};
  std::function<void(bool)> history_requested;
  void cancel_edit() {cancel_pose();++pose_selection;}
  std::atomic<bool> automated_pointer{false}; // 仅诊断入口：不抢占用户系统光标。
  runtime::PosePointer pose_pointer() {std::lock_guard lock(pose_mutex_);return pose_pointer_;}
  HWND hwnd{},hidden{},prepare_hidden{};
  HDC dc{},render_dc{},prepare_dc{};
  GLContext present_context,render_context,prepare_context;
  std::atomic<bool> close{false},minimized{false},size_changed{false};
  std::atomic<int> pointer_x{-1},pointer_y{-1},click_x{-1},click_y{-1};
  std::atomic<bool> click_toggle{false},pointer_toggle{false};
  std::atomic<uint64_t> clicks{0},focus_requests{0},ground_requests{0};
  CameraState camera;
  CameraMailbox mailbox;
  std::atomic<int> width,height;
  int monitor_index=0;
  std::string monitor_device;
  bool embedded=false;
  // 0 selects the host's screen for embedded viewports, otherwise the primary screen.
  Window(int width,int height,bool fullscreen,Telemetry *telemetry=nullptr,int monitor=0,HWND parent=nullptr);
  ~Window();
  // 仅在呈现作用域退出、Cycles 会话及其工作线程全部销毁之后调用。
  void recreate_contexts();
  void check_graphics() const;
  void swap();
  void poll();
  void publish();
};
}
