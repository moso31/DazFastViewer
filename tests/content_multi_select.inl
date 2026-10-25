static void multi_select_checks() {
  QTemporaryDir temp;const auto root=temp.filePath("library");
  const auto first=root+"/A Top.duf",second=root+"/B Shorts.duf";write(first);write(second);
  ContentBrowser browser(nullptr,temp.filePath("ui.ini"),temp.filePath("cache"));browser.resize(780,640);browser.set_roots({root});browser.show();
  auto *view=browser.findChild<QListView *>("contentItems");browser.locate(first);
  until([&]{return view->isVisible()&&view->model()->rowCount()==2;});
  QStringList opened;int batches=0,singles=0;browser.open_assets=[&](const QStringList &paths){opened=paths;++batches;};browser.open_asset=[&](const QString &){++singles;};
  auto a=view->model()->index(0,0),b=view->model()->index(1,0);QApplication::processEvents();
  QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(a).center());
  QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::ControlModifier,view->visualRect(b).center());
  require(view->selectionModel()->selectedRows().size()==2,"Ctrl 多选服装失败");
  QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(a).center());
  QTest::mouseDClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(a).center());
  require(batches==1&&singles==0&&opened==QStringList{first,second},"双击没有保留并一次提交全部选择");
  QTest::keyClick(view,Qt::Key_Return);require(batches==2&&singles==0,"回车未批量激活选择");
}
