  bool city_test_=false;int city_test_stage_=0;std::string city_test_id_;uint64_t city_test_epoch_=0;
  nlohmann::json city_checks_=nlohmann::json::array();
  void city_test_tick(const RenderStatus &state){
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>240000){finish_test(false,"城市界面验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()){finish_test(false,state.error+state.edit_error);return;}
    if(loading_||!document_)return;
    auto check=[](bool ok,const char *why){if(!ok)throw std::runtime_error(why);};
    try{
      if(city_test_stage_==0){auto *action=findChild<QAction *>("OpenCityPCG");check(action,"City menu missing");action->trigger();for(auto *button:city_panel_->findChildren<QPushButton *>())if(button->text()==QStringLiteral("扫描建筑原型"))button->click();auto *button=city_panel_->findChild<QPushButton *>("CityGenerate");check(button,"City generate button missing");button->click();check(loading_,"City panel did not start background generation");++city_test_stage_;return;}
      if(state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4)return;
      if(state.camera.epoch<city_test_epoch_)return;
      auto record=[&](const char *label){city_checks_.push_back({{"stage",label},{"buildings",state.city.buildings},{"cells",state.city.cells},{"triangles",state.city.triangles},{"unique_triangles",state.city.unique_triangles},{"sessions",state.sessions},{"samples",state.samples}});std::ofstream(output_/"city-checks.json")<<city_checks_.dump(2);grab().save(QString::fromStdWString((output_/(std::string(label)+".png")).wstring()));};
      auto force=[&](int level){city_panel_->view_changed(city_test_id_,{level,true,false});};
      if(city_test_stage_==1){check(document_->cities.size()==1&&state.city.buildings>0,"Generated city missing from viewport");city_test_id_=document_->cities[0]->id;record("generated");force(1);}
      else if(city_test_stage_>=2&&city_test_stage_<=5){const int level=city_test_stage_-1;check(state.city.cells[level-1]==16,"Forced city LOD did not reach viewport");record(("LOD"+std::to_string(level)).c_str());if(level<4)force(level+1);else {save_scene_extension(output_/"editor-city.dufex",*document_,snapshot_);history_->undo();check(snapshot_.city_views.at(city_test_id_).forced==3,"LOD undo failed");history_->redo();check(snapshot_.city_views.at(city_test_id_).forced==4,"LOD redo failed");}}
      else if(city_test_stage_==6){record("lod-undo-redo");force(-1);renderer_->camera_view({0,0,0,150000,.4f,.7f});city_test_epoch_=renderer_->input_camera().epoch;}
      else if(city_test_stage_==7){if(state.city.cells[3]!=16)return;record("auto-far");renderer_->camera_view({0,0,15,100,.4f,.7f});city_test_epoch_=renderer_->input_camera().epoch;}
      else if(city_test_stage_==8){if(!state.city.cells[0])return;record("auto-near");auto edit=history_edit(QStringLiteral("测试城市共享材质"));const auto &s=document_->loaded.scene;const auto id=document_->cities[0]->material_sources[0];const auto it=std::find_if(s.instances.begin(),s.instances.end(),[&](const auto &i){return i.id==id;});snapshot_.material_overrides[id][s.meshes[it->mesh].material_slots[0]]["base_color"]=nlohmann::json::array({1.,0.,0.});send();}
      else if(city_test_stage_==9){record("shared-material");city_panel_->remove(city_test_id_);check(document_->cities.empty(),"City remove failed");}
      else if(city_test_stage_==10){record("removed");history_->undo();check(document_->cities.size()==1,"City remove undo failed");}
      else if(city_test_stage_==11){record("remove-undo");auto config=document_->cities[0]->config;++config.seed;generate_city(config,city_test_id_);}
      else if(city_test_stage_==12){check(document_->cities[0]->config.seed==1338,"City regeneration failed");record("regenerated");history_->undo();check(document_->cities[0]->config.seed==1337,"City regeneration undo failed");}
      else if(city_test_stage_==13){record("regenerate-undo");load(output_/"editor-city.dufex");}
      else if(city_test_stage_==14){check(document_->cities.size()==1&&snapshot_.city_views.at(city_test_id_).forced==4&&state.city.cells[3]==16,"Saved city did not restore");record("reopened");finish_test(true);city_test_=false;return;}
      ++city_test_stage_;
    }catch(const std::exception &e){finish_test(false,e.what());city_test_=false;}
  }
