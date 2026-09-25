  bool powerpose_test_=false;
  int pp_case_=0,pp_phase_=0,pp_steps_=0,pp_target_=-1,pp_skin_=-1;
  Snapshot pp_initial_;
  RenderStatus pp_before_;
  QPointF pp_point_;
  double pp_release_=0;
  double pp_dx_=0,pp_dy_=0;
  nlohmann::json pp_checks_=nlohmann::json::array();
  void powerpose_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000) {finish_test(false,"PowerPose 副屏验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()) {finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation) return;
    const bool ready=state.applied_revision==snapshot_.revision&&state.presented_revision==snapshot_.revision&&state.presented_epoch==state.requested_epoch&&!state.preview&&state.samples>=4&&state.selections==tree_selection(hierarchy_);
    struct Case {int page;const char *id;bool right;double dx,dy;int cancel=0;bool pin=false;};
    static const Case cases[]={{0,"B09",false,0,12},{0,"B09",true,0,12},{0,"B07",true,8,8},{0,"B28",false,0,-12},{0,"B23",false,8,8},
      {1,"H21",false,0,20},{1,"H24",false,0,12},{1,"H29",true,0,12},{2,"N02",true,12,0},{0,"B35",true,12,-12},{0,"B36",false,8,8},
      {0,"B21",true,0,6,0,true},{0,"B01",false,12,12,1},{0,"B01",false,12,12,2},{0,"B01",false,12,12,3}};
    std::ofstream(output_/"powerpose-progress.json")<<nlohmann::json({{"case",pp_case_},{"phase",pp_phase_},{"ready",ready},{"dragging",state.pose_dragging},{"previews",state.pose_previews},{"revision",snapshot_.revision},{"applied",state.applied_revision},{"steps",pp_steps_}}).dump();
    auto fail=[&](const std::string &message){finish_test(false,std::string("PowerPose ")+std::to_string(pp_case_)+": "+message);};
    if(pp_target_<0) {
      if(!ready) return;
      for(size_t s=0;s<document_->skeletons.skins.size()&&pp_target_<0;++s) for(size_t t=0;t<document_->catalog.targets.size();++t) {
        const auto &skin=document_->skeletons.skins[s];if(skin.instance==document_->catalog.targets[t].instance&&skin.joints.front().id.find("Genesis8")==0&&document_->catalog.targets[t].conform_target.empty()) {pp_skin_=int(s);pp_target_=int(t);break;}
      }
      if(pp_target_<0) {fail("没有支持的角色");return;}pp_initial_=snapshot_;choose(pp_target_);findChild<QDockWidget*>(QStringLiteral("PowerPose"))->raise();return;
    }
    if(pp_case_>=int(std::size(cases))) {
      if(!ready) return;
      if(snapshot_.poses!=pp_initial_.poses||state.pose_restore_max_error>1e-6) {fail("恢复后姿势或几何不同");return;}
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"powerpose-restored.png").wstring()));finish_test(true);return;
    }
    const auto &c=cases[pp_case_];const auto button=c.right?Qt::RightButton:Qt::LeftButton;
    auto mouse=[&](QEvent::Type type,QPointF offset={}) {const auto pos=pp_point_+offset;QMouseEvent event(type,pos,powerpose_->canvas()->mapToGlobal(pos.toPoint()),type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseButtonRelease?Qt::NoButton:button,c.pin?Qt::ShiftModifier:Qt::NoModifier);QApplication::sendEvent(powerpose_->canvas(),&event);};
    if(pp_phase_==0) {
      if(!ready) return;pp_before_=state;choose(pp_target_);powerpose_->page(c.page);refresh_powerpose(state);pp_point_=powerpose_->point_position(c.id);pp_steps_=0;
      pp_dx_=c.dx;pp_dy_=c.dy;
      // 复杂场景可能已经把本方向摆到限位；改测能够离开限位的反方向。
      for(const auto &page:runtime::powerpose_templates()) for(const auto &point:page.points) if(point.id==c.id) {
        const auto &skin=document_->skeletons.skins[pp_skin_];const auto bound=runtime::bind_powerpose(skin,point);
        if(runtime::apply_powerpose(skin,bound,pp_initial_.poses[pp_skin_],c.right,pp_dx_,pp_dy_,c.pin?.1:1)==pp_initial_.poses[pp_skin_]) {pp_dx_=-pp_dx_;pp_dy_=-pp_dy_;}
      }
      if(c.pin) for(const auto *name:{"lFoot","rFoot"}) {const int joint=runtime::powerpose_joint(document_->skeletons.skins[pp_skin_],name);pin_joint(pp_skin_,joint,true);pin_joint(pp_skin_,joint,true,true);}
      mouse(QEvent::MouseButtonPress);pp_phase_=1;return;
    }
    if(pp_phase_==1) {
      if(pp_steps_>=8) {if(!state.pose_dragging) return;
        if(snapshot_.poses!=pp_initial_.poses||snapshot_.revision!=pp_before_.applied_revision||state.skinning.evaluations!=pp_before_.skinning.evaluations||state.collision.evaluations!=pp_before_.collision.evaluations||state.adapter.geometry_updates!=pp_before_.adapter.geometry_updates) {fail("拖动提前提交或执行完整形变");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/(std::string(c.id)+(c.right?"-right":"-left")+"-proxy.png")).wstring()));
        pp_release_=now();
        if(c.cancel==1) {QKeyEvent e(QEvent::KeyPress,Qt::Key_Escape,{});QApplication::sendEvent(powerpose_->canvas(),&e);}
        else if(c.cancel==2) {QFocusEvent e(QEvent::FocusOut);QApplication::sendEvent(powerpose_->canvas(),&e);}
        else if(c.cancel==3) {choose(-1);}
        else mouse(QEvent::MouseButtonRelease,{pp_dx_,pp_dy_});
        pp_phase_=2;return;
      }
      ++pp_steps_;mouse(QEvent::MouseMove,{pp_dx_*pp_steps_/8,pp_dy_*pp_steps_/8});return;
    }
    if(pp_phase_==2) {
      if(!ready||state.pose_dragging||now()-pp_release_<.15) return;
      if(c.cancel) {if(snapshot_.poses!=pp_initial_.poses||snapshot_.revision!=pp_before_.applied_revision) {fail("取消后写入姿势");return;}}
      else {
        if(snapshot_.revision!=pp_before_.applied_revision+1) return;
        const bool figure=std::string(c.id)=="B35"||std::string(c.id)=="B36";
        if(!figure&&snapshot_.poses==pp_initial_.poses) {fail("松手没有提交骨骼变化");return;}
        if(figure&&state.instance_transforms==pp_before_.instance_transforms) {fail("整体移动没有更新实例矩阵");return;}
        for(size_t s=0;s<snapshot_.poses.size();++s) if(int(s)!=pp_skin_&&snapshot_.poses[s]!=pp_initial_.poses[s]) {fail("修改了其他角色或附件的输入姿势");return;}
        if(snapshot_.subdivision_levels!=pp_initial_.subdivision_levels) {fail("修改了细分设置");return;}
        if(c.pin) for(const auto &pin:pose_pins_) {const auto &skin=document_->skeletons.skins[pp_skin_];const auto &p=state.effective_poses[pp_skin_];const auto &world=state.skin_world[pp_skin_];const auto now_point=world.point(runtime::joint_point(skin,p,pin.joint,true));
          const double distance=std::sqrt(std::pow(now_point.x-pin.world.x,2)+std::pow(now_point.y-pin.world.y,2)+std::pow(now_point.z-pin.world.z,2));
          const double angle=runtime::orientation_error_degrees(runtime::rotation_frame(world)*runtime::joint_orientation(skin,p,pin.joint),pin.world_orientation);
          if(distance>.005||angle>.5) {fail("髋部移动后双脚固定偏差过大："+skin.joints[pin.joint].name+" / "+std::to_string(distance)+" m / "+std::to_string(angle)+" deg");return;}
        }
      }
      pp_checks_.push_back({{"point",c.id},{"right",c.right},{"delta",{pp_dx_,pp_dy_}},{"cancel",c.cancel},{"pins",c.pin},{"proxy_ms",state.pose_solve_ms},{"input_to_present_ms",state.pose_latency_ms},{"release_seconds",now()-pp_release_},{"previews",state.pose_previews-pp_before_.pose_previews},{"curves",state.adapter.curves},{"triangles",state.adapter.unique_triangles}});
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/(std::string(c.id)+(c.right?"-right":"-left")+"-final.png")).wstring()));
      pose_pins_.clear();renderer_->pose_pins({});snapshot_.poses=pp_initial_.poses;snapshot_.values=pp_initial_.values;send();choose(pp_target_);pp_phase_=3;return;
    }
    if(pp_phase_==3&&ready) {
      pp_checks_.back()["restore_max_error_m"]=state.pose_restore_max_error;pp_checks_.back()["restore_exact_hash"]=state.mesh_hashes==pp_before_.mesh_hashes;
      if(state.pose_restore_max_error>1e-6||state.instance_transforms!=pp_before_.instance_transforms) {fail("恢复后网格或实例矩阵不一致，最大顶点误差 "+std::to_string(state.pose_restore_max_error)+" m");return;}++pp_case_;pp_phase_=0;
    }
  }
