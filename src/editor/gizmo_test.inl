  bool gizmo_test_=false;
  int gz_case_=0,gz_phase_=0,gz_steps_=0,gz_target_=-1,gz_skin_=-1,gz_joint_=-1,gz_index_=0;
  int gz_ground_=0,gz_other_target_=-1;
  uint64_t gz_ground_requests_=0;
  double gz_height_=0;
  runtime::TransformValues gz_ground_transform_;
  double gz_at_=0;
  Snapshot gz_initial_;
  RenderStatus gz_before_;
  RenderStatus gz_drag_before_;
  QPoint gz_point_;
  std::vector<GizmoSegment> gz_lines_;
  nlohmann::json gz_checks_=nlohmann::json::array();
  void gizmo_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000) {finish_test(false,"Gizmo 副屏验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()) {finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation) return;
    const bool ready=state.applied_revision==snapshot_.revision&&state.presented_revision==snapshot_.revision&&state.presented_epoch==state.requested_epoch&&!state.preview&&state.samples>=4&&state.selections==tree_selection(hierarchy_);
    struct Case {GizmoTool tool;GizmoSpace space;bool bone=false;int cancel=0;bool light=false;bool plane=false;};
    static const Case cases[]={{GizmoTool::translate,GizmoSpace::world},{GizmoTool::translate,GizmoSpace::local},
      {GizmoTool::rotate,GizmoSpace::local},{GizmoTool::rotate,GizmoSpace::world},{GizmoTool::scale,GizmoSpace::local},
      {GizmoTool::rotate,GizmoSpace::world,true},{GizmoTool::rotate,GizmoSpace::local,true},
      {GizmoTool::translate,GizmoSpace::world,false,1},{GizmoTool::rotate,GizmoSpace::world,false,2},{GizmoTool::scale,GizmoSpace::local,false,3},
      {GizmoTool::translate,GizmoSpace::world,false,0,true},{GizmoTool::rotate,GizmoSpace::local,false,0,true},{GizmoTool::scale,GizmoSpace::local,false,0,true},
      {GizmoTool::translate,GizmoSpace::local,false,0,false,true},{GizmoTool::translate,GizmoSpace::world,false,0,false,true}};
    std::ofstream(output_/"gizmo-progress.json")<<nlohmann::json({{"case",gz_case_},{"phase",gz_phase_},{"ground",gz_ground_},{"ready",ready},{"dragging",state.pose_dragging},{"restoring",state.pose_restoring},{"available",state.gizmo_available},{"lines",state.gizmo_shape.lines.size()},{"revision",snapshot_.revision},{"applied",state.applied_revision},{"selection",state.selections}}).dump();
    if(gz_target_<0&&ready) {
      for(size_t s=0;s<document_->skeletons.skins.size()&&gz_target_<0;++s) for(size_t t=0;t<document_->catalog.targets.size();++t) if(document_->catalog.targets[t].instance==document_->skeletons.skins[s].instance&&document_->catalog.targets[t].conform_target.empty()) {
        const auto &skin=document_->skeletons.skins[s];for(size_t j=0;j<skin.joints.size();++j) if(skin.joints[j].name=="lHand") {gz_target_=int(t);gz_skin_=int(s);gz_joint_=int(j);break;}
      }
      if(gz_target_<0) {finish_test(false,"Gizmo 验证需要带左手骨骼的角色");return;}gz_initial_=snapshot_;gz_before_=state;
      // 非零初始旋转可区分 Local 和 World，结束时逐项恢复到原始场景。
      snapshot_.values[gz_target_].transform.rotation_degrees={15,25,35};send();return;
    }
    if(gz_target_<0) return;
    if(gz_case_>=int(std::size(cases))) {
      if(!ready||state.pose_restoring) return;
      auto *ratio=chrome->findChild<QDoubleSpinBox *>("GroundAlignmentRatio");
      if(gz_ground_==0) {
        snapshot_.values=gz_initial_.values;snapshot_.poses=gz_initial_.poses;snapshot_.values[gz_target_].transform.rotation_degrees={20,15,10};snapshot_.values[gz_target_].transform.scale={1.2f,1.3f,.9f};snapshot_.values[gz_target_].transform.translation_cm.y=57;
        send();choose(gz_target_,gz_joint_);ratio->setValue(.01);chrome->ground_action()->trigger();gz_ground_=1;return;
      }
      if(gz_ground_==1) {
        if(!ground_pending_.empty()) return;
        gz_height_=state.bounds.at(gz_target_).maximum.z-state.bounds.at(gz_target_).minimum.z;gz_drag_before_=state;
        if(std::abs(state.bounds.at(gz_target_).minimum.z-.01*gz_height_)>3e-5) {finish_test(false,"地面对齐使用了旋转和缩放更新前的包围盒");return;}
        gz_checks_.push_back({{"ground_queued_after_transform",true},{"world_height",gz_height_}});
        for(size_t t=0;t<document_->catalog.targets.size();++t) if(int(t)!=gz_target_) try {if(attachment_host(*document_,t)==t) {gz_other_target_=int(t);break;}} catch(const std::exception &) {}
        if(gz_other_target_>=0) {choose(gz_other_target_);if(ratio->value()!=0) {finish_test(false,"另一个角色的默认比例被污染");return;}ratio->setValue(-.03);}
        choose(-1);choose(gz_target_);if(ratio->value()!=.01) {finish_test(false,"选择切换丢失角色地面对齐比例");return;}
        chrome->ground_action()->trigger();gz_ground_=2;return;
      }
      if(gz_ground_==2||gz_ground_==4||gz_ground_==6) {
        if(!ground_pending_.empty()) return;
        const double expected=(gz_ground_==2?.01:gz_ground_==4?0:-.02)*gz_height_;const auto &b=state.bounds.at(gz_target_);
        if(std::abs(b.minimum.z-expected)>3e-5||std::abs(b.minimum.x-gz_drag_before_.bounds.at(gz_target_).minimum.x)>3e-5||std::abs(b.minimum.y-gz_drag_before_.bounds.at(gz_target_).minimum.y)>3e-5) {finish_test(false,"地面对齐没有按世界 Y 与当前高度比例移动");return;}
        if(snapshot_.poses!=gz_initial_.poses) {finish_test(false,"地面对齐改写了骨骼姿势");return;}
        if(gz_other_target_>=0) {const auto instance=document_->catalog.targets[gz_other_target_].instance;if(snapshot_.values[gz_other_target_].ground_alignment_ratio!=-.03||state.instance_transforms.at(instance)!=gz_drag_before_.instance_transforms.at(instance)) {finish_test(false,"地面对齐污染了另一个角色的比例或位置");return;}}
        gz_checks_.push_back({{"ground_ratio",snapshot_.values[gz_target_].ground_alignment_ratio},{"world_height",gz_height_},{"bottom",b.minimum.z},{"other_figure_checked",gz_other_target_>=0}});
        if(gz_ground_==2) {
          gz_ground_requests_=state.ground_requests;
          ratio->setValue(0);const auto hwnd=FindWindowExW(HWND(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
          SendMessageW(hwnd,WM_KEYDOWN,VK_CONTROL,0);SendMessageW(hwnd,WM_KEYDOWN,'D',0);SendMessageW(hwnd,WM_KEYUP,'D',0);SendMessageW(hwnd,WM_KEYUP,VK_CONTROL,0);gz_at_=now();gz_ground_=3;return;
        }
        if(gz_ground_==4) {ratio->setValue(-.02);chrome->ground_action()->trigger();gz_ground_=6;return;}
        gz_ground_transform_=snapshot_.values[gz_target_].transform;chrome->ground_action()->trigger();gz_ground_=7;return;
      }
      if(gz_ground_==3) {if(now()-gz_at_<.5||state.ground_requests<=gz_ground_requests_) return;gz_ground_=4;return;}
      if(gz_ground_==7) {
        if(!ground_pending_.empty()) return;
        const auto &t=snapshot_.values[gz_target_].transform;if(t.translation_cm!=gz_ground_transform_.translation_cm||state.camera.epoch!=gz_drag_before_.camera.epoch) {finish_test(false,"重复对齐累积位移或 Ctrl+D 移动了相机");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"ground-alignment.png").wstring()));gz_ground_=8;
      }
      if(gz_phase_!=9) {snapshot_.values=gz_initial_.values;snapshot_.poses=gz_initial_.poses;snapshot_.lights=gz_initial_.lights;send();gz_phase_=9;return;}
      if(ready) {if(state.mesh_hashes!=gz_before_.mesh_hashes||state.instance_transforms!=gz_before_.instance_transforms) {finish_test(false,"Gizmo 恢复后几何或矩阵不一致");return;}std::ofstream(output_/"gizmo-check.json")<<nlohmann::json({{"pass",true},{"cases",gz_checks_},{"restored",true}}).dump(2);finish_test(true);}return;
    }
    const auto c=cases[gz_case_];const auto hwnd=FindWindowExW(HWND(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
    if(c.light&&snapshot_.lights.empty()) {++gz_case_;return;}
    auto mouse=[&](UINT message,QPoint p){SendMessageW(hwnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,MAKELPARAM(p.x(),p.y()));};
    if(gz_phase_==0&&ready) {
      if(c.light) {for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) if((*it)->data(0,Qt::UserRole+2).toInt()==0) {choose_item(hierarchy_,*it);sync_selection();break;}}
      else choose(gz_target_,c.bone?gz_joint_:-1);
      chrome->findChild<QAction *>("GizmoTool"+QString::number(int(c.tool)))->trigger();
      for(auto *a:chrome->findChild<QToolBar *>("TransformModule")->actions()) if(a->text()==(c.space==GizmoSpace::local?"Local":"World")) a->trigger();
      renderer_->gizmo({c.tool,c.space});gz_at_=now();gz_phase_=1;return;
    }
    if(gz_phase_==1&&ready&&now()-gz_at_>.2) {
      if(!state.gizmo_available||state.gizmo_shape.lines.empty()) {finish_test(false,"已选中节点却没有 Gizmo");return;}
      if(c.bone||c.light) {const auto p=state.gizmo_shape.pivot;ir::Bounds bounds;bounds.add({p.x-.3f,p.y-.3f,p.z-.3f});bounds.add({p.x+.3f,p.y+.3f,p.z+.3f});renderer_->frame(bounds);}else renderer_->frame(state.bounds.at(gz_target_));
      gz_at_=now();gz_phase_=2;return;
    }
    if(gz_phase_==2&&ready&&now()-gz_at_>.2) {
      gz_lines_.clear();const int handle=c.plane?(state.gizmo_shape.planes.empty()?-1:state.gizmo_shape.planes.front().handle):c.tool==GizmoTool::scale?3:0;for(const auto &line:state.gizmo_shape.lines) if(line.handle==handle) gz_lines_.push_back(line);
      gz_index_=-1;for(size_t k=0;k<gz_lines_.size();++k) {const auto &l=gz_lines_[k];QPoint p(qRound((l.a.x+l.b.x)*.5),qRound((l.a.y+l.b.y)*.5));if(state.gizmo_shape.hit(float(p.x()),float(p.y()))==handle&&p.x()>15&&p.y()>15&&p.x()<state.width-15&&p.y()<state.height-15) {gz_index_=int(k);gz_point_=p;break;}}
      if(c.plane&&!state.gizmo_shape.planes.empty()) {ir::Vec2 center{};for(auto p:state.gizmo_shape.planes.front().corners) {center.x+=p.x/4;center.y+=p.y/4;}gz_point_={qRound(center.x),qRound(center.y)};gz_index_=state.gizmo_shape.hit(float(gz_point_.x()),float(gz_point_.y()))==handle?0:-1;}
      if(gz_index_<0) {finish_test(false,"没有可见手柄供实际命中");return;}
      if(gz_case_==2||gz_case_==3) screen()->grabWindow(winId()).save(QString::fromStdWString((output_/("gizmo-"+std::to_string(gz_case_)+"-ready.png")).wstring()));
      gz_steps_=0;gz_at_=now();gz_drag_before_=state;mouse(WM_LBUTTONDOWN,gz_point_);gz_phase_=3;return;
    }
    if(gz_phase_==3&&now()-gz_at_>.08) {
      ++gz_steps_;QPoint p=gz_point_;
      if(c.tool==GizmoTool::rotate) {const auto &l=gz_lines_[(gz_index_+gz_steps_)%gz_lines_.size()];p={qRound((l.a.x+l.b.x)*.5),qRound((l.a.y+l.b.y)*.5)};}
      else p+=QPoint(gz_steps_*3,-gz_steps_);
      mouse(WM_MOUSEMOVE,p);gz_at_=now();if(gz_steps_<12) return;
      if(!state.pose_dragging||!state.pose_gizmo) {finish_test(false,"实际拖动没有进入 Gizmo 预览");return;}
      if(state.skinning.evaluations!=gz_drag_before_.skinning.evaluations||state.collision.evaluations!=gz_drag_before_.collision.evaluations||state.adapter.geometry_updates!=gz_drag_before_.adapter.geometry_updates) {finish_test(false,"Gizmo 拖动仍在求值完整蒙皮、碰撞或 Cycles 几何");return;}
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/("gizmo-"+std::to_string(gz_case_)+"-drag.png")).wstring()));
      if(c.cancel==1) SendMessageW(hwnd,WM_KEYDOWN,VK_ESCAPE,0);if(c.cancel==2) SendMessageW(hwnd,WM_KILLFOCUS,0,0);if(c.cancel==3) choose(-1);
      mouse(WM_LBUTTONUP,p);gz_phase_=4;return;
    }
    if(gz_phase_==4&&ready&&!state.pose_restoring&&now()-gz_at_>.3) {
      const auto base=gz_initial_.values[gz_target_].transform;auto expected=base;expected.rotation_degrees={15,25,35};
      const bool changed=c.light?snapshot_.lights!=gz_initial_.lights:c.bone?snapshot_.poses!=gz_initial_.poses:runtime::make_transform(snapshot_.values[gz_target_].transform)!=runtime::make_transform(expected);
      if(changed==bool(c.cancel)) {finish_test(false,c.cancel?"取消 Gizmo 后仍改写参数":"松手未提交 Gizmo 变换");return;}
      gz_checks_.push_back({{"tool",int(c.tool)},{"space",int(c.space)},{"bone",c.bone},{"light",c.light},{"plane",c.plane},{"cancel",c.cancel},{"solve_ms",state.pose_solve_ms},{"input_to_present_ms",state.pose_latency_ms}});
      snapshot_.values=gz_initial_.values;snapshot_.values[gz_target_].transform.rotation_degrees={15,25,35};snapshot_.poses=gz_initial_.poses;snapshot_.lights=gz_initial_.lights;send();++gz_case_;gz_phase_=0;return;
    }
  }
