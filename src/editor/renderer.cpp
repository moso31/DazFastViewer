#include "editor/renderer.h"
#include "viewport/display.h"
#include "bench/fixtures.h"
#include "device/device.h"
#include "scene/scene.h"
#include "scene/integrator.h"
#include "scene/pass.h"
#include "session/session.h"
#include <epoxy/wgl.h>
#include <fstream>

namespace dfv::editor {
Renderer::Renderer(HWND host,int width,int height,const std::filesystem::path &output):output_(output),telemetry_(output) {
  window_=std::make_unique<Window>(width,height,false,&telemetry_,2,host);
  thread_=std::jthread([this](std::stop_token stop) {run(stop);});
}
Renderer::~Renderer() {thread_.request_stop();if(thread_.joinable()) thread_.join();}
void Renderer::set_document(std::shared_ptr<const Document> document,const Snapshot &snapshot) {
  frame(document->loaded.scene.bounds());
  std::lock_guard lock(mutex_);document_=std::move(document);snapshot_=snapshot;
}
void Renderer::frame(const ir::Bounds &bounds) {
  if(bounds.empty) return;const auto c=bounds.center();
  window_->camera.target={c.x,c.y,c.z};window_->camera.distance=std::max(bounds.extent()*1.6f,.35f);window_->camera.yaw=.3f;
  window_->camera.pitch=bounds.maximum.z-bounds.minimum.z<bounds.extent()*.5f?.7f:.08f;window_->publish();
}
void Renderer::edit(const Snapshot &snapshot) {std::lock_guard lock(mutex_);if(document_ && snapshot.generation==document_->generation) snapshot_=snapshot;}
RenderStatus Renderer::status() {std::lock_guard lock(mutex_);return status_;}
void Renderer::orbit(float x,float y) {window_->camera.orbit(x,y);window_->publish();}
void Renderer::run(std::stop_token stop) {
  using namespace ccl;
  std::unique_ptr<Session> session;Display *display=nullptr;
  auto cleanup=[&] {
    if(session) {
      session->cancel(true);
      window_->present_context.activate();if(display) display->release_present_resources();window_->present_context.deactivate();
      session.reset();display=nullptr;
    }
  };
  RenderStatus state;
  try {
    DeviceInfo device;
    for(const auto &candidate:Device::available_devices()) if(candidate.type==DEVICE_OPTIX) {device=candidate;break;}
    if(device.type!=DEVICE_OPTIX) throw std::runtime_error("找不到 OptiX 设备；编辑器不会自动回退 CPU");
    std::shared_ptr<const Document> current;
    ir::Scene render_scene;
    std::unique_ptr<runtime::DeformationRuntime> runtime;
    std::unique_ptr<CyclesAdapter> adapter;
    uint64_t epoch=0,camera_epoch=0,applied_revision=0,attempted_revision=0,measured_evaluation=0,measured_skinning=0,measured_transform=0;
    SessionParams params;params.device=device;params.samples=64;params.pixel_size=1;params.background=false;
    params.use_resolution_divider=false;params.use_auto_tile=false;params.threads=8;
    BufferParams buffers;buffers.width=buffers.full_width=window_->width;buffers.height=buffers.full_height=window_->height;
    while(!stop.stop_requested()) {
      std::shared_ptr<const Document> document;Snapshot desired;
      {std::lock_guard lock(mutex_);document=document_;if(document) desired=snapshot_;}
      if(!document) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
      if(current!=document) {
        cleanup();current=document;render_scene=current->loaded.scene;ir::add_studio(render_scene);
        runtime=std::make_unique<runtime::DeformationRuntime>(render_scene,current->catalog.targets,current->skeletons.skins,current->formulas.graphs);
        runtime->evaluate(desired.values,desired.poses);
        SceneParams scene_params;scene_params.background=false;scene_params.bvh_type=BVH_TYPE_DYNAMIC;
        scene_params.use_texture_cache=false;scene_params.auto_texture_cache=false;
        session=std::make_unique<Session>(params,scene_params);
        auto &scene=*session->scene;
        auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);
        scene.integrator->set_seed(1337);scene.integrator->set_max_bounce(8);scene.integrator->set_max_diffuse_bounce(4);
        scene.integrator->set_max_glossy_bounce(4);scene.integrator->set_max_transmission_bounce(8);scene.integrator->set_transparent_max_bounce(8);
        scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);
        auto camera=window_->mailbox.latest();render_scene.camera=render_camera(camera,window_->width,window_->height);camera_epoch=camera.epoch;
        adapter=std::make_unique<CyclesAdapter>(scene);adapter->load(render_scene);
        auto driver=std::make_unique<Display>(*window_,telemetry_,session->dfv_render_epoch,session->dfv_render_samples,false);
        display=driver.get();session->set_display_driver(std::move(driver));session->dfv_requested_epoch=++epoch;
        applied_revision=attempted_revision=desired.revision;session->reset(params,buffers);session->start();
        state={};state.generation=current->generation;state.applied_revision=applied_revision;measured_evaluation=measured_skinning=measured_transform=UINT64_MAX;
      }
      if(session->progress.get_error()) throw std::runtime_error(session->progress.get_error_message());
      if(display->failed()) throw std::runtime_error(display->error());
      auto camera=window_->mailbox.latest();
      if((camera.epoch!=camera_epoch || desired.revision!=attempted_revision) && telemetry_.displayed_epoch.load()>=epoch && session->ready_to_reset()) {
        ir::Delta delta;
        if(desired.generation==current->generation && desired.revision!=attempted_revision) {
          attempted_revision=desired.revision;
          try {delta=runtime->evaluate(desired.values,desired.poses);applied_revision=desired.revision;state.edit_error.clear();}
          catch(const std::exception &e) {state.edit_error=e.what();}
        }
        if(camera.epoch!=camera_epoch) {delta.camera=render_camera(camera,window_->width,window_->height);camera_epoch=camera.epoch;}
        {thread_scoped_lock lock(session->scene->mutex);adapter->apply(delta);session->dfv_requested_epoch=++epoch;session->reset(params,buffers);}
        state.applied_revision=applied_revision;
      }
      window_->present_context.activate();
      glViewport(0,0,window_->width,window_->height);glClearColor(.035f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);
      session->draw();
      if(!SwapBuffers(window_->dc)) {window_->present_context.deactivate();throw std::runtime_error("Qt 视口 SwapBuffers 失败");}
      display->after_swap();window_->present_context.deactivate();
      if(telemetry_.displayed_epoch.load()>=epoch) state.presented_revision=applied_revision;
      state.requested_epoch=epoch;state.presented_epoch=telemetry_.displayed_epoch.load();
      state.frames=telemetry_.submitted.load();state.samples=session->dfv_render_samples.load();state.adapter=adapter->stats();state.evaluation=runtime->morph_stats();
      state.skinning=runtime->skin_stats();state.formulas=runtime->formula_stats();state.effective=runtime->effective();
      if(state.evaluation.morph_evaluations!=measured_evaluation||state.skinning.evaluations!=measured_skinning||state.evaluation.transform_evaluations!=measured_transform) {
        measured_evaluation=state.evaluation.morph_evaluations;measured_skinning=state.skinning.evaluations;measured_transform=state.evaluation.transform_evaluations;state.max_displacement=0;state.bounds.clear();
        for(const auto &target:current->catalog.targets) {
        const auto mesh=render_scene.instances[target.instance].mesh;
        const auto &base=current->loaded.scene.meshes[mesh].positions;const auto &positions=render_scene.meshes[mesh].positions;
        ir::Bounds bounds;for(const auto p:positions) bounds.add(render_scene.instances[target.instance].transform.point(p));state.bounds.push_back(bounds);
        for(size_t v=0;v<base.size();++v) {
          const double x=positions[v].x-base[v].x,y=positions[v].y-base[v].y,z=positions[v].z-base[v].z;
          state.max_displacement=std::max(state.max_displacement,std::sqrt(x*x+y*y+z*z));
        }
        }
      }
      {std::lock_guard lock(mutex_);status_=state;}
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    cleanup();
  } catch(const std::exception &e) {
    state.error=e.what();try {cleanup();} catch(...) {}
    std::lock_guard lock(mutex_);status_=state;
  }
  nlohmann::json report={{"generation",state.generation},{"applied_revision",state.applied_revision},{"presented_revision",state.presented_revision},
    {"mesh_creations",state.adapter.meshes},{"geometry_updates",state.adapter.geometry_updates},{"instance_updates",state.adapter.instance_updates},
    {"camera_updates",state.adapter.camera_updates},{"morph_evaluations",state.evaluation.morph_evaluations},{"offsets_visited",state.evaluation.offsets_visited},
    {"skin_evaluations",state.skinning.evaluations},{"skin_vertices",state.skinning.vertices},
    {"formula_evaluations",state.formulas.expressions},{"formula_channels",state.formulas.channels},{"edit_error",state.edit_error},
    {"max_displacement_m",state.max_displacement},{"frames",state.frames},{"interop_readback_bytes",telemetry_.readback_bytes.load()},
    {"requested_epoch",state.requested_epoch},{"presented_epoch",state.presented_epoch},{"error",state.error},{"visible_fps","NOT_MEASURED"}};
  std::ofstream(output_/"editor-render.json")<<report.dump(2);
}
}
