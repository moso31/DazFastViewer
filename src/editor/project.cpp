#include "editor/parameter_widgets.h"
#include "editor/project.h"
#include "runtime/physics_json.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QToolButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <stdexcept>

namespace dfv::editor {
static void fail(const QString &message) {throw std::runtime_error(message.toUtf8().toStdString());}
QStringList ProjectSettings::normalize(const QStringList &roots) {
  QStringList result;
  for(const auto &root:roots) {
    const auto path=QDir::cleanPath(QDir::fromNativeSeparators(root.trimmed()));
    if(root.trimmed().isEmpty() || !QDir::isAbsolutePath(path)) fail(QStringLiteral("内容库必须使用绝对目录：")+root);
    if(!result.contains(path,Qt::CaseInsensitive)) result.append(path);
  }
  return result;
}
ProjectSettings ProjectSettings::load(const QString &path) {
  ProjectSettings settings;settings.file=QFileInfo(path).absoluteFilePath();QFile file(settings.file);
  if(!file.exists()) return settings;
  if(!file.open(QIODevice::ReadOnly)) fail(QStringLiteral("无法读取项目设置：")+file.errorString());
  QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
  if(error.error!=QJsonParseError::NoError || !doc.isObject()) fail(QStringLiteral("项目设置 JSON 无效：")+error.errorString());
  const auto object=doc.object();
  if(object.contains("physics"))settings.physics=runtime::physics_options_from_json(nlohmann::json::parse(QJsonDocument(object.value("physics").toObject()).toJson().toStdString()));
  if(object.contains("parameter_settings"))settings.parameter_settings=nlohmann::json::parse(QJsonDocument(object.value("parameter_settings").toObject()).toJson().toStdString()).get<runtime::ParameterSettingsState>();
  settings.history_limit=object.value("history_limit").toInt(50);
  if(settings.history_limit<1||settings.history_limit>500)fail(QStringLiteral("历史记录条数必须在 1 到 500 之间"));
  if(object.value("version").toInt()!=1 || !object.value("content_roots").isArray()) fail(QStringLiteral("不支持的项目设置格式"));
  for(const auto &root:object.value("content_roots").toArray()) {
    if(!root.isString()) fail(QStringLiteral("内容库路径必须是字符串"));settings.content_roots.append(root.toString());
  }
  settings.content_roots=normalize(settings.content_roots);return settings;
}
void ProjectSettings::save() const {
  if(history_limit<1||history_limit>500)fail(QStringLiteral("历史记录条数必须在 1 到 500 之间"));
  QJsonArray paths;for(const auto &root:normalize(content_roots)) paths.append(root);
  QSaveFile output(file);if(!output.open(QIODevice::WriteOnly)) fail(QStringLiteral("无法保存项目设置：")+output.errorString());
  const auto physics=QJsonDocument::fromJson(QByteArray::fromStdString(runtime::physics_json(this->physics).dump())).object();
  const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"content_roots",paths},{"history_limit",history_limit},{"physics",physics},{"parameter_settings",QJsonDocument::fromJson(QByteArray::fromStdString(nlohmann::json(parameter_settings).dump())).object()}}).toJson(QJsonDocument::Indented);
  if(output.write(bytes)!=bytes.size() || !output.commit()) fail(QStringLiteral("项目设置保存失败：")+output.errorString());
}
bool edit_project_settings(QWidget *parent,ProjectSettings &settings,ApplicationSettings &application,const QString &application_file,bool persistent,const std::function<void(bool)> &applied) {
  QDialog dialog(parent);dialog.setObjectName("ProjectSettingsDialog");dialog.setWindowTitle(QStringLiteral("项目设置"));dialog.setWindowFlag(Qt::WindowMaximizeButtonHint);dialog.resize(760,660);
  auto *layout=new QVBoxLayout(&dialog);auto *tabs=new QTabWidget;tabs->setObjectName("ProjectSettingsTabs");layout->addWidget(tabs,1);
  QMap<QString,QToolButton *> headers;
  auto page=[&](const QString &title,const char *name) {
    auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    auto *widget=new QWidget;widget->setObjectName(name);auto *body=new QVBoxLayout(widget);body->setAlignment(Qt::AlignTop);scroll->setWidget(widget);tabs->addTab(scroll,title);return body;
  };
  auto section=[&](QVBoxLayout *parent_layout,const QString &key,const QString &title) {
    auto *header=new QToolButton;header->setObjectName(key+"Collapse");header->setText(title);header->setCheckable(true);
    const bool expanded=application.expanded.value(key,true);header->setChecked(expanded);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header->setArrowType(expanded?Qt::DownArrow:Qt::RightArrow);
    header->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);parent_layout->addWidget(header);headers[key]=header;
    auto *content=new QWidget;content->setObjectName(key+"Section");content->setVisible(expanded);parent_layout->addWidget(content);
    QObject::connect(header,&QToolButton::toggled,&dialog,[header,content](bool on){content->setVisible(on);header->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});return content;
  };
  auto note=[](QLayout *target,const QString &text) {auto *label=new QLabel(text);label->setWordWrap(true);target->addWidget(label);};
  auto *resources=page(QStringLiteral("资源与保存"),"ProjectResourcesPage");
  auto *library_layout=new QVBoxLayout(section(resources,"libraries",QStringLiteral("资源库路径")));
  note(library_layout,QStringLiteral("按从上到下的顺序查找资源，同一路径优先使用靠前的库。保存后用于后续资产加载。"));
  auto *list=new QListWidget;list->setObjectName("ProjectContentRoots");list->addItems(settings.content_roots);list->setMinimumHeight(180);library_layout->addWidget(list,1);
  auto *buttons=new QHBoxLayout;library_layout->addLayout(buttons);
  auto button=[&](const QString &name,auto callback) {auto *b=new QPushButton(name);buttons->addWidget(b);QObject::connect(b,&QPushButton::clicked,&dialog,callback);};
  button(QStringLiteral("添加目录…"),[&] {const auto path=QFileDialog::getExistingDirectory(&dialog,QStringLiteral("选择包含 data / Runtime 的内容库根目录"));if(!path.isEmpty()) {QStringList paths;for(int i=0;i<list->count();++i) paths.append(list->item(i)->text());paths.append(path);list->clear();list->addItems(ProjectSettings::normalize(paths));}});
  button(QStringLiteral("移除"),[&] {delete list->takeItem(list->currentRow());});
  auto move=[&](int delta) {const int row=list->currentRow(),next=row+delta;if(row>=0 && next>=0 && next<list->count()) {auto *item=list->takeItem(row);list->insertItem(next,item);list->setCurrentRow(next);}};
  button(QStringLiteral("上移"),[&] {move(-1);});button(QStringLiteral("下移"),[&] {move(1);});buttons->addStretch();
  auto *save_layout=new QVBoxLayout(section(resources,"saveFile",QStringLiteral("保存文件")));
  auto *location=new QLineEdit(QDir::toNativeSeparators(settings.file));location->setObjectName("ProjectSaveFile");location->setReadOnly(true);save_layout->addWidget(location);
  note(save_layout,QStringLiteral("资源库路径保存在此项目文件中。界面和渲染偏好随应用保存，下次启动自动恢复。"));
  auto *history_form=new QFormLayout;save_layout->addLayout(history_form);
  auto *history_limit=new QSpinBox;history_limit->setObjectName("HistoryLimit");history_limit->setRange(1,500);history_limit->setValue(settings.history_limit);history_limit->setSuffix(QStringLiteral(" 条"));history_limit->setKeyboardTracking(false);
  history_form->addRow(QStringLiteral("撤销历史上限"),history_limit);
  note(save_layout,QStringLiteral("默认 50 条。拖动与复合操作各计一条；降低上限会释放超出的历史。场景恢复点自动保存在本机，异常退出后可恢复，不覆盖手动保存的场景。"));

  auto *render=page(QStringLiteral("渲染"),"ProjectRenderPage");
  auto *display=new QFormLayout(section(render,"display",QStringLiteral("界面与视口")));
  auto *ui_percent=new QSpinBox;ui_percent->setObjectName("UiScalePercent");ui_percent->setRange(50,200);ui_percent->setSingleStep(10);ui_percent->setSuffix("%");ui_percent->setValue(application.ui_percent);ui_percent->setKeyboardTracking(false);
  display->addRow(QStringLiteral("界面缩放"),ui_percent);
  auto *percent=new QSpinBox;percent->setObjectName("ViewportRenderPercent");percent->setRange(50,100);percent->setSingleStep(5);percent->setSuffix("%");percent->setValue(application.viewport.percent);percent->setKeyboardTracking(false);
  display->addRow(QStringLiteral("视口渲染倍率"),percent);
  auto *filter=new QComboBox;filter->setObjectName("ViewportReconstruction");filter->addItems({QStringLiteral("双三次（较清晰）"),QStringLiteral("双线性（较柔和）")});filter->setCurrentIndex(application.viewport.reconstruction==Reconstruction::bicubic?0:1);
  display->addRow(QStringLiteral("升采样方式"),filter);
  auto *sharpen_row=new QWidget;auto *sharpen_layout=new QHBoxLayout(sharpen_row);sharpen_layout->setContentsMargins(0,0,0,0);
  auto *sharpen=new QSlider(Qt::Horizontal);sharpen->setObjectName("ViewportSharpenStrength");sharpen->setRange(0,int(max_viewport_sharpen*100));sharpen->setSingleStep(1);sharpen->setPageStep(10);sharpen->setValue(qRound(application.viewport.sharpen*100));
  sharpen->setAccessibleName(QStringLiteral("视口锐化强度"));sharpen->setToolTip(QStringLiteral("增强视口画面的边缘和局部对比度，0.00 关闭，最大 20.00。方向键微调 0.01。仅影响显示，不改变模型或材质；过高可能放大噪点。"));
  auto *sharpen_value=new QLabel;sharpen_value->setObjectName("ViewportSharpenValue");sharpen_value->setAlignment(Qt::AlignRight|Qt::AlignVCenter);sharpen_value->setMinimumWidth(sharpen_value->fontMetrics().horizontalAdvance("20.00"));
  auto show_sharpen=[=](int value){sharpen_value->setText(QString::number(value/100.,'f',2));};show_sharpen(sharpen->value());QObject::connect(sharpen,&QSlider::valueChanged,&dialog,show_sharpen);
  sharpen_layout->addWidget(sharpen,1);sharpen_layout->addWidget(sharpen_value);display->addRow(QStringLiteral("视口锐化强度"),sharpen_row);
  note(display,QStringLiteral("界面缩放只调整文字和控件。渲染倍率 50% 对应约四分之一像素，可更快刷新，但细节较少。"));
  auto *reset_ui=new QPushButton(QStringLiteral("界面恢复 100%"));reset_ui->setObjectName("UiZoomReset");auto *reset_view=new QPushButton(QStringLiteral("渲染倍率恢复 100%"));
  display->addRow(reset_ui,reset_view);QObject::connect(reset_ui,&QPushButton::clicked,&dialog,[=]{ui_percent->setValue(100);});QObject::connect(reset_view,&QPushButton::clicked,&dialog,[=]{percent->setValue(100);});
  auto *textures=new QFormLayout(section(render,"textures",QStringLiteral("纹理精度")));
  auto *limit=new QComboBox;limit->setObjectName("RenderTextureLimit");for(int size:{512,1024,2048,4096,0}) limit->addItem(size?QString::number(size):QStringLiteral("无限制"),size);
  limit->setCurrentIndex(std::max(0,limit->findData(application.render.texture_limit)));textures->addRow(QStringLiteral("纹理最大分辨率"),limit);
  note(textures,QStringLiteral("较低精度减少显存占用，可能改善刷新速度，但会减少皮肤、头发等纹理细节。应用后会释放旧渲染资源并按新精度重新加载，原始贴图不变。"));
  auto *materials=new QFormLayout(section(render,"materials",QStringLiteral("材质质量")));
  auto *sss=new QCheckBox(QStringLiteral("启用皮肤次表面散射（SSS）"));sss->setObjectName("RenderSubsurface");sss->setChecked(application.render.subsurface);materials->addRow(sss);
  note(materials,QStringLiteral("关闭可减少部分角色的计算量，但皮肤透光和柔和感会改变。"));
  auto *bump=new QCheckBox(QStringLiteral("启用凹凸与法线"));bump->setObjectName("RenderBumpNormal");bump->setChecked(application.render.bump_and_normal);materials->addRow(bump);
  note(materials,QStringLiteral("关闭可减少材质计算，但毛孔、织物等表面细节会变平。几何置换保持原设置。"));
  auto *transparent=new QSpinBox;transparent->setObjectName("RenderTransparentBounces");transparent->setRange(1,32);transparent->setValue(application.render.transparent_bounces);materials->addRow(QStringLiteral("透明层数上限"),transparent);
  note(materials,QStringLiteral("默认 32。降低后可能加快重叠透明头发的计算，但较深层的发片、睫毛或透明物体可能变暗或不再透光。"));
  auto *physics_page=page(QStringLiteral("物理"),"ProjectPhysicsPage");
  auto *physics_form=new QFormLayout(section(physics_page,"physics",QStringLiteral("快速物理")));
  auto *paused=new QCheckBox(QStringLiteral("暂停所有已启用的物理"));paused->setObjectName("PhysicsPaused");paused->setChecked(settings.physics.paused);physics_form->addRow(paused);
  auto *ground=new QCheckBox(QStringLiteral("使用虚拟地面"));ground->setObjectName("PhysicsGround");ground->setChecked(settings.physics.ground);physics_form->addRow(ground);
  auto number=[&](const char *name,double lo,double hi,double value,const QString &label){auto *spin=parameter_widgets::number(name,true);spin->setRange(lo,hi);spin->setDecimals(3);spin->setValue(value);spin->setKeyboardTracking(false);physics_form->addRow(label,spin);return spin;};
  auto *refresh=new QLineEdit(QString::number(settings.physics.refresh_hz,'g',17));refresh->setObjectName("PhysicsRefreshHz");refresh->setToolTip(QStringLiteral("默认每秒 4 次，支持任意正数和科学计数法。实际更新频率不会超过渲染帧率，不改变解算时间步长。"));physics_form->addRow(QStringLiteral("物理帧刷新率（次/秒）"),refresh);
  auto *height=number("PhysicsGroundHeight",-1000000,1000000,settings.physics.ground_height*100,QStringLiteral("地面高度（DAZ Y，cm）"));
  auto *gravity=number("PhysicsGravity",0,100,settings.physics.gravity,QStringLiteral("重力（m/s²）"));
  auto *physics_quality=new QComboBox;physics_quality->setObjectName("PhysicsQuality");physics_quality->addItems({QStringLiteral("交互优先"),QStringLiteral("均衡"),QStringLiteral("效果优先")});physics_quality->setCurrentIndex(settings.physics.quality);physics_form->addRow(QStringLiteral("解算质量"),physics_quality);
  note(physics_form,QStringLiteral("每件物体、服装或发型需在自身根节点启用。点击模拟后按角色独立推进指定轮数；画面可以延迟显示。物理帧没有几何变化时不重置渲染。"));
  tabs->setCurrentIndex(application.settings_tab);
  auto *status=new QLabel;status->setObjectName("ProjectSettingsStatus");status->setWordWrap(true);layout->addWidget(status);
  auto *box=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Save|QDialogButtonBox::Cancel);box->setObjectName("ProjectSettingsButtons");
  box->button(QDialogButtonBox::Apply)->setText(QStringLiteral("应用"));box->button(QDialogButtonBox::Apply)->setToolTip(QStringLiteral("立即应用到当前窗口，不写入配置，保持设置窗口打开。"));
  box->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));box->button(QDialogButtonBox::Save)->setToolTip(QStringLiteral("应用并保存所有设置，下次启动时恢复。"));
  box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));box->button(QDialogButtonBox::Cancel)->setToolTip(QStringLiteral("关闭窗口，丢弃尚未应用的修改；已应用的设置继续生效。"));layout->addWidget(box);
  QObject::connect(box,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  auto *numeric=parameter_widgets::context(&dialog);numeric->local=settings.parameter_settings;numeric->owner="project";parameter_widgets::decorate(&dialog,"project/");
  auto submit=[&](bool save) {
    try {
      auto updated=settings;updated.parameter_settings=numeric->local;updated.content_roots.clear();for(int i=0;i<list->count();++i) updated.content_roots.append(list->item(i)->text());updated.content_roots=ProjectSettings::normalize(updated.content_roots);
      auto preferences=application;preferences.ui_percent=ui_percent->value();preferences.viewport={percent->value(),filter->currentIndex()==0?Reconstruction::bicubic:Reconstruction::bilinear,sharpen->value()/100.f};
      updated.history_limit=history_limit->value();
      updated.physics={paused->isChecked(),ground->isChecked(),float(gravity->value()),float(height->value()/100),physics_quality->currentIndex()};bool refresh_ok=false;updated.physics.refresh_hz=refresh->text().trimmed().toDouble(&refresh_ok);if(!refresh_ok)throw std::runtime_error("物理帧刷新率必须是正数");runtime::validate_physics(updated.physics);
      preferences.render={limit->currentData().toInt(),transparent->value(),sss->isChecked(),bump->isChecked()};preferences.settings_tab=tabs->currentIndex();
      for(auto i=headers.cbegin();i!=headers.cend();++i) preferences.expanded[i.key()]=i.value()->isChecked();
      // 应用可能已更新内存中的路径，保存时仍需写入磁盘。
      if(save&&persistent) {
        updated.save();
        preferences.save(application_file);
      }
      settings=std::move(updated);application=std::move(preferences);if(applied)applied(save);
      if(save) dialog.accept();
      else status->setText(QStringLiteral("已应用到当前窗口，尚未保存。点击“保存”可在下次启动时恢复。"));
    }
    catch(const std::exception &e) {status->setText(QString::fromUtf8(e.what()));}
  };
  QObject::connect(box,&QDialogButtonBox::accepted,&dialog,[&]{submit(true);});
  QObject::connect(box->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{submit(false);});
  for(int i=0;i<list->count();++i) if(!QFileInfo(list->item(i)->text()).isDir()) {list->item(i)->setToolTip(QStringLiteral("目录当前不可访问，配置会保留"));list->item(i)->setForeground(Qt::darkRed);}
  return dialog.exec()==QDialog::Accepted;
}
}
