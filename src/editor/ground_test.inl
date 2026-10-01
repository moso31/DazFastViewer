  bool ground_test_=false;
  std::vector<int> ground_test_keys_;
  size_t ground_test_case_=0;
  int ground_test_phase_=0;
  uint64_t ground_test_requests_=0;
  Snapshot ground_test_initial_;
  RenderStatus ground_test_initial_state_,ground_test_before_;
  ir::Bounds ground_test_bounds_;
  nlohmann::json ground_test_checks_=nlohmann::json::array();
  void ground_test_tick(const RenderStatus &state){
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>180000){finish_test(false,"普通对象地面对齐验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation)return;
    const bool ready=state.applied_revision==snapshot_.revision&&state.presented_revision==snapshot_.revision&&!state.preview&&state.samples>=4&&state.selections==tree_selection(hierarchy_);
    if(!ready)return;
    const auto &scene=document_->loaded.scene;const runtime::InstanceGroups groups(scene);
    auto bounds=[&](int key){return key>=0?state.bounds.at(size_t(key)):state.selection_bounds;};
    if(ground_test_keys_.empty()){
      if(!document_->skeletons.skins.empty()){finish_test(false,"普通物体验证场景不应含骨架");return;}
      for(size_t t=0;t<document_->catalog.targets.size();++t)ground_test_keys_.push_back(int(t));
      for(uint32_t i=0;i<scene.instances.size();++i)if(scene.instances[i].prototype>=0&&groups.roots[i]==i)ground_test_keys_.push_back(runtime::instance_selection(i));
      if(ground_test_keys_.size()<3){finish_test(false,"需要两个普通对象和至少一个 Instance");return;}
      ground_test_initial_=snapshot_;ground_test_initial_state_=state;choose(ground_test_keys_.front());return;
    }
    if(ground_test_phase_==4){
      if(state.instance_transforms!=ground_test_initial_state_.instance_transforms){finish_test(false,"还原地面对齐后变换未恢复");return;}
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"ground-objects.png").wstring()));
      std::ofstream(output_/"ground-objects.json")<<nlohmann::json{{"result","PASS"},{"cases",ground_test_checks_},{"restored",true},{"sessions",state.sessions}}.dump(2);finish_test(true);return;
    }
    const int key=ground_test_keys_.at(ground_test_case_/3);const double ratios[]={0,.03,-.02};const double ratio=ratios[ground_test_case_%3];
    if(selected_!=key||state.selected_target!=key){choose(key);return;}
    if(ground_test_phase_==0){
      if(!chrome->ground_action()->isEnabled()){finish_test(false,"普通物体或 Instance 的对齐按钮未启用");return;}
      ground_test_before_=state;ground_test_bounds_=bounds(key);ground_panel_->findChild<QDoubleSpinBox *>("GroundAlignmentRatio")->setValue(ratio*100);ground_test_requests_=state.ground_requests;
      const auto hwnd=FindWindowExW(HWND(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
      SendMessageW(hwnd,WM_KEYDOWN,VK_CONTROL,0);SendMessageW(hwnd,WM_KEYDOWN,'D',0);SendMessageW(hwnd,WM_KEYUP,'D',0);SendMessageW(hwnd,WM_KEYUP,VK_CONTROL,0);ground_test_phase_=1;return;
    }
    if(ground_test_phase_==1){
      if(state.ground_requests<=ground_test_requests_||!ground_pending_.empty())return;
      const auto after=bounds(key);const auto height=ground_test_bounds_.maximum.z-ground_test_bounds_.minimum.z;
      if(std::abs(after.minimum.z-ratio*height)>3e-5||std::abs(after.minimum.x-ground_test_bounds_.minimum.x)>3e-5||std::abs(after.minimum.y-ground_test_bounds_.minimum.y)>3e-5){finish_test(false,"普通对象地面对齐位置错误");return;}
      const auto selected=key>=0?document_->catalog.targets[size_t(key)].instance:uint32_t(-5-key);
      for(size_t i=0;i<scene.instances.size();++i)if(i!=selected&&(key>=0||groups.roots[i]!=selected)&&state.instance_transforms[i]!=ground_test_before_.instance_transforms[i]){finish_test(false,"对齐影响了未选中的对象或原型");return;}
      if(state.camera.epoch!=ground_test_before_.camera.epoch||state.sessions!=ground_test_initial_state_.sessions){finish_test(false,"Ctrl+D 移动了相机或重建会话");return;}
      ground_test_checks_.push_back({{"key",key},{"ratio",ratio},{"height",height},{"bottom",after.minimum.z},{"native_shortcut",true}});
      const auto saved=snapshot_json(*document_,snapshot_);auto restored=initial_snapshot(*document_);apply_snapshot_json(*document_,restored,saved);
      if(restored.instance_ground!=snapshot_.instance_ground){finish_test(false,"DUFEX 未保存实例对齐");return;}
      ground_test_before_=state;chrome->ground_action()->trigger();ground_test_phase_=2;return;
    }
    if(ground_test_phase_==2){
      if(!ground_pending_.empty())return;if(state.instance_transforms!=ground_test_before_.instance_transforms){finish_test(false,"重复对齐累积了位移");return;}
      if(++ground_test_case_==ground_test_keys_.size()*3){const auto revision=snapshot_.revision;snapshot_=ground_test_initial_;snapshot_.revision=revision;send();ground_test_phase_=4;return;}
      ground_test_phase_=0;choose(ground_test_keys_.at(ground_test_case_/3));
    }
  }
