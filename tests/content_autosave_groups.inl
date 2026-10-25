static void autosave_group_checks(const QString &output={}) {
  std::unique_ptr<UiScale> style;if(!output.isEmpty())style=std::make_unique<UiScale>(false);
  QTemporaryDir temp;const auto root=temp.filePath("library"),autosaves=temp.filePath("autosaves"),settings=temp.filePath("history.ini");
  QStringList automatic,recent;
  for(int i=-1;i<=6;++i){const auto path=autosaves+(i<0?"/temp.dufex":"/temp"+QString::number(i)+".dufex");write(path);automatic.append(path);}
  QImage screenshot(256,256,QImage::Format_RGB32);screenshot.fill(Qt::cyan);require(screenshot.save(automatic[0]+".png"),"自动保存截图创建失败");
  for(int i=0;i<14;++i){const auto path=root+QStringLiteral("/场景版本 %1.dufex").arg(i+1);write(path);recent.append(path);}
  const auto named_temp=root+"/temp.dufex",pose=root+"/pose.duf";write(named_temp);write(pose);
  {ContentHistory history(settings);history.used(automatic.front(),"scene");}
  ContentBrowser browser(nullptr,settings,temp.filePath("cache"));browser.set_roots({root});browser.set_autosave_directory(autosaves);browser.resize(1000,780);browser.show();
  for(const auto &path:recent)browser.record_use(path,"scene");browser.record_use(named_temp,"scene");browser.record_use(pose,"pose");browser.record_use(automatic.front(),"scene");
  auto *view=browser.findChild<QListView *>("contentItems");auto *categories=browser.findChild<QListWidget *>("contentCategories");auto *zoom=browser.findChild<QSlider *>("contentZoom");
  auto category=[&](const QString &name){for(int i=0;i<categories->count();++i)if(categories->item(i)->data(Qt::UserRole)==name){categories->setCurrentRow(i);return;}};
  auto index=[&](int row){return view->model()->index(row,0);};
  browser.show_recent();category("scene");until([&]{return view->model()->rowCount()==25;});
  require(index(0).data().toString()==QStringLiteral("自动保存")&&index(9).data().toString()==QStringLiteral("近期使用"),"场景分组顺序错误");
  for(int i=0;i<8;++i)require(index(i+1).data(Qt::UserRole).toString()==automatic[i],"自动保存文件未按轮转顺序排列");
  auto icon=[&](int row){return qvariant_cast<QIcon>(index(row).data(Qt::DecorationRole)).pixmap(32,32).toImage();};
  until([&]{const auto image=icon(1);return !image.isNull()&&image.pixelColor(16,16)==QColor(Qt::cyan);});
  require(icon(2)==QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(32,32).toImage(),"没有截图的旧自动保存未使用空文件图标");
  require(!(index(0).flags()&Qt::ItemIsSelectable)&&!(index(9).flags()&Qt::ItemIsEnabled),"分组标题可被当成资源选中");
  for(int level:{0,1,7,21})for(int width:{460,1000}) {
    zoom->setValue(level);browser.resize(width,780);QTest::qWait(35);view->doItemsLayout();
    const auto a=view->visualRect(index(0)),b=view->visualRect(index(9));
    require(a.left()>=0&&a.right()<view->viewport()->width()&&a.width()>=view->viewport()->width()-2,"自动保存标题没有独占整行");
    require(view->visualRect(index(1)).top()>a.bottom()&&b.top()>view->visualRect(index(8)).bottom()&&view->visualRect(index(10)).top()>b.bottom(),"图标跨越分组标题或组间发生重叠");
    view->scrollTo(index(24),QAbstractItemView::PositionAtBottom);QTest::qWait(10);
    require(view->viewport()->rect().intersects(view->visualRect(index(24))),"最后一项无法通过同一外层滚动条访问");
  }
  zoom->setValue(7);browser.resize(1000,780);view->scrollToTop();QTest::qWait(50);
  QString opened;browser.open_asset=[&](const QString &path){opened=path;};
  QTest::mouseDClick(view->viewport(),Qt::LeftButton,{},view->visualRect(index(0)).center());require(opened.isEmpty(),"双击分组标题错误打开了文件");
  QTest::mouseClick(view->viewport(),Qt::LeftButton,{},view->visualRect(index(1)).center());
  QTest::mouseDClick(view->viewport(),Qt::LeftButton,{},view->visualRect(index(1)).center());require(opened==automatic.front(),"双击自动保存没有打开对应 DUFEX");
  if(!output.isEmpty()){QDir().mkpath(output);view->scrollToTop();QTest::qWait(50);browser.grab().save(output+"/autosave-groups.png");}
  category("pose");require(view->model()->rowCount()==1&&index(0).data(Qt::UserRole).toString()==pose&&!view->property("contentGrouped").toBool(),"非场景分类出现分组");
  category("all");for(int i=0;i<view->model()->rowCount();++i)require(!index(i).data(Qt::UserRole).toString().startsWith(autosaves+"/"),"自动保存混入了普通近期使用");
  require(QFile::remove(automatic.front()),"测试自动保存删除失败");category("scene");require(view->model()->rowCount()==24&&index(1).data(Qt::UserRole).toString()==automatic[1],"切回场景没有刷新自动保存文件");
}
