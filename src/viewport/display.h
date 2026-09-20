#pragma once
#include "viewport/window.h"
#include "bench/telemetry.h"
#include "session/display_driver.h"
#include <epoxy/gl.h>
#include <array>

namespace dfv {
class Display final:public ccl::DisplayDriver {
  enum class State {idle,writing,ready,displaying,retiring};
  struct Slot {GLuint texture=0;GLsync fence=nullptr;State state=State::idle;Frame frame;};
  Window &window_;
  Telemetry &telemetry_;
  std::atomic<uint64_t> &epoch_;
  std::atomic<int> &samples_;
  std::array<Slot,3> slots_;
  std::mutex slots_mutex_;
  int writing_=-1,current_=-1;
  GLuint pbo_=0,program_=0,font_=0;
  GLsync upload_=nullptr;
  bool allow_readback_;
  Frame last_drawn_;
  uint64_t last_presented_=0;
  std::atomic<bool> failed_{false};
  std::string error_;
  void allocate();
  void make_program();
public:
  Display(Window &window,Telemetry &telemetry,std::atomic<uint64_t> &epoch,std::atomic<int> &samples,bool allow_readback);
  ~Display() override;
  bool update_begin(const Params &,int,int) override;
  void update_end() override;
  void next_tile_begin() override {}
  ccl::half4 *map_texture_buffer() override;
  void unmap_texture_buffer() override;
  void zero() override {}
  void draw(const Params &) override;
  ccl::GraphicsInteropDevice graphics_interop_get_device() override;
  void graphics_interop_update_buffer() override;
  void graphics_interop_activate() override {window_.render_context.activate();}
  void graphics_interop_deactivate() override {window_.render_context.deactivate();}
  void after_swap();
  Frame drawn_frame() const {return last_drawn_;}
  void hud(const std::string &text);
  void release_present_resources();
  bool failed() const {return failed_.load();}
  const std::string &error() const {return error_;}
};
}
