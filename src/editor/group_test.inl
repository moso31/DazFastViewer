  bool group_test_=false;
  int gt_phase_=0,gt_case_=0,gt_steps_=0,gt_history_=0;
  double gt_at_=0;
  std::string gt_id_;
  std::string gt_motion_label_;
  RenderStatus gt_drag_state_;
  ir::Vec3 gt_preview_shift_;
  double gt_max_world_error_=0;
  Snapshot gt_initial_,gt_before_;
  RenderStatus gt_state_;
  std::vector<uint32_t> gt_members_;
  std::vector<GizmoSegment> gt_lines_;
  QPoint gt_point_;
  size_t gt_line_=0;
  nlohmann::json gt_checks_=nlohmann::json::array();
  void group_test_tick(const RenderStatus &state){
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>240000){finish_test(false,"Group 变换验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation)return;
    const bool ready=state.applied_revision==snapshot_.revision&&state.presented_revision==snapshot_.revision&&!state.preview&&state.samples>=4&&state.selections==tree_selection(hierarchy_);
    std::ofstream(output_/"group-progress.json")<<nlohmann::json{{"phase",gt_phase_},{"case",gt_case_},{"ready",ready},{"revision",snapshot_.revision},{"applied",state.applied_revision},{"gizmo",state.gizmo_available},{"dragging",state.pose_dragging}};
    if(gt_phase_==0&&ready){
      for(QTreeWidgetItemIterator it(hierarchy_);*it;++it){auto *item=*it;if(item->data(0,Qt::UserRole).toInt()!=-4)continue;
        const auto id=item->data(0,Qt::UserRole+3).toString().toStdString();if(!group_node(*document_,id)||group_instances(*document_,id).empty())continue;
        if(!gt_motion_label_.empty()&&id!=gt_motion_label_&&group_node(*document_,id)->label!=gt_motion_label_)continue;
        gt_id_=id;gt_members_=group_instances(*document_,id);choose_item(hierarchy_,item);sync_selection();break;
      }
      if(gt_id_.empty()){finish_test(false,"测试场景需要一个含几何子节点的 Group");return;}
      gt_initial_=snapshot_;gt_state_=state;const auto *node=group_node(*document_,gt_id_);
      if(!gt_motion_label_.empty()){std::ofstream(output_/"group-native.json")<<nlohmann::json{{"world",node->world.value},{"edit_frame",node->edit_frame.value},{"translation_frame",node->translation_frame.value}}.dump(2);gt_phase_=2;return;}
      if(!parameters_->edit_control("group/0",node->translation_cm.x+15)||!parameters_->edit_control("group/3",node->rotation_degrees.x+20)||!parameters_->edit_control("group/9",node->general_scale*125)){finish_test(false,"Group 缺少 Transform 参数");return;}
      if(snapshot_.values!=gt_initial_.values||snapshot_.poses!=gt_initial_.poses){finish_test(false,"Group 参数改写了子对象输入");return;}
      const auto after=snapshot_.group_transforms;for(int i=0;i<3;++i)history_move(false);
      if(snapshot_.group_transforms!=gt_initial_.group_transforms||selected_group_!=gt_id_){finish_test(false,"Group 属性撤销未恢复输入或选择");return;}
      for(int i=0;i<3;++i)history_move(true);if(snapshot_.group_transforms!=after||selected_group_!=gt_id_){finish_test(false,"Group 属性重做失败");return;}
      gt_phase_=1;return;
    }
    if(gt_phase_==1&&ready){
      for(size_t i=0;i<state.instance_transforms.size();++i){const bool member=std::find(gt_members_.begin(),gt_members_.end(),uint32_t(i))!=gt_members_.end();if((state.instance_transforms[i]!=gt_state_.instance_transforms[i])!=member){finish_test(false,"Group 属性未移动全部子节点或污染其他对象");return;}}
      auto restored=initial_snapshot(*document_);apply_snapshot_json(*document_,restored,snapshot_json(*document_,snapshot_));if(restored.group_transforms!=snapshot_.group_transforms){finish_test(false,"Group 保存读取丢失变换");return;}
      const auto file=output_/"group-save.dufex";save_scene_extension(file,*document_,snapshot_);const auto reopened=load_scene_extension(file,roots_,document_->generation);
      if(reopened.snapshot.group_transforms!=snapshot_.group_transforms||reopened.snapshot.values!=snapshot_.values||group_node(*reopened.document,gt_id_)->world!=group_node(*document_,gt_id_)->world){finish_test(false,"Group DUFEX 重新加载丢失变换或原始轴心");return;}
      gt_phase_=2;return;
    }
    if(gt_phase_==8&&ready){
      if(state.instance_transforms!=gt_state_.instance_transforms||state.mesh_hashes!=gt_state_.mesh_hashes||state.sessions!=gt_state_.sessions){finish_test(false,"Group 恢复后矩阵、网格或会话不一致");return;}
      std::ofstream(output_/"group-check.json")<<nlohmann::json{{"pass",true},{"group",gt_id_},{"members",gt_members_.size()},{"properties",gt_motion_label_.empty()},{"save_roundtrip",gt_motion_label_.empty()},{"file_reopen",gt_motion_label_.empty()},{"max_world_matrix_error",gt_max_world_error_},{"undo_redo",true},{"cases",gt_checks_},{"restored",true}}.dump(2);finish_test(true);return;
    }
    const auto tool=!gt_motion_label_.empty()||gt_case_%3==0?GizmoTool::translate:gt_case_%3==1?GizmoTool::rotate:GizmoTool::scale;
    const auto space=gt_case_<3?GizmoSpace::local:GizmoSpace::world;const bool cancel=gt_case_>=6;
    const auto hwnd=FindWindowExW(HWND(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
    auto mouse=[&](UINT message,QPoint p){SendMessageW(hwnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,MAKELPARAM(p.x(),p.y()));};
    if(gt_phase_==2&&ready){
      if(gt_case_==9){const auto revision=snapshot_.revision;snapshot_=gt_initial_;snapshot_.revision=revision;send();gt_phase_=8;return;}
      chrome->findChild<QAction *>("GizmoTool"+QString::number(int(tool)))->trigger();for(auto *a:chrome->findChild<QToolBar *>("TransformModule")->actions())if(a->text()==(space==GizmoSpace::local?"Local":"World"))a->trigger();
      renderer_->gizmo({tool,space});if(!state.selection_bounds.empty)renderer_->frame(state.selection_bounds);gt_at_=now();gt_phase_=3;return;
    }
    if(gt_phase_==3&&ready&&now()-gt_at_>.3){
      if(!state.gizmo_available){finish_test(false,"选择 Group 后没有 Gizmo");return;}
      const int handle=!gt_motion_label_.empty()?gt_case_%3:tool==GizmoTool::scale?3:0;gt_lines_.clear();for(const auto &line:state.gizmo_shape.lines)if(line.handle==handle)gt_lines_.push_back(line);
      bool hit=false;for(size_t i=0;i<gt_lines_.size();++i){const auto &line=gt_lines_[i];QPoint p(qRound((line.a.x+line.b.x)*.5),qRound((line.a.y+line.b.y)*.5));if(state.gizmo_shape.hit(float(p.x()),float(p.y()))==handle&&p.x()>15&&p.y()>15&&p.x()<state.width-15&&p.y()<state.height-15){gt_point_=p;gt_line_=i;hit=true;break;}}
      if(!hit){finish_test(false,"Group 手柄无法命中");return;}
      gt_before_=snapshot_;gt_drag_state_=state;gt_history_=history_->stack().index();gt_steps_=0;mouse(WM_LBUTTONDOWN,gt_point_);gt_at_=now();gt_phase_=4;return;
    }
    if(gt_phase_==4&&now()-gt_at_>.07){
      ++gt_steps_;QPoint p=gt_point_;if(tool==GizmoTool::rotate){const auto &line=gt_lines_[(gt_line_+gt_steps_)%gt_lines_.size()];p={qRound((line.a.x+line.b.x)*.5),qRound((line.a.y+line.b.y)*.5)};}else p+=QPoint(gt_steps_*3,-gt_steps_);
      mouse(WM_MOUSEMOVE,p);gt_at_=now();if(gt_steps_<12)return;
      if(!state.pose_dragging||!state.pose_gizmo){finish_test(false,"Group 鼠标拖动未进入预览");return;}
      if(gt_case_==1)screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"group-drag.png").wstring()));
      gt_phase_=6;return;
    }
    if(gt_phase_==6&&now()-gt_at_>.25){
      QPoint p=gt_point_;if(tool==GizmoTool::rotate){const auto &line=gt_lines_[(gt_line_+gt_steps_)%gt_lines_.size()];p={qRound((line.a.x+line.b.x)*.5),qRound((line.a.y+line.b.y)*.5)};}else p+=QPoint(gt_steps_*3,-gt_steps_);
      if(state.gizmo_pointer.x!=p.x()||state.gizmo_pointer.y!=p.y()){mouse(WM_MOUSEMOVE,p);gt_at_=now();return;}
      const auto a=state.gizmo_shape.pivot,b=gt_drag_state_.gizmo_shape.pivot;gt_preview_shift_={a.x-b.x,a.y-b.y,a.z-b.z};
      std::ofstream(output_/("group-preview-"+std::to_string(gt_case_)+".json"))<<nlohmann::json{{"before_pivot",{b.x,b.y,b.z}},{"preview_pivot",{a.x,a.y,a.z}},{"pointer",{p.x(),p.y()}}}.dump(2);
      if(cancel)SendMessageW(hwnd,WM_KEYDOWN,VK_ESCAPE,0);mouse(WM_LBUTTONUP,p);gt_phase_=5;return;
    }
    if(gt_phase_==5&&ready&&!state.pose_dragging&&!state.pose_restoring&&(cancel||state.pose_commit>gt_drag_state_.pose_commit)&&now()-gt_at_>.3){
      const bool changed=snapshot_.group_transforms!=gt_before_.group_transforms;
      if(changed==cancel||snapshot_.values!=gt_before_.values||snapshot_.poses!=gt_before_.poses){finish_test(false,"Group 提交或取消不正确，或改写了子节点输入");return;}
      if(history_->stack().index()!=gt_history_+(cancel?0:1)){finish_test(false,"Group 拖动历史条数错误");return;}
      if(!gt_motion_label_.empty()){
        if(state.evaluation.morph_evaluations!=gt_drag_state_.evaluation.morph_evaluations||state.skinning.evaluations!=gt_drag_state_.skinning.evaluations||state.adapter.geometry_updates!=gt_drag_state_.adapter.geometry_updates||state.adapter.scene_updates!=gt_drag_state_.adapter.scene_updates){finish_test(false,"Group 平移触发冗余形变或全场景更新");return;}
        nlohmann::json check={{"case",gt_case_},{"preview_shift_m",{gt_preview_shift_.x,gt_preview_shift_.y,gt_preview_shift_.z}},{"objects",nlohmann::json::array()}};double error=0;
        for(size_t i=0;i<state.instance_transforms.size();++i){const auto before=gt_drag_state_.instance_transforms.at(i);auto expected=before;const bool member=std::find(gt_members_.begin(),gt_members_.end(),uint32_t(i))!=gt_members_.end();if(member&&!cancel){expected[3]+=gt_preview_shift_.x;expected[7]+=gt_preview_shift_.y;expected[11]+=gt_preview_shift_.z;}
          double e=0;for(size_t k=0;k<12;++k)e=std::max(e,std::abs(double(expected[k])-state.instance_transforms[i][k]));error=std::max(error,e);
          if(member||e>1e-4)check["objects"].push_back({{"id",document_->loaded.scene.instances[i].id},{"before",before},{"expected",expected},{"actual",state.instance_transforms[i]},{"error",e}});
        }
        check["max_error"]=error;gt_max_world_error_=std::max(gt_max_world_error_,error);std::ofstream(output_/("group-motion-"+std::to_string(gt_case_)+".json"))<<check.dump(2);
        if(error>1e-4||state.mesh_hashes!=gt_drag_state_.mesh_hashes){finish_test(false,"Group 平移提交后的世界矩阵或顶点与预览不一致");return;}
      }
      if(!cancel){const auto after=snapshot_.group_transforms;history_move(false);if(snapshot_.group_transforms!=gt_before_.group_transforms||selected_group_!=gt_id_){finish_test(false,"Group 鼠标操作撤销失败");return;}history_move(true);if(snapshot_.group_transforms!=after||selected_group_!=gt_id_){finish_test(false,"Group 鼠标操作重做失败");return;}}
      gt_checks_.push_back({{"tool",int(tool)},{"space",int(space)},{"cancel",cancel}});++gt_case_;gt_phase_=2;return;
    }
  }
