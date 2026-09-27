#include "editor/project.h"
#include "render_ir/material_quality.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QToolButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QSettings>
#include <QFontDatabase>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#include <functional>
static void require(bool ok,const char *why) {if(!ok) throw std::runtime_error(why);}
int main(int argc,char **argv) {
  QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");app.setFont(QFont(QStringLiteral("Microsoft YaHei"),9));
  try {
    QTemporaryDir temp;require(temp.isValid(),"临时目录失败");
    const auto a=temp.path()+QStringLiteral("/中文内容库"),b=temp.path()+"/second",file=temp.path()+QStringLiteral("/项目.json");
    dfv::editor::ProjectSettings settings{file,{a,b,a.toUpper(),QDir::toNativeSeparators(b)}};settings.save();
    auto loaded=dfv::editor::ProjectSettings::load(file);require(loaded.content_roots==QStringList{a,b},"根目录顺序 / Unicode / 去重失效");
    loaded.content_roots={b,a};loaded.save();require(dfv::editor::ProjectSettings::load(file).content_roots==QStringList{b,a},"重新打开丢失优先级");
    bool rejected=false;try {loaded.content_roots={"relative/path"};loaded.save();} catch(...) {rejected=true;}
    require(rejected && dfv::editor::ProjectSettings::load(file).content_roots==QStringList{b,a},"无效写入损坏已有配置");
    loaded.content_roots={b,a};
    require(loaded.history_limit==50,"旧项目默认历史条数不是 50");loaded.history_limit=80;loaded.save();require(dfv::editor::ProjectSettings::load(file).history_limit==80,"历史上限没有保存");
    loaded.history_limit=0;rejected=false;try{loaded.save();}catch(...){rejected=true;}require(rejected&&dfv::editor::ProjectSettings::load(file).history_limit==80,"无效历史上限损坏配置");loaded.history_limit=80;
    const auto preferences_file=temp.filePath("application.ini");
    {QSettings legacy(preferences_file,QSettings::IniFormat);legacy.setValue("ui/scalePercent",125);legacy.setValue("viewport/renderPercent",67);legacy.setValue("viewport/reconstruction","bilinear");}
    auto preferences=dfv::editor::ApplicationSettings::load(preferences_file);
    require(preferences.ui_percent==125&&preferences.viewport.percent==67&&preferences.viewport.reconstruction==dfv::Reconstruction::bilinear,"旧菜单偏好没有继承");
    require(preferences.render==dfv::RenderQuality{},"默认画质必须保持无限制纹理和原材质");
    require(preferences.viewport.sharpen==0,"旧偏好应默认关闭锐化");
    {QSettings legacy(preferences_file,QSettings::IniFormat);legacy.setValue("viewport/sharpenPercent",75);}
    require(dfv::editor::ApplicationSettings::load(preferences_file).viewport.sharpen==.75f,"旧百分数锐化未换算为小数");
    int apply_count=0,save_count=0;dfv::editor::ApplicationSettings active_preferences=preferences;QStringList active_roots=loaded.content_roots;
    auto dialog_test=[&](auto action) {
      std::exception_ptr failure;
      QTimer::singleShot(30,[&]{auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());try{require(dialog,"项目设置没有创建对话框");action(dialog);}catch(...){failure=std::current_exception();if(dialog)dialog->reject();}});
      const bool accepted=dfv::editor::edit_project_settings(nullptr,loaded,preferences,preferences_file,true,[&](bool saved){++apply_count;if(saved)++save_count;active_preferences=preferences;active_roots=loaded.content_roots;});
      if(failure)std::rethrow_exception(failure);return accepted;
    };
    auto buttons=[](QDialog *d){return d->findChild<QDialogButtonBox *>("ProjectSettingsButtons");};
    require(dialog_test([&](QDialog *d){
      auto *tabs=d->findChild<QTabWidget *>("ProjectSettingsTabs");require(tabs&&tabs->count()==2&&tabs->tabText(1)==QStringLiteral("渲染"),"项目设置未按功能拆成选项卡");
      auto *history=d->findChild<QSpinBox *>("HistoryLimit");require(history&&history->value()==80,"资源与保存缺少历史上限");history->setValue(60);
      auto *libraries=d->findChild<QToolButton *>("librariesCollapse"),*save_file=d->findChild<QToolButton *>("saveFileCollapse");
      require(libraries&&save_file&&libraries->isChecked()&&save_file->isChecked(),"资源页缺少两个折叠模块");
      if(argc>1){QDir().mkpath(QString::fromLocal8Bit(argv[1]));d->grab().save(QString::fromLocal8Bit(argv[1])+"/resources.png");}
      libraries->click();require(d->findChild<QWidget *>("librariesSection")->isHidden(),"资源路径模块没有折叠");tabs->setCurrentIndex(1);
      auto *limit=d->findChild<QComboBox *>("RenderTextureLimit");require(limit&&limit->count()==5&&limit->currentData().toInt()==0,"纹理档位或默认值错误");
      for(int value:{512,1024,2048,4096,0}) require(limit->findData(value)>=0,"纹理档位缺失");
      limit->setCurrentIndex(limit->findData(2048));d->findChild<QSpinBox *>("UiScalePercent")->setValue(140);
      d->findChild<QSpinBox *>("ViewportRenderPercent")->setValue(75);d->findChild<QComboBox *>("ViewportReconstruction")->setCurrentIndex(0);
      auto *sharpen=d->findChild<QSlider *>("ViewportSharpenStrength");require(sharpen&&d->findChild<QWidget *>("displaySection")->isAncestorOf(sharpen),"锐化没有放在界面与视口模块");sharpen->setValue(350);
      auto *sharpen_value=d->findChild<QLabel *>("ViewportSharpenValue");require(sharpen->orientation()==Qt::Horizontal&&sharpen->maximum()==2000&&sharpen_value&&sharpen_value->text()=="3.50","锐化滑块范围或小数显示错误");
      sharpen->triggerAction(QAbstractSlider::SliderSingleStepAdd);require(sharpen_value->text()=="3.51","滑块微调没有更新小数显示");sharpen->setValue(2000);require(sharpen_value->text()=="20.00","滑块上限未显示 20.00");sharpen->setValue(350);
      d->findChild<QCheckBox *>("RenderSubsurface")->setChecked(false);d->findChild<QCheckBox *>("RenderBumpNormal")->setChecked(false);d->findChild<QSpinBox *>("RenderTransparentBounces")->setValue(8);
      if(argc>1)d->grab().save(QString::fromLocal8Bit(argv[1])+"/render.png");
      buttons(d)->button(QDialogButtonBox::Save)->click();
    }),"保存项目选项卡失败");
    require(dfv::editor::ApplicationSettings::load(preferences_file)==preferences,"重开丢失画质、界面、选项卡或折叠状态");
    require(preferences.viewport.sharpen==3.5f,"锐化强度没有保存");
    require(preferences.render.texture_limit==2048&&!preferences.render.subsurface&&!preferences.render.bump_and_normal&&preferences.render.transparent_bounces==8,"渲染参数没有保存");
    const auto before=preferences;const auto before_roots=loaded.content_roots;
    require(!dialog_test([&](QDialog *d){
      require(d->findChild<QTabWidget *>("ProjectSettingsTabs")->currentIndex()==1&&!d->findChild<QToolButton *>("librariesCollapse")->isChecked(),"对话框没有恢复页签与折叠状态");
      require(d->findChild<QComboBox *>("RenderTextureLimit")->currentData().toInt()==2048,"对话框纹理值没有恢复");
      require(d->findChild<QSlider *>("ViewportSharpenStrength")->value()==350,"对话框锐化值没有恢复");d->findChild<QSlider *>("ViewportSharpenStrength")->setValue(900);
      d->findChild<QComboBox *>("RenderTextureLimit")->setCurrentIndex(0);d->findChild<QListWidget *>("ProjectContentRoots")->clear();buttons(d)->button(QDialogButtonBox::Cancel)->click();
    }),"取消错误地保存了设置");
    require(preferences==before&&loaded.content_roots==before_roots&&dfv::editor::ApplicationSettings::load(preferences_file)==before,"取消污染当前或持久设置");
    require(apply_count==1&&save_count==1,"保存没有通知主窗口，或取消错误地应用设置");
    require(!dialog_test([&](QDialog *d){
      auto *box=buttons(d);require(box->button(QDialogButtonBox::Apply)&&box->button(QDialogButtonBox::Apply)->text()==QStringLiteral("应用")&&box->button(QDialogButtonBox::Save)->text()==QStringLiteral("保存"),"应用和保存按钮没有拆分");
      d->findChild<QComboBox *>("RenderTextureLimit")->setCurrentIndex(0);d->findChild<QSpinBox *>("UiScalePercent")->setValue(160);
      d->findChild<QSlider *>("ViewportSharpenStrength")->setValue(1625);
      auto *roots=d->findChild<QListWidget *>("ProjectContentRoots");roots->clear();roots->addItem(a);
      box->button(QDialogButtonBox::Apply)->click();require(d->isVisible()&&apply_count==2&&save_count==1,"应用没有保持窗口打开或通知主窗口");
      require(active_preferences.viewport.sharpen==16.25f,"应用没有更新视口锐化");d->findChild<QSlider *>("ViewportSharpenStrength")->setValue(0);
      require(active_preferences.render.texture_limit==512&&active_preferences.ui_percent==160&&active_roots==QStringList{a},"应用未将最新画质、界面和资源路径传给主窗口");
      require(dfv::editor::ApplicationSettings::load(preferences_file)==before&&dfv::editor::ProjectSettings::load(file).content_roots==before_roots,"应用不应写入磁盘配置");
      d->findChild<QComboBox *>("RenderTextureLimit")->setCurrentIndex(1);roots->clear();box->button(QDialogButtonBox::Cancel)->click();
    }),"应用后取消错误地执行保存");
    require(preferences==active_preferences&&loaded.content_roots==active_roots&&apply_count==2,"取消没有丢弃未应用的改动，或撤销了已应用的设置");
    require(dialog_test([&](QDialog *d){
      require(d->findChild<QComboBox *>("RenderTextureLimit")->currentData().toInt()==512&&d->findChild<QListWidget *>("ProjectContentRoots")->count()==1,"重新打开没有保留当前已应用设置");
      buttons(d)->button(QDialogButtonBox::Save)->click();
    }),"先应用后保存失败");
    require(apply_count==3&&save_count==2&&dfv::editor::ApplicationSettings::load(preferences_file)==preferences&&dfv::editor::ProjectSettings::load(file).content_roots==QStringList{a},"先应用后保存没有持久化全部参数与资源路径");
    for(int limit:{512,1024,2048,4096,0}){preferences.render.texture_limit=limit;preferences.save(preferences_file);require(dfv::editor::ApplicationSettings::load(preferences_file).render.texture_limit==limit,"纹理精度重新启动后被重置");}
    for(float value:{0.f,0.35f,20.f}){preferences.viewport.sharpen=value;preferences.save(preferences_file);require(dfv::editor::ApplicationSettings::load(preferences_file).viewport.sharpen==value,"锐化边界值未持久保存");}
    preferences.viewport.sharpen=20.01f;rejected=false;try{preferences.save(preferences_file);}catch(...){rejected=true;}require(rejected&&dfv::editor::ApplicationSettings::load(preferences_file).viewport.sharpen==20,"非法锐化值覆盖了配置");
    dfv::ir::Material material;material.subsurface=.7f;material.translucency_texture=3;material.bump_texture=4;material.normal_texture=5;material.displacement_texture=6;
    require(dfv::ir::viewport_material(material,{})==material,"默认材质画质发生变化");
    const auto simplified=dfv::ir::viewport_material(material,{0,32,false,false});
    require(simplified.subsurface==0&&simplified.translucency_texture==3&&simplified.bump_texture==-1&&simplified.normal_texture==-1&&simplified.displacement_texture==6,"视口材质降级范围错误");
    require(material.subsurface==.7f&&material.bump_texture==4,"渲染质量写回了原场景材质");
    material.thin_walled=true;material.translucency=.4f;
    const auto thin=dfv::ir::viewport_material(material,{0,32,false,true});require(thin.translucency_texture==3&&thin.translucency==.4f,"关闭皮肤 SSS 破坏薄壁透光");
    QFile corrupt(file);require(corrupt.open(QIODevice::WriteOnly),"无法写入损坏用例");corrupt.write("{");corrupt.close();
    rejected=false;try {dfv::editor::ProjectSettings::load(file);} catch(...) {rejected=true;}require(rejected,"损坏配置被静默覆盖");
    std::cout<<"项目选项卡、折叠模块、应用/保存/取消、旧偏好继承、全部纹理档位、材质降级与原场景保护：PASS\n";
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
