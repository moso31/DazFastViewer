#include "editor/application_settings.h"
#include <QSettings>
#include <memory>
#include <stdexcept>

namespace dfv::editor {
static std::unique_ptr<QSettings> storage(const QString &file) {
  return file.isEmpty()?std::make_unique<QSettings>():std::make_unique<QSettings>(file,QSettings::IniFormat);
}
ApplicationSettings ApplicationSettings::load(const QString &file) {
  auto s=storage(file);ApplicationSettings value;
  // 沿用原菜单的键，已有用户的界面与视口偏好不需要重新设置。
  value.ui_percent=std::clamp(s->value("ui/scalePercent",100).toInt(),50,200);
  value.viewport.percent=std::clamp(s->value("viewport/renderPercent",100).toInt(),50,100);
  value.viewport.reconstruction=s->value("viewport/reconstruction","bicubic").toString()=="bilinear"?Reconstruction::bilinear:Reconstruction::bicubic;
  const int limit=s->value("render/textureLimit",0).toInt();
  for(int supported:{0,512,1024,2048,4096}) if(limit==supported) value.render.texture_limit=limit;
  value.render.transparent_bounces=std::clamp(s->value("render/transparentBounces",32).toInt(),1,32);
  value.render.subsurface=s->value("render/subsurface",true).toBool();
  value.render.bump_and_normal=s->value("render/bumpAndNormal",true).toBool();
  value.settings_tab=std::clamp(s->value("projectDialog/tab",0).toInt(),0,1);
  for(const auto *key:{"libraries","saveFile","display","textures","materials"}) value.expanded[key]=s->value(QStringLiteral("projectDialog/expanded/")+key,true).toBool();
  return value;
}
void ApplicationSettings::save(const QString &file) const {
  if(ui_percent<50||ui_percent>200||viewport.percent<50||viewport.percent>100||
     (render.texture_limit!=0&&render.texture_limit!=512&&render.texture_limit!=1024&&render.texture_limit!=2048&&render.texture_limit!=4096)||
     render.transparent_bounces<1||render.transparent_bounces>32) throw std::runtime_error("应用设置超出可用范围");
  auto s=storage(file);
  s->setValue("ui/scalePercent",ui_percent);s->setValue("viewport/renderPercent",viewport.percent);
  s->setValue("viewport/reconstruction",viewport.reconstruction==Reconstruction::bilinear?"bilinear":"bicubic");
  s->setValue("render/textureLimit",render.texture_limit);s->setValue("render/transparentBounces",render.transparent_bounces);
  s->setValue("render/subsurface",render.subsurface);s->setValue("render/bumpAndNormal",render.bump_and_normal);
  s->setValue("projectDialog/tab",settings_tab);
  for(auto i=expanded.cbegin();i!=expanded.cend();++i) s->setValue("projectDialog/expanded/"+i.key(),i.value());
  s->sync();if(s->status()!=QSettings::NoError) throw std::runtime_error("无法保存应用设置，请检查设置位置是否可写");
}
}
