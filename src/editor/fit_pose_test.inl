  QString fit_pose_label_;
  Snapshot fit_pose_initial_;
  std::vector<runtime::JointPose> fit_pose_effective_;
  int fit_pose_skin_=-1,fit_pose_foot_=-1,fit_pose_shin_=-1,fit_pose_case_=0;
  double fit_pose_begin_=0,fit_pose_applied_=0;
  nlohmann::json fit_pose_checks_=nlohmann::json::array();
  void fit_pose_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000){finish_test(false,"适配姿势首帧测试超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation)return;
    if(fit_pose_begin_&&state.applied_revision==snapshot_.revision&&!fit_pose_applied_)fit_pose_applied_=now();
    const bool ready=state.presented_revision==snapshot_.revision&&state.presented_epoch==state.requested_epoch&&!state.preview&&state.samples>=1;
    if(!ready)return;
    static const char *names[]={"foot_fk","shin_fk","leg_ik","restore"};
    if(fit_pose_skin_<0){
      int host=-1;for(const auto &f:document_->catalog.targets)if(text(f.label)==fit_pose_label_)for(size_t h=0;h<document_->catalog.targets.size();++h){const auto &t=document_->catalog.targets[h];if(f.conform_target=="#"+t.id.substr(0,t.id.rfind('/')))host=int(h);}
      if(host<0){finish_test(false,"适配姿势测试未找到服装宿主");return;}
      fit_pose_skin_=document_->formulas.graphs[host].skin;
      if(fit_pose_skin_<0){finish_test(false,"适配姿势测试宿主缺少骨架");return;}
      const auto &skin=document_->skeletons.skins[fit_pose_skin_];for(size_t j=0;j<skin.joints.size();++j){if(skin.joints[j].name=="lFoot")fit_pose_foot_=int(j);if(skin.joints[j].name=="lShin")fit_pose_shin_=int(j);}
      if(fit_pose_foot_<0||fit_pose_shin_<0){finish_test(false,"适配姿势测试缺少脚部骨骼");return;}
      fit_pose_initial_=snapshot_;fit_pose_effective_=state.effective_poses.at(fit_pose_skin_);
    }else{
      fit_pose_checks_.push_back({{"case",names[fit_pose_case_]},{"applied_ms",(fit_pose_applied_-fit_pose_begin_)*1000},{"first_frame_ms",(state.present_time-fit_pose_begin_)*1000},{"revision",snapshot_.revision},{"sessions",state.sessions}});
      std::ofstream(output_/"fit-pose-progress.json")<<fit_pose_checks_.dump(2);
      if(++fit_pose_case_==4){finish_test(true);return;}
    }
    snapshot_.poses=fit_pose_initial_.poses;auto &pose=snapshot_.poses[fit_pose_skin_];const auto &skin=document_->skeletons.skins[fit_pose_skin_];
    if(fit_pose_case_==0)pose[fit_pose_foot_].rotation_degrees.x+=5;
    if(fit_pose_case_==1)pose[fit_pose_shin_].rotation_degrees.x+=8;
    if(fit_pose_case_==2){auto solved=fit_pose_effective_;runtime::IkGoal goal{fit_pose_foot_,true,runtime::joint_point(skin,solved,fit_pose_foot_,true)};goal.position.x+=.01f;goal.position.z+=.015f;
      const auto chain=runtime::ik_chain(skin,fit_pose_foot_);const auto result=runtime::solve_ik(runtime::ik_limits(skin,pose,solved),solved,chain,std::span(&goal,1));if(!result.changed){finish_test(false,"适配姿势测试 IK 未生效");return;}pose=runtime::ik_input(pose,fit_pose_effective_,solved);}
    renderer_->trace((std::string("fit_pose_")+names[fit_pose_case_]).c_str());fit_pose_applied_=0;fit_pose_begin_=now();send();
  }
