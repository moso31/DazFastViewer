#include "editor/renderer.h"
#include "editor/project.h"
#include "editor/parameters.h"
#include "daz/pose.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QScreen>
#include <QSignalBlocker>
#include <QTranslator>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFileInfo>
#include <QComboBox>
#include <QDir>
#include <QMessageBox>
#include <fstream>

namespace {
using namespace dfv;
using namespace dfv::editor;
static std::filesystem::path file_path(const QString &s) {return std::filesystem::path(s.toStdWString());}
static QString text(const std::string &s) {return QString::fromUtf8(s.data(),qsizetype(s.size()));}
class Editor final:public QMainWindow {
  QWidget *host_=nullptr;
  ParameterPanel *parameters_=nullptr;
  QComboBox *libraries_=nullptr;
  QTreeWidget *hierarchy_=nullptr;
  QTreeView *explorer_=nullptr;
  QFileSystemModel *files_=nullptr;
  QLabel *selection_=nullptr;
  QLabel *pose_status_=nullptr;
  QAction *open_=nullptr;
  QAction *project_action_=nullptr;
  QDoubleSpinBox *transform_[9]{};
  ProjectSettings project_;
  std::shared_ptr<Document> document_;
  Snapshot snapshot_;
  std::unique_ptr<Renderer> renderer_;
  std::jthread loader_;
  std::vector<std::filesystem::path> roots_;
  std::filesystem::path output_;
  std::filesystem::path reload_file_;
  std::filesystem::path pose_file_;
  bool pose_test_=false,frame_pending_=false;
  bool formula_test_=false;
  std::vector<std::string> formula_names_={"Arms Length","Chest Scale","Eyes Closed","HS Sanny Shy","Flex Quad Left"};
  uint64_t formula_ui_revision_=0;
  size_t formula_case_=0;
  uint64_t test_formula_evaluations_=0;
  nlohmann::json parameter_checks_=nlohmann::json::array();
  bool loading_=false,self_test_=false;
  QString load_error_;
  int selected_=-1,test_stage_=0;
  size_t test_morph_=0;
  uint64_t generation_=0,test_evaluations_=0;
  uint64_t test_skin_evaluations_=0;
  nlohmann::json pose_report_;
  double test_initial_displacement_=0;
  qint64 test_started_=QDateTime::currentMSecsSinceEpoch();
  QDockWidget *dock(const QString &title,QWidget *widget,Qt::DockWidgetArea area) {
    auto *d=new QDockWidget(title,this);d->setObjectName(title);d->setWidget(widget);addDockWidget(area,d);return d;
  }
  void send() {++snapshot_.revision;renderer_->edit(snapshot_);}
  int selected_skin() const {
    if(selected_<0||!document_) return -1;const auto instance=document_->catalog.targets[size_t(selected_)].instance;
    for(size_t i=0;i<document_->skeletons.skins.size();++i) if(document_->skeletons.skins[i].instance==instance) return int(i);
    return -1;
  }
  bool apply_pose_file(const std::filesystem::path &file) {
    try {
      if(loading_) throw std::runtime_error("请等待场景加载完成后应用姿势");
      const auto index=selected_skin();if(index<0) throw std::runtime_error("请先选中一个带骨骼蒙皮的角色");
      const auto &skin=document_->skeletons.skins[size_t(index)];
      auto applied=daz::apply_pose(daz::read_pose(file),skin,snapshot_.poses[size_t(index)],document_->catalog.targets[size_t(selected_)],snapshot_.values[size_t(selected_)]);
      snapshot_.poses[size_t(index)]=std::move(applied.joints);snapshot_.values[size_t(selected_)]=std::move(applied.properties);pose_report_=applied.report;
      std::ofstream(output_/"pose-report.json")<<pose_report_.dump(2);
      const auto skipped=pose_report_["unapplied"].size();
      pose_status_->setText(QStringLiteral("姿势：%1\n已应用 %2 个骨骼通道；%3 个参数未应用。%4").arg(QString::fromStdWString(file.stem().wstring())).arg(pose_report_["applied_bone_channels"].get<int>()).arg(skipped).arg(skipped?QStringLiteral("点击下方查看详情。") : QString()));
      pose_status_->setToolTip(QString::fromStdWString(file.wstring()));select(selected_);send();frame_pending_=true;return true;
    } catch(const std::exception &e) {
      if(self_test_) finish_test(false,e.what());else QMessageBox::warning(this,QStringLiteral("无法应用姿势"),text(e.what()));return false;
    }
  }
  void reset_pose() {
    if(loading_) return;const auto index=selected_skin();if(index<0) return;
    snapshot_.poses[size_t(index)]=document_->skeletons.skins[size_t(index)].initial;send();frame_pending_=true;
    pose_status_->setText(QStringLiteral("已恢复载入时的骨骼姿势；Morph 保持当前值。"));pose_report_=nullptr;
  }
  void open_asset(const std::filesystem::path &file) {
    try {const auto data=daz::read_document_file(file);
      if(data.value("asset_info",nlohmann::json::object()).value("type","")=="preset_pose") apply_pose_file(file);else load(file);
    } catch(const std::exception &e) {QMessageBox::warning(this,QStringLiteral("无法打开 DUF"),text(e.what()));}
  }
  void update_libraries() {
    QSignalBlocker block(libraries_);libraries_->clear();roots_.clear();
    for(const auto &root:project_.content_roots) {roots_.push_back(file_path(root));libraries_->addItem(root);}
    if(!project_.content_roots.empty()) explorer_->setRootIndex(files_->setRootPath(project_.content_roots.front()));
    else explorer_->setRootIndex(files_->setRootPath(QString()));
  }
  void project_settings() {
    if(loading_) return;
    if(edit_project_settings(this,project_)) {
      update_libraries();if(document_) load(std::filesystem::u8path(document_->loaded.report.at("input").get<std::string>()),true);
    }
  }
  void set_morph(size_t morph,double value) {
    if(selected_<0) return;
    auto &current=snapshot_.values[size_t(selected_)].morphs[morph];const float next=float(value);
    if(current==next) return;runtime::set_parameter(document_->catalog.targets[size_t(selected_)],snapshot_.values[size_t(selected_)],morph,next);
    parameters_->refresh(morph);
    send();
  }
  void select(int index) {
    selected_=index;
    if(index<0 || !document_) {selection_->setText(QStringLiteral("请先选择场景对象"));parameters_->bind(nullptr,nullptr);for(auto *spin:transform_) spin->setEnabled(false);return;}
    const auto &target=document_->catalog.targets[size_t(index)];selection_->setText(text(target.label));
    const auto &values=snapshot_.values[size_t(index)];
    const auto &t=values.transform;
    const float data[]={t.translation_cm.x,t.translation_cm.y,t.translation_cm.z,t.rotation_degrees.x,t.rotation_degrees.y,t.rotation_degrees.z,t.scale.x*100,t.scale.y*100,t.scale.z*100};
    for(int i=0;i<9;++i) {QSignalBlocker block(transform_[i]);transform_[i]->setValue(data[i]);transform_[i]->setEnabled(true);}
    parameters_->bind(&target,&values);
  }
  void reset_selected() {
    if(selected_<0) return;
    auto &value=snapshot_.values[size_t(selected_)];value.transform={};
    const auto &target=document_->catalog.targets[size_t(selected_)];
    for(size_t m=0;m<value.morphs.size();++m) value.morphs[m]=target.morphs[m].evaluable||target.morphs[m].unsupported.empty()?target.morphs[m].initial:0;
    runtime::sync_aliases(target,value);
    const auto skin=selected_skin();if(skin>=0) {snapshot_.poses[size_t(skin)]=document_->skeletons.skins[size_t(skin)].initial;frame_pending_=true;pose_status_->setText(QStringLiteral("已重置选中角色的姿势与形态。"));pose_report_=nullptr;}
    select(selected_);send();
  }
  void finish_test(bool pass,const std::string &error={}) {
    const auto status=renderer_->status();
    nlohmann::json report={{"status",pass?"PASS":"FAIL"},{"error",error},{"stage",test_stage_},
      {"mesh_creations",status.adapter.meshes},{"geometry_updates",status.adapter.geometry_updates},{"instance_updates",status.adapter.instance_updates},
      {"morph_evaluations",status.evaluation.morph_evaluations},{"max_displacement_m",status.max_displacement},
      {"skin_evaluations",status.skinning.evaluations},{"skin_vertices",status.skinning.vertices},
      {"formula_evaluations",status.formulas.expressions},{"formula_channels",status.formulas.channels},
      {"qt_version",QT_VERSION_STR},{"monitor",screen()->name().toStdString()},{"window",{x(),y(),width(),height()}},
      {"scope","selected-object-morph-transform-reset-camera-no-morph-evaluation"}};
    if(!reload_file_.empty()) report["scope"]="background-scene-replacement-generation-isolation";
    if(pose_test_) report["scope"]="pose-apply-reset-camera-no-skin-evaluation";
    if(formula_test_) {report["scope"]="parameter-slider-regression-with-ERC-JCM";report["parameter_checks"]=parameter_checks_;}
    report["project_file"]=project_.file.toUtf8().toStdString();report["content_roots"]=nlohmann::json::array();
    for(const auto &root:project_.content_roots) report["content_roots"].push_back(root.toUtf8().toStdString());
    report["named_parameters"]=nlohmann::json::array();
    if(document_) for(const auto &target:document_->catalog.targets) for(const auto &m:target.morphs)
      if(m.label=="Arms Length" || m.label=="Chest Scale" || m.label=="Eyes Closed" || m.label=="HS Sanny Shy")
        report["named_parameters"].push_back({{"label",m.label},{"group",m.group},{"kind",m.kind},{"source",m.source}});
    std::ofstream(output_/"editor-check.json")<<report.dump(2);QApplication::exit(pass?0:1);
  }
  void tick() {
    const auto state=renderer_->status();
    if(!state.error.empty()) {statusBar()->showMessage(QStringLiteral("渲染错误：")+text(state.error));if(self_test_) finish_test(false,state.error);return;}
    if(!state.edit_error.empty()) {statusBar()->showMessage(QStringLiteral("本次编辑未应用：")+text(state.edit_error));if(self_test_) finish_test(false,state.edit_error);return;}
    if(document_&&state.generation==document_->generation&&state.applied_revision==snapshot_.revision&&selected_>=0&&size_t(selected_)<state.effective.size()) parameters_->evaluated(state.effective[size_t(selected_)]);
    if(!load_error_.isEmpty()) statusBar()->showMessage(load_error_);
    else if(!loading_) statusBar()->showMessage(QStringLiteral("OptiX · %1 samples · 网格 %2 · 顶点更新 %3 · 蒙皮求值 %4").arg(state.samples).arg(state.adapter.meshes).arg(state.adapter.geometry_updates).arg(state.skinning.evaluations));
    if(frame_pending_&&document_&&state.generation==document_->generation&&state.applied_revision==snapshot_.revision&&selected_>=0&&size_t(selected_)<state.bounds.size()) {renderer_->frame(state.bounds[size_t(selected_)]);frame_pending_=false;return;}
    if(!self_test_) return;
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>(formula_test_?360000:180000)) {finish_test(false,"等待编辑器验证超时");return;}
    if(!document_ || state.generation!=document_->generation || state.presented_revision!=snapshot_.revision || state.presented_epoch!=state.requested_epoch || state.frames==0 || state.samples<8) return;
    if(formula_test_) {
      const auto &names=formula_names_;
      if(test_stage_==0) {
        test_initial_displacement_=state.max_displacement;const auto &morphs=document_->catalog.targets[0].morphs;
        auto found=std::find_if(morphs.begin(),morphs.end(),[&](const auto &m) {return m.label==names[formula_case_]&&m.kind!="alias"&&m.unsupported.empty();});
        if(found==morphs.end()) {finish_test(false,"指定参数未启用："+names[formula_case_]);return;}
        test_morph_=size_t(found-morphs.begin());parameters_->query(text(found->label));parameters_->select_parameter(test_morph_);
        if(names[formula_case_]=="Eyes Closed"||names[formula_case_]=="HS Sanny Shy") {
          for(const auto &skin:document_->skeletons.skins) if(skin.instance==document_->catalog.targets[0].instance) for(const auto &joint:skin.joints) if(joint.id=="head") {
            const auto p=joint.center_cm;const auto c=document_->loaded.scene.instances[skin.instance].transform.point({p.x*.01f,-p.z*.01f,p.y*.01f+.08f});
            ir::Bounds face;face.add({c.x-.18f,c.y-.18f,c.z-.22f});face.add({c.x+.18f,c.y+.18f,c.z+.22f});renderer_->frame(face);
          }
        }
        parameters_->set_slider(qRound((.5-found->minimum)/(found->maximum-found->minimum)*1000));test_stage_=1;
      } else if(test_stage_==1) {
        // 等待一次 Qt 绘制，确保最终 ERC 值已显示再截图。
        if(formula_ui_revision_!=state.applied_revision) {formula_ui_revision_=state.applied_revision;return;}
        if(state.max_displacement<1e-5||state.adapter.geometry_updates<1) {finish_test(false,"参数未产生可见几何更新");return;}
        parameter_checks_.push_back({{"name",names[formula_case_]},{"displacement_m",state.max_displacement},{"effective",state.effective[0][test_morph_]},{"status","PASS"}});
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/("editor-parameter-"+std::to_string(formula_case_)+".png")).wstring()));
        reset_selected();test_stage_=2;
      } else if(test_stage_==2) {
        if(state.max_displacement!=test_initial_displacement_) {finish_test(false,"参数恢复出现漂移");return;}
        if(++formula_case_<names.size()) {test_stage_=0;return;}
        test_evaluations_=state.evaluation.morph_evaluations;test_skin_evaluations_=state.skinning.evaluations;test_formula_evaluations_=state.formulas.expressions;renderer_->orbit(20,0);test_stage_=3;
      } else if(test_stage_==3) {
        if(state.skinning.evaluations!=test_skin_evaluations_||state.evaluation.morph_evaluations!=test_evaluations_||state.formulas.expressions!=test_formula_evaluations_) {finish_test(false,"相机操作触发变形求值");return;}
        finish_test(true);
      }
      return;
    }
    if(pose_test_) {
      if(test_stage_==0) {test_initial_displacement_=state.max_displacement;test_stage_=1;apply_pose_file(pose_file_);}
      else if(test_stage_==1) {
        if(state.max_displacement<.1||state.skinning.evaluations<2||state.adapter.geometry_updates<1) {finish_test(false,"姿势没有更新蒙皮网格");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"editor-pose.png").wstring()));
        test_stage_=2;reset_pose();
      } else if(test_stage_==2) {
        if(state.max_displacement!=test_initial_displacement_) {finish_test(false,"恢复姿势出现顶点漂移");return;}
        test_evaluations_=state.evaluation.morph_evaluations;test_skin_evaluations_=state.skinning.evaluations;test_stage_=3;renderer_->orbit(20,0);
      } else if(test_stage_==3) {
        if(state.skinning.evaluations!=test_skin_evaluations_||state.evaluation.morph_evaluations!=test_evaluations_) {finish_test(false,"相机更新触发了变形求值");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"editor-pose-reset.png").wstring()));finish_test(true);
      }
      return;
    }
    if(test_stage_==0) {
      if(!reload_file_.empty()) {test_stage_=5;load(reload_file_);return;}
      const auto &target=document_->catalog.targets[0];
      test_initial_displacement_=state.max_displacement;
      auto found=std::find_if(target.morphs.begin(),target.morphs.end(),[](const auto &m) {return m.label=="Bodybuilder Size" && m.unsupported.empty();});
      if(found==target.morphs.end()) {finish_test(false,"缺少 Bodybuilder Size 验证 Morph");return;}
      test_morph_=size_t(found-target.morphs.begin());parameters_->query(QStringLiteral("Bodybuilder"));
      parameters_->select_parameter(test_morph_);parameters_->set_slider(1000);
      ++test_stage_;
    } else if(test_stage_==1) {
      if(state.max_displacement<.005 || state.adapter.geometry_updates<1) {finish_test(false,"Morph 未改变可见几何");return;}
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"editor-morph.png").wstring()));
      transform_[0]->setValue(20);++test_stage_;
    } else if(test_stage_==2) {
      if(state.adapter.instance_updates<1) {finish_test(false,"实例变换未应用");return;}
      reset_selected();++test_stage_;
    } else if(test_stage_==3) {
      if(state.max_displacement!=test_initial_displacement_) {finish_test(false,"恢复载入形态出现漂移");return;}
      test_evaluations_=state.evaluation.morph_evaluations;renderer_->orbit(20,0);++test_stage_;
    } else if(test_stage_==4) {
      if(state.adapter.camera_updates<2) return;
      if(state.evaluation.morph_evaluations!=test_evaluations_ || state.adapter.meshes!=2) {finish_test(false,"相机触发 Morph 或网格重建");return;}
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"editor-final.png").wstring()));
      const auto &morphs=document_->catalog.targets[0].morphs;
      for(size_t i=0;i<morphs.size();++i) if(morphs[i].label=="HS Sanny Shy") {parameters_->query(QStringLiteral("HS Sanny Shy"));parameters_->select_parameter(i);repaint();screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"editor-catalog.png").wstring()));break;}
      finish_test(true);
    } else if(test_stage_==5 && document_->generation==2) {
      if(state.adapter.geometry_updates || state.evaluation.morph_evaluations) {finish_test(false,"场景切换继承了旧对象编辑状态");return;}
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"editor-reload.png").wstring()));finish_test(true);
    }
  }
public:
  void test_parameters(const QStringList &names) {if(names.empty()) return;formula_names_.clear();for(const auto &name:names) formula_names_.push_back(name.toUtf8().toStdString());}
  Editor(const std::filesystem::path &output,ProjectSettings project,bool self_test,std::filesystem::path reload_file,std::filesystem::path pose_file={},bool pose_test=false,bool formula_test=false):project_(std::move(project)),output_(output),reload_file_(std::move(reload_file)),pose_file_(std::move(pose_file)),pose_test_(pose_test),formula_test_(formula_test),self_test_(self_test||pose_test||formula_test) {
    setWindowTitle(QStringLiteral("DazFastViewer · 场景与形态编辑器"));setAttribute(Qt::WA_ShowWithoutActivating);
    setDockOptions(AnimatedDocks|AllowNestedDocks|AllowTabbedDocks);
    auto *central=new QWidget;auto *layout=new QVBoxLayout(central);layout->setContentsMargins(4,4,4,4);
    host_=new QWidget;host_->setAttribute(Qt::WA_NativeWindow);host_->setAttribute(Qt::WA_DontCreateNativeAncestors);host_->setFixedSize(960,720);
    layout->addWidget(host_,0,Qt::AlignCenter);layout->addWidget(new QLabel(QStringLiteral("右键旋转 · Shift＋右键平移 · 滚轮缩放")),0,Qt::AlignCenter);setCentralWidget(central);
    files_=new QFileSystemModel(this);files_->setNameFilters({"*.duf"});files_->setNameFilterDisables(false);files_->setReadOnly(true);
    explorer_=new QTreeView;explorer_->setModel(files_);
    for(int i=1;i<4;++i) explorer_->hideColumn(i);explorer_->setHeaderHidden(true);explorer_->setMinimumWidth(185);
    auto *browser=new QWidget;auto *browser_layout=new QVBoxLayout(browser);browser_layout->setContentsMargins(0,0,0,0);
    libraries_=new QComboBox;libraries_->setMinimumContentsLength(16);libraries_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    browser_layout->addWidget(libraries_);browser_layout->addWidget(explorer_);update_libraries();
    connect(libraries_,&QComboBox::currentTextChanged,this,[this](const QString &root) {explorer_->setRootIndex(files_->setRootPath(root));});
    auto *explorer_dock=dock(QStringLiteral("内容浏览器"),browser,Qt::LeftDockWidgetArea);
    hierarchy_=new QTreeWidget;hierarchy_->setHeaderLabel(QStringLiteral("场景对象"));hierarchy_->setMinimumWidth(185);
    auto *hierarchy_dock=dock(QStringLiteral("场景层次"),hierarchy_,Qt::LeftDockWidgetArea);tabifyDockWidget(explorer_dock,hierarchy_dock);hierarchy_dock->raise();
    auto *panel=new QWidget;auto *properties=new QVBoxLayout(panel);panel->setMinimumWidth(300);
    selection_=new QLabel(QStringLiteral("请先加载并选择对象"));selection_->setWordWrap(true);properties->addWidget(selection_);
    auto *form=new QFormLayout;
    const QString names[]={QStringLiteral("位移 X（厘米）"),QStringLiteral("位移 Y（厘米）"),QStringLiteral("位移 Z（厘米）"),QStringLiteral("旋转 X（度）"),QStringLiteral("旋转 Y（度）"),QStringLiteral("旋转 Z（度）"),QStringLiteral("缩放 X（%）"),QStringLiteral("缩放 Y（%）"),QStringLiteral("缩放 Z（%）")};
    for(int i=0;i<9;++i) {
      auto *spin=new QDoubleSpinBox;transform_[i]=spin;spin->setDecimals(2);spin->setRange(i>=6?.01:-10000,i>=6?10000:10000);spin->setValue(i>=6?100:0);
      spin->setEnabled(false);spin->setKeyboardTracking(false);form->addRow(names[i],spin);
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this](double) {
        if(selected_<0) return;auto &t=snapshot_.values[size_t(selected_)].transform;
        t.translation_cm={float(transform_[0]->value()),float(transform_[1]->value()),float(transform_[2]->value())};
        t.rotation_degrees={float(transform_[3]->value()),float(transform_[4]->value()),float(transform_[5]->value())};
        t.scale={float(transform_[6]->value()/100),float(transform_[7]->value()/100),float(transform_[8]->value()/100)};send();
      });
    }
    properties->addLayout(form);
    pose_status_=new QLabel(QStringLiteral("选中角色后，可从“姿势”菜单加载姿势 DUF。"));pose_status_->setWordWrap(true);properties->addWidget(pose_status_);
    auto *pose_details=new QPushButton(QStringLiteral("姿势应用详情"));properties->addWidget(pose_details);
    connect(pose_details,&QPushButton::clicked,this,[this] {
      QString details=QStringLiteral("尚未应用姿势。");
      if(!pose_report_.is_null()) {details=QStringLiteral("未应用的非零控制器或未知骨骼通道：\n");for(const auto &c:pose_report_["unapplied"]) details+=text(c.value("node","")+c.value("modifier","")+" · "+c.value("property","")+" = "+std::to_string(c.at("value").get<float>())+"\n"+c.value("reason","")+"\n");
        if(pose_report_["unapplied"].empty()) details=QStringLiteral("姿势文件中所有非零参数均已应用。");}
      QMessageBox::information(this,QStringLiteral("姿势应用详情"),details);
    });
    auto *reset=new QPushButton(QStringLiteral("重置选中对象"));properties->addWidget(reset);connect(reset,&QPushButton::clicked,this,[this] {reset_selected();});
    parameters_=new ParameterPanel;parameters_->changed=[this](size_t index,double value) {set_morph(index,value);};properties->addWidget(parameters_,1);
    dock(QStringLiteral("对象属性与 Morph"),panel,Qt::RightDockWidgetArea);
    auto *file_menu=menuBar()->addMenu(QStringLiteral("文件"));open_=file_menu->addAction(QStringLiteral("打开 DUF…"));open_->setShortcut(QKeySequence::Open);
    connect(open_,&QAction::triggered,this,[this] {const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("加载角色、场景或姿势"),{},QStringLiteral("DAZ 文件 (*.duf)"));if(!file.isEmpty()) open_asset(file_path(file));});
    auto *project_menu=menuBar()->addMenu(QStringLiteral("项目"));project_action_=project_menu->addAction(QStringLiteral("项目设置…"));
    connect(project_action_,&QAction::triggered,this,[this] {project_settings();});
    connect(file_menu->addAction(QStringLiteral("退出")),&QAction::triggered,this,&QWidget::close);
    auto *edit=menuBar()->addMenu(QStringLiteral("编辑"));connect(edit->addAction(QStringLiteral("重置选中对象")),&QAction::triggered,this,[this] {reset_selected();});
    auto *pose_menu=menuBar()->addMenu(QStringLiteral("姿势"));
    connect(pose_menu->addAction(QStringLiteral("应用姿势 DUF…")),&QAction::triggered,this,[this] {const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("为选中角色应用姿势"),{},QStringLiteral("DAZ 姿势 (*.duf)"));if(!file.isEmpty()) apply_pose_file(file_path(file));});
    connect(pose_menu->addAction(QStringLiteral("恢复载入姿势")),&QAction::triggered,this,[this] {reset_pose();});
    auto *view=menuBar()->addMenu(QStringLiteral("视图"));for(auto *d:findChildren<QDockWidget *>()) view->addAction(d->toggleViewAction());
    connect(view->addAction(QStringLiteral("框选当前对象")),&QAction::triggered,this,[this] {frame_pending_=true;});
    connect(explorer_,&QTreeView::doubleClicked,this,[this](const QModelIndex &index) {const auto file=files_->filePath(index);if(QFileInfo(file).suffix().compare("duf",Qt::CaseInsensitive)==0) open_asset(file_path(file));});
    connect(hierarchy_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item) {select(item?item->data(0,Qt::UserRole).toInt():-1);});
    QScreen *secondary=nullptr;for(auto *screen:QGuiApplication::screens()) if(screen!=QGuiApplication::primaryScreen()) {secondary=screen;break;}
    if(!secondary) throw std::runtime_error("缺少第二屏，编辑器不会在主屏启动");
    resize(1580,920);const QRect available=secondary->availableGeometry();
    if(width()>available.width() || height()>available.height()) throw std::runtime_error("第二屏工作区不足以容纳当前编辑器布局");
    move(available.center()-QPoint(width()/2,height()/2));show();
    renderer_=std::make_unique<Renderer>(reinterpret_cast<HWND>(host_->winId()),qRound(host_->width()*host_->devicePixelRatioF()),qRound(host_->height()*host_->devicePixelRatioF()),output_);
    auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this] {tick();});timer->start(50);
  }
  ~Editor() override {if(loader_.joinable()) loader_.join();renderer_.reset();}
  void load(const std::filesystem::path &file,bool preserve=false) {
    if(loading_) {statusBar()->showMessage(QStringLiteral("正在加载，请稍候…"));return;}
    loading_=true;load_error_.clear();open_->setEnabled(false);project_action_->setEnabled(false);statusBar()->showMessage(QStringLiteral("正在后台解析场景与参数依赖…"));
    const auto generation=++generation_;const auto roots=roots_;
    loader_=std::jthread([this,file,roots,generation,preserve] {
      try {
        auto document=std::make_shared<Document>();document->generation=generation;document->loaded=daz::load(file,{roots,false});
        std::vector<std::filesystem::path> resolved;for(const auto &p:document->loaded.report["content_roots"]) resolved.push_back(std::filesystem::u8path(p.get<std::string>()));
        document->catalog=daz::discover_morphs(document->loaded,resolved);
        document->skeletons=daz::load_skeletons(document->loaded);
        document->formulas=daz::enable_formulas(document->catalog,document->skeletons);
        std::ofstream(output_/"asset-report.json")<<document->loaded.report.dump(2);
        std::ofstream(output_/"morph-catalog.json")<<document->catalog.report.dump(2);
        std::ofstream(output_/"skeleton-report.json")<<document->skeletons.report.dump(2);
        std::ofstream(output_/"formula-report.json")<<document->formulas.report.dump(2);
        QMetaObject::invokeMethod(this,[this,document,preserve] {
          const auto old=document_;const auto previous=snapshot_;parameters_->bind(nullptr,nullptr);
          document_=document;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);snapshot_={};snapshot_.generation=document->generation;snapshot_.revision=1;
          frame_pending_=false;pose_report_=nullptr;pose_status_->setText(QStringLiteral("选中角色后，可从“姿势”菜单加载姿势 DUF。"));
          for(const auto &skin:document_->skeletons.skins) {
            auto pose=skin.initial;
            if(preserve&&old) for(size_t s=0;s<old->skeletons.skins.size();++s) if(old->skeletons.skins[s].id==skin.id) {
              std::map<std::string,runtime::JointPose> by_id;for(size_t j=0;j<old->skeletons.skins[s].joints.size();++j) by_id[old->skeletons.skins[s].joints[j].id]=previous.poses[s][j];
              for(size_t j=0;j<skin.joints.size();++j) if(by_id.contains(skin.joints[j].id)) pose[j]=by_id.at(skin.joints[j].id);
            }
            snapshot_.poses.push_back(std::move(pose));
          }
          for(const auto &target:document_->catalog.targets) {
            runtime::Properties values;for(const auto &m:target.morphs) values.morphs.push_back(m.evaluable||m.unsupported.empty()?m.initial:0);
            if(preserve && old) for(size_t t=0;t<old->catalog.targets.size();++t) if(old->catalog.targets[t].id==target.id) {
              values.transform=previous.values[t].transform;std::map<std::string,float> weights;
              for(size_t m=0;m<old->catalog.targets[t].morphs.size();++m) weights[old->catalog.targets[t].morphs[m].id]=previous.values[t].morphs[m];
              for(size_t m=0;m<target.morphs.size();++m) if(target.morphs[m].unsupported.empty() && weights.contains(target.morphs[m].id)) values.morphs[m]=target.morphs[m].clamped?std::clamp(weights[target.morphs[m].id],target.morphs[m].minimum,target.morphs[m].maximum):weights[target.morphs[m].id];
            }
            runtime::sync_aliases(target,values);snapshot_.values.push_back(std::move(values));
          }
          {QSignalBlocker block(hierarchy_);hierarchy_->clear();
            for(size_t i=0;i<document_->catalog.targets.size();++i) {auto *item=new QTreeWidgetItem(hierarchy_,{text(document_->catalog.targets[i].label)});item->setData(0,Qt::UserRole,int(i));item->setToolTip(0,text(document_->catalog.targets[i].id));
              for(const auto &skin:document_->skeletons.skins) if(skin.instance==document_->catalog.targets[i].instance) {
                std::vector<QTreeWidgetItem *> bones;for(const auto &joint:skin.joints) {auto *bone=joint.parent<0?item:new QTreeWidgetItem(bones[size_t(joint.parent)],{text(joint.label)});bone->setData(0,Qt::UserRole,int(i));bone->setToolTip(0,text(joint.name+" · "+joint.rotation_order));bones.push_back(bone);}
              }
            }}
          renderer_->set_document(document_,snapshot_);select(-1);hierarchy_->setCurrentItem(hierarchy_->topLevelItem(0));
          if(!pose_file_.empty()&&!pose_test_) {const auto file=pose_file_;pose_file_.clear();apply_pose_file(file);}
        },Qt::QueuedConnection);
      } catch(const std::exception &e) {
        const std::string error=e.what();QMetaObject::invokeMethod(this,[this,error] {loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);load_error_=QStringLiteral("加载失败：")+text(error);statusBar()->showMessage(load_error_);if(self_test_) finish_test(false,error);},Qt::QueuedConnection);
      }
    });
  }
};
}
int main(int argc,char **argv) {
  QApplication app(argc,argv);app.setApplicationName("DazFastViewer");app.setOrganizationName("DazFastViewer");app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"),9));
  QTranslator translator;if(translator.load("qt_zh_CN",app.applicationDirPath()+"/translations")) app.installTranslator(&translator);
  QCommandLineParser parser;parser.addHelpOption();parser.addOption({"file",QStringLiteral("启动后加载的 DUF"),"path"});
  parser.addOption({"content-root",QStringLiteral("内容库目录，可重复"),"directory"});parser.addOption({"output",QStringLiteral("输出目录"),"directory"});
  parser.addOption({"project",QStringLiteral("项目设置文件"),"file"});
  parser.addOption({"self-test",QStringLiteral("一次副屏编辑器验证后自动退出")});
  parser.addOption({"pose",QStringLiteral("加载角色后应用的单帧姿势 DUF"),"file"});
  parser.addOption({"pose-test",QStringLiteral("验证姿势、恢复与相机后自动退出"),"file"});
  parser.addOption({"formula-test",QStringLiteral("验证指定 Morph 滑块、ERC 与恢复后退出")});
  parser.addOption({"test-parameter",QStringLiteral("指定滑块验证参数，可重复，与 --formula-test 配合"),"name"});
  parser.addOption({"reload-test",QStringLiteral("验证后台场景替换后退出"),"file"});parser.process(app);
  const auto output=parser.isSet("output")?file_path(parser.value("output")):std::filesystem::path("artifacts")/("editor-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz").toStdString());
  std::filesystem::create_directories(output);
  try {
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);
    ccl::path_init(app.applicationDirPath().toStdString(),DFV_CYCLES_SOURCE);
    auto project=ProjectSettings::load(parser.isSet("project")?parser.value("project"):QDir(app.applicationDirPath()).absoluteFilePath("../DazFastViewer.project.json"));
    project.content_roots=ProjectSettings::normalize(parser.values("content-root")+project.content_roots);
    Editor editor(output,std::move(project),parser.isSet("self-test")||parser.isSet("reload-test"),parser.isSet("reload-test")?file_path(parser.value("reload-test")):std::filesystem::path{},
      parser.isSet("pose-test")?file_path(parser.value("pose-test")):parser.isSet("pose")?file_path(parser.value("pose")):std::filesystem::path{},parser.isSet("pose-test"),parser.isSet("formula-test"));
    editor.test_parameters(parser.values("test-parameter"));
    if(parser.isSet("file")) editor.load(file_path(parser.value("file")));
    return app.exec();
  } catch(const std::exception &e) {std::ofstream(output/"error.txt")<<e.what();return 1;}
}
