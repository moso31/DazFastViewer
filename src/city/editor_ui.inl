  city::Panel *city_panel_=nullptr;
  const Document *city_panel_document_=nullptr;
  city::Views city_panel_views_;
  void focus_city(const std::string &id){if(!document_)return;for(const auto &c:document_->cities)if(c->id==id){ir::Bounds bounds;const GroupFrames frames(*document_,snapshot_.group_transforms);const auto transform=frames.delta(id)*ir::Transform::translate({float(c->config.origin_x),float(c->config.origin_y),float(c->config.origin_z)});for(const auto &cell:c->cells)for(int k=0;k<8;++k)bounds.add(transform.point({k&1?cell.bounds.maximum.x:cell.bounds.minimum.x,k&2?cell.bounds.maximum.y:cell.bounds.minimum.y,k&4?cell.bounds.maximum.z:cell.bounds.minimum.z}));renderer_->frame(bounds);break;}}
  void publish_city(std::shared_ptr<Document> next,Snapshot snapshot){
    city::prune(*next,snapshot);prune_material_overrides(next->loaded.scene,snapshot.material_overrides);next->generation=++generation_;snapshot.generation=next->generation;snapshot.revision=next_revision();
    parameters_->bind(nullptr,nullptr);document_=std::move(next);snapshot_=std::move(snapshot);select(-1);rebuild_hierarchy();frame_pending_=false;load_error_.clear();prune_pending_parameters();renderer_->set_document(document_,submitted_snapshot(),false);
  }
  void generate_city(city::Config config,std::string id){
    if(loading_||closing_)return;if(history_)history_->finish_gesture();if(id.empty())id="city-"+std::to_string(QDateTime::currentMSecsSinceEpoch());
    loading_=true;open_->setEnabled(false);project_action_->setEnabled(false);city_panel_->busy(true);city_panel_->message(QStringLiteral("正在后台准备城市…"));const auto roots=roots_;
    loader_=std::jthread([this,config=std::move(config),id,roots](std::stop_token stop){
      try{auto generated=city::generate(config,id,roots,[this,stop](const std::string &message){if(stop.stop_requested())throw std::runtime_error("城市生成已取消");QMetaObject::invokeMethod(this,[this,message]{if(!closing_)city_panel_->message(text(message));},Qt::QueuedConnection);});
        if(stop.stop_requested())throw std::runtime_error("城市生成已取消");
        QMetaObject::invokeMethod(this,[this,id,generated=std::make_shared<city::Generated>(std::move(generated))]()mutable{
          if(closing_)return;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);city_panel_->busy(false);
          try{auto edit=history_edit(QStringLiteral("生成城市"));auto next=document_?std::make_shared<Document>(*document_):std::make_shared<Document>();auto snapshot=document_?snapshot_:initial_snapshot(*next);const bool empty=next->loaded.scene.instances.empty();city::install(*next,std::move(*generated));
            if(empty&&snapshot.lights.empty()){ir::AreaLight sun;sun.id="city-preview-sun";sun.kind=ir::LightKind::distant;sun.power={2.5f,2.3f,2.1f};sun.angle=.0093f;sun.transform.value={1,0,0,0,0,.8f,.6f,0,0,-.6f,.8f,0};snapshot.lights.push_back(sun);next->loaded.scene.lights=snapshot.lights;}
            publish_city(std::move(next),std::move(snapshot));focus_city(id);city_panel_->message(QStringLiteral("城市已生成。可切换 LOD 对照；Ctrl+S 保存为 DUFEX，Ctrl+Z 撤销。"));
          }catch(const std::exception &e){city_panel_->message(text(e.what()));}
        },Qt::QueuedConnection);
      }catch(const std::exception &e){const std::string error=e.what();QMetaObject::invokeMethod(this,[this,error]{if(closing_)return;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);city_panel_->busy(false);city_panel_->message(text(error));},Qt::QueuedConnection);}
    });
  }
  void install_city_ui(QMenu *create){
    city_panel_=new city::Panel(city::default_directory(roots_));auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(city_panel_);auto *panel=dock(QStringLiteral("城市 PCG"),scroll,Qt::RightDockWidgetArea);panel->setObjectName("CityPCG");if(auto *materials=findChild<QDockWidget *>("Materials"))tabifyDockWidget(materials,panel);panel->hide();
    auto *action=create->addAction(QStringLiteral("城市 PCG…"));action->setObjectName("OpenCityPCG");connect(action,&QAction::triggered,this,[panel]{panel->show();panel->raise();});
    city_panel_->generate=[this](city::Config config,std::string id){generate_city(std::move(config),std::move(id));};city_panel_->cancel=[this]{loader_.request_stop();};city_panel_->focus=[this](const std::string &id){focus_city(id);};
    city_panel_->view_changed=[this](const std::string &id,city::View view){if(loading_||!document_)return;auto edit=history_edit(QStringLiteral("城市 LOD"));snapshot_.city_views[id]=view;send();};
    city_panel_->remove=[this](const std::string &id){if(loading_||!document_||id.empty())return;try{auto edit=history_edit(QStringLiteral("移除城市"));auto next=std::make_shared<Document>(*document_);city::remove(*next,id);publish_city(std::move(next),snapshot_);}catch(const std::exception &e){city_panel_->message(text(e.what()));}};
  }
  void city_tick(){if(!city_panel_||!renderer_)return;if(city_panel_document_!=document_.get()||city_panel_views_!=snapshot_.city_views){city_panel_->bind(document_?document_->cities:city::Cities{},snapshot_.city_views);city_panel_document_=document_.get();city_panel_views_=snapshot_.city_views;}city_panel_->status(renderer_->status().city);}
