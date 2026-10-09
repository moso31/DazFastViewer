#include "editor/renderer.h"
#include "editor/group_transforms.h"
#include "editor/node_properties.h"
#include "water/document.h"
#include "cloud/document.h"
#include "editor/pose_drag.h"
#include "editor/powerpose_drag.h"
#include "editor/render_edit_queue.h"
#include "editor/deformation_refiner.h"
#include "editor/physics_present.h"
#include "viewport/display.h"
#include "viewport/overlay.h"
#include "runtime/picking.h"
#include "bench/fixtures.h"
#include "device/device.h"
#include "scene/scene.h"
#include "scene/integrator.h"
#include "scene/pass.h"
#include "session/session.h"
#include "diagnostics/load_profile.h"
#include "diagnostics/event_profile.h"
#include <epoxy/wgl.h>
#include <cuew.h>
#include <fstream>
#include <bit>

namespace dfv::editor {
Renderer::Renderer(HWND host,int width,int height,const std::filesystem::path &output,SamplingSettings sampling):output_(output),sampling_(sampling),telemetry_(output) {
  requested_render_quality_=sampling;
  window_=std::make_unique<Window>(width,height,false,&telemetry_,0,host);
  thread_=std::jthread([this](std::stop_token stop) {run(stop);});
}
Renderer::~Renderer() {thread_.request_stop();if(thread_.joinable()) thread_.join();}
void Renderer::set_document(std::shared_ptr<const Document> document,const Snapshot &snapshot,bool frame_scene) {
  if(frame_scene) {ir::Bounds bounds;for(const auto &target:document->catalog.targets) {const auto &i=document->loaded.scene.instances[target.instance];if(i.visible) for(auto p:document->loaded.scene.meshes[i.mesh].positions) bounds.add(i.transform.point(p));}
    if(!document->cities.empty()){const GroupFrames frames(*document,snapshot.group_transforms);for(const auto &c:document->cities){const auto transform=frames.delta(c->id)*ir::Transform::translate({float(c->config.origin_x),float(c->config.origin_y),float(c->config.origin_z)});for(const auto &cell:c->cells)for(int k=0;k<8;++k)bounds.add(transform.point({k&1?cell.bounds.maximum.x:cell.bounds.minimum.x,k&2?cell.bounds.maximum.y:cell.bounds.minimum.y,k&4?cell.bounds.maximum.z:cell.bounds.minimum.z}));}}frame(bounds);}
  std::lock_guard lock(mutex_);document_=std::move(document);snapshot_=snapshot;
  if(sampling_.rebuild_probe) telemetry_.event("document_submitted");
}
void Renderer::resize(int width,int height) {
  if(width<1||height<1) return;
  SetWindowPos(window_->hwnd,nullptr,0,0,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
  std::lock_guard lock(mutex_);requested_width_=width;requested_height_=height;resize_preview_until_=now()+.15;
}
void Renderer::pointer(int x,int y,bool click,bool toggle) {
  if(!window_->automated_pointer) {POINT point{x,y};ClientToScreen(window_->hwnd,&point);SetCursorPos(point.x,point.y);}
  PostMessageW(window_->hwnd,WM_MOUSEMOVE,toggle?MK_CONTROL:0,MAKELPARAM(x,y));
  if(click) PostMessageW(window_->hwnd,WM_LBUTTONUP,toggle?MK_CONTROL:0,MAKELPARAM(x,y));
}
void Renderer::frame(const ir::Bounds &bounds) {
  if(bounds.empty) return;const auto c=bounds.center();
  window_->camera.target={c.x,c.y,c.z};window_->camera.distance=std::max(bounds.extent()*1.6f,.35f);window_->camera.yaw=.3f;
  window_->camera.pitch=bounds.maximum.z-bounds.minimum.z<bounds.extent()*.5f?.7f:.08f;window_->publish();
}
void Renderer::focus(const ir::Bounds &bounds) {
  if(bounds.empty) return;const auto c=bounds.center();window_->camera.target={c.x,c.y,c.z};
  const auto dx=bounds.maximum.x-bounds.minimum.x,dy=bounds.maximum.y-bounds.minimum.y,dz=bounds.maximum.z-bounds.minimum.z;
  window_->camera.distance=std::max(.025f,.55f*std::sqrt(dx*dx+dy*dy+dz*dz)/std::sin(.4f));window_->publish();
}
void Renderer::edit(const Snapshot &snapshot) {std::lock_guard lock(mutex_);if(document_ && snapshot.generation==document_->generation) {snapshot_=snapshot;edit_preview_until_=now()+.15;Frame f;f.id=snapshot.revision;telemetry_.event("edit_input",f);}}
void Renderer::interaction(bool active) {std::lock_guard lock(mutex_);if(active&&!edit_active_) interaction_revision_=snapshot_.revision+1;edit_active_=active;}
void Renderer::retry_resources() {std::lock_guard lock(mutex_);++retry_resources_;}
void Renderer::select(uint64_t generation,int target,int joint,std::vector<Selection> selections,bool ik_allowed,const std::string &group) {std::lock_guard lock(mutex_);selected_group_=group;selection_generation_=generation;selected_target_=target;selected_joint_=joint;selections_=std::move(selections);ik_allowed_=ik_allowed;++window_->pose_selection;}
RenderStatus Renderer::status() {std::lock_guard lock(mutex_);return status_;}
CameraState Renderer::input_camera() {return window_->mailbox.latest();}
std::future<ViewportCapture> Renderer::capture(uint64_t generation,uint64_t revision){
  std::lock_guard lock(mutex_);capture_=std::make_unique<CaptureRequest>();capture_->generation=generation;capture_->revision=revision;capture_->camera_epoch=window_->mailbox.latest().epoch;return capture_->result.get_future();
}
void Renderer::cancel_capture(){std::lock_guard lock(mutex_);capture_.reset();}
std::future<water::Inputs> Renderer::water_inputs(uint64_t generation,uint64_t revision,const water::Water &w){
  std::lock_guard lock(mutex_);water_request_=std::make_unique<WaterRequest>();water_request_->generation=generation;water_request_->revision=revision;water_request_->water=w;return water_request_->result.get_future();
}
void Renderer::capture_frame(uint64_t generation,uint64_t revision,const CameraState &camera){
  std::unique_ptr<CaptureRequest> request;{std::lock_guard lock(mutex_);if(!capture_||capture_->generation!=generation||capture_->revision!=revision)return;request=std::move(capture_);}
  try{
    if(camera.epoch!=request->camera_epoch)throw std::runtime_error("保存期间视口发生变化，请重新保存");
    const int w=window_->width,h=window_->height,n=std::min(w,h);if(n<=0)throw std::runtime_error("视口尺寸无效");
    ViewportCapture result;result.size=n;result.rgba.resize(size_t(n)*n*4);result.view={camera.target.x,camera.target.y,camera.target.z,camera.distance,camera.yaw,camera.pitch};
    GLint alignment=4,buffer=GL_BACK;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glGetIntegerv(GL_READ_BUFFER,&buffer);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels((w-n)/2,(h-n)/2,n,n,GL_RGBA,GL_UNSIGNED_BYTE,result.rgba.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);glReadBuffer(buffer);
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("视口缩略图读取失败");request->result.set_value(std::move(result));
  }catch(...){request->result.set_exception(std::current_exception());}
}
void Renderer::camera_view(const std::array<float,6> &v) {window_->camera.target={v[0],v[1],v[2]};window_->camera.distance=v[3];window_->camera.yaw=v[4];window_->camera.pitch=v[5];window_->publish();}
void Renderer::keyboard(int key,bool pressed) {SetFocus(window_->hwnd);PostMessageW(window_->hwnd,pressed?WM_KEYDOWN:WM_KEYUP,WPARAM(key),0);}
void Renderer::orbit(float x,float y) {window_->camera.orbit(x,y);window_->publish();}
void Renderer::run(std::stop_token stop) {
  using namespace ccl;
  std::unique_ptr<Session> session;Display *display=nullptr;
  std::unique_ptr<CyclesAdapter> adapter;
  nlohmann::json sampling_report;
  std::atomic<size_t> gpu_device_bytes{0},gpu_host_bytes{0};
  std::ofstream progress_log(output_/"cycles-progress.log");std::string last_progress;
  HoverOverlay overlay;runtime::PickingScene picking;std::vector<runtime::JointRegions> regions;std::vector<uint8_t> pickable;bool geometry_dirty=true;uint64_t clicks=0,pick_revision=0;
  std::optional<runtime::InstanceGroups> instance_groups;
  PhysicsService physics_service;
  std::future<std::shared_ptr<PreparedPhysics>> physics_prepare;
  std::future<bool> physics_commit;
  std::vector<std::future<void>> physics_retired;
  std::shared_ptr<PreparedPhysics> physics_prepared,physics_installing;
  std::shared_ptr<PhysicsResult> physics_pending;
  std::atomic<uint64_t> physics_ticket{0};uint64_t physics_prepare_ticket=0,physics_commit_ticket=0;
  Snapshot physics_input;runtime::PhysicsOptions physics_options;std::shared_ptr<const Document> physics_document;
  double physics_last_commit=-1e30;uint64_t physics_last_render_frame=0;
  double physics_due=0;bool physics_requested=false;uint64_t physics_request_serial=0;
  bool physics_display_wait=false;uint64_t physics_display_camera=0,physics_display_ticket=0;
  auto retire_physics=[&](std::shared_ptr<PreparedPhysics> value){if(value)physics_retired.push_back(std::async(std::launch::async,[this,value=std::move(value)]()mutable{release_physics_present(std::move(value),*window_);}));};
  auto cleanup=[&] {
    physics_display_wait=false;
    ++physics_ticket;physics_service.cancel();physics_service.retire(std::move(physics_pending));
    if(physics_commit.valid())try{physics_commit.get();}catch(...){}
    if(physics_prepare.valid())try{retire_physics(physics_prepare.get());}catch(...){}
    retire_physics(std::move(physics_prepared));retire_physics(std::move(physics_installing));
    for(auto &f:physics_retired)try{f.get();}catch(...){}physics_retired.clear();physics_document.reset();physics_requested=false;
    if(session) {
      {diagnostics::Scope scope("cancel");session->cancel(true);}
      const auto &integrator=*session->scene->integrator;const auto &background=session->scene->dscene.data.background;
      sampling_report={{"denoise",integrator.get_use_denoise()},{"max_samples",sampling_.samples},{"adaptive_sampling",integrator.get_use_adaptive_sampling()},
        {"adaptive_threshold",integrator.get_adaptive_threshold()},{"min_bounces",integrator.get_min_bounce()},{"transparent_min_bounces",integrator.get_transparent_min_bounce()},
        {"sampling_pattern",sampling_.blue_noise?"blue_noise_first":"tabulated_sobol"},{"background_mis",background.use_mis},{"background_map_resolution",{background.map_res_x,background.map_res_y}}};
      try {
        GLContext::Binding binding(window_->present_context);overlay.release();if(display) display->release_present_resources();
      } catch(const std::exception &e) {
        telemetry_.graphics("cleanup_failure",e.what());
        // 无法访问的 GL 对象留给旧共享组销毁，CPU 侧不能沿用旧编号。
        overlay=HoverOverlay{};
      }
      sampling_report["update_interval_seconds"]=sampling_.update_interval_seconds;sampling_report["prune_hidden"]=sampling_.prune_hidden;sampling_report["texture_limit"]=sampling_.texture_limit;
      sampling_report["subsurface"]=sampling_.subsurface;sampling_report["bump_and_normal"]=sampling_.bump_and_normal;
      sampling_report["transparent_bounces"]=integrator.get_transparent_max_bounce();
      sampling_report["device_allocated_bytes"]=session->stats.mem_used;sampling_report["device_peak_bytes"]=session->stats.mem_peak;
      const auto *gpu=static_cast<GPUDevice *>(session->device.get());
      sampling_report["host_mapped_bytes"]=gpu->dfv_host_bytes();sampling_report["cuda_device_allocated_bytes"]=gpu->dfv_device_bytes();
      adapter.reset();
      {diagnostics::Scope scope("session_destroy");session.reset();}display=nullptr;
      gpu_device_bytes=0;gpu_host_bytes=0;
    }
  };
  RenderStatus state;
  bool graphics_pending=false,graphics_recovering=false,graphics_blocked=false;
  unsigned graphics_attempts=0;
  uint64_t graphics_recoveries=0,render_restart=0,failed_generation=0;
  double next_graphics_attempt=0,graphics_stable_since=0;
  auto publish_state=[&] {
    state.physics_committing=physics_commit.valid();
    state.physics_prepared=bool(physics_prepared);
    state.graphics_recovering=graphics_recovering;state.graphics_blocked=graphics_blocked;
    state.graphics_attempts=graphics_attempts;state.graphics_recoveries=graphics_recoveries;
    std::lock_guard lock(mutex_);status_=state;
  };
  double next_state_report=0;
  PoseDrag pose_drag;uint64_t pose_serial=0,pose_commits=0,pose_previews=0;bool pose_paused=false;
  PowerPoseDrag powerpose_drag;uint64_t powerpose_serial=0,powerpose_selection=0;
  GizmoDrag gizmo_drag;uint64_t gizmo_serial=0,gizmo_consumed=0,gizmo_selection=UINT64_MAX,gizmo_revision=UINT64_MAX;int gizmo_light=-1;bool gizmo_valid=false;std::string gizmo_group;std::vector<uint32_t> gizmo_group_members;
  std::vector<std::vector<ir::Vec3>> powerpose_reference;
  struct {bool active=false,powerpose=false;uint64_t generation=0,revision=0;bool gizmo=false;} pose_recovery;
  uint64_t sessions=0;
  bool blank_presented=false;
  auto timing=[&](const char *name,double begin,uint64_t revision=0) {Frame f;f.epoch=state.requested_epoch;f.id=revision;telemetry_.event(name,f,(now()-begin)*1000);};
  try {
    const auto devices=Device::available_devices();
    DeviceInfo device;
    for(const auto &candidate:devices) if(candidate.type==DEVICE_OPTIX) {device=candidate;break;}
    if(device.type!=DEVICE_OPTIX) throw std::runtime_error("找不到 OptiX 设备；请使用受支持的 NVIDIA 显卡并更新驱动到 R590 或更高版本（OptiX 9.1 要求）。编辑器不会降低画质或回退 CPU。");
    {
      // Match the render GPU to the actual viewport, including systems with multiple NVIDIA GPUs.
      GLContext::Binding binding(window_->render_context);
      int count=0;unsigned matched=0;
      if(!cuDeviceGetCount||!cuGLGetDevices||cuDeviceGetCount(&count)!=CUDA_SUCCESS||count<1)
        throw std::runtime_error("无法查询 NVIDIA 显示设备，请检查显卡驱动");
      std::vector<CUdevice> display_devices(size_t(count),0);
      const auto result=cuGLGetDevices(&matched,display_devices.data(),unsigned(count),CU_GL_DEVICE_LIST_ALL);
      bool found=false;
      if(result==CUDA_SUCCESS) for(const auto &candidate:devices) if(candidate.type==DEVICE_OPTIX) {
        if(std::find(display_devices.begin(),display_devices.begin()+matched,candidate.num)!=display_devices.begin()+matched) {device=candidate;found=true;break;}
      }
      if(!found) throw std::runtime_error("视口没有使用可互操作的 NVIDIA 显卡。请在 Windows 设置 → 系统 → 屏幕 → 显示卡中，将 DazFastViewer.exe 设为高性能 NVIDIA GPU，然后重新启动程序。");
    }
    std::shared_ptr<const Document> current,group_source,group_document;GroupTransforms applied_groups;std::optional<GroupFrames> group_frames;std::vector<ir::AreaLight> source_lights;
    // The runtime holds references into the document it was constructed with.
    std::shared_ptr<const Document> runtime_document,runtime_source;GroupTransforms document_groups;
    std::optional<GroupFrames> runtime_frames;
    std::unique_ptr<ir::Scene> render_scene_ptr;
    std::shared_ptr<const Document> pending_document;
    std::unique_ptr<ir::Scene> pending_scene;
    cloud::Runtime cloud_runtime,cloud_physics_runtime;
    city::Runtime city_runtime;uint64_t city_camera_epoch=0,water_camera_epoch=0;
    std::unique_ptr<runtime::DeformationRuntime> pending_runtime;
    std::unique_ptr<runtime::DeformationRuntime> runtime;
    DeformationRefiner refiner;
    uint64_t refine_ticket=0,refine_revision=0;
    bool refine_pending=false,refine_enabled=false;
    std::vector<ir::Transform> refine_frames;
    runtime::CollisionStats refined_collision;
    uint64_t epoch=0,camera_epoch=0,applied_revision=0,attempted_revision=0,measured_evaluation=0,measured_skinning=0,measured_transform=0;
    uint64_t retried=0,failed_revision=UINT64_MAX;
    std::vector<std::vector<ir::Vec3>> previous_positions;
    std::vector<double> displacements;
    SessionParams params;params.device=device;params.samples=sampling_.samples;params.pixel_size=1;params.background=false;
    params.use_resolution_divider=false;params.use_auto_tile=false;params.threads=8;
    params.dfv_update_interval=sampling_.update_interval_seconds;
    BufferParams buffers;buffers.width=buffers.full_width=window_->width;buffers.height=buffers.full_height=window_->height;
    bool preview=false;
    ViewportQuality quality;int applied_percent=0;
    int camera_width=0,camera_height=0;
    auto set_quality=[&](bool moving) {
      preview=moving;params.samples=moving?2:sampling_.samples;
      const auto [width,height]=render_size(window_->width,window_->height,quality.percent,moving);
      buffers.width=buffers.full_width=width;buffers.height=buffers.full_height=height;applied_percent=quality.percent;
    };
    Snapshot desired;InstanceGrounds applied_instance_ground;bool edit_affects_render=false;
    RenderEditQueue queued;bool clay_wait=false;uint64_t gpu_revision=0;
    std::vector<ir::SubdivisionSettings> queued_subdivision_before;
    auto reset_render_state=[&] {
      refiner.cancel();refine_pending=false;refine_frames.clear();refined_collision={};
      runtime.reset();render_scene_ptr.reset();pending_runtime.reset();pending_scene.reset();pending_document.reset();
      runtime_document.reset();runtime_source.reset();runtime_frames.reset();
      current.reset();picking={};regions.clear();pickable.clear();instance_groups.reset();geometry_dirty=true;
      previous_positions.clear();displacements.clear();powerpose_reference.clear();
      pose_drag.active=powerpose_drag.active=gizmo_drag.active=false;pose_recovery={};pose_paused=false;gizmo_valid=false;
      queued.clear();clay_wait=false;gpu_revision=0;
      queued_subdivision_before.clear();
      ++window_->pose_selection;pose_serial=window_->pose_pointer().serial;gizmo_selection=gizmo_revision=UINT64_MAX;
      state={};state.clicks=clicks;telemetry_.displayed_epoch=0;telemetry_.displayed_samples=0;
    };
    while(!stop.stop_requested()) {
      std::shared_ptr<const Document> document;std::string selected_group;
      std::vector<Selection> selections;int width,height,selected_target,selected_joint;uint64_t selection_generation,retry;
      bool editing,ik_allowed;double preview_until,resize_until;
      std::vector<runtime::PosePin> pose_pins;
      runtime::PowerPoseInput powerpose;
      GizmoSettings gizmo_settings;
      uint64_t material_hover_generation;std::vector<std::pair<size_t,size_t>> material_hover;
      {std::lock_guard lock(mutex_);material_hover_generation=material_hover_generation_;material_hover=material_hover_;}
      {std::lock_guard lock(mutex_);document=document_;if(document&&(desired.generation!=snapshot_.generation||desired.revision!=snapshot_.revision)){desired=snapshot_;source_lights=desired.lights;}selected_group=selected_group_;width=requested_width_;height=requested_height_;selected_target=selected_target_;selected_joint=selected_joint_;selections=selections_;selection_generation=selection_generation_;retry=retry_resources_;editing=edit_active_&&snapshot_.revision>=interaction_revision_;preview_until=edit_preview_until_;resize_until=resize_preview_until_;pose_pins=pose_pins_;ik_allowed=ik_allowed_;}
      {std::lock_guard lock(mutex_);powerpose=powerpose_input_;gizmo_settings=gizmo_settings_;quality=quality_;}
      try {
        if(!document){group_source.reset();group_document.reset();group_frames.reset();applied_groups.clear();document_groups.clear();}
        if(document&&(document!=group_source||desired.group_transforms!=applied_groups)) {
          const auto begin=now();GroupFrames next(*document,desired.group_transforms);bool reused=false;
          if(runtime&&session&&runtime_source==document&&runtime_frames){std::vector<ir::Transform> frames;for(const auto &id:next.hierarchy.targets){const auto a=next.delta(id),b=runtime_frames->delta(id);frames.push_back(a==b?ir::Transform{}:a*ir::inverse(b));}reused=runtime->reframe(frames);if(reused)refine_frames=std::move(frames);}
          if(!reused){group_document=transformed_groups(document,desired.group_transforms);document_groups=desired.group_transforms;}
          group_frames=std::move(next);group_source=document;applied_groups=desired.group_transforms;
          if(reused)timing("group_reframe",begin,desired.revision);
        }
        document=group_document;desired.lights=source_lights;if(group_frames)for(auto &light:desired.lights)light.transform=group_frames->delta(light.id)*light.transform;
      }catch(const std::exception &e){state.edit_error=e.what();publish_state();std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
      runtime::PhysicsOptions wanted_physics;{std::lock_guard lock(mutex_);wanted_physics=physics_options_;}
      for(auto it=physics_retired.begin();it!=physics_retired.end();)if(physics_ready(*it)){try{it->get();}catch(...){}it=physics_retired.erase(it);}else ++it;
      // 场景切换期间提交线程仍拥有旧会话；保持相机反馈，直到可以安全回收。
      if(physics_commit.valid()&&!physics_ready(physics_commit)&&current!=document){
        ++physics_ticket;physics_service.cancel();GLContext::Binding b(window_->present_context);auto c=window_->mailbox.latest();overlay.draw_pose(c,window_->width,window_->height,{},{},{},{});window_->swap();state.camera=c;publish_state();std::this_thread::sleep_for(std::chrono::milliseconds(8));continue;
      }
      if(current!=document&&physics_document){
        ++physics_ticket;physics_service.cancel();physics_service.retire(std::move(physics_pending));
        if(physics_commit.valid())try{physics_commit.get();}catch(...){}

        retire_physics(std::move(physics_prepared));retire_physics(std::move(physics_installing));
        physics_document.reset();
      }
      const bool retry_payloads=retry!=retried;
      if(width>0&&height>0&&(width!=window_->width||height!=window_->height)) {
        window_->width=width;window_->height=height;
      }
      if(!document) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
      const auto restart=render_restarts_.load();
      if(restart!=render_restart||(graphics_blocked&&document->generation!=failed_generation)) {
        render_restart=restart;graphics_attempts=0;graphics_blocked=false;graphics_pending=true;graphics_recovering=true;
        next_graphics_attempt=0;state.error.clear();telemetry_.graphics("restart_requested","重新启动当前场景的渲染");
      }
      if(graphics_blocked) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
      if(graphics_pending&&now()<next_graphics_attempt) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
      try {
      if(graphics_pending) {
        if(graphics_attempts>=2) {
          graphics_pending=graphics_recovering=false;graphics_blocked=true;failed_generation=document->generation;
          state.error="自动恢复渲染失败，已停止重试。可保存场景，然后使用“视图 → 重启渲染”。最后错误："+state.error;
          telemetry_.graphics("recovery_exhausted",state.error);publish_state();continue;
        }
        ++graphics_attempts;graphics_recovering=true;publish_state();
        telemetry_.graphics("recovery_begin","attempt="+std::to_string(graphics_attempts)+" generation="+std::to_string(document->generation));
        cleanup();reset_render_state();window_->recreate_contexts();graphics_pending=false;
        graphics_stable_since=0;last_progress.clear();
      }
      window_->check_graphics();
      RenderQuality render_quality;{std::lock_guard lock(mutex_);render_quality=requested_render_quality_;}
      if(render_quality!=static_cast<const RenderQuality &>(sampling_)) {
        // 先停止旧 GPU 会话并销毁 ImageManager/设备，再创建新纹理。
        // 保留 Document、编辑快照、相机和 CPU 运行时缓存，避免重新读取 DUF。
        telemetry_.event("render_quality_release_begin");cleanup();
        static_cast<RenderQuality &>(sampling_)=render_quality;state.error.clear();
        telemetry_.event("render_quality_release_end");
      }
      if(current==document&&!state.error.empty()) {
        if(desired.revision==failed_revision) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
        // 导入等级过高等可恢复错误：允许用户降低等级后重新准备场景。
        state.error.clear();
      }
      if((!runtime||!session)&&document_groups!=applied_groups){group_document=transformed_groups(group_source,applied_groups);document=group_document;document_groups=applied_groups;}
      if(current!=document||!session) {
        diagnostics::EventProfile profile(sampling_.rebuild_probe,[&](const char *name,double ms){telemetry_.event(name,{},ms);});
        if(pending_document!=document||!pending_runtime) {
          diagnostics::Scope scope("document_prepare");
          pending_runtime.reset();pending_scene=std::make_unique<ir::Scene>(document->loaded.scene);pending_document=document;
          diagnostics::Scope construction("runtime_construct");
          pending_runtime=std::make_unique<runtime::DeformationRuntime>(*pending_scene,document->catalog.targets,document->skeletons.skins,document->formulas.graphs,runtime.get());
        }
        const auto resources=pending_runtime->prepare(desired.values,desired.poses,retry_payloads);retried=retry;
        state.pending_payloads=resources.pending;state.resource_error=resources.error;
        if(resources.pending||!resources.error.empty()) {publish_state();std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
        {diagnostics::Scope scope("initial_evaluate");pending_runtime->evaluate(scene_properties(*document,desired),desired.poses);}
        apply_instance_node_visibility(*document,desired,*pending_scene);
        pending_scene->lights=group_lights(*group_source,*pending_scene,group_frames->hierarchy,desired.lights,pending_runtime->effective_poses(),&*group_frames);pending_scene->options=desired.options;apply_subdivision_levels(*pending_scene,desired.subdivision_levels);
        apply_material_overrides(*pending_scene,document->loaded.scene,desired.material_overrides);
        if(!desired.instance_ground.empty()||!group_frames->hierarchy.groups.empty()){const auto bases=group_instance_bases(*group_source,*pending_scene,group_frames->hierarchy,pending_runtime->effective_poses(),&*group_frames);apply_instance_ground(*pending_scene,document->loaded.scene,desired.instance_ground,nullptr,&bases);}
        applied_instance_ground=desired.instance_ground;
        const auto camera=window_->mailbox.latest();pending_scene->camera=render_camera(camera,window_->width,window_->height);
        city_runtime.bind(document->cities,*pending_scene);city_runtime.apply(*pending_scene,desired.city_views);state.city=city_runtime.stats;city_camera_epoch=camera.epoch;
        {const auto eye=camera.eye();water::materials(water::effective(*document,desired),*pending_scene);water::adapt(water::effective(*document,desired),*pending_scene,{eye.x,eye.y,eye.z});water_camera_epoch=camera.epoch;}
        cloud_runtime.apply(*document,cloud::effective(*document,desired),*pending_scene);
        cloud_physics_runtime.clear();
        camera_epoch=camera.epoch;camera_width=window_->width;camera_height=window_->height;
        set_quality(camera.navigating||now()<camera.preview_until);
        if(!session) {
        SceneParams scene_params;scene_params.background=false;scene_params.bvh_type=BVH_TYPE_DYNAMIC;
        scene_params.use_texture_cache=false;scene_params.auto_texture_cache=false;
        scene_params.texture_limit=sampling_.texture_limit;
        {diagnostics::Scope scope("session_create");session=std::make_unique<Session>(params,scene_params);}
        ++sessions;telemetry_.event("session_created");
        auto &scene=*session->scene;
        if(sampling_.rebuild_probe) scene.enable_update_stats();
        auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);
        scene.integrator->set_seed(1337);scene.integrator->set_max_bounce(8);scene.integrator->set_max_diffuse_bounce(4);
        scene.integrator->set_max_glossy_bounce(4);scene.integrator->set_max_transmission_bounce(8);scene.integrator->set_transparent_max_bounce(sampling_.transparent_bounces);
        scene.integrator->set_use_denoise(false);
        scene.integrator->set_use_adaptive_sampling(sampling_.adaptive_threshold>0);scene.integrator->set_adaptive_min_samples(32);scene.integrator->set_adaptive_threshold(sampling_.adaptive_threshold);
        scene.integrator->set_sampling_pattern(sampling_.blue_noise?SAMPLING_PATTERN_BLUE_NOISE_FIRST:SAMPLING_PATTERN_TABULATED_SOBOL);
        scene.integrator->set_min_bounce(sampling_.min_bounces);scene.integrator->set_transparent_min_bounce(sampling_.transparent_min_bounces);
        session->dfv_event=[&](const char *name,uint64_t epoch,double ms) {
#ifdef DFV_RENDER_INTERACTION_TEST
          // 仅 GPU 回归程序延长首帧场景锁，验证此时白模输入仍可推进。
          if(std::string_view(name)=="scene_data_dirty")std::this_thread::sleep_for(std::chrono::milliseconds(500));
#endif
          Frame frame;frame.epoch=epoch;frame.samples=session->dfv_render_samples.load();telemetry_.event(name,frame,ms);
          if(std::string_view(name)=="path_trace") {const auto *gpu=static_cast<GPUDevice *>(session->device.get());gpu_device_bytes=gpu->dfv_device_bytes();gpu_host_bytes=gpu->dfv_host_bytes();}
        };
          const auto begin=now();adapter=std::make_unique<CyclesAdapter>(*session->scene,false,sampling_.prune_hidden,sampling_);adapter->load(*pending_scene);timing("adapter_load",begin);
          auto driver=std::make_unique<Display>(*window_,telemetry_,session->dfv_render_epoch,session->dfv_render_samples,false);
          display=driver.get();display->set_options(desired.options);session->set_display_driver(std::move(driver));
          session->dfv_requested_epoch=++epoch;session->reset(params,buffers);session->start();
        } else {
          const auto begin=now();thread_scoped_lock lock(session->scene->mutex);
          const bool changed=adapter->synchronize(*pending_scene);timing("scene_incremental_sync",begin,desired.revision);display->set_options(desired.options);
          if(changed) {session->dfv_requested_epoch=++epoch;session->reset(params,buffers);}
        }
        // 新场景成功准备后才释放旧运行时及其文档；显示驱动继续保留有效帧。
        runtime.reset();render_scene_ptr=std::move(pending_scene);runtime=std::move(pending_runtime);current=document;pending_document.reset();
        refiner.cancel();refine_pending=false;refine_frames.clear();refined_collision=runtime->collision_stats();
        refine_enabled=refined_collision.evaluations||refined_collision.cache_hits;if(refine_enabled)runtime->defer_collision();
        runtime_document=document;runtime_source=group_source;runtime_frames=group_frames;
        regions.clear();regions.resize(render_scene_ptr->instances.size());pickable=scene_pick_mask(*current,desired,*render_scene_ptr);pick_revision=desired.revision;
        for(const auto &skin:current->skeletons.skins) regions.at(skin.instance)=runtime::joint_regions(render_scene_ptr->meshes.at(render_scene_ptr->instances.at(skin.instance).mesh),skin);
        geometry_dirty=true;edit_affects_render=false;previous_positions.clear();
        applied_revision=attempted_revision=desired.revision;
        gpu_revision=applied_revision;queued.clear();clay_wait=false;pose_recovery={};
        queued_subdivision_before.clear();
        {Frame f;f.epoch=epoch;f.id=applied_revision;f.width=buffers.width;f.height=buffers.height;telemetry_.event("edit_reset",f);}
        state={};state.city=city_runtime.stats;state.clicks=clicks;state.generation=current->generation;state.applied_revision=applied_revision;measured_evaluation=measured_skinning=measured_transform=UINT64_MAX;
      }
      auto &render_scene=*render_scene_ptr;
      if(physics_document!=current||!same_physics_input(physics_input,desired)||physics_options!=wanted_physics){
        ++physics_ticket;physics_service.retire(std::move(physics_pending));physics_input=desired;physics_document=current;physics_options=wanted_physics;physics_due=now();physics_requested=false;state.physics_error.clear();
        if(physics_prepared)retire_physics(std::move(physics_prepared));

      }
      if(physics_ready(physics_prepare)){
        try{auto p=physics_prepare.get();if(physics_prepare_ticket==physics_ticket.load())physics_prepared=std::move(p);else retire_physics(std::move(p));}catch(const std::exception &e){state.physics_error=e.what();}
      }
      if(physics_ready(physics_commit)){
        try{
          const bool committed=physics_commit.get();
          if(physics_commit_ticket==physics_ticket.load()&&physics_document==current){
            auto &p=*physics_installing;auto &r=*p.result;
            for(const auto &e:r.delta.meshes)render_scene.meshes[e.index].positions.swap(r.scene->meshes[e.index].positions);
            for(const auto &e:r.delta.instances)render_scene.instances[e.index].transform=e.transform;
            for(const auto &e:r.delta.materials)render_scene.materials[e.index]=e.value;
            cloud_runtime.clear();
            overlay.swap_delta(p.overlay,render_scene,r.delta);picking.swap_delta(p.picking,render_scene,r.delta);
            for(size_t k=0;k<p.targets.size();++k){auto i=p.targets[k];state.bounds[i]=p.bounds[k];state.head_bounds[i]=p.head_bounds[k];displacements[i]=p.displacements[k];}
            if(state.mesh_hashes.size()!=render_scene.meshes.size())state.mesh_hashes.resize(render_scene.meshes.size());for(auto [i,h]:p.hashes)state.mesh_hashes[i]=h;
            physics_service.acknowledge(p.result);
            state.physics=r.stats;state.physics_error=r.stats.warning;++state.physics_applied;retire_physics(std::move(physics_installing));gizmo_revision=UINT64_MAX;
            state.max_displacement=displacements.empty()?0:*std::max_element(displacements.begin(),displacements.end());
          }else{if(committed&&physics_installing)physics_service.acknowledge(physics_installing->result);retire_physics(std::move(physics_installing));}
        }catch(const std::exception &e){state.physics_error=e.what();retire_physics(std::move(physics_installing));}
      }
      if(!physics_requested&&now()>=physics_due&&!editing){
        const bool active=std::any_of(desired.values.begin(),desired.values.end(),[](const auto &v){return v.physics.enabled;});
        physics_request_serial=physics_service.request(active&&document_groups!=applied_groups?transformed_groups(group_source,applied_groups):current,desired,physics_options);physics_requested=true;
      }

      if(physics_requested&&!physics_pending&&!physics_prepared&&!physics_prepare.valid()&&!physics_commit.valid()){
        physics_service.request_frame();
        if(auto r=physics_service.take()){
          if(r->serial!=physics_request_serial){physics_service.retire(std::move(r));}
          else if(!r->error.empty())state.physics_error=r->error;
          else {state.physics=r->stats;state.physics_error=r->stats.warning;if(r->scene&&(!r->delta.meshes.empty()||!r->delta.instances.empty())){cloud_physics_runtime.apply(*current,cloud::effective(*current,desired),*r->scene,&r->delta);physics_pending=std::move(r);}}
          physics_service.retire(std::move(r));
        }
      }
      if(physics_pending&&!physics_prepared&&!physics_prepare.valid()&&!physics_commit.valid()){
        physics_prepare_ticket=physics_ticket;auto r=std::exchange(physics_pending,{});
        physics_prepare=std::async(std::launch::async,[this,r=std::move(r),current,regions,pickable]{return prepare_physics_present(r,current,*window_,regions,pickable);});
      }
      state.physics_busy=physics_service.busy()||physics_prepare.valid()||physics_commit.valid()||bool(physics_prepared);
      auto proxy_excluded=[&](int selected) {
        std::vector<uint32_t> result;if(selected==-4&&!gizmo_group.empty())return gizmo_group_members;if(selected<0||size_t(selected)>=current->catalog.targets.size())return result;
        const auto &targets=current->catalog.targets;auto node=[](const auto &t){return "#"+t.id.substr(0,t.id.rfind('/'));};std::set<std::string> family{node(targets[selected])};
        bool changed=true;while(changed){const auto before=family.size();for(const auto &t:targets)if(family.contains(t.parent)||family.contains(t.conform_target)||std::any_of(t.ancestors.begin(),t.ancestors.end(),[&](const auto &p){return family.contains(p);}))family.insert(node(t));changed=family.size()!=before;}
        for(const auto &t:targets)if(family.contains(node(t)))result.push_back(t.instance);return result;
      };
      if(pose_recovery.active&&pose_recovery.generation!=current->generation) pose_recovery.active=false;
      if(pose_recovery.active&&desired.revision>pose_recovery.revision&&applied_revision==desired.revision)pose_recovery.active=false;
      const auto pointer=window_->pose_pointer();
      const auto input_camera=window_->mailbox.latest();
      const float gizmo_dpi=GetDpiForWindow(window_->hwnd)/96.f;
      if(gizmo_consumed&&pointer.serial==gizmo_consumed) clicks=window_->clicks.load();
      if(gizmo_drag.active&&(current!=document||gizmo_drag.generation!=current->generation||gizmo_drag.revision!=desired.revision||pointer.cancelled||
        pointer.selection!=window_->pose_selection||pointer.serial!=gizmo_drag.press.serial||input_camera.epoch!=gizmo_drag.camera.epoch||
        width!=gizmo_drag.width||height!=gizmo_drag.height||gizmo_settings!=gizmo_drag.settings||powerpose.held)) {
        gizmo_drag.active=false;gizmo_valid=false;state.pose_dragging=false;gizmo_revision=UINT64_MAX;
      }
      if(!gizmo_drag.active&&!pose_recovery.active&&(gizmo_selection!=window_->pose_selection||gizmo_revision!=applied_revision||gizmo_drag.generation!=current->generation)) {
        gizmo_selection=window_->pose_selection;gizmo_revision=applied_revision;gizmo_valid=false;gizmo_light=-1;gizmo_group.clear();gizmo_group_members.clear();
        if(gizmo_settings.tool!=GizmoTool::select&&current==document&&desired.revision==applied_revision&&selection_generation==current->generation&&(selections.size()==1||!selected_group.empty())) {
          if(selected_target>=0&&size_t(selected_target)<current->catalog.targets.size()) {
            const auto &target=current->catalog.targets[selected_target];const auto &instance=render_scene.instances[target.instance];
            if(instance.visible) {
              const auto frame=group_target_frame(*group_source,size_t(selected_target),*group_frames);const auto loaded=group_frames->delta(group_frames->hierarchy.targets[selected_target])*group_source->loaded.scene.instances[target.instance].transform;
              gizmo_drag.object(frame,desired.values[selected_target].transform,loaded,instance.transform);gizmo_valid=selected_joint<0;
              if(selected_joint>=0) for(size_t s=0;s<current->skeletons.skins.size();++s) {const auto &skin=current->skeletons.skins[s];if(skin.instance==target.instance&&size_t(selected_joint)<skin.joints.size()) {gizmo_drag.bone(skin,selected_joint,desired.poses[s],runtime->effective_poses()[s],instance.transform);gizmo_drag.skin=int(s);gizmo_valid=true;break;}}
              gizmo_drag.target=selected_target;
            }
          } else if(selected_target==-4&&!selected_group.empty()&&group_node(*group_source,selected_group)) {
            const auto &node=*group_node(*group_source,selected_group);const auto found=desired.group_transforms.find(selected_group);const auto value=found==desired.group_transforms.end()?runtime::TransformValues{}:found->second;
            gizmo_drag.object(group_frame(node),value,node.world,group_world(*group_source,render_scene,selected_group,runtime->effective_poses(),&*group_frames));gizmo_drag.target=-4;gizmo_group=selected_group;gizmo_group_members=group_instances(*current,selected_group);gizmo_valid=true;
          } else if(!selections.empty()&&selections.front()[2]>=0&&size_t(selections.front()[2])<desired.lights.size()) {
            gizmo_light=selections.front()[2];runtime::Target frame;const auto &matrix=render_scene.lights[gizmo_light].transform;
            frame.has_edit_frame=true;frame.edit_frame=matrix;gizmo_drag.object(frame,{},matrix,matrix);gizmo_drag.target=-1;gizmo_valid=true;
          }
        }
        gizmo_drag.generation=current->generation;gizmo_drag.revision=applied_revision;
      }
      state.gizmo_available=gizmo_valid&&gizmo_settings.tool!=GizmoTool::select;
      if(gizmo_valid&&!gizmo_drag.active&&!pose_recovery.active) gizmo_drag.layout(input_camera,window_->width,window_->height,gizmo_settings,gizmo_dpi);
      if(pointer.serial!=gizmo_serial&&!pose_recovery.active&&desired.revision==applied_revision) {
        gizmo_serial=pointer.serial;
        if(gizmo_valid&&gizmo_settings.tool!=GizmoTool::select&&!pose_drag.active&&!powerpose_drag.active&&!pose_recovery.active&&
          pointer.held&&!pointer.cancelled&&pointer.selection==window_->pose_selection&&current==document&&desired.revision==applied_revision&&!input_camera.navigating) {
          ir::Mesh light_proxy;const ir::Mesh *mesh=&light_proxy;const std::vector<ir::Vec3> *source=nullptr;
          if(gizmo_drag.target>=0) {const auto &instance=render_scene.instances[current->catalog.targets[gizmo_drag.target].instance];mesh=&render_scene.meshes[instance.mesh];if(gizmo_drag.joint>=0) source=&runtime->skin_source(gizmo_drag.skin);}
          if(!gizmo_group.empty()){light_proxy=group_proxy(render_scene,gizmo_group_members,gizmo_drag.world);mesh=&light_proxy;}
          if(gizmo_drag.begin(pointer,input_camera,window_->width,window_->height,gizmo_settings,gizmo_dpi,*mesh,source)) gizmo_consumed=pointer.serial;
        }
      }
      if(gizmo_drag.active) {
        const auto begin=now();const bool updated=gizmo_drag.update(pointer);state.pose_gizmo=true;state.pose_powerpose=false;
        if(updated) {state.pose_solve_ms=(now()-begin)*1000;state.pose_previews=++pose_previews;telemetry_.event("gizmo_proxy_update",{},state.pose_solve_ms);}
        if(!pointer.held) {
          if(gizmo_drag.moved&&gizmo_drag.changed()&&!pointer.cancelled) {
            state.pose_commit=++pose_commits;state.pose_generation=gizmo_drag.generation;state.pose_revision=gizmo_drag.revision;state.pose_skin=gizmo_drag.skin;
            state.pose_joint=gizmo_drag.joint;state.pose_target=gizmo_drag.target;state.pose_light=gizmo_light;state.pose_group=gizmo_group;state.pose_figure=gizmo_drag.joint<0;
            state.pose_transform=gizmo_drag.transform;state.pose_input=gizmo_drag.poses;state.pose_error=state.pose_angle_error=0;
            if(gizmo_light>=0){const auto &id=source_lights.at(size_t(gizmo_light)).id;state.gizmo_light_transform=ir::inverse(group_member_parent_delta(*group_source,render_scene,group_frames->hierarchy,id,runtime->effective_poses(),&*group_frames)*group_frames->delta(id))*gizmo_drag.world;}
            if(sampling_.interaction_probe&&!gizmo_group.empty())std::ofstream(output_/("group-commit-"+std::to_string(pose_commits)+".json"))<<nlohmann::json{{"group",gizmo_group},{"world",gizmo_drag.world.value},{"pivot",{gizmo_drag.pivot().x,gizmo_drag.pivot().y,gizmo_drag.pivot().z}},{"translation_cm",{gizmo_drag.transform.translation_cm.x,gizmo_drag.transform.translation_cm.y,gizmo_drag.transform.translation_cm.z}},{"press",{gizmo_drag.press.start_x,gizmo_drag.press.start_y}},{"release",{pointer.x,pointer.y}}}.dump(2);
            pose_recovery={true,false,gizmo_drag.generation,gizmo_drag.revision,true};clay_wait=true;telemetry_.event("gizmo_commit");
          }
          gizmo_drag.active=false;state.pose_dragging=false;gizmo_revision=UINT64_MAX;
        } else if(gizmo_drag.moved) {
          if(!pose_paused) {session->set_pause(true);pose_paused=true;}
          GLContext::Binding binding(window_->present_context);glViewport(0,0,window_->width,window_->height);
          overlay.draw_pose(gizmo_drag.camera,window_->width,window_->height,gizmo_drag.proxy,gizmo_drag.world,{},gizmo_drag.pivot(),proxy_excluded(gizmo_drag.target));
          const auto shape=gizmo_shape(gizmo_drag.camera,window_->width,window_->height,gizmo_drag.pivot(),gizmo_drag.orientation(),gizmo_settings,gizmo_dpi,gizmo_drag.enabled);
          overlay.draw_gizmo(shape,window_->width,window_->height,gizmo_drag.handle,gizmo_dpi);state.gizmo_shape=shape;state.gizmo_pointer=pointer;
          window_->swap();binding.release();window_->check_graphics();state.pose_dragging=true;
          if(updated) {state.pose_latency_ms=(now()-pointer.input_seconds)*1000;telemetry_.event("gizmo_proxy_present",{},state.pose_latency_ms);}
          publish_state();std::this_thread::sleep_for(std::chrono::milliseconds(8));continue;
        }
      }
      if(powerpose_drag.active&&(powerpose_drag.press.generation!=current->generation||powerpose_drag.press.revision!=desired.revision||
        powerpose.cancelled||powerpose.serial!=powerpose_drag.press.serial||powerpose_selection!=window_->pose_selection||
        input_camera.epoch!=powerpose_drag.camera.epoch||window_->width!=powerpose_drag.width||window_->height!=powerpose_drag.height)) {
        powerpose_drag.active=false;state.pose_dragging=false;
      }
      if(powerpose.serial!=powerpose_serial&&(powerpose.moved||powerpose.cancelled)&&
         (powerpose.cancelled||powerpose.generation!=current->generation||powerpose.revision!=desired.revision||desired.revision==applied_revision)) {
        powerpose_serial=powerpose.serial;
        if(!gizmo_drag.active&&!pose_drag.active&&!powerpose.cancelled&&current==document&&powerpose.generation==current->generation&&
          powerpose.revision==desired.revision&&desired.revision==applied_revision&&powerpose.skin>=0&&powerpose.target>=0&&
          size_t(powerpose.skin)<current->skeletons.skins.size()&&size_t(powerpose.target)<current->catalog.targets.size()&&
          selection_generation==current->generation&&selected_target==powerpose.target&&!selections.empty()&&
          std::all_of(selections.begin(),selections.end(),[&](const auto &s){return s[0]==powerpose.target;})) {
          const auto &skin=current->skeletons.skins[powerpose.skin];const auto &target=current->catalog.targets[powerpose.target];
          if(skin.id==powerpose.instance&&skin.instance==target.instance) for(const auto &page:runtime::powerpose_templates()) for(const auto &point:page.points) if(point.id==powerpose.point) {
            const auto &instance=render_scene.instances[skin.instance];const auto &mesh=render_scene.meshes[instance.mesh];
            const auto frame=group_target_frame(*group_source,size_t(powerpose.target),*group_frames);const auto loaded=group_frames->delta(group_frames->hierarchy.targets[powerpose.target])*group_source->loaded.scene.instances[skin.instance].transform;
            powerpose_drag.begin(skin,mesh,runtime->skin_source(powerpose.skin),desired.poses[powerpose.skin],runtime->effective_poses()[powerpose.skin],instance.transform,
              desired.values[powerpose.target].transform,runtime::bind_powerpose(skin,point),powerpose,input_camera,window_->width,window_->height,runtime::pin_goals(pose_pins,powerpose.skin,instance.transform),&frame,loaded);
            powerpose_selection=window_->pose_selection;
            if(sampling_.interaction_probe&&powerpose_drag.active) {powerpose_reference.clear();for(const auto &m:render_scene.meshes) powerpose_reference.push_back(m.positions);state.pose_restore_max_error=0;}
          }
        }
      }
      if(powerpose_drag.active) {
        state.pose_gizmo=false;
        const auto &skin=current->skeletons.skins[powerpose.skin];const auto begin=now();const bool updated=powerpose_drag.update(skin,powerpose);
        if(updated) {state.pose_solve_ms=(now()-begin)*1000;state.pose_error=powerpose_drag.result.error;state.pose_angle_error=powerpose_drag.result.angle_error_degrees;state.pose_previews=++pose_previews;telemetry_.event("powerpose_proxy_update",{},state.pose_solve_ms);}
        if(!powerpose.held) {
          if(powerpose_drag.moved&&powerpose_drag.changed()&&!powerpose.cancelled) {
            auto poses=desired.poses;state.pose_input=powerpose_drag.commit(skin,[&](const auto &input){poses[powerpose.skin]=input;return runtime->resolve_poses(desired.values,poses)[powerpose.skin];});
            state.pose_commit=++pose_commits;state.pose_generation=powerpose.generation;state.pose_revision=powerpose.revision;state.pose_skin=powerpose.skin;state.pose_joint=powerpose_drag.bound.joints.empty()?-1:powerpose_drag.bound.joints.front();
            state.pose_gizmo=false;state.pose_powerpose=true;state.pose_figure=powerpose_drag.bound.figure;state.pose_target=powerpose.target;state.pose_transform=powerpose_drag.transform;
            state.pose_error=powerpose_drag.result.error;state.pose_angle_error=powerpose_drag.result.angle_error_degrees;
            pose_recovery={true,true,powerpose.generation,powerpose.revision};clay_wait=true;telemetry_.event("powerpose_commit");
          }
          powerpose_drag.active=false;state.pose_dragging=false;
        } else if(powerpose_drag.moved) {
          if(!pose_paused) {session->set_pause(true);pose_paused=true;telemetry_.event("powerpose_preview_begin");}
          GLContext::Binding binding(window_->present_context);glViewport(0,0,window_->width,window_->height);
          const auto goal=powerpose_drag.bones.empty()?powerpose_drag.world.point({}):powerpose_drag.bones.front().second;
          overlay.draw_pose(powerpose_drag.camera,window_->width,window_->height,powerpose_drag.proxy,powerpose_drag.world,powerpose_drag.bones,goal,proxy_excluded(powerpose_drag.press.target));
          window_->swap();binding.release();window_->check_graphics();state.pose_dragging=true;state.pose_powerpose=true;state.pose_skin=powerpose.skin;
          if(updated) {state.pose_latency_ms=(now()-powerpose.input_seconds)*1000;telemetry_.event("powerpose_proxy_present",{},state.pose_latency_ms);}
          publish_state();std::this_thread::sleep_for(std::chrono::milliseconds(8));continue;
        }
      }
      if(pose_drag.active&&(pose_drag.generation!=current->generation||pose_drag.revision!=desired.revision||pointer.cancelled||pointer.selection!=window_->pose_selection||pointer.serial!=pose_drag.press.serial||input_camera.epoch!=pose_drag.camera().epoch||window_->width!=state.width||window_->height!=state.height)) {
        pose_drag.active=false;state.pose_dragging=false;clicks=window_->clicks.load();
      }
      if(pointer.serial!=pose_serial&&!pose_recovery.active&&desired.revision==applied_revision) {
        pose_serial=pointer.serial;
        // 只使用按下时已经存在的单选身份；首次选中、Ctrl 多选和过期场景不能启动 IK。
        if(!gizmo_drag.active&&gizmo_consumed!=pointer.serial&&!powerpose_drag.active&&!pose_recovery.active&&ik_allowed&&pointer.held&&!pointer.cancelled&&!geometry_dirty&&current==document&&desired.revision==applied_revision&&
           selection_generation==current->generation&&pointer.selection==window_->pose_selection&&(clay_wait||(telemetry_.displayed_epoch.load()>=epoch&&!preview))&&selections.size()==1) {
          const auto hit=picking.screen(input_camera,pointer.start_x,pointer.start_y,window_->width,window_->height);
          int hit_target=-1;for(size_t t=0;t<current->catalog.targets.size();++t) if(int(current->catalog.targets[t].instance)==hit.instance) hit_target=int(t);
          int skin_index=-1;for(size_t s=0;s<current->skeletons.skins.size();++s) if(int(current->skeletons.skins[s].instance)==hit.instance) skin_index=int(s);
          bool host=false;if(hit_target>=0) try {host=attachment_host(*current,size_t(hit_target))==size_t(hit_target);} catch(const std::exception &) {}
          if(runtime::ik_selection_allowed(selections.size(),selections.front()[0],hit_target,pointer.modified,skin_index>=0&&host)) {
            const auto &skin=current->skeletons.skins[size_t(skin_index)];const auto &instance=render_scene.instances[skin.instance];const auto &mesh=render_scene.meshes[instance.mesh];
            const auto region=runtime::hover_region(hit,int(skin.instance),selections.front()[1],regions);
            const int joint=region.joint>=0?region.joint:runtime::hit_joint(mesh,hit.triangle,skin);
            auto pins=runtime::pin_goals(pose_pins,skin_index,instance.transform);
            if(joint>=0) {pose_drag.begin(skin,mesh,runtime->skin_source(size_t(skin_index)),desired.poses[size_t(skin_index)],runtime->effective_poses()[size_t(skin_index)],instance.transform,joint,pointer,input_camera,window_->width,window_->height,std::move(pins));
              pose_drag.generation=current->generation;pose_drag.revision=desired.revision;pose_drag.skin=skin_index;pose_drag.target=hit_target;
            }
          }
        }
      }
      if(pose_drag.active) {
        state.pose_gizmo=false;state.pose_powerpose=false;
        const auto begin=now();const bool updated=pose_drag.update(pointer);
        if(updated) {state.pose_solve_ms=(now()-begin)*1000;state.pose_error=pose_drag.result.error;state.pose_angle_error=pose_drag.result.angle_error_degrees;state.pose_previews=++pose_previews;telemetry_.event("ik_proxy_update",{},state.pose_solve_ms);}
        if(!pointer.held) {
          if(pose_drag.moved&&!pointer.cancelled) {
            const auto finalize_begin=now();auto pose_inputs=desired.poses;
            state.pose_input=pose_drag.commit(current->skeletons.skins.at(pose_drag.skin),[&](const auto &input){pose_inputs.at(pose_drag.skin)=input;return runtime->resolve_poses(desired.values,pose_inputs).at(pose_drag.skin);});
            timing("ik_constraint_finalize",finalize_begin,pose_drag.revision);state.pose_error=pose_drag.result.error;state.pose_angle_error=pose_drag.result.angle_error_degrees;
            state.pose_commit=++pose_commits;state.pose_generation=pose_drag.generation;state.pose_revision=pose_drag.revision;state.pose_skin=pose_drag.skin;state.pose_joint=pose_drag.joint;
            state.pose_gizmo=false;state.pose_powerpose=false;state.pose_figure=false;
            clicks=window_->clicks.load();telemetry_.event("ik_commit");
            pose_recovery={true,false,pose_drag.generation,pose_drag.revision};clay_wait=true;
          }
          pose_drag.active=false;state.pose_dragging=false;
        } else if(pose_drag.moved) {
          if(!pose_paused) {session->set_pause(true);pose_paused=true;telemetry_.event("ik_preview_begin");}
          GLContext::Binding binding(window_->present_context);glViewport(0,0,window_->width,window_->height);
          overlay.draw_pose(pose_drag.camera(),window_->width,window_->height,pose_drag.proxy,pose_drag.world(),pose_drag.bones,pose_drag.world().point(pose_drag.goal.position),proxy_excluded(pose_drag.target));
          window_->swap();binding.release();window_->check_graphics();state.pose_dragging=true;state.pose_joint=pose_drag.joint;state.pose_skin=pose_drag.skin;
          if(updated) {state.pose_latency_ms=(now()-pointer.input_seconds)*1000;telemetry_.event("ik_proxy_present",{},state.pose_latency_ms);}
          publish_state();std::this_thread::sleep_for(std::chrono::milliseconds(8));continue;
        }
      }
      if(pose_paused) {session->set_pause(false);pose_paused=false;telemetry_.event("ik_preview_end");}
      if(session->progress.get_error()) throw std::runtime_error(session->progress.get_error_message());
      std::string progress,detail;session->progress.get_status(progress,detail);
      progress+=" | "+detail;
      if(progress!=last_progress) {progress_log<<now()<<" "<<progress<<std::endl;last_progress=progress;}
      if(display->failed()) throw GraphicsError(display->error());
      auto camera=window_->mailbox.latest();
      const bool size_changed=camera_width!=window_->width||camera_height!=window_->height;
      const bool edit_pending=desired.generation==current->generation&&desired.revision!=attempted_revision;
      const bool navigation_preview=camera.needs_preview(camera_epoch,now())||size_changed||now()<resize_until;
      bool wanted_preview=navigation_preview||(!clay_wait&&!pose_recovery.active&&((edit_affects_render&&(editing||now()<preview_until))||
        (edit_pending&&!state.pending_payloads&&state.resource_error.empty())));
      if(clay_wait)wanted_preview=false;
      const bool resolution_changed=quality.percent!=applied_percent;
      const bool quality_changed=wanted_preview!=preview||resolution_changed;
      ir::Delta delta;bool new_render_edit=false,subdivision_edit=false,material_layout_edit=false,refinement_finished=false;
      const bool city_pending=!current->cities.empty()&&!navigation_preview&&city_camera_epoch!=camera.epoch;
      const bool water_pending=!current->waters.empty()&&!navigation_preview&&water_camera_epoch!=camera.epoch;
      std::vector<ir::SubdivisionSettings> previous_subdivision;
      // 进入预览时允许取消尚未出图的完整渲染；预览之间仍等待出图，防止连续输入饿死渲染。
      if((quality_changed || size_changed || camera.epoch!=camera_epoch || edit_pending || city_pending || water_pending) &&
         (edit_pending||clay_wait||resolution_changed||(wanted_preview&&!preview)||((telemetry_.displayed_epoch.load()>=epoch||
           (pose_recovery.active&&display->drawn_frame().epoch>=epoch))&&session->ready_to_reset()))) {
        if(desired.generation==current->generation && desired.revision!=attempted_revision) {
          try {
            const auto prepare_begin=now();
            const auto resources=runtime->prepare(desired.values,desired.poses,retry_payloads);
            timing("edit_prepare",prepare_begin,desired.revision);
            retried=retry;
            state.pending_payloads=resources.pending;state.resource_error=resources.error;
            if(!resources.pending&&resources.error.empty()) {
            attempted_revision=desired.revision;const auto evaluate_begin=now();
            for(const auto &mesh:render_scene.meshes) previous_subdivision.push_back(mesh.subdivision);
            subdivision_edit=apply_subdivision_levels(render_scene,desired.subdivision_levels);
            diagnostics::LoadProfile profile;diagnostics::active=&profile;
            try {delta=runtime->evaluate(scene_properties(*current,desired),desired.poses);apply_instance_node_visibility(*current,desired,render_scene,&delta);} catch(...) {diagnostics::active=nullptr;throw;}
            diagnostics::active=nullptr;timing("edit_evaluate",evaluate_begin,desired.revision);
            if(refine_enabled&&(refine_pending||!delta.meshes.empty()||!delta.instances.empty()||!delta.visibility.empty()||!delta.grafts.empty()||!delta.masks.empty())) {
              refine_ticket=refiner.request(runtime_document,desired,refine_frames);refine_revision=desired.revision;refine_pending=true;
            }
            for(const auto &[name,value]:profile.timings) {Frame f;f.id=desired.revision;telemetry_.event(name.c_str(),f,value.seconds*1000);}
            if(!previous_positions.empty()) {
              std::erase_if(delta.meshes,[&](const auto &edit) {const auto &old=previous_positions.at(edit.index);return old.size()==edit.positions.size()&&std::equal(old.begin(),old.end(),edit.positions.begin(),[](auto a,auto b){return a.x==b.x&&a.y==b.y&&a.z==b.z;});});previous_positions.clear();
            }
            auto lights=group_lights(*group_source,render_scene,group_frames->hierarchy,desired.lights,runtime->effective_poses(),&*group_frames);
            for(size_t l=0;l<lights.size();++l) {
              const auto &a=lights[l],&b=render_scene.lights[l];
              if(a.transform.value!=b.transform.value||a.power.x!=b.power.x||a.power.y!=b.power.y||a.power.z!=b.power.z||a.width!=b.width||a.height!=b.height) delta.lights.push_back({uint32_t(l),a});
            }
            render_scene.lights=std::move(lights);
            if(render_scene.options!=desired.options) {
              if(render_scene.options.environment!=desired.options.environment||render_scene.options.environment_file!=desired.options.environment_file||render_scene.options.backdrop!=desired.options.backdrop) delta.options=desired.options;
              render_scene.options=desired.options;display->set_options(desired.options);
            }
            material_layout_edit=apply_material_overrides(render_scene,current->loaded.scene,desired.material_overrides,&delta);
            water::materials(water::effective(*current,desired),render_scene,&delta);
            if(desired.instance_ground!=applied_instance_ground||!group_frames->hierarchy.groups.empty()){const auto bases=group_instance_bases(*group_source,render_scene,group_frames->hierarchy,runtime->effective_poses(),&*group_frames);apply_instance_ground(render_scene,current->loaded.scene,desired.instance_ground,&delta,&bases);applied_instance_ground=desired.instance_ground;}
            cloud_runtime.apply(*current,cloud::effective(*current,desired),render_scene,&delta);
            new_render_edit=material_layout_edit||!delta.materials.empty()||subdivision_edit||delta.options||!delta.meshes.empty()||!delta.instances.empty()||!delta.visibility.empty()||!delta.lights.empty()||!delta.grafts.empty()||!delta.masks.empty();
            edit_affects_render=new_render_edit;applied_revision=desired.revision;if(!new_render_edit&&!queued.pending)gpu_revision=applied_revision;state.edit_error.clear();}
          }
          catch(const std::exception &e) {
            for(size_t m=0;m<previous_subdivision.size();++m) render_scene.meshes[m].subdivision=previous_subdivision[m];
            subdivision_edit=false;attempted_revision=desired.revision;state.edit_error=e.what();
          }
        }
        if(camera.epoch!=camera_epoch||size_changed) {delta.camera=render_camera(camera,window_->width,window_->height);render_scene.camera=*delta.camera;camera_epoch=camera.epoch;camera_width=window_->width;camera_height=window_->height;}
        if(!current->cities.empty()&&(edit_pending||(!navigation_preview&&(city_pending||size_changed)))){const auto count=delta.visibility.size();material_layout_edit=city_runtime.apply(render_scene,desired.city_views,&delta)||material_layout_edit;state.city=city_runtime.stats;city_camera_epoch=camera.epoch;new_render_edit=new_render_edit||material_layout_edit||count!=delta.visibility.size();}
        if(!current->waters.empty()&&!navigation_preview&&(water_pending||edit_pending)){
          const auto eye=camera.eye();const auto begin=now();if(water::adapt(water::effective(*current,desired),render_scene,{eye.x,eye.y,eye.z})){
            new_render_edit=true;
            for(const auto &i:render_scene.instances)if(water::find(current->waters,i.id)){std::erase_if(delta.meshes,[&](const auto &e){return e.index==i.mesh;});delta.meshes.push_back({i.mesh,render_scene.meshes[i.mesh].positions});if(std::find(queued.water_meshes.begin(),queued.water_meshes.end(),i.mesh)==queued.water_meshes.end())queued.water_meshes.push_back(i.mesh);}
            timing("water_lod",begin,applied_revision);
          }water_camera_epoch=camera.epoch;
        }
        // 色调等仅影响显示的编辑保留累计采样；真实场景修改才启动编辑预览。
        // 相机触发的水体 LOD／城市显隐更新仍属于导航，等待期间保留光追预览。
        // 用户编辑、姿态恢复和物理更新继续使用原有白模流程。
        const bool camera_only_update=!edit_pending&&!editing&&!pose_recovery.active&&desired.revision==gpu_revision&&!clay_wait;
        wanted_preview=navigation_preview||(!clay_wait&&!pose_recovery.active&&edit_affects_render&&(editing||now()<preview_until||(!camera_only_update&&new_render_edit)));
        if(!camera_only_update&&(!delta.meshes.empty()||!delta.instances.empty()||!delta.visibility.empty()||!delta.grafts.empty()||!delta.masks.empty()))clay_wait=true;
        if(clay_wait)wanted_preview=false;
        if(subdivision_edit&&queued_subdivision_before.empty())queued_subdivision_before=previous_subdivision;
        if(wanted_preview!=preview||resolution_changed||new_render_edit||delta.camera)queued.merge(delta,subdivision_edit||material_layout_edit);
        state.applied_revision=applied_revision;
      }
      // Apply only the current snapshot's complete deformation. Preview remains
      // editable while the independent CPU worker resolves clothing collisions.
      if(refine_pending&&refine_revision==desired.revision)if(auto result=refiner.take();result&&result->ticket==refine_ticket) {
        if(!result->error.empty()){state.edit_error=result->error;refine_pending=false;queued.clear();}
        else {
          for(auto &edit:result->geometry.meshes)if(render_scene.meshes.at(edit.index).positions!=edit.positions) {
            render_scene.meshes[edit.index].positions=edit.positions;
            auto found=std::find_if(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e){return e.index==edit.index;});
            if(found==delta.meshes.end())delta.meshes.push_back(std::move(edit));else *found=std::move(edit);
          }
          for(const auto &edit:result->geometry.instances)if(render_scene.instances.at(edit.index).transform!=edit.transform) {
            render_scene.instances[edit.index].transform=edit.transform;
            auto found=std::find_if(delta.instances.begin(),delta.instances.end(),[&](const auto &e){return e.index==edit.index;});
            if(found==delta.instances.end())delta.instances.push_back(edit);else *found=edit;
          }
          if(!desired.instance_ground.empty()||!group_frames->hierarchy.groups.empty()) {
            const auto bases=group_instance_bases(*group_source,render_scene,group_frames->hierarchy,runtime->effective_poses(),&*group_frames);
            apply_instance_ground(render_scene,current->loaded.scene,desired.instance_ground,&delta,&bases);
          }
          cloud_runtime.apply(*current,cloud::effective(*current,desired),render_scene,&delta);
          refined_collision=result->collision;refine_pending=false;refinement_finished=true;gizmo_revision=UINT64_MAX;queued.merge(delta);clay_wait=true;
          Frame f;f.id=refine_revision;telemetry_.event("edit_refine",f,result->seconds*1000);
          for(const auto &[name,value]:result->profile.timings)telemetry_.event(("refine/"+name).c_str(),f,value.seconds*1000);
        }
      }
      // 拖动期间只更新白模；松手后尝试提交，锁繁忙时保留最新 Delta，下一轮继续处理输入。
      if(!refine_pending&&!physics_commit.valid()&&queued.pending&&!(clay_wait&&editing)) {
        thread_scoped_lock lock(session->scene->mutex,std::try_to_lock);
        if(lock.owns_lock()) {
          const auto begin=now();if(!queued.synchronize&&!queued.water_meshes.empty()){
            if(!adapter->update_water_meshes(render_scene,queued.water_meshes))queued.synchronize=true;
            else std::erase_if(queued.delta.meshes,[&](const auto &e){return std::find(queued.water_meshes.begin(),queued.water_meshes.end(),e.index)!=queued.water_meshes.end();});
          }
          if(queued.synchronize){
            try{adapter->synchronize(render_scene);}catch(const std::exception &e){for(size_t m=0;m<queued_subdivision_before.size();++m)render_scene.meshes[m].subdivision=queued_subdivision_before[m];state.edit_error=e.what();adapter->apply(queued.delta);}
            ir::Delta camera_only;camera_only.camera=queued.delta.camera;adapter->apply(camera_only);
          }else adapter->apply(queued.delta);
          timing("adapter_apply",begin,applied_revision);set_quality(clay_wait?false:wanted_preview);session->dfv_requested_epoch=++epoch;session->reset(params,buffers);gpu_revision=applied_revision;queued.clear();
          queued_subdivision_before.clear();
          Frame f;f.epoch=epoch;f.id=gpu_revision;f.width=buffers.width;f.height=buffers.height;telemetry_.event("edit_reset",f);
        }
      }
      RenderProbe probe;{std::lock_guard lock(mutex_);probe=probe_;}
      std::unique_ptr<WaterRequest> water_request;
      {std::lock_guard lock(mutex_);if(water_request_&&(water_request_->generation!=current->generation||water_request_->revision<=applied_revision))water_request=std::move(water_request_);}
      if(water_request){try{
        if(water_request->generation!=current->generation||water_request->revision!=applied_revision)throw std::runtime_error("海岸线输入已改变，请重算");
        const auto begin=now();water_request->result.set_value(water::inputs(*current,render_scene,water_request->water));timing("water_inputs",begin,applied_revision);
      }catch(...){water_request->result.set_exception(std::current_exception());}}
      if(!physics_commit.valid()&&probe.serial!=state.probe_serial&&session->ready_to_reset()) {
        ir::Delta experiment;
        for(uint32_t i=0;i<render_scene.materials.size();++i) {
          auto material=render_scene.materials[i];
          if(probe.disable_sss) {material.subsurface=0;if(!material.thin_walled) material.translucency_texture=-1;}
          if(probe.disable_bump) {material.bump_texture=-1;material.normal_texture=-1;}
          experiment.materials.push_back({i,material});
        }
        for(uint32_t i=0;i<render_scene.instances.size();++i) {
          const auto &instance=render_scene.instances[i];
          experiment.visibility.push_back({i,instance.visible&&std::find(probe.hidden.begin(),probe.hidden.end(),instance.id)==probe.hidden.end()});
        }
        {thread_scoped_lock lock(session->scene->mutex);adapter->apply(experiment);
          session->scene->integrator->set_transparent_max_bounce(probe.transparent_bounces);
          session->scene->integrator->tag_update(session->scene.get(),Integrator::UPDATE_ALL);
          session->dfv_requested_epoch=++epoch;session->reset(params,buffers);}
        state.probe_serial=probe.serial;Frame f;f.id=probe.serial;f.epoch=epoch;telemetry_.event("render_probe",f);
      }
      if(!physics_commit.valid()&&physics_prepared&&now()-physics_last_commit>=1.0/physics_options.refresh_hz&&display->drawn_frame().id!=physics_last_render_frame&&!queued.pending&&!editing&&!navigation_preview&&!pose_recovery.active&&!pose_drag.active&&!powerpose_drag.active&&!gizmo_drag.active&&!clay_wait&&desired.revision==applied_revision){
        physics_last_commit=now();physics_last_render_frame=display->drawn_frame().id;
        physics_installing=std::move(physics_prepared);physics_commit_ticket=physics_ticket;const auto ticket=physics_commit_ticket;const auto requested=++epoch;
        auto *engine=session.get();auto *bridge=adapter.get();auto result=physics_installing->result;auto settings=params;auto dimensions=buffers;
        physics_commit=std::async(std::launch::async,[engine,bridge,result,settings,dimensions,requested,ticket,&physics_ticket]{
          SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);thread_scoped_lock lock(engine->scene->mutex);
#ifdef DFV_PHYSICS_TEST
          // 延长持锁的提交阶段，验收相机及新编辑仍能立即响应。
          std::this_thread::sleep_for(std::chrono::milliseconds(600));
#endif
          const bool committed=ticket==physics_ticket.load();if(committed)bridge->apply(result->delta);
          // 即使取消也完成已预留的显示代次，避免同几何场景切换永远等待旧帧。
          engine->dfv_requested_epoch=requested;engine->reset(settings,dimensions);return committed;
        });
        physics_display_wait=true;physics_display_camera=camera.epoch;physics_display_ticket=ticket;
        clay_wait=true;state.physics_busy=true;publish_state();continue;
      }
      GLContext::Binding binding(window_->present_context);
      display->set_reconstruction(quality.reconstruction);
      display->set_sharpen(quality.sharpen);
      const bool bounds_dirty=geometry_dirty||!delta.meshes.empty()||!delta.instances.empty()||!delta.visibility.empty()||!delta.masks.empty();
      bool picking_changed=false;if(pick_revision!=desired.revision){auto next_pickable=scene_pick_mask(*current,desired,render_scene);picking_changed=next_pickable!=pickable;pickable=std::move(next_pickable);pick_revision=desired.revision;}
      if(geometry_dirty) {instance_groups.emplace(render_scene);auto begin=now();overlay.update(render_scene,regions);timing("overlay_update",begin,applied_revision);begin=now();picking.update(render_scene,pickable);timing("picking_update",begin,applied_revision);geometry_dirty=false;}
      else if(bounds_dirty||refinement_finished) {auto begin=now();overlay.apply(render_scene,regions,delta,refine_pending);timing("overlay_update",begin,applied_revision);begin=now();picking.apply(render_scene,delta);timing("picking_update",begin,applied_revision);}
      if(picking_changed)picking.set_pickable(render_scene,pickable);
      if(sampling_.interaction_probe&&bounds_dirty) {
        const bool all=state.mesh_hashes.size()!=render_scene.meshes.size();state.mesh_hashes.resize(render_scene.meshes.size());
        for(size_t m=0;m<render_scene.meshes.size();++m) if(all||std::any_of(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e){return e.index==m;})) {
          uint64_t hash=14695981039346656037ull;for(auto p:render_scene.meshes[m].positions) for(float v:{p.x,p.y,p.z}) {hash^=std::bit_cast<uint32_t>(v);hash*=1099511628211ull;}state.mesh_hashes[m]=hash;
        }
        state.instance_transforms.clear();for(const auto &instance:render_scene.instances) state.instance_transforms.push_back(instance.transform.value);
        if(!powerpose_reference.empty()) {
          double maximum=0;
          if(powerpose_reference.size()!=render_scene.meshes.size()) maximum=std::numeric_limits<double>::infinity();
          else for(size_t m=0;m<powerpose_reference.size();++m) {const auto &before=powerpose_reference[m],&after=render_scene.meshes[m].positions;
            if(before.size()!=after.size()) {maximum=std::numeric_limits<double>::infinity();break;}
            for(size_t v=0;v<before.size();++v) {const double x=double(before[v].x)-after[v].x,y=double(before[v].y)-after[v].y,z=double(before[v].z)-after[v].z;maximum=std::max(maximum,x*x+y*y+z*z);}
          }
          state.pose_restore_max_error=std::sqrt(maximum);
        }
      }
      glViewport(0,0,window_->width,window_->height);glClearColor(.035f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);
      if(!physics_commit.valid())session->draw();
      else display->draw({}); // 只访问三缓冲显示邮箱，不等待 Cycles 的场景锁。
      const auto beauty=display->drawn_frame();
      if(beauty.id&&beauty.epoch>=epoch&&camera.epoch==camera_epoch&&!queued.pending&&!editing&&!preview&&!pose_recovery.active&&!clay_wait&&applied_revision==desired.revision)
        capture_frame(current->generation,applied_revision,camera);
      if(pose_recovery.active&&desired.revision>pose_recovery.revision&&applied_revision==desired.revision)pose_recovery.active=false;
      if(!physics_commit.valid()&&clay_wait&&!editing&&!pose_recovery.active&&!queued.pending&&gpu_revision==desired.revision&&beauty.epoch>=epoch){std::lock_guard lock(mutex_);if(snapshot_.revision==desired.revision&&snapshot_.generation==desired.generation&&!edit_active_)clay_wait=false;}
      state.pose_restoring=pose_recovery.active||clay_wait;
      if(!clay_wait||camera.epoch!=physics_display_camera||physics_ticket.load()!=physics_display_ticket||editing)physics_display_wait=false;
      // 仅物理更新时保留上一张完整画面，避免持续模拟造成白模闪烁；操作相机立即使用交互预览。
      if(clay_wait&&!pose_recovery.active&&!(physics_display_wait&&beauty.id)){overlay.draw_pose(camera,window_->width,window_->height,{}, {},{},{});++state.clay_presentations;Frame f;f.id=applied_revision;telemetry_.event("edit_clay_present",f);}
      // 松手后的等待使用本次手势的白模和输入版本，不能借用另一种工具的旧状态。
      if(pose_recovery.active) {
        if(pose_recovery.gizmo) {
          overlay.draw_pose(gizmo_drag.camera,window_->width,window_->height,gizmo_drag.proxy,gizmo_drag.world,{},gizmo_drag.pivot(),proxy_excluded(gizmo_drag.target));
          overlay.draw_gizmo(gizmo_shape(gizmo_drag.camera,window_->width,window_->height,gizmo_drag.pivot(),gizmo_drag.orientation(),gizmo_drag.settings,gizmo_dpi,gizmo_drag.enabled),window_->width,window_->height,gizmo_drag.handle,gizmo_dpi);
        } else if(pose_recovery.powerpose) {
          const auto goal=powerpose_drag.bones.empty()?powerpose_drag.world.point({}):powerpose_drag.bones.front().second;
          overlay.draw_pose(powerpose_drag.camera,window_->width,window_->height,powerpose_drag.proxy,powerpose_drag.world,powerpose_drag.bones,goal,proxy_excluded(powerpose_drag.press.target));
        } else overlay.draw_pose(pose_drag.camera(),window_->width,window_->height,pose_drag.proxy,pose_drag.world(),pose_drag.bones,pose_drag.world().point(pose_drag.goal.position),proxy_excluded(pose_drag.target));
      }
      state.camera=camera;state.pointer_x=window_->pointer_x;state.pointer_y=window_->pointer_y;
      auto hover=preview&&!clay_wait?runtime::PickHit{}:picking.screen(camera,state.pointer_x,state.pointer_y,window_->width,window_->height);
      bool editable=hover.instance>=0&&render_scene.instances.at(size_t(hover.instance)).prototype>=0;for(const auto &target:current->catalog.targets) if(int(target.instance)==hover.instance) editable=true;
      if(!editable) hover={};
      state.hovered_detail_joint=hover.instance>=0&&hover.triangle>=0&&size_t(hover.triangle)<regions[size_t(hover.instance)].detail.size()?regions[size_t(hover.instance)].detail[size_t(hover.triangle)]:-1;
      const bool selection_dirty=state.selection_generation!=selection_generation||state.selected_target!=selected_target||state.selected_joint!=selected_joint||state.selections!=selections;
      state.selection_generation=selection_generation;state.selected_target=selection_generation==current->generation?selected_target:-1;
      state.selected_joint=state.selected_target>=0?selected_joint:-1;state.selections=selection_generation==current->generation?selections:std::vector<Selection>{};
      const int instance_key=state.selected_target>=0&&size_t(state.selected_target)<current->catalog.targets.size()?int(current->catalog.targets[size_t(state.selected_target)].instance):state.selected_target<=-5?-5-state.selected_target:-1;
      const int selected_instance=instance_key>=0&&size_t(instance_key)<render_scene.instances.size()?instance_key:-1;
      state.focus_requests=window_->focus_requests;
      state.ground_requests=window_->ground_requests;
      if(bounds_dirty||selection_dirty) {state.selection_bounds={};
      for(const auto &selection:state.selections) {
        if(selection[2]>=0&&size_t(selection[2])<render_scene.lights.size()) {
          const auto &m=render_scene.lights[size_t(selection[2])].transform;
          for(int x:{-1,1}) for(int y:{-1,1}) for(int z:{-1,1}) state.selection_bounds.add(m.point({x*.1f,y*.1f,z*.1f}));continue;
        }
        const int key=selection[0];const int selected_joint=selection[1];
        const int selected_instance=key>=0&&size_t(key)<current->catalog.targets.size()?int(current->catalog.targets[size_t(key)].instance):key<=-5?-5-key:-1;
        if(selected_instance<0||size_t(selected_instance)>=render_scene.instances.size()) continue;
        ir::Bounds member_bounds;
      {
        const auto &instance=render_scene.instances.at(size_t(selected_instance));const auto &mesh=render_scene.meshes.at(instance.mesh);
        if(selected_joint<0) member_bounds=instance_groups->bounds(render_scene,uint32_t(selected_instance));
        else {
          const auto &r=regions.at(size_t(selected_instance));
          const runtime::Skin *skin=nullptr;for(const auto &s:current->skeletons.skins) if(s.instance==uint32_t(selected_instance)) {skin=&s;break;}
          member_bounds=runtime::joint_region_bounds(mesh,instance.transform,r,selected_joint,skin);
        }
        if(member_bounds.empty&&selected_joint>=0) for(size_t s=0;s<current->skeletons.skins.size();++s) {
          const auto &skin=current->skeletons.skins[s];if(skin.instance!=uint32_t(selected_instance)||size_t(selected_joint)>=skin.joints.size()) continue;
          const auto &pose=runtime->effective_poses()[s];const auto &j=skin.joints[size_t(selected_joint)];const auto &p=pose[size_t(selected_joint)];const auto matrices=runtime::joint_transforms(skin,pose);
          auto center=matrices[size_t(selected_joint)].point({(j.center_cm.x+p.center_offset_cm.x)*.01f,-(j.center_cm.z+p.center_offset_cm.z)*.01f,(j.center_cm.y+p.center_offset_cm.y)*.01f});center=instance.transform.point(center);
          member_bounds.add({center.x-.01f,center.y-.01f,center.z-.01f});member_bounds.add({center.x+.01f,center.y+.01f,center.z+.01f});break;
        }
      }
      if(!member_bounds.empty) {state.selection_bounds.add(member_bounds.minimum);state.selection_bounds.add(member_bounds.maximum);}
      }
      }
      std::vector<runtime::HoverRegion> selected_regions;
      for(const auto &selection:state.selections) {
        const int target=selection[0];
        if(selection[2]<0&&target>=0&&size_t(target)<current->catalog.targets.size())
          selected_regions.push_back({int(current->catalog.targets[size_t(target)].instance),selection[1]});
      }
      const runtime::HoverRegion active_region{selected_instance,state.selected_joint};
      state.hover_toggle=window_->pointer_toggle;
      auto region=runtime::selection_region(hover,active_region,selected_regions,regions,state.hover_toggle);
      if(region.instance>=0&&render_scene.instances[size_t(region.instance)].prototype>=0) region={int(instance_groups->roots[size_t(region.instance)]),-1};
      state.hovered=region.instance;state.hovered_joint=region.joint;state.hovered_triangles=overlay.triangle_count(region.instance,region.joint);
      const auto *members=region.instance>=0&&region.joint<0?&instance_groups->members[size_t(region.instance)]:nullptr;
      if(members) {state.hovered_triangles=0;for(auto i:*members) state.hovered_triangles+=overlay.triangle_count(int(i));}
      const bool presented=(clay_wait||telemetry_.displayed_epoch.load()>=epoch)&&camera.epoch==camera_epoch;
      if(material_hover_generation!=current->generation)material_hover.clear();state.material_hover_primitives=0;for(auto [i,slot]:material_hover)state.material_hover_primitives+=overlay.surface_count(i,slot);
      if(presented&&!preview&&!pose_recovery.active) overlay.draw(camera,window_->width,window_->height,region.instance,region.joint,members,material_hover.empty()?nullptr:&material_hover);
      state.gizmo_shape={};
      if(gizmo_valid&&!pose_recovery.active&&!pose_drag.active&&!powerpose_drag.active&&gizmo_revision==applied_revision&&desired.revision==applied_revision) {
        state.gizmo_shape=gizmo_drag.shape;overlay.draw_gizmo(gizmo_drag.shape,window_->width,window_->height,gizmo_drag.shape.hit(float(window_->pointer_x),float(window_->pointer_y),8*gizmo_dpi),gizmo_dpi);
      }
      if(presented&&!pose_drag.active&&window_->clicks.load()!=clicks) {
        clicks=window_->clicks.load();const auto hit=picking.screen(camera,window_->click_x,window_->click_y,window_->width,window_->height);
        state.clicks=clicks;state.hit_target=-1;state.hit_joint=-1;state.hit_toggle=window_->click_toggle;
        if(hit.instance>=0&&render_scene.instances.at(size_t(hit.instance)).prototype>=0) state.hit_target=runtime::instance_selection(instance_groups->roots[size_t(hit.instance)]);
        for(size_t t=0;t<current->catalog.targets.size();++t) if(int(current->catalog.targets[t].instance)==hit.instance) {
          const auto selected=runtime::selection_region(hit,active_region,selected_regions,regions,state.hit_toggle);
          if(selected.instance>=0) {state.hit_target=int(t);state.hit_joint=selected.joint;}
        }
      }
      state.width=window_->width;state.height=window_->height;
      state.visible.clear();for(const auto &instance:render_scene.instances) state.visible.push_back(instance.visible);
      if(const auto code=glGetError();code!=GL_NO_ERROR) throw GraphicsError("OpenGL 呈现失败; gl="+std::to_string(code));
      window_->swap();
      if(!pose_recovery.active&&!clay_wait) display->after_swap();binding.release();window_->check_graphics();
      const auto shown=display->drawn_frame();
      if(shown.id&&shown.epoch>=epoch) {
        if(graphics_recovering) {
          graphics_recovering=false;++graphics_recoveries;state.error.clear();
          telemetry_.graphics("recovery_succeeded","session="+std::to_string(sessions)+" epoch="+std::to_string(epoch));
        }
        if(!graphics_stable_since) graphics_stable_since=now();
        // 短时间反复故障累计到同一预算；稳定显示 30 秒后再开放自动恢复。
        if(now()-graphics_stable_since>=30) graphics_attempts=0;
      }
      if(sampling_.rebuild_probe&&(shown.id==0)!=blank_presented) {blank_presented=shown.id==0;telemetry_.event(blank_presented?"blank_present_begin":"blank_present_end");}
      state.present_time=telemetry_.last_present_time;state.sessions=sessions;
      state.preview=preview;state.quality=quality;state.render_width=shown.width;state.render_height=shown.height;
      if(shown.id&&shown.width==std::max(1,window_->width.load()/4)&&shown.height==std::max(1,window_->height.load()/4)) state.last_preview_frame=shown.id;
      if(!queued.pending&&!clay_wait&&telemetry_.displayed_epoch.load()>=epoch) state.presented_revision=applied_revision;
      state.requested_epoch=epoch;state.presented_epoch=telemetry_.displayed_epoch.load();
      state.gpu_device_bytes=gpu_device_bytes;state.gpu_host_bytes=gpu_host_bytes;
      state.sampling=sampling_;
      state.frames=telemetry_.submitted.load();state.samples=telemetry_.displayed_samples.load();if(!physics_commit.valid())state.adapter=adapter->stats();state.evaluation=runtime->morph_stats();
      if(now()>=next_state_report) {
        next_state_report=now()+2;
        const auto &c=state.camera;
        const nlohmann::json report={{"seconds",now()},{"generation",state.generation},{"sessions",sessions},
          {"requested_epoch",epoch},{"presented_epoch",state.presented_epoch},{"samples",state.samples},{"preview",preview},
          {"graphics_recoveries",graphics_recoveries},{"graphics_attempts",graphics_attempts},{"graphics_recovering",graphics_recovering},
          {"render_size",{state.render_width,state.render_height}},{"viewport_size",{state.width,state.height}},
          {"camera",{c.target.x,c.target.y,c.target.z,c.distance,c.yaw,c.pitch}},
          {"gpu_device_bytes",state.gpu_device_bytes},{"gpu_host_bytes",state.gpu_host_bytes},
          {"triangles",state.adapter.triangles},{"curves",state.adapter.curves},
          {"sampling",{{"adaptive_threshold",sampling_.adaptive_threshold},{"samples",sampling_.samples},{"prune_hidden",sampling_.prune_hidden},
            {"texture_limit",sampling_.texture_limit},{"subsurface",sampling_.subsurface},{"bump_and_normal",sampling_.bump_and_normal},{"transparent_bounces",sampling_.transparent_bounces},
            {"update_interval_seconds",sampling_.update_interval_seconds},{"denoise",false}}}};
        std::ofstream(output_/"render-state.json")<<report.dump(2);
        telemetry_.flush();
      }
      state.skinning=runtime->skin_stats();state.formulas=runtime->formula_stats();state.effective=runtime->effective();state.conform=runtime->conform_stats();state.collision=refined_collision;state.graft_seams=runtime->graft_seams();
      state.effective_poses=runtime->effective_poses();state.input_poses=runtime->input_poses();state.skin_world.clear();for(const auto &skin:current->skeletons.skins) state.skin_world.push_back(render_scene.instances[skin.instance].transform);
      state.target_world.clear();for(const auto &target:current->catalog.targets)state.target_world.push_back(render_scene.instances[target.instance].transform);
      if(bounds_dirty||state.instance_bounds.size()!=render_scene.instances.size()) {
        const bool all=state.instance_bounds.size()!=render_scene.instances.size();state.instance_bounds.resize(render_scene.instances.size());state.instance_geometry_bounds.resize(render_scene.instances.size());
        for(uint32_t i=0;i<render_scene.instances.size();++i){const auto &instance=render_scene.instances[i];
          if(!all&&std::none_of(delta.instances.begin(),delta.instances.end(),[&](const auto &e){return e.index==i;})&&std::none_of(delta.visibility.begin(),delta.visibility.end(),[&](const auto &e){return e.index==i;})&&std::none_of(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e){return e.index==instance.mesh;}))continue;
          auto &bounds=state.instance_geometry_bounds[i];bounds={};for(auto p:render_scene.meshes[instance.mesh].positions)bounds.add(instance.transform.point(p));state.instance_bounds[i]=instance.visible?bounds:ir::Bounds{};
        }
        state.group_worlds.clear();for(const auto &id:group_frames->hierarchy.groups)state.group_worlds[id]=group_world(*group_source,render_scene,id,runtime->effective_poses(),&*group_frames);
      }
      if(selected_target>=0&&size_t(selected_target)<desired.values.size()&&selection_generation==current->generation&&desired.values[size_t(selected_target)].extension.kind==runtime::ExtensionKind::density){
        if(state.weight_target!=selected_target||state.weight_generation!=current->generation||bounds_dirty){const auto &instance=render_scene.instances.at(current->catalog.targets.at(size_t(selected_target)).instance);state.weight_positions=std::make_shared<const std::vector<ir::Vec3>>(render_scene.meshes.at(instance.mesh).positions);}
        state.weight_target=selected_target;state.weight_generation=current->generation;
      }else{state.weight_positions.reset();state.weight_target=-1;}
      state.effective_roots.assign(current->catalog.targets.size(),{});
      for(size_t t=0;t<current->catalog.targets.size();++t) for(size_t s=0;s<current->skeletons.skins.size();++s)
        if(current->catalog.targets[t].instance==current->skeletons.skins[s].instance&&!runtime->effective_poses()[s].empty()) state.effective_roots[t]=runtime->effective_poses()[s][0];
      if(state.evaluation.morph_evaluations!=measured_evaluation||state.skinning.evaluations!=measured_skinning||state.evaluation.transform_evaluations!=measured_transform||!delta.meshes.empty()) {
        const auto diagnostic_begin=now();
        const bool all=measured_evaluation==UINT64_MAX||state.bounds.size()!=current->catalog.targets.size();
        measured_evaluation=state.evaluation.morph_evaluations;measured_skinning=state.skinning.evaluations;measured_transform=state.evaluation.transform_evaluations;
        state.bounds.resize(current->catalog.targets.size());state.head_bounds.resize(current->catalog.targets.size());displacements.resize(current->catalog.targets.size());
        for(size_t t=0;t<current->catalog.targets.size();++t) {const auto &target=current->catalog.targets[t];
        const auto mesh=render_scene.instances[target.instance].mesh;
        const bool mesh_changed=all||std::any_of(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e){return e.index==mesh;});
        if(!mesh_changed&&!std::any_of(delta.instances.begin(),delta.instances.end(),[&](const auto &e){return e.index==target.instance;})) continue;
        const auto &base=current->loaded.scene.meshes[mesh].positions;const auto &positions=render_scene.meshes[mesh].positions;
        auto bounds=state.instance_geometry_bounds.at(target.instance);state.bounds[t]=bounds;
        ir::Bounds head;const auto &region=regions[target.instance];
        if(region.head>=0) for(size_t t=0;t<region.body.size();++t) if(region.body[t]==region.head) for(auto v:render_scene.meshes[mesh].triangles[t].vertices) head.add(render_scene.instances[target.instance].transform.point(positions[v]));
        state.head_bounds[t]=head;
        if(mesh_changed)displacements[t]=vertex_displacement(base,positions,!water::find(current->waters,target.id)).value_or(0);
        }
        state.max_displacement=displacements.empty()?0:*std::max_element(displacements.begin(),displacements.end());
        timing("diagnostic_bounds",diagnostic_begin,applied_revision);
      }
      publish_state();
      } catch(const std::exception &e) {
        const std::string error=e.what();
        const bool graphics=dynamic_cast<const GraphicsError *>(&e)||window_->present_context.failed()||window_->render_context.failed()||(display&&display->failed());
        telemetry_.graphics(graphics?"render_graphics_failure":"render_failure",error);
        cleanup();reset_render_state();
        current=document;failed_revision=desired.revision;state={};state.generation=document->generation;state.clicks=clicks;state.error=error;
        if(graphics) {graphics_pending=graphics_recovering=true;graphics_stable_since=0;next_graphics_attempt=now()+.25;}
        else graphics_recovering=false;
        publish_state();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    cleanup();
  } catch(const std::exception &e) {
    telemetry_.graphics("render_thread_failure",e.what());
    state.error=e.what();try {cleanup();} catch(...) {}
    graphics_recovering=false;publish_state();
  }
  nlohmann::json report={{"generation",state.generation},{"applied_revision",state.applied_revision},{"presented_revision",state.presented_revision},
    {"mesh_creations",state.adapter.meshes},{"instances",state.adapter.instances},{"unique_triangles",state.adapter.unique_triangles},{"instanced_triangles",state.adapter.triangles},{"curves",state.adapter.curves},{"geometry_updates",state.adapter.geometry_updates},{"instance_updates",state.adapter.instance_updates},
    {"camera_updates",state.adapter.camera_updates},{"morph_evaluations",state.evaluation.morph_evaluations},{"offsets_visited",state.evaluation.offsets_visited},
    {"skin_evaluations",state.skinning.evaluations},{"skin_vertices",state.skinning.vertices},
    {"conform_bound_vertices",state.conform.bindings},{"conform_authored_morphs",state.conform.authored_morphs},{"conform_evaluations",state.conform.evaluations},
    {"collision_evaluations",state.collision.evaluations},{"collision_corrected_vertices",state.collision.corrected_vertices},{"collision_cache_hits",state.collision.cache_hits},
    {"topology_updates",state.adapter.topology_updates},{"scene_updates",state.adapter.scene_updates},
    {"formula_evaluations",state.formulas.expressions},{"formula_channels",state.formulas.channels},{"edit_error",state.edit_error},
    {"max_displacement_m",state.max_displacement},{"frames",state.frames},{"interop_readback_bytes",telemetry_.readback_bytes.load()},
    {"requested_epoch",state.requested_epoch},{"presented_epoch",state.presented_epoch},{"error",state.error},{"visible_fps","NOT_MEASURED"},
    {"navigation_preview",{{"width_divisor",4},{"height_divisor",4},{"samples",2},{"idle_seconds",.15}}}};
  report["sessions"]=sessions;report["sampling"]=sampling_report;
  report["graphics_recoveries"]=graphics_recoveries;report["graphics_attempts"]=graphics_attempts;report["graphics_blocked"]=graphics_blocked;
  std::ofstream(output_/"editor-render.json")<<report.dump(2);
}
}
