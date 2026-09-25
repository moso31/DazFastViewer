  bool pose_edit_test_=false;
  int pose_test_level_=-1;
  int pose_test_target_=-1,pose_test_skin_=-1,pose_test_hand_=-1,pose_test_finger_=-1,pose_probe_=0,pose_drag_steps_=0;
  Snapshot pose_test_initial_;
  ir::Transform pose_angle_reference_;
  RenderStatus pose_test_before_;
  QPoint pose_test_point_;
  double pose_test_at_=0,pose_release_at_=0;
  nlohmann::json pose_checks_=nlohmann::json::array();
  void pose_edit_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000) {finish_test(false,"FK / IK 副屏验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()) {finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation) return;
    const bool ready=state.presented_revision==snapshot_.revision&&state.presented_epoch==state.requested_epoch&&!state.preview&&state.samples>=4&&state.selections==tree_selection(hierarchy_);
    auto hwnd=FindWindowExW(HWND(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);if(!hwnd) {finish_test(false,"IK 测试缺少原生视口");return;}
    auto mouse=[&](UINT message,int dx=0,int dy=0) {const int x=pose_test_point_.x()+dx,y=pose_test_point_.y()+dy;SendMessageW(hwnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,MAKELPARAM(x,y));};
    auto unchanged=[&] {return snapshot_.poses==pose_test_initial_.poses;};
    auto record=[&](const char *name) {pose_checks_.push_back({{"case",name},{"tool",int(chrome->settings().tool)},{"stage",test_stage_},{"revision",snapshot_.revision},{"previews",state.pose_previews},{"solve_ms",state.pose_solve_ms},{"input_to_present_ms",state.pose_latency_ms},{"error_m",state.pose_error},{"geometry_updates",state.adapter.geometry_updates},{"skin_evaluations",state.skinning.evaluations},{"curves",state.adapter.curves}});};
    std::ofstream(output_/"pose-progress.json")<<nlohmann::json({{"stage",test_stage_},{"ready",ready},{"dragging",state.pose_dragging},{"restoring",state.pose_restoring},{"epochs",{state.presented_epoch,state.requested_epoch}},{"revisions",{state.applied_revision,state.presented_revision,snapshot_.revision}},{"pending",state.pending_payloads},{"preview",state.preview},{"samples",state.samples},{"previews",state.pose_previews},{"selected",tree_selection(hierarchy_)},{"hover",state.hovered_joint},{"probe",pose_probe_},{"pointer",{state.pointer_x,state.pointer_y}},{"requested",{pose_test_point_.x(),pose_test_point_.y()}}}).dump();
    if(test_stage_==0&&ready) {
      for(size_t s=0;s<document_->skeletons.skins.size()&&pose_test_skin_<0;++s) {
        const auto &skin=document_->skeletons.skins[s];int hand=-1,finger=-1;for(size_t j=0;j<skin.joints.size();++j) {if(skin.joints[j].name=="lHand") hand=int(j);if(skin.joints[j].name=="lMid3") finger=int(j);}
        for(size_t t=0;t<document_->catalog.targets.size();++t) if(hand>=0&&finger>=0&&document_->catalog.targets[t].instance==skin.instance&&document_->catalog.targets[t].conform_target.empty()) {pose_test_skin_=int(s);pose_test_target_=int(t);pose_test_hand_=hand;pose_test_finger_=finger;break;}
      }
      if(pose_test_skin_<0) {finish_test(false,"IK 测试未找到带左手和中指的角色");return;}
      if(pose_test_level_>=0) {const int level=pose_test_level_;pose_test_level_=-1;set_subdivision(size_t(pose_test_target_),level);return;}
      pose_test_initial_=snapshot_;pose_test_before_=state;choose(pose_test_target_,pose_test_finger_);
      const auto &s=document_->skeletons.skins[pose_test_skin_];int channel=-1;for(int c=3;c<6;++c) if(runtime::editable_channel(s,pose_test_finger_,c)) channel=c;
      if(channel<0||!parameters_->edit_control("joint/"+std::to_string(channel),runtime::joint_value(snapshot_.poses[pose_test_skin_][pose_test_finger_],channel)-10)) {finish_test(false,"指节 FK 控件无法编辑");return;}
      ++test_stage_;return;
    }
    if(test_stage_==1&&ready) {
      if(unchanged()||state.adapter.geometry_updates<=pose_test_before_.adapter.geometry_updates) {finish_test(false,"FK 数值未联动真实形变");return;}
      record("finger_fk");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"finger-fk.png").wstring()));snapshot_.poses=pose_test_initial_.poses;send();++test_stage_;return;
    }
    if(test_stage_==2&&ready) {chrome->findChild<QAction *>("GizmoTool1")->trigger();choose(pose_test_target_,pose_test_hand_);++test_stage_;return;}
    if(test_stage_==3&&ready) {
      if(state.selection_bounds.empty) {finish_test(false,"手部没有聚焦范围");return;}auto b=state.selection_bounds;const auto c=b.center();const float e=std::max(.15f,b.extent());b.add({c.x-e,c.y-e,c.z-e});b.add({c.x+e,c.y+e,c.z+e});renderer_->frame(b);choose(pose_test_target_);++test_stage_;return;
    }
    if(test_stage_==4&&ready) {
      if(pose_probe_&&state.pointer_x==pose_test_point_.x()&&state.pointer_y==pose_test_point_.y()&&state.hovered_joint==pose_test_hand_) {record("hand_hit");choose(-1);++test_stage_;return;}
      if(pose_probe_&&!(state.pointer_x==pose_test_point_.x()&&state.pointer_y==pose_test_point_.y())) {renderer_->pointer(pose_test_point_.x(),pose_test_point_.y());return;}
      if(pose_probe_>441) {finish_test(false,"射线未命中手部");return;}
      const int n=pose_probe_++;pose_test_point_={state.width/2+(n?((n-1)%21-10)*state.width/30:0),state.height/2+(n?((n-1)/21-10)*state.height/30:0)};renderer_->pointer(pose_test_point_.x(),pose_test_point_.y());return;
    }
    if((test_stage_==5||test_stage_==8||test_stage_==11||test_stage_==17||test_stage_==23)&&ready) {
      if(state.gizmo_available&&state.gizmo_shape.hit(float(pose_test_point_.x()),float(pose_test_point_.y()))>=0) {finish_test(false,"IK 回退验证点错误地命中了 Gizmo");return;}
      pose_test_before_=state;pose_test_at_=now();pose_drag_steps_=0;mouse(WM_LBUTTONDOWN);++test_stage_;return;
    }
    if(test_stage_==6||test_stage_==9) {
      if(now()-pose_test_at_<.1) return;mouse(WM_MOUSEMOVE,-30,-20);mouse(WM_LBUTTONUP,-30,-20);pose_test_at_=now();++test_stage_;return;
    }
    if((test_stage_==7||test_stage_==10)&&now()-pose_test_at_>.3) {
      if(!unchanged()||state.pose_previews!=pose_test_before_.pose_previews) {finish_test(false,"未选中或多选时触发 IK");return;}record(test_stage_==7?"unselected_rejected":"multi_selection_rejected");
      choose(pose_test_target_);if(test_stage_==7) choose(pose_test_target_,pose_test_hand_,true);++test_stage_;return;
    }
    if(test_stage_==12||test_stage_==18||test_stage_==24) {
      if(now()-pose_test_at_<.08) return;
      if(pose_drag_steps_<20) {++pose_drag_steps_;mouse(WM_MOUSEMOVE,-pose_drag_steps_*2,-pose_drag_steps_);pose_test_at_=now();return;}
      if(!state.pose_dragging||state.pose_gizmo||state.pose_previews<=pose_test_before_.pose_previews) {finish_test(false,"单选角色的左键拖动没有回退到 IK 预览");return;}
      if(state.skinning.evaluations!=pose_test_before_.skinning.evaluations||state.collision.evaluations!=pose_test_before_.collision.evaluations||state.adapter.geometry_updates!=pose_test_before_.adapter.geometry_updates) {finish_test(false,"IK 代理拖动仍在求值完整蒙皮、碰撞或 Cycles 几何");return;}
      record(test_stage_==12?"bounded_proxy_drag":test_stage_==24?"angle_proxy_drag":"cancel_preview");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"ik-proxy.png").wstring()));
      if(test_stage_==18) SendMessageW(hwnd,WM_KEYDOWN,VK_ESCAPE,0);mouse(WM_LBUTTONUP,-40,-20);pose_release_at_=now();++test_stage_;return;
    }
    if(test_stage_==13&&ready&&snapshot_.poses!=pose_test_initial_.poses) {
      if(state.adapter.geometry_updates<=pose_test_before_.adapter.geometry_updates||state.skinning.evaluations<=pose_test_before_.skinning.evaluations) {finish_test(false,"松手未恢复完整形变");return;}
      bool properties_same=snapshot_.values.size()==pose_test_initial_.values.size();for(size_t t=0;properties_same&&t<snapshot_.values.size();++t) {const auto &a=snapshot_.values[t],&b=pose_test_initial_.values[t];properties_same=a.morphs==b.morphs&&a.visible==b.visible&&runtime::make_transform(a.transform)==runtime::make_transform(b.transform);}
      if(snapshot_.subdivision_levels!=pose_test_initial_.subdivision_levels||!properties_same) {finish_test(false,"IK 改变了细分或角色属性");return;}
      record("full_quality_commit");pose_checks_.back()["restore_ms"]=(now()-pose_release_at_)*1000;screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"ik-final.png").wstring()));
      snapshot_.poses=pose_test_initial_.poses;send();test_stage_=16;return;
    }
    if(test_stage_==16&&ready) {record("pose_restore");chrome->findChild<QAction *>("GizmoTool2")->trigger();choose(pose_test_target_);test_stage_=17;return;}
    if(test_stage_==19&&ready&&now()-pose_release_at_>.3) {if(!unchanged()) {finish_test(false,"Esc 取消后姿势改变");return;}record("escape_cancelled");choose(pose_test_target_,pose_test_hand_);test_stage_=20;return;}
    if(test_stage_==20&&ready) {
      chrome->findChild<QAction *>("GizmoTool3")->trigger();
      if(!pose_pins_.empty()) {finish_test(false,"IK 固定没有默认关闭");return;}
      const auto &s=document_->skeletons.skins[pose_test_skin_];pose_angle_reference_=runtime::rotation_frame(state.skin_world.at(pose_test_skin_))*runtime::joint_orientation(s,state.effective_poses.at(pose_test_skin_),pose_test_hand_);
      if(!parameters_->edit_control("pose/pin-angle",1)||pose_pins_.size()!=1||!pose_pins_[0].angle||pose_pins_[0].position) {finish_test(false,"角度开关未独立开启");return;}
      record("angle_enabled_position_free");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"ik-angle-control.png").wstring()));choose(pose_test_target_);test_stage_=23;return;
    }
    if(test_stage_==25&&ready&&snapshot_.poses!=pose_test_initial_.poses) {
      const auto &s=document_->skeletons.skins[pose_test_skin_];const auto current=runtime::rotation_frame(state.skin_world.at(pose_test_skin_))*runtime::joint_orientation(s,state.effective_poses.at(pose_test_skin_),pose_test_hand_);
      const double angle=runtime::orientation_error_degrees(current,pose_angle_reference_);if(angle>.5) {finish_test(false,"固定角度完整恢复后偏离超过 0.5 度："+std::to_string(angle));return;}
      record("angle_full_quality_commit");pose_checks_.back()["angle_error_degrees"]=angle;pose_checks_.back()["restore_ms"]=(now()-pose_release_at_)*1000;choose(pose_test_target_,pose_test_hand_);test_stage_=26;return;
    }
    if(test_stage_==26&&ready) {
      if(!parameters_->edit_control("pose/pin",1)||!pose_pins_[0].angle||!pose_pins_[0].position) {finish_test(false,"位置与角度不能同时固定");return;}
      if(!parameters_->edit_control("pose/pin-angle",0)||pose_pins_.size()!=1||pose_pins_[0].angle||!pose_pins_[0].position) {finish_test(false,"关闭角度影响了位置固定");return;}
      if(!parameters_->edit_control("pose/pin",0)||!pose_pins_.empty()) {finish_test(false,"固定开关不能独立关闭");return;}
      record("independent_pin_toggles");snapshot_.poses=pose_test_initial_.poses;send();test_stage_=27;return;
    }
    if(test_stage_==27&&ready) {record("angle_pose_restore");finish_test(true);return;}
  }
