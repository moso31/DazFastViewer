  bool edit_resume_test_=false;
  int er_phase_=0,er_round_=0,er_target_=-1,er_skin_=-1,er_morph_=-1;
  uint64_t er_revision_=0;
  double er_at_=0,er_last_=0;
  std::map<uint64_t,double> er_inputs_;
  Snapshot er_initial_;
  RenderStatus er_before_,er_initial_state_;
  QPointF er_point_;
  QPointer<QSlider> er_slider_;
  nlohmann::json er_checks_=nlohmann::json::array();
  void edit_resume_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000){finish_test(false,"连续编辑验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation)return;
    const bool full=state.applied_revision==snapshot_.revision&&state.presented_revision==snapshot_.revision&&!state.pose_restoring&&!state.preview&&state.samples>=4;
    std::ofstream(output_/"edit-resume-progress.json")<<nlohmann::json{{"phase",er_phase_},{"round",er_round_},{"revision",snapshot_.revision},{"applied",state.applied_revision},{"presented",state.presented_revision},{"checks",er_checks_}}.dump(2);
    auto mouse=[&](QWidget *widget,QEvent::Type type,QPointF p){QMouseEvent event(type,p,widget->mapToGlobal(p.toPoint()),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(widget,&event);};
    if(er_phase_==0){
      if(!full)return;
      for(size_t s=0;s<document_->skeletons.skins.size()&&er_target_<0;++s)for(size_t t=0;t<document_->catalog.targets.size();++t){const auto &skin=document_->skeletons.skins[s];if(skin.instance==document_->catalog.targets[t].instance&&skin.joints.front().id.find("Genesis8")==0&&document_->catalog.targets[t].conform_target.empty()){er_target_=int(t);er_skin_=int(s);break;}}
      if(er_target_<0){finish_test(false,"没有 Genesis 8 角色");return;}
      er_initial_=snapshot_;er_initial_state_=state;choose(er_target_);findChild<QDockWidget*>(QStringLiteral("PowerPose"))->raise();powerpose_->page(0);er_phase_=1;return;
    }
    if(er_phase_==1){
      if(er_round_==0&&state.applied_revision!=snapshot_.revision)return;
      refresh_powerpose(state);er_before_=state;er_revision_=snapshot_.revision;er_point_=powerpose_->point_position("B09");er_at_=now();
      mouse(powerpose_->canvas(),QEvent::MouseButtonPress,er_point_);mouse(powerpose_->canvas(),QEvent::MouseMove,er_point_+QPointF(0,er_round_%2?-6:9));er_phase_=2;return;
    }
    if(er_phase_==2){
      if(now()-er_at_>1){finish_test(false,"PowerPose 连续拖动超过 1 秒没有预览");return;}
      if(!state.pose_dragging||state.pose_previews==er_before_.pose_previews)return;
      er_checks_.push_back({{"kind","powerpose"},{"round",er_round_},{"input_ms",(now()-er_at_)*1000},{"before_beauty",er_before_.presented_revision<snapshot_.revision}});
      mouse(powerpose_->canvas(),QEvent::MouseButtonRelease,er_point_+QPointF(0,er_round_%2?-6:9));er_at_=now();er_phase_=3;return;
    }
    if(er_phase_==3){
      if(now()-er_at_>1){finish_test(false,"PowerPose 松手后超过 1 秒不能再次编辑");return;}
      if(snapshot_.revision<=er_revision_)return;
      er_checks_.back()["release_to_next_press_ms"]=(now()-er_at_)*1000;
      if(state.presented_revision==snapshot_.revision){finish_test(false,"未覆盖完整画面之前的再次拖动");return;}
      if(++er_round_<4)er_phase_=1;else er_phase_=4;return;
    }
    if(er_phase_==4){
      if(!full)return;choose(er_target_);
      const auto &target=document_->catalog.targets[er_target_];for(size_t m=0;m<target.morphs.size();++m)if(target.morphs[m].channel_id=="FBMBodySize"){er_morph_=int(m);break;}
      if(er_morph_<0){finish_test(false,"测试场景没有 FBMBodySize");return;}
      parameters_->query(QStringLiteral("Body Size"));parameters_->select_parameter(er_morph_);findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"))->raise();er_phase_=5;return;
    }
    if(er_phase_==5){
      const auto &morph=document_->catalog.targets[er_target_].morphs[er_morph_];
      for(auto *spin:parameters_->findChildren<QDoubleSpinBox *>("valueSpin"))if(spin->property("parameterId").toString().toStdString()==morph.source+"#"+morph.id)er_slider_=spin->parentWidget()->findChild<QSlider *>("valueSlider");
      if(!er_slider_){finish_test(false,"未挂载 Body Size 控件");return;}
      er_point_=er_slider_->rect().center();er_round_=0;er_before_=state;er_at_=er_last_=now();mouse(er_slider_,QEvent::MouseButtonPress,er_point_);er_phase_=6;return;
    }
    if(er_phase_==6){
      if(er_round_&&state.applied_revision>er_before_.applied_revision&&er_inputs_.contains(state.applied_revision)){er_checks_.push_back({{"kind","morph"},{"round",er_round_},{"input_ms",(now()-er_inputs_.at(state.applied_revision))*1000},{"before_beauty",state.presented_revision<state.applied_revision}});er_before_=state;}
      if(now()-er_last_<.1)return;
      if(er_round_==16){mouse(er_slider_,QEvent::MouseButtonRelease,er_point_+QPointF(20,0));er_phase_=7;return;}
      ++er_round_;er_at_=er_last_=now();mouse(er_slider_,QEvent::MouseMove,er_point_+QPointF(er_round_%2?20:10,0));er_inputs_[snapshot_.revision]=er_at_;return;
    }
    if(er_phase_==7){
      if(!full)return;
      const auto count=std::count_if(er_checks_.begin(),er_checks_.end(),[](const auto &v){return v.at("kind")=="morph";});
      if(count<2||snapshot_.values[er_target_].morphs[er_morph_]==er_initial_.values[er_target_].morphs[er_morph_]){finish_test(false,"Morph 连续输入没有更新预览");return;}
      snapshot_.values=er_initial_.values;snapshot_.poses=er_initial_.poses;send();er_phase_=8;return;
    }
    if(er_phase_==8&&full){
      const bool exact=state.mesh_hashes==er_initial_state_.mesh_hashes&&state.instance_transforms==er_initial_state_.instance_transforms;
      er_checks_.push_back({{"kind","restore"},{"exact",exact},{"sessions",state.sessions}});screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"restored.png").wstring()));finish_test(exact,exact?"":"完整修正后未恢复原始几何");
    }
  }
