#include "editor/renderer.h"
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
#include <epoxy/wgl.h>
#include <fstream>
#include <bit>

namespace dfv::editor {
Renderer::Renderer(HWND host,int width,int height,const std::filesystem::path &output,SamplingSettings sampling):output_(output),sampling_(sampling),telemetry_(output) {
  window_=std::make_unique<Window>(width,height,false,&telemetry_,2,host);
  thread_=std::jthread([this](std::stop_token stop) {run(stop);});
}
Renderer::~Renderer() {thread_.request_stop();if(thread_.joinable()) thread_.join();}
void Renderer::set_document(std::shared_ptr<const Document> document,const Snapshot &snapshot,bool frame_scene) {
  if(frame_scene) {ir::Bounds bounds;for(const auto &target:document->catalog.targets) {const auto &i=document->loaded.scene.instances[target.instance];if(i.visible) for(auto p:document->loaded.scene.meshes[i.mesh].positions) bounds.add(i.transform.point(p));}frame(bounds);}
  std::lock_guard lock(mutex_);document_=std::move(document);snapshot_=snapshot;
}
void Renderer::resize(int width,int height) {
  if(width<1||height<1) return;
  SetWindowPos(window_->hwnd,nullptr,0,0,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
  std::lock_guard lock(mutex_);requested_width_=width;requested_height_=height;resize_preview_until_=now()+.15;
}
void Renderer::pointer(int x,int y,bool click,bool toggle) {
  POINT point{x,y};ClientToScreen(window_->hwnd,&point);SetCursorPos(point.x,point.y);
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
void Renderer::select(uint64_t generation,int target,int joint,std::vector<Selection> selections) {std::lock_guard lock(mutex_);selection_generation_=generation;selected_target_=target;selected_joint_=joint;selections_=std::move(selections);}
RenderStatus Renderer::status() {std::lock_guard lock(mutex_);return status_;}
CameraState Renderer::input_camera() {return window_->mailbox.latest();}
void Renderer::camera_view(const std::array<float,6> &v) {window_->camera.target={v[0],v[1],v[2]};window_->camera.distance=v[3];window_->camera.yaw=v[4];window_->camera.pitch=v[5];window_->publish();}
void Renderer::keyboard(int key,bool pressed) {SetFocus(window_->hwnd);PostMessageW(window_->hwnd,pressed?WM_KEYDOWN:WM_KEYUP,WPARAM(key),0);}
void Renderer::orbit(float x,float y) {window_->camera.orbit(x,y);window_->publish();}
void Renderer::run(std::stop_token stop) {
  using namespace ccl;
  std::unique_ptr<Session> session;Display *display=nullptr;
  std::unique_ptr<CyclesAdapter> adapter;
  nlohmann::json sampling_report;
  std::ofstream progress_log(output_/"cycles-progress.log");std::string last_progress;
  HoverOverlay overlay;runtime::PickingScene picking;std::vector<runtime::JointRegions> regions;std::vector<uint8_t> pickable;bool geometry_dirty=true;uint64_t clicks=0;
  std::optional<runtime::InstanceGroups> instance_groups;
  auto cleanup=[&] {
    if(session) {
      session->cancel(true);
      const auto &integrator=*session->scene->integrator;const auto &background=session->scene->dscene.data.background;
      sampling_report={{"denoise",integrator.get_use_denoise()},{"max_samples",sampling_.samples},{"adaptive_sampling",integrator.get_use_adaptive_sampling()},
        {"adaptive_threshold",integrator.get_adaptive_threshold()},{"min_bounces",integrator.get_min_bounce()},{"transparent_min_bounces",integrator.get_transparent_min_bounce()},
        {"sampling_pattern",sampling_.blue_noise?"blue_noise_first":"tabulated_sobol"},{"background_mis",background.use_mis},{"background_map_resolution",{background.map_res_x,background.map_res_y}}};
      window_->present_context.activate();overlay.release();if(display) display->release_present_resources();window_->present_context.deactivate();
      adapter.reset();
      session.reset();display=nullptr;
    }
  };
  RenderStatus state;
  uint64_t sessions=0;
  auto timing=[&](const char *name,double begin,uint64_t revision=0) {Frame f;f.epoch=state.requested_epoch;f.id=revision;telemetry_.event(name,f,(now()-begin)*1000);};
  try {
    DeviceInfo device;
    for(const auto &candidate:Device::available_devices()) if(candidate.type==DEVICE_OPTIX) {device=candidate;break;}
    if(device.type!=DEVICE_OPTIX) throw std::runtime_error("找不到 OptiX 设备；编辑器不会自动回退 CPU");
    std::shared_ptr<const Document> current;
    ir::Scene render_scene;
    std::unique_ptr<runtime::DeformationRuntime> runtime;
    uint64_t epoch=0,camera_epoch=0,applied_revision=0,attempted_revision=0,measured_evaluation=0,measured_skinning=0,measured_transform=0;
    uint64_t retried=0;
    std::vector<std::vector<ir::Vec3>> previous_positions;
    std::vector<double> displacements;
    SessionParams params;params.device=device;params.samples=sampling_.samples;params.pixel_size=1;params.background=false;
    params.use_resolution_divider=false;params.use_auto_tile=false;params.threads=8;
    BufferParams buffers;buffers.width=buffers.full_width=window_->width;buffers.height=buffers.full_height=window_->height;
    bool preview=false;
    int camera_width=0,camera_height=0;
    auto set_quality=[&](bool moving) {
      preview=moving;params.samples=moving?2:sampling_.samples;
      buffers.width=buffers.full_width=moving?std::max(1,window_->width.load()/4):window_->width.load();
      buffers.height=buffers.full_height=moving?std::max(1,window_->height.load()/4):window_->height.load();
    };
    Snapshot desired;bool edit_affects_render=false;
    while(!stop.stop_requested()) {
      std::shared_ptr<const Document> document;
      std::vector<Selection> selections;int width,height,selected_target,selected_joint;uint64_t selection_generation,retry;
      bool editing;double preview_until,resize_until;
      {std::lock_guard lock(mutex_);document=document_;if(document&&(desired.generation!=snapshot_.generation||desired.revision!=snapshot_.revision)) desired=snapshot_;width=requested_width_;height=requested_height_;selected_target=selected_target_;selected_joint=selected_joint_;selections=selections_;selection_generation=selection_generation_;retry=retry_resources_;editing=edit_active_&&snapshot_.revision>=interaction_revision_;preview_until=edit_preview_until_;resize_until=resize_preview_until_;}
      const bool retry_payloads=retry!=retried;
      if(width>0&&height>0&&(width!=window_->width||height!=window_->height)) {
        window_->width=width;window_->height=height;
      }
      if(!document) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
      if(current==document&&!state.error.empty()) {std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
      try {
      if(current!=document&&current&&session&&current->generation==document->generation&&document->asset_revision>current->asset_revision) {
        // 参数目录更新保留 Cycles 会话；从基础几何重建求值状态，避免二次叠加形变。
        runtime.reset();previous_positions.clear();for(const auto &mesh:render_scene.meshes) previous_positions.push_back(mesh.positions);
        current=document;render_scene=current->loaded.scene;
        runtime=std::make_unique<runtime::DeformationRuntime>(render_scene,current->catalog.targets,current->skeletons.skins,current->formulas.graphs);
        attempted_revision=0;state.edit_error.clear();
      }
      if(current!=document||!session) {
        cleanup();geometry_dirty=true;edit_affects_render=false;
        if(current!=document) {
          // 先销毁持有旧场景引用的求值器，再释放文档、顶点与射线缓存。
          runtime.reset();picking={};regions={};pickable={};render_scene={};
          current=document;state={};previous_positions.clear();render_scene=current->loaded.scene;
          regions.clear();regions.resize(render_scene.instances.size());
          pickable=runtime::viewport_pick_mask(render_scene,current->catalog.targets);
          for(const auto &skin:current->skeletons.skins) regions.at(skin.instance)=runtime::joint_regions(render_scene.meshes.at(render_scene.instances.at(skin.instance).mesh),skin);
          runtime=std::make_unique<runtime::DeformationRuntime>(render_scene,current->catalog.targets,current->skeletons.skins,current->formulas.graphs);
        }
        const auto resources=runtime->prepare(desired.values,desired.poses,retry_payloads);
        retried=retry;
        state.generation=current->generation;state.pending_payloads=resources.pending;state.resource_error=resources.error;
        if(resources.pending||!resources.error.empty()) {
          {std::lock_guard lock(mutex_);status_=state;}std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
        }
        runtime->evaluate(desired.values,desired.poses);
        render_scene.lights=desired.lights;render_scene.options=desired.options;
        SceneParams scene_params;scene_params.background=false;scene_params.bvh_type=BVH_TYPE_DYNAMIC;
        scene_params.use_texture_cache=false;scene_params.auto_texture_cache=false;
        const auto initial_camera=window_->mailbox.latest();
        set_quality(initial_camera.navigating||now()<initial_camera.preview_until);
        session=std::make_unique<Session>(params,scene_params);
        ++sessions;telemetry_.event("session_created");
        auto &scene=*session->scene;
        auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);
        scene.integrator->set_seed(1337);scene.integrator->set_max_bounce(8);scene.integrator->set_max_diffuse_bounce(4);
        scene.integrator->set_max_glossy_bounce(4);scene.integrator->set_max_transmission_bounce(8);scene.integrator->set_transparent_max_bounce(32);
        scene.integrator->set_use_denoise(false);
        scene.integrator->set_use_adaptive_sampling(sampling_.adaptive_threshold>0);scene.integrator->set_adaptive_min_samples(32);scene.integrator->set_adaptive_threshold(sampling_.adaptive_threshold);
        scene.integrator->set_sampling_pattern(sampling_.blue_noise?SAMPLING_PATTERN_BLUE_NOISE_FIRST:SAMPLING_PATTERN_TABULATED_SOBOL);
        scene.integrator->set_min_bounce(sampling_.min_bounces);scene.integrator->set_transparent_min_bounce(sampling_.transparent_min_bounces);
        session->dfv_event=[&](const char *name,uint64_t epoch,double ms) {Frame frame;frame.epoch=epoch;telemetry_.event(name,frame,ms);};
        auto camera=window_->mailbox.latest();render_scene.camera=render_camera(camera,window_->width,window_->height);camera_epoch=camera.epoch;camera_width=window_->width;camera_height=window_->height;
        const auto load_begin=now();adapter=std::make_unique<CyclesAdapter>(scene);adapter->load(render_scene);timing("adapter_load",load_begin);
        auto driver=std::make_unique<Display>(*window_,telemetry_,session->dfv_render_epoch,session->dfv_render_samples,false);
        display=driver.get();display->set_options(desired.options);session->set_display_driver(std::move(driver));session->dfv_requested_epoch=++epoch;
        applied_revision=attempted_revision=desired.revision;session->reset(params,buffers);session->start();
        {Frame f;f.epoch=epoch;f.id=applied_revision;f.width=buffers.width;f.height=buffers.height;telemetry_.event("edit_reset",f);}
        state={};state.clicks=clicks;state.generation=current->generation;state.applied_revision=applied_revision;measured_evaluation=measured_skinning=measured_transform=UINT64_MAX;
      }
      if(session->progress.get_error()) throw std::runtime_error(session->progress.get_error_message());
      std::string progress,detail;session->progress.get_status(progress,detail);
      progress+=" | "+detail;
      if(progress!=last_progress) {progress_log<<now()<<" "<<progress<<std::endl;last_progress=progress;}
      if(display->failed()) throw std::runtime_error(display->error());
      auto camera=window_->mailbox.latest();
      const bool size_changed=camera_width!=window_->width||camera_height!=window_->height;
      const bool edit_pending=desired.generation==current->generation&&desired.revision!=attempted_revision;
      const bool navigation_preview=camera.needs_preview(camera_epoch,now())||size_changed||now()<resize_until;
      bool wanted_preview=navigation_preview||(edit_affects_render&&(editing||now()<preview_until))||
        (edit_pending&&!state.pending_payloads&&state.resource_error.empty());
      const bool quality_changed=wanted_preview!=preview;
      ir::Delta delta;bool new_render_edit=false;
      // 进入预览时允许取消尚未出图的完整渲染；预览之间仍等待出图，防止连续输入饿死渲染。
      if((quality_changed || size_changed || camera.epoch!=camera_epoch || edit_pending) &&
         ((wanted_preview&&!preview)||(telemetry_.displayed_epoch.load()>=epoch && session->ready_to_reset()))) {
        if(desired.generation==current->generation && desired.revision!=attempted_revision) {
          try {
            const auto prepare_begin=now();
            const auto resources=runtime->prepare(desired.values,desired.poses,retry_payloads);
            timing("edit_prepare",prepare_begin,desired.revision);
            retried=retry;
            state.pending_payloads=resources.pending;state.resource_error=resources.error;
            if(!resources.pending&&resources.error.empty()) {
            attempted_revision=desired.revision;const auto evaluate_begin=now();
            diagnostics::LoadProfile profile;diagnostics::active=&profile;
            try {delta=runtime->evaluate(desired.values,desired.poses);} catch(...) {diagnostics::active=nullptr;throw;}
            diagnostics::active=nullptr;timing("edit_evaluate",evaluate_begin,desired.revision);
            for(const auto &[name,value]:profile.timings) {Frame f;f.id=desired.revision;telemetry_.event(name.c_str(),f,value.seconds*1000);}
            if(!previous_positions.empty()) {
              std::erase_if(delta.meshes,[&](const auto &edit) {const auto &old=previous_positions.at(edit.index);return old.size()==edit.positions.size()&&std::equal(old.begin(),old.end(),edit.positions.begin(),[](auto a,auto b){return a.x==b.x&&a.y==b.y&&a.z==b.z;});});previous_positions.clear();
            }
            for(size_t l=0;l<desired.lights.size();++l) {
              const auto &a=desired.lights[l],&b=render_scene.lights[l];
              if(a.transform.value!=b.transform.value||a.power.x!=b.power.x||a.power.y!=b.power.y||a.power.z!=b.power.z||a.width!=b.width||a.height!=b.height) delta.lights.push_back({uint32_t(l),a});
            }
            render_scene.lights=desired.lights;
            if(render_scene.options!=desired.options) {
              if(render_scene.options.environment!=desired.options.environment||render_scene.options.environment_file!=desired.options.environment_file||render_scene.options.backdrop!=desired.options.backdrop) delta.options=desired.options;
              render_scene.options=desired.options;display->set_options(desired.options);
            }
            new_render_edit=delta.options||!delta.meshes.empty()||!delta.instances.empty()||!delta.visibility.empty()||!delta.lights.empty();
            edit_affects_render=new_render_edit;applied_revision=desired.revision;state.edit_error.clear();}
          }
          catch(const std::exception &e) {attempted_revision=desired.revision;state.edit_error=e.what();}
        }
        if(camera.epoch!=camera_epoch||size_changed) {delta.camera=render_camera(camera,window_->width,window_->height);camera_epoch=camera.epoch;camera_width=window_->width;camera_height=window_->height;}
        // 色调等仅影响显示的编辑保留累计采样；真实场景修改才启动编辑预览。
        wanted_preview=navigation_preview||(edit_affects_render&&(editing||now()<preview_until||new_render_edit));
        if(wanted_preview!=preview||delta.options||delta.camera||!delta.meshes.empty()||!delta.instances.empty()||!delta.visibility.empty()||!delta.lights.empty())
          {const auto begin=now();thread_scoped_lock lock(session->scene->mutex);timing("scene_lock",begin,applied_revision);const auto apply_begin=now();adapter->apply(delta);timing("adapter_apply",apply_begin,applied_revision);set_quality(wanted_preview);session->dfv_requested_epoch=++epoch;const auto reset_begin=now();session->reset(params,buffers);timing("session_reset",reset_begin,applied_revision);
            Frame f;f.epoch=epoch;f.id=applied_revision;f.width=buffers.width;f.height=buffers.height;telemetry_.event("edit_reset",f);}
        state.applied_revision=applied_revision;
      }
      window_->present_context.activate();
      const bool bounds_dirty=geometry_dirty||!delta.meshes.empty()||!delta.instances.empty()||!delta.visibility.empty();
      if(geometry_dirty) {instance_groups.emplace(render_scene);auto begin=now();overlay.update(render_scene,regions);timing("overlay_update",begin,applied_revision);begin=now();picking.update(render_scene,pickable);timing("picking_update",begin,applied_revision);geometry_dirty=false;}
      else if(bounds_dirty) {auto begin=now();overlay.apply(render_scene,regions,delta);timing("overlay_update",begin,applied_revision);begin=now();picking.apply(render_scene,delta);timing("picking_update",begin,applied_revision);}
      if(sampling_.interaction_probe&&bounds_dirty) {
        const bool all=state.mesh_hashes.size()!=render_scene.meshes.size();state.mesh_hashes.resize(render_scene.meshes.size());
        for(size_t m=0;m<render_scene.meshes.size();++m) if(all||std::any_of(delta.meshes.begin(),delta.meshes.end(),[&](const auto &e){return e.index==m;})) {
          uint64_t hash=14695981039346656037ull;for(auto p:render_scene.meshes[m].positions) for(float v:{p.x,p.y,p.z}) {hash^=std::bit_cast<uint32_t>(v);hash*=1099511628211ull;}state.mesh_hashes[m]=hash;
        }
        state.instance_transforms.clear();for(const auto &instance:render_scene.instances) state.instance_transforms.push_back(instance.transform.value);
      }
      glViewport(0,0,window_->width,window_->height);glClearColor(.035f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);
      session->draw();
      state.camera=camera;state.pointer_x=window_->pointer_x;state.pointer_y=window_->pointer_y;
      auto hover=preview?runtime::PickHit{}:picking.screen(camera,state.pointer_x,state.pointer_y,window_->width,window_->height);
      bool editable=hover.instance>=0&&render_scene.instances.at(size_t(hover.instance)).prototype>=0;for(const auto &target:current->catalog.targets) if(int(target.instance)==hover.instance) editable=true;
      if(!editable) hover={};
      state.hovered_detail_joint=hover.instance>=0&&hover.triangle>=0&&size_t(hover.triangle)<regions[size_t(hover.instance)].detail.size()?regions[size_t(hover.instance)].detail[size_t(hover.triangle)]:-1;
      const bool selection_dirty=state.selection_generation!=selection_generation||state.selected_target!=selected_target||state.selected_joint!=selected_joint||state.selections!=selections;
      state.selection_generation=selection_generation;state.selected_target=selection_generation==current->generation?selected_target:-1;
      state.selected_joint=state.selected_target>=0?selected_joint:-1;state.selections=selection_generation==current->generation?selections:std::vector<Selection>{};
      const int instance_key=state.selected_target>=0&&size_t(state.selected_target)<current->catalog.targets.size()?int(current->catalog.targets[size_t(state.selected_target)].instance):state.selected_target<=-5?-5-state.selected_target:-1;
      const int selected_instance=instance_key>=0&&size_t(instance_key)<render_scene.instances.size()?instance_key:-1;
      state.focus_requests=window_->focus_requests;
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
          for(size_t f=0;f<mesh.triangles.size();++f) {
            int j=f<r.detail.size()?r.detail[f]:-1;bool selected=j==selected_joint;
            for(int depth=0;j>=0&&size_t(j)<r.parents.size()&&depth<256;++depth) {selected|=j==selected_joint;j=r.parents[size_t(j)];}
            if(selected&&mesh.draws(mesh.triangles[f])) for(auto v:mesh.triangles[f].vertices) member_bounds.add(instance.transform.point(mesh.positions[v]));
          }
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
      const bool presented=telemetry_.displayed_epoch.load()>=epoch&&camera.epoch==camera_epoch;
      if(presented&&!preview) overlay.draw(camera,window_->width,window_->height,region.instance,region.joint,members);
      if(presented&&window_->clicks.load()!=clicks) {
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
      if(!SwapBuffers(window_->dc)) {window_->present_context.deactivate();throw std::runtime_error("Qt 视口 SwapBuffers 失败");}
      display->after_swap();window_->present_context.deactivate();
      const auto shown=display->drawn_frame();
      state.present_time=telemetry_.last_present_time;state.sessions=sessions;
      state.preview=preview;state.render_width=shown.width;state.render_height=shown.height;
      if(shown.id&&shown.width==std::max(1,window_->width.load()/4)&&shown.height==std::max(1,window_->height.load()/4)) state.last_preview_frame=shown.id;
      if(telemetry_.displayed_epoch.load()>=epoch) state.presented_revision=applied_revision;
      state.requested_epoch=epoch;state.presented_epoch=telemetry_.displayed_epoch.load();
      state.frames=telemetry_.submitted.load();state.samples=telemetry_.displayed_samples.load();state.adapter=adapter->stats();state.evaluation=runtime->morph_stats();
      state.skinning=runtime->skin_stats();state.formulas=runtime->formula_stats();state.effective=runtime->effective();state.conform=runtime->conform_stats();state.collision=runtime->collision_stats();state.graft_seams=runtime->graft_seams();
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
        ir::Bounds bounds;for(const auto p:positions) bounds.add(render_scene.instances[target.instance].transform.point(p));state.bounds[t]=bounds;
        ir::Bounds head;const auto &region=regions[target.instance];
        if(region.head>=0) for(size_t t=0;t<region.body.size();++t) if(region.body[t]==region.head) for(auto v:render_scene.meshes[mesh].triangles[t].vertices) head.add(render_scene.instances[target.instance].transform.point(positions[v]));
        state.head_bounds[t]=head;
        if(mesh_changed) {displacements[t]=0;for(size_t v=0;v<base.size();++v) {
          const double x=positions[v].x-base[v].x,y=positions[v].y-base[v].y,z=positions[v].z-base[v].z;
          displacements[t]=std::max(displacements[t],std::sqrt(x*x+y*y+z*z));
        }}
        }
        state.max_displacement=displacements.empty()?0:*std::max_element(displacements.begin(),displacements.end());
        timing("diagnostic_bounds",diagnostic_begin,applied_revision);
      }
      {std::lock_guard lock(mutex_);status_=state;}
      } catch(const std::exception &e) {
        const std::string error=e.what();cleanup();runtime.reset();render_scene={};picking={};regions={};pickable={};
        current=document;state={};state.generation=document->generation;state.clicks=clicks;state.error=error;
        std::lock_guard lock(mutex_);status_=state;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    cleanup();
  } catch(const std::exception &e) {
    state.error=e.what();try {cleanup();} catch(...) {}
    std::lock_guard lock(mutex_);status_=state;
  }
  nlohmann::json report={{"generation",state.generation},{"applied_revision",state.applied_revision},{"presented_revision",state.presented_revision},
    {"mesh_creations",state.adapter.meshes},{"instances",state.adapter.instances},{"unique_triangles",state.adapter.unique_triangles},{"instanced_triangles",state.adapter.triangles},{"curves",state.adapter.curves},{"geometry_updates",state.adapter.geometry_updates},{"instance_updates",state.adapter.instance_updates},
    {"camera_updates",state.adapter.camera_updates},{"morph_evaluations",state.evaluation.morph_evaluations},{"offsets_visited",state.evaluation.offsets_visited},
    {"skin_evaluations",state.skinning.evaluations},{"skin_vertices",state.skinning.vertices},
    {"conform_bound_vertices",state.conform.bindings},{"conform_authored_morphs",state.conform.authored_morphs},{"conform_evaluations",state.conform.evaluations},
    {"collision_evaluations",state.collision.evaluations},{"collision_corrected_vertices",state.collision.corrected_vertices},
    {"formula_evaluations",state.formulas.expressions},{"formula_channels",state.formulas.channels},{"edit_error",state.edit_error},
    {"max_displacement_m",state.max_displacement},{"frames",state.frames},{"interop_readback_bytes",telemetry_.readback_bytes.load()},
    {"requested_epoch",state.requested_epoch},{"presented_epoch",state.presented_epoch},{"error",state.error},{"visible_fps","NOT_MEASURED"},
    {"navigation_preview",{{"width_divisor",4},{"height_divisor",4},{"samples",2},{"idle_seconds",.15}}}};
  report["sessions"]=sessions;report["sampling"]=sampling_report;std::ofstream(output_/"editor-render.json")<<report.dump(2);
}
}
