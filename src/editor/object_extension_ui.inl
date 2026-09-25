  ExtensionPanel *extension_panel_=nullptr;
  MeasurementService measurements_;
  uint64_t measurement_serial_=0,measurement_generation_=0,measurement_revision_=0,measurement_displayed_=0;
  int measurement_target_=-1,extension_bound_=-1;
  const Document *measurement_document_=nullptr;
  std::filesystem::path extension_file_;
  std::string measurement_scale_="1";
  void initialize_favorites() {
    if(!document_)return;
    for(size_t t=0;t<snapshot_.values.size();++t)parameters_->import_favorites(&document_->catalog.targets.at(t),snapshot_.values[t].favorites);
    parameters_->import_favorites(nullptr,snapshot_.control_favorites);
  }
  void measurement_units(){
    if(!edit_measurement_scale(this,measurement_scale_))return;
    extension_panel_->measurement_scale(measurement_scale_);
    if(!self_test_)QSettings().setValue("measurement/scale",text(measurement_scale_));
  }
  int extension_target() const {
    if(loading_||!document_||selected_<0||selected_light_>=0||hierarchy_->selectedItems().size()!=1)return -1;
    const auto &t=document_->catalog.targets.at(size_t(selected_));const auto &mesh=document_->loaded.scene.meshes.at(document_->loaded.scene.instances.at(t.instance).mesh);
    return mesh.triangles.empty()&&mesh.polygons.empty()?-1:selected_;
  }
  void bind_extension() {
    const int t=extension_target();if(chrome)chrome->weight_action()->setEnabled(t>=0);
    if(!extension_panel_)return;extension_panel_->bind(t>=0?snapshot_.values.at(t).extension:runtime::ObjectExtension{},extension_bound_!=t);extension_bound_=t;
  }
  void enable_extension() {
    const int t=extension_target();if(t<0)return;auto &v=snapshot_.values.at(t);
    if(v.extension.kind==runtime::ExtensionKind::none){v.extension={};v.extension.kind=growth_character(*document_,size_t(t))?runtime::ExtensionKind::growth:runtime::ExtensionKind::density;send();}
    bind_extension();extension_panel_->expand();auto *d=findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"));if(d){d->show();d->raise();}
  }
  void change_extension(runtime::ObjectExtension next,bool shape,double step) {
    const int t=extension_target();if(t<0)return;
    try {
      runtime::validate_extension(next);std::vector<std::string> missing;
      if(next.kind==runtime::ExtensionKind::growth&&(shape||step!=0)){
        missing=edit_growth(*document_,snapshot_,size_t(t),next,shape,step);
        const auto &target=document_->catalog.targets[size_t(t)];
        std::erase_if(pending_parameters_,[&](const auto &p){if(p.first.first!=target.id)return false;for(const auto &v:runtime::growth_values(next.age,next.strength))for(const auto &m:target.morphs)if(m.label==v.label&&m.id==p.first.second)return true;return false;});
        apply_parameters_->setEnabled(!pending_parameters_.empty());
      }else snapshot_.values[t].extension=next;
      send();bind_extension();
      // 只刷新已挂载行，年龄步进不重建数千项 Morph 目录。
      if(selected_joint_<0){QSignalBlocker block(transform_[1]);transform_[1]->setValue(snapshot_.values[t].transform.translation_cm.y);}
      parameters_->refresh(0);
      if(!missing.empty()){QStringList names;for(const auto &m:missing)names<<text(m);statusBar()->showMessage(QStringLiteral("部分生长参数不可用：")+names.join(QStringLiteral("、")),10000);}
    }catch(const std::exception &e){bind_extension();statusBar()->showMessage(text(e.what()),8000);}
  }
  void update_measurement() {
    const int t=extension_target();if(t<0||snapshot_.values[t].extension.kind==runtime::ExtensionKind::none){if(measurement_target_>=0)measurements_.request(++measurement_serial_,{}, {},0);measurement_target_=-1;return;}
    if(measurement_document_!=document_.get()||measurement_generation_!=document_->generation||measurement_revision_!=snapshot_.revision||measurement_target_!=t){
      extension_panel_->pending();const auto state=renderer_->status();if(state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||size_t(t)>=state.target_world.size())return;
      const bool object=snapshot_.values[t].extension.kind==runtime::ExtensionKind::density;if(object&&(state.weight_target!=t||!state.weight_positions))return;
      const bool different=measurement_document_!=document_.get()||measurement_target_!=t;
      measurement_document_=document_.get();measurement_generation_=document_->generation;measurement_revision_=snapshot_.revision;measurement_target_=t;
      extension_panel_->bind(snapshot_.values[t].extension,different);extension_panel_->pending();measurements_.request(++measurement_serial_,document_,submitted_snapshot(),size_t(t),state.target_world[t],object?state.weight_positions:nullptr);
    }
    const auto result=measurements_.result();if(result.serial==measurement_serial_&&result.serial!=measurement_displayed_){measurement_displayed_=result.serial;extension_panel_->result(result.weight,result.error);}
  }
  void save_extension(bool save_as=false) {
    if(loading_||!document_)return;
    initialize_favorites();
    auto path=extension_file_;if(path.empty()&&!document_->source_file.empty())path=extension_path(document_->source_file);
    if(save_as||path.empty()){const auto file=QFileDialog::getSaveFileName(this,QStringLiteral("保存场景修改"),QString::fromStdWString(path.wstring()),QStringLiteral("场景扩展 (*.dufex)"));if(file.isEmpty())return;path=file_path(file);if(path.extension().empty())path+=L".dufex";}
    try{auto saved=snapshot_;saved.pose_pins=pose_pins_;save_scene_extension(path,*document_,saved);extension_file_=path;statusBar()->showMessage(QStringLiteral("场景修改已保存：")+QString::fromStdWString(path.wstring()),8000);}
    catch(const std::exception &e){QMessageBox::warning(this,QStringLiteral("保存失败"),text(e.what()));}
  }
