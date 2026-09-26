// 一次加载内依次复现头部特写和材质消融，原始场景与用户设置保持只读。
nlohmann::json render_plan_,render_records_=nlohmann::json::array();
size_t render_case_=0;
int render_phase_=0;
uint64_t render_case_epoch_=0;
double render_case_start_=0,render_case_request_=0;
bool render_wait_empty_=false;
void start_profile_after_empty(const std::filesystem::path &file) {
  render_wait_empty_=true;empty_scene();
  auto *timer=new QTimer(this);timer->setInterval(50);
  connect(timer,&QTimer::timeout,this,[this,timer,file] {
    const auto state=renderer_->status();
    if(!document_||state.generation!=document_->generation||state.samples<1) return;
    timer->stop();timer->deleteLater();render_wait_empty_=false;load(file);
  });timer->start();
}
void render_profile_tick(const RenderStatus &state) {
  if(render_wait_empty_||loading_) return;
  if(!render_phase_&&!document_&&QDateTime::currentMSecsSinceEpoch()-test_started_>900000) {finish_test(false,"性能实验场景加载超时");return;}
  if(now()-render_case_request_>900&&render_phase_) {finish_test(false,"渲染性能实验超时");return;}
  if(!state.error.empty()||!state.edit_error.empty()) {finish_test(false,state.error+state.edit_error);return;}
  if(!document_||state.generation!=document_->generation||state.bounds.size()!=document_->catalog.targets.size()) return;
  const auto &entry=render_plan_.at(render_case_);
  if(!render_phase_) {
    if(!render_case_) {
      nlohmann::json audit;const auto &scene=document_->loaded.scene;
      for(const auto &t:document_->catalog.targets) {const auto &i=scene.instances[t.instance];const auto &m=scene.meshes[i.mesh];
        audit["instances"].push_back({{"id",i.id},{"label",t.label},{"visible",i.visible},{"triangles",m.triangles.size()},{"curves",m.curves.size()},{"materials",i.materials},{"parent",t.parent},{"ancestors",t.ancestors},{"conform_target",t.conform_target}});}
      for(const auto &m:scene.materials) audit["materials"].push_back({{"id",m.id},{"sss",m.subsurface},{"sss_radius",{m.subsurface_radius.x,m.subsurface_radius.y,m.subsurface_radius.z}},{"opacity",m.opacity},{"opacity_texture",m.opacity_texture},{"dual_weight",m.dual_weight},{"hair",m.hair}});
      std::ofstream(output_/"render-resources.json")<<audit.dump(2);
    }
    const auto label=entry.at("target").get<std::string>();int target=-1;
    for(size_t t=0;t<document_->catalog.targets.size();++t) if(document_->catalog.targets[t].label==label) {
      if(target>=0) {finish_test(false,"性能实验目标名称重复");return;}target=int(t);
    }
    if(target<0||state.head_bounds[target].empty) {finish_test(false,"性能实验未找到角色头部："+label);return;}
    RenderProbe probe;probe.serial=render_case_+1;
    probe.disable_sss=entry.value("disable_sss",false);probe.disable_bump=entry.value("disable_bump",false);
    probe.transparent_bounces=entry.value("transparent_bounces",32);
    if(probe.transparent_bounces<0||probe.transparent_bounces>32) {finish_test(false,"透明反弹实验参数越界");return;}
    const auto hidden=entry.value("hide",std::vector<std::string>{});
    for(const auto &name:hidden) {
      bool found=false;
      for(const auto &t:document_->catalog.targets) if(t.label==name) {probe.hidden.push_back(document_->loaded.scene.instances[t.instance].id);found=true;}
      if(!found) {finish_test(false,"性能实验隐藏目标不存在："+name);return;}
    }
    if(entry.value("isolate",false)) {
      const auto &root=document_->catalog.targets[target];
      for(const auto &t:document_->catalog.targets) {
        const bool attached=t.instance==root.instance||t.conform_target==root.id||t.parent=="#"+root.id||
          std::find(t.ancestors.begin(),t.ancestors.end(),root.id)!=t.ancestors.end();
        if(!attached) probe.hidden.push_back(document_->loaded.scene.instances[t.instance].id);
      }
    }
    choose(-1);
    // 默认只操作相机，基线不再触发诊断专用材质／显隐同步。
    // 消融之后恢复基线时仍显式恢复原始材质。
    const bool apply_probe=entry.contains("disable_sss")||entry.contains("disable_bump")||entry.contains("hide")||entry.contains("isolate")||entry.contains("transparent_bounces")||(render_case_>0&&state.probe_serial>0);
    if(apply_probe) renderer_->render_probe(std::move(probe));
    const auto &bounds=state.head_bounds[target];const auto center=bounds.center();
    const auto &m=document_->loaded.scene.instances[document_->catalog.targets[target].instance].transform.value;
    const float yaw=entry.value("yaw",std::atan2(-m[1],m[5]));
    const float distance=entry.value("distance_scale",1.6f)*bounds.extent();
    renderer_->camera_view({center.x,center.y,center.z,std::max(.025f,distance),yaw,entry.value("pitch",-.04f)});
    render_case_request_=now();render_phase_=1;return;
  }
  const bool apply_probe=entry.contains("disable_sss")||entry.contains("disable_bump")||entry.contains("hide")||entry.contains("isolate")||entry.contains("transparent_bounces")||(render_case_>0&&state.probe_serial>0);
  if((apply_probe&&state.probe_serial!=render_case_+1)||state.preview||state.presented_epoch!=state.requested_epoch||state.samples<1) return;
  if(now()-render_case_request_<.2) return;
  if(render_phase_==1) {render_case_epoch_=state.presented_epoch;render_case_start_=now();render_phase_=2;}
  if(state.presented_epoch!=render_case_epoch_) {render_case_epoch_=state.presented_epoch;render_case_start_=now();return;}
  const double duration=entry.value("seconds",12.0);
  if(now()-render_case_start_<duration&&state.samples<entry.value("samples",4096)) return;
  const auto name="case-"+std::to_string(render_case_);
  const auto pixmap=screen()->grabWindow(winId());const auto ratio=pixmap.devicePixelRatio();const auto origin=host_->mapTo(this,QPoint{});
  pixmap.copy(QRect(qRound(origin.x()*ratio),qRound(origin.y()*ratio),state.width,state.height)).save(QString::fromStdWString((output_/(name+".png")).wstring()));
  const auto &c=state.camera;
  render_records_.push_back({{"case",render_case_},{"settings",entry},{"epoch",render_case_epoch_},{"start_seconds",render_case_start_},{"end_seconds",now()},
    {"gpu_device_bytes",state.gpu_device_bytes},{"gpu_host_bytes",state.gpu_host_bytes},
    {"samples",state.samples},{"render_size",{state.render_width,state.render_height}},{"camera",{c.target.x,c.target.y,c.target.z,c.distance,c.yaw,c.pitch}},
    {"sampling",{{"adaptive_threshold",state.sampling.adaptive_threshold},{"prune_hidden",state.sampling.prune_hidden},{"texture_limit",state.sampling.texture_limit},
      {"subsurface",state.sampling.subsurface},{"bump_and_normal",state.sampling.bump_and_normal},{"transparent_bounces",state.sampling.transparent_bounces},{"update_interval_seconds",state.sampling.update_interval_seconds}}},
    {"geometry_updates",state.adapter.geometry_updates},{"instance_updates",state.adapter.instance_updates},{"scene_updates",state.adapter.scene_updates}});
  std::ofstream(output_/"render-profile.json")<<render_records_.dump(2);
  renderer_->trace("render_profile_case_end");
  if(++render_case_==render_plan_.size()) {finish_test(true);return;}
  render_phase_=0;
}
