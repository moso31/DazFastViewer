#include "editor/ui_scale.h"
#include "editor/chrome.h"
#include "editor/content_browser.h"
#include "editor/viewport_settings.h"
#include <QSpinBox>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QSettings>
#include <QScreen>
#include <QStyleOptionDockWidget>
#include <QStyleOptionMenuItem>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>
#include <iostream>
using namespace dfv::editor;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
class TitleProbe final:public QDockWidget {
public:
  explicit TitleProbe(QWidget *parent):QDockWidget("MMMM",parent){setObjectName("ScaleTitleProbe");setFeatures(QDockWidget::NoDockWidgetFeatures);setWidget(new QLabel(QStringLiteral("面板内容")));}
  int title_ink_height() {
    QStyleOptionDockWidget option;initStyleOption(&option);const auto image=grab(option.rect).toImage().convertToFormat(QImage::Format_RGB32);
    int top=image.height(),bottom=-1;const int margin=qRound(3*devicePixelRatioF());
    // 跳过边框，测量实际绘制的文字，避免只验证 QWidget::font() 而漏掉标题的缓存字体。
    for(int y=margin;y<image.height()-margin;++y)for(int x=margin;x<image.width()-margin;++x)if(qGray(image.pixel(x,y))<160){top=std::min(top,y);bottom=std::max(bottom,y);}
    return bottom>=top?bottom-top+1:0;
  }
};
static void check_menu(EditorWindow &window) {
  auto *menu=window.chrome->menus();auto *tools=window.findChild<QToolBar *>("TransformModule");
  const auto image=menu->grab().toImage().convertToFormat(QImage::Format_RGB32);const double ratio=menu->devicePixelRatioF();
  for(auto *action:menu->actions()) {
    const auto rect=menu->actionGeometry(action);
    check(menu->rect().contains(rect),"顶部菜单项被压缩或裁切");
    const int padding=rect.width()-menu->fontMetrics().horizontalAdvance(action->text());
    check(padding>=ui_pixels(10)&&padding<=ui_pixels(14),"菜单项的文字间距不均或被额外拉宽");
  }
  const auto first=menu->actionGeometry(menu->actions().front());
  QRect ink;
  for(int y=qRound((first.top()+1)*ratio);y<qRound(first.bottom()*ratio);++y)
    for(int x=qRound((first.left()+2)*ratio);x<qRound((first.right()-2)*ratio);++x)
      if(qGray(image.pixel(x,y))<140)ink=ink.united(QRect(x,y,1,1));
  const int icon=qRound(tools->iconSize().height()*ratio);
  check(ink.height()>=icon*.75&&ink.height()<=icon*1.2,"菜单实际字高与工具图标不协调");
  const auto text_center=menu->mapTo(window.chrome,QPoint(first.center().x(),qRound(ink.center().y()/ratio)));
  const auto tool_center=tools->widgetForAction(window.findChild<QAction *>("GizmoTool0"));
  check(std::abs(text_center.y()-tool_center->mapTo(window.chrome,tool_center->rect().center()).y())<=ui_pixels(3),"菜单文字没有与工具图标垂直对齐");
  const auto background=image.pixelColor(0,0);
  for(int y=image.height()-std::max(2,qRound(2*ratio));y<image.height();++y) {
    int run=0;
    for(int x=0;x<image.width();++x){const auto pixel=image.pixelColor(x,y);const auto delta=std::max({std::abs(pixel.red()-background.red()),std::abs(pixel.green()-background.green()),std::abs(pixel.blue()-background.blue())});run=delta>4?run+1:0;check(run<image.width()/2,"菜单下方仍有多余横线");}
  }
  std::cout<<"菜单 "<<qRound(ui_scale()*100)<<"%：字高="<<ink.height()<<" 图标="<<icon<<" 高度="<<menu->height()<<" 宽度="<<menu->width()<<'\n';
}
int main(int argc,char **argv){
  QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"),9));
  try {
    QTemporaryDir temp;const auto file=temp.filePath("ui.ini");EditorWindow window;window.resize(1300,700);window.install_chrome();
    if(QGuiApplication::platformName()==QStringLiteral("windows")){window.setAttribute(Qt::WA_ShowWithoutActivating);const auto screens=app.screens();window.move(screens.at(screens.size()>1?1:0)->availableGeometry().topLeft()+QPoint(20,20));}
    window.chrome->menus()->addMenu(QStringLiteral("文件"));auto *settings=window.chrome->menus()->addMenu(QStringLiteral("项目"));for(const auto &name:{"编辑","穿戴与附件","创建","视图"})window.chrome->menus()->addMenu(QString::fromUtf8(name));ContentBrowser browser(&window,temp.filePath("content.ini"),temp.filePath("cache"));
    browser.hide();
    auto *dock=new TitleProbe(&window);window.addDockWidget(Qt::LeftDockWidgetArea,dock);
    auto *tabbed=new QDockWidget(QStringLiteral("停靠标签"),&window);tabbed->setObjectName("ScaleTabbedDock");tabbed->setWidget(new QLabel(QStringLiteral("标签内容")));window.addDockWidget(Qt::RightDockWidgetArea,tabbed);
    auto *other=new QDockWidget(QStringLiteral("另一个面板"),&window);other->setObjectName("ScaleOtherDock");other->setWidget(new QLabel(QStringLiteral("另一个内容")));window.addDockWidget(Qt::RightDockWidgetArea,other);window.tabifyDockWidget(tabbed,other);
    UiScale scale(true,file);scale.add_menu(settings);window.show();QTest::qWait(50);
    auto screenshot=[&](const char *name){if(argc>1){QDir().mkpath(QString::fromLocal8Bit(argv[1]));window.grab().save(QString::fromLocal8Bit(argv[1])+"/"+name);window.chrome->grab().save(QString::fromLocal8Bit(argv[1])+"/chrome-"+name);}};
    const int title100=dock->title_ink_height();check(title100>0,"停靠标题没有实际绘出文字");screenshot("scale-100.png");check_menu(window);
    auto *menu_bar=window.chrome->menus();
    QTest::mouseClick(menu_bar,Qt::LeftButton,{},menu_bar->actionGeometry(settings->menuAction()).center());QTest::qWait(20);
    check(settings->isVisible()&&QApplication::activePopupWidget()==settings,"菜单按钮点击未打开对应弹出菜单");
    QTest::keyClick(settings,Qt::Key_Escape);check(!settings->isVisible(),"菜单 Esc 关闭失效");
    scale.set_percent(200);QTest::qWait(40);const int title200=dock->title_ink_height();screenshot("scale-200.png");
    check_menu(window);
    std::cout<<"停靠标题实际字高：100%="<<title100<<"，200%="<<title200<<'\n';
    check(title200>=title100*1.7,"停靠标题实际绘制字号没有跟随界面缩放");
    check(dock->widget()->font().pointSizeF()==app.font().pointSizeF(),"停靠面板内容没有同步缩放");
    const auto tabs=window.findChildren<QTabBar *>();check(!tabs.isEmpty(),"没有创建停靠标签栏");for(auto *tab:tabs)check(tab->font().pointSizeF()==app.font().pointSizeF(),"停靠标签字号没有同步缩放");
    scale.set_percent(100);QTest::qWait(40);check(dock->title_ink_height()==title100,"停靠标题恢复 100% 时字号漂移");
    auto *ui_percent=window.findChild<QSpinBox *>("UiScalePercent");check(ui_percent&&ui_percent->value()==100,"设置菜单缺少界面缩放参数");
    auto *button=window.chrome->findChild<QToolButton *>("WindowControl2");const auto base=button->size();const auto font=app.font().pointSizeF();const auto metrics=app.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    ui_percent->setValue(150);QTest::qWait(40);check(button->size()==QSize(qRound(base.width()*1.5),qRound(base.height()*1.5)),"固定按钮没有随全局缩放");
    check_menu(window);screenshot("scale-150.png");
    check(app.font().pointSizeF()==font*1.5&&app.style()->pixelMetric(QStyle::PM_ScrollBarExtent)>metrics,"字体或原生控件没有缩放");
    check(browser.findChild<QListView *>("contentItems")->iconSize().width()==168,"内容图标没有跟随全局缩放");
    check(window.chrome->height()>=51&&window.chrome->height()<100,"顶部栏缩放裁切或高度异常");
    {auto *late=new TitleProbe(&window);late->setObjectName("ScaleLateDock");window.addDockWidget(Qt::BottomDockWidgetArea,late);QTest::qWait(30);scale.set_percent(100);QTest::qWait(30);check(late->title_ink_height()==title100,"缩放期间创建的停靠窗口恢复字号失败");delete late;scale.set_percent(150);QTest::qWait(30);}
    {QDialog dialog(&window);auto *layout=new QVBoxLayout(&dialog);auto *label=new QLabel(QStringLiteral("动态对话框"));label->setMinimumWidth(100);layout->addWidget(label);dialog.show();QTest::qWait(30);check(label->minimumWidth()==150,"后来创建的控件没有继承缩放");scale.set_percent(100);QTest::qWait(20);check(label->minimumWidth()==100,"动态控件恢复基准失败");}
    for(int i=0;i<5;++i){scale.set_percent(170);scale.set_percent(80);scale.set_percent(100);}QTest::qWait(30);check(button->size()==base&&app.font().pointSizeF()==font,"反复缩放存在尺寸漂移");
    ui_percent->setFocus();QTest::keyClick(ui_percent,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(ui_percent,"125");QTest::keyClick(ui_percent,Qt::Key_Return);check(scale.percent()==125,"界面缩放参数无法直接输入");
    ui_percent->stepUp();check(scale.percent()==135,"界面缩放参数步进未生效");
    scale.set_percent(500);check(scale.percent()==200,"缩放上界失效");scale.set_percent(0);check(scale.percent()==50,"缩放下界失效");
    scale.set_percent(130);check(QSettings(file,QSettings::IniFormat).value("ui/scalePercent").toInt()==130,"缩放偏好未保存");
    window.findChild<QAction *>("UiZoomReset")->trigger();check(scale.percent()==100,"恢复默认未生效");
    {
      ViewportSettings quality(true,file);quality.add_menu(settings);int changes=0;quality.changed=[&](dfv::ViewportQuality){++changes;};
      auto *percent=window.findChild<QSpinBox *>("ViewportRenderPercent");auto *filter=window.findChild<QComboBox *>("ViewportReconstruction");
      check(percent&&filter&&percent->value()==100,"缺少视口分辨率入口");percent->setValue(67);filter->setCurrentIndex(1);
      check(quality.value().percent==67&&quality.value().reconstruction==dfv::Reconstruction::bilinear&&changes==2,"自定义比例或重建算法未提交");
      ui_percent->setValue(140);check(quality.value().percent==67&&changes==2,"界面缩放改变了独立的渲染倍率");
      for(auto key:{Qt::Key_Plus,Qt::Key_Equal,Qt::Key_Minus,Qt::Key_0})QTest::keyClick(&window,key,Qt::ControlModifier);
      check(scale.percent()==140&&quality.value().percent==67,"旧缩放快捷键仍在改变设置");
      for(auto *action:settings->findChildren<QAction *>())check(action->shortcuts().isEmpty(),"缩放设置不应绑定快捷键");
      ViewportSettings reopen(true,file);check(reopen.value()==quality.value(),"视口质量偏好没有恢复");
      check(dfv::render_size(1600,900,50)==std::pair{800,450}&&dfv::render_size(1600,900,75)==std::pair{1200,675},"渲染尺寸没有按宽高缩小");
      check(dfv::render_size(1600,900,50,true)==std::pair{200,112}&&dfv::render_size(1,1,50)==std::pair{1,1},"导航预览或最小尺寸错误");
      quality.set({0});check(quality.value().percent==50,"渲染比例下限失效");quality.set({200});check(quality.value().percent==100,"渲染比例上限失效");
    }
    std::cout<<"停靠标题实际字号、停靠标签与内容、设置菜单、UI 与渲染倍率独立、动态控件、图标、顶部栏和偏好保存：PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
