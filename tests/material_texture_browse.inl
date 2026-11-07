static void material_texture_browse(QApplication &app){
  QTemporaryDir temp;check(temp.isValid(),"贴图浏览测试目录创建失败");QDir folder(temp.path());folder.mkdir("first");folder.mkdir("second");
  const auto first=folder.filePath("first/a.png"),second=folder.filePath("second/b.png");QImage pixel(2,2,QImage::Format_RGB32);pixel.fill(Qt::red);check(pixel.save(first)&&pixel.save(second),"贴图浏览夹具写入失败");
  auto d=fixture();d->loaded.scene.textures.push_back({"first",std::filesystem::path(first.toStdWString()),ir::ColorSpace::srgb});d->loaded.scene.materials[0].color_texture=0;
  auto snapshot=initial_snapshot(*d);MaterialPanel panel;panel.resize(650,650);panel.bind(d,&snapshot,0);panel.restore_selection({{"figure","Skin"}});panel.show();app.processEvents();
  auto browse=[&](){QString opened;QTimer dialog_poll;dialog_poll.setInterval(10);
    QObject::connect(&dialog_poll,&QTimer::timeout,&panel,[&]{auto *dialog=qobject_cast<QFileDialog *>(app.activeModalWidget());if(dialog){opened=dialog->directory().absolutePath();dialog_poll.stop();dialog->reject();}});dialog_poll.start();
    QTimer::singleShot(0,&panel,[&]{auto *menu=qobject_cast<QMenu *>(app.activePopupWidget());if(!menu)return;
      QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().front()).center());
    });
    panel.findChild<QPushButton *>("material/color_texture")->click();app.processEvents();return opened;
  };
  check(browse()==QFileInfo(first).absolutePath(),"浏览未打开当前原始贴图文件夹");
  snapshot.material_overrides["figure"]["Skin"]["color_texture"]={{"file",second.toUtf8().toStdString()}};
  check(browse()==QFileInfo(second).absolutePath(),"浏览仍使用上次目录，没有读取当前覆盖贴图");
}
