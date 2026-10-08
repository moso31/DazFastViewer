#include "editor/chrome.h"
#include "editor/dialog_layout.h"
#include "editor/ui_scale.h"
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTabBar>
#include <iostream>
#include <windows.h>

using namespace dfv::editor;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
static QJsonArray rect(QRect r){return {r.x(),r.y(),r.width(),r.height()};}
static bool same(const QJsonValue &a,const QJsonValue &b) {
  if(a.type()!=b.type())return false;
  if(a.isDouble())return std::abs(a.toDouble()-b.toDouble())<=2;
  if(a.isArray()){const auto x=a.toArray(),y=b.toArray();if(x.size()!=y.size())return false;for(qsizetype i=0;i<x.size();++i)if(!same(x[i],y[i]))return false;return true;}
  if(a.isObject()){const auto x=a.toObject(),y=b.toObject();if(x.keys()!=y.keys())return false;for(auto i=x.begin();i!=x.end();++i)if(!same(i.value(),y[i.key()]))return false;return true;}
  return a==b;
}
static QJsonObject layout(EditorWindow &window) {
  QJsonObject result{{"window",rect(window.geometry())},{"maximized",window.isMaximized()}};
  for(auto *dock:window.findChildren<QDockWidget *>())result[dock->objectName()]=QJsonObject{
    {"area",int(window.dockWidgetArea(dock))},{"floating",dock->isFloating()},{"hidden",dock->isHidden()},
    {"size",dock->isHidden()?QJsonArray{}:QJsonArray{dock->width(),dock->height()}},{"position",dock->isFloating()?rect(dock->geometry()):QJsonArray{}}};
  for(auto *bar:window.chrome->findChildren<QToolBar *>())result[bar->objectName()]=QJsonObject{{"rect",rect(bar->geometry())},{"hidden",bar->isHidden()},{"floating",bar->isFloating()}};
  QJsonArray tabs;for(auto *tab:window.findChildren<QTabBar *>())if(tab->isVisible())tabs.push_back(tab->tabText(tab->currentIndex()));result["activeTabs"]=tabs;
  return result;
}
static void phase(const QString &file,bool write,bool maximized,int percent) {
  QSettings settings(file,QSettings::IniFormat);DialogLayouts dialogs(file);
  EditorWindow window;window.setAttribute(Qt::WA_ShowWithoutActivating);window.resize(1450,900);
  window.move(QGuiApplication::primaryScreen()->availableGeometry().topLeft()+QPoint(60,70));
  window.setDockOptions(QMainWindow::AllowNestedDocks|QMainWindow::AllowTabbedDocks);
  auto dock=[&](const char *name,Qt::DockWidgetArea area){auto *v=new QDockWidget(name,&window);v->setObjectName(name);v->setWidget(new QLabel(name));window.addDockWidget(area,v);return v;};
  auto *left=dock("Hierarchy",Qt::LeftDockWidgetArea),*viewport=dock("Viewport",Qt::RightDockWidgetArea),*right=dock("Properties",Qt::RightDockWidgetArea);
  window.splitDockWidget(viewport,right,Qt::Horizontal);
  auto *browser=dock("Browser",Qt::LeftDockWidgetArea);window.tabifyDockWidget(left,browser);left->raise();
  auto *floating=dock("Floating",Qt::BottomDockWidgetArea),*hidden=dock("Hidden",Qt::BottomDockWidgetArea);
  window.install_chrome();for(const auto &name:{"文件","项目","编辑","穿戴与附件","创建","视图"})window.chrome->menus()->addMenu(QString::fromUtf8(name));
  UiScale scale(false);scale.set_percent(percent);
  if(write) {
    window.showNormal();if(maximized)window.showMaximized();QTest::qWait(100);
    window.resizeDocks({left,viewport,right},{310,750,390},Qt::Horizontal);
    hidden->hide();floating->setFloating(true);floating->resize(420,280);floating->move(window.screen()->availableGeometry().topLeft()+QPoint(100,130));
    browser->raise();if(maximized)floating->showMaximized();
    auto *modules=window.chrome->findChild<QMainWindow *>("ToolbarModules");auto *history=window.chrome->findChild<QToolBar *>("HistoryModule");
    modules->removeToolBar(history);modules->addToolBar(history);history->show();modules->insertToolBarBreak(history);QTest::qWait(100);
  }else {window.restore_layout(settings);QTest::qWait(200);}
  if(QGuiApplication::platformName()=="windows")check(bool(IsZoomed(HWND(window.winId())))==maximized,"Qt 与 Windows 的主窗口最大化状态不一致");
  const auto actual=QJsonDocument(layout(window)).toJson(QJsonDocument::Compact);
  if(write)settings.setValue("expected",actual);
  else if(!same(QJsonDocument::fromJson(actual).object(),QJsonDocument::fromJson(settings.value("expected").toByteArray()).object())) {
    std::cerr<<"expected="<<settings.value("expected").toByteArray().constData()<<"\nactual="<<actual.constData()<<'\n';throw std::runtime_error("跨进程重启改变窗口、停靠尺寸或工具栏布局");
  }
  {
    QDialog dialog(&window);dialog.setObjectName("PersistedDialog");dialog.setWindowFlag(Qt::WindowMaximizeButtonHint);dialog.resize(write?540:200,write?380:150);
    dialog.move(window.screen()->availableGeometry().topLeft()+QPoint(100,150));dialog.show();QTest::qWait(50);
    if(write&&maximized){dialog.showMaximized();QTest::qWait(50);}
    check(dialog.isMaximized()==maximized,"对话框跨进程最大化状态丢失");
    if(QGuiApplication::platformName()=="windows")check(bool(IsZoomed(HWND(dialog.winId())))==maximized,"对话框 Qt / Windows 状态不一致");
    const auto bounds=QJsonDocument(QJsonObject{{"bounds",rect(dialog.geometry())}}).toJson();
    if(write)settings.setValue("expectedDialog",bounds);else check(bounds==settings.value("expectedDialog").toByteArray(),"对话框跨进程位置尺寸变化");dialog.reject();
  }
  window.save_layout(settings);settings.sync();window.close();
}
int main(int argc,char **argv) {
  // 此回归必须检查真实 HWND；工程统一的 offscreen 测试环境不能复现它。
  qputenv("QT_QPA_PLATFORM","windows");
  QApplication app(argc,argv);app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"),9));
  try {
    const auto args=app.arguments();
    if(args.size()==5){phase(args[1],args[2]=="write",args[3]=="max",args[4].toInt());return 0;}
    QTemporaryDir temp;
    for(int percent:{100,150,200})for(const auto &state:{"normal","max"})for(const auto &mode:{"write","read","read"}) {
      QProcess child;child.start(app.applicationFilePath(),{temp.filePath(QString::number(percent)+state+".ini"),mode,state,QString::number(percent)});
      check(child.waitForFinished(30000),"窗口重启测试超时");
      if(child.exitStatus()!=QProcess::NormalExit||child.exitCode()!=0){std::cerr<<child.readAllStandardError().constData();throw std::runtime_error("窗口跨进程恢复失败");}
    }
    std::cout<<"100% / 150% / 200% 跨进程重启：窗口、最大化、停靠尺寸、浮动窗、顶部模块和对话框 PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
