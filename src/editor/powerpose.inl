  PowerPosePanel *powerpose_=nullptr;
  void refresh_powerpose(const RenderStatus &state) {
    if(!powerpose_) return;int index=selected_skin();const auto selections=tree_selection(hierarchy_);
    if(index>=0) {
      const auto &skin=document_->skeletons.skins[index];const auto root=skin.joints.empty()?std::string{}:skin.joints.front().id;
      if((root!="Genesis8Female"&&root!="Genesis8Male"&&root!="Genesis8_1Female"&&root!="Genesis8_1Male")||selections.empty()||
        std::any_of(selections.begin(),selections.end(),[&](const auto &s){return s[0]!=selected_;})) index=-1;
      else try {if(attachment_host(*document_,size_t(selected_))!=size_t(selected_)) index=-1;}catch(...) {index=-1;}
    }
    const bool ready=index>=0&&!loading_&&state.generation==document_->generation&&state.applied_revision==snapshot_.revision&&state.error.empty()&&state.edit_error.empty()&&!state.pending_payloads;
    powerpose_->bind(index>=0?&document_->skeletons.skins[index]:nullptr,index,selected_,document_?document_->generation:0,snapshot_.revision,roots_,ready,pose_pins_);
  }
  void select_powerpose(const runtime::BoundPosePoint &point) {
    const int skin=selected_skin(),target=selected_;if(skin<0||point.joints.empty()) return;
    if(point.point->kind!=runtime::PosePointKind::group) {
      int bone=runtime::powerpose_joint(document_->skeletons.skins[skin],point.point->node);
      if(bone<0) bone=point.joints.front();choose(target,bone==0?-1:bone);return;
    }
    {QSignalBlocker block(hierarchy_);hierarchy_->clearSelection();QTreeWidgetItem *first=nullptr;
      for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) if((*it)->data(0,Qt::UserRole).toInt()==target&&std::find(point.joints.begin(),point.joints.end(),(*it)->data(0,Qt::UserRole+1).toInt())!=point.joints.end()) {
        (*it)->setSelected(true);if(!first) first=*it;
      }
      if(first) {hierarchy_->setCurrentItem(first,0,QItemSelectionModel::NoUpdate);hierarchy_->scrollToItem(first);}
    }sync_selection();
  }
  void powerpose_action(const runtime::BoundPosePoint &point,int operation,bool enabled) {
    const int index=selected_skin();if(index<0||point.joints.empty()) return;
    if(operation==0) {select_powerpose(point);auto *d=findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"));if(d) {d->show();d->raise();}return;}
    if(operation>=2) {pin_joint(index,point.joints.front(),enabled,operation==3);refresh_powerpose(renderer_->status());return;}
    if(point.figure) {
      auto &t=snapshot_.values[selected_].transform;
      for(const auto &slot:point.slots) for(const auto &c:slot) {auto &v=c.channel<3?t.translation_cm:t.rotation_degrees;float *value=c.channel%3==0?&v.x:c.channel%3==1?&v.y:&v.z;*value=0;}
    } else snapshot_.poses[index]=runtime::reset_powerpose(document_->skeletons.skins[index],point,snapshot_.poses[index]);
    send();select(selected_,selected_joint_,selected_light_);
  }
