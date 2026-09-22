#include "editor/renderer.h"
#include "editor/project.h"
#include "editor/parameters.h"
#include "render_ir/options_json.h"
#include "daz/pose.h"
#include "runtime/picking.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QTreeView>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
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
#include <QSettings>
#include <QCloseEvent>
#include <QTreeWidgetItemIterator>
#include <psapi.h>
#include <fstream>
#include <cctype>
#include <limits>

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
  QCheckBox *visible_=nullptr;
  QCheckBox *manual_morph_=nullptr;
  QPushButton *refresh_parameters_=nullptr,*apply_parameters_=nullptr,*retry_parameters_=nullptr;
  struct PendingParameter {float value;bool unlimited;};
  std::map<std::pair<std::string,std::string>,PendingParameter> pending_parameters_;
  QLabel *pose_status_=nullptr;
  QAction *open_=nullptr;
  QAction *delete_=nullptr;
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
  uint64_t focus_requests_=0;
  bool focus_pending_=false;
  void focus_selection() {
    if(!renderer_||!document_) return;
    if(selected_light_>=0) {ir::Bounds b;const auto &m=snapshot_.lights.at(size_t(selected_light_)).transform;b.add(m.point({-.1f,-.1f,-.1f}));b.add(m.point({.1f,.1f,.1f}));renderer_->focus(b);focus_pending_=false;return;}
    const auto state=renderer_->status();
    focus_pending_=state.selection_generation!=document_->generation||state.selected_target!=selected_||state.selected_joint!=selected_joint_;
    if(!focus_pending_) renderer_->focus(state.selection_bounds);
  }
  bool formula_test_=false;
  bool lazy_test_=false;int lazy_wait_ticks_=0;size_t lazy_updates_=0;uint64_t lazy_generation_=0;
  bool workflow_test_=false;
  bool head_selection_test_=false;
  int head_test_joint_=-1,eye_test_joint_=-1,lip_test_joint_=-1,head_test_skin_=-1,probe_index_=0;
  QPoint probe_center_,probe_point_;
  uint64_t head_test_epoch_=0;
  size_t head_test_triangles_=0;
  bool options_test_=false;int options_wait_=0;CameraState options_camera_;size_t options_geometry_=0;uint64_t options_epoch_=0;
  bool navigation_test_=false;
  bool keep_open_after_test_=false;
  RenderStatus navigation_before_;
  CameraState navigation_after_input_;
  qint64 navigation_at_=0;
  nlohmann::json navigation_checks_=nlohmann::json::array();
  bool attachment_test_=false;std::vector<std::array<int,3>> attachment_cases_;nlohmann::json attachment_report_=nlohmann::json::array();
  bool capture_test_=false;
  bool capture_head_=false;
  int capture_samples_=16;
  double capture_seconds_=0; qint64 capture_started_=0;bool capture_timed_=false;
  nlohmann::json capture_records_=nlohmann::json::array();
  QString visibility_label_;
  int visibility_target_=-1;
  bool visibility_initial_=true;
  size_t visibility_geometry_updates_=0;
  QStringList capture_targets_;
  bool capture_front_=false;
  bool lifecycle_test_=false;
  std::filesystem::path lifecycle_first_,lifecycle_second_;
  std::weak_ptr<const Document> retired_document_;
  nlohmann::json lifecycle_samples_=nlohmann::json::array();
  size_t lifecycle_objects_=0;
  int lifecycle_rounds_=8;
  uint64_t workflow_click_=0;
  int workflow_target_=-1,workflow_joint_=-1;
  nlohmann::json hover_checks_=nlohmann::json::array();
  POINT workflow_cursor_{};
  std::vector<std::string> formula_names_={"Arms Length","Chest Scale","Eyes Closed","HS Sanny Shy","Flex Quad Left"};
  uint64_t formula_ui_revision_=0;
  size_t formula_case_=0;
  uint64_t test_formula_evaluations_=0;
  nlohmann::json parameter_checks_=nlohmann::json::array();
  bool loading_=false,self_test_=false;
  QString load_error_;
  int selected_=-1,selected_joint_=-1,selected_light_=-1,test_stage_=0;
  uint64_t clicks_=0;
  QSize viewport_size_;qint64 resize_at_=0;
  QByteArray default_layout_;
  QByteArray workflow_layout_;
  QDoubleSpinBox *light_power_=nullptr;
  ir::Transform light_base_;
  size_t test_morph_=0;
  uint64_t generation_=0,test_evaluations_=0;
  uint64_t test_skin_evaluations_=0;
  nlohmann::json pose_report_;
  double test_initial_displacement_=0;
  qint64 test_started_=QDateTime::currentMSecsSinceEpoch();
  QDockWidget *dock(const QString &title,QWidget *widget,Qt::DockWidgetArea area) {
    auto *d=new QDockWidget(title,this);d->setObjectName(title);d->setWidget(widget);addDockWidget(area,d);return d;
  }
  Snapshot submitted_snapshot() const {
    auto submitted=snapshot_;
    if(document_&&!pending_parameters_.empty()) for(size_t t=0;t<document_->catalog.targets.size();++t) {
      const auto &target=document_->catalog.targets[t];for(size_t m=0;m<target.morphs.size();++m)
        if(auto p=pending_parameters_.find({target.id,target.morphs[m].id});p!=pending_parameters_.end()) {submitted.values[t].morphs[m]=p->second.value;if(!p->second.unlimited) submitted.values[t].unlimited_morphs.erase(target.morphs[m].id);}
      runtime::sync_aliases(target,submitted.values[t]);
    }
    return submitted;
  }
  void send() {++snapshot_.revision;renderer_->edit(submitted_snapshot());}
  void prune_pending_parameters() {
    std::erase_if(pending_parameters_,[&](const auto &p) {for(const auto &t:document_->catalog.targets) if(t.id==p.first.first) for(const auto &m:t.morphs) if(m.id==p.first.second&&m.unsupported.empty()) return false;return true;});
    apply_parameters_->setEnabled(!pending_parameters_.empty());
  }
  void apply_parameters() {if(pending_parameters_.empty()) return;pending_parameters_.clear();apply_parameters_->setEnabled(false);send();}
  void refresh_parameter_catalog() {
    if(loading_||!document_||selected_<0) return;
    const auto previous=document_;const auto selected=size_t(selected_);const auto roots=roots_;loading_=true;refresh_parameters_->setEnabled(false);
    progress(QStringLiteral("正在刷新所选对象及穿戴物的参数目录…"));
    loader_=std::jthread([this,previous,selected,roots](std::stop_token stop) {
      try {
        auto updated=editor::refresh_parameters(*previous,selected,roots,[this,stop](const std::string &message){if(stop.stop_requested()) throw std::runtime_error("已取消刷新");progress(text(message));});
        QMetaObject::invokeMethod(this,[this,previous,updated] {
          loading_=false;if(document_!=previous) return;
          const auto old_values=snapshot_.values;parameters_->bind(nullptr,nullptr);
          for(size_t t=0;t<updated->catalog.targets.size();++t) {
            std::map<std::string,float> weights;for(size_t m=0;m<previous->catalog.targets[t].morphs.size();++m) weights[previous->catalog.targets[t].morphs[m].id]=old_values[t].morphs[m];
            auto &values=snapshot_.values[t];values.morphs.clear();for(const auto &m:updated->catalog.targets[t].morphs) values.morphs.push_back(weights.contains(m.id)?weights.at(m.id):(m.evaluable?m.initial:0));runtime::sync_aliases(updated->catalog.targets[t],values);
          }
          document_=updated;prune_pending_parameters();++snapshot_.revision;renderer_->set_document(document_,submitted_snapshot(),false);select(selected_,selected_joint_,selected_light_);
          statusBar()->showMessage(QStringLiteral("参数目录已刷新，已保留当前输入和姿势"));
        },Qt::QueuedConnection);
      } catch(const std::exception &e) {const std::string error=e.what();QMetaObject::invokeMethod(this,[this,error] {loading_=false;load_error_=QStringLiteral("参数刷新失败：")+text(error);},Qt::QueuedConnection);}
    });
  }
  void progress(const QString &message) {QMetaObject::invokeMethod(this,[this,message] {statusBar()->showMessage(message);},Qt::QueuedConnection);}
  void choose(int target,int joint=-1) {
    for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) if((*it)->data(0,Qt::UserRole).toInt()==target&&(*it)->data(0,Qt::UserRole+1).toInt()==joint&&(*it)->data(0,Qt::UserRole+2).toInt()<0) {
      for(auto *p=(*it)->parent();p;p=p->parent()) p->setExpanded(true);hierarchy_->setCurrentItem(*it);hierarchy_->scrollToItem(*it);if(selected_!=target||selected_joint_!=joint) select(target,joint);return;
    }
    hierarchy_->setCurrentItem(nullptr);select(-1);
  }
  void rebuild_hierarchy() {
    QSignalBlocker block(hierarchy_);hierarchy_->clear();
    std::map<std::string,QTreeWidgetItem *> objects;
    auto identify=[](QTreeWidgetItem *item,int target,int joint=-1,int light=-1) {item->setData(0,Qt::UserRole,target);item->setData(0,Qt::UserRole+1,joint);item->setData(0,Qt::UserRole+2,light);};
    std::vector<QTreeWidgetItem *> items;
    for(size_t i=0;i<document_->catalog.targets.size();++i) {
      const auto &target=document_->catalog.targets[i];auto *item=new QTreeWidgetItem(hierarchy_,{text(target.label)});identify(item,int(i));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,snapshot_.values[i].visible?Qt::Checked:Qt::Unchecked);items.push_back(item);
      for(const auto &object:document_->loaded.objects) if(object.instance==target.instance) objects[object.id]=item;
      for(const auto &skin:document_->skeletons.skins) if(skin.instance==target.instance) {
        std::vector<QTreeWidgetItem *> bones;
        for(size_t j=0;j<skin.joints.size();++j) {const auto &joint=skin.joints[j];auto *bone=joint.parent<0?item:new QTreeWidgetItem(bones[size_t(joint.parent)],{text(joint.label)});if(joint.parent>=0) identify(bone,int(i),int(j));bones.push_back(bone);if(!joint.scene_id.empty()) objects.emplace(joint.scene_id,bone);}
      }
    }
    for(size_t i=0;i<items.size();++i) {
      const auto &target=document_->catalog.targets[i];auto ancestors=target.ancestors;if(ancestors.empty()) ancestors.push_back(target.parent);
      for(const auto &parent:ancestors) if(parent.starts_with('#')&&objects.contains(parent.substr(1))&&objects[parent.substr(1)]!=items[i]) {
        hierarchy_->takeTopLevelItem(hierarchy_->indexOfTopLevelItem(items[i]));objects[parent.substr(1)]->addChild(items[i]);break;
      }
    }
    for(int option=0;option<2;++option) {const auto &node=option==0?snapshot_.options.environment:snapshot_.options.tonemapper;if(!node.id.empty()) {auto *item=new QTreeWidgetItem(hierarchy_,{text(node.label)});identify(item,option==0?-2:-3);}}
    for(size_t i=0;i<snapshot_.lights.size();++i) {auto *item=new QTreeWidgetItem(hierarchy_,{QStringLiteral("灯光 · ")+text(snapshot_.lights[i].id)});identify(item,-1,-1,int(i));}
  }
  void set_visible(size_t target,bool visible) {
    if(loading_||!document_||snapshot_.values.at(target).visible==visible) return;
    snapshot_.values[target].visible=visible;
    {QSignalBlocker block(hierarchy_);for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) if((*it)->data(0,Qt::UserRole).toInt()==int(target)&&(*it)->data(0,Qt::UserRole+1).toInt()<0) (*it)->setCheckState(0,visible?Qt::Checked:Qt::Unchecked);}
    if(selected_==int(target)) {QSignalBlocker block(visible_);visible_->setChecked(visible);}send();
  }
  void add_light() {
    if(loading_) return;
    auto next=document_?std::make_shared<Document>(*document_):std::make_shared<Document>();next->generation=++generation_;
    next->loaded.scene.lights=snapshot_.lights;
    ir::AreaLight light;size_t id=next->generation;auto exists=[&] {return std::any_of(next->loaded.scene.lights.begin(),next->loaded.scene.lights.end(),[&](const auto &old) {return old.id==light.id;});};
    do {light.id="Area "+std::to_string(id++);} while(exists());light.transform=ir::Transform::translate({0,-2,3});
    next->loaded.scene.lights.push_back(light);document_=next;snapshot_.generation=next->generation;snapshot_.lights=next->loaded.scene.lights;++snapshot_.revision;
    rebuild_hierarchy();renderer_->set_document(document_,submitted_snapshot(),false);hierarchy_->setCurrentItem(hierarchy_->topLevelItem(hierarchy_->topLevelItemCount()-1));
  }
  void delete_selection() {
    if(loading_||!document_||(selected_<0&&selected_light_<0)||selected_joint_>=0) return;
    try {
      auto next=std::make_shared<Document>(*document_);auto snapshot=snapshot_;size_t removed=0;
      if(selected_light_>=0) removed=remove_light(*next,snapshot,size_t(selected_light_));
      else removed=remove_target(*next,snapshot,size_t(selected_));
      next->generation=++generation_;snapshot.generation=next->generation;
      parameters_->bind(nullptr,nullptr);document_=std::move(next);snapshot_=std::move(snapshot);
      select(-1);rebuild_hierarchy();frame_pending_=false;pose_report_=nullptr;pose_status_->clear();load_error_.clear();
      prune_pending_parameters();renderer_->set_document(document_,submitted_snapshot(),false);
      statusBar()->showMessage(removed?QStringLiteral("已删除 %1 个对象及其关联资源").arg(removed):QStringLiteral("已删除灯光"));
    } catch(const std::exception &e) {QMessageBox::warning(this,QStringLiteral("无法删除对象"),text(e.what()));}
  }
  void clear_scene() {
    if(loading_) return;
    pending_parameters_.clear();apply_parameters_->setEnabled(false);
    parameters_->bind(nullptr,nullptr);document_=std::make_shared<Document>();document_->generation=++generation_;
    snapshot_=initial_snapshot(*document_);select(-1);rebuild_hierarchy();frame_pending_=false;pose_report_=nullptr;pose_status_->clear();load_error_.clear();
    renderer_->set_document(document_,snapshot_,false);
  }
  void apply_combined_file(const std::filesystem::path &file,const nlohmann::json &data) {
    if(loading_||!document_||selected_<0) throw std::runtime_error("请先选中复合预设的目标对象");
    const auto skin=selected_skin();if(skin<0) throw std::runtime_error("姿势 / 形态通道需要带骨骼的目标角色");
    auto next=std::make_shared<Document>(*document_);auto snapshot=snapshot_;
    auto applied=daz::apply_pose(daz::parse_pose(data),next->skeletons.skins.at(size_t(skin)),snapshot.poses.at(size_t(skin)),next->catalog.targets.at(size_t(selected_)),snapshot.values.at(size_t(selected_)));
    apply_materials(*next,size_t(selected_),daz::load(file,{roots_,false}));
    snapshot.poses[size_t(skin)]=std::move(applied.joints);snapshot.values[size_t(selected_)]=std::move(applied.properties);
    next->generation=++generation_;snapshot.generation=next->generation;++snapshot.revision;next->loaded.scene.lights=snapshot.lights;
    parameters_->bind(nullptr,nullptr);document_=std::move(next);snapshot_=std::move(snapshot);pose_report_=std::move(applied.report);
    std::ofstream(output_/"pose-report.json")<<pose_report_.dump(2);
    pose_status_->setText(QStringLiteral("已应用复合 DUF 的材质与姿势 / 形态；%1 项通道未应用，可查看详情。").arg(pose_report_["unapplied"].size()));
    select(selected_,selected_joint_);renderer_->set_document(document_,submitted_snapshot(),false);
  }
  void apply_material_file(const std::filesystem::path &file) {
    if(loading_||selected_<0||!document_) throw std::runtime_error("请先选中材质预设的目标对象");
    auto preset=daz::load(file,{roots_,false});auto next=std::make_shared<Document>(*document_);next->generation=++generation_;
    apply_materials(*next,size_t(selected_),preset);
    next->loaded.scene.lights=snapshot_.lights;document_=next;snapshot_.generation=next->generation;++snapshot_.revision;select(selected_,selected_joint_);renderer_->set_document(document_,submitted_snapshot(),false);
  }
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
      pose_status_->setText(QStringLiteral("预设：%1\n已应用 %2 个骨骼通道、%3 个 Morph 通道；%4 项未应用。%5").arg(QString::fromStdWString(file.stem().wstring())).arg(pose_report_["applied_bone_channels"].get<int>()).arg(pose_report_["applied_morph_channels"].get<int>()).arg(skipped).arg(skipped?QStringLiteral("点击下方查看详情。") : QString()));
      pose_status_->setToolTip(QString::fromStdWString(file.wstring()));select(selected_,selected_joint_);send();frame_pending_=self_test_;return true;
    } catch(const std::exception &e) {
      if(self_test_) finish_test(false,e.what());else QMessageBox::warning(this,QStringLiteral("无法应用预设"),text(e.what()));return false;
    }
  }
  void reset_pose() {
    if(loading_) return;const auto index=selected_skin();if(index<0) return;
    snapshot_.poses[size_t(index)]=document_->skeletons.skins[size_t(index)].initial;send();frame_pending_=true;
    pose_status_->setText(QStringLiteral("已恢复载入时的骨骼姿势；Morph 保持当前值。"));pose_report_=nullptr;
  }
  void open_asset(const std::filesystem::path &file) {
    try {const auto data=daz::read_document_file(file);
      const auto contents=daz::inspect_contents(data);
      if(contents.instantiate) load(file,false,document_!=nullptr);
      else if(contents.properties&&contents.materials) apply_combined_file(file,data);
      else if(contents.properties) apply_pose_file(file);
      else if(contents.materials) apply_material_file(file);
      else throw std::runtime_error("DUF 中没有当前可实例化或应用的内容；资产定义需要 scene 实例引用");
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
      update_libraries();statusBar()->showMessage(QStringLiteral("内容库设置已保存，将用于后续资产加载。"));
    }
  }
  void set_morph(size_t morph,double value) {
    if(selected_<0) return;
    auto &current=snapshot_.values[size_t(selected_)].morphs[morph];const float next=float(value);
    if(current==next) return;
    const auto &target=document_->catalog.targets[size_t(selected_)];const auto canonical=target.morphs[morph].alias_morph>=0?size_t(target.morphs[morph].alias_morph):morph;
    const auto key=std::make_pair(target.id,target.morphs[canonical].id);
    if(manual_morph_->isChecked()) pending_parameters_.try_emplace(key,PendingParameter{snapshot_.values[size_t(selected_)].morphs[canonical],snapshot_.values[size_t(selected_)].unlimited_morphs.contains(target.morphs[canonical].id)});
    runtime::set_parameter(target,snapshot_.values[size_t(selected_)],morph,next);
    parameters_->refresh(morph);
    if(manual_morph_->isChecked()) {if(pending_parameters_.at(key).value==snapshot_.values[size_t(selected_)].morphs[canonical]) {if(!pending_parameters_.at(key).unlimited) snapshot_.values[size_t(selected_)].unlimited_morphs.erase(target.morphs[canonical].id);pending_parameters_.erase(key);}apply_parameters_->setEnabled(!pending_parameters_.empty());return;}
    send();
  }
  void select(int index,int joint=-1,int light=-1) {
    {QSignalBlocker block(visible_);visible_->setEnabled(document_&&index>=0&&joint<0&&light<0);visible_->setChecked(document_&&index>=0?snapshot_.values.at(size_t(index)).visible:false);}
    selected_=index;selected_joint_=joint;selected_light_=light;light_power_->setVisible(light>=0);
    if(delete_) delete_->setEnabled(!loading_&&document_&&joint<0&&(index>=0||light>=0));
    if(renderer_) renderer_->select(document_?document_->generation:0,light<0?index:-1,joint);
    if(light>=0&&document_) {
      const auto &l=snapshot_.lights.at(size_t(light));light_base_=l.transform;light_base_.value[3]=light_base_.value[7]=light_base_.value[11]=0;selection_->setText(text(l.id));parameters_->bind(nullptr,nullptr);
      const float data[]={l.transform.value[3]*100,l.transform.value[11]*100,-l.transform.value[7]*100,0,0,0,100,100,100};
      for(int i=0;i<9;++i) {QSignalBlocker block(transform_[i]);transform_[i]->setValue(data[i]);transform_[i]->setEnabled(i<6);}
      std::vector<ParameterControl> light_controls;for(int i=0;i<6;++i) {ParameterControl c;c.id="light/"+std::to_string(i);c.label=std::string(1,"XYZ"[i%3])+(i<3?" Translate":" Rotate");c.group=i<3?"/General/Transforms/Translation":"/General/Transforms/Rotation";c.read=[this,i]{return transform_[i]->value();};c.write=[this,i](double v){transform_[i]->setValue(v);};c.slider_minimum=-180;c.slider_maximum=180;light_controls.push_back(std::move(c));}parameters_->bind_controls(std::move(light_controls));
      QSignalBlocker block(light_power_);light_power_->setValue(std::max({l.power.x,l.power.y,l.power.z}));return;
    }
    if(document_&&(index==-2||index==-3)) {
      auto *node=index==-2?&snapshot_.options.environment:&snapshot_.options.tonemapper;selection_->setText(text(node->label));
      parameters_->bind_options(node,[this,node](size_t p,size_t c,double v){try {ir::set_option(*node,p,c,v);send();}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);}});return;
    }
    if(index<0 || !document_) {selection_->setText(QStringLiteral("请先选择场景对象"));parameters_->bind(nullptr,nullptr);for(auto *spin:transform_) spin->setEnabled(false);return;}
    const auto &target=document_->catalog.targets[size_t(index)];selection_->setText(text(target.label));
    const auto &values=snapshot_.values[size_t(index)];
    const auto &t=values.transform;
    const float data[]={t.translation_cm.x,t.translation_cm.y,t.translation_cm.z,t.rotation_degrees.x,t.rotation_degrees.y,t.rotation_degrees.z,t.scale.x*100,t.scale.y*100,t.scale.z*100};
    for(int i=0;i<9;++i) {QSignalBlocker block(transform_[i]);transform_[i]->setValue(data[i]);transform_[i]->setEnabled(true);}
    std::string node;
    if(joint>=0) {const auto skin=selected_skin();if(skin>=0) {const auto &j=document_->skeletons.skins[size_t(skin)].joints.at(size_t(joint));node=j.id;selection_->setText(text(target.label+" / "+j.label));}}
    if(joint>=0) for(auto *spin:transform_) spin->setEnabled(false);
    std::vector<ParameterControl> controls;
    const daz::AssetObject *asset=nullptr;for(const auto &o:document_->loaded.objects) if(o.instance==target.instance) {asset=&o;break;}
    const float saved[]={asset?asset->translation_cm.x:0,asset?asset->translation_cm.y:0,asset?asset->translation_cm.z:0,asset?asset->rotation_degrees.x:0,asset?asset->rotation_degrees.y:0,asset?asset->rotation_degrees.z:0,asset?asset->scale.x:1,asset?asset->scale.y:1,asset?asset->scale.z:1};
    if(joint<0) {
      ParameterControl general;general.id="transform/general_scale";general.label="Scale（%）";general.group="/General/Transforms/Scale";general.minimum=.01;general.maximum=10000;general.slider_minimum=1;general.slider_maximum=300;const float initial=asset?asset->general_scale:1;general.read=[this,initial]{return initial*snapshot_.values[size_t(selected_)].transform.general_scale*100;};general.write=[this,initial](double value){snapshot_.values[size_t(selected_)].transform.general_scale=float(value/(initial*100));send();};controls.push_back(std::move(general));
      for(int i=0;i<9;++i) {ParameterControl c;c.id="transform/"+std::to_string(i);c.label=std::string(1,"XYZ"[i%3])+std::string(i<3?" Translate（厘米）":i<6?" Rotate（度）":" Scale（%）");c.group=i<3?"/General/Transforms/Translation":i<6?"/General/Transforms/Rotation":"/General/Transforms/Scale";c.minimum=transform_[i]->minimum();c.maximum=transform_[i]->maximum();c.step=.1;c.slider_minimum=i<3?-200:i<6?-180:1;c.slider_maximum=i<3?200:i<6?180:300;const float base=saved[i];c.read=[this,i,base]{return i<6?transform_[i]->value()+base:transform_[i]->value()*base;};c.write=[this,i,base](double v){transform_[i]->setValue(i<6?v-base:v/base);};controls.push_back(std::move(c));}
    } else if(const int skin=selected_skin();skin>=0) {
      for(int i=0;i<9;++i) {ParameterControl c;c.id="joint/"+std::to_string(i);c.label=std::string(1,"XYZ"[i%3])+(i<3?" Translate":i<6?" Rotate":" Scale");c.group=i<3?"/General/Transforms/Translation":i<6?"/General/Transforms/Rotation":"/General/Transforms/Scale";c.enabled=false;c.detail="已保存的骨骼通道；当前通过姿势预设 / ERC 编辑";c.read=[this,skin,joint,i]{const auto &p=snapshot_.poses[size_t(skin)][size_t(joint)];const auto v=i<3?p.translation_cm:i<6?p.rotation_degrees:p.scale;return double(i%3==0?v.x:i%3==1?v.y:v.z);};controls.push_back(std::move(c));}
    }
    parameters_->set_extra(std::move(controls));parameters_->bind(&target,&values,node);
  }
  void reset_selected() {
    if(selected_<0) return;
    auto &value=snapshot_.values[size_t(selected_)];value.transform={};value.unlimited_morphs.clear();
    const auto &target=document_->catalog.targets[size_t(selected_)];
    std::erase_if(pending_parameters_,[&](const auto &p){return p.first.first==target.id;});apply_parameters_->setEnabled(!pending_parameters_.empty());
    for(size_t m=0;m<value.morphs.size();++m) value.morphs[m]=target.morphs[m].evaluable||target.morphs[m].unsupported.empty()?target.morphs[m].initial:0;
    runtime::sync_aliases(target,value);
    const auto skin=selected_skin();if(skin>=0) {snapshot_.poses[size_t(skin)]=document_->skeletons.skins[size_t(skin)].initial;frame_pending_=true;pose_status_->setText(QStringLiteral("已重置选中角色的姿势与形态。"));pose_report_=nullptr;}
    select(selected_);send();
  }
  void finish_test(bool pass,const std::string &error={}) {
    if(workflow_test_) SetCursorPos(workflow_cursor_.x,workflow_cursor_.y);
    const auto status=renderer_->status();
    nlohmann::json report={{"status",pass?"PASS":"FAIL"},{"error",error},{"stage",test_stage_},
      {"bone_attachments",attachment_report_},{"displayed_samples",status.samples},{"denoise",false},{"sampling_report","editor-render.json"},
      {"render_options_test",options_test_},{"lazy_test",lazy_test_},{"asset_revision",document_?document_->asset_revision:0},
      {"mesh_creations",status.adapter.meshes},{"curves",status.adapter.curves},{"geometry_updates",status.adapter.geometry_updates},{"instance_updates",status.adapter.instance_updates},
      {"morph_evaluations",status.evaluation.morph_evaluations},{"max_displacement_m",status.max_displacement},
      {"skin_evaluations",status.skinning.evaluations},{"skin_vertices",status.skinning.vertices},
      {"conform_bound_vertices",status.conform.bindings},{"conform_authored_morphs",status.conform.authored_morphs},{"conform_evaluations",status.conform.evaluations},
      {"formula_evaluations",status.formulas.expressions},{"formula_channels",status.formulas.channels},
      {"qt_version",QT_VERSION_STR},{"monitor",screen()->name().toStdString()},{"window",{x(),y(),width(),height()}},
      {"scope","selected-object-morph-transform-reset-camera-no-morph-evaluation"}};
    if(workflow_test_) {report["scope"]="raycast-body-part-tree-head-morph-hover-resize-layout";report["viewport"]={status.width,status.height};report["hovered_instance"]=status.hovered;report["hovered_joint"]=status.hovered_joint;report["hovered_triangles"]=status.hovered_triangles;report["selected_joint"]=selected_joint_;report["hover_checks"]=hover_checks_;}
    if(head_selection_test_) report["scope"]="figure-head-detail-and-bound-clothing-picking";
    if(navigation_test_) {report["scope"]="navigation-preview-and-refinement";report["checks"]=navigation_checks_;}
    if(capture_test_) {report["scope"]="scene-render";report["instances"]=document_->catalog.targets.size();report["skins"]=document_->skeletons.skins.size();}
    if(!visibility_label_.isEmpty()) {report["scope"]="property-and-hierarchy-visibility-toggle-restore";report["visible"]=status.visible;}
    if(lifecycle_test_) {report["scope"]="append-delete-clear-replace-resource-lifetime-and-render-error-recovery";report["samples"]=lifecycle_samples_;report["retired_document_expired"]=retired_document_.expired();}
    if(!reload_file_.empty()) report["scope"]="background-scene-replacement-generation-isolation";
    if(pose_test_) report["scope"]="pose-apply-reset-camera-no-skin-evaluation";
    if(formula_test_) {report["scope"]="parameter-slider-regression-with-ERC-JCM";report["parameter_checks"]=parameter_checks_;}
    report["project_file"]=project_.file.toUtf8().toStdString();report["content_roots"]=nlohmann::json::array();
    for(const auto &root:project_.content_roots) report["content_roots"].push_back(root.toUtf8().toStdString());
    report["named_parameters"]=nlohmann::json::array();
    if(document_) for(const auto &target:document_->catalog.targets) for(const auto &m:target.morphs)
      if(m.label=="Arms Length" || m.label=="Chest Scale" || m.label=="Eyes Closed" || m.label=="HS Sanny Shy")
        report["named_parameters"].push_back({{"label",m.label},{"group",m.group},{"kind",m.kind},{"source",m.source}});
    std::ofstream(output_/"editor-check.json")<<report.dump(2);
    if(keep_open_after_test_&&navigation_test_) {
      navigation_test_=self_test_=false;focus_pending_=false;
      const auto viewport=FindWindowExW(reinterpret_cast<HWND>(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
      if(viewport) SendMessageW(viewport,WM_CANCELMODE,0,0);
      if(document_&&!status.bounds.empty()) {choose(0);renderer_->frame(status.bounds.front());}
      statusBar()->showMessage(pass?QStringLiteral("导航验收通过，可以直接体验。") : text(error));return;
    }
    QApplication::exit(pass?0:1);
  }
  void head_selection_tick(const RenderStatus &state) {
    auto &skin=document_->skeletons.skins.at(size_t(std::max(0,head_test_skin_)));
    std::ofstream(output_/"selection-progress.json")<<nlohmann::json({{"stage",test_stage_},{"camera_epoch",state.camera.epoch},{"frame_epoch",head_test_epoch_},{"selected_target",state.selected_target},{"selected_joint",state.selected_joint},{"requested_pointer",{probe_point_.x(),probe_point_.y()}},{"actual_pointer",{state.pointer_x,state.pointer_y}},{"detail",state.hovered_detail_joint},{"probe",probe_index_}}).dump();
    auto position=[&](int joint) {const auto &instance=document_->loaded.scene.instances[skin.instance];const auto &mesh=document_->loaded.scene.meshes[instance.mesh];const auto regions=runtime::joint_regions(mesh,skin);ir::Bounds bounds;
      for(size_t t=0;t<mesh.triangles.size();++t) {const auto &face=mesh.triangles[t];const bool matches=joint==lip_test_joint_?face.material_slot<mesh.material_slots.size()&&mesh.material_slots[face.material_slot]=="Lips":(joint==regions.head?regions.body[t]:regions.detail[t])==joint;
        if(matches) for(auto v:face.vertices) bounds.add(instance.transform.point(mesh.positions[v]));}
      if(bounds.empty) throw std::runtime_error("测试部位没有几何区域");return bounds.center();};
    auto project=[&](ir::Vec3 p) {const auto m=state.camera.matrix();p={p.x-m[3],p.y-m[7],p.z-m[11]};const float z=m[2]*p.x+m[6]*p.y+m[10]*p.z,e=std::tan(.4f);return QPoint(qRound((1+(m[0]*p.x+m[4]*p.y+m[8]*p.z)/z/e/std::max(1.f,float(state.width)/state.height))*state.width*.5f-.5f),qRound((1-(m[1]*p.x+m[5]*p.y+m[9]*p.z)/z/e/std::max(1.f,float(state.height)/state.width))*state.height*.5f-.5f));};
    auto move_probe=[&] {const int n=probe_index_++;const int x=n==0?0:((n-1)%21-10)*4,y=n==0?0:((n-1)/21-10)*4;probe_point_=probe_center_+QPoint(x,y);probe_point_.setX(std::clamp(probe_point_.x(),1,state.width-2));probe_point_.setY(std::clamp(probe_point_.y(),1,state.height-2));renderer_->pointer(probe_point_.x(),probe_point_.y());};
    auto at_probe=[&] {return state.pointer_x==probe_point_.x()&&state.pointer_y==probe_point_.y();};
    auto click=[&] {workflow_click_=state.clicks;renderer_->pointer(probe_point_.x(),probe_point_.y(),true);};
    auto record=[&](const char *name) {hover_checks_.push_back({{"mode",name},{"selected_target",selected_},{"selected_joint",selected_joint_},{"hovered_joint",state.hovered_joint},{"highlight_triangles",state.hovered_triangles}});};
    auto lip=[&](int joint) {if(joint<0||size_t(joint)>=skin.joints.size()) return false;const auto &name=skin.joints[size_t(joint)].id;return name.find("LipUpper")!=std::string::npos||name.find("LipLower")!=std::string::npos;};
    if(test_stage_==0) {
      head_test_skin_=0;workflow_target_=0;for(size_t t=0;t<document_->catalog.targets.size();++t) if(document_->catalog.targets[t].instance==skin.instance) workflow_target_=int(t);
      for(size_t j=0;j<skin.joints.size();++j) {const auto &id=skin.joints[j].id;if(id=="head") head_test_joint_=int(j);if(id=="lEye") eye_test_joint_=int(j);if(id=="LipUpperMiddle") lip_test_joint_=int(j);}
      if(head_test_joint_<0||eye_test_joint_<0||lip_test_joint_<0) {finish_test(false,"头部分级测试缺少头 / 眼 / 唇节点");return;}
      choose(-1);auto c=position(head_test_joint_);ir::Bounds face;face.add({c.x-.14f,c.y-.14f,c.z-.19f});face.add({c.x+.14f,c.y+.14f,c.z+.19f});head_test_epoch_=state.camera.epoch;renderer_->frame(face);
      test_evaluations_=state.evaluation.morph_evaluations;test_skin_evaluations_=state.skinning.evaluations;test_stage_=1;
    } else if(test_stage_==1) {
      if(state.camera.epoch==head_test_epoch_||state.selected_target!=-1) return;probe_center_=project(position(eye_test_joint_));probe_index_=0;move_probe();test_stage_=2;
    } else if(test_stage_==2) {
      if(!at_probe()) return;if(state.hovered_detail_joint!=eye_test_joint_) {if(probe_index_>441) {finish_test(false,"头部近景没有命中眼球");return;}move_probe();return;}
      if(state.hovered_joint!=-1) {finish_test(false,"第一次眼球射线没有预选角色整体");return;}record("eye_first_hit_figure");click();test_stage_=3;
    } else if(test_stage_==3) {
      if(state.clicks<=workflow_click_||state.selected_target!=workflow_target_) return;
      if(selected_!=workflow_target_||selected_joint_!=-1||state.hovered_joint!=head_test_joint_) {finish_test(false,"人物选中后眼球悬停没有聚合整个头部");return;}
      const auto regions=runtime::joint_regions(document_->loaded.scene.meshes[document_->loaded.scene.instances[skin.instance].mesh],skin);
      head_test_triangles_=size_t(std::count(regions.body.begin(),regions.body.end(),head_test_joint_));
      if(state.hovered_triangles!=head_test_triangles_) {finish_test(false,"Head 高亮没有覆盖完整头部及子节点");return;}
      record("second_hit_whole_head");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"head-whole.png").wstring()));click();test_stage_=4;
    } else if(test_stage_==4) {
      if(state.clicks<=workflow_click_||state.selected_joint!=head_test_joint_) return;
      if(selected_joint_!=head_test_joint_||state.hovered_joint!=eye_test_joint_||state.hovered_triangles>=head_test_triangles_||!state.hovered_triangles) {finish_test(false,"选中 Head 后没有进入眼球细分");return;}
      record("third_hit_eye");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"head-eye.png").wstring()));click();test_stage_=5;
    } else if(test_stage_==5) {
      if(state.clicks<=workflow_click_||state.selected_joint!=eye_test_joint_) return;if(selected_joint_!=eye_test_joint_) {finish_test(false,"第三次点击未选中眼球");return;}
      record("eye_selected");choose(workflow_target_,head_test_joint_);probe_center_=project(position(lip_test_joint_));probe_index_=0;move_probe();test_stage_=6;
    } else if(test_stage_==6) {
      if(!at_probe()||state.selected_joint!=head_test_joint_) return;
      if(!lip(state.hovered_joint)) {if(probe_index_>441) {finish_test(false,"Head 选中后不能命中嘴唇");return;}move_probe();return;}
      lip_test_joint_=state.hovered_joint;record("head_to_lip");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"head-lip.png").wstring()));click();test_stage_=7;
    } else if(test_stage_==7) {
      if(state.clicks<=workflow_click_) return;if(selected_joint_!=lip_test_joint_) {finish_test(false,"嘴唇射线与场景树节点不一致");return;}
      record("lip_selected");int garment=-1;for(size_t t=0;t<document_->catalog.targets.size();++t) if(!document_->catalog.targets[t].conform_target.empty()) {garment=int(t);break;}
      if(garment<0) {finish_test(true);return;}choose(garment);if(selected_!=garment) {finish_test(false,"绑定服装无法通过场景树选择");return;}
      record("bound_clothing_tree_selection");head_test_epoch_=state.camera.epoch;renderer_->frame(state.bounds.at(size_t(workflow_target_)));test_stage_=8;
    } else if(test_stage_==8) {
      if(state.camera.epoch==head_test_epoch_) return;probe_point_={state.width/2,state.height/2};renderer_->pointer(probe_point_.x(),probe_point_.y());test_stage_=9;
    } else if(test_stage_==9) {
      if(!at_probe()) return;if(state.hovered!=int(skin.instance)) {finish_test(false,"绑定服装阻挡了角色射线");return;}
      record("ray_through_bound_clothing");click();test_stage_=10;
    } else if(test_stage_==10) {
      if(state.clicks<=workflow_click_) return;if(selected_!=workflow_target_||selected_joint_!=-1) {finish_test(false,"点击穿戴区域没有选择角色");return;}
      if(state.evaluation.morph_evaluations!=test_evaluations_||state.skinning.evaluations!=test_skin_evaluations_) {finish_test(false,"分级选择触发了变形");return;}record("body_selected_through_clothing");finish_test(true);
    }
  }
  void lifecycle_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000) {finish_test(false,"场景生命周期验证超时");return;}
    if(loading_||!document_||state.generation!=document_->generation) return;
    const int recovery=lifecycle_rounds_*6;
    if(test_stage_==recovery+1) {
      if(state.error.empty()) return;
      retired_document_=document_;clear_scene();++test_stage_;return;
    }
    if(!state.error.empty()) {finish_test(false,state.error);return;}
    if(state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<4) return;
    if(!retired_document_.expired()) {finish_test(false,"旧文档仍被持有");return;}
    for(size_t t=0;t<document_->catalog.targets.size();++t) for(size_t m=0;m<document_->catalog.targets[t].morphs.size();++m) {
      const auto &parameter=document_->catalog.targets[t].morphs[m];
      if(parameter.evaluable&&parameter.alias_morph<0&&snapshot_.values[t].morphs[m]!=parameter.initial) {finish_test(false,"增删或替换场景改变了已保存的 ERC 原始输入");return;}
      if(parameter.channel_id=="Cancel"&&state.effective.at(t).at(m)!=0) {finish_test(false,"增删场景后 ERC 抵消形态失效");return;}
    }
    PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
    K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory));
    const auto &scene=document_->loaded.scene;
    lifecycle_samples_.push_back({{"stage",test_stage_},{"private_bytes",memory.PrivateUsage},{"working_set",memory.WorkingSetSize},{"objects",document_->catalog.targets.size()},{"meshes",scene.meshes.size()},{"materials",scene.materials.size()},{"textures",scene.textures.size()}});
    std::ofstream(output_/"lifecycle-progress.json")<<lifecycle_samples_.dump(2);
    if(test_stage_==recovery+2) {finish_test(true);return;}
    retired_document_=document_;
    if(test_stage_==recovery) {
      auto broken=std::make_shared<Document>(*document_);broken->generation=++generation_;broken->formulas.graphs.clear();
      parameters_->bind(nullptr,nullptr);document_=broken;snapshot_.generation=broken->generation;select(-1);renderer_->set_document(document_,snapshot_,false);++test_stage_;return;
    }
    const int phase=test_stage_%6;
    if(phase==0) {lifecycle_objects_=document_->catalog.targets.size();if(!lifecycle_objects_) {finish_test(false,"生命周期样例没有模型");return;}load(lifecycle_first_,false,true);}
    else if(phase==1) {
      if(document_->catalog.targets.size()!=lifecycle_objects_*2) {finish_test(false,"追加没有创建独立对象");return;}
      choose(int(lifecycle_objects_));delete_->trigger();
    } else if(phase==2) {
      if(document_->catalog.targets.size()!=lifecycle_objects_) {finish_test(false,"删除污染原场景");return;}clear_scene();
    } else if(phase==3) {
      if(!scene.instances.empty()||!scene.meshes.empty()||!scene.materials.empty()||!scene.textures.empty()||!snapshot_.values.empty()||!snapshot_.poses.empty()) {finish_test(false,"空场景仍持有旧资源");return;}load(lifecycle_second_);
    } else if(phase==4) {choose(0);delete_->trigger();}
    else {if(!document_->catalog.targets.empty()) {finish_test(false,"删除最后一个模型失败");return;}load(lifecycle_first_);}
    ++test_stage_;
  }
  void navigation_tick(const RenderStatus &state) {
    const auto time=QDateTime::currentMSecsSinceEpoch();
    std::ofstream(output_/"navigation-progress.json")<<nlohmann::json({{"stage",test_stage_},{"preview",state.preview},{"camera_epoch",state.camera.epoch},{"navigating",state.camera.navigating},{"focus_requests",state.focus_requests},{"last_preview_frame",state.last_preview_frame},{"previous_preview_frame",navigation_before_.last_preview_frame},{"requested_epoch",state.requested_epoch},{"presented_epoch",state.presented_epoch},{"samples",state.samples}}).dump(2);
    if(time-test_started_>600000) {finish_test(false,"导航预览验证超时");return;}
    if(!state.error.empty()) {finish_test(false,state.error);return;}
    if(!document_||state.generation!=document_->generation||resize_at_) return;
    const auto hwnd=FindWindowExW(reinterpret_cast<HWND>(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
    if(!hwnd) {finish_test(false,"找不到原生视口");return;}
    auto input=[&](UINT message,WPARAM w=0,LPARAM l=0) {SendMessageW(hwnd,message,w,l);};
    const int item=test_stage_/3,phase=test_stage_%3;
    if(phase&&time-navigation_at_>15000) {finish_test(false,"导航输入未在 15 秒内完成预期切换");return;}
    // 用实际呈现的尺寸和版本判断恢复，结束采样后的诊断计数可能归零。
    const bool full=state.frames>0&&!state.preview&&state.presented_epoch==state.requested_epoch&&state.render_width==state.width&&state.render_height==state.height;
    static const char *names[]={"W","A","S","D","Q","E","右键环绕","Ctrl＋右键转头","右键松开但 W 仍按住","滚轮","F 聚焦","捕获丢失","窗口失焦",
      "Shift＋右键平移","中键平移","后侧键转头","后侧键单击聚焦","侧键拖回起点不聚焦","Ctrl＋侧键不聚焦","侧键加键盘不聚焦","侧键加右键不聚焦",
      "侧键在外部释放不聚焦","视口外侧键不聚焦","前侧键不聚焦","侧键轻微抖动仍单击","Hierarchy 切换后立即侧键聚焦","侧键加 W 不聚焦","侧键捕获丢失","侧键失焦","视口外滚轮","视口外中键","Hierarchy 区域侧键不聚焦"};
    const bool no_change=(item>=18&&item<=23)||item==27||item==28||item==29||item==30||item==31;
    const bool focus_click=item==16||item==24||item==25;
    const bool instant=item==9||item==10||focus_click||item==17;
    const LPARAM point=MAKELPARAM(200,200),moved=MAKELPARAM(220,210),outside=MAKELPARAM(-20,-20);
    auto side=[&](bool down,LPARAM position,WORD flags=0,WORD button=XBUTTON1) {input(down?WM_XBUTTONDOWN:WM_XBUTTONUP,MAKEWPARAM(flags,button),position);};
    auto wheel=[&](bool in_view) {POINT p{in_view?200:-20,in_view?200:-20};ClientToScreen(hwnd,&p);input(WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(p.x,p.y));};
    if(phase==0) {
      if(!full) return;
      if(item==int(std::size(names))) {finish_test(true);return;}
      activateWindow();raise();SetForegroundWindow(reinterpret_cast<HWND>(winId()));
      // 用户仍按着键时不注入下一项单击，避免把正确的组合键排除当成失败。
      for(int key=VK_BACK;key<256;++key) if((GetKeyState(key)|GetAsyncKeyState(key))&0x8000) return;
      const bool drag_case=(item>=6&&item<=8)||item==11||item==12||item==13||item==14||item==15;
      POINT cursor{drag_case?220:200,drag_case?210:200};ClientToScreen(hwnd,&cursor);SetCursorPos(cursor.x,cursor.y);
      MSG pending{};while(PeekMessageW(&pending,hwnd,WM_MOUSEMOVE,WM_MOUSEMOVE,PM_REMOVE)) DispatchMessageW(&pending);
      navigation_before_=state;navigation_at_=time;SetFocus(hwnd);
      navigation_before_.camera=renderer_->input_camera();
      if(item<6) input(WM_KEYDOWN,"WASDQE"[item]);
      else if(item==9) wheel(true);
      else if(item==10) {input(WM_KEYDOWN,'F');input(WM_KEYUP,'F');}
      else if(item==14) {input(WM_MBUTTONDOWN,MK_MBUTTON,point);input(WM_MOUSEMOVE,MK_MBUTTON,moved);}
      else if(item>=15) {
        if(item==25) {choose(-1);choose(int(state.bounds.size())-1);}
        if(item==29) wheel(false);
        else if(item==30) {input(WM_MBUTTONDOWN,MK_MBUTTON,outside);input(WM_MOUSEMOVE,MK_MBUTTON,outside);input(WM_MBUTTONUP,0,outside);}
        else if(item==31) {SendMessageW(reinterpret_cast<HWND>(hierarchy_->winId()),WM_XBUTTONDOWN,MAKEWPARAM(0,XBUTTON1),point);SendMessageW(reinterpret_cast<HWND>(hierarchy_->winId()),WM_XBUTTONUP,MAKEWPARAM(0,XBUTTON1),point);}
        else {
          side(true,item==22?outside:point,item==18?MK_CONTROL:0,item==23?XBUTTON2:XBUTTON1);
          if(item==15||item==17) input(WM_MOUSEMOVE,MK_XBUTTON1,moved);
          if(item==17) input(WM_MOUSEMOVE,MK_XBUTTON1,point);
          if(item==19) {input(WM_KEYDOWN,'X');input(WM_KEYUP,'X');}
          if(item==20) {input(WM_RBUTTONDOWN,MK_RBUTTON|MK_XBUTTON1,point);input(WM_RBUTTONUP,MK_XBUTTON1,point);}
          if(item==24) input(WM_MOUSEMOVE,MK_XBUTTON1,MAKELPARAM(201,201));
          if(item==26) input(WM_KEYDOWN,'W');
          if(item==27) ReleaseCapture();
          else if(item==28) SetFocus(reinterpret_cast<HWND>(host_->winId()));
          else if(item!=15&&item!=26) side(false,item==21||item==22?outside:point,0,item==23?XBUTTON2:XBUTTON1);
        }
      }
      else {
        input(WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(200,200));
        input(WM_MOUSEMOVE,MK_RBUTTON|(item==7?MK_CONTROL:item==13?MK_SHIFT:0),MAKELPARAM(220,210));
        if(item==8) {input(WM_KEYDOWN,'W');input(WM_RBUTTONUP);}
      }
      navigation_after_input_=renderer_->input_camera();++test_stage_;return;
    }
    if(phase==1) {
      if(no_change) {if(time-navigation_at_<300) return;++test_stage_;return;}
      if(state.last_preview_frame<=navigation_before_.last_preview_frame) return;
      if(!instant) {
        if(!state.camera.navigating||!state.preview) {finish_test(false,"持续输入期间提前恢复全分辨率");return;}
        if(state.render_width!=std::max(1,state.width/4)||state.render_height!=std::max(1,state.height/4)||state.samples>2) {finish_test(false,"移动预览尺寸或采样上限错误");return;}
        if(time-navigation_at_<500) return;
      }
      if(item==7||item==15) {
        const auto d=navigation_after_input_.eye()-navigation_before_.camera.eye();
        if(std::abs(d.x)+std::abs(d.y)+std::abs(d.z)>.0001f) {finish_test(false,"第一人称转头改变了相机位置");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"navigation-preview.png").wstring()));
      }
      if(item==13||item==14) {
        auto expected=navigation_before_.camera;expected.pan(20,10);const auto d=navigation_after_input_.target-expected.target;
        if(std::abs(d.x)+std::abs(d.y)+std::abs(d.z)>.0001f||navigation_after_input_.yaw!=expected.yaw||navigation_after_input_.pitch!=expected.pitch) {finish_test(false,"屏幕朝向平移不等于原 Shift＋右键");return;}
      }
      if(item<6) input(WM_KEYUP,"WASDQE"[item]);
      else if(item==8) input(WM_KEYUP,'W');
      else if(item==11) ReleaseCapture();
      else if(item==12) {input(WM_KEYDOWN,'W');SetFocus(reinterpret_cast<HWND>(host_->winId()));}
      else if(item==14) input(WM_MBUTTONUP,0,moved);
      else if(item==15||item==26) {if(item==26) input(WM_KEYUP,'W');side(false,moved);}
      else if(!instant) input(WM_RBUTTONUP);
      ++test_stage_;return;
    }
    if(!full) return;
    const uint64_t expected_focus=navigation_before_.focus_requests+((item==10||focus_click)?1:0);
    if(state.focus_requests!=expected_focus) {finish_test(false,"侧键单击、拖动或组合键的聚焦计数错误");return;}
    if(no_change&&state.camera.epoch!=navigation_before_.camera.epoch) {finish_test(false,"非导航操作改变了相机");return;}
    if(focus_click) {
      const auto c=state.selection_bounds.center();const auto t=state.camera.target;
      if(std::abs(c.x-t.x)+std::abs(c.y-t.y)+std::abs(c.z-t.z)>.0001f||state.selected_target!=selected_) {finish_test(false,"侧键聚焦未同步 Hierarchy 当前选择");return;}
    }
    if(state.camera.navigating||state.adapter.geometry_updates!=navigation_before_.adapter.geometry_updates||state.skinning.evaluations!=navigation_before_.skinning.evaluations||state.evaluation.morph_evaluations!=navigation_before_.evaluation.morph_evaluations) {finish_test(false,"导航结束状态或几何隔离错误");return;}
    if(item==7) screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"navigation-refined.png").wstring()));
    navigation_checks_.push_back({{"input",names[item]},{"preview_frame",state.last_preview_frame},{"full_size",{state.render_width,state.render_height}},{"full_samples",state.samples},{"focus_requests",state.focus_requests},{"selected_target",state.selected_target}});
    ++test_stage_;
  }
  void tick() {
    const auto focus_state=renderer_->status();if(focus_state.focus_requests!=focus_requests_||focus_pending_) {focus_requests_=focus_state.focus_requests;focus_selection();}
    const QSize size(qRound(host_->width()*host_->devicePixelRatioF()),qRound(host_->height()*host_->devicePixelRatioF()));
    if(size!=viewport_size_) {viewport_size_=size;resize_at_=QDateTime::currentMSecsSinceEpoch()+180;}
    if(resize_at_&&QDateTime::currentMSecsSinceEpoch()>=resize_at_) {resize_at_=0;renderer_->resize(size.width(),size.height());}
    const auto state=renderer_->status();
    if(navigation_test_) {navigation_tick(state);return;}
    if(refresh_parameters_) refresh_parameters_->setEnabled(!loading_&&document_&&selected_>=0);
    if(retry_parameters_) retry_parameters_->setEnabled(document_&&!state.resource_error.empty());
    static int resources_tick=0;if(++resources_tick%5==0) parameters_->resource_states();
    if(delete_) delete_->setEnabled(!loading_&&document_&&selected_joint_<0&&(selected_>=0||selected_light_>=0));
    if(attachment_test_) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000) {finish_test(false,"骨骼附件界面验证超时");return;}
      if(!state.error.empty()||!state.edit_error.empty()) {finish_test(false,state.error+state.edit_error);return;}
      if(!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<64) return;
      if(test_stage_==0) {
        for(size_t t=0;t<document_->catalog.targets.size();++t) for(size_t s=0;s<document_->skeletons.skins.size();++s) {
          const auto &skin=document_->skeletons.skins[s];for(size_t j=0;j<skin.joints.size();++j) if(skin.joints[j].name=="head"&&document_->catalog.targets[t].parent=="#"+skin.joints[j].scene_id) {
            for(size_t owner=0;owner<document_->catalog.targets.size();++owner) if(document_->catalog.targets[owner].instance==skin.instance) attachment_cases_.push_back({int(t),int(owner),int(j)});
          }
        }
        if(attachment_cases_.empty()) {finish_test(false,"未找到 Head 骨骼附件");return;}++test_stage_;
      }
      const size_t index=size_t((test_stage_-1)/2);const auto item=attachment_cases_.at(index);const auto &owner=document_->catalog.targets[size_t(item[1])];
      if(test_stage_%2==1) {
        hierarchy_->collapseAll();choose(item[0]);const auto *parent=hierarchy_->currentItem()?hierarchy_->currentItem()->parent():nullptr;
        if(!parent||parent->data(0,Qt::UserRole).toInt()!=item[1]||parent->data(0,Qt::UserRole+1).toInt()!=item[2]) {finish_test(false,"眼镜没有挂在所属角色的 Head 下");return;}
        auto transform=document_->loaded.scene.instances[owner.instance].transform;
        for(size_t s=0;s<document_->skeletons.skins.size();++s) if(document_->skeletons.skins[s].instance==owner.instance) transform=transform*runtime::joint_transforms(document_->skeletons.skins[s],snapshot_.poses[s]).at(size_t(item[2]));
        const float yaw=std::atan2(-transform.value[1],transform.value[5]);renderer_->frame(state.head_bounds.at(size_t(item[1])));renderer_->orbit((.3f-yaw)/.005f,20);
        attachment_report_.push_back({{"attachment",document_->catalog.targets[size_t(item[0])].id},{"owner",owner.id},{"parent",parent->text(0).toStdString()},{"tree_parent_verified",true}});++test_stage_;return;
      }
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/std::filesystem::u8path(owner.label+"-head-attachment.png")).wstring()));
      if(index+1==attachment_cases_.size()) {finish_test(true);return;}++test_stage_;return;
    }
    if(options_test_) {
      if(test_stage_==7) {if(++options_wait_>=4) {renderer_->keyboard('W',false);++test_stage_;}return;}
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>360000) {finish_test(false,"环境 / 导航界面验证超时");return;}
      if(!state.error.empty()||!state.edit_error.empty()) {finish_test(false,state.error+state.edit_error);return;}
      if(!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.samples<8) return;
      if(test_stage_==0) {choose(-3);options_geometry_=state.adapter.geometry_updates;std::ofstream(output_/"render-options.json")<<ir::options_json(snapshot_.options).dump(2);++test_stage_;return;}
      if(test_stage_==1) {
        options_epoch_=state.requested_epoch;
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"tonemapper-panel.png").wstring()));
        auto &node=snapshot_.options.tonemapper;for(size_t i=0;i<node.parameters.size();++i) if(node.parameters[i].id=="Exposure Value") {ir::set_option(node,i,0,node.parameters[i].value[0]+1);break;}send();++test_stage_;return;
      }
      if(test_stage_==2) {if(state.adapter.geometry_updates!=options_geometry_||state.requested_epoch!=options_epoch_) {finish_test(false,"色调编辑触发了几何更新或重新采样");return;}choose(-2);++test_stage_;return;}
      if(test_stage_==3) {
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"environment-panel.png").wstring()));
        auto &node=snapshot_.options.environment;for(size_t i=0;i<node.parameters.size();++i) if(node.parameters[i].id=="Dome Rotation") {ir::set_option(node,i,0,node.parameters[i].value[0]+15);break;}send();++test_stage_;return;
      }
      if(test_stage_==4) {if(state.adapter.geometry_updates!=options_geometry_||state.requested_epoch<=options_epoch_) {finish_test(false,"环境编辑没有正确增量重新采样");return;}choose(0);++test_stage_;return;}
      if(test_stage_==5) {if(state.selected_target!=0||state.selection_bounds.empty) return;options_camera_=state.camera;renderer_->keyboard('F',true);renderer_->keyboard('F',false);++test_stage_;return;}
      if(test_stage_==6) {if(state.camera.epoch<=options_camera_.epoch) return;options_camera_=state.camera;renderer_->keyboard('W',true);options_wait_=0;++test_stage_;return;}
      if(test_stage_==8) {
        const auto a=options_camera_.target,b=state.camera.target;if(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)<.0001f) {finish_test(false,"W 没有移动相机");return;}
        if(state.adapter.geometry_updates!=options_geometry_) {finish_test(false,"相机输入触发几何更新");return;}screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"parameters-panel.png").wstring()));finish_test(true);return;
      }return;
    }
    if(lifecycle_test_) {lifecycle_tick(state);return;}
    if(!loading_&&document_&&state.generation==document_->generation&&state.clicks!=clicks_) {clicks_=state.clicks;choose(state.hit_target,state.hit_joint);}
    if(!state.error.empty()) {statusBar()->showMessage(QStringLiteral("渲染错误：")+text(state.error));if(self_test_) finish_test(false,state.error);return;}
    if(!state.edit_error.empty()) {statusBar()->showMessage(QStringLiteral("本次编辑未应用：")+text(state.edit_error));if(self_test_) finish_test(false,state.edit_error);return;}
    if(document_&&state.generation==document_->generation&&state.applied_revision==snapshot_.revision&&selected_>=0&&size_t(selected_)<state.effective.size()) parameters_->evaluated(state.effective[size_t(selected_)]);
    if(!load_error_.isEmpty()) statusBar()->showMessage(load_error_);
    else if(!state.resource_error.empty()) statusBar()->showMessage(QStringLiteral("Morph 未应用：")+text(state.resource_error)+QStringLiteral("；可重试加载或刷新参数目录"));
    else if(state.pending_payloads) statusBar()->showMessage(QStringLiteral("正在异步载入 %1 项 Morph 数据，完成后应用最新输入…").arg(state.pending_payloads));
    else if(!pending_parameters_.empty()) statusBar()->showMessage(QStringLiteral("有 %1 项参数更改待应用").arg(pending_parameters_.size()));
    else if(!loading_) statusBar()->showMessage(QStringLiteral("OptiX · %1 samples · 网格 %2 · 顶点更新 %3 · 蒙皮求值 %4 · 发丝 %5").arg(state.samples).arg(state.adapter.meshes).arg(state.adapter.geometry_updates).arg(state.skinning.evaluations).arg(state.adapter.curves));
    if(frame_pending_&&document_&&state.generation==document_->generation&&state.applied_revision==snapshot_.revision&&selected_>=0&&size_t(selected_)<state.bounds.size()) {renderer_->frame(state.bounds[size_t(selected_)]);frame_pending_=false;return;}
    if(!self_test_) return;
    if(lazy_test_) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>180000) {finish_test(false,"Morph 异步界面验证超时");return;}
      if(!load_error_.isEmpty()||!state.resource_error.empty()) {finish_test(false,load_error_.toStdString()+state.resource_error);return;}
      if(loading_||!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<8) return;
      const auto &target=document_->catalog.targets.at(0);size_t a=target.morphs.size();for(size_t m=0;m<target.morphs.size();++m) if(target.morphs[m].channel_id=="A") a=m;
      if(a==target.morphs.size()) {finish_test(false,"缺少异步测试夹具参数 A");return;}
      if(test_stage_==0) {
        choose(0);if(!target.morphs[a].payload||target.morphs[a].payload->state()!=runtime::PayloadState::unloaded) {finish_test(false,"选择角色时预读了未使用差值");return;}
        lazy_generation_=document_->generation;parameters_->select_parameter(a);set_morph(a,.2);set_morph(a,.7);++test_stage_;
      } else if(test_stage_==1) {
        if(state.effective.at(0).at(a)!=.7f||state.adapter.geometry_updates==0) {finish_test(false,"首用参数未按最新值增量提交");return;}
        lazy_updates_=state.adapter.geometry_updates;manual_morph_->setChecked(true);set_morph(a,.4);++test_stage_;
      } else if(test_stage_==2) {
        if(state.effective.at(0).at(a)!=.7f||state.adapter.geometry_updates!=lazy_updates_||!apply_parameters_->isEnabled()) {finish_test(false,"手动输入在应用前改变了几何");return;}
        if(++lazy_wait_ticks_<4) return;apply_parameters_->click();++test_stage_;
      } else if(test_stage_==3) {
        if(state.effective.at(0).at(a)!=.4f||state.adapter.geometry_updates<=lazy_updates_) {finish_test(false,"手动应用未提交几何");return;}
        lazy_updates_=state.adapter.geometry_updates;set_morph(a,.9);refresh_parameters_->click();++test_stage_;
      } else if(test_stage_==4) {
        if(!document_->asset_revision||document_->generation!=lazy_generation_||snapshot_.values.at(0).morphs.at(a)!=.9f||state.effective.at(0).at(a)!=.4f||state.adapter.geometry_updates!=lazy_updates_) {finish_test(false,"目录刷新丢失输入、提前提交暂存值或重建了网格");return;}
        apply_parameters_->click();++test_stage_;
      } else if(test_stage_==5) {
        if(state.effective.at(0).at(a)!=.9f||state.adapter.geometry_updates<=lazy_updates_) {finish_test(false,"刷新后的暂存值未能应用");return;}
        reset_selected();++test_stage_;
      } else {
        if(state.max_displacement!=0||!pending_parameters_.empty()) {finish_test(false,"手动模式下重置失败");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"lazy-editor.png").wstring()));finish_test(true);
      }
      return;
    }
    if(!visibility_label_.isEmpty()) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>360000) {finish_test(false,"可见性界面验证超时");return;}
      if(!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<8) return;
      if(test_stage_==0) {
        for(size_t t=0;t<document_->catalog.targets.size();++t) if(text(document_->catalog.targets[t].label)==visibility_label_) visibility_target_=int(t);
        if(visibility_target_<0) {finish_test(false,"可见性测试对象缺失");return;}
        choose(visibility_target_);visibility_initial_=snapshot_.values[size_t(visibility_target_)].visible;visibility_geometry_updates_=state.adapter.geometry_updates;
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"visible-initial.png").wstring()));
        visible_->setChecked(!visibility_initial_);test_stage_=1;return;
      }
      const auto instance=document_->catalog.targets[size_t(visibility_target_)].instance;
      const bool expected=test_stage_==1?!visibility_initial_:visibility_initial_;
      if(state.visible.at(instance)!=expected||visible_->isChecked()!=expected||hierarchy_->currentItem()->checkState(0)!=(expected?Qt::Checked:Qt::Unchecked)||state.adapter.geometry_updates!=visibility_geometry_updates_) {
        finish_test(false,"树、属性和渲染可见性不同步，或切换触发了几何重建");return;
      }
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/(test_stage_==1?"visible-toggled.png":"visible-restored.png")).wstring()));
      if(test_stage_==1) {hierarchy_->currentItem()->setCheckState(0,visibility_initial_?Qt::Checked:Qt::Unchecked);test_stage_=2;return;}
      finish_test(true);return;
    }
    if(capture_test_) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000) {finish_test(false,"场景显示验证超时");return;}
      if(document_&&state.generation==document_->generation&&state.presented_revision==snapshot_.revision&&state.presented_epoch==state.requested_epoch&&state.samples>=(capture_seconds_>0?1:capture_samples_)) {
        if(test_stage_==0&&!capture_targets_.empty()) {
          ir::Bounds bounds;size_t matched=0;
          for(size_t t=0;t<document_->catalog.targets.size();++t) if(capture_targets_.contains(text(document_->catalog.targets[t].label))) {const auto &b=capture_head_?state.head_bounds.at(t):state.bounds.at(t);if(b.empty) continue;bounds.add(b.minimum);bounds.add(b.maximum);++matched;}
          if(matched!=size_t(capture_targets_.size())) {finish_test(false,"截图目标未唯一匹配");return;}
          choose(-1);renderer_->frame(bounds);if(capture_front_) {float yaw=3.14159265f;for(const auto &t:document_->catalog.targets) if(capture_targets_.contains(text(t.label))) {const auto &m=document_->loaded.scene.instances[t.instance].transform.value;yaw=std::atan2(-m[1],m[5]);break;}renderer_->orbit((.3f-yaw)/.005f,24.f);}capture_started_=QDateTime::currentMSecsSinceEpoch();test_stage_=1;return;
        }
        if(capture_seconds_>0) {
          if(!capture_started_) capture_started_=QDateTime::currentMSecsSinceEpoch();
          const double elapsed=(QDateTime::currentMSecsSinceEpoch()-capture_started_)*.001;
          auto save=[&](const char *name) {
            const auto pixmap=screen()->grabWindow(winId());
            pixmap.save(QString::fromStdWString((output_/(std::string(name)+"-window.png")).wstring()));
            const auto ratio=pixmap.devicePixelRatio();const auto origin=host_->mapTo(this,QPoint{});
            pixmap.copy(QRect(qRound(origin.x()*ratio),qRound(origin.y()*ratio),state.width,state.height)).save(QString::fromStdWString((output_/(std::string(name)+".png")).wstring()));
            capture_records_.push_back({{"frame",name},{"seconds_after_camera_set",elapsed},{"displayed_samples",state.samples},{"epoch",state.presented_epoch},{"width",state.width},{"height",state.height}});
            std::ofstream(output_/"convergence.json")<<capture_records_.dump(2);
          };
          if(!capture_timed_) {if(elapsed<capture_seconds_) return;save("timed");capture_timed_=true;}
          if(state.samples<capture_samples_&&elapsed<capture_seconds_*4) return;
          save("reference");
        }
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"scene.png").wstring()));finish_test(true);
      }return;
    }
    if(workflow_test_) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>360000) {finish_test(false,"场景交互验证超时");return;}
      if(!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<8) return;
      if(head_selection_test_) {head_selection_tick(state);return;}
      auto check_hover=[&](const char *mode,bool part) {
        if(state.hovered<0) return false;
        const auto total=document_->loaded.scene.meshes.at(document_->loaded.scene.instances.at(size_t(state.hovered)).mesh).triangles.size();
        if(part?(state.hovered_joint<0||state.hovered_triangles==0||state.hovered_triangles>=total):(state.hovered_joint>=0||state.hovered_triangles!=total)) {finish_test(false,"悬停覆盖范围与选取层级不一致");return false;}
        hover_checks_.push_back({{"mode",mode},{"joint",state.hovered_joint},{"highlight_triangles",state.hovered_triangles},{"object_triangles",total}});return true;
      };
      if(test_stage_==0) {choose(-1);workflow_click_=state.clicks;renderer_->pointer(state.width/2,state.height/2);test_stage_=10;}
      else if(test_stage_==10) {
        if(state.selection_generation!=document_->generation||state.selected_target!=-1||!check_hover("unselected_whole",false)) return;
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"workflow-hover-whole.png").wstring()));
        renderer_->pointer(state.width/2,state.height/2,true);test_stage_=1;
      }
      else if(test_stage_==1&&state.clicks>workflow_click_) {
        if(selected_<0||selected_joint_>=0) {finish_test(false,"首次角色射线未选择整体");return;}
        test_evaluations_=state.evaluation.morph_evaluations;test_skin_evaluations_=state.skinning.evaluations;test_stage_=11;
      } else if(test_stage_==11) {
        if(state.selection_generation!=document_->generation||state.selected_target!=selected_||!check_hover("selected_figure_part",true)) return;
        if(selected_joint_>=0||state.evaluation.morph_evaluations!=test_evaluations_||state.skinning.evaluations!=test_skin_evaluations_) {finish_test(false,"一级选择后的悬停修改了选择或触发变形");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"workflow-hover-part.png").wstring()));
        workflow_click_=state.clicks;renderer_->pointer(state.width/2,state.height/2,true);test_stage_=2;
      } else if(test_stage_==2&&state.clicks>workflow_click_) {
        if(selected_joint_<0) {finish_test(false,"二次角色射线未选择部位");return;}
        if(selected_joint_!=state.hovered_joint) {finish_test(false,"二次点击选中的部位与悬停高亮不一致");return;}
        const auto index=selected_skin();const auto &skin=document_->skeletons.skins[size_t(index)];int head=-1;
        for(size_t j=0;j<skin.joints.size();++j) if(skin.joints[j].id=="head") head=int(j);
        if(head<0) {finish_test(false,"角色缺少头部节点");return;}choose(selected_,head);
        const auto &morphs=document_->catalog.targets[size_t(selected_)].morphs;
        auto found=std::find_if(morphs.begin(),morphs.end(),[](const auto &m) {return m.label=="Jaw Open"&&m.owner=="head"&&m.unsupported.empty();});
        if(found==morphs.end()) {finish_test(false,"头部 Jaw Open 别名未启用");return;}
        test_morph_=size_t(found-morphs.begin());parameters_->query(QStringLiteral("Jaw Open"));parameters_->select_parameter(test_morph_);parameters_->set_slider(500);test_initial_displacement_=state.max_displacement;test_stage_=3;
      } else if(test_stage_==3) {
        if(state.max_displacement<=test_initial_displacement_) {finish_test(false,"头部 Morph 未改变网格");return;}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"workflow-head.png").wstring()));
        test_evaluations_=state.evaluation.morph_evaluations;test_skin_evaluations_=state.skinning.evaluations;
        resize(width()-160,height()-90);test_stage_=4;
      } else if(test_stage_==4) {
        if(resize_at_||state.width!=viewport_size_.width()||state.height!=viewport_size_.height()) return;
        if(state.evaluation.morph_evaluations!=test_evaluations_||state.skinning.evaluations!=test_skin_evaluations_) {finish_test(false,"视口缩放重新求值了变形");return;}
        renderer_->pointer(state.width/2,state.height/2);test_stage_=5;
      } else if(test_stage_==5) {
        if(!check_hover("selected_bone_hover_other_part",true)) return;
        if(selected_joint_<0) {finish_test(false,"缩放视口清除了部位选择");return;}
        const auto saved=saveState(1);
        {QSettings settings(QString::fromStdWString((output_/"layout-test.ini").wstring()),QSettings::IniFormat);settings.setValue("docks",saved);settings.sync();}
        auto *panel=hierarchy_->parentWidget();panel->hide();
        {QSettings settings(QString::fromStdWString((output_/"layout-test.ini").wstring()),QSettings::IniFormat);if(!restoreState(settings.value("docks").toByteArray(),1)||!panel->isVisible()) {finish_test(false,"布局磁盘保存与恢复失败");return;}}
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"workflow-resized.png").wstring()));
        workflow_layout_=saveState(1);auto *viewport=findChild<QDockWidget *>("Viewport");viewport->setFloating(true);viewport->setGeometry(QRect(screen()->availableGeometry().topLeft()+QPoint(120,100),QSize(800,650)));test_stage_=6;
      } else if(test_stage_==6) {
        if(resize_at_||state.width!=viewport_size_.width()||state.height!=viewport_size_.height()) return;
        auto *viewport=findChild<QDockWidget *>("Viewport");viewport->screen()->grabWindow(viewport->winId()).save(QString::fromStdWString((output_/"workflow-floating.png").wstring()));
        viewport->setFloating(false);restoreState(workflow_layout_,1);test_stage_=7;
      } else if(test_stage_==7) {
        if(resize_at_||state.width!=viewport_size_.width()||state.height!=viewport_size_.height()) return;
        if(selected_joint_<0||state.evaluation.morph_evaluations!=test_evaluations_||state.skinning.evaluations!=test_skin_evaluations_) {finish_test(false,"浮动停靠改变了选择或重新求值变形");return;}
        workflow_target_=selected_;workflow_joint_=selected_joint_;choose(-1);renderer_->pointer(state.width/2,state.height/2);test_stage_=12;
      } else if(test_stage_==12) {
        if(state.selected_target!=-1||!check_hover("cleared_selection_whole",false)) return;
        workflow_click_=state.clicks;renderer_->pointer(2,2,true);test_stage_=13;
      } else if(test_stage_==13&&state.clicks>workflow_click_) {
        if(selected_>=0||state.hovered>=0||state.hovered_triangles!=0) {finish_test(false,"空白点击没有清除选择及悬停");return;}
        choose(workflow_target_,workflow_joint_);renderer_->pointer(state.width/2,state.height/2);test_stage_=14;
      } else if(test_stage_==14) {
        if(state.selected_target!=selected_||!check_hover("tree_selection_part",true)) return;
        if(state.evaluation.morph_evaluations!=test_evaluations_||state.skinning.evaluations!=test_skin_evaluations_) {finish_test(false,"选择或悬停触发变形求值");return;}
        finish_test(true);
      }
      return;
    }
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>(formula_test_?360000:180000)) {finish_test(false,"等待编辑器验证超时");return;}
    if(!document_ || state.generation!=document_->generation || state.presented_revision!=snapshot_.revision || state.presented_epoch!=state.requested_epoch || state.frames==0 || state.samples<(state.conform.bindings?32:8)) return;
    if(formula_test_) {
      const auto &names=formula_names_;
      if(test_stage_==0) {
        test_initial_displacement_=state.max_displacement;const auto &morphs=document_->catalog.targets[0].morphs;
        if(formula_case_==0) {screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"editor-before.png").wstring()));if(state.conform.bindings>0&&!state.bounds.empty()) renderer_->frame(state.bounds[0]);}
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
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/("editor-parameter-reset-"+std::to_string(formula_case_)+".png")).wstring()));
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
  void workflow_test() {workflow_test_=true;self_test_=true;GetCursorPos(&workflow_cursor_);}
  void head_selection_test() {workflow_test();head_selection_test_=true;}
  void capture_test(QStringList targets={},bool front=false,bool head=false,int samples=16,double seconds=0) {capture_test_=true;self_test_=true;capture_targets_=std::move(targets);capture_front_=front;capture_head_=head;capture_samples_=std::clamp(samples,1,1<<20);capture_seconds_=seconds;}
  void visibility_test(QString label) {visibility_label_=std::move(label);self_test_=true;}
  void lifecycle_test(const std::filesystem::path &first,const std::filesystem::path &second,int rounds) {if(rounds<1||rounds>100) throw std::runtime_error("生命周期验证轮数应为 1 到 100");lifecycle_rounds_=rounds;lifecycle_test_=true;self_test_=true;lifecycle_first_=first;lifecycle_second_=second;}
  void test_parameters(const QStringList &names) {if(names.empty()) return;formula_names_.clear();for(const auto &name:names) formula_names_.push_back(name.toUtf8().toStdString());}
  Editor(const std::filesystem::path &output,ProjectSettings project,bool self_test,std::filesystem::path reload_file,std::filesystem::path pose_file={},bool pose_test=false,bool formula_test=false,SamplingSettings sampling={}):project_(std::move(project)),output_(output),reload_file_(std::move(reload_file)),pose_file_(std::move(pose_file)),pose_test_(pose_test),formula_test_(formula_test),self_test_(self_test||pose_test||formula_test) {
    setWindowTitle(QStringLiteral("DazFastViewer · 场景与形态编辑器"));setAttribute(Qt::WA_ShowWithoutActivating);
    setDockOptions(AnimatedDocks|AllowNestedDocks|AllowTabbedDocks);
    auto *central=new QWidget;auto *layout=new QVBoxLayout(central);layout->setContentsMargins(4,4,4,4);
    host_=new QWidget;host_->setAttribute(Qt::WA_NativeWindow);host_->setAttribute(Qt::WA_DontCreateNativeAncestors);host_->setMinimumSize(160,120);host_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    layout->addWidget(host_,1);auto *navigation_help=new QLabel(QStringLiteral("右键环绕 · 中键 / Shift＋右键平移 · 后侧键 / Ctrl＋右键转头\n后侧键单击 / F 聚焦所选 · 滚轮缩放 · WASDQE 移动（Shift 加速）"));navigation_help->setAlignment(Qt::AlignCenter);layout->addWidget(navigation_help);auto *viewport_dock=dock(QStringLiteral("视口"),central,Qt::RightDockWidgetArea);viewport_dock->setObjectName("Viewport");
    files_=new QFileSystemModel(this);files_->setNameFilters({"*.duf"});files_->setNameFilterDisables(false);files_->setReadOnly(true);
    explorer_=new QTreeView;explorer_->setModel(files_);
    for(int i=1;i<4;++i) explorer_->hideColumn(i);explorer_->setHeaderHidden(true);explorer_->setMinimumWidth(185);
    auto *browser=new QWidget;auto *browser_layout=new QVBoxLayout(browser);browser_layout->setContentsMargins(0,0,0,0);
    libraries_=new QComboBox;libraries_->setMinimumContentsLength(16);libraries_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    browser_layout->addWidget(libraries_);browser_layout->addWidget(explorer_);update_libraries();
    connect(libraries_,&QComboBox::currentTextChanged,this,[this](const QString &root) {explorer_->setRootIndex(files_->setRootPath(root));});
    auto *explorer_dock=dock(QStringLiteral("内容浏览器"),browser,Qt::LeftDockWidgetArea);
    hierarchy_=new QTreeWidget;hierarchy_->setHeaderLabel(QStringLiteral("场景对象"));hierarchy_->setMinimumWidth(240);hierarchy_->setIndentation(12);hierarchy_->header()->setStretchLastSection(false);hierarchy_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    auto *hierarchy_dock=dock(QStringLiteral("场景层次"),hierarchy_,Qt::LeftDockWidgetArea);tabifyDockWidget(explorer_dock,hierarchy_dock);hierarchy_dock->raise();
    auto *panel=new QWidget;auto *properties=new QVBoxLayout(panel);panel->setMinimumWidth(380);
    selection_=new QLabel(QStringLiteral("请先加载并选择对象"));selection_->setWordWrap(true);properties->addWidget(selection_);
    auto *form=new QFormLayout;
    const QString names[]={QStringLiteral("位移 X（厘米）"),QStringLiteral("位移 Y（厘米）"),QStringLiteral("位移 Z（厘米）"),QStringLiteral("旋转 X（度）"),QStringLiteral("旋转 Y（度）"),QStringLiteral("旋转 Z（度）"),QStringLiteral("缩放 X（%）"),QStringLiteral("缩放 Y（%）"),QStringLiteral("缩放 Z（%）")};
    for(int i=0;i<9;++i) {
      auto *spin=new QDoubleSpinBox;transform_[i]=spin;spin->setDecimals(6);spin->setRange(-std::numeric_limits<float>::max(),std::numeric_limits<float>::max());spin->setValue(i>=6?100:0);
      spin->setEnabled(false);spin->setKeyboardTracking(false);form->addRow(names[i],spin);
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this](double) {
        if(selected_light_>=0) {auto &light=snapshot_.lights[size_t(selected_light_)];runtime::TransformValues v;v.translation_cm={float(transform_[0]->value()),float(transform_[1]->value()),float(transform_[2]->value())};v.rotation_degrees={float(transform_[3]->value()),float(transform_[4]->value()),float(transform_[5]->value())};light.transform=runtime::make_transform(v)*light_base_;send();return;}
        if(selected_<0||selected_joint_>=0) return;auto &t=snapshot_.values[size_t(selected_)].transform;
        t.translation_cm={float(transform_[0]->value()),float(transform_[1]->value()),float(transform_[2]->value())};
        t.rotation_degrees={float(transform_[3]->value()),float(transform_[4]->value()),float(transform_[5]->value())};
        t.scale={float(transform_[6]->value()/100),float(transform_[7]->value()/100),float(transform_[8]->value()/100)};send();
      });
    }
    auto *legacy_transforms=new QWidget(panel);legacy_transforms->setLayout(form);legacy_transforms->hide();
    light_power_=new QDoubleSpinBox;light_power_->setRange(-std::numeric_limits<float>::max(),std::numeric_limits<float>::max());light_power_->setPrefix(QStringLiteral("灯光功率 "));light_power_->setKeyboardTracking(false);light_power_->hide();properties->addWidget(light_power_);
    connect(light_power_,&QDoubleSpinBox::valueChanged,this,[this](double value) {if(selected_light_<0) return;auto &p=snapshot_.lights[size_t(selected_light_)].power;const auto previous=std::max({p.x,p.y,p.z});const float ratio=previous>0?float(value)/previous:0;p=previous>0?ir::Vec3{p.x*ratio,p.y*ratio,p.z*ratio}:ir::Vec3{float(value),float(value),float(value)};send();});
    pose_status_=new QLabel(QStringLiteral("选中角色后，双击内容库中的姿势或形态 DUF 即可应用。"));pose_status_->setWordWrap(true);properties->addWidget(pose_status_);
    auto *pose_details=new QPushButton(QStringLiteral("预设应用详情"));properties->addWidget(pose_details);
    connect(pose_details,&QPushButton::clicked,this,[this] {
      QString details=QStringLiteral("尚未应用预设。");
      if(!pose_report_.is_null()) {details=QStringLiteral("未应用的通道：\n");for(const auto &c:pose_report_["unapplied"]) details+=text(c.value("node","")+c.value("modifier","")+" · "+c.value("property","")+" = "+std::to_string(c.at("value").get<float>())+"\n"+c.value("reason","")+"\n");
        if(pose_report_["unapplied"].empty()) details=QStringLiteral("预设中所有非零参数均已应用。");}
      QMessageBox::information(this,QStringLiteral("预设应用详情"),details);
    });
    visible_=new QCheckBox(QStringLiteral("可见（Visible）"));visible_->setEnabled(false);properties->addWidget(visible_);
    connect(visible_,&QCheckBox::toggled,this,[this](bool value) {if(selected_>=0) set_visible(size_t(selected_),value);});
    connect(hierarchy_,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem *item,int column) {const int target=item->data(0,Qt::UserRole).toInt();if(column==0&&target>=0&&item->data(0,Qt::UserRole+1).toInt()<0) set_visible(size_t(target),item->checkState(0)==Qt::Checked);});
    auto *reset=new QPushButton(QStringLiteral("重置选中对象"));properties->addWidget(reset);connect(reset,&QPushButton::clicked,this,[this] {reset_selected();});
    parameters_=new ParameterPanel;parameters_->changed=[this](size_t index,double value) {set_morph(index,value);};properties->addWidget(parameters_,1);
    auto *parameter_actions=new QHBoxLayout;
    refresh_parameters_=new QPushButton(QStringLiteral("刷新参数目录"));retry_parameters_=new QPushButton(QStringLiteral("重试加载"));retry_parameters_->setEnabled(false);
    parameter_actions->addWidget(refresh_parameters_);parameter_actions->addWidget(retry_parameters_);properties->addLayout(parameter_actions);
    connect(refresh_parameters_,&QPushButton::clicked,this,[this]{load_error_.clear();refresh_parameter_catalog();});
    connect(retry_parameters_,&QPushButton::clicked,this,[this]{renderer_->retry_resources();send();});
    auto *apply_actions=new QHBoxLayout;manual_morph_=new QCheckBox(QStringLiteral("手动应用参数"));apply_parameters_=new QPushButton(QStringLiteral("应用"));apply_parameters_->setEnabled(false);
    apply_actions->addWidget(manual_morph_);apply_actions->addWidget(apply_parameters_);properties->addLayout(apply_actions);
    connect(apply_parameters_,&QPushButton::clicked,this,[this]{apply_parameters();});connect(manual_morph_,&QCheckBox::toggled,this,[this](bool manual){if(!manual) apply_parameters();});
    auto *property_dock=dock(QStringLiteral("对象属性与 Morph"),panel,Qt::RightDockWidgetArea);splitDockWidget(viewport_dock,property_dock,Qt::Horizontal);
    auto *file_menu=menuBar()->addMenu(QStringLiteral("文件"));open_=file_menu->addAction(QStringLiteral("添加 / 应用 DUF…"));open_->setShortcut(QKeySequence::Open);
    connect(open_,&QAction::triggered,this,[this] {const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("加载角色、场景或姿势"),{},QStringLiteral("DAZ 文件 (*.duf)"));if(!file.isEmpty()) open_asset(file_path(file));});
    connect(file_menu->addAction(QStringLiteral("保存环境与色调设置…")),&QAction::triggered,this,[this]{
      if(!document_) return;const auto file=QFileDialog::getSaveFileName(this,QStringLiteral("保存渲染设置"),{},QStringLiteral("渲染设置 (*.dfv-render.json)"));if(file.isEmpty()) return;
      try {std::ofstream output(file_path(file));output<<ir::options_json(snapshot_.options).dump(2);if(!output) throw std::runtime_error("写入失败");}catch(const std::exception &e){QMessageBox::warning(this,QStringLiteral("保存失败"),text(e.what()));}
    });
    connect(file_menu->addAction(QStringLiteral("载入环境与色调设置…")),&QAction::triggered,this,[this]{
      if(!document_) return;const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("载入渲染设置"),{},QStringLiteral("渲染设置 (*.dfv-render.json)"));if(file.isEmpty()) return;
      try {nlohmann::json json;std::ifstream(file_path(file))>>json;auto options=ir::options_from_json(json);if(!options.environment_file.empty()&&!std::filesystem::is_regular_file(options.environment_file)) throw std::runtime_error("环境贴图不存在");parameters_->bind(nullptr,nullptr);snapshot_.options=std::move(options);rebuild_hierarchy();select(-3);send();}catch(const std::exception &e){QMessageBox::warning(this,QStringLiteral("载入失败"),text(e.what()));}
    });
    auto *project_menu=menuBar()->addMenu(QStringLiteral("项目"));project_action_=project_menu->addAction(QStringLiteral("项目设置…"));
    connect(project_action_,&QAction::triggered,this,[this] {project_settings();});
    connect(file_menu->addAction(QStringLiteral("退出")),&QAction::triggered,this,&QWidget::close);
    auto *focus_action=new QAction(QStringLiteral("聚焦选中对象（F）"),this);hierarchy_->addAction(focus_action);focus_action->setShortcut(QKeySequence(Qt::Key_F));focus_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);connect(focus_action,&QAction::triggered,this,[this]{focus_selection();});
    auto *edit=menuBar()->addMenu(QStringLiteral("编辑"));edit->addAction(focus_action);connect(edit->addAction(QStringLiteral("重置选中对象")),&QAction::triggered,this,[this] {reset_selected();});
    delete_=edit->addAction(QStringLiteral("删除选中对象及其子对象"));delete_->setShortcut(QKeySequence::Delete);delete_->setEnabled(false);
    delete_->setToolTip(QStringLiteral("删除对象、子对象及绑定的穿戴物；选择骨骼部位时请先选择所属模型"));
    connect(delete_,&QAction::triggered,this,[this] {delete_selection();});
    hierarchy_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hierarchy_,&QWidget::customContextMenuRequested,this,[this](const QPoint &point) {hierarchy_->setCurrentItem(hierarchy_->itemAt(point));QMenu menu(this);menu.addAction(delete_);menu.exec(hierarchy_->viewport()->mapToGlobal(point));});
    connect(file_menu->addAction(QStringLiteral("新建空场景")),&QAction::triggered,this,[this] {clear_scene();});
    connect(file_menu->addAction(QStringLiteral("打开场景（替换）…")),&QAction::triggered,this,[this] {const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("打开场景"),{},QStringLiteral("DAZ 场景 (*.duf)"));if(!file.isEmpty()) load(file_path(file));});
    auto *create=menuBar()->addMenu(QStringLiteral("创建"));
    connect(create->addAction(QStringLiteral("面光源")),&QAction::triggered,this,[this] {add_light();});
    auto *view=menuBar()->addMenu(QStringLiteral("视图"));for(auto *d:findChildren<QDockWidget *>()) view->addAction(d->toggleViewAction());
    connect(view->addAction(QStringLiteral("框选当前对象")),&QAction::triggered,this,[this] {frame_pending_=true;});
    connect(explorer_,&QTreeView::doubleClicked,this,[this](const QModelIndex &index) {const auto file=files_->filePath(index);if(QFileInfo(file).suffix().compare("duf",Qt::CaseInsensitive)==0) open_asset(file_path(file));});
    connect(hierarchy_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item) {select(item?item->data(0,Qt::UserRole).toInt():-1,item?item->data(0,Qt::UserRole+1).toInt():-1,item?item->data(0,Qt::UserRole+2).toInt():-1);});
    connect(view->addAction(QStringLiteral("恢复默认布局")),&QAction::triggered,this,[this] {restoreState(default_layout_,1);});
    QScreen *secondary=nullptr;for(auto *screen:QGuiApplication::screens()) if(screen!=QGuiApplication::primaryScreen()) {secondary=screen;break;}
    if(!secondary) throw std::runtime_error("缺少第二屏，编辑器不会在主屏启动");
    resize(1580,920);const QRect available=secondary->availableGeometry();
    if(width()>available.width() || height()>available.height()) throw std::runtime_error("第二屏工作区不足以容纳当前编辑器布局");
    resizeDocks({viewport_dock,property_dock},{850,330},Qt::Horizontal);default_layout_=saveState(1);
    if(!self_test_) {QSettings settings;restoreGeometry(settings.value("window/geometry").toByteArray());restoreState(settings.value("window/docks").toByteArray(),1);}
    if(!available.contains(frameGeometry())) {resize(std::min(width(),available.width()),std::min(height(),available.height()));move(available.center()-QPoint(width()/2,height()/2));}
    for(auto *d:findChildren<QDockWidget *>()) if(d->isFloating()&&!available.intersects(d->frameGeometry())) d->move(available.topLeft()+QPoint(30,30));
    show();
    renderer_=std::make_unique<Renderer>(reinterpret_cast<HWND>(host_->winId()),qRound(host_->width()*host_->devicePixelRatioF()),qRound(host_->height()*host_->devicePixelRatioF()),output_,sampling);
    auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this] {tick();});timer->start(50);
  }
  void closeEvent(QCloseEvent *event) override {if(!self_test_) {QSettings settings;settings.setValue("window/geometry",saveGeometry());settings.setValue("window/docks",saveState(1));}QMainWindow::closeEvent(event);}
  ~Editor() override {loader_.request_stop();if(loader_.joinable()) loader_.join();renderer_.reset();}
  void options_test() {options_test_=self_test_=true;}
  void navigation_test() {navigation_test_=self_test_=true;}
  void keep_open_after_test() {keep_open_after_test_=true;}
  void attachment_test() {attachment_test_=self_test_=true;}
  void lazy_test() {lazy_test_=self_test_=true;}
  void load(const std::filesystem::path &file,bool preserve=false,bool append=false) {
    if(loading_) {statusBar()->showMessage(QStringLiteral("正在加载，请稍候…"));return;}
    loading_=true;load_error_.clear();open_->setEnabled(false);project_action_->setEnabled(false);statusBar()->showMessage(QStringLiteral("正在后台解析场景与参数依赖…"));
    const auto generation=++generation_;const auto roots=roots_;const auto previous_document=append?document_:nullptr;
    loader_=std::jthread([this,file,roots,generation,preserve,previous_document](std::stop_token stop) {
      try {
        auto document=std::make_shared<Document>();document->generation=generation;document->loaded=daz::load(file,{roots,false});
        std::vector<std::filesystem::path> resolved;for(const auto &p:document->loaded.report["content_roots"]) resolved.push_back(std::filesystem::u8path(p.get<std::string>()));
        progress(QStringLiteral("正在发现 Morph 与读取场景参数…"));
        document->catalog=daz::discover_morphs(document->loaded,resolved,[this,stop](const std::string &message) {if(stop.stop_requested()) throw std::runtime_error("已取消加载");progress(text(message));},true);
        progress(QStringLiteral("正在解析骨架与场景姿势…"));
        document->skeletons=daz::load_skeletons(document->loaded);
        progress(QStringLiteral("正在编译 Formula / ERC…"));
        document->formulas=daz::enable_formulas(document->catalog,document->skeletons);
        std::ofstream(output_/"asset-report.json")<<document->loaded.report.dump(2);
        std::ofstream(output_/"morph-catalog.json")<<document->catalog.report.dump(2);
        std::ofstream(output_/"skeleton-report.json")<<document->skeletons.report.dump(2);
        std::ofstream(output_/"formula-report.json")<<document->formulas.report.dump(2);
        release_load_data(*document);
        if(previous_document) {auto merged=std::make_shared<Document>(*previous_document);merged->generation=generation;append_document(*merged,std::move(*document));document=std::move(merged);}
        else if(document->loaded.scene.lights.empty()) ir::add_studio(document->loaded.scene);
        QMetaObject::invokeMethod(this,[this,document,preserve,previous_document] {
          const auto old=document_;const auto previous=snapshot_;parameters_->bind(nullptr,nullptr);
          if(!preserve&&!previous_document) {pending_parameters_.clear();apply_parameters_->setEnabled(false);}
          document_=document;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);snapshot_={};snapshot_.options=(preserve||previous_document)?previous.options:document->loaded.scene.options;snapshot_.generation=document->generation;snapshot_.revision=1;snapshot_.lights=document->loaded.scene.lights;
          if(previous_document) for(size_t l=0;l<previous.lights.size();++l) snapshot_.lights[l]=previous.lights[l];
          frame_pending_=false;pose_report_=nullptr;pose_status_->setText(QStringLiteral("选中角色后，双击内容库中的姿势或形态 DUF 即可应用。"));
          for(const auto &skin:document_->skeletons.skins) {
            auto pose=skin.initial;
            if((preserve||previous_document)&&old) for(size_t s=0;s<old->skeletons.skins.size();++s) if(old->skeletons.skins[s].id==skin.id) {
              std::map<std::string,runtime::JointPose> by_id;for(size_t j=0;j<old->skeletons.skins[s].joints.size();++j) by_id[old->skeletons.skins[s].joints[j].id]=previous.poses[s][j];
              for(size_t j=0;j<skin.joints.size();++j) if(by_id.contains(skin.joints[j].id)) pose[j]=by_id.at(skin.joints[j].id);
            }
            snapshot_.poses.push_back(std::move(pose));
          }
          for(const auto &target:document_->catalog.targets) {
            runtime::Properties values;values.visible=document_->loaded.scene.instances.at(target.instance).visible;for(const auto &m:target.morphs) values.morphs.push_back(m.evaluable||m.unsupported.empty()?m.initial:0);
            if((preserve||previous_document) && old) for(size_t t=0;t<old->catalog.targets.size();++t) if(old->catalog.targets[t].id==target.id) {
              values.unlimited_morphs=previous.values[t].unlimited_morphs;values.transform=previous.values[t].transform;values.visible=previous.values[t].visible;std::map<std::string,float> weights;
              for(size_t m=0;m<old->catalog.targets[t].morphs.size();++m) weights[old->catalog.targets[t].morphs[m].id]=previous.values[t].morphs[m];
              for(size_t m=0;m<target.morphs.size();++m) if((target.morphs[m].evaluable||target.morphs[m].unsupported.empty())&&weights.contains(target.morphs[m].id)) values.morphs[m]=weights[target.morphs[m].id];
            }
            runtime::sync_aliases(target,values);snapshot_.values.push_back(std::move(values));
          }
          rebuild_hierarchy();
          renderer_->set_document(document_,submitted_snapshot(),!previous_document);select(-1);
          const auto first=previous_document?previous_document->catalog.targets.size():0;
          if(first<document_->catalog.targets.size()) choose(int(first));else if(hierarchy_->topLevelItemCount()) hierarchy_->setCurrentItem(hierarchy_->topLevelItem(0));
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
  parser.addOption({"workflow-test",QStringLiteral("验证射线、部位 Morph 与视口缩放后退出")});
  parser.addOption({"head-selection-test",QStringLiteral("验证三级头部选择及绑定服装射线后退出")});
  parser.addOption({"options-test",QStringLiteral("验证环境 / 色调参数及 F / WASDQE 导航后退出")});
  parser.addOption({"navigation-test",QStringLiteral("验证移动预览、第一人称转头与静止恢复后退出")});
  parser.addOption({"keep-open-after-test",QStringLiteral("导航验收完成后保留客户端供手动体验")});
  parser.addOption({"attachment-test",QStringLiteral("逐一验证 Head 附件的场景树父节点并截图")});
  parser.addOption({"capture-test",QStringLiteral("场景显示验证后截图退出")});
  parser.addOption({"capture-target",QStringLiteral("截图时框选的对象标签，可重复"),"label"});
  parser.addOption({"capture-front",QStringLiteral("截图时从框选对象正面观察")});
  parser.addOption({"capture-head",QStringLiteral("截图时框选角色头部")});
  parser.addOption({"capture-samples",QStringLiteral("截图前累积样本数"),"count","16"});
  parser.addOption({"capture-seconds",QStringLiteral("定时记录原始画面，再继续至目标样本或四倍时长"),"seconds","0"});
  parser.addOption({"sampling-settings",QStringLiteral("采样诊断配置 JSON；降噪始终禁用"),"file"});
  parser.addOption({"visibility-test",QStringLiteral("验证指定对象的属性和层级可见性开关"),"label"});
  parser.addOption({"lifecycle-test",QStringLiteral("验证反复增删与替换场景，指定第二个测试 DUF"),"file"});
  parser.addOption({"lifecycle-rounds",QStringLiteral("生命周期验证轮数"),"count","8"});
  parser.addOption({"pose",QStringLiteral("加载角色后应用的单帧姿势 DUF"),"file"});
  parser.addOption({"pose-test",QStringLiteral("验证姿势、恢复与相机后自动退出"),"file"});
  parser.addOption({"formula-test",QStringLiteral("验证指定 Morph 滑块、ERC 与恢复后退出")});
  parser.addOption({"lazy-test",QStringLiteral("验证异步 Morph、手动应用和参数目录刷新后退出")});
  parser.addOption({"test-parameter",QStringLiteral("指定滑块验证参数，可重复，与 --formula-test 配合"),"name"});
  parser.addOption({"reload-test",QStringLiteral("验证后台场景替换后退出"),"file"});parser.process(app);
  const auto output=parser.isSet("output")?file_path(parser.value("output")):std::filesystem::path("artifacts")/("editor-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz").toStdString());
  std::filesystem::create_directories(output);
  try {
    SamplingSettings sampling;
    if(parser.isSet("sampling-settings")) {nlohmann::json j;std::ifstream(file_path(parser.value("sampling-settings")))>>j;
      sampling.samples=j.value("samples",sampling.samples);sampling.adaptive_threshold=j.value("adaptive_threshold",sampling.adaptive_threshold);sampling.blue_noise=j.value("blue_noise",sampling.blue_noise);
      sampling.min_bounces=j.value("min_bounces",sampling.min_bounces);sampling.transparent_min_bounces=j.value("transparent_min_bounces",sampling.transparent_min_bounces);
      if(sampling.samples<1||sampling.samples>(1<<20)||!std::isfinite(sampling.adaptive_threshold)||sampling.adaptive_threshold<0||sampling.adaptive_threshold>1||sampling.min_bounces<0||sampling.min_bounces>8||sampling.transparent_min_bounces<0||sampling.transparent_min_bounces>32) throw std::runtime_error("采样诊断参数无效");
    }
    bool capture_seconds_valid=false;const double capture_seconds=parser.value("capture-seconds").toDouble(&capture_seconds_valid);
    if(!capture_seconds_valid||!std::isfinite(capture_seconds)||capture_seconds<0||capture_seconds>180) throw std::runtime_error("截图诊断时长无效");
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);
    ccl::path_init(app.applicationDirPath().toStdString(),DFV_CYCLES_SOURCE);
    auto project=ProjectSettings::load(parser.isSet("project")?parser.value("project"):QDir(app.applicationDirPath()).absoluteFilePath("../DazFastViewer.project.json"));
    project.content_roots=ProjectSettings::normalize(parser.values("content-root")+project.content_roots);
    Editor editor(output,std::move(project),parser.isSet("self-test")||parser.isSet("reload-test")||parser.isSet("lifecycle-test")||parser.isSet("lazy-test"),parser.isSet("reload-test")?file_path(parser.value("reload-test")):std::filesystem::path{},
      parser.isSet("pose-test")?file_path(parser.value("pose-test")):parser.isSet("pose")?file_path(parser.value("pose")):std::filesystem::path{},parser.isSet("pose-test"),parser.isSet("formula-test"),sampling);
    editor.test_parameters(parser.values("test-parameter"));
    if(parser.isSet("options-test")) editor.options_test();
    if(parser.isSet("navigation-test")) editor.navigation_test();
    if(parser.isSet("keep-open-after-test")) editor.keep_open_after_test();
    if(parser.isSet("attachment-test")) editor.attachment_test();
    if(parser.isSet("lazy-test")) editor.lazy_test();
    if(parser.isSet("workflow-test")) editor.workflow_test();
    if(parser.isSet("head-selection-test")) editor.head_selection_test();
    if(parser.isSet("capture-test")) editor.capture_test(parser.values("capture-target"),parser.isSet("capture-front"),parser.isSet("capture-head"),parser.value("capture-samples").toInt(),capture_seconds);
    if(parser.isSet("visibility-test")) editor.visibility_test(parser.value("visibility-test"));
    if(parser.isSet("lifecycle-test")) editor.lifecycle_test(file_path(parser.value("file")),file_path(parser.value("lifecycle-test")),parser.value("lifecycle-rounds").toInt());
    if(parser.isSet("file")) editor.load(file_path(parser.value("file")));
    return app.exec();
  } catch(const std::exception &e) {std::ofstream(output/"error.txt")<<e.what();return 1;}
}
