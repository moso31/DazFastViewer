  std::unique_ptr<EditHistory> history_;
  std::unique_ptr<RecoverySession> recovery_;
  HistoryInput *history_input_=nullptr;
  QAction *undo_action_=nullptr,*redo_action_=nullptr;
  std::shared_ptr<const Document> history_document_;
  const Document *history_runtime_document_=nullptr;
  std::shared_ptr<const Document> pins_document_;
  uint64_t edit_serial_=0;
  uint64_t scene_identity_=1,scene_serial_=1;
  bool restoring_history_=false,closing_=false;
  QString recovered_file_,recovery_error_;
  EditHistory::Transaction history_edit(const QString &name){return EditHistory::Transaction(history_.get(),name);}
  uint64_t next_revision(){return edit_serial_=std::max(edit_serial_,snapshot_.revision)+1;}
  void synchronize_pins() {
    if(!document_)return;
    if(pins_document_&&pins_generation_!=document_->generation){std::vector<runtime::PosePin> pins;
      for(auto pin:pose_pins_){if(pin.skin<0||size_t(pin.skin)>=pins_document_->skeletons.skins.size())continue;const auto &old=pins_document_->skeletons.skins[pin.skin];if(pin.joint<0||size_t(pin.joint)>=old.joints.size())continue;
        for(size_t s=0;s<document_->skeletons.skins.size();++s){const auto &skin=document_->skeletons.skins[s];if(skin.id!=old.id)continue;for(size_t j=0;j<skin.joints.size();++j)if(skin.joints[j].id==old.joints[pin.joint].id){pin.skin=int(s);pin.joint=int(j);pins.push_back(pin);break;}}}
      pose_pins_=std::move(pins);if(renderer_)renderer_->pose_pins(pose_pins_);
    }
    pins_document_=document_;pins_generation_=document_->generation;snapshot_.pose_pins=pose_pins_;
  }
  EditSelection history_selection(int target,int joint,int light,const std::string &group={}) const {
    EditSelection result;if(!document_)return result;
    if(light>=0&&size_t(light)<snapshot_.lights.size()){result.light=snapshot_.lights[light].id;return result;}
    result.special=target;if(target==-4){result.object=group;return result;}
    if(target>=0&&size_t(target)<document_->catalog.targets.size()){
      const auto &t=document_->catalog.targets[target];result.object=t.id;result.special=-1;
      if(joint>=0)for(const auto &skin:document_->skeletons.skins)if(skin.instance==t.instance&&size_t(joint)<skin.joints.size())result.joint=skin.joints[joint].id;
    }else if(target<=-5&&size_t(-5-target)<document_->loaded.scene.instances.size()){result.object=document_->loaded.scene.instances[size_t(-5-target)].id;result.special=-5;}
    return result;
  }
  EditState capture_edit() {
    synchronize_pins();initialize_favorites();EditState state;
    if(document_.get()!=history_runtime_document_){history_document_.reset();history_runtime_document_=nullptr;}
    state.scene_id=scene_identity_;
    state.document=document_.get()==history_runtime_document_?history_document_:document_;
    state.snapshot=snapshot_;if(state.document)state.snapshot.generation=state.document->generation;
    state.snapshot.pose_pins=pose_pins_;state.pending=pending_parameters_;state.save_file=extension_file_;state.manual=manual_morph_&&manual_morph_->isChecked();
    if(hierarchy_)for(auto *item:hierarchy_->selectedItems())state.context.selection.push_back(history_selection(item->data(0,Qt::UserRole).toInt(),item->data(0,Qt::UserRole+1).toInt(),item->data(0,Qt::UserRole+2).toInt(),item->data(0,Qt::UserRole+3).toString().toStdString()));
    state.context.active=history_selection(selected_,selected_joint_,selected_light_,selected_group_);
    if(materials_)state.context.surfaces=materials_->selection_ids();return state;
  }
  void restore_context(const EditContext &context) {
    {QSignalBlocker block(hierarchy_);hierarchy_->clearSelection();QTreeWidgetItem *first=nullptr,*active=nullptr;
      for(QTreeWidgetItemIterator it(hierarchy_);*it;++it){auto *item=*it;const auto key=history_selection(item->data(0,Qt::UserRole).toInt(),item->data(0,Qt::UserRole+1).toInt(),item->data(0,Qt::UserRole+2).toInt(),item->data(0,Qt::UserRole+3).toString().toStdString());
        if(std::find(context.selection.begin(),context.selection.end(),key)!=context.selection.end()){item->setSelected(true);if(!first)first=item;for(auto *p=item->parent();p;p=p->parent())p->setExpanded(true);if(key==context.active)active=item;}}
      hierarchy_->setCurrentItem(active?active:first,0,QItemSelectionModel::NoUpdate);
    }sync_selection();if(materials_)materials_->restore_selection(context.surfaces);
  }
  void restore_edit(const EditState &state) {
    if(!state.document)return;
    const auto current=document_.get()==history_runtime_document_?history_document_:document_;
    const bool structure=current!=state.document;
    auto next=structure?std::make_shared<Document>(*state.document):document_;
    if(structure)next->generation=++generation_;
    auto snapshot=state.snapshot;snapshot.generation=next->generation;snapshot.revision=next_revision();
    restoring_history_=true;
    if(powerpose_)powerpose_->cancel();if(renderer_)renderer_->cancel_edit();parameters_->bind(nullptr,nullptr);
    document_=std::move(next);snapshot_=std::move(snapshot);history_document_=state.document;history_runtime_document_=document_.get();
    scene_identity_=state.scene_id;scene_serial_=std::max(scene_serial_,scene_identity_);
    pending_parameters_=state.pending;extension_file_=state.save_file;{QSignalBlocker block(manual_morph_);manual_morph_->setChecked(state.manual);}apply_parameters_->setEnabled(!pending_parameters_.empty());
    pose_pins_=snapshot_.pose_pins;pins_generation_=document_->generation;renderer_->pose_pins(pose_pins_);
    ground_pending_.clear();focus_pending_=frame_pending_=false;effective_generation_=0;measurement_document_=nullptr;load_error_.clear();pose_report_=nullptr;pose_status_->clear();
    rebuild_hierarchy();renderer_->set_document(document_,submitted_snapshot(),false);restore_context(state.context);restoring_history_=false;
  }
  void checkpoint() {
    if(recovery_&&document_)recovery_->checkpoint(capture_edit(),roots_);
  }
  bool accept_pose_commit(const RenderStatus &state) {
    if(state.pose_commit==pose_commit_)return false;pose_commit_=state.pose_commit;
    if(!document_||state.pose_generation!=document_->generation||state.pose_revision!=snapshot_.revision||(!state.pose_gizmo&&(state.pose_skin<0||size_t(state.pose_skin)>=snapshot_.poses.size())))return false;
    auto edit=history_edit(state.pose_gizmo?QStringLiteral("Gizmo 变换"):state.pose_powerpose?QStringLiteral("PowerPose 姿势"):QStringLiteral("IK 姿势"));
    if(state.pose_gizmo&&!state.pose_group.empty()){if(!group_node(*document_,state.pose_group))return false;runtime::validate_transform(state.pose_transform);if(state.pose_transform==runtime::TransformValues{})snapshot_.group_transforms.erase(state.pose_group);else snapshot_.group_transforms[state.pose_group]=state.pose_transform;}
    else if(state.pose_gizmo&&state.pose_light>=0&&size_t(state.pose_light)<snapshot_.lights.size())snapshot_.lights[state.pose_light].transform=state.gizmo_light_transform;
    else if(state.pose_figure&&(state.pose_powerpose||state.pose_gizmo)&&state.pose_target>=0&&size_t(state.pose_target)<snapshot_.values.size())snapshot_.values[state.pose_target].transform=state.pose_transform;
    else snapshot_.poses.at(size_t(state.pose_skin))=state.pose_input;
    send();select(selected_,selected_joint_,selected_light_);
    pose_status_->setText(state.pose_gizmo?QStringLiteral("节点变换已应用。"):state.pose_error>.005||state.pose_angle_error>.5?QStringLiteral("固定约束受关节限位或可达范围限制（位置 %1 厘米，角度 %2 度）。").arg(state.pose_error*100,0,'f',2).arg(state.pose_angle_error,0,'f',2):state.pose_powerpose?QStringLiteral("PowerPose 姿势已应用。"):QStringLiteral("IK 姿势已应用。"));return true;
  }
  void history_move(bool redo) {
    if(!history_||loading_||closing_)return;
    try {
      const auto state=renderer_->status();const bool committed=accept_pose_commit(state);
      if(!committed&&(state.pose_dragging||(powerpose_&&powerpose_->dragging()))){renderer_->cancel_edit();if(powerpose_)powerpose_->cancel();send();return;}
      if(renderer_)renderer_->cancel_edit();ground_pending_.clear();if(redo)history_->redo();else history_->undo();
    }
    catch(const std::exception &e){statusBar()->showMessage(QStringLiteral("无法恢复操作：")+text(e.what()),10000);}
  }
  void initialize_history(QMenu *menu) {
    if(!document_){document_=std::make_shared<Document>();document_->generation=++generation_;snapshot_=initial_snapshot(*document_);}
    if(!windowTitle().contains("[*]"))setWindowTitle(windowTitle()+"[*]");
    history_=std::make_unique<EditHistory>([this]{return capture_edit();},[this](const EditState &s){restore_edit(s);},project_.history_limit);
    history_input_=new HistoryInput(*history_,this);
    undo_action_=chrome->undo_action();menu->addAction(undo_action_);undo_action_->setShortcut(QKeySequence::Undo);
    redo_action_=chrome->redo_action();menu->addAction(redo_action_);redo_action_->setShortcuts({QKeySequence(Qt::CTRL|Qt::Key_Y),QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_Z)});
    connect(undo_action_,&QAction::triggered,this,[this]{history_move(false);});connect(redo_action_,&QAction::triggered,this,[this]{history_move(true);});menu->addSeparator();
    history_->changed=[this]{checkpoint();update_history_actions();};history_->failed=[this](const QString &error){statusBar()->showMessage(error,10000);};
    update_history_actions();
  }
  void update_history_actions() {
    if(!history_)return;const auto &stack=history_->stack();
    undo_action_->setEnabled(!loading_&&(stack.canUndo()||history_->gesturing()));redo_action_->setEnabled(!loading_&&stack.canRedo());
    undo_action_->setText(stack.canUndo()?QStringLiteral("撤销 %1").arg(stack.undoText()):QStringLiteral("撤销"));redo_action_->setText(stack.canRedo()?QStringLiteral("重做 %1").arg(stack.redoText()):QStringLiteral("重做"));
    undo_action_->setToolTip(undo_action_->text()+QStringLiteral("（Ctrl+Z）"));redo_action_->setToolTip(redo_action_->text()+QStringLiteral("（Ctrl+Y / Ctrl+Shift+Z）"));
    setWindowModified(!stack.isClean());
  }
  void history_interaction(bool active,quintptr key) {
    if(history_&&!restoring_history_){if(active)history_->begin_gesture(key);else history_->finish_gesture();}
    if(renderer_)renderer_->interaction(active);
  }
public:
  bool start_recovery() {
    if(self_test_)return false;
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/recovery/"+QString::fromLatin1(QCryptographicHash::hash(project_.file.toUtf8(),QCryptographicHash::Sha256).toHex().left(16));
    try {
      const auto candidates=RecoverySession::candidates(directory);recovery_=std::make_unique<RecoverySession>(directory);
      if(candidates.empty())return false;
      const auto candidate=candidates.front();
      if(QMessageBox::question(this,QStringLiteral("恢复场景"),QStringLiteral("上次程序未正常退出。是否恢复最后一次操作记录？"),QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)!=QMessageBox::Yes){RecoverySession::dismiss(candidate);return false;}
      loading_=true;update_history_actions();recovered_file_=candidate;const auto generation=++generation_;
      loader_=std::jthread([this,candidate,generation](std::stop_token stop){try{
        auto state=restore_recovery(RecoverySession::read(candidate),generation,[this,stop](const std::string &message){if(stop.stop_requested())throw std::runtime_error("恢复已取消");progress(text(message));});
        QMetaObject::invokeMethod(this,[this,state=std::move(state),candidate]{if(closing_)return;loading_=false;restore_edit(state);history_->clear();history_->stack().resetClean();checkpoint();
          try{if(recovery_->flush())RecoverySession::dismiss(candidate);}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),10000);}statusBar()->showMessage(QStringLiteral("已恢复上次场景及暂存参数。"),10000);update_history_actions();},Qt::QueuedConnection);
      }catch(const std::exception &e){const auto error=text(e.what());QMetaObject::invokeMethod(this,[this,error]{if(closing_)return;loading_=false;clear_scene();load_error_=QStringLiteral("恢复失败，原恢复文件已保留：")+error;statusBar()->showMessage(load_error_);},Qt::QueuedConnection);}});return true;
    }catch(const std::exception &e){recovery_error_=text(e.what());statusBar()->showMessage(QStringLiteral("自动恢复不可用：")+recovery_error_);return false;}
  }
private:
