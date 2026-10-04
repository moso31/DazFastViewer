  bool extension_test_=false;
  bool dufex_test_=false;
  nlohmann::json dufex_expected_;
  std::filesystem::path dufex_native_;
  runtime::TransformValues dufex_original_transform_;
  void dufex_tick(const RenderStatus &state){
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>180000){finish_test(false,"DUFEX 界面验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<4)return;
    try{
      if(test_stage_==0){
        if(document_->source_file.parent_path()!=output_||snapshot_.values.empty())throw std::runtime_error("验证需要输出目录内的测试场景");
        dufex_native_=document_->source_file;dufex_original_transform_=snapshot_.values[0].transform;snapshot_.values[0].transform.translation_cm.x+=12;
        send();auto c=renderer_->input_camera();renderer_->camera_view({c.target.x,c.target.y,c.target.z,c.distance*.9f,c.yaw+.2f,c.pitch+.1f});++test_stage_;return;
      }
      if(test_stage_==1){
        extension_file_=extension_path(dufex_native_);save_extension();dufex_expected_=snapshot_json(*document_,snapshot_);
        const QImage png(QString::fromStdWString(extension_file_.wstring())+".png");if(png.size()!=QSize(256,256))throw std::runtime_error("保存未生成 256 平方缩略图");
        auto min=255,max=0;for(int y=0;y<png.height();++y)for(int x=0;x<png.width();++x){const auto v=qGray(png.pixel(x,y));min=std::min(min,v);max=std::max(max,v);}if(max-min<10)throw std::runtime_error("缩略图为空白");
        ++test_stage_;open_asset(extension_file_);return;
      }
      if(test_stage_==2){
        if(snapshot_json(*document_,snapshot_)!=dufex_expected_)throw std::runtime_error("界面保存重开参数不一致");
        const auto c=renderer_->input_camera();const std::array<float,6> view={c.target.x,c.target.y,c.target.z,c.distance,c.yaw,c.pitch};if(!snapshot_.view||view!=*snapshot_.view)throw std::runtime_error("观察相机未恢复");
        ++test_stage_;load(dufex_native_);return;
      }
      if(snapshot_.values[0].transform!=dufex_original_transform_||snapshot_.view)throw std::runtime_error("打开原 DUF 错误套用了同名 DUFEX");finish_test(true);
    }catch(const std::exception &e){finish_test(false,e.what());}
  }
  bool empty_test_=false;
  uint64_t empty_epoch_=0;
  void empty_tick(const RenderStatus &state){
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>60000){finish_test(false,"空场景刷新超时");return;}
    if(!document_||!document_->catalog.targets.empty()||!document_->loaded.scene.instances.empty()){finish_test(false,"默认场景并非空场景");return;}
    if(state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.frames==0||resize_at_)return;
    if(test_stage_==0){screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"empty-initial.png").wstring()));move(pos()+QPoint(40,20));resize(width()-60,height()-40);empty_epoch_=state.requested_epoch;++test_stage_;return;}
    if(state.requested_epoch<=empty_epoch_||state.width!=viewport_size_.width()||state.height!=viewport_size_.height())return;
    screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"empty-resized.png").wstring()));finish_test(true);
  }
  int extension_figure_=-1,extension_other_=-1,extension_prop_=-1;
  runtime::Properties extension_before_;
  nlohmann::json extension_expected_,extension_checks_=nlohmann::json::array();
  double extension_mass_=0;
  double extension_scale_before_=0;
  std::string extension_favorite_morph_;
  bool extension_favorite_added_=false;
  uint64_t extension_geometry_=0;
  uint64_t extension_measured_serial_=0;
  qint64 extension_measured_at_=0;
  size_t extension_instances_=0,extension_meshes_=0;
  void extension_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000){finish_test(false,"生长界面验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.samples<4)return;
    auto check=[this](bool ok,const char *message){if(!ok)finish_test(false,message);return ok;};
    auto scale_control=[this]()->QDoubleSpinBox *{for(auto *spin:parameters_->findChildren<QDoubleSpinBox *>())if(spin->property("parameterId").toString()=="transform/general_scale")return spin;return nullptr;};
    auto favorite_button=[this](const std::string &id)->QToolButton *{auto *tree=parameters_->findChild<QTreeWidget *>("parameterRows");for(int i=0;i<tree->topLevelItemCount();++i)if(auto *row=tree->itemWidget(tree->topLevelItem(i),0))if(auto *spin=row->findChild<QDoubleSpinBox *>("valueSpin");spin&&spin->property("parameterId").toString().toStdString()==id)return row->findChild<QToolButton *>("favoriteButton");return nullptr;};
    auto measured=[this](){
      if(measurement_displayed_!=measurement_serial_||!measurement_serial_||!measurements_.result().error.empty())return false;
      const auto now=QDateTime::currentMSecsSinceEpoch();if(extension_measured_serial_!=measurement_serial_){extension_measured_serial_=measurement_serial_;extension_measured_at_=now;return false;}
      return now-extension_measured_at_>=100; // 等待新增结果行完成布局与屏幕呈现后再截图。
    };
    auto screen_save=[this](const char *name){QCoreApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest);repaint();screen()->grabWindow(winId()).save(QString::fromStdWString((output_/name).wstring()));};
    if(test_stage_==0){
      for(size_t t=0;t<document_->catalog.targets.size();++t)if(growth_character(*document_,t)){if(extension_figure_<0)extension_figure_=int(t);else if(extension_other_<0)extension_other_=int(t);}else if(extension_prop_<0)extension_prop_=int(t);
      if(!check(extension_figure_>=0&&extension_other_>=0&&extension_prop_>=0,"测试需要两个角色和一个道具"))return;
      choose(extension_figure_);extension_before_=snapshot_.values[extension_figure_];chrome->weight_action()->trigger();
      if(!check(snapshot_.values[extension_figure_].morphs==extension_before_.morphs&&snapshot_.values[extension_figure_].transform.general_scale==extension_before_.transform.general_scale,"注册生长立即改变了外形"))return;
      extension_panel_->findChild<QDoubleSpinBox *>("GrowthAgeStep")->setValue(.25);extension_panel_->findChild<QDoubleSpinBox *>("GrowthStrength")->setValue(.375);extension_panel_->findChild<QDoubleSpinBox *>("GrowthSensitivity")->setValue(8);++test_stage_;return;
    }
    if(test_stage_==1){if(!measured())return;const auto r=measurements_.result().weight;if(!check(r.kg>0&&r.height_cm>0&&r.parts.size()==13,"角色测量未返回身高和部位重量"))return;
      extension_checks_.push_back({{"check","registration-and-background-measurement"},{"height_cm",r.height_cm},{"kg",r.kg},{"missing",r.missing_morphs}});extension_before_=snapshot_.values[extension_figure_];if(!check(scale_control()!=nullptr,"缺少 Scale 控件"))return;extension_scale_before_=scale_control()->value();screen_save("growth-initial.png");extension_panel_->findChild<QDoubleSpinBox *>("GrowthAge")->stepUp();++test_stage_;return;
    }
    if(test_stage_==2){if(!check(snapshot_.values[extension_figure_].extension.age==extension_before_.extension.age+.25,"年龄步长未生效"))return;
      if(!check(scale_control()&&extension_scale_before_==90&&scale_control()->value()==92&&scale_control()->text()=="92","Scale 未精确显示 90 → 92"))return;
      extension_checks_.push_back({{"check","scale-decimal-increment"},{"before",extension_scale_before_},{"after",scale_control()->value()},{"text",scale_control()->text().toStdString()}});
      extension_panel_->findChild<QDoubleSpinBox *>("GrowthAge")->stepDown();++test_stage_;return;}
    if(test_stage_==3){if(!measured())return;const auto &v=snapshot_.values[extension_figure_];if(!check(v.morphs==extension_before_.morphs&&std::abs(v.transform.general_scale-extension_before_.transform.general_scale)<1e-6&&std::abs(v.transform.translation_cm.y-extension_before_.transform.translation_cm.y)<1e-4,"真实角色年龄往返未恢复"))return;
      auto *fold=extension_panel_->findChild<QToolButton *>("ExtensionCollapse");fold->click();if(!check(!extension_panel_->findChild<QDoubleSpinBox *>("GrowthAge")->isVisible(),"扩展组未折叠"))return;fold->click();auto *morph=findChild<QToolButton *>("MorphCollapse");morph->click();if(!check(!parameters_->isVisible(),"Morph 未折叠"))return;morph->click();
      const auto revision=snapshot_.revision;auto *star=favorite_button("transform/general_scale");if(!check(star!=nullptr,"缺少缩放收藏按钮"))return;
      if(star->text()!=QStringLiteral("★"))star->click();star->click();
      if(!check(!snapshot_.values[extension_figure_].favorites->nodes.at("").at("transform/general_scale"),"取消收藏未写入场景"))return;
      const auto &morphs=document_->catalog.targets[extension_figure_].morphs;const auto found=std::find_if(morphs.begin(),morphs.end(),[](const auto &m){return m.label=="Body Size"&&m.alias_morph<0;});if(!check(found!=morphs.end(),"缺少测试 Morph"))return;
      extension_favorite_morph_=found->source+"#"+found->id;parameters_->query(QStringLiteral("Body Size"));parameters_->select_parameter(size_t(found-morphs.begin()));star=favorite_button(extension_favorite_morph_);if(!check(star!=nullptr,"缺少 Morph 收藏按钮"))return;
      extension_favorite_added_=star->text()!=QStringLiteral("★");star->click();parameters_->query({});if(!check(snapshot_.revision==revision,"收藏修改触发几何重算"))return;
      extension_checks_.push_back({{"check","fractional-age-roundtrip-and-collapse"},{"passed",true}});choose(extension_other_);chrome->weight_action()->trigger();extension_panel_->findChild<QDoubleSpinBox *>("GrowthAge")->setValue(5);
      if(!check(snapshot_.values[extension_figure_].extension.age==extension_before_.extension.age&&snapshot_.values[extension_other_].extension.age==5,"不同角色生长参数串值"))return;
      extension_instances_=document_->loaded.scene.instances.size();extension_meshes_=document_->loaded.scene.meshes.size();
      extension_file_=output_/"review.dufex";save_extension();extension_expected_=snapshot_json(*document_,snapshot_);++test_stage_;load(extension_file_);return;
    }
    if(test_stage_==4){if(!check(snapshot_json(*document_,snapshot_)==extension_expected_,"真实场景保存重开参数不一致"))return;
      choose(extension_figure_);parameters_->query(QStringLiteral("Scale（%）"));auto *star=favorite_button("transform/general_scale");if(!check(star&&star->text()==QStringLiteral("☆"),"保存重开丢失取消收藏"))return;
      parameters_->query(QStringLiteral("Body Size"));star=favorite_button(extension_favorite_morph_);if(!check(star&&((star->text()==QStringLiteral("★"))==extension_favorite_added_),"保存重开丢失 Morph 收藏"))return;parameters_->query({});
      extension_checks_.push_back({{"check","favorite-add-remove-dufex-roundtrip"},{"passed",true}});
      if(!check(document_->loaded.scene.instances.size()==extension_instances_&&document_->loaded.scene.meshes.size()==extension_meshes_,"真实场景保存重开丢失几何"))return;
      extension_checks_.push_back({{"check","character-dufex-roundtrip-and-isolation"},{"passed",true},{"instances",extension_instances_},{"meshes",extension_meshes_}});
      choose(extension_prop_);chrome->weight_action()->trigger();const auto &native=document_->catalog.targets[extension_prop_].native_extension;const double density=native.kind==runtime::ExtensionKind::density?native.density:1000;
      if(!check(snapshot_.values[extension_prop_].extension.kind==runtime::ExtensionKind::density&&snapshot_.values[extension_prop_].extension.density==density,"道具密度未继承原生值或默认值"))return;extension_panel_->findChild<QDoubleSpinBox *>("ObjectDensity")->setValue(7890);++test_stage_;return;
    }
    if(test_stage_==5){if(!measured())return;extension_mass_=measurements_.result().weight.kg;extension_geometry_=state.adapter.geometry_updates;if(!check(extension_mass_>0,"道具体积测量为零"))return;screen_save("object-weight.png");extension_panel_->findChild<QDoubleSpinBox *>("ObjectDensity")->setValue(3945);++test_stage_;return;}
    if(test_stage_==6){if(!measured())return;if(!check(std::abs(measurements_.result().weight.kg-extension_mass_*.5)<1e-8&&state.adapter.geometry_updates==extension_geometry_,"密度更改触发几何更新或重量未线性变化"))return;extension_checks_.push_back({{"check","density-multiplier-without-geometry-update"},{"kg",measurements_.result().weight.kg}});save_extension();extension_expected_=snapshot_json(*document_,snapshot_);++test_stage_;load(extension_file_);return;}
    if(test_stage_==7){if(!check(snapshot_json(*document_,snapshot_)==extension_expected_&&document_->loaded.scene.instances.size()==extension_instances_&&document_->loaded.scene.meshes.size()==extension_meshes_,"道具密度与场景参数或几何未恢复"))return;choose(extension_figure_);++test_stage_;return;}
    if(test_stage_==8){if(!measured())return;screen_save("growth-restored.png");extension_checks_.push_back({{"check","final-dufex-roundtrip"},{"passed",true}});extension_panel_->findChild<QToolButton *>("WeightDetails")->click();++test_stage_;return;}
    if(test_stage_==9){screen_save("growth-parts.png");extension_geometry_=state.adapter.geometry_updates;
      QTimer::singleShot(100,this,[this]{auto *dialog=QApplication::activeModalWidget();if(!dialog)return;auto *input=dialog->findChild<QLineEdit *>("MeasurementScale");if(!input)return;input->setText("2");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"measurement-units.png").wstring()));dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
      findChild<QAction *>("MeasurementUnits")->trigger();++test_stage_;return;
    }
    if(test_stage_==10){const auto &r=measurements_.result().weight;const auto label=extension_panel_->findChild<QLabel *>("WeightSummary")->text();
      if(!check(label.contains(text(runtime::measurement_height(r.height_cm,"2")))&&label.contains(text(runtime::measurement_mass(r.kg,"2")))&&state.adapter.geometry_updates==extension_geometry_,"全局倍率显示或几何缓存错误"))return;
      screen_save("growth-units-2x.png");extension_checks_.push_back({{"check","measurement-units-dialog-linear-cubic-no-geometry"},{"passed",true}});finish_test(true);
    }
  }
