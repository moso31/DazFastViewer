#pragma once
#include "viewport/window.h"
#include "bench/telemetry.h"
#include "session/display_driver.h"
#include <epoxy/gl.h>
#include <array>
#include <optional>
#include "render_ir/options.h"
#include "viewport/quality.h"

namespace dfv {
class Display final:public ccl::DisplayDriver {
  enum class State {idle,writing,ready,displaying,retiring};
  struct Slot {GLuint texture=0;int width=0,height=0;GLsync fence=nullptr;State state=State::idle;Frame frame;};
  Window &window_;
  Telemetry &telemetry_;
  std::atomic<uint64_t> &epoch_;
  std::atomic<int> &samples_;
  std::array<Slot,3> slots_;
  std::mutex slots_mutex_;
  int writing_=-1,current_=-1;
  GLuint pbo_=0,program_=0,font_=0;
  GLuint retired_pbo_=0;
  size_t pbo_bytes_=0;
  bool interop_changed_=false;
  GLsync upload_=nullptr;
  bool allow_readback_;
  ir::RenderOptions options_;
  Reconstruction reconstruction_=Reconstruction::bicubic;
  float sharpen_=0;
  Frame last_drawn_;
  uint64_t last_presented_=0;
  std::atomic<bool> failed_{false};
  mutable std::mutex error_mutex_;
  std::string error_;
  std::optional<GLContext::Binding> update_binding_,interop_binding_;
  void fail(const std::string &message) noexcept;
  void finish_update() noexcept;
  void allocate(int width,int height);
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
  void graphics_interop_activate() override;
  void graphics_interop_deactivate() override {interop_binding_.reset();}
  void set_options(const ir::RenderOptions &options) {options_=options;}
  void set_reconstruction(Reconstruction value) {reconstruction_=value;}
  void set_sharpen(float value) {sharpen_=value>=0?std::min(value,max_viewport_sharpen):0.f;}
  void after_swap();
  Frame drawn_frame() const {return last_drawn_;}
  void hud(const std::string &text);
  void release_present_resources();
  bool failed() const {return failed_.load();}
  std::string error() const {std::lock_guard lock(error_mutex_);return error_;}
};
}
