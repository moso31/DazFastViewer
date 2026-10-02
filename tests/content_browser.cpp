#include "editor/content_browser.h"
#include "editor/content_catalog.h"
#include "daz/documents.h"
#include "daz/loader.h"
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemModel>
#include <QImage>
#include <QHelpEvent>
#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QScreen>
#include <QSlider>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#include <QWheelEvent>
#include <QMenu>
#include <QTimer>
#include <QToolButton>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>

using namespace dfv::editor;
static void require(bool ok,const char *why) {if(!ok) throw std::runtime_error(why);}
template<class F> static void until(F condition,int timeout=5000) {QElapsedTimer timer;timer.start();while(!condition()&&timer.elapsed()<timeout) QTest::qWait(5);require(condition(),"异步操作超时");}
static void write(const QString &path,const QByteArray &bytes="{}") {QDir().mkpath(QFileInfo(path).path());QFile f(path);require(f.open(QIODevice::WriteOnly),"临时文件写入失败");f.write(bytes);}
static ContentSearch query(ContentIndex &index,const QString &text,const QString &scope={}) {bool done=false;ContentSearch result;index.search(text,scope,[&](ContentSearch r){result=std::move(r);done=true;});until([&]{return done;});return result;}
static void wheel(QWidget *widget,int delta) {const QPointF p(widget->rect().center());QWheelEvent e(p,widget->mapToGlobal(p.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(widget,&e);}
static bool tree_label_visible(QTreeView *tree){const auto rect=tree->visualRect(tree->currentIndex());return rect.width()>64&&tree->viewport()->rect().contains(QPoint(rect.left()+48,rect.center().y()));}

static void source_checks() {
  QTemporaryDir temp;const auto assets=temp.path()+"/assets",data=temp.path()+"/data-library",scene=assets+"/Scenes/saved.duf",entry=assets+"/Wardrobe/LSO Heels.duf";
  const auto definition=data+"/data/H&C/Shoes/base.dsf",wrong=data+"/data/Other/base.dsf";
  using J=nlohmann::json;
  auto save=[](const QString &path,const J &json){write(path,QByteArray::fromStdString(json.dump()));};
  save(definition,{{"node_library",J::array({{{"id","shoe"},{"label","LSO Heels"}}})}});save(wrong,{{"node_library",J::array({{{"id","shoe"}}})}});
  const J node={{"id","shoe"},{"url","/data/H%26C/Shoes/base.dsf#shoe"},{"geometries",J::array({{{"url","/data/H%26C/Shoes/base.dsf#geometry"}}})}};
  save(scene,{{"asset_info",{{"type","scene"}}},{"scene",{{"nodes",J::array({node,node})}}}});
  save(entry,{{"asset_info",{{"type","wearable"}}},{"scene",{{"nodes",J::array({node})}}}});
  auto wrong_node=node;wrong_node["url"]="/data/Other/base.dsf#shoe";
  const auto decoy=assets+"/Wrong/LSO Heels.duf",material=assets+"/Materials/LSO Heels.duf",outfit=assets+"/LSO Outfit.duf";
  save(decoy,{{"asset_info",{{"type","wearable"}}},{"scene",{{"nodes",J::array({wrong_node})}}}});
  save(material,{{"asset_info",{{"type","preset_hierarchical_material"}}},{"scene",{{"nodes",J::array({node})}}}});
  save(outfit,{{"asset_info",{{"type","preset_wearables"}}},{"scene",{{"nodes",J::array({node,node})}}}});
  ContentOrigin origin{scene,"shoe",QStringLiteral("用户重命名的鞋子"),{{definition,"geometry"}}};
  const QStringList candidates{scene,decoy,material,outfit,entry},roots{assets,data};
  require(resolve_content_entry(origin,roots,candidates)==entry,"未根据跨库节点引用定位独立模型，或误选了同名资源 / 材质 / 场景 / 整套服装");
  require(resolve_content_entry(origin,roots,{decoy,material}).isEmpty(),"缺少匹配入口时错误回退到原场景或同名模型");
  require(resolve_content_entry(origin,roots,candidates,[]{return true;}).isEmpty(),"取消定位后继续返回资源");
  ContentBrowser browser(nullptr,temp.path()+"/ui.ini",temp.path()+"/cache");browser.set_roots(roots);browser.resize(780,640);browser.show();
  auto *search=browser.findChild<QComboBox *>("contentSearch");auto *view=browser.findChild<QListView *>("contentItems");auto *tree=browser.findChild<QTreeView *>("contentTree");
  search->setEditText("unrelated search");browser.locate_asset(origin);
  until([&]{return browser.property("contentLocateState").toString()=="found";});
  until([&]{return view->currentIndex().data(Qt::UserRole).toString()==entry;});
  auto *files=qobject_cast<QFileSystemModel *>(tree->model());until([&]{return files->filePath(tree->currentIndex())==QFileInfo(entry).path()&&tree_label_visible(tree);});
  require(search->currentText()=="unrelated search","模型定位清空了搜索文本");
}

static void navigation_checks(){
  QTemporaryDir temp;const auto root=temp.path()+"/library",folder=root+"/People/Hair/Styles/Very long folder name/Current";
  QDir().mkpath(folder);QImage icon(32,32,QImage::Format_RGB32);icon.fill(Qt::magenta);require(icon.save(folder+"/folder.png"),"目录图标创建失败");
  const auto texture=folder+"/texture.jpg";require(icon.save(texture),"纹理创建失败");write(folder+"/style.duf");write(folder+"/style.duf.djl",R"({"path":"People/Hair/Styles/Very long folder name/Current/style.duf"})");
  ContentBrowser browser(nullptr,temp.path()+"/ui.ini",temp.path()+"/cache");browser.resize(780,480);browser.set_roots({root});browser.show();require(browser.locate(texture),"不能定位非 DUF 文件");
  auto *view=browser.findChild<QListView *>("contentItems");until([&]{return view->currentIndex().data(Qt::UserRole).toString()==texture;});
  auto *crumbs=browser.findChild<QWidget *>("contentBreadcrumbs");require(crumbs,"缺少面包屑");
  auto *overflow=crumbs->findChild<QToolButton *>("contentBreadcrumbOverflow");browser.resize(460,480);QTest::qWait(40);require(overflow->isVisible()&&!overflow->menu()->actions().empty(),"窄窗口未折叠前部面包屑");
  QToolButton *parent=nullptr;for(auto *button:crumbs->findChildren<QToolButton *>())if(button->property("contentBreadcrumbPath").toString()==QFileInfo(folder).path())parent=button;
  require(parent,"上级目录缺少导航入口");parent->click();until([&]{for(int i=0;i<view->model()->rowCount();++i)if(view->model()->index(i,0).data(Qt::UserRole).toString()==folder)return true;return false;});
  until([&]{const auto image=qvariant_cast<QIcon>(view->model()->index(0,0).data(Qt::DecorationRole)).pixmap(32,32).toImage();return !image.isNull()&&image.pixelColor(16,16)==QColor(Qt::magenta);});
  // 新展开的目录由 QFileSystemModel 异步填充，列宽不能停留在上次定位时的值。
  const auto expanded=root+"/General/aniMate - full folder name after asynchronous expansion",entry=expanded+"/A long content filename without truncation.duf";write(entry);
  browser.findChild<QSlider *>("contentZoom")->setValue(0);browser.resize(1100,480);require(browser.locate(root),"无法回到目录树根");
  auto *tree=browser.findChild<QTreeView *>("contentTree");auto *files=qobject_cast<QFileSystemModel *>(tree->model());
  until([&]{return files->index(root+"/General").isValid();});tree->expand(files->index(root+"/General"));until([&]{return files->index(expanded).isValid();});tree->expand(files->index(expanded));
  until([&]{const auto i=files->index(entry);return i.isValid()&&tree->visualRect(i).width()>=tree->fontMetrics().horizontalAdvance(QFileInfo(entry).fileName())+30;});
  browser.resize(340,480);QTest::qWait(40);browser.resize(1100,480);QTest::qWait(40);const auto i=files->index(entry);
  require(tree->visualRect(i).width()>=tree->fontMetrics().horizontalAdvance(QFileInfo(entry).fileName())+30,"拉宽目录树后文件名仍被旧列宽截断");
}

static void checks() {
  QTemporaryDir temp;require(temp.isValid(),"临时目录失败");const auto settings=temp.path()+"/history.ini",root=temp.path()+QStringLiteral("/中文库"),pose=root+"/Poses/Standing [01].duf",scene=root+"/Scenes/Test.DUF";
  write(pose,R"({"asset_info":{"type":"preset_pose"}})");write(scene);write(root+"/Poses2/Other.duf");write(root+"/ignore.txt");
  {
    ContentHistory history(settings);for(int i=0;i<15;++i) history.searched(QString::number(i));history.searched("10");history.searched("  ");require(history.searches().size()==10&&history.searches().front()=="10","搜索历史上限 / 最近置顶失败");
    history.searched("STAND");history.searched("stand");require(history.searches().front()=="stand"&&history.searches().count("STAND")==0,"搜索历史大小写去重失败");
    for(const auto &category:content_categories()) if(category!="all") for(int i=0;i<25;++i) history.used(root+"/"+category+QString::number(i)+".duf",category,100+i);
    for(int i=0;i<130;++i) history.used(root+"/material-new"+QString::number(i)+".duf","material");
    require(history.recent().size()==100,"全局近期记录上限失败");for(const auto &category:content_categories()) if(category!="all") require(history.recent(category).size()==20,"各分类最近 20 个被热门分类挤掉");
    history.used(pose,"pose");history.used(QDir::toNativeSeparators(pose.toUpper()),"shape");require(history.recent().front().category=="shape"&&history.recent("pose").size()==19,"跨分类重开 / 规范路径去重失败");
  }
  {ContentHistory loaded(settings);require(loaded.recent().size()==100&&loaded.recent("scene").size()==20&&loaded.searches().front()=="stand","历史重启持久化失败");}
  for(const auto &[type,category]:std::vector<std::pair<std::string,QString>>{{"scene","scene"},{"preset_shape","shape"},{"preset_pose","pose"},{"preset_material","material"},{"preset_hierarchical_material","material"},{"preset_character","character"},{"preset_wearables","wearable"},{"preset_properties","properties"},{"preset_light","light"},{"preset_camera","camera"}}) require(content_category(nlohmann::json{{"asset_info",{{"type",type}}}})==category,"DUF 类型分类错误");
  require(content_category(nlohmann::json{{"scene",{{"nodes",nlohmann::json::array({{{"geometries",nlohmann::json::array({1})}}})}}}})=="scene","无类型场景回退错误");
  require(preview_candidates(pose).front()==pose+".png"&&preview_candidates(pose,true).front()==root+"/Poses/Standing [01].tip.png","预览图命名优先级错误");
  const auto cache=temp.path()+"/cache";
  {ContentIndex index(cache);index.refresh({root,root+"/Poses"});until([&]{return !index.scanning();});require(index.size()==3,"索引扩展名 / 重叠根去重失败");
    require(query(index,"standing [01]").paths==QStringList{pose},"大小写 / 包含匹配错误");require(query(index,".*").paths.empty(),"错误启用正则语义");
    require(query(index,"",root+"/Poses").paths==QStringList{pose},"路径前缀越过目录边界");require(query(index,QStringLiteral("中文库")).paths.size()==3,"Unicode 路径搜索失败");
    bool stale=false,latest=false;index.search("Standing",{},[&](auto){stale=true;});index.search("Test.DUF",{},[&](auto r){latest=r.paths==QStringList{scene};});until([&]{return latest;});require(!stale,"过期搜索结果覆盖新请求");
    QFile::remove(scene);write(root+"/new.duf");index.refresh({root,root+"/Poses"});until([&]{return !index.scanning();});require(query(index,"test.duf").paths.empty()&&query(index,"new.duf").paths.size()==1,"索引新增 / 删除刷新失败");
  }
  {ContentIndex index(cache);bool cached=false;index.changed=[&]{if(index.scanning()&&index.size()==3) cached=true;};index.refresh({root,root+"/Poses"},true);until([&]{return !index.scanning();});require(cached,"重启没有先读取持久化索引");}
  {ContentIndex index(cache);index.refresh({root});index.refresh({root+"/Poses2"});until([&]{return !index.scanning();});require(index.size()==1&&query(index,"Other").paths.size()==1,"切换内容库后旧扫描覆盖新索引");}

  QImage icon(80,120,QImage::Format_RGB32);icon.fill(Qt::red);require(icon.save(pose+".png"),"缩略图生成失败");QImage tip(220,100,QImage::Format_RGB32);tip.fill(Qt::green);tip.save(root+"/Poses/Standing [01].tip.png");
  const auto ui_settings=temp.path()+"/ui.ini";
  {
    ContentBrowser browser(nullptr,ui_settings,cache);browser.resize(780,640);browser.set_roots({root});browser.show();
    auto *zoom=browser.findChild<QSlider *>("contentZoom");auto *view=browser.findChild<QListView *>("contentItems");auto *tree=browser.findChild<QTreeView *>("contentTree");auto *search=browser.findChild<QComboBox *>("contentSearch");auto *categories=browser.findChild<QListWidget *>("contentCategories");
    require(zoom->value()==7&&view->iconSize().width()==112&&view->viewMode()==QListView::IconMode,"默认不是中等图标");
    wheel(view->viewport(),-840);require(zoom->value()==0&&!view->isVisible()&&tree->isVisible(),"最小档没有恢复列表树");
    wheel(tree->viewport(),120);require(zoom->value()==1&&view->isVisible(),"列表 Ctrl＋滚轮没有恢复图标");
    require(view->iconSize().width()==64,"最小图标尺寸发生变化");wheel(view->viewport(),60);require(zoom->value()==1,"高分辨率滚轮错误累积");wheel(view->viewport(),60);require(zoom->value()==2&&view->iconSize().width()==72,"高分辨率滚轮或细化档位错误");
    search->setEditText("standing [01]");QTest::keyClick(search->lineEdit(),Qt::Key_Return);until([&]{return view->model()->rowCount()==1&&view->model()->index(0,0).data(Qt::UserRole).toString()==pose;});
    until([&]{const auto pix=view->model()->index(0,0).data(Qt::DecorationRole).value<QIcon>().pixmap(64,64).toImage();return !pix.isNull()&&pix.pixelColor(pix.width()/2,pix.height()/2).red()>240&&pix.pixelColor(pix.width()/2,pix.height()/2).green()<10;});
    const auto cursor_before=QCursor::pos(),point=view->visualRect(view->model()->index(0,0)).center();QCursor::setPos(view->viewport()->mapToGlobal(point));QHelpEvent hover(QEvent::ToolTip,point,QCursor::pos());QApplication::sendEvent(view->viewport(),&hover);
    auto *preview=browser.findChild<QLabel *>("contentPreviewImage");until([&]{return !preview->pixmap().isNull();});const auto preview_image=preview->pixmap().toImage();require(preview_image.pixelColor(preview_image.width()/2,preview_image.height()/2).green()>240,"悬停没有优先使用 tip 大图");QCursor::setPos(cursor_before);
    int opened=0;browser.open_asset=[&](const QString &p){require(p==pose,"激活了错误文件");++opened;};view->setCurrentIndex(view->model()->index(0,0));QTest::keyClick(view,Qt::Key_Return);require(opened==1,"键盘打开丢失或重复");
    require(ContentHistory(ui_settings).recent().empty(),"仅浏览或请求打开就记入近期使用");
    int full_pose=0,partial_pose=0;browser.apply_pose=[&](const QString &path,bool partial){require(path==pose,"姿势点击路径错误");if(partial)++partial_pose;else ++full_pose;};
    const auto check_pose_clicks=[&](QAbstractItemView *items,const QModelIndex &index){
      items->scrollTo(index);until([&]{return items->visualRect(index).intersects(items->viewport()->rect());});
      const auto point=items->visualRect(index).intersected(items->viewport()->rect()).center();
      const int full_before=full_pose,partial_before=partial_pose;
      QTest::mouseClick(items->viewport(),Qt::LeftButton,Qt::NoModifier,point);
      require(full_pose==full_before+1&&partial_pose==partial_before,"普通单击没有应用全身姿势");
      QTest::mouseDClick(items->viewport(),Qt::LeftButton,Qt::NoModifier,point);
      require(full_pose==full_before+1&&partial_pose==partial_before&&opened==1,"普通双击重复应用姿势或走了普通加载入口");
      QTest::mouseClick(items->viewport(),Qt::LeftButton,Qt::ControlModifier,point);
      require(full_pose==full_before+1&&partial_pose==partial_before&&opened==1,"Ctrl+单击不应应用姿势");
      QTest::mouseDClick(items->viewport(),Qt::LeftButton,Qt::ControlModifier,point);
      QTest::mouseRelease(items->viewport(),Qt::LeftButton,Qt::ControlModifier,point);
      require(full_pose==full_before+1&&partial_pose==partial_before+1&&opened==1,"Ctrl+左键双击应仅应用一次局部姿势");
    };
    check_pose_clicks(view,view->model()->index(0,0));
    zoom->setValue(0);auto *pose_files=qobject_cast<QFileSystemModel *>(tree->model());
    until([&]{return pose_files->index(pose).isValid();});tree->expand(pose_files->index(root));tree->expand(pose_files->index(QFileInfo(pose).path()));
    check_pose_clicks(tree,pose_files->index(pose));zoom->setValue(2);browser.apply_pose={};
    for(int i=0;i<25;++i) browser.record_use(root+"/pose"+QString::number(i)+".duf","pose");browser.record_use(pose,"pose");browser.record_use(root+"/scene.duf","scene");browser.show_recent();
    require(search->currentText()=="standing [01]","切换近期使用清空了搜索文本");
    search->setEditText("not a recent resource");QTest::keyClick(search->lineEdit(),Qt::Key_Return);QTest::qWait(200);
    require(view->model()->rowCount()==27,"近期使用错误应用了搜索过滤");
    auto *tabs=browser.findChild<QComboBox *>("contentTabs");search->setEditText("standing [01]");tabs->setCurrentIndex(0);
    until([&]{return view->model()->rowCount()==1&&view->model()->index(0,0).data(Qt::UserRole).toString()==pose;});
    require(search->currentText()=="standing [01]","切回内容库丢失搜索文本");browser.show_recent();
    require(categories->currentRow()==0&&view->model()->rowCount()==27,"ALL 默认分类或全局记录错误");categories->setCurrentRow(3);require(view->model()->rowCount()==20&&view->model()->index(0,0).data(Qt::UserRole).toString()==pose,"近期分类上限或时间排序错误");
    zoom->setValue(21);require(view->iconSize().width()==224,"最大图标尺寸发生变化");browser.save();
    bool removed=false;QTimer::singleShot(0,[&]{if(auto *menu=qobject_cast<QMenu *>(QApplication::activePopupWidget()))if(auto *action=menu->findChild<QAction *>("RemoveRecentContent")){removed=true;menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);}});
    view->customContextMenuRequested(view->visualRect(view->model()->index(0,0)).center());
    require(removed&&QFileInfo::exists(pose)&&ContentHistory(ui_settings).recent().size()==26,"移除近期记录失败或误删原文件");
    browser.show_recent();require(view->model()->rowCount()==26,"移除没有刷新 ALL 分类");browser.record_use(pose,"pose");require(view->model()->rowCount()==27,"移除后的资源不能再次记入近期使用");
    for(int i=0;i<90;++i)write(root+QString("/Folder%1/Deep/Asset.duf").arg(i,3,10,QChar('0')));
    const auto located=root+"/Folder089/Deep/Asset.duf";require(browser.locate(located),"无法定位内容源文件");auto *files=qobject_cast<QFileSystemModel *>(tree->model());
    until([&]{auto index=tree->currentIndex();return files->filePath(index)==QFileInfo(located).path()&&tree_label_visible(tree);});
    until([&]{return view->currentIndex().isValid()&&view->currentIndex().data(Qt::UserRole).toString()==located;});
    require(search->currentText()=="standing [01]","定位时清空了搜索文本");
    QTest::qWait(250);require(view->currentIndex().data(Qt::UserRole).toString()==located,"定位被延迟搜索覆盖");
    QTest::keyClick(search->lineEdit(),Qt::Key_Return);until([&]{return view->model()->rowCount()==1&&view->model()->index(0,0).data(Qt::UserRole).toString()==pose;});
    for(int i=0;i<350;++i)write(root+QString("/Many/Asset%1.duf").arg(i,3,10,QChar('0')));
    const auto last=root+"/Many/Asset349.duf";require(browser.locate(last),"无法定位列表末尾图标");
    until([&]{return view->currentIndex().data(Qt::UserRole).toString()==last&&view->viewport()->rect().contains(view->visualRect(view->currentIndex()).center());});
    zoom->setValue(0);require(browser.locate(last),"列表模式不能定位资源");
    until([&]{return view->isVisible()&&view->currentIndex().data(Qt::UserRole).toString()==last&&view->viewport()->rect().contains(view->visualRect(view->currentIndex()).center());});zoom->setValue(21);
    require(browser.locate(pose),"无法切换定位来源");until([&]{auto index=tree->currentIndex();return files->filePath(index)==QFileInfo(pose).path()&&tree_label_visible(tree);});
    tabs->setCurrentIndex(1);require(search->currentText()=="standing [01]"&&view->model()->rowCount()==27,"定位后切换近期使用丢失文本或仍在过滤");tabs->setCurrentIndex(0);until([&]{return view->model()->rowCount()==1&&view->model()->index(0,0).data(Qt::UserRole).toString()==pose;});
  }
  {
    ContentBrowser restored(nullptr,ui_settings,cache);restored.set_roots({root});require(restored.findChild<QSlider *>("contentZoom")->value()==21,"图标尺寸重启丢失");restored.show_recent();require(restored.findChild<QListView *>("contentItems")->model()->rowCount()==27,"近期使用重启丢失");require(restored.findChild<QComboBox *>("contentSearch")->itemText(0)=="standing [01]","搜索记录重启丢失");
  }
  {const auto legacy_file=temp.path()+"/legacy.ini";{QSettings legacy(legacy_file,QSettings::IniFormat);legacy.setValue("content/zoom",3);}ContentBrowser migrated(nullptr,legacy_file,cache);require(migrated.findChild<QListView *>("contentItems")->iconSize().width()==160,"旧版图标配置迁移改变了显示大小");}
  std::cout<<"Content history / independent category quotas / classification / index cancellation and cache / literal search / refresh / thumbnails / UI zoom and persistence: PASS\n";
}

static void benchmark(const QStringList &roots) {
  const QString output="artifacts/content-browser";QDir().mkpath(output);ContentIndex index(output+"/benchmark-index");QElapsedTimer elapsed;elapsed.start();double cache_ms=-1;bool cache_seen=false;
  index.changed=[&]{if(index.scanning()&&index.size()&&!cache_seen) {cache_seen=true;cache_ms=elapsed.nsecsElapsed()/1e6;}};
  index.refresh(roots,true);until([&]{return !index.scanning();},240000);const double scan_ms=elapsed.nsecsElapsed()/1e6;
  QJsonArray queries;const auto all=query(index,"");QStringList needles={"test.duf","bed hogs","Genesis 8 Female","material","zz__no_result__","[",QStringLiteral("姿势")};if(!all.paths.empty()) needles.append(all.paths.back());
  for(const auto &needle:needles) {elapsed.restart();const auto result=query(index,needle);const double wall=elapsed.nsecsElapsed()/1e6;queries.append(QJsonObject{{"query",needle},{"matches",result.paths.size()},{"worker_ms",result.milliseconds},{"response_ms",wall}});require(wall<1000,"真实库搜索超过 1 秒");}
  QTemporaryDir temp;ContentBrowser browser(nullptr,temp.path()+"/ui.ini",output+"/benchmark-index");browser.resize(1100,800);browser.show();browser.set_roots(roots);
  auto *search=browser.findChild<QComboBox *>("contentSearch");auto *view=browser.findChild<QListView *>("contentItems");until([&]{return browser.property("contentIndexSize").toULongLong()==index.size();},10000);
  QJsonArray ui_queries;
  for(const auto &needle:QStringList{"Genesis 8 Female","DTPC03"}) {const auto expected=query(index,needle).paths.size();elapsed.restart();search->setEditText(needle);QTest::keyClick(search->lineEdit(),Qt::Key_Return);until([&]{return view->model()->rowCount()==expected;},10000);QApplication::processEvents();const double wall=elapsed.nsecsElapsed()/1e6;ui_queries.append(QJsonObject{{"query",needle},{"matches",expected},{"visible_results_ms",wall}});require(wall<1000,"真实界面结果展示超过 1 秒");}
  QJsonObject report{{"resources",double(index.size())},{"scan_ms",scan_ms},{"cached_startup_ms",cache_ms},{"roots",QJsonArray::fromStringList(roots)},{"queries",queries},{"ui_queries",ui_queries}};
  write(output+(cache_seen?"/search-benchmark-warm.json":"/search-benchmark.json"),QJsonDocument(report).toJson());std::cout<<QJsonDocument(report).toJson().toStdString();
}

static void visual() {
  QScreen *secondary=nullptr;for(auto *screen:QGuiApplication::screens()) if(screen!=QGuiApplication::primaryScreen()) {secondary=screen;break;}require(secondary,"缺少第二屏");
  const QString output="artifacts/content-browser";QDir().mkpath(output);QTemporaryDir temp;
  ContentBrowser browser(nullptr,temp.path()+"/visual.ini",output+"/benchmark-index");browser.setAttribute(Qt::WA_ShowWithoutActivating);browser.setWindowTitle(QStringLiteral("内容浏览器 · 副屏验证"));browser.resize(1120,850);browser.move(secondary->availableGeometry().topLeft()+QPoint(30,30));browser.set_roots({"H:/G1","H:/G3"});browser.show();
  auto *tree=browser.findChild<QTreeView *>("contentTree");auto *model=static_cast<QFileSystemModel *>(tree->model());tree->setCurrentIndex(model->index("H:/G1/Scenes"));
  auto *view=browser.findChild<QListView *>("contentItems");until([&]{return view->model()->rowCount()>10;},30000);QTest::qWait(1200);browser.grab().save(output+"/icons.png");
  auto *search=browser.findChild<QComboBox *>("contentSearch");search->setEditText("DTPC03");QTest::keyClick(search->lineEdit(),Qt::Key_Return);until([&]{return view->model()->rowCount()>0&&view->model()->index(0,0).data(Qt::UserRole).toString().contains("DTPC03",Qt::CaseInsensitive);},240000);QTest::qWait(1200);browser.grab().save(output+"/search.png");
  const auto point=view->visualRect(view->model()->index(0,0)).center();QHelpEvent hover(QEvent::ToolTip,point,view->viewport()->mapToGlobal(point));QApplication::sendEvent(view->viewport(),&hover);auto *preview=browser.findChild<QFrame *>("contentPreview");until([&]{return !browser.findChild<QLabel *>("contentPreviewImage")->pixmap().isNull();});require(preview->width()>100,"副屏悬停预览尺寸异常");preview->grab().save(output+"/tooltip.png");
  search->setEditText({});browser.record_use("H:/G1/Scenes/test.duf","scene");browser.record_use("H:/G1/Scenes/3.duf","scene");browser.show_recent();QTest::qWait(500);browser.grab().save(output+"/recent.png");
  std::cout<<"Secondary display browser / real thumbnails / real search: PASS\n";
}

static void locate_real() {
  QScreen *secondary=nullptr;for(auto *screen:QGuiApplication::screens())if(screen!=QGuiApplication::primaryScreen()){secondary=screen;break;}require(secondary,"缺少第二屏");
  const QString output="artifacts/content-interaction/locate",scene="H:/G1/Scenes/test9.duf",expected="H:/G1/People/Genesis 8 Female/Clothing/dForce H&C Long skirt outfit/LSO Heels.duf";
  QDir().mkpath(output);QTemporaryDir temp;ContentBrowser browser(nullptr,temp.path()+"/ui.ini","artifacts/content-browser/benchmark-index");
  browser.setAttribute(Qt::WA_ShowWithoutActivating);browser.resize(1180,850);browser.move(secondary->availableGeometry().topLeft()+QPoint(20,20));browser.set_roots({"H:/G1","H:/G3"});browser.show();
  auto *view=browser.findChild<QListView *>("contentItems");auto *tree=browser.findChild<QTreeView *>("contentTree");auto *files=qobject_cast<QFileSystemModel *>(tree->model());auto *search=browser.findChild<QComboBox *>("contentSearch");
  search->setEditText("existing search text");QJsonArray checks;
  const auto doc=dfv::daz::document_view(std::filesystem::path(scene.toStdWString()));
  for(const auto &node:dfv::daz::array_member(dfv::daz::object_member(*doc,"scene"),"nodes")){
    const auto label=QString::fromStdString(node.value("label",std::string{}));const bool hair=label=="FE Low Ponytail Base Hair";if(!label.startsWith("LSO Heels")&&!hair)continue;const auto expected_entry=hair?QString("H:/G1/People/Genesis 8 Female/Hair/FeSoul/FE Low Ponytail Base Hair/!FE Low Ponytail Base Hair.duf"):expected;
    ContentOrigin origin{scene,QString::fromStdString(node.at("id").get<std::string>()),label,{}};
    for(const auto &g:dfv::daz::array_member(node,"geometries")){const auto url=QString::fromStdString(dfv::daz::decode_uri(g.at("url").get<std::string>()));const auto hash=url.indexOf('#');origin.geometries.emplace_back((QFileInfo("H:/G1"+url.left(hash)).isFile()?"H:/G1":"H:/G3")+url.left(hash),url.mid(hash+1));}
    until([&]{return browser.property("contentIndexSize").toULongLong()>10000;},120000);QElapsedTimer timer;timer.start();browser.locate_asset(origin);until([&]{return browser.property("contentLocateState").toString()!="resolving";},120000);
    require(browser.property("contentLocatedPath").toString().compare(expected_entry,Qt::CaseInsensitive)==0,"test9 鞋子没有定位到正确的加载入口");
    until([&]{return view->currentIndex().data(Qt::UserRole).toString().compare(expected_entry,Qt::CaseInsensitive)==0&&view->viewport()->rect().contains(view->visualRect(view->currentIndex()).center())&&files->filePath(tree->currentIndex()).compare(QFileInfo(expected_entry).path(),Qt::CaseInsensitive)==0&&tree_label_visible(tree);});
    require(search->currentText()=="existing search text","真实资源定位丢失搜索文本");QTest::qWait(300);browser.grab().save(output+"/"+origin.node+".png");checks.append(QJsonObject{{"elapsed_ms",double(timer.elapsed())},{"node",origin.node},{"label",label},{"located",browser.property("contentLocatedPath").toString()},{"tree_and_icon_visible",true},{"search_preserved",true}});
  }
  require(checks.size()==3,"test9 中两个鞋子和头发未全部检查");write(output+"/test9.json",QJsonDocument(QJsonObject{{"result","PASS"},{"checks",checks}}).toJson());
  std::cout<<"test9 LSO Heels / exact DUF / both panes visible / query preserved: PASS\n";
}
int main(int argc,char **argv) {
  QApplication app(argc,argv);app.setApplicationName("DazFastViewerContentTest");app.setOrganizationName("DazFastViewerTests");app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"),9));
  try {const auto args=app.arguments();if(args.contains("--benchmark")) benchmark(args.mid(args.indexOf("--benchmark")+1));else if(args.contains("--visual")) visual();else if(args.contains("--locate-real"))locate_real();else {source_checks();checks();navigation_checks();}return 0;}
  catch(const std::exception &e) {std::cerr<<e.what()<<std::endl;return 1;}
}
