  bool cloud_test_=false;
  int cloud_test_stage_=0;
  size_t cloud_test_syncs_=0,cloud_test_geometry_=0;
  const Document *cloud_test_document_=nullptr;
  int cloud_test_collider_=-1;
  ir::Bounds cloud_test_bounds_;
  nlohmann::json cloud_checks_=nlohmann::json::array();
  void cloud_test_tick(const RenderStatus &state){
    auto fail=[&](const std::string &why){finish_test(false,why);cloud_test_=false;};
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000){fail("体积云界面验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()){fail(state.error+state.edit_error);return;}if(loading_)return;
    auto check=[](bool value,const char *why){if(!value)throw std::runtime_error(why);};
    try{
      if(cloud_test_stage_==0){
        if(document_&&!document_->catalog.targets.empty()){
          if(state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4)return;
          float tallest=0;for(size_t t=0;t<state.bounds.size();++t){const auto &b=state.bounds[t];if(!b.empty&&snapshot_.values[t].visible&&b.maximum.z-b.minimum.z>tallest){tallest=b.maximum.z-b.minimum.z;cloud_test_bounds_=b;cloud_test_collider_=int(t);}}
          cloud_checks_.push_back({{"stage","scene-baseline"},{"triangles",state.adapter.triangles},{"gpu_bytes",state.gpu_device_bytes},{"samples",state.samples}});screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"scene-baseline.png").wstring()));
        }
        auto *action=findChild<QAction *>("CreateCloud");check(action,"cloud menu missing");action->trigger();++cloud_test_stage_;return;
      }
      if(!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4)return;
      auto record=[&](const char *name){cloud_checks_.push_back({{"stage",name},{"sessions",state.sessions},{"scene_updates",state.adapter.scene_updates},{"geometry_updates",state.adapter.geometry_updates},{"triangles",state.adapter.triangles}});std::ofstream(output_/"cloud-checks.json")<<cloud_checks_.dump(2);};
      if(cloud_test_stage_==1){check(document_->clouds.size()==1&&cloud_panel_->isVisible(),"cloud creation/selection failed");record("created");auto c=selected_cloud()->config;c.height=0;c.width=c.length=500;c.thickness=120;c.scale=90;
        if(cloud_test_collider_>=0){const auto b=cloud_test_bounds_;const auto p=b.center();const double height=b.maximum.z-b.minimum.z;c.x=p.x;c.y=p.y;c.height=p.z;c.thickness=std::max(20.,height*.4);c.width=c.length=std::max(200.,height*3);c.scale=std::max(10.,height*.2);c.density=3/c.thickness;c.coverage=.8;c.padding=std::max(2.,height*.03);c.softness=std::max(2.,height*.03);c.velocity_z=-height*.1;const ObjectHierarchy h(*document_);c.sources={h.targets.at(size_t(cloud_test_collider_))};}
        const auto *before=document_.get();const auto id=selected_cloud()->id;
        generate_cloud(c,id);c.height+=10;generate_cloud(c,id);c.thickness+=20;generate_cloud(c,id);
        check(!loading_&&cloud_panel_->isEnabled()&&document_.get()==before,"cloud burst disabled input or rebuilt document");check(selected_cloud()->config==c,"cloud burst lost latest settings");}
      else if(cloud_test_stage_==2){record("placed");const auto c=selected_cloud()->config;renderer_->camera_view({float(c.x),float(c.y),float(c.height),float(c.width*1.2),.1f,.5f});cloud_test_document_=document_.get();cloud_test_syncs_=state.adapter.scene_updates;cloud_test_geometry_=state.adapter.geometry_updates;cloud_panel_->findChild<QDoubleSpinBox *>("time")->setValue(6.5);}
      else if(cloud_test_stage_==3){check(selected_cloud()&&selected_cloud()->config.time==6.5,"cloud time not submitted");check(document_.get()==cloud_test_document_&&state.adapter.scene_updates==cloud_test_syncs_&&state.adapter.geometry_updates==cloud_test_geometry_,"time edit rebuilt scene/geometry");record("time");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"cloud-time.png").wstring()));history_->undo();}
      else if(cloud_test_stage_==4){check(cloud::effective(*document_,snapshot_).front()->config.time==0,"cloud undo failed");record("undo");history_->redo();}
      else if(cloud_test_stage_==5){check(cloud::effective(*document_,snapshot_).front()->config.time==6.5,"cloud redo failed");record("redo");save_scene_extension(output_/"cloud-editor.dufex",*document_,snapshot_);load(output_/"cloud-editor.dufex");}
      else if(cloud_test_stage_==6){check(document_->clouds.size()==1&&document_->clouds.front()->config.time==6.5,"cloud save/reopen failed");for(size_t t=0;t<document_->catalog.targets.size();++t)if(cloud::find(document_->clouds,document_->catalog.targets[t].id)){choose(int(t));break;}check(cloud_panel_->isVisible()&&!physics_panel_->isVisible()&&extension_target()<0,"cloud panel bindings incorrect");record("reopened");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"cloud-panel.png").wstring()));
        if(cloud_test_collider_<0){finish_test(true);cloud_test_=false;return;}auto edit=history_edit(QStringLiteral("验证云层碰撞体隐藏"));snapshot_.values.at(cloud_test_collider_).visible=false;send();}
      else if(cloud_test_stage_==7){record("collider-hidden");check(!snapshot_.values.at(cloud_test_collider_).visible,"collision visibility edit failed");history_->undo();}
      else if(cloud_test_stage_==8){record("collider-visible");check(snapshot_.values.at(cloud_test_collider_).visible,"collision visibility undo failed");auto edit=history_edit(QStringLiteral("验证穿透对象移动"));snapshot_.values.at(cloud_test_collider_).transform.translation_cm.x+=cloud_test_bounds_.extent()*25;send();}
      else if(cloud_test_stage_==9){record("collider-moved");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"cloud-collider-moved.png").wstring()));history_->undo();}
      else if(cloud_test_stage_==10){record("collider-restored");finish_test(true);cloud_test_=false;return;}
      ++cloud_test_stage_;
    }catch(const std::exception &e){fail(e.what());}
  }
