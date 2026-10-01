  bool history_test_=false;
  int history_stage_=0,history_expected_=0;
  std::optional<EditState> history_before_load_;
  CameraState history_camera_;
  bool history_visibility_=true;
  nlohmann::json history_checks_=nlohmann::json::array();
  void history_test_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>240000){finish_test(false,"撤销界面验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()){finish_test(false,state.error+state.edit_error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4)return;
    auto require=[](bool ok,const char *message){if(!ok)throw std::runtime_error(message);};
    try {
      if(history_stage_==0){
        choose(0);history_->clear();history_camera_=renderer_->input_camera();
        auto *history_bar=chrome->findChild<QToolBar *>("HistoryModule");require(history_bar,"缺少撤销重做工具组");
        auto *undo_button=qobject_cast<QToolButton *>(history_bar->widgetForAction(undo_action_)),*redo_button=qobject_cast<QToolButton *>(history_bar->widgetForAction(redo_action_));
        std::ofstream(output_/"history-toolbar-layout.json")<<nlohmann::json({{"width",history_bar->width()},{"minimum_width",history_bar->minimumWidth()},{"hint",history_bar->sizeHint().width()},{"undo_visible",undo_button&&undo_button->isVisible()},{"redo_visible",redo_button&&redo_button->isVisible()}}).dump(2);
        require(undo_button&&redo_button&&!undo_button->isEnabled()&&!redo_button->isEnabled(),"空历史的按钮状态错误");
        require(undo_button->isVisible()&&redo_button->isVisible(),"撤销重做按钮被折叠隐藏");
        const auto toolbar_before=capture_edit();set_visible(0,!snapshot_.values[0].visible);const auto toolbar_after=capture_edit();require(undo_button->isEnabled()&&!redo_button->isEnabled(),"编辑后按钮状态错误");
        undo_button->click();require(same_edit(capture_edit(),toolbar_before)&&!undo_button->isEnabled()&&redo_button->isEnabled(),"顶部撤销按钮无效");
        redo_button->click();require(same_edit(capture_edit(),toolbar_after)&&undo_button->isEnabled()&&!redo_button->isEnabled(),"顶部重做按钮无效");
        undo_button->click();history_->clear();history_checks_.push_back("toolbar_undo_redo");
        auto roundtrip=[&](const char *name,const std::function<void()> &action){const auto before=capture_edit();const auto count=history_->stack().index();action();history_->finish_gesture();const auto after=capture_edit();require(history_->stack().count()==count+1,(std::string(name)+": 没有记录一条操作").c_str());history_->undo();require(same_edit(capture_edit(),before),(std::string(name)+": 撤销错误").c_str());history_->redo();require(same_edit(capture_edit(),after),(std::string(name)+": 重做错误").c_str());history_checks_.push_back(name);};
        roundtrip("visibility",[&]{set_visible(0,!snapshot_.values[0].visible);});
        roundtrip("translation",[&]{require(parameters_->edit_control("transform/0",17),"缺少位移参数");});
        roundtrip("general_scale",[&]{require(parameters_->edit_control("transform/general_scale",110.25),"缺少整体缩放");});
        const auto &morphs=document_->catalog.targets[0].morphs;const auto found=std::find_if(morphs.begin(),morphs.end(),[](const auto &m){return m.label=="A";});require(found!=morphs.end(),"夹具缺少 Morph A");const size_t morph=size_t(found-morphs.begin());
        roundtrip("morph_drag",[&]{history_interaction(true,quintptr(parameters_));for(float v:{.2f,.4f,.6f})set_morph(morph,v);history_interaction(false,quintptr(parameters_));});
        manual_morph_->setChecked(true);roundtrip("draft",[&]{set_morph(morph,.8);});roundtrip("apply_draft",[&]{apply_parameters();});manual_morph_->setChecked(false);
        roundtrip("favorites",[&]{parameters_->favorite_changed("","A",true);});
        roundtrip("ground_ratio",[&]{ground_panel_->changed(.3,2.5,true);});
        roundtrip("extension",[&]{enable_extension();});roundtrip("extension_parameter",[&]{auto next=snapshot_.values[0].extension;next.sensitivity+=1;change_extension(next,false,0);});
        roundtrip("subdivision",[&]{set_subdivision(0,subdivision_level(*document_,snapshot_,0)==0?1:0);});
        materials_->bind(document_,&snapshot_,0);roundtrip("material",[&]{auto *spin=materials_->findChild<QDoubleSpinBox *>("material/roughness");require(spin,"缺少材质参数");spin->setValue(.67);});
        roundtrip("reset",[&]{reset_selected();});
        roundtrip("new_light",[&]{add_light();});roundtrip("light_power",[&]{light_power_->setValue(333);});roundtrip("delete_light",[&]{delete_selection();});
        choose(0);roundtrip("delete_model",[&]{delete_selection();});history_->undo();choose(0);
        roundtrip("new_scene",[&]{clear_scene();});history_->undo();choose(0);
        // 回传的姿态必须先进入历史；紧接着的撤销恢复该姿态而不是更早的操作。
        if(!snapshot_.poses.empty()){
          roundtrip("pose_commit",[&]{RenderStatus commit;commit.pose_commit=pose_commit_+1;commit.pose_generation=document_->generation;commit.pose_revision=snapshot_.revision;commit.pose_skin=0;commit.pose_input=snapshot_.poses[0];commit.pose_input[0].rotation_degrees.z+=5;require(accept_pose_commit(commit),"姿态回传被拒绝");});
          const auto current=capture_edit();RenderStatus stale;stale.pose_commit=pose_commit_+1;stale.pose_generation=document_->generation;stale.pose_revision=snapshot_.revision-1;stale.pose_skin=0;stale.pose_input=snapshot_.poses[0];stale.pose_input[0].rotation_degrees.z=99;require(!accept_pose_commit(stale)&&same_edit(capture_edit(),current),"过期姿态覆盖了撤销结果");history_checks_.push_back("stale_pose_rejected");
        }
        extension_file_=output_/"history-scene.dufex";save_extension();history_before_load_=capture_edit();history_expected_=history_->stack().count()+1;load(extension_file_);history_stage_=1;return;
      }
      if(history_stage_==1){require(history_->stack().count()==history_expected_,"替换场景未记入历史");history_->undo();require(same_edit(capture_edit(),*history_before_load_),"替换场景无法恢复原场景");history_->redo();history_checks_.push_back("replace_scene");history_stage_=2;return;}
      if(history_stage_==2){choose(0);history_->clear();history_visibility_=snapshot_.values[0].visible;set_visible(0,!history_visibility_);renderer_->keyboard(VK_CONTROL,true);renderer_->keyboard('Z',true);renderer_->keyboard('Z',false);renderer_->keyboard(VK_CONTROL,false);history_stage_=3;return;}
      if(history_stage_==3){require(snapshot_.values[0].visible==history_visibility_&&history_->stack().index()==0,"原生视口 Ctrl+Z 无效");renderer_->keyboard(VK_CONTROL,true);renderer_->keyboard('Y',true);renderer_->keyboard('Y',false);renderer_->keyboard(VK_CONTROL,false);history_stage_=4;return;}
      if(history_stage_==4){require(snapshot_.values[0].visible!=history_visibility_&&history_->stack().index()==1,"原生视口 Ctrl+Y 无效");history_checks_.push_back("native_viewport_shortcuts");history_->undo();history_stage_=5;return;}
      const auto camera=renderer_->input_camera();require(camera.target.x==history_camera_.target.x&&camera.target.y==history_camera_.target.y&&camera.target.z==history_camera_.target.z&&camera.distance==history_camera_.distance&&camera.yaw==history_camera_.yaw&&camera.pitch==history_camera_.pitch,"撤销操作改变了相机");
      std::ofstream(output_/"history-checks.json")<<nlohmann::json({{"checks",history_checks_},{"revision",snapshot_.revision},{"sessions",state.sessions},{"history_limit",history_->stack().undoLimit()}}).dump(2);
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"history-editor.png").wstring()));finish_test(true);
    }catch(const std::exception &e){finish_test(false,e.what());}
  }
