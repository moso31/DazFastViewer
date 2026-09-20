#include "viewport/display.h"
#include "bench/fixtures.h"
#include "cycles/adapter.h"
#include "daz/loader.h"
#include "device/device.h"
#include "scene/scene.h"
#include "scene/integrator.h"
#include "scene/pass.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/mesh.h"
#include "scene/attribute.h"
#include "session/session.h"
#include "bench/output.h"
#include "util/path.h"
#include <epoxy/wgl.h>
#include <OpenImageIO/imageio.h>
#include <OpenColorIO/OpenColorIO.h>
#include <psapi.h>
#include <mmsystem.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace {
struct Options {
  bool devices=false,smoke=false,benchmark=false,medium=true,readback=false,inspect=false,strict=false,fullscreen=false,help=false,dump_shaders=false;
  int width=1600,height=900,samples=256,render_delay_ms=0,monitor=2;
  double seconds=60,warmup=10,refine=10,preview_seconds=0;
  std::string backend="OPTIX";
  std::filesystem::path output="artifacts/run";
  std::filesystem::path file;
  std::vector<std::filesystem::path> content_roots;
};
Options parse(int argc,char **argv) {
  Options o;
  for(int i=1;i<argc;++i) {
    const std::string arg=argv[i];
    auto value=[&]() {if(++i>=argc) throw std::runtime_error("参数缺少值: "+arg);return std::string(argv[i]);};
    if(arg=="--devices") o.devices=true;
    else if(arg=="--help") o.help=true;
    else if(arg=="--file") o.file=std::filesystem::u8path(value());
    else if(arg=="--content-root") o.content_roots.push_back(std::filesystem::u8path(value()));
    else if(arg=="--inspect") o.inspect=true;
    else if(arg=="--dump-shaders") o.dump_shaders=true;
    else if(arg=="--strict-dson") o.strict=true;
    else if(arg=="--monitor") o.monitor=std::stoi(value());
    else if(arg=="--fullscreen") o.fullscreen=true;
    else if(arg=="--preview-seconds") o.preview_seconds=std::stod(value());
    else if(arg=="--smoke") {o.smoke=true;o.medium=false;o.width=640;o.height=360;o.samples=16;}
    else if(arg=="--benchmark") o.benchmark=true;
    else if(arg=="--device") o.backend=value();
    else if(arg=="--scene") {const auto name=value();if(name!="medium" && name!="smoke") throw std::runtime_error("未知场景");o.medium=name=="medium";}
    else if(arg=="--width") o.width=std::stoi(value());
    else if(arg=="--height") o.height=std::stoi(value());
    else if(arg=="--samples") o.samples=std::stoi(value());
    else if(arg=="--seconds") o.seconds=std::stod(value());
    else if(arg=="--warmup") o.warmup=std::stod(value());
    else if(arg=="--refine") o.refine=std::stod(value());
    else if(arg=="--output") o.output=std::filesystem::u8path(value());
    else if(arg=="--allow-readback") o.readback=true;
    else if(arg=="--simulate-render-delay-ms") o.render_delay_ms=std::stoi(value());
    else throw std::runtime_error("未知参数: "+arg);
  }
  if(o.width<64 || o.height<64 || o.width>8192 || o.height>8192 || o.samples<1 ||
     !std::isfinite(o.seconds) || o.seconds<=0 || !std::isfinite(o.warmup) || o.warmup<0 ||
     !std::isfinite(o.refine) || o.refine<0 || o.render_delay_ms<0 || o.render_delay_ms>1000 || o.monitor<1 ||
     !std::isfinite(o.preview_seconds) || o.preview_seconds<0) throw std::runtime_error("无效尺寸、样本数或时长");
  if(o.inspect && o.file.empty()) throw std::runtime_error("--inspect 需要 --file");
  return o;
}
std::string utf8_path(const std::filesystem::path &p) {auto s=p.generic_u8string();return {s.begin(),s.end()};}
void save_json(const std::filesystem::path &path,const nlohmann::json &value) {
  std::ofstream file(path);if(!file) throw std::runtime_error("无法写入报告: "+utf8_path(path));file<<value.dump(2)<<'\n';
}
ccl::DeviceInfo select_device(const std::string &backend) {
  if(backend!="CUDA" && backend!="OPTIX") throw std::runtime_error("仅支持显式 CUDA 或 OPTIX，禁止自动回退 CPU");
  for(const auto &device:ccl::Device::available_devices())
    if(ccl::Device::string_from_type(device.type)==backend) return device;
  throw std::runtime_error("找不到请求的渲染设备: "+backend);
}
void screenshot(const std::filesystem::path &path,int width,int height) {
  std::vector<unsigned char> pixels(size_t(width)*height*4),flipped(pixels.size());
  glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  for(int y=0;y<height;++y) std::copy_n(pixels.data()+size_t(y)*width*4,size_t(width)*4,flipped.data()+size_t(height-1-y)*width*4);
  auto output=OIIO::ImageOutput::create(path.string());
  if(!output || !output->open(path.string(),OIIO::ImageSpec(width,height,4,OIIO::TypeDesc::UINT8)) ||
     !output->write_image(OIIO::TypeDesc::UINT8,flipped.data()) || !output->close())
    throw std::runtime_error("保存截图失败");
}
dfv::CameraState trajectory(double seconds,const dfv::CameraState &base) {
  auto state=base;
  const double t=std::fmod(std::max(0.0,seconds),60.0);
  if(t<20) {state.yaw=base.yaw+float(t*0.10);state.pitch=base.pitch+float(0.12*std::sin(t*0.3));}
  else if(t<40) {state.yaw=base.yaw+2;state.target.x=float(2*std::sin((t-20)*0.2));state.target.z=1+float(0.3*std::sin((t-20)*0.3));}
  else {state.yaw=base.yaw+2;state.distance=base.distance*float(1+0.25*std::sin((t-40)*0.3));}
  return state;
}
int run(const Options &o,const ccl::DeviceInfo &device) {
  using namespace dfv;
  std::filesystem::create_directories(o.output);
  const auto build_manifest=std::filesystem::path(ccl::path_get("build-manifest.json"));
  if(std::filesystem::exists(build_manifest)) std::filesystem::copy_file(build_manifest,o.output/"build-manifest.json",std::filesystem::copy_options::overwrite_existing);
  Telemetry telemetry(o.output);
  ir::Scene render_scene;
  size_t unsupported=0;
  if(o.file.empty()) render_scene=build_fixture(o.medium,"artifacts/assets/medium-v1");
  else {
    auto loaded=daz::load(o.file,{o.content_roots,o.strict});unsupported=loaded.report["warnings"].size();
    loaded.report["studio_lighting_added"]=true;
    save_json(o.output/"asset-report.json",loaded.report);render_scene=std::move(loaded.scene);
    std::cout<<"DAZ static preview: "<<render_scene.instances.size()<<" instances; "<<unsupported<<" compatibility diagnostics (asset-report.json)"<<std::endl;
  }
  const auto asset_bounds=render_scene.bounds();
  if(!o.file.empty()) ir::add_studio(render_scene);
  ccl::SessionParams params;
  params.device=device;params.background=o.smoke||o.dump_shaders;params.headless=params.background;
  params.samples=o.samples;params.pixel_size=1;params.use_resolution_divider=false;
  params.use_auto_tile=false;params.threads=8;
  ccl::SceneParams scene_params;scene_params.background=params.background;
  scene_params.use_texture_cache=false;scene_params.auto_texture_cache=false;
  // 窗口先于 Session 创建，确保 interop 析构时 context 仍然有效。
  std::unique_ptr<Window> window;
  if(!params.background) window=std::make_unique<Window>(o.width,o.height,o.fullscreen,&telemetry,o.monitor);
  auto session=std::make_unique<ccl::Session>(params,scene_params);
  auto &scene=*session->scene;
  auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);
  if(o.smoke && !o.file.empty()) {
    auto *albedo=scene.create_node<ccl::Pass>();albedo->set_name(ccl::ustring("diffuse_color"));albedo->set_type(ccl::PASS_DIFFUSE_COLOR);
  }
  scene.integrator->set_seed(1337);
  scene.integrator->set_max_bounce(8);scene.integrator->set_max_diffuse_bounce(4);
  scene.integrator->set_max_glossy_bounce(4);scene.integrator->set_max_transmission_bounce(8);
  scene.integrator->set_transparent_max_bounce(8);
  scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);
  const auto start=now();
  CameraState initial=window?window->mailbox.latest():CameraState{};
  if(o.file.empty()) {if(o.medium) initial.distance=19;}
  else {
    const auto c=asset_bounds.center();initial.target={c.x,c.y,c.z};
    initial.distance=std::max(asset_bounds.extent()*1.6f,.35f);initial.yaw=.3f;
    initial.pitch=asset_bounds.maximum.z-asset_bounds.minimum.z<asset_bounds.extent()*.5f?.7f:.08f;
  }
  render_scene.camera=render_camera(initial,o.width,o.height);
  CyclesAdapter adapter(scene);adapter.load(render_scene);const auto counts=adapter.stats();
  if(o.dump_shaders) {
    nlohmann::json materials=nlohmann::json::array();
    for(const auto &m:render_scene.materials)
      materials.push_back({{"id",m.id},{"color",{m.base_color.x,m.base_color.y,m.base_color.z}},
        {"roughness",m.roughness},{"metallic",m.metallic},{"opacity",m.opacity},{"transmission",m.transmission},
        {"texture",m.color_texture<0?"":utf8_path(render_scene.textures.at(m.color_texture).file)}});
    save_json(o.output/"mapped-materials.json",materials);
    nlohmann::json meshes=nlohmann::json::array();
    for(auto *geometry:scene.geometry) if(geometry->is_mesh()) {
      const auto *mesh=static_cast<ccl::Mesh *>(geometry);
      const auto *uv=mesh->attributes.find(ccl::ATTR_STD_UV)->data<ccl::float2>();
      nlohmann::json groups=nlohmann::json::array();
      for(size_t group=0;group<mesh->get_used_shaders().size();++group) {
        const auto *shader=static_cast<ccl::Shader *>(mesh->get_used_shaders()[group]);
        nlohmann::json sample=nullptr;
        for(size_t t=0;t<mesh->num_triangles();++t) if(mesh->get_shader()[t]==group) {
          sample={{"triangle",t},{"uv",{uv[t*3].x,uv[t*3].y}},{"vertex",mesh->get_triangles()[t*3]}};break;
        }
        groups.push_back({{"shader",shader->name.string()},{"sample",sample}});
      }
      meshes.push_back(groups);
    }
    save_json(o.output/"mapped-meshes.json",meshes);
    for(size_t i=0;i<scene.shaders.size();++i) {
      auto &shader=*scene.shaders[i];
      shader.graph->finalize(&scene);
      shader.graph->dump_graph((o.output/(std::to_string(i)+".dot")).string().c_str());
    }
    std::cout<<"Shader diagnostics saved without rendering"<<std::endl;return 0;
  }
  session->dfv_requested_epoch=initial.epoch;
  if(window) {window->camera=initial;window->mailbox.publish(initial);}
  session->dfv_event=[&](const char *name,uint64_t epoch,double ms) {
    Frame f;f.epoch=epoch;telemetry.event(name,f,ms);
    if(o.render_delay_ms && std::string_view(name)=="render_work_cpu") {
      telemetry.event("simulated_render_delay",f,o.render_delay_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(o.render_delay_ms));
    }
  };
  ccl::BufferParams buffers;buffers.width=buffers.full_width=o.width;buffers.height=buffers.full_height=o.height;
  std::ofstream manifest(o.output/"manifest.json");
  LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
  manifest<<"{\n  \"backend\": "<<std::quoted(o.backend)<<",\n  \"device\": "<<std::quoted(device.description)
    <<",\n  \"process_id\": "<<GetCurrentProcessId()<<", \"qpc_frequency\": "<<frequency.QuadPart
    <<",\n  \"scene\": "<<std::quoted(o.file.empty()?(o.medium?"medium-v1":"smoke-v1"):"dson-static-preview")
    <<",\n  \"input_file\": "<<std::quoted(utf8_path(o.file))<<", \"monitor_index\": "<<o.monitor
    <<", \"monitor_device\": "<<std::quoted(window?window->monitor_device:"")
    <<",\n  \"width\": "<<o.width<<", \"height\": "<<o.height<<",\n  \"pixel_size\": 1, \"resolution_divider\": false,"
    <<"\n  \"denoise\": false, \"adaptive_sampling\": false, \"seed\": 1337,"
    <<"\n  \"bounces\": {\"max\":8,\"diffuse\":4,\"glossy\":4,\"transmission\":8,\"transparent\":8},"
    <<"\n  \"samples\": "<<o.samples<<", \"unique_triangles\": "<<counts.unique_triangles
    <<", \"instanced_triangles\": "<<counts.triangles<<", \"geometry_objects\": "<<counts.instances
    <<", \"materials\": "<<counts.materials<<", \"textures\": "<<counts.textures<<", \"area_lights\": "<<render_scene.lights.size()<<","
    <<"\n  \"measurement_seconds\": "<<o.seconds<<", \"warmup_seconds\": "<<o.warmup<<", \"refine_seconds\": "<<o.refine
    <<", \"benchmark\": "<<(o.benchmark?"true":"false")
    <<", \"trajectory\": "<<std::quoted(o.benchmark?"orbit20-pan20-dolly20-v1":"manual")
    <<", \"input_hz\": "<<(o.benchmark?120:0)<<", \"simulated_render_delay_ms\": "<<o.render_delay_ms<<","
    <<"\n  \"blender_commit\": \"d13f752e3b9c4f8c261cda552b1021f8bcc0382c\","
    <<"\n  \"standalone_commit\": \"3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba\","
    <<"\n  \"libraries_commit\": \"60d6e96b917568278d400a4024c98da0fb777338\"\n}\n";
  manifest.close();
  std::cout<<"Scene ready: "<<counts.triangles<<" triangles, "<<counts.textures<<" textures; "<<o.backend<<std::endl;
  if(o.smoke) {
    const auto path=std::filesystem::absolute(o.output/"smoke.png").string();
    auto output=std::make_unique<Output>(o.output,!o.file.empty());auto *output_result=output.get();
    session->set_output_driver(std::move(output));
    session->reset(params,buffers);session->start();session->wait();
    if(session->progress.get_error()) throw std::runtime_error(session->progress.get_error_message());
    if(!output_result->written || !output_result->error.empty()) throw std::runtime_error("输出失败: "+output_result->error);
    auto input=OIIO::ImageInput::open(path);
    if(!input || input->spec().width!=o.width || input->spec().height!=o.height)
      throw std::runtime_error("烟雾测试图像缺失或尺寸错误");
    std::vector<float> pixels(size_t(o.width)*o.height*input->spec().nchannels);
    if(!input->read_image(0,0,0,-1,OIIO::TypeDesc::FLOAT,pixels.data())) throw std::runtime_error("烟雾测试图像损坏");
    input->close();
    std::cout<<"Render complete: "<<path<<" in "<<now()-start<<" s"<<std::endl;
    return 0;
  }
  auto display_owner=std::make_unique<Display>(*window,telemetry,session->dfv_render_epoch,session->dfv_render_samples,o.readback);
  Display *display=display_owner.get();session->set_display_driver(std::move(display_owner));
  std::atomic<bool> stop{false};
  std::string worker_error;std::mutex error_mutex;
  auto failure=[&](const std::exception &e) {std::lock_guard lock(error_mutex);worker_error=e.what();stop=true;};
  session->reset(params,buffers);session->start();
  std::jthread presenter([&] {
    bool active=false;
    try {
      window->present_context.activate();active=true;
      std::cout<<"OpenGL: "<<glGetString(GL_VERSION)<<" / "<<glGetString(GL_RENDERER)<<std::endl;
      if(epoxy_has_wgl_extension(window->dc,"WGL_EXT_swap_control")) wglSwapIntervalEXT(1);
      uint64_t swap_id=0;
      while(!stop.load()) {
        if(window->minimized) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
        glViewport(0,0,o.width,o.height);glClearColor(0.03f,0.03f,0.03f,1);glClear(GL_COLOR_BUFFER_BIT);
        session->draw();
        std::ostringstream text;text<<o.backend<<" | "<<o.width<<'x'<<o.height<<" | epoch "<<telemetry.displayed_epoch
          <<" | samples "<<session->dfv_render_samples<<" | submitted "<<telemetry.submitted<<" | RMB orbit / Shift+RMB pan / wheel dolly";
        if(unsupported) text<<" | Preview limitations: "<<unsupported;
        display->hud(text.str());
        LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);
        if(!SwapBuffers(window->dc)) throw std::runtime_error("SwapBuffers 失败");
        QueryPerformanceCounter(&after);telemetry.swap(++swap_id,before.QuadPart,after.QuadPart,display->drawn_frame());
        display->after_swap();
      }
      // 仅结束后回读截图，不混入导航性能区间或 interop 回退统计。
      session->draw();screenshot(o.output/"viewport.png",o.width,o.height);
      display->release_present_resources();
      window->present_context.deactivate();active=false;
    } catch(const std::exception &e) {if(active) window->present_context.deactivate();failure(e);}
  });
  std::jthread controller([&] {
    try {
      uint64_t applied=initial.epoch;
      while(!stop.load()) {
        auto camera=window->mailbox.latest();
        // 等当前相机产生可呈现结果，期间覆盖邮箱，避免高频 reset 饿死渲染。
        if(camera.epoch!=applied && telemetry.displayed_epoch.load()>=applied && session->ready_to_reset()) {
          const double begin=now();
          {ccl::thread_scoped_lock lock(scene.mutex);
            camera=window->mailbox.latest();
            ir::Delta delta;delta.camera=render_camera(camera,o.width,o.height);adapter.apply(delta);session->dfv_requested_epoch=camera.epoch;
            session->reset(params,buffers);
          }
          applied=camera.epoch;Frame frame;frame.epoch=applied;
          telemetry.event("camera_apply",frame,(now()-begin)*1000);
          telemetry.event("input_to_apply",frame,(now()-camera.input_seconds)*1000);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } catch(const std::exception &e) {failure(e);}
  });
  double next_input=now(),last_tick=now(),peak_ui_gap_ms=0;
  bool completed=false,invalid_display=false,measurement_started=false;
  bool invalid_foreground=false,invalid_minimized=false,invalid_size=false;
  while(!window->close && !stop.load()) {
    const double time=now();peak_ui_gap_ms=std::max(peak_ui_gap_ms,(time-last_tick)*1000);last_tick=time;
    window->poll();
    if(display->failed() || session->progress.get_error()) {stop=true;break;}
    const double first=telemetry.first_frame.load();
    if(first<0 && time-start>180) {std::lock_guard lock(error_mutex);worker_error="首帧等待超过 180 秒";stop=true;break;}
    if(!o.benchmark && o.preview_seconds>0 && first>=0 && time-first>=o.preview_seconds) {completed=true;break;}
    if(o.benchmark && first>=0) {
      const double measurement=first+o.warmup;
      if(telemetry.measurement_start.load()<0) {telemetry.measurement_start=measurement;telemetry.measurement_end=measurement+o.seconds;}
      if(!measurement_started && time>=measurement) {
        measurement_started=true;
        RECT client{};GetClientRect(window->hwnd,&client);
        window->size_changed=client.right!=o.width || client.bottom!=o.height;
        std::cout<<"Measurement client: "<<client.right<<'x'<<client.bottom<<" DPI "<<GetDpiForWindow(window->hwnd)
                 <<" foreground="<<(GetForegroundWindow()==window->hwnd)<<std::endl;
      }
      if(time<measurement+o.seconds && time>=next_input) {
        const auto epoch=window->camera.epoch;
        window->camera=trajectory(time<measurement?(time-first)*60/std::max(o.warmup,0.001):time-measurement,initial);window->camera.epoch=epoch;
        window->publish();
        next_input=std::max(next_input+1.0/120,time);
      }
      if(time>=measurement && time<measurement+o.seconds) {
        invalid_minimized|=window->minimized.load();invalid_size|=window->size_changed.load();
        invalid_foreground|=GetForegroundWindow()!=window->hwnd;
        invalid_display=invalid_minimized || invalid_size || invalid_foreground;
      }
      if(time>=measurement+o.seconds+o.refine) {completed=true;break;}
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  stop=true;controller.join();presenter.join();
  session->cancel(true);
  PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory));
  std::ofstream result(o.output/"run.json");
  result<<"{\n  \"completed\": "<<(completed?"true":"false")<<", \"display_invalid\": "<<(invalid_display?"true":"false")
    <<", \"invalid_foreground\": "<<(invalid_foreground?"true":"false")<<", \"invalid_size\": "<<(invalid_size?"true":"false")
    <<", \"invalid_minimized\": "<<(invalid_minimized?"true":"false")
    <<",\n  \"visible_fps_verified\": false, \"visible_fps\": null,"
    <<"\n  \"benchmark\": "<<(o.benchmark?"true":"false")
    <<", \"navigation_submission_fps\": "<<(o.benchmark?std::to_string(telemetry.measured_frames.load()/o.seconds):"null")
    <<", \"navigation_frames\": "<<telemetry.measured_frames<<", \"produced_frames\": "<<telemetry.produced
    <<", \"submitted_frames\": "<<telemetry.submitted<<", \"interop_readback_bytes\": "<<telemetry.readback_bytes
    <<", \"skipped_updates\": "<<telemetry.skipped<<",\n  \"measurement_start\": "<<telemetry.measurement_start
    <<", \"measurement_end\": "<<telemetry.measurement_end<<", \"ui_max_loop_gap_ms\": "<<peak_ui_gap_ms
    <<", \"cycles_peak_bytes\": "<<session->stats.mem_peak<<", \"process_private_bytes\": "<<memory.PrivateUsage
    <<", \"final_input_epoch\": "<<window->camera.epoch<<", \"final_submitted_epoch\": "<<telemetry.displayed_epoch
    <<", \"adapter_mesh_creations\": "<<adapter.stats().meshes<<", \"adapter_camera_updates\": "<<adapter.stats().camera_updates<<"\n}\n";
  result.close();
  if(display->failed()) throw std::runtime_error(display->error());
  if(session->progress.get_error()) throw std::runtime_error(session->progress.get_error_message());
  if(!worker_error.empty()) throw std::runtime_error(worker_error);
  if(o.benchmark) std::cout<<"Navigation submission FPS: "<<telemetry.measured_frames.load()/o.seconds<<"; visible FPS unverified"<<std::endl;
  else std::cout<<"Preview closed; screenshot: "<<utf8_path(o.output/"viewport.png")<<std::endl;
  return o.benchmark && !completed ? 2 : 0;
}
}
int wmain(int argc,wchar_t **wide_argv) {
  SetConsoleOutputCP(CP_UTF8);
  std::vector<std::string> args;std::vector<char *> argv;
  for(int i=0;i<argc;++i) {
    const int size=WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,nullptr,0,nullptr,nullptr);
    std::string arg(size,'\0');WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,arg.data(),size,nullptr,nullptr);arg.pop_back();args.push_back(std::move(arg));
  }
  for(auto &arg:args) argv.push_back(arg.data());
  struct TimerPeriod {TimerPeriod(){timeBeginPeriod(1);}~TimerPeriod(){timeEndPeriod(1);}} timer_period;
  PROCESS_POWER_THROTTLING_STATE power{};power.Version=PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  power.ControlMask=PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;power.StateMask=0;
  SetProcessInformation(GetCurrentProcess(),ProcessPowerThrottling,&power,sizeof(power));
  try {
    // 本基准显式使用 Cycles 内置 Linear/sRGB，避免依赖用户的 OCIO 环境。
    auto color_config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();
    color_config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(color_config);
    const auto options=parse(argc,argv.data());
    ccl::path_init(ccl::path_dirname(argv[0]),DFV_CYCLES_SOURCE);
    if(options.help) {
      std::cout<<"DazFastViewer: --file <DUF> [--content-root <directory>] [--inspect] [--strict-dson]\n"
               <<"  --monitor 2 --width 1600 --height 900 --device OPTIX\n"
               <<"  --preview-seconds <seconds>  自动关闭预览；省略则保持交互窗口\n"
               <<"  --smoke  离线 PNG/EXR；--samples <count>；--output <directory>\n"
               <<"  --dump-shaders  导出材质图和绑定诊断，不创建窗口或执行渲染\n"
               <<"  右键旋转 / Shift+右键平移 / 滚轮缩放 / Esc 退出\n";return 0;
    }
    if(options.inspect) {
      auto loaded=dfv::daz::load(options.file,{options.content_roots,options.strict});
      std::filesystem::create_directories(options.output);save_json(options.output/"asset-report.json",loaded.report);
      std::cout<<"Loaded "<<loaded.scene.meshes.size()<<" meshes, "<<loaded.scene.materials.size()<<" materials, "<<loaded.scene.textures.size()<<" textures; "
               <<loaded.report["warnings"].size()<<" compatibility diagnostics"<<std::endl;return 0;
    }
    if(options.devices) {
      for(const auto &device:ccl::Device::available_devices())
        std::cout<<ccl::Device::string_from_type(device.type)<<" | "<<device.id<<" | "<<device.description<<'\n';
      return 0;
    }
    return run(options,select_device(options.backend));
  } catch(const std::exception &error) {std::cerr<<"ERROR: "<<error.what()<<std::endl;return 1;}
}
