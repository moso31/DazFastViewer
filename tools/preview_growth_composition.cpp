#include "growth_composition_model.h"
#include "cycles/runtime_paths.h"
#include "editor/renderer.h"
#include "editor/scene_extension.h"
#include "editor/object_extension.h"
#include "editor/parameter_widgets.h"
#include "editor/ground_panel.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QStyleOptionSpinBox>
#include <QPainter>
#include <QTest>
#include <QThread>
#include <fstream>
#include <iostream>

using namespace dfv;using namespace dfv::editor;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
static auto find_control(const runtime::Target &target,const std::string &label) {
  auto found=target.morphs.end();int best=-1;
  for(auto i=target.morphs.begin();i!=target.morphs.end();++i)if(i->label==label&&i->alias_morph<0){
    const bool root=i->owner.empty()||i->owner.find('/')==std::string::npos;
    const int rank=(i->scene_channel?4:0)+(root?2:0);if(rank>best){best=rank;found=i;}
  }
  return found;
}
static std::vector<std::string> apply_composition(const runtime::Target &target,runtime::Properties &value,double fat,bool legacy) {
  auto result=value;result.extension.fatness=fat;auto missing=runtime::apply_growth(target,result);
  if(legacy){
    const auto baby=find_control(target,"Genesis 8 Baby");
    const bool has_baby=baby!=target.morphs.end()&&baby->evaluable&&baby->unsupported.empty()&&!baby->locked;
    const double mix=has_baby&&result.extension.baby_blend?1-runtime::growth_baby_weight(result.extension):1;
    const double age=result.extension.curve.enabled?std::min(result.extension.age,result.extension.curve.planned_age):result.extension.age;
    for(const auto &v:runtime::growth_values(age,result.extension.strength)){
      const auto m=find_control(target,v.label);if(m==target.morphs.end()||!m->evaluable||!m->unsupported.empty()||m->locked)continue;
      double x=v.value*mix;if(m->clamped)x=std::clamp(x,double(m->minimum),double(m->maximum));
      runtime::set_parameter(target,result,size_t(m-target.morphs.begin()),float(x));
    }
  }
  value=std::move(result);return missing;
}
static void verify_model(){
  for(double age:{0.,3.,8.,12.5,18.,50.}){
    const auto old=runtime::growth_values(age,.5),mid=preview::composition_values(age,.5,.5);
    check(old.size()==mid.size(),"参数数量变化");for(size_t i=0;i<old.size();++i)check(old[i].value==mid[i].value,"中性值没有保持旧效果");
    const auto frame=preview::composition_values(age,1,.5),fat=preview::composition_values(age,.5,1);
    for(size_t i=0;i<mid.size();++i){
      check(frame[i].value==mid[i].value||fat[i].value==mid[i].value,"两个轴同时驱动同一 Morph");
      if(mid[i].label=="Weight")check(std::abs(fat[i].value-mid[i].value-.5)<1e-12,"幼年或成年胖瘦失效");
      if(mid[i].label=="Breasts Size"||mid[i].label=="Youth Morph")check(frame[i].value==mid[i].value&&fat[i].value==mid[i].value,"体型轴改动成熟度");
    }
  }
  std::cout<<"PASS neutral compatibility, independent axes, all-age fat, maturity preservation\n";
}
int main(int argc,char **argv){QApplication app(argc,argv);try{
  std::cout.setf(std::ios::unitbuf);verify_model();
  check(argc>=4,"GrowthCompositionPreview scene.duf project.json output [--capture]");
  const bool batch=argc>4&&std::string_view(argv[4])=="--capture";
  const fs::path input=fs::absolute(fs::u8path(argv[1])),output=fs::absolute(fs::u8path(argv[3]));fs::create_directories(output/"renderer");
  J project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;for(const auto &v:project.at("content_roots"))roots.push_back(fs::u8path(v.get<std::string>()));
  std::shared_ptr<Document> d;Snapshot snapshot;
  if(input.extension()==".dufex"){auto restored=load_scene_extension(input,roots,1);d=restored.document;snapshot=restored.snapshot;}
  else {d=std::make_shared<Document>();d->generation=1;d->source_file=input;std::cout<<"Loading "<<input.string()<<'\n';d->loaded=daz::load(input,{roots,false});d->catalog=daz::discover_morphs(d->loaded,roots,{},true);d->skeletons=daz::load_skeletons(d->loaded);d->formulas=daz::enable_formulas(d->catalog,d->skeletons);snapshot=initial_snapshot(*d);}
  size_t target=SIZE_MAX;for(size_t t=0;t<d->catalog.targets.size();++t)if(growth_character(*d,t)){check(target==SIZE_MAX,"预览场景需要只有一个角色本体");target=t;}check(target!=SIZE_MAX,"找不到角色本体");
  // 对照使用统一中性照明，照明设置随导出的副本保存。
  snapshot.options={};snapshot.options.environment.id="composition-preview-lighting";
  ir::Option tint;tint.id=tint.label="Environment Tint";tint.type="color";tint.value={.65,.65,.65};tint.supported=true;snapshot.options.environment.parameters.push_back(tint);
  d->loaded.scene.options=snapshot.options;
  const auto scene_bounds=d->loaded.scene.bounds();const auto center=scene_bounds.center();const float extent=std::max(.1f,scene_bounds.extent());
  snapshot.lights.clear();for(int i=0;i<2;++i){ir::AreaLight light;light.id="composition-preview-light-"+std::to_string(i);light.transform=ir::Transform::translate({center.x+(i?1.1f:-1.1f)*extent,center.y-extent,scene_bounds.maximum.z+extent});light.width=light.height=extent;const float power=(i?8.f:16.f)*extent*extent;light.power={power,power,power};snapshot.lights.push_back(light);}d->loaded.scene.lights=snapshot.lights;
  snapshot.values[target].extension.kind=runtime::ExtensionKind::growth;const auto original=snapshot;
  const auto &actor=d->catalog.targets[target];std::cout<<"Figure: "<<actor.label<<" age "<<snapshot.values[target].extension.age<<'\n';
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(app.applicationDirPath().toStdString(),cycles_user_directory());
  QWidget window;window.setWindowTitle(QStringLiteral("体格与肥胖 · 角色效果预览"));window.resize(1100,760);auto *layout=new QHBoxLayout(&window);
  auto *host=new QWidget;host->setMinimumSize(500,500);layout->addWidget(host,1);
  auto *outer=new QScrollArea;outer->setWidgetResizable(true);outer->setFixedWidth(360);auto *panel=new QWidget;outer->setWidget(panel);layout->addWidget(outer);auto *form=new QVBoxLayout(panel);
  auto *title=new QLabel(QString::fromUtf8(actor.label));form->addWidget(title);auto *description=new QLabel(QStringLiteral("预览方案：体格调整骨架宽度与手脚比例；肥胖调整软组织。\n两项均为 0.5 时保持原中档效果。"));description->setWordWrap(true);form->addWidget(description);
  auto row=[&]{auto *r=new QHBoxLayout;r->setSpacing(4);form->addLayout(r);return r;};
  auto spin=[&](QHBoxLayout *r,const char *id,const QString &label,double value,double step,double minimum=-std::numeric_limits<float>::max()){
    auto *s=parameter_widgets::number(id);s->setDecimals(9);s->setRange(minimum,std::numeric_limits<float>::max());s->setSingleStep(step);s->setValue(value);s->setKeyboardTracking(false);s->setMinimumWidth(48);s->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);s->setProperty("fixedParameterSettings",true);r->addWidget(new QLabel(label));r->addWidget(s,1);return s;};
  auto *r=row();auto *age=spin(r,"GrowthAge",QStringLiteral("年龄"),original.values[target].extension.age,1,0);auto *step=spin(r,"GrowthAgeStep",QStringLiteral("步长"),1,.25,0);
  r=row();auto *frame=spin(r,"GrowthStrength",QStringLiteral("体格"),.5,.125);auto *rate=spin(r,"GrowthSensitivity",QStringLiteral("每年增长"),original.values[target].extension.sensitivity,2.5);rate->setEnabled(false);
  r=row();auto *fat=spin(r,"GrowthFatness",QStringLiteral("肥胖"),.5,.125);auto *multiplier=spin(r,"GrowthMultiplier",QStringLiteral("倍率"),1,.05,1e-9);multiplier->setEnabled(false);
  auto *scale_hint=new QLabel(QStringLiteral("此预览固定场景 Scale，只比较 Morph。\n0.5 为中性；建议先比较 0、0.5、1。"));scale_hint->setWordWrap(true);form->addWidget(scale_hint);
  auto *legacy=new QCheckBox(QStringLiteral("对照：原来的单轴体格"));form->addWidget(legacy);
  auto *front=new QPushButton(QStringLiteral("正面")),*side=new QPushButton(QStringLiteral("侧面")),*oblique=new QPushButton(QStringLiteral("斜前方"));r=row();for(auto *b:{front,side,oblique})r->addWidget(b);
  auto *reset=new QPushButton(QStringLiteral("恢复 0.5 / 0.5"));form->addWidget(reset);auto *save=new QPushButton(QStringLiteral("保存当前效果副本…"));form->addWidget(save);
  auto *status=new QLabel;status->setWordWrap(true);form->addWidget(status);form->addStretch();parameter_widgets::decorate(panel,"composition-preview/");
  window.show();ShowWindow(reinterpret_cast<HWND>(window.winId()),SW_SHOWNORMAL);app.processEvents();
  // 真 Qt 事件验证新增行：点击门槛、原生箭头、范围、布局及外层滚动。
  auto wheel=[&](QDoubleSpinBox *s){QWheelEvent e(s->rect().center(),s->mapToGlobal(s->rect().center()),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(s,&e);};
  auto arrows=[](QDoubleSpinBox *s){QStyleOptionSpinBox o;o.initFrom(s);o.rect={0,0,200,30};o.state=QStyle::State_Enabled|QStyle::State_Active;o.frame=true;o.buttonSymbols=s->buttonSymbols();o.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;QImage im(200,30,QImage::Format_ARGB32);im.fill(Qt::transparent);QPainter p(&im);s->style()->drawComplexControl(QStyle::CC_SpinBox,&o,&p,s);p.end();return im.copy(170,0,30,30);};
  GroundPanel ground;ground.bind(true,0,0,false);const auto native=arrows(ground.findChild<QDoubleSpinBox *>("GroundAlignmentOffset"));
  wheel(fat);check(fat->value()==.5&&arrows(fat)==native,"肥胖未选中时被滚轮改值或箭头样式错误");QTest::mouseClick(fat,Qt::LeftButton);wheel(fat);check(fat->value()==.375&&arrows(fat)==native,"选中滚轮或箭头错误");wheel(frame);check(frame->value()==.5,"滚动其他参数改值");
  fat->setValue(1.5);check(fat->value()==1.5,"肥胖被旧范围截断");fat->setValue(.5);panel->hide();panel->show();wheel(fat);check(fat->value()==.5,"隐藏后保留了滚轮选择");
  check(fat->mapTo(panel,QPoint{}).y()==multiplier->mapTo(panel,QPoint{}).y()&&fat->mapTo(panel,QPoint{}).y()>frame->mapTo(panel,QPoint{}).y()&&fat->mapTo(panel,QPoint{}).x()<multiplier->mapTo(panel,QPoint{}).x(),"肥胖行布局错误");
  outer->setFixedHeight(200);app.processEvents();wheel(fat);check(outer->verticalScrollBar()->value()>0&&fat->value()==.5,"未选中滚轮没有滚动外层页面");outer->setMinimumHeight(0);outer->setMaximumHeight(QWIDGETSIZE_MAX);app.processEvents();outer->verticalScrollBar()->setValue(0);
  std::cout<<"PASS native Qt fatness layout, click-selected wheel, arrows, range and outer scroll\n";
  SamplingSettings sampling;sampling.samples=128;sampling.adaptive_threshold=0;sampling.update_interval_seconds=.05;
  Renderer renderer(reinterpret_cast<HWND>(host->winId()),host->width(),host->height(),output/"renderer",sampling);renderer.resize(host->width(),host->height());renderer.automated_pointer();renderer.set_document(d,snapshot,false);
  auto pump=[&]{app.processEvents();QThread::msleep(2);};
  auto ready=[&](int samples){const auto start=now();for(;;){pump();const auto s=renderer.status();if(!s.error.empty()||!s.edit_error.empty()||!s.resource_error.empty())throw std::runtime_error(s.error+s.edit_error+s.resource_error);if(s.generation==d->generation&&s.presented_revision==snapshot.revision&&!s.preview&&s.samples>=samples&&s.presented_epoch==s.requested_epoch&&s.camera.epoch==renderer.input_camera().epoch)return s;if(now()-start>300)throw std::runtime_error("角色预览超时");}};
  auto first=ready(1);check(target<first.bounds.size()&&!first.bounds[target].empty,"角色包围盒为空");renderer.frame(first.bounds[target]);auto camera=renderer.input_camera();camera.yaw=0;camera.pitch=.02f;auto view=std::array<float,6>{camera.target.x,camera.target.y,camera.target.z,camera.distance,0,.02f};renderer.camera_view(view);ready(16);
  J report={{"source",input.generic_string()},{"target",actor.id},{"model","composition-preview-v1"},{"neutral",.5},{"cases",J::array()},{"ui_checks","passed"}};
  auto apply=[&]{auto next=original;next.revision=snapshot.revision+1;auto &v=next.values[target];v.extension.age=age->value();v.extension.strength=frame->value();const auto missing=apply_composition(actor,v,fat->value(),legacy->isChecked());snapshot=std::move(next);renderer.edit(snapshot);QStringList names;for(const auto &m:missing)names<<QString::fromUtf8(m);status->setText(missing.empty()?QStringLiteral("正在更新角色…"):QStringLiteral("缺少 Morph：")+names.join(QStringLiteral("、")));return missing;};
  auto safe_apply=[&]{try{apply();}catch(const std::exception &e){status->setText(QString::fromUtf8(e.what()));}};
  for(auto *s:{age,frame,fat})QObject::connect(s,&QDoubleSpinBox::valueChanged,&window,safe_apply);
  QObject::connect(step,&QDoubleSpinBox::valueChanged,&window,[&](double x){age->setSingleStep(std::abs(x));});
  QObject::connect(legacy,&QCheckBox::toggled,&window,[&](bool on){fat->setEnabled(!on);safe_apply();});
  QObject::connect(front,&QPushButton::clicked,&window,[&]{view[4]=0;renderer.camera_view(view);});QObject::connect(side,&QPushButton::clicked,&window,[&]{view[4]=1.5707963f;renderer.camera_view(view);});QObject::connect(oblique,&QPushButton::clicked,&window,[&]{view[4]=.65f;renderer.camera_view(view);});
  auto settings=[&](double a,double s,double f,bool old=false){QSignalBlocker ba(age),bs(frame),bf(fat),bl(legacy);age->setValue(a);frame->setValue(s);fat->setValue(f);legacy->setChecked(old);fat->setEnabled(!old);return apply();};
  QObject::connect(reset,&QPushButton::clicked,&window,[&]{settings(age->value(),.5,.5);});
  auto metadata=[&]{return J{{"age",age->value()},{"frame",frame->value()},{"fat",fat->value()},{"legacy",legacy->isChecked()},{"source",input.generic_string()},{"model","composition-preview-v1"}};};
  QObject::connect(save,&QPushButton::clicked,&window,[&]{try{const auto path=QFileDialog::getSaveFileName(&window,QStringLiteral("保存预览效果副本"),QString::fromStdWString((output/"custom.dufex").wstring()),"DUFEX (*.dufex)");if(path.isEmpty())return;const auto p=fs::path(path.toStdWString());auto copy=snapshot;const auto c=renderer.input_camera();copy.view=std::array<float,6>{c.target.x,c.target.y,c.target.z,c.distance,c.yaw,c.pitch};save_scene_extension(p,*d,copy);std::ofstream(p.string()+".preview.json")<<metadata().dump(2);status->setText(QStringLiteral("已保存 Morph 效果副本。正式编辑器重新调年龄将使用原算法。"));}catch(const std::exception &e){status->setText(QString::fromUtf8(e.what()));}});
  auto capture=[&](const fs::path &file){auto future=renderer.capture(d->generation,snapshot.revision);const auto start=now();while(future.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready){pump();if(now()-start>60)throw std::runtime_error("预览截图超时");}const auto im=future.get();QImage image(im.rgba.data(),im.size,im.size,QImage::Format_RGBA8888);check(image.mirrored().save(QString::fromStdWString(file.wstring())),"保存预览截图失败");};
  if(batch){
    for(double a:{original.values[target].extension.age,18.}){
      settings(a,.5,.5);auto base=ready(24);renderer.frame(base.bounds[target]);camera=renderer.input_camera();view={camera.target.x,camera.target.y,camera.target.z,camera.distance,0,.02f};
      // 同一年龄的全部对照固定相机和 Scale；左右两列分别正面、侧面。
      for(const auto &[name,s,f,old]:std::vector<std::tuple<std::string,double,double,bool>>{{"baseline",.5,.5,false},{"frame-small",0,.5,false},{"frame-large",1,.5,false},{"fat-thin",.5,0,false},{"fat-heavy",.5,1,false},{"legacy-large",1,.5,true}}){
        std::cout<<"CASE age="<<a<<' '<<name<<'\n';const auto missing=settings(a,s,f,old);auto item=metadata();item["name"]=name;item["missing"]=missing;
        for(const auto &control:runtime::growth_values(a,.5)){const auto m=find_control(actor,control.label);if(m!=actor.morphs.end())item["morphs"][control.label]=snapshot.values[target].morphs[size_t(m-actor.morphs.begin())];}
        const auto prefix=std::string("age-")+std::to_string(int(a))+"-"+name;
        for(int side_view=0;side_view<2;++side_view){view[4]=side_view?1.5707963f:0;renderer.camera_view(view);const auto state=ready(32);capture(output/(prefix+(side_view?"-side.png":"-front.png")));item["samples"]=state.samples;}
        auto copy=snapshot;view[4]=0;copy.view=view;save_scene_extension(output/(prefix+".dufex"),*d,copy);
        if(a==18&&name=="fat-heavy"){auto restored=load_scene_extension(output/(prefix+".dufex"),roots,2);check(restored.snapshot.values[target].morphs==copy.values[target].morphs,"预览副本重开 Morph 不一致");item["reopen_verified"]=true;}
        report["cases"].push_back(item);std::ofstream(output/"report.json")<<report.dump(2);
      }
    }
    settings(original.values[target].extension.age,.5,.5);renderer.camera_view(view);ready(16);check(panel->grab().save(QString::fromStdWString((output/"controls.png").wstring())),"面板截图失败");std::cout<<"Preview capture complete\n";return 0;
  }
  apply();QTimer timer;QObject::connect(&timer,&QTimer::timeout,&window,[&]{if(host->width()!=renderer.status().width||host->height()!=renderer.status().height)renderer.resize(host->width(),host->height());const auto s=renderer.status();if(!s.error.empty()||!s.edit_error.empty())status->setText(QString::fromUtf8(s.error+s.edit_error));else if(s.presented_revision==snapshot.revision&&!s.preview)status->setText(QStringLiteral("可继续调整参数，或切换正面／侧面观察。"));});timer.start(250);return app.exec();
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
