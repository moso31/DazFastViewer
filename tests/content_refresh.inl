static void refresh_checks(){
  QTemporaryDir temp;const auto root=temp.path()+"/library",folder=root+"/Presets",keep=folder+"/Keep.duf",removed=folder+"/Removed.duf",renamed=folder+"/Renamed.duf",added=folder+"/Added.duf";
  write(keep);write(removed);QImage icon(32,32,QImage::Format_RGB32);icon.fill(Qt::red);require(icon.save(keep+".png"),"刷新测试缩略图写入失败");
  ContentBrowser browser(nullptr,temp.path()+"/ui.ini",temp.path()+"/cache");browser.set_roots({root});browser.resize(780,600);browser.show();require(browser.locate(keep),"刷新测试定位失败");
  auto *view=browser.findChild<QListView *>("contentItems");auto *tree=browser.findChild<QTreeView *>("contentTree");auto *search=browser.findChild<QComboBox *>("contentSearch");
  auto row=[&](const QString &path){for(int i=0;i<view->model()->rowCount();++i){const auto idx=view->model()->index(i,0);if(idx.data(Qt::UserRole).toString()==path)return idx;}return QModelIndex{};};
  auto color=[&]{const auto image=qvariant_cast<QIcon>(row(keep).data(Qt::DecorationRole)).pixmap(32,32).toImage();return image.isNull()?QColor{}:image.pixelColor(16,16);};
  until([&]{return row(keep).isValid()&&row(removed).isValid()&&color()==QColor(Qt::red);});
  auto refresh_menu=[&](QAbstractItemView *list,const QPoint &point){bool clicked=false;QTimer::singleShot(0,[&]{if(auto *menu=qobject_cast<QMenu *>(QApplication::activePopupWidget())){if(auto *action=menu->findChild<QAction *>("RefreshContent")){clicked=true;menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);}else menu->close();}});list->customContextMenuRequested(point);require(clicked,"右键菜单缺少刷新内容库");};
  // 同一个文件的缩略图更换不会触发目录项增删，必须明确清除图像缓存。
  icon.fill(Qt::green);require(icon.save(keep+".png"),"更新预览图失败");refresh_menu(view,view->visualRect(row(keep)).center());until([&]{return color()==QColor(Qt::green);});
  write(added);require(QFile::rename(removed,renamed),"重命名资源失败");refresh_menu(view,QPoint(view->viewport()->width()-2,view->viewport()->height()-2));
  until([&]{return row(added).isValid()&&row(renamed).isValid()&&!row(removed).isValid();});
  search->setEditText("Added");until([&]{return row(added).isValid()&&view->model()->rowCount()==1;});require(QFile::remove(added),"删除资源失败");refresh_menu(view,view->visualRect(row(added)).center());
  until([&]{return view->model()->rowCount()==0;});require(search->currentText()=="Added","刷新清空了搜索条件");
  write(added);refresh_menu(view,QPoint(2,2));until([&]{return row(added).isValid();});
  browser.findChild<QSlider *>("contentZoom")->setValue(0);require(browser.locate(folder),"目录树定位失败");
  refresh_menu(tree,QPoint(tree->viewport()->width()-2,tree->viewport()->height()-2));
  auto *files=qobject_cast<QFileSystemModel *>(tree->model());until([&]{return files->index(added).isValid()&&files->filePath(tree->currentIndex())==folder;});
  require(QFile::remove(renamed),"删除树资源失败");refresh_menu(tree,tree->visualRect(files->index(folder)).center());
  files=qobject_cast<QFileSystemModel *>(tree->model());until([&]{return files->index(keep).isValid()&&files->index(added).isValid();});
  require(!files->index(renamed).isValid(),"刷新后目录树仍显示已删除资源");
}
