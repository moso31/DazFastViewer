#include "editor/renderer.h"
#include "editor/project.h"
#include "editor/parameters.h"
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
  bool loading_=false,self_test_=false;
  QString load_error_;
  int selected_=-1,test_stage_=0;
  size_t test_morph_=0;
  uint64_t generation_=0,test_evaluations_=0;
  double test_initial_displacement_=0;
  qint64 test_started_=QDateTime::currentMSecsSinceEpoch();
  QDockWidget *dock(const QString &title,QWidget *widget,Qt::DockWidgetArea area) {
    auto *d=new QDockWidget(title,this);d->setObjectName(title);d->setWidget(widget);addDockWidget(area,d);return d;
  }
  void send() {++snapshot_.revision;renderer_->edit(snapshot_);}
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
    if(current==next) return;current=next;
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
    for(size_t m=0;m<value.morphs.size();++m) value.morphs[m]=target.morphs[m].unsupported.empty()?target.morphs[m].initial:0;
    select(selected_);send();
  }
  void finish_test(bool pass,const std::string &error={}) {
    const auto status=renderer_->status();
    nlohmann::json report={{"status",pass?"PASS":"FAIL"},{"error",error},{"stage",test_stage_},
      {"mesh_creations",status.adapter.meshes},{"geometry_updates",status.adapter.geometry_updates},{"instance_updates",status.adapter.instance_updates},
      {"morph_evaluations",status.evaluation.morph_evaluations},{"max_displacement_m",status.max_displacement},
      {"qt_version",QT_VERSION_STR},{"monitor",screen()->name().toStdString()},{"window",{x(),y(),width(),height()}},
      {"scope","selected-object-morph-transform-reset-camera-no-morph-evaluation"}};
    if(!reload_file_.empty()) report["scope"]="background-scene-replacement-generation-isolation";
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
    if(!load_error_.isEmpty()) statusBar()->showMessage(load_error_);
    else if(!loading_) statusBar()->showMessage(QStringLiteral("OptiX · %1 samples · 网格 %2 · 顶点更新 %3 · 变换更新 %4").arg(state.samples).arg(state.adapter.meshes).arg(state.adapter.geometry_updates).arg(state.adapter.instance_updates));
    if(!self_test_) return;
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>180000) {finish_test(false,"等待编辑器验证超过 180 秒");return;}
    if(!document_ || state.generation!=document_->generation || state.presented_revision!=snapshot_.revision || state.presented_epoch!=state.requested_epoch || state.frames==0 || state.samples<8) return;
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
  Editor(const std::filesystem::path &output,ProjectSettings project,bool self_test,std::filesystem::path reload_file):project_(std::move(project)),output_(output),reload_file_(std::move(reload_file)),self_test_(self_test) {
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
    auto *reset=new QPushButton(QStringLiteral("重置选中对象"));properties->addWidget(reset);connect(reset,&QPushButton::clicked,this,[this] {reset_selected();});
    parameters_=new ParameterPanel;parameters_->changed=[this](size_t index,double value) {set_morph(index,value);};properties->addWidget(parameters_,1);
    dock(QStringLiteral("对象属性与 Morph"),panel,Qt::RightDockWidgetArea);
    auto *file_menu=menuBar()->addMenu(QStringLiteral("文件"));open_=file_menu->addAction(QStringLiteral("打开 DUF…"));open_->setShortcut(QKeySequence::Open);
    connect(open_,&QAction::triggered,this,[this] {const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("加载角色或场景"),{},QStringLiteral("DAZ 场景 (*.duf)"));if(!file.isEmpty()) load(file_path(file));});
    auto *project_menu=menuBar()->addMenu(QStringLiteral("项目"));project_action_=project_menu->addAction(QStringLiteral("项目设置…"));
    connect(project_action_,&QAction::triggered,this,[this] {project_settings();});
    connect(file_menu->addAction(QStringLiteral("退出")),&QAction::triggered,this,&QWidget::close);
    auto *edit=menuBar()->addMenu(QStringLiteral("编辑"));connect(edit->addAction(QStringLiteral("重置选中对象")),&QAction::triggered,this,[this] {reset_selected();});
    auto *view=menuBar()->addMenu(QStringLiteral("视图"));for(auto *d:findChildren<QDockWidget *>()) view->addAction(d->toggleViewAction());
    connect(explorer_,&QTreeView::doubleClicked,this,[this](const QModelIndex &index) {const auto file=files_->filePath(index);if(QFileInfo(file).suffix().compare("duf",Qt::CaseInsensitive)==0) load(file_path(file));});
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
        std::ofstream(output_/"asset-report.json")<<document->loaded.report.dump(2);
        std::ofstream(output_/"morph-catalog.json")<<document->catalog.report.dump(2);
        QMetaObject::invokeMethod(this,[this,document,preserve] {
          const auto old=document_;const auto previous=snapshot_;parameters_->bind(nullptr,nullptr);
          document_=document;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);snapshot_={};snapshot_.generation=document->generation;snapshot_.revision=1;
          for(const auto &target:document_->catalog.targets) {
            runtime::Properties values;for(const auto &m:target.morphs) values.morphs.push_back(m.unsupported.empty()?m.initial:0);
            if(preserve && old) for(size_t t=0;t<old->catalog.targets.size();++t) if(old->catalog.targets[t].id==target.id) {
              values.transform=previous.values[t].transform;std::map<std::string,float> weights;
              for(size_t m=0;m<old->catalog.targets[t].morphs.size();++m) weights[old->catalog.targets[t].morphs[m].id]=previous.values[t].morphs[m];
              for(size_t m=0;m<target.morphs.size();++m) if(target.morphs[m].unsupported.empty() && weights.contains(target.morphs[m].id)) values.morphs[m]=target.morphs[m].clamped?std::clamp(weights[target.morphs[m].id],target.morphs[m].minimum,target.morphs[m].maximum):weights[target.morphs[m].id];
            }
            snapshot_.values.push_back(std::move(values));
          }
          {QSignalBlocker block(hierarchy_);hierarchy_->clear();
            for(size_t i=0;i<document_->catalog.targets.size();++i) {auto *item=new QTreeWidgetItem(hierarchy_,{text(document_->catalog.targets[i].label)});item->setData(0,Qt::UserRole,int(i));item->setToolTip(0,text(document_->catalog.targets[i].id));}}
          renderer_->set_document(document_,snapshot_);select(-1);hierarchy_->setCurrentItem(hierarchy_->topLevelItem(0));
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
  parser.addOption({"reload-test",QStringLiteral("验证后台场景替换后退出"),"file"});parser.process(app);
  const auto output=parser.isSet("output")?file_path(parser.value("output")):std::filesystem::path("artifacts")/("editor-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz").toStdString());
  std::filesystem::create_directories(output);
  try {
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);
    ccl::path_init(app.applicationDirPath().toStdString(),DFV_CYCLES_SOURCE);
    auto project=ProjectSettings::load(parser.isSet("project")?parser.value("project"):QDir(app.applicationDirPath()).absoluteFilePath("../DazFastViewer.project.json"));
    project.content_roots=ProjectSettings::normalize(parser.values("content-root")+project.content_roots);
    Editor editor(output,std::move(project),parser.isSet("self-test")||parser.isSet("reload-test"),parser.isSet("reload-test")?file_path(parser.value("reload-test")):std::filesystem::path{});
    if(parser.isSet("file")) editor.load(file_path(parser.value("file")));
    return app.exec();
  } catch(const std::exception &e) {std::ofstream(output/"error.txt")<<e.what();return 1;}
}
