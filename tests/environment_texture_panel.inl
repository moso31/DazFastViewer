static void environment_texture_panel(QApplication &app) {
  using namespace dfv::editor;
  QTemporaryDir folder;require(folder.isValid(),"环境贴图测试目录失败");
  const auto file=folder.filePath(QStringLiteral("sky.hdr"));{QFile f(file);require(f.open(QIODevice::WriteOnly),"贴图测试文件失败");f.write("fixture");}
  dfv::ir::OptionNode node;node.id="environment";dfv::ir::Option map;map.id=map.label="Environment Map";map.group="/Environment/Dome";map.value={2};map.step=.1;map.supported=true;node.parameters={map};
  std::string path=file.toUtf8().toStdString();int edits=0;
  QScrollArea outer;outer.setWidgetResizable(true);auto *panel=new ParameterPanel;panel->shared_scroll(&outer);outer.setWidget(panel);outer.resize(640,370);
  auto bind=[&]{panel->bind_options(&node,[&](size_t i,size_t k,double v){dfv::ir::set_option(node,i,k,v);},{},{},[&]{return path;},[&](const auto &v){path=v;++edits;});};bind();outer.show();app.processEvents();
  auto *rows=panel->findChild<QTreeWidget *>("parameterRows");require(rows&&rows->topLevelItemCount()==2&&rows->topLevelItem(1)->data(0,Qt::UserRole+1).toString().endsWith("/Environment Map/texture"),"路径行不在 Environment Map 下方");
  auto field=[&]{return panel->findChild<QLineEdit *>("environmentTexturePath");};
  require(field()&&field()->text()==file&&!field()->isClearButtonEnabled(),"路径未显示或错误套用了搜索清空按钮");
  auto *spin=panel->findChild<QDoubleSpinBox *>("valueSpin");require(spin,"贴图强度控件缺失");
  auto wheel=[&](QWidget *w){const QPointF p(w->rect().center());QWheelEvent e(p,w->mapToGlobal(p.toPoint()),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&e);app.processEvents();};
  wheel(spin);require(node.parameters[0].value[0]==2,"未选中时误改 HDRI 强度");QTest::mouseClick(spin,Qt::LeftButton);wheel(spin);require(std::abs(node.parameters[0].value[0]-2.1)<1e-6,"选中后 HDRI 强度滚轮失效");
  QTest::mouseClick(field(),Qt::LeftButton);wheel(spin);require(std::abs(node.parameters[0].value[0]-2.1)<1e-6,"点击路径后旧强度仍接收滚轮");
  QTest::mouseClick(panel->findChild<QToolButton *>("environmentTextureClear"),Qt::LeftButton);require(path.empty()&&field()->text().isEmpty()&&edits==1,"清除贴图未更新状态");
  field()->setFocus();QTest::keyClicks(field(),file);QTest::keyClick(field(),Qt::Key_Return);require(path==file.toUtf8().toStdString()&&edits==2,"路径手动输入未提交");
  panel->query("Environment Map");app.processEvents();require(!rows->topLevelItem(1)->isHidden(),"搜索 Environment Map 隐藏了纹理路径");
  auto *search=panel->findChild<QLineEdit *>("parameterSearch");search->clear();app.processEvents();
  path=folder.filePath("missing.hdr").toStdString();panel->resource_states();require(field()->text()==QString::fromStdString(path),"外部恢复路径未刷新");
  require(rows->itemWidget(rows->topLevelItem(1),0)->findChild<QLabel *>("valueLabel")->text().contains(QStringLiteral("文件未找到")),"缺失贴图没有路径提示");
  // 浏览走真实非原生文件对话框，取消不能修改状态。
  QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());require(dialog,"环境贴图浏览未打开文件对话框");dialog->selectFile(file);QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);});
  QTest::mouseClick(panel->findChild<QToolButton *>("environmentTextureBrowse"),Qt::LeftButton);require(path==file.toUtf8().toStdString()&&edits==3,"浏览选择没有写入贴图路径");
  QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());require(dialog,"环境浏览取消测试缺少对话框");dialog->reject();});
  QTest::mouseClick(panel->findChild<QToolButton *>("environmentTextureBrowse"),Qt::LeftButton);require(edits==3,"取消浏览修改了贴图");
  bind();app.processEvents();require(field()&&field()->text()==file,"切换或重绑面板丢失路径");
  require(rows->verticalScrollBar()->maximum()==0,"路径栏引入嵌套滚动");
  if(qEnvironmentVariableIsSet("DFV_ENVIRONMENT_UI_CAPTURE"))outer.grab().save(qEnvironmentVariable("DFV_ENVIRONMENT_UI_CAPTURE"));
  outer.close();std::cout<<"Environment texture path / clear / browse / wheel: PASS\n";
}
