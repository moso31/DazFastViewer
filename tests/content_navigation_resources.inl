static QStringList visible_content_paths(QListView *view) {
  QStringList result;for(int row=0;row<view->model()->rowCount();++row)result.append(view->model()->index(row,0).data(Qt::UserRole).toString());result.sort();return result;
}
static void recent_hair_checks() {
  QTemporaryDir temp;const auto root=temp.path()+"/library",entry=root+"/Products/Model.duf",link=root+"/Products/Model.duf.djl",material=root+"/Products/Color.duf",settings=temp.path()+"/settings.ini";
  write(root+"/data/Vendor/Base.dsf",R"({"node_library":[{"id":"hair","presentation":{"type":"Follower/Hair/Long"}}]})");
  write(entry,R"({"asset_info":{"type":"wearable"},"scene":{"nodes":[{"url":"/data/Vendor/Base.dsf#hair","geometries":[{}]}]}})");
  write(link,R"({"path":"/Products/Model.duf"})");
  write(material,R"({"asset_info":{"type":"preset_material"},"scene":{"nodes":[{"presentation":{"type":"Follower/Hair"}}]}})");
  require(content_file_category(entry,{root})=="hair"&&content_file_category(link,{root})=="hair","真实引用或 DJL 的头发分类失败");
  require(content_file_category(material,{root})=="material","头发材质被迁移为发型");
  {ContentHistory history(settings);history.used(entry,"wearable",100);history.used(link,"wearable",200);history.used(material,"material",300);}
  ContentBrowser browser(nullptr,settings,temp.path()+"/cache");browser.resize(900,600);browser.set_roots({root});browser.show();browser.show_recent();
  auto *categories=browser.findChild<QListWidget *>("contentCategories");auto *view=browser.findChild<QListView *>("contentItems");
  categories->setCurrentRow(content_categories().indexOf("hair"));until([&]{return view->model()->rowCount()==2;});
  require(view->model()->index(0,0).data(Qt::UserRole).toString()==link,"迁移改变了发型使用顺序");
  ContentHistory migrated(settings);require(migrated.recent("wearable").empty()&&migrated.recent("material").size()==1&&migrated.recent("hair").back().used==100,"迁移没有持久化或改变了使用时间");
  auto stale=migrated.recent("hair").front();migrated.used(stale.path,"scene",400);migrated.recategorize(stale,"hair");require(migrated.recent().front().category=="scene","后台旧分类覆盖了新使用记录");
  std::cout<<"hair metadata / DJL / material distinction / existing history migration / timestamp / stale results: PASS\n";
}
static void search_filter_checks() {
  QTemporaryDir temp;const auto root=temp.path()+QStringLiteral("/中文库"),cache=temp.path()+"/cache";
  const auto pillow=root+"/Environments/Toon Nursery/Iray Materials/STN Pillow Pink.duf",data=root+"/DaTa/Sue Yee/Hat/Hat_5526.dsf";
  const auto script=root+"/Scripts/Apply.dse",legacy=root+"/People/Hat.DSF",unicode=root+QStringLiteral("/Poses/站姿 [01].duf"),ordinary=root+"/Products/Data/Visible.duf";
  const QStringList paths{pillow,script,legacy,unicode,ordinary};for(const auto &path:paths)write(path);write(data);write(root+"/DaTa/Hidden.duf");
  // 模拟上一版持久化索引：启动时不得先发布包含底层 DSF 的旧快照。
  const QStringList roots{root,root+"/DaTa/Sue Yee"};QDir().mkpath(cache);
  const auto key=QCryptographicHash::hash(roots.join('\n').toCaseFolded().toUtf8(),QCryptographicHash::Sha256).toHex();
  {QFile file(cache+"/"+key+".index");require(file.open(QIODevice::WriteOnly),"旧索引创建失败");QDataStream stream(&file);stream.setVersion(QDataStream::Qt_6_10);stream<<quint32(0x44465632)<<roots<<quint32(1)<<data;}
  ContentIndex index(cache);bool leaked=false;index.changed=[&]{for(const auto &entry:*index.entries())if(entry.path==data)leaked=true;};index.refresh(roots,true);until([&]{return !index.scanning();});
  auto actual=query(index,"").paths,expected=paths;actual.sort();expected.sort();require(actual==expected&&!leaked,"data 排除、目录边界、重叠库或旧索引迁移错误");
  require(query(index,"Toon Nursery").paths.empty()&&query(index,"Iray Materials").paths.empty(),"父文件夹仍然命中搜索");
  require(query(index,"pILLoW").paths==QStringList{pillow},"文件名大小写匹配失败");
  require(query(index,QStringLiteral("站姿 [01]")).paths==QStringList{unicode},"Unicode 文件名或字面匹配失败");
  require(query(index,"Pillow",root+"/People").paths.empty(),"文件名查询忽略了目录范围");
  ContentBrowser browser(nullptr,temp.path()+"/settings.ini",cache);browser.resize(900,600);browser.set_roots(roots);browser.show();
  auto *view=browser.findChild<QListView *>("contentItems");auto *search=browser.findChild<QComboBox *>("contentSearch");
  until([&]{return browser.property("contentIndexSize").toInt()==paths.size();});
  search->setEditText("Pillow");QTest::keyClick(search->lineEdit(),Qt::Key_Return);until([&]{return visible_content_paths(view)==QStringList{pillow};});
  search->setEditText("Toon Nursery");QTest::keyClick(search->lineEdit(),Qt::Key_Return);until([&]{return visible_content_paths(view).isEmpty();});
  require(search->lineEdit()->isClearButtonEnabled(),"搜索框缺少原生清空按钮");
  QTest::keyClick(search->lineEdit(),Qt::Key_Escape);until([&]{return search->currentText().isEmpty()&&browser.findChild<QComboBox *>("contentTabs")->currentIndex()==0&&!visible_content_paths(view).isEmpty();});
  std::cout<<"filename search / data exclusion / overlapping roots / legacy cache / scripts / Unicode / scope / clear: PASS\n";
}
static QToolButton *breadcrumb_button(ContentBrowser &browser,const QString &path) {
  for(auto *button:browser.findChild<QWidget *>("contentBreadcrumbs")->findChildren<QToolButton *>())if(button->property("contentBreadcrumbPath").toString()==path)return button;
  throw std::runtime_error("目录缺少面包屑按钮");
}
static void search_navigation_checks() {
  QTemporaryDir temp;const auto root=temp.path()+"/library",first=root+"/First",second=root+"/Second",nested=first+"/Child/nested.duf";
  write(first+"/a.duf");write(second+"/b.duf");write(nested);
  ContentBrowser browser(nullptr,temp.path()+"/settings.ini",temp.path()+"/cache");browser.resize(1000,600);browser.set_roots({root});browser.show();browser.locate(first+"/a.duf");
  auto *view=browser.findChild<QListView *>("contentItems");auto *tree=browser.findChild<QTreeView *>("contentTree");auto *files=qobject_cast<QFileSystemModel *>(tree->model());
  auto *search=browser.findChild<QComboBox *>("contentSearch");auto *tabs=browser.findChild<QComboBox *>("contentTabs");
  until([&]{return view->currentIndex().data(Qt::UserRole).toString()==first+"/a.duf"&&files->filePath(tree->currentIndex())==first;});
  auto run_search=[&]{search->setEditText("nested");QTest::keyClick(search->lineEdit(),Qt::Key_Return);until([&]{return tabs->currentIndex()==2&&visible_content_paths(view)==QStringList{nested};});};
  auto click_folder=[&](const QString &folder){const auto index=files->index(folder);require(index.isValid(),"目录树索引缺失");tree->scrollTo(index);tree->doItemsLayout();const auto rect=tree->visualRect(index).intersected(tree->viewport()->rect());require(!rect.isEmpty(),"目录树点击目标不可见");QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center());};
  run_search();click_folder(first);
  const QStringList first_items{first+"/Child",first+"/a.duf"};
  until([&]{return tabs->currentIndex()==0&&visible_content_paths(view)==first_items;});
  require(search->currentText()=="nested","目录导航丢失搜索文本");QTest::qWait(220);require(visible_content_paths(view)==first_items,"迟到的搜索覆盖了当前目录");
  run_search();click_folder(second);until([&]{return tabs->currentIndex()==0&&visible_content_paths(view)==QStringList{second+"/b.duf"};});
  run_search();QTest::mouseClick(breadcrumb_button(browser,root),Qt::LeftButton);until([&]{return tabs->currentIndex()==0&&visible_content_paths(view)==QStringList{first,second};});
  // 查询防抖计时器尚未触发时导航，也不能随后跳回搜索结果。
  search->setEditText("no matching resource");QTest::mouseClick(breadcrumb_button(browser,root),Qt::LeftButton);QTest::qWait(250);
  require(tabs->currentIndex()==0&&visible_content_paths(view)==QStringList{first,second},"排队的搜索覆盖了面包屑导航");
  browser.findChild<QSlider *>("contentZoom")->setValue(0);run_search();click_folder(second);
  until([&]{return tabs->currentIndex()==0&&!view->isVisible()&&files->filePath(tree->currentIndex())==second;});
  run_search();require(view->isVisible(),"列表模式按 Enter 未恢复搜索结果");
  std::cout<<"search / selected folder / other folder / breadcrumbs / queued search / list mode: PASS\n";
}
static void verify_resource_items(ContentBrowser &browser,const QStringList &paths,const QColor &icon_color={},const QColor &tip_color={}) {
  auto *view=browser.findChild<QListView *>("contentItems");auto expected=paths;expected.sort();until([&]{return visible_content_paths(view)==expected;});
  QString opened;browser.open_asset=[&](const QString &path){opened=path;};
  for(const auto &path:expected) {
    QModelIndex index;for(int row=0;row<view->model()->rowCount();++row){const auto item=view->model()->index(row,0);if(item.data(Qt::UserRole).toString()==path)index=item;}
    require(index.data().toString()==QFileInfo(path).fileName()&&index.data(Qt::ToolTipRole).toString()==path,"资源名称或悬停路径使用了 PNG");
    until([&]{const auto image=index.data(Qt::DecorationRole).value<QIcon>().pixmap(32,32).toImage();return !image.isNull()&&(!icon_color.isValid()||image.pixelColor(16,16)==icon_color);});
    view->setCurrentIndex(index);view->scrollTo(index);view->doItemsLayout();const auto point=view->visualRect(index).center();
    QHelpEvent hover(QEvent::ToolTip,point,view->viewport()->mapToGlobal(point));QApplication::sendEvent(view->viewport(),&hover);
    auto *preview_text=browser.findChild<QLabel *>("contentPreviewText");require(preview_text&&preview_text->text()==path,"预览弹窗仍然描述 PNG");
    if(tip_color.isValid())until([&]{const auto image=browser.findChild<QLabel *>("contentPreviewImage")->pixmap().toImage();return !image.isNull()&&image.pixelColor(image.width()/2,image.height()/2)==tip_color;});
    QTest::keyClick(view,Qt::Key_Return);require(opened==path,"激活资源时传递了 PNG 路径");
    bool copied=false;QTimer::singleShot(0,[&]{if(auto *menu=qobject_cast<QMenu *>(QApplication::activePopupWidget()))if(auto *action=menu->findChild<QAction *>("CopyActualContentPath")){copied=true;menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);}});
    view->customContextMenuRequested(point);require(copied&&QApplication::clipboard()->text()==QDir::toNativeSeparators(QFileInfo(path).canonicalFilePath()),"右键拷贝没有使用资源文件路径");
  }
  browser.open_asset={};
}
static void resource_thumbnail_checks() {
  QTemporaryDir temp;const auto root=temp.path()+"/library";QStringList resources;
  QImage image(64,64,QImage::Format_RGB32);
  for(const QString &name:QStringList{"Apply.dse","Tutorial.dsa","Binary.dsb","Shape.DSF","Scene.duf","Scene.dufex","Legacy.ds","Legacy.daz"}) {
    const auto path=root+"/"+name;write(path);resources.append(path);const auto base=root+"/"+QFileInfo(path).completeBaseName();
    image.fill(Qt::red);require(image.save(base+".png")&&image.save(path+".png"),"图标创建失败");image.fill(Qt::green);require(image.save(base+".tip.png")&&image.save(path+".tip.png"),"悬停图创建失败");
  }
  image.save(root+"/orphan.png");image.save(root+"/folder.png");
  {ContentIndex index(temp.path()+"/index");index.refresh({root});until([&]{return !index.scanning();});auto actual=query(index,"").paths;actual.sort();auto expected=resources;expected.sort();require(actual==expected,"搜索漏掉 DAZ 资源或把 PNG 加入索引");}
  ContentBrowser browser(nullptr,temp.path()+"/settings.ini",temp.path()+"/cache");browser.resize(1100,650);browser.set_roots({root});browser.show();
  verify_resource_items(browser,resources,Qt::red,Qt::green);
  auto *view=browser.findChild<QListView *>("contentItems");auto *search=browser.findChild<QComboBox *>("contentSearch");
  search->setEditText("Apply");QTest::keyClick(search->lineEdit(),Qt::Key_Return);verify_resource_items(browser,{root+"/Apply.dse"},Qt::red,Qt::green);
  browser.record_use(root+"/Apply.dse","material");browser.show_recent();until([&]{return view->model()->rowCount()==1;});require(visible_content_paths(view)==QStringList{root+"/Apply.dse"},"近期使用记录了 PNG");
  browser.locate(root);browser.findChild<QSlider *>("contentZoom")->setValue(0);
  auto *tree=browser.findChild<QTreeView *>("contentTree");auto *files=qobject_cast<QFileSystemModel *>(tree->model());
  until([&]{return files->rowCount(tree->rootIndex())==resources.size();});QStringList tree_paths;for(int row=0;row<files->rowCount(tree->rootIndex());++row)tree_paths.append(files->filePath(files->index(row,0,tree->rootIndex())));tree_paths.sort();resources.sort();require(tree_paths==resources,"列表树与图标模式的资源条目不一致");
  std::cout<<"DAZ entries / PNG icons / tip previews / search / activation / clipboard / tree: PASS\n";
}
static void resource_thumbnail_real() {
  QTemporaryDir temp;const QString root="H:/G3/People/Genesis 8 Female/Anatomy/3feetwolf/HD Nipples - 2.0/HD Textures";
  const QStringList paths{root+"/!About HD Textures.dsa",root+"/Apply HD Textures.dse",root+"/Create HD Nipples Textures for Current Character.dse"};
  for(const auto &path:paths)require(QFileInfo(path).isFile(),"真实 HD Textures 资源缺失");
  ContentBrowser browser(nullptr,temp.path()+"/settings.ini",temp.path()+"/cache");browser.resize(1050,580);if(QApplication::screens().size()>1)browser.move(QApplication::screens().at(1)->availableGeometry().topLeft()+QPoint(40,40));browser.set_roots({root});browser.show();
  verify_resource_items(browser,paths);std::cout<<"real entries / names / preview paths / activation / clipboard: PASS"<<std::endl;
  auto *view=browser.findChild<QListView *>("contentItems");
  for(int row=0;row<view->model()->rowCount();++row){const auto index=view->model()->index(row,0);const auto path=index.data(Qt::UserRole).toString();const QImage expected(preview_candidates(path).at(1));require(!expected.isNull(),"真实配套图标缺失");until([&]{const auto actual=index.data(Qt::DecorationRole).value<QIcon>().pixmap(112,112).toImage();const auto expected_scaled=expected.scaled(actual.size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);if(actual.isNull())return false;const auto a=actual.pixelColor(actual.width()/2,actual.height()/2),b=expected_scaled.pixelColor(expected_scaled.width()/2,expected_scaled.height()/2);return std::abs(a.red()-b.red())<=8&&std::abs(a.green()-b.green())<=8&&std::abs(a.blue()-b.blue())<=8;});}
  const auto output=QDir::currentPath()+"/artifacts/content-browser/resource-icons";QDir().mkpath(output);QTest::qWait(100);require(browser.grab().save(output+"/hd-textures.png"),"验收截图保存失败");
  write(output+"/result.json",QJsonDocument(QJsonObject{{"result","PASS"},{"entries",QJsonArray::fromStringList(visible_content_paths(view))},{"duplicate_png_entries",0},{"preview_and_clipboard_use_resource",true}}).toJson());
  std::cout<<"real HD Textures / 3 resources / original thumbnails / preview / clipboard: PASS\n";
}
