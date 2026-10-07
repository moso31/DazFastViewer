#include <QClipboard>
static void actual_path_checks() {
  QTemporaryDir temp;const auto library=temp.path()+"/library",other=temp.path()+"/other library";
  const auto actual=other+QStringLiteral("/People/角色 资源.duf"),entry=library+"/Links/resource.djl",chain=other+"/Links/chain.djl",missing=library+"/Links/missing.djl";
  write(actual);write(entry,R"({"path":"/Links/chain.djl"})");
  write(chain,QByteArray::fromStdString(nlohmann::json{{"path","/People/角色%20资源.duf"}}.dump()));write(missing,R"({"path":"/not-found.duf"})");
  ContentBrowser browser(nullptr,temp.path()+"/settings.ini",temp.path()+"/cache");browser.set_roots({library,other});browser.resize(780,600);browser.show();
  auto *view=browser.findChild<QListView *>("contentItems");auto *tree=browser.findChild<QTreeView *>("contentTree");auto *files=qobject_cast<QFileSystemModel *>(tree->model());
  auto copy_menu=[&](QAbstractItemView *list,const QModelIndex &index) {
    bool copied=false;QTimer::singleShot(0,[&]{if(auto *menu=qobject_cast<QMenu *>(QApplication::activePopupWidget()))if(auto *action=menu->findChild<QAction *>("CopyActualContentPath")){copied=true;require(action->text()==QStringLiteral("拷贝实际路径"),"实际路径菜单名称不正确");menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);}});
    list->customContextMenuRequested(list->visualRect(index).center());require(copied,"内容右键菜单没有拷贝实际路径");
  };
  auto copy_entry=[&](const QString &path) {
    require(browser.locate(path),"路径测试无法定位入口");until([&]{return view->currentIndex().data(Qt::UserRole).toString()==path;});copy_menu(view,view->currentIndex());
  };
  const auto expected=QDir::toNativeSeparators(QFileInfo(actual).canonicalFilePath());
  copy_entry(actual);require(QApplication::clipboard()->text()==expected,"普通内容没有复制完整磁盘路径");
  copy_entry(entry);require(QApplication::clipboard()->text()==expected,"跨库链式 DJL 复制了链接路径或丢失中文与空格");
  browser.record_use(entry,"figure");browser.show_recent();until([&]{return view->model()->rowCount()==1;});copy_menu(view,view->model()->index(0,0));require(QApplication::clipboard()->text()==expected,"近期使用没有解析实际资源路径");
  copy_entry(missing);QApplication::clipboard()->setText("keep clipboard");copy_menu(view,view->currentIndex());require(QApplication::clipboard()->text()=="keep clipboard"&&browser.findChild<QLabel *>("contentNotice")->isVisible(),"失效链接覆盖了剪贴板或没有给出提示");
  require(browser.locate(entry),"树菜单测试定位失败");until([&]{return files->index(entry).isValid();});tree->setCurrentIndex(files->index(entry));tree->scrollTo(files->index(entry));QTest::qWait(20);copy_menu(tree,files->index(entry));require(QApplication::clipboard()->text()==expected,"内容树没有复制实际资源路径");
  const auto directory=QFileInfo(entry).path();copy_menu(tree,files->index(directory));require(QApplication::clipboard()->text()==QDir::toNativeSeparators(QFileInfo(directory).canonicalFilePath()),"目录菜单没有复制实际目录路径");
}
