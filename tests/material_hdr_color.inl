static void material_hdr_color(QApplication &app,const QString &screenshot={}) {
  using Color=hdr_color::Color;
  const double maximum=hdr_color::linear(10);
  const Color initial={3.,2.,.125};
  HdrColorDialog dialog(initial,maximum);dialog.show();app.processEvents();
  check(dialog.color()==initial,"打开 HDR 选色器改变了原始浮点颜色");
  auto *picker=dialog.findChild<QColorDialog *>("hdrBaseColor");
  check(picker&&picker->isVisible()&&!picker->isWindow(),"HDR 基础选色器没有嵌入弹窗");
  check(picker->currentColor().greenF()<.9&&picker->currentColor().blueF()<.5,"HDR 预览截断了颜色比例");
  auto *twice=dialog.findChild<QPushButton *>("hdrDouble");auto *half=dialog.findChild<QPushButton *>("hdrHalf");
  twice->click();check(std::abs(dialog.color()[0]-6)<1e-12&&std::abs(dialog.color()[1]-4)<1e-12&&std::abs(dialog.color()[2]-.25)<1e-12,"HDR 翻倍没有在线性空间保留原始颜色比例");
  half->click();check(std::abs(dialog.color()[0]-3)<1e-12&&std::abs(dialog.color()[1]-2)<1e-12,"HDR 减半不能还原强度");
  auto *slider=dialog.findChild<QSlider *>("hdrExposureSlider");slider->setValue(200);
  check(std::abs(dialog.color()[0]-4)<1e-12&&std::abs(dialog.color()[1]-8./3)<1e-12,"曝光滑条 +2 EV 不等于四倍基础色");
  QStyleOptionSlider option;option.initFrom(slider);option.orientation=slider->orientation();option.minimum=slider->minimum();option.maximum=slider->maximum();option.sliderPosition=slider->sliderPosition();option.sliderValue=slider->value();
  const auto handle=slider->style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderHandle,slider).center();
  QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,handle);QTest::mouseMove(slider,handle+QPoint(20,0));QTest::mouseRelease(slider,Qt::LeftButton,Qt::NoModifier,handle+QPoint(20,0));
  check(dialog.color()[0]>4&&std::abs(dialog.color()[1]/dialog.color()[0]-2./3)<1e-12,"鼠标拖动曝光滑条没有保持色相并提高 HDR 强度");slider->setValue(200);
  picker->setCurrentColor(QColor::fromRgbF(.5,.25,.125));
  const auto chosen=picker->currentColor();check(std::abs(dialog.color()[0]-4*hdr_color::linear(chosen.redF()))<1e-12,"修改基础色丢失了 HDR 曝光或重复应用 gamma");
  auto *exposure=dialog.findChild<QDoubleSpinBox *>("hdrExposure");
  exposure->setValue(0);check(std::abs(dialog.color()[0]-hdr_color::linear(chosen.redF()))<1e-12,"HDR 曝光归零没有恢复基础色");
  exposure->setValue(-1);check(std::abs(dialog.color()[0]-.5*hdr_color::linear(chosen.redF()))<1e-12,"负曝光不能降低颜色强度");
  for(int i=0;i<40;++i)twice->click();
  check(hdr_color::peak(dialog.color())<=maximum&&!twice->isEnabled(),"HDR 曝光超出材质支持的上限");
  for(double component:dialog.color())check(std::isfinite(component),"HDR 曝光产生非有限值");
  dialog.close();
  HdrColorDialog black({0,0,0},maximum);black.findChild<QPushButton *>("hdrDouble")->click();check(black.color()==Color{0,0,0},"黑色曝光产生无效颜色");
  HdrColorDialog imported({500,600,700},maximum);imported.show();app.processEvents();check(imported.color()==Color{500,600,700},"导入的超范围 HDR 颜色在打开时被截断");imported.close();
  if(!screenshot.isEmpty()){HdrColorDialog preview(initial,maximum);preview.show();app.processEvents();check(preview.grab().save(screenshot),"HDR 弹窗截图保存失败");preview.close();}

  auto d=fixture();d->loaded.scene.materials[0].base_color={3,2,.125f};auto snapshot=initial_snapshot(*d);
  MaterialPanel panel;panel.resize(620,600);panel.bind(d,&snapshot,0);panel.restore_selection({{"figure","Skin"},{"dress","Nails"}});panel.show();app.processEvents();
  int changes=0,edits=0;panel.changed=[&]{++changes;};panel.edit_requested=[&](const QString &,const std::function<void()> &apply){++edits;apply();};
  auto open=[&](const std::function<void(HdrColorDialog &)> &action){
    std::exception_ptr error;bool opened=false;
    QTimer::singleShot(0,[&]{auto *active=dynamic_cast<HdrColorDialog *>(QApplication::activeModalWidget());
      if(!active){if(auto *modal=qobject_cast<QDialog *>(QApplication::activeModalWidget()))modal->reject();return;}
      opened=true;try{action(*active);}catch(...){error=std::current_exception();active->reject();}
    });
    panel.findChild<QPushButton *>("material/base_color")->click();app.processEvents();
    if(error)std::rethrow_exception(error);check(opened,"材质色块未打开 HDR 选色器");
  };
  const auto before=snapshot.material_overrides;
  open([](HdrColorDialog &active){QTest::mouseClick(active.findChild<QDialogButtonBox *>("hdrDialogButtons")->button(QDialogButtonBox::Ok),Qt::LeftButton);});check(snapshot.material_overrides==before&&changes==0&&edits==0,"直接确认 HDR 颜色产生修改或精度损失");
  open([](HdrColorDialog &active){active.findChild<QPushButton *>("hdrDouble")->click();QTest::mouseClick(active.findChild<QDialogButtonBox *>("hdrDialogButtons")->button(QDialogButtonBox::Cancel),Qt::LeftButton);});check(snapshot.material_overrides==before&&changes==0&&edits==0,"取消 HDR 编辑仍写入材质");
  open([](HdrColorDialog &active){active.findChild<QPushButton *>("hdrDouble")->click();QTest::keyClick(active.findChild<QColorDialog *>("hdrBaseColor"),Qt::Key_Escape);check(!active.isVisible(),"Esc 仅隐藏了内嵌选色器，没有取消 HDR 弹窗");});check(snapshot.material_overrides==before&&changes==0&&edits==0,"Esc 取消 HDR 编辑仍写入材质");
  open([](HdrColorDialog &active){QTest::mouseClick(active.findChild<QPushButton *>("hdrDouble"),Qt::LeftButton);QTest::mouseClick(active.findChild<QPushButton *>("hdrDouble"),Qt::LeftButton);QTest::mouseClick(active.findChild<QDialogButtonBox *>("hdrDialogButtons")->button(QDialogButtonBox::Ok),Qt::LeftButton);});
  check(changes==1&&edits==1,"HDR 一次确认没有合并为一次材质编辑");
  check(snapshot.material_overrides.size()==2&&!snapshot.material_overrides.contains("other"),"HDR 多选编辑污染未选中对象");
  for(auto surface:panel.selected_surfaces()){auto textures=d->loaded.scene.textures;const auto material=effective_material(d->loaded.scene,snapshot.material_overrides,surface.instance,surface.slot,textures);check(material.base_color==ir::Vec3{12,8,.5f},"HDR 多选未写入正确的线性 RGB");}
  check(panel.findChild<QPushButton *>("material/base_color")->text().contains(QStringLiteral("×12")),"HDR 色块没有显示强度");
  const auto saved=snapshot_json(*d,snapshot);auto restored=initial_snapshot(*d);apply_snapshot_json(*d,restored,saved);check(restored.material_overrides==snapshot.material_overrides,"HDR 弹窗颜色不能随场景保存恢复");
  open([](HdrColorDialog &active){check(active.color()==Color{12,8,.5},"重新打开丢失 HDR 强度");active.accept();});check(changes==1,"重新打开并确认 HDR 颜色产生额外修改");
  snapshot.material_overrides["dress"]["Nails"]["base_color"]=J::array({1.,0.,0.});panel.restore_selection({{"figure","Skin"},{"dress","Nails"}});app.processEvents();
  const auto mixed=snapshot.material_overrides;open([](HdrColorDialog &active){active.accept();});check(snapshot.material_overrides==mixed,"多值颜色直接确认覆盖了其他表面");
}
