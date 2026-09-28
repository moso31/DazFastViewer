  PhysicsPanel *physics_panel_=nullptr;
  uint64_t physics_command_sequence_=0;
  void bind_physics() {
    if(!physics_panel_)return;
    const bool eligible=!loading_&&document_&&selected_>=0&&selected_joint_<0&&selected_light_<0&&hierarchy_->selectedItems().size()==1&&physics_eligible(*document_,size_t(selected_));
    physics_panel_->setVisible(eligible);if(!eligible)return;
    const auto &target=document_->catalog.targets[size_t(selected_)];const auto &mesh=document_->loaded.scene.meshes[document_->loaded.scene.instances[target.instance].mesh];
    auto slots=mesh.material_slots;if(slots.empty())slots.push_back("#0");
    physics_panel_->bind(snapshot_.values[size_t(selected_)].physics,slots);
  }
  void change_physics(runtime::PhysicsObjectSettings value) {
    if(loading_||!document_||selected_<0||selected_joint_>=0)return;
    auto &current=snapshot_.values[size_t(selected_)].physics;value.run_sequence=current.run_sequence;value.reset_sequence=current.reset_sequence;if(current==value)return;const bool filter_change=current.enabled!=value.enabled;
    auto edit=history_edit(QStringLiteral("修改物理参数"));current=std::move(value);send();if(filter_change&&physics_only_->isChecked())QTimer::singleShot(0,this,[this]{refresh_physics_filter();});
  }

  void simulate_physics(bool reset) {
    if(loading_||!document_||selected_<0||selected_joint_>=0)return;
    auto &value=snapshot_.values[size_t(selected_)].physics;if(!value.enabled)return;
    value.paused=false;const auto sequence=std::max({physics_command_sequence_,value.run_sequence,value.reset_sequence})+1;physics_command_sequence_=sequence;if(reset)value.reset_sequence=sequence;else value.run_sequence=sequence;send();
  }
  void refresh_physics_filter() {
    if(!document_)return;const auto context=capture_edit().context;rebuild_hierarchy();restore_context(context);
  }

  bool physics_ui_test_=false;
  std::vector<int> physics_ui_targets_;
  void physics_ui_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>300000){finish_test(false,"物理界面验证超时");return;}
    if(loading_||!document_||state.generation!=document_->generation||!state.frames)return;
    try{
      auto check=[](bool ok,const char *why){if(!ok)throw std::runtime_error(why);};
      if(test_stage_==0){
        for(size_t i=0;i<snapshot_.values.size();++i)if(physics_eligible(*document_,i)){physics_ui_targets_.push_back(int(i));if(physics_ui_targets_.size()==2)break;}
        check(physics_ui_targets_.size()==2,"验证场景需要两件可模拟对象");
        for(int i:physics_ui_targets_){choose(i,-1);check(physics_panel_->isVisible(),"服装根节点没有独立物理组");physics_panel_->findChild<QCheckBox *>("PhysicsEnabled")->setChecked(true);}
        physics_only_->setChecked(true);check(hierarchy_->topLevelItemCount()==2,"物理筛选未仅显示启用对象");
        for(int i=0;i<2;++i)check(!hierarchy_->topLevelItem(i)->parent()&&hierarchy_->topLevelItem(i)->childCount()==0,"物理筛选仍维持树形关系");
        choose(physics_ui_targets_[0],-1);for(auto *p=physics_panel_->parentWidget();p;p=p->parentWidget())if(auto *dock=qobject_cast<QDockWidget *>(p)){dock->show();dock->raise();}
        auto *scroll=findChild<QScrollArea *>("ObjectPropertiesScroll");check(scroll&&scroll->findChildren<QScrollArea *>().empty(),"属性页仍有组内独立滚动区");
        findChild<QToolButton *>("MorphCollapse")->setChecked(false);if(auto *header=findChild<QToolButton *>("ExtensionCollapse"))header->setChecked(false);physics_panel_->findChild<QToolButton *>("PhysicsCollapse")->setChecked(false);test_stage_=1;return;
      }
      if(test_stage_==1){
        auto *scroll=findChild<QScrollArea *>("ObjectPropertiesScroll");check(physics_panel_->mapTo(scroll->widget(),QPoint{}).y()<220,"收起的属性组没有靠顶部排列");
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"physics-collapsed.png").wstring()));physics_panel_->findChild<QToolButton *>("PhysicsCollapse")->setChecked(true);test_stage_=2;return;
      }
      if(test_stage_==2){
        check(physics_panel_->findChild<QDoubleSpinBox *>("PhysicsMass")->isVisible(),"参数未随物理组展开");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"physics-expanded.png").wstring()));
        physics_only_->setChecked(false);check(hierarchy_->topLevelItemCount()!=2||hierarchy_->topLevelItem(0)->childCount()>0,"关闭筛选后没有恢复场景树");
        physics_only_->setChecked(true);choose(physics_ui_targets_[0],-1);physics_panel_->findChild<QCheckBox *>("PhysicsEnabled")->setChecked(false);test_stage_=3;return;
      }
      if(test_stage_==3){check(hierarchy_->topLevelItemCount()==1,"关闭物理后筛选列表未立即更新");check(state.physics_applied==0,"仅启用并切换参数组就运行了物理");finish_test(true);}
    }catch(const std::exception &e){finish_test(false,e.what());}
  }
