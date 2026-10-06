  cloud::Panel *cloud_panel_=nullptr;
  const Document *cloud_panel_document_=nullptr;
  const cloud::Cloud *cloud_panel_value_=nullptr;
  uint64_t cloud_list_revision_=UINT64_MAX,cloud_list_generation_=UINT64_MAX;
  const cloud::Cloud *selected_cloud()const{
    if(!document_||selected_<0||size_t(selected_)>=document_->catalog.targets.size())return nullptr;const auto &id=document_->catalog.targets[size_t(selected_)].id;if(const auto *v=cloud::find(snapshot_.cloud_overrides,id))return v;return cloud::find(document_->clouds,id);
  }
  void bind_cloud(){
    if(!cloud_panel_)return;const auto *v=selected_cloud();std::vector<cloud::Candidate> candidates;
    if(v){const auto state=renderer_->status();const bool current=state.generation==snapshot_.generation&&state.applied_revision==snapshot_.revision;
      candidates=cloud::candidates(*document_,current?state.instance_geometry_bounds:std::vector<ir::Bounds>{});cloud_list_generation_=current&&state.instance_geometry_bounds.size()==document_->loaded.scene.instances.size()?state.generation:UINT64_MAX;cloud_list_revision_=state.applied_revision;}
    cloud_panel_->bind(v,candidates);cloud_panel_document_=document_.get();cloud_panel_value_=v;
  }
  void cloud_tick(){
    if(!cloud_panel_)return;if(cloud_panel_document_!=document_.get()||cloud_panel_value_!=selected_cloud())bind_cloud();
    if(selected_cloud()&&(cloud_list_generation_!=snapshot_.generation||cloud_list_revision_!=snapshot_.revision)){const auto state=renderer_->status();if(state.generation==snapshot_.generation&&state.applied_revision==snapshot_.revision)bind_cloud();}
  }
  void generate_cloud(cloud::Config config,std::string id={}){
    if(loading_||closing_)return;try{cloud::validate(config);}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),10000);return;}
    if(history_)history_->finish_gesture();if(id.empty())id="cloud-"+std::to_string(QDateTime::currentMSecsSinceEpoch());
    auto value=std::make_shared<cloud::Cloud>();value->id=id;value->config=config;
    const auto *base=document_?cloud::find(document_->clouds,id):nullptr;
    if(base&&base->config.x==config.x&&base->config.y==config.y&&base->config.height==config.height&&base->config.thickness==config.thickness){
      auto edit=history_edit(QStringLiteral("修改体积云"));std::erase_if(snapshot_.cloud_overrides,[&](const auto &v){return v->id==id;});snapshot_.cloud_overrides.push_back(value);send();return;
    }
    const auto source=document_?document_:std::make_shared<Document>();auto saved=document_?snapshot_:initial_snapshot(*source);const auto expected=document_;const auto revision=snapshot_.revision;
    loading_=true;open_->setEnabled(false);project_action_->setEnabled(false);cloud_panel_->setEnabled(false);statusBar()->showMessage(QStringLiteral("正在后台创建体积云…"));
    loader_=std::jthread([this,source,saved=std::move(saved),value,expected,revision](std::stop_token stop)mutable{
      try{auto next=std::make_shared<Document>(*source);cloud::install(*next,value);std::erase_if(saved.cloud_overrides,[&](const auto &v){return v->id==value->id;});while(saved.values.size()<next->catalog.targets.size())saved.values.emplace_back();if(stop.stop_requested())return;
        QMetaObject::invokeMethod(this,[this,next,saved=std::move(saved),id=value->id,expected,revision]()mutable{
          if(closing_)return;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);cloud_panel_->setEnabled(true);
          if(document_!=expected||snapshot_.revision!=revision){statusBar()->showMessage(QStringLiteral("场景已改变，体积云旧结果已丢弃。"),8000);return;}
          auto edit=history_edit(QStringLiteral("创建或调整体积云范围"));next->generation=++generation_;saved.generation=next->generation;saved.revision=next_revision();next->loaded.scene.lights=saved.lights;parameters_->bind(nullptr,nullptr);document_=std::move(next);snapshot_=std::move(saved);rebuild_hierarchy();frame_pending_=false;renderer_->set_document(document_,submitted_snapshot(),!expected||expected->loaded.scene.instances.empty());
          for(size_t t=0;t<document_->catalog.targets.size();++t)if(document_->catalog.targets[t].id==id+"/volume"){choose(int(t));break;}bind_cloud();if(auto *dock=findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"))){dock->show();dock->raise();}statusBar()->showMessage(QStringLiteral("体积云已创建，可调节高度、密度、时间并勾选碰撞对象。"),6000);
        },Qt::QueuedConnection);
      }catch(const std::exception &e){const std::string error=e.what();QMetaObject::invokeMethod(this,[this,error]{if(closing_)return;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);cloud_panel_->setEnabled(true);bind_cloud();statusBar()->showMessage(text(error),15000);},Qt::QueuedConnection);}
    });
  }
