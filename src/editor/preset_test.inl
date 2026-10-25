  bool preset_test_=false,preset_test_busy_=false;
  void preset_test_tick(const RenderStatus &state){
    if(preset_test_busy_)return;
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>120000){finish_test(false,"预设菜单验证超时");return;}
    if(!state.error.empty()){finish_test(false,state.error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.samples<1)return;
    preset_test_busy_=true;
    try{
      choose(0);const auto before=capture_edit();auto *menu=findChild<QMenu *>("SavePresetMenu");if(!menu)throw std::runtime_error("缺少保存预设菜单");
      nlohmann::json checks=nlohmann::json::array();
      for(const auto &[action_name,file_name,type]:{std::tuple{"SaveMaterialPreset","menu-material.duf","preset_material"},std::tuple{"SavePosePreset","menu-pose.duf","preset_pose"},std::tuple{"SaveMorphPreset","menu-morph.duf","preset_shape"}}){
        auto *action=menu->findChild<QAction *>(action_name);if(!action)throw std::runtime_error("缺少预设保存动作");
        const auto file=std::filesystem::absolute(output_/file_name);bool accepted=false;
        QTimer dialog_timer;dialog_timer.setInterval(50);connect(&dialog_timer,&QTimer::timeout,this,[&]{
          for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QFileDialog *>(widget);dialog&&dialog->isVisible()){
            dialog->setOption(QFileDialog::DontConfirmOverwrite);dialog->setDirectory(QString::fromStdWString(file.parent_path().wstring()));
            auto *name=dialog->findChild<QLineEdit *>("fileNameEdit");if(!name){dialog->reject();dialog_timer.stop();return;}name->setText(QString::fromStdWString(file.filename().wstring()));
            accepted=true;dialog_timer.stop();QMetaObject::invokeMethod(dialog,"accept");return;
          }else if(auto *error=qobject_cast<QMessageBox *>(widget);error&&error->isVisible()){error->accept();dialog_timer.stop();}
        });
        dialog_timer.start();action->trigger();dialog_timer.stop();
        if(!accepted||!std::filesystem::exists(file)||daz::read_document_file(file).at("asset_info").at("type")!=type)throw std::runtime_error("菜单未保存正确的 DUF 预设");
        const QImage thumbnail(QString::fromStdWString(file.wstring())+".png");if(thumbnail.isNull()||thumbnail.size()!=QSize(256,256))throw std::runtime_error("菜单未保存 256×256 预设缩略图");
        if(!same_edit(capture_edit(),before))throw std::runtime_error("保存预设改变了场景");checks.push_back(action_name);
      }
      menu->popup(mapToGlobal(QPoint(80,80)));QApplication::processEvents();screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"preset-menu.png").wstring()));menu->hide();
      std::ofstream(output_/"preset-menu-checks.json")<<nlohmann::json({{"result","PASS"},{"checks",checks}}).dump(2);finish_test(true);
    }catch(const std::exception &e){finish_test(false,e.what());}
  }
