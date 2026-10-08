  bool node_properties_test_=false;
  int np_phase_=0,np_hover_=-1;
  std::string np_group_;
  QPoint np_pointer_;
  Snapshot np_initial_;
  std::vector<uint32_t> np_members_;
  void node_properties_tick(const RenderStatus &state){
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>240000){finish_test(false,"节点属性验证超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()){finish_test(false,state.error+state.edit_error+state.resource_error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.preview||state.samples<4)return;
    auto fail=[&](const std::string &message){finish_test(false,message);};
    auto group_item=[&]()->QTreeWidgetItem *{for(QTreeWidgetItemIterator it(hierarchy_);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()==-4&&(*it)->data(0,Qt::UserRole+3).toString().toStdString()==np_group_)return *it;return nullptr;};
    const auto hwnd=FindWindowExW(HWND(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
    std::ofstream(output_/"node-properties-progress.json")<<nlohmann::json{{"phase",np_phase_},{"revision",snapshot_.revision},{"hovered",state.hovered}};
    if(np_phase_==0){
      for(const auto &node:document_->loaded.nodes)if(node.group){auto members=group_instances(*document_,node.id);if(members.size()>1){np_group_=node.id;np_members_=std::move(members);break;}}
      if(np_group_.empty()){fail("测试场景缺少包含多个子对象的组");return;}choose_item(hierarchy_,group_item());sync_selection();np_initial_=snapshot_;
      findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"))->raise();parameters_->query(QStringLiteral("Visible"));
      if(!delete_->isEnabled()){fail("组不能通过 Del 删除");return;}group_item()->setCheckState(0,Qt::Unchecked);np_phase_=1;return;
    }
    if(np_phase_==1){
      for(auto i:np_members_)if(state.visible.at(i)){fail("关闭组 Visible 后仍有子对象可见");return;}
      if(snapshot_.values!=np_initial_.values){fail("父组显隐覆盖了子对象独立参数");return;}
      if(!parameters_->edit_control("Visible",1)){fail("组缺少 Visible 参数");return;}np_phase_=2;return;
    }
    if(np_phase_==2){
      runtime::PickingScene probe;probe.update(document_->loaded.scene,scene_pick_mask(*document_,snapshot_,document_->loaded.scene));bool found=false;
      for(int y=state.height/4;y<state.height*3/4&&!found;y+=std::max(1,state.height/25))for(int x=state.width/4;x<state.width*3/4&&!found;x+=std::max(1,state.width/25)){
        const auto hit=probe.screen(state.camera,x,y,state.width,state.height);if(hit.instance>=0&&std::find(np_members_.begin(),np_members_.end(),uint32_t(hit.instance))!=np_members_.end()){np_hover_=hit.instance;np_pointer_={x,y};found=true;}}
      if(!found){fail("无法找到组子对象的视口命中点");return;}SendMessageW(hwnd,WM_MOUSEMOVE,0,MAKELPARAM(np_pointer_.x(),np_pointer_.y()));np_phase_=3;return;
    }
    if(np_phase_==3){
      if(state.pointer_x!=np_pointer_.x()||state.pointer_y!=np_pointer_.y()||state.hovered!=np_hover_)return;
      if(!parameters_->edit_control("Selectable",0)){fail("组缺少 Selectable 参数");return;}np_phase_=4;return;
    }
    if(np_phase_==4){
      if(state.hovered>=0&&std::find(np_members_.begin(),np_members_.end(),uint32_t(state.hovered))!=np_members_.end()){fail("不可选中组的子对象仍黄色高亮");return;}
      workflow_click_=state.clicks;SendMessageW(hwnd,WM_LBUTTONDOWN,0,MAKELPARAM(np_pointer_.x(),np_pointer_.y()));SendMessageW(hwnd,WM_LBUTTONUP,0,MAKELPARAM(np_pointer_.x(),np_pointer_.y()));np_phase_=5;return;
    }
    if(np_phase_==5){
      if(state.clicks<=workflow_click_)return;
      if(state.hit_target>=0&&std::find(np_members_.begin(),np_members_.end(),document_->catalog.targets.at(size_t(state.hit_target)).instance)!=np_members_.end()){fail("不可选中对象仍能通过视口点击选中");return;}
      choose_item(hierarchy_,group_item());sync_selection();const auto file=output_/"node-properties.dufex";save_scene_extension(file,*document_,snapshot_);const auto reopened=load_scene_extension(file,roots_,document_->generation);
      if(reopened.snapshot.node_properties!=snapshot_.node_properties){fail("保存重开丢失组属性");return;}
      if(!parameters_->edit_control("Selectable",1)){fail("从场景树重新开启 Selectable 失败");return;}np_phase_=6;return;
    }
    if(np_phase_==6){
      if(state.hovered!=np_hover_)return;std::vector<QTreeWidgetItem *> objects;for(QTreeWidgetItemIterator it(hierarchy_);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()>=0&&(*it)->data(0,Qt::UserRole+1).toInt()<0)objects.push_back(*it);
      if(objects.size()<2){fail("测试场景缺少两个模型");return;}choose_item(hierarchy_,objects[0]);choose_item(hierarchy_,objects[1],true);sync_selection();
      auto expected=*document_;auto expected_snapshot=snapshot_;const auto deletion=tree_deletion_selection(hierarchy_);const auto removed=remove_selection(expected,expected_snapshot,deletion.targets,deletion.nodes,deletion.lights);delete_->trigger();if(removed<2||document_->loaded.scene.instances.size()!=expected.loaded.scene.instances.size()){fail("Del 没有删除多选中的全部对象");return;}
      history_move(false);if(document_->loaded.scene.instances.size()!=state.visible.size()){fail("多选删除撤销没有恢复全部对象");return;}
      choose_item(hierarchy_,group_item());if(group_item()->childCount())choose_item(hierarchy_,group_item()->child(0),true);sync_selection();delete_->trigger();np_phase_=7;return;
    }
    if(np_phase_==7){
      if(group_node(*document_,np_group_)){fail("父组和子对象混选后 Del 未删除组");return;}
      const auto file=output_/"deleted-group.dufex";save_scene_extension(file,*document_,snapshot_);const auto reopened=load_scene_extension(file,roots_,document_->generation);
      if(group_node(*reopened.document,np_group_)){fail("删除组保存重开后复活");return;}history_move(false);np_phase_=8;return;
    }
    if(np_phase_==8){
      if(document_->loaded.scene.instances.size()<np_members_.size()||!group_node(*document_,np_group_)){fail("整组删除撤销不完整");return;}
      choose_item(hierarchy_,group_item());sync_selection();parameters_->query(QStringLiteral("Selectable"));parameters_->edit_control("Selectable",1);np_phase_=9;return;
    }
    if(np_phase_==9){
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"node-properties.png").wstring()));
      std::ofstream(output_/"node-properties-check.json")<<nlohmann::json{{"pass",true},{"group",np_group_},{"members",np_members_.size()},{"visible",true},{"selectable_hover_click",true},{"save_reopen",true},{"multi_delete_undo",true},{"nested_delete_reopen",true}}.dump(2);finish_test(true);
    }
  }
