  water::Panel *water_panel_=nullptr;
  const Document *water_panel_document_=nullptr;
  const water::Water *water_panel_value_=nullptr;
  uint64_t water_list_revision_=UINT64_MAX,water_list_generation_=UINT64_MAX;
  std::string water_panel_id_;
  uint64_t water_status_revision_=UINT64_MAX,water_status_generation_=UINT64_MAX,water_stamp_=0;
  bool water_working_=false;
  std::string water_error_;
  QWidget *environment_tools_=nullptr;
  const water::Water *selected_water()const{
    if(!document_||selected_<0||size_t(selected_)>=document_->catalog.targets.size())return nullptr;const auto &id=document_->catalog.targets[size_t(selected_)].id;if(const auto *w=water::find(snapshot_.water_overrides,id))return w;return water::find(document_->waters,id);
  }
  void bind_water(){
    if(!water_panel_)return;const auto *w=selected_water();std::vector<water::Candidate> candidates;
    if(w){const auto state=renderer_->status();const bool current=state.generation==snapshot_.generation&&state.applied_revision==snapshot_.revision;
      candidates=water::candidates(*document_,current?state.instance_geometry_bounds:std::vector<ir::Bounds>{});water_list_generation_=current&&state.instance_geometry_bounds.size()==document_->loaded.scene.instances.size()?state.generation:UINT64_MAX;water_list_revision_=state.applied_revision;}
    water_panel_->bind(w,candidates);water_panel_document_=document_.get();water_panel_id_=w?w->id:std::string{};water_panel_value_=w;
  }
  void water_tick(){
    if(!water_panel_)return;const auto *w=selected_water();if(water_panel_document_!=document_.get()||water_panel_value_!=w||water_panel_id_!=(w?w->id:std::string{}))bind_water();if(!w||water_working_)return;
    if(water_list_generation_!=snapshot_.generation||water_list_revision_!=snapshot_.revision){const auto state=renderer_->status();if(state.generation==snapshot_.generation&&state.applied_revision==snapshot_.revision)bind_water();}
    if(!water_error_.empty()){water_panel_->message(text(water_error_));return;}
    if(w->config.coast&&(water_status_revision_!=snapshot_.revision||water_status_generation_!=snapshot_.generation)){water_stamp_=water::input_stamp(*document_,snapshot_);water_status_revision_=snapshot_.revision;water_status_generation_=snapshot_.generation;}
    water_panel_->message(!w->config.coast?QStringLiteral("静态水体 · 修改时间只计算指定帧。") : !w->cache?QStringLiteral("尚未计算交界。选择参与对象后点击“重算”。") : w->cache->stamp!=water_stamp_?QStringLiteral("需要重算：对象、姿态或场景结构已改变。当前保留上次交界结果。") : QStringLiteral("交界有效 · %1 个对象，%2 个局部网格").arg(w->cache->objects).arg(w->cache->cells.size()));
    if(w->cache&&w->cache->stamp==water_stamp_&&!w->cache->warnings.empty())water_panel_->message(QStringLiteral("交界已计算；")+text(w->cache->warnings.front()));
  }
  void generate_water(water::Config config,std::string id,bool recalculate){
    if(loading_||closing_)return;water_error_.clear();if(history_)history_->finish_gesture();if(id.empty())id="water-"+std::to_string(QDateTime::currentMSecsSinceEpoch());
    auto source=document_?document_:std::make_shared<Document>();auto saved=document_?snapshot_:initial_snapshot(*source);const auto expected_document=document_;const auto expected_revision=snapshot_.revision;
    loading_=water_working_=true;open_->setEnabled(false);project_action_->setEnabled(false);water_panel_->busy(true);water_panel_->message(QStringLiteral("正在准备水体…"));statusBar()->showMessage(QStringLiteral("正在后台生成水体…"));
    loader_=std::jthread([this,source,saved,id,config,expected_document,expected_revision,recalculate](std::stop_token stop)mutable{
      auto progress=[this,stop](const std::string &message){if(stop.stop_requested())throw std::runtime_error("水体计算已取消");QMetaObject::invokeMethod(this,[this,message]{if(!closing_){water_panel_->message(text(message));statusBar()->showMessage(text(message));}},Qt::QueuedConnection);};
      try{
        auto water=std::make_shared<water::Water>();water->id=id;water->config=config;
        if(const auto *old=water::find(water::effective(*source,saved),id);old&&water::same_shape(old->config,config))water->cache=old->cache;
        if(recalculate){
          progress("读取视口当前交界几何");const auto begin=std::chrono::steady_clock::now();
          auto pending=renderer_->water_inputs(saved.generation,saved.revision,*water);
          while(pending.wait_for(std::chrono::milliseconds(50))!=std::future_status::ready){if(stop.stop_requested())throw std::runtime_error("水体计算已取消");const auto state=renderer_->status();if(!state.error.empty()||!state.edit_error.empty())throw std::runtime_error(state.error+state.edit_error);}
          auto input=pending.get();nlohmann::json report={{"objects",nlohmann::json::array()},{"warnings",input.warnings}};for(const auto &o:input.objects)report["objects"].push_back({{"id",o.id},{"triangles",o.mesh.triangles.size()},{"volume",o.volume}});std::ofstream(output_/"water-coast-inputs.json")<<report.dump(2);
          water->cache=water::calculate(*water,input,water::input_stamp(*source,saved),progress);
          renderer_->trace("water_coast",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
        }
        water::check_manual_lod_budget(*water);
        if(stop.stop_requested())throw std::runtime_error("水体计算已取消");progress("生成静态水面");
        const auto *base=water::find(source->waters,id);const bool parameter_edit=base&&base->config.x==config.x&&base->config.y==config.y&&base->config.level==config.level;
        auto next=source;std::erase_if(saved.water_overrides,[&](const auto &w){return w->id==id;});
        if(parameter_edit)saved.water_overrides.push_back(water);
        else {next=std::make_shared<Document>(*source);water::install(*next,water,true);while(saved.values.size()<next->catalog.targets.size())saved.values.emplace_back();}
        QMetaObject::invokeMethod(this,[this,next,saved=std::move(saved),id,expected_document,expected_revision,recalculate,parameter_edit]()mutable{
          if(closing_)return;loading_=water_working_=false;open_->setEnabled(true);project_action_->setEnabled(true);water_panel_->busy(false);
          if(document_!=expected_document||snapshot_.revision!=expected_revision){water_error_="场景已改变，已丢弃旧计算结果，请重算。";water_panel_->message(text(water_error_));return;}
          auto edit=history_edit(recalculate?QStringLiteral("重算水体交界"):QStringLiteral("创建或修改水体"));saved.revision=next_revision();
          if(parameter_edit){snapshot_=std::move(saved);renderer_->edit(submitted_snapshot());}
          else {next->generation=++generation_;saved.generation=next->generation;next->loaded.scene.lights=saved.lights;parameters_->bind(nullptr,nullptr);document_=std::move(next);snapshot_=std::move(saved);rebuild_hierarchy();frame_pending_=false;renderer_->set_document(document_,submitted_snapshot(),false);}
          for(size_t t=0;t<document_->catalog.targets.size();++t)if(document_->catalog.targets[t].id==id+"/surface"){choose(int(t));break;}bind_water();statusBar()->showMessage(recalculate?QStringLiteral("水体交界已重算，可 Ctrl+Z 撤销。") : QStringLiteral("静态水体已更新。"),5000);
        },Qt::QueuedConnection);
      }catch(const std::exception &e){const std::string error=e.what();QMetaObject::invokeMethod(this,[this,error]{if(closing_)return;loading_=water_working_=false;water_error_=error;open_->setEnabled(true);project_action_->setEnabled(true);water_panel_->busy(false);bind_water();water_panel_->message(text(error));statusBar()->showMessage(text(error),15000);},Qt::QueuedConnection);}
    });
  }
  void create_options(bool environment){
    if(loading_)return;auto edit=history_edit(environment?QStringLiteral("创建环境设置"):QStringLiteral("创建色调设置"));
    if(!document_){document_=std::make_shared<Document>();document_->generation=++generation_;snapshot_=initial_snapshot(*document_);renderer_->set_document(document_,snapshot_,false);}
    auto &node=environment?snapshot_.options.environment:snapshot_.options.tonemapper;const bool created=node.id.empty();if(created){node=ir::default_options(environment);rebuild_hierarchy();send();}
    const int target=environment?-2:-3;for(QTreeWidgetItemIterator it(hierarchy_);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()==target){hierarchy_->setCurrentItem(*it);select(target);break;}
    if(auto *panel=findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"))){panel->show();panel->raise();}
  }
