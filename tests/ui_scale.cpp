#include "editor/ui_scale.h"
#include "editor/chrome.h"
#include "editor/content_browser.h"
#include "editor/viewport_settings.h"
#include <QSpinBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>
#include <iostream>
using namespace dfv::editor;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv){
  QApplication app(argc,argv);app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"),9));
  try {
    QTemporaryDir temp;const auto file=temp.filePath("ui.ini");EditorWindow window;window.resize(1300,700);window.install_chrome();
    auto *settings=window.chrome->menus()->addMenu(QStringLiteral("设置"));ContentBrowser browser(&window,temp.filePath("content.ini"),temp.filePath("cache"));
    UiScale scale(true,file);scale.add_menu(settings);window.show();QTest::qWait(50);
    auto *ui_percent=window.findChild<QSpinBox *>("UiScalePercent");check(ui_percent&&ui_percent->value()==100,"设置菜单缺少界面缩放参数");
    auto *button=window.chrome->findChild<QToolButton *>("WindowControl2");const auto base=button->size();const auto font=app.font().pointSizeF();const auto metrics=app.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    ui_percent->setValue(150);QTest::qWait(40);check(button->size()==QSize(qRound(base.width()*1.5),qRound(base.height()*1.5)),"固定按钮没有随全局缩放");
    check(app.font().pointSizeF()==font*1.5&&app.style()->pixelMetric(QStyle::PM_ScrollBarExtent)>metrics,"字体或原生控件没有缩放");
    check(browser.findChild<QListView *>("contentItems")->iconSize().width()==168,"内容图标没有跟随全局缩放");
    check(window.chrome->height()>=51&&window.chrome->height()<100,"顶部栏缩放裁切或高度异常");
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
    std::cout<<"设置菜单、UI 与渲染倍率独立、无缩放快捷键、动态控件、图标、顶部栏和偏好保存：PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
