  std::string cg_label_;
  int cg_phase_=0,cg_case_=0,cg_history_=0;
  uint64_t cg_requests_=0;
  Snapshot cg_initial_,cg_before_;
  RenderStatus cg_state_,cg_before_state_;
  GroundSelection cg_selection_;
  bool cg_changed_=false;
  std::vector<std::vector<std::pair<int,std::string>>> cg_cases_;
  nlohmann::json cg_checks_=nlohmann::json::array();
  void collective_ground_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>240000){finish_test(false,"整体地面对齐验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.pose_restoring||state.preview||state.samples<4||state.selections!=tree_selection(hierarchy_))return;
    auto shortcut=[&]{cg_requests_=state.ground_requests;const auto hwnd=FindWindowExW(HWND(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);SendMessageW(hwnd,WM_KEYDOWN,VK_CONTROL,0);SendMessageW(hwnd,WM_KEYDOWN,'D',0);SendMessageW(hwnd,WM_KEYUP,'D',0);SendMessageW(hwnd,WM_KEYUP,VK_CONTROL,0);};
    if(cg_phase_==0){
      QTreeWidgetItem *group=nullptr,*outside=nullptr;std::vector<QTreeWidgetItem *> children;std::vector<uint32_t> members;
      for(QTreeWidgetItemIterator it(hierarchy_);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()==-4){const auto id=(*it)->data(0,Qt::UserRole+3).toString().toStdString();const auto *node=group_node(*document_,id);if(node&&(node->label==cg_label_||id==cg_label_)){group=*it;members=group_instances(*document_,id);break;}}
      if(!group){finish_test(false,"整体对齐测试组不存在");return;}
      for(QTreeWidgetItemIterator it(hierarchy_);*it;++it){const int t=(*it)->data(0,Qt::UserRole).toInt();if(t<0||(*it)->data(0,Qt::UserRole+1).toInt()>=0)continue;const auto i=document_->catalog.targets[t].instance;if(std::find(members.begin(),members.end(),i)!=members.end())children.push_back(*it);else if(!outside)outside=*it;}
      if(children.size()<2){finish_test(false,"整体对齐测试组需要至少两个模型");return;}
      auto add=[&](std::vector<QTreeWidgetItem *> items){std::vector<std::pair<int,std::string>> keys;for(auto *item:items)keys.push_back({item->data(0,Qt::UserRole).toInt(),item->data(0,Qt::UserRole+3).toString().toStdString()});cg_cases_.push_back(std::move(keys));};
      add({group});add({children[0],children[1]});add({group,children[0]});if(outside)add({group,outside});
      cg_initial_=snapshot_;cg_state_=state;
      for(auto &v:snapshot_.values){v.ground_alignment_ratio=.5;v.ground_alignment_offset_cm=80;v.ground_alignment_body_only=true;}send();cg_phase_=1;return;
    }
    if(cg_phase_==1){
      if(cg_case_==int(cg_cases_.size())){snapshot_=cg_initial_;send();cg_phase_=7;return;}
      {QSignalBlocker block(hierarchy_);hierarchy_->clearSelection();QTreeWidgetItem *active=nullptr;
        for(const auto &[key,id]:cg_cases_[cg_case_])for(QTreeWidgetItemIterator it(hierarchy_);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()==key&&(*it)->data(0,Qt::UserRole+1).toInt()<0&&(*it)->data(0,Qt::UserRole+3).toString().toStdString()==id){(*it)->setSelected(true);if(!active)active=*it;break;}
        hierarchy_->setCurrentItem(active,0,QItemSelectionModel::NoUpdate);}
      sync_selection();cg_phase_=2;return;
    }
    if(cg_phase_==2){
      if(!chrome->ground_action()->isEnabled()||ground_panel_->findChild<QDoubleSpinBox *>("GroundAlignmentRatio")->isEnabled()){finish_test(false,"整体对齐入口或设置状态错误: case="+std::to_string(cg_case_)+" target="+std::to_string(ground_target())+" items="+std::to_string(hierarchy_->selectedItems().size())+" group="+selected_group_+" action="+std::to_string(chrome->ground_action()->isEnabled())+" ratio="+std::to_string(ground_panel_->findChild<QDoubleSpinBox *>("GroundAlignmentRatio")->isEnabled()));return;}
      cg_before_=snapshot_;cg_before_state_=state;cg_selection_=ground_objects();cg_changed_=ground_vertical_shift(ground_selection_bounds(cg_selection_,state.instance_bounds),0)!=0;cg_history_=history_->stack().index();shortcut();cg_phase_=3;return;
    }
    if(cg_phase_==3){
      if(state.ground_requests<=cg_requests_||!ground_pending_.empty())return;
      const auto before=ground_selection_bounds(cg_selection_,cg_before_state_.instance_bounds),after=ground_selection_bounds(cg_selection_,state.instance_bounds);const auto shift=ground_vertical_shift(before,0);
      if(std::abs(after.minimum.z)>1e-4||state.camera.epoch!=cg_before_state_.camera.epoch||state.sessions!=cg_state_.sessions){finish_test(false,"整体落地高度、相机或会话错误");return;}
      for(size_t i=0;i<state.instance_transforms.size();++i){auto expected=cg_before_state_.instance_transforms[i];if(std::find(cg_selection_.instances.begin(),cg_selection_.instances.end(),i)!=cg_selection_.instances.end())expected[11]+=float(shift);for(size_t k=0;k<12;++k)if(std::abs(expected[k]-state.instance_transforms[i][k])>1e-4){finish_test(false,"整体落地改变相对位置或重复移动后代");return;}}
      for(size_t t=0;t<snapshot_.values.size();++t){const auto &v=snapshot_.values[t],&b=cg_before_.values[t];if(v.ground_alignment_ratio!=b.ground_alignment_ratio||v.ground_alignment_offset_cm!=b.ground_alignment_offset_cm||v.ground_alignment_body_only!=b.ground_alignment_body_only){finish_test(false,"整体对齐改写角色设置");return;}}
      if(history_->stack().index()!=cg_history_+(shift!=0)){finish_test(false,"整体对齐撤销条数错误");return;}
      cg_checks_.push_back({{"case",cg_case_},{"members",cg_selection_.instances.size()},{"bottom",after.minimum.z},{"native_shortcut",true}});shortcut();cg_phase_=4;return;
    }
    if(cg_phase_==4){if(state.ground_requests<=cg_requests_||!ground_pending_.empty())return;if(history_->stack().index()!=cg_history_+int(cg_changed_)){finish_test(false,"重复整体对齐累积位移");return;}if(!cg_changed_){++cg_case_;cg_phase_=1;return;}history_move(false);cg_phase_=5;return;}
    if(cg_phase_==5){if(state.instance_transforms!=cg_before_state_.instance_transforms){finish_test(false,"整体对齐撤销未恢复");return;}history_move(true);cg_phase_=6;return;}
    if(cg_phase_==6){if(std::abs(ground_selection_bounds(cg_selection_,state.instance_bounds).minimum.z)>1e-4){finish_test(false,"整体对齐重做失败");return;}history_move(false);++cg_case_;cg_phase_=1;return;}
    if(cg_phase_==7){if(state.instance_transforms!=cg_state_.instance_transforms){finish_test(false,"整体对齐测试未恢复场景");return;}std::ofstream(output_/"collective-ground.json")<<nlohmann::json{{"pass",true},{"cases",cg_checks_},{"restored",true},{"undo_redo",true}}.dump(2);finish_test(true);}
  }
