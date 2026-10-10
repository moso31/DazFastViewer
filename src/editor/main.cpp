#include "editor/parameter_widgets.h"
#include "cycles/runtime_paths.h"
#include "editor/renderer.h"
#include "editor/project.h"
#include "editor/parameters.h"
#include "editor/material_panel.h"
#include "editor/content_browser.h"
#include "editor/content_catalog.h"
#include "editor/powerpose_panel.h"
#include "editor/chrome.h"
#include "editor/ui_scale.h"
#include "editor/window_placement.h"
#include "editor/viewport_settings.h"
#include "editor/extension_panel.h"
#include "editor/physics_panel.h"
#include "editor/ground_panel.h"
#include "editor/geograft_panel.h"
#include "editor/geograft_visibility.h"
#include "editor/outfit_dialog.h"
#include "editor/dialog_layout.h"
#include "editor/ground_selection.h"
#include "editor/object_hierarchy.h"
#include "editor/group_transforms.h"
#include "editor/node_properties.h"
#include "editor/object_extension.h"
#include "editor/scene_extension.h"
#include "editor/edit_history.h"
#include "editor/history_input.h"
#include "editor/recovery.h"
#include "city/panel.h"
#include "city/document.h"
#include "water/document.h"
#include "water/panel.h"
#include "cloud/document.h"
#include "cloud/panel.h"
#include "render_ir/default_options.h"
#include <QStandardPaths>
#include <QCryptographicHash>
#include "runtime/measurement_units.h"
#include "runtime/decimal_float.h"
#include "runtime/visibility.h"
#include "daz/documents.h"
#include "daz/content_entry.h"
#include "render_ir/options_json.h"
#include "daz/pose.h"
#include "runtime/picking.h"
#include "diagnostics/event_profile.h"
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
#include <QToolButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
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
#include <QProgressDialog>
#include <QSaveFile>
#include <QEventLoop>
#include <QInputDialog>
#include <QSettings>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
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
class Editor final:public EditorWindow {
  #include "editor/history_ui.inl"
  #include "editor/physics_ui.inl"
  #include "editor/group_ui.inl"
  #include "editor/history_test.inl"
  #include "editor/object_extension_ui.inl"
  #include "editor/object_extension_test.inl"
  #include "editor/powerpose.inl"
  #include "editor/powerpose_test.inl"
#include "editor/edit_resume_test.inl"
  #include "editor/pose_test.inl"
  #include "editor/gizmo_test.inl"
  #include "editor/group_test.inl"
  #include "editor/node_properties_test.inl"
  #include "editor/ground_test.inl"
  #include "editor/collective_ground_test.inl"
  #include "editor/feedback_test.inl"
  #include "editor/render_profile.inl"
  #include "city/editor_ui.inl"
  #include "water/editor_ui.inl"
  #include "cloud/editor_ui.inl"
  #include "cloud/editor_test.inl"
  #include "water/editor_test.inl"
  #include "city/editor_test.inl"
  QWidget *host_=nullptr;
  ParameterPanel *parameters_=nullptr;
  GeograftPanel *geograft_panel_=nullptr;
  void bind_geograft() {
    if(!geograft_panel_)return;std::vector<GeograftItem> items;
    if(!loading_&&document_&&selected_>=0&&selected_joint_<0&&selected_light_<0&&hierarchy_->selectedItems().size()==1)
      for(auto t:geograft_targets(*document_,size_t(selected_)))items.push_back({t,text(document_->catalog.targets[t].label),snapshot_.values[t].graft_enabled});
    geograft_panel_->bind(items);
  }
  ContentBrowser *browser_=nullptr;
  QTreeWidget *hierarchy_=nullptr;
  QCheckBox *physics_only_=nullptr;
  QLabel *selection_=nullptr;
  QCheckBox *manual_morph_=nullptr;
  QPushButton *refresh_parameters_=nullptr,*apply_parameters_=nullptr,*retry_parameters_=nullptr;
  PendingParameters pending_parameters_;
  QLabel *pose_status_=nullptr;
  QAction *open_=nullptr;
  QAction *delete_=nullptr;
  QAction *project_action_=nullptr;
  QDoubleSpinBox *transform_[9]{};
  ProjectSettings project_;
  std::shared_ptr<Document> document_;
  Snapshot snapshot_;
  MaterialPanel *materials_=nullptr;
  std::unique_ptr<Renderer> renderer_;
  UiScale *ui_scale_=nullptr;
  ViewportSettings *viewport_settings_=nullptr;
  ApplicationSettings application_settings_;
  std::jthread loader_;
  std::vector<std::filesystem::path> roots_;
  std::filesystem::path output_;
  std::filesystem::path reload_file_;
  std::filesystem::path pose_file_;
  bool pose_test_=false,frame_pending_=false;
  uint64_t pose_commit_=0;
  uint64_t pins_generation_=0;
  std::vector<runtime::PosePin> pose_pins_;
  void pin_joint(int skin,int joint,bool enabled,bool angle=false) {
    auto edit=history_edit(angle?QStringLiteral("固定关节角度"):QStringLiteral("固定关节位置"));
    const auto state=renderer_->status();if(!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||size_t(skin)>=state.effective_poses.size()) return;
    auto found=std::find_if(pose_pins_.begin(),pose_pins_.end(),[&](const auto &p){return p.skin==skin&&p.joint==joint;});
    if(found==pose_pins_.end()) {if(!enabled) return;pose_pins_.push_back({skin,joint,{},false,false});found=std::prev(pose_pins_.end());}
    const auto &s=document_->skeletons.skins.at(skin);const auto &pose=state.effective_poses.at(skin);const auto &world=state.skin_world.at(skin);
    if(angle) {found->angle=enabled;if(enabled) found->world_orientation=runtime::rotation_frame(world)*runtime::joint_orientation(s,pose,joint);}
    else {found->position=enabled;if(enabled) found->world=world.point(runtime::joint_point(s,pose,joint,true));}
    std::erase_if(pose_pins_,[](const auto &p){return !p.position&&!p.angle;});
    pins_generation_=document_->generation;renderer_->pose_pins(pose_pins_);
  }
  void constrain_pins(int skin) {
    if(pose_pins_.empty()) return;const auto state=renderer_->status();if(!document_||state.generation!=document_->generation||size_t(skin)>=state.effective_poses.size()) return;
    auto effective=state.effective_poses[skin];const auto &before=state.input_poses.at(skin);const auto &input=snapshot_.poses.at(skin);std::vector<int> chain;std::vector<runtime::IkGoal> goals;
    for(size_t j=0;j<effective.size();++j) {auto add=[](ir::Vec3 a,ir::Vec3 b,ir::Vec3 c){return ir::Vec3{a.x+b.x-c.x,a.y+b.y-c.y,a.z+b.z-c.z};};effective[j].rotation_degrees=add(effective[j].rotation_degrees,input[j].rotation_degrees,before[j].rotation_degrees);effective[j].translation_cm=add(effective[j].translation_cm,input[j].translation_cm,before[j].translation_cm);}
    const auto &s=document_->skeletons.skins.at(skin);auto limits=runtime::ik_limits(s,input,effective);
    goals=runtime::pin_goals(pose_pins_,skin,state.skin_world.at(skin));
    for(const auto &g:goals) for(int j:runtime::ik_chain(s,g.joint)) if(std::find(chain.begin(),chain.end(),j)==chain.end()) chain.push_back(j);
    if(goals.empty()) return;auto solved=effective;const auto result=runtime::solve_ik(limits,solved,chain,goals,48);snapshot_.poses.at(skin)=runtime::ik_input(input,effective,solved);
    if(result.error>.005||result.angle_error_degrees>.5) pose_status_->setText(QStringLiteral("固定位置／角度受关节限位或可达范围限制，已保留最接近的姿势。"));
  }
  uint64_t focus_requests_=0;
  GroundPanel *ground_panel_=nullptr;
  uint64_t ground_requests_=0,ground_generation_=0;
  std::string ground_pending_;
  GroundSelection ground_pending_selection_;
  bool focus_pending_=false;
  bool edit_regression_test_=false;
  uint64_t regression_epoch_=0,regression_clicks_=0,regression_triangles_=0,regression_updates_=0;
  nlohmann::json regression_checks_=nlohmann::json::array();
  bool joint_selection_test_=false;
  std::array<int,2> finger_joints_{-1,-1};
  std::array<ir::Bounds,2> finger_bounds_;
  void joint_selection_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>180000) {finish_test(false,"同角色骨骼多选验证超时");return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4||state.selections!=tree_selection(hierarchy_)) return;
    auto check=[&](bool ok,const char *message) {if(!ok) finish_test(false,message);return ok;};
    auto same_point=[](auto a,auto b) {return std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)<1e-5;};
    auto focused=[&](const ir::Bounds &bounds) {return !state.selection_bounds.empty&&same_point(state.selection_bounds.minimum,bounds.minimum)&&same_point(state.selection_bounds.maximum,bounds.maximum)&&same_point(state.camera.target,bounds.center());};
    auto record=[&](const char *name) {regression_checks_.push_back({{"input",name},{"selected",state.selections},{"focus",{state.camera.target.x,state.camera.target.y,state.camera.target.z}}});};
    const Selection left{0,finger_joints_[0],-1},right{0,finger_joints_[1],-1},other{1,-1,-1};
    if(test_stage_==0) {
      if(!check(document_->catalog.targets.size()==2&&!document_->skeletons.skins.empty(),"骨骼多选夹具需要两个角色")) return;
      const auto &skin=document_->skeletons.skins.front();
      if(!check(skin.instance==document_->catalog.targets[0].instance,"夹具首个角色未绑定骨架")) return;
      for(size_t j=0;j<skin.joints.size();++j) {if(skin.joints[j].id=="lMid3") finger_joints_[0]=int(j);if(skin.joints[j].id=="rMid3") finger_joints_[1]=int(j);}
      if(!check(finger_joints_[0]>=0&&finger_joints_[1]>=0,"夹具缺少左右中指末节")) return;
      choose(0,finger_joints_[0]);++test_stage_;return;
    }
    if(test_stage_==1||test_stage_==2) {
      if(!check(!state.selection_bounds.empty,"指尖没有独立的聚焦范围")) return;
      finger_bounds_[size_t(test_stage_-1)]=state.selection_bounds;
      if(test_stage_==1) choose(0,finger_joints_[1]);
      else {choose(0,finger_joints_[0]);regression_epoch_=state.camera.epoch;renderer_->frame(finger_bounds_[1]);}
      ++test_stage_;return;
    }
    if(test_stage_==3||test_stage_==7) {
      if(state.camera.epoch<=regression_epoch_) return;
      renderer_->keyboard(VK_CONTROL,true);renderer_->pointer(state.width/2,state.height/2,false,true);++test_stage_;return;
    }
    if(test_stage_==4||test_stage_==8) {
      const int joint=finger_joints_[test_stage_==4?1:0];
      if(state.pointer_x!=state.width/2||state.pointer_y!=state.height/2||state.hovered_detail_joint!=joint) return;
      // SetCursorPos 生成的系统鼠标消息带物理按键状态；等定位完成后再模拟 Ctrl。
      if(!state.hover_toggle) {renderer_->keyboard(VK_CONTROL,true);return;}
      if(!check(state.hovered_joint==joint,"Ctrl 悬停退回角色整体或命中错误骨骼")) return;
      regression_clicks_=state.clicks;renderer_->pointer(state.width/2,state.height/2,true,true);++test_stage_;return;
    }
    if(test_stage_==5) {
      if(state.clicks<=regression_clicks_) return;
      if(!check(state.selections==std::vector<Selection>{left,right},"Ctrl 右手指尖没有保留左手指尖")) return;
      record("viewport-ctrl-add-right-fingertip");renderer_->keyboard(VK_CONTROL,false);
      regression_epoch_=state.camera.epoch;renderer_->keyboard('F',true);renderer_->keyboard('F',false);++test_stage_;return;
    }
    if(test_stage_==6) {
      if(state.camera.epoch<=regression_epoch_) return;
      auto merged=finger_bounds_[0];merged.add(finger_bounds_[1].minimum);merged.add(finger_bounds_[1].maximum);
      if(!check(focused(merged),"F 没有使用两侧指尖的范围并集")) return;
      record("focus-both-fingertips");screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"joint-multi-focus.png").wstring()));
      choose(1,-1,true);regression_epoch_=state.camera.epoch;renderer_->frame(finger_bounds_[0]);++test_stage_;return;
    }
    if(test_stage_==9) {
      if(state.clicks<=regression_clicks_) return;
      if(!check(state.selections==std::vector<Selection>{right,other},"跨角色激活后 Ctrl 取消左指尖丢失其余选择")) return;
      record("remove-left-fingertip-with-another-active-figure");renderer_->keyboard(VK_CONTROL,false);choose(1,-1,true);
      regression_epoch_=state.camera.epoch;renderer_->keyboard('F',true);renderer_->keyboard('F',false);++test_stage_;return;
    }
    if(test_stage_==10) {
      if(state.camera.epoch<=regression_epoch_) return;
      if(!check(state.selections==std::vector<Selection>{right}&&focused(finger_bounds_[1]),"取消左指尖后聚焦没有缩小到右指尖")) return;
      record("focus-remaining-right-fingertip");finish_test(true);
    }
  }
  void edit_regression_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>180000) {finish_test(false,"多选 / 细分 / 缩放验证超时");return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4) return;
    auto check=[&](bool ok,const char *message) {if(!ok) finish_test(false,message);return ok;};
    auto spin=[&](const char *id)->QDoubleSpinBox * {for(auto *s:parameters_->findChildren<QDoubleSpinBox *>()) if(s->property("parameterId").toString()==id) return s;return nullptr;};
    auto focused=[&] {const auto center=state.selection_bounds.center();return !state.selection_bounds.empty&&std::abs(center.x-state.camera.target.x)+std::abs(center.y-state.camera.target.y)+std::abs(center.z-state.camera.target.z)<1e-5;};
    if(test_stage_==0) {
      if(!check(document_->catalog.targets.size()==2&&state.effective_roots.size()==2,"回归夹具需要两个带 ERC 的立方体")) return;
      choose(0);parameters_->query(QStringLiteral("Scale（%）"));auto *s=spin("transform/general_scale");
      if(!check(s&&std::abs(s->value()-1)<1e-5,"属性面板没有显示 ERC 求值后的 1% 缩放")) return;
      regression_checks_.push_back({{"input","saved-2-percent-minus-ERC-1-percent"},{"display",s->value()}});
      choose(1,-1,true);regression_epoch_=state.camera.epoch;focus_selection();++test_stage_;return;
    }
    if(test_stage_==1) {
      if(state.selections.size()!=2||state.camera.epoch<=regression_epoch_) return;
      if(!check(focused()&&state.selection_bounds.minimum.x<.1f&&state.selection_bounds.maximum.x>2.5f,"多选聚焦未合并两个对象的范围")) return;
      regression_checks_.push_back({{"input","tree-multi-focus"},{"selected",state.selections.size()}});
      choose(1);regression_epoch_=state.camera.epoch;renderer_->frame(state.bounds.at(0));++test_stage_;return;
    }
    if(test_stage_==2||test_stage_==5) {
      if(state.camera.epoch<=regression_epoch_) return;
      regression_clicks_=state.clicks;renderer_->pointer(state.width/2,state.height/2,true,true);++test_stage_;return;
    }
    if(test_stage_==3) {
      if(state.clicks<=regression_clicks_||state.selections.size()!=2) return;
      if(!check(tree_selection(hierarchy_).size()==2,"视口 Ctrl 增选未同步层级树")) return;
      regression_epoch_=state.camera.epoch;renderer_->keyboard('F',true);renderer_->keyboard('F',false);++test_stage_;return;
    }
    if(test_stage_==4) {
      if(state.camera.epoch<=regression_epoch_) return;
      if(!check(focused(),"视口 F 未聚焦完整多选范围")) return;
      regression_checks_.push_back({{"input","viewport-ctrl-add-and-F"},{"selected",state.selections.size()}});
      regression_epoch_=state.camera.epoch;renderer_->frame(state.bounds.at(0));++test_stage_;return;
    }
    if(test_stage_==6) {
      if(state.clicks<=regression_clicks_||state.selections.size()!=1) return;
      if(!check(selected_==1,"Ctrl 取消选中后活动对象错误")) return;
      regression_checks_.push_back({{"input","viewport-ctrl-remove"},{"selected",state.selections.size()}});
      choose(0);parameters_->query(QStringLiteral("渲染细分等级"));auto *s=spin("SubDRenderLevel");if(!check(s!=nullptr,"未显示渲染细分等级控件")) return;
      regression_triangles_=state.adapter.unique_triangles;s->setValue(2);++test_stage_;return;
    }
    if(test_stage_==7) {
      if(!check(state.adapter.unique_triangles==regression_triangles_+144,"细分级别 1 → 2 未真正更新渲染拓扑")) return;
      regression_checks_.push_back({{"input","subdivision-level-1-to-2"},{"triangles_before",regression_triangles_},{"triangles_after",state.adapter.unique_triangles}});
      const auto &morphs=document_->catalog.targets[0].morphs;size_t a=0;while(a<morphs.size()&&morphs[a].channel_id!="A") ++a;
      if(!check(a<morphs.size(),"缺少形变参数 A")) return;regression_updates_=state.adapter.geometry_updates;set_morph(a,.5);++test_stage_;return;
    }
    if(test_stage_==8) {
      if(!check(state.adapter.geometry_updates>regression_updates_,"细分后 Morph 未更新渲染顶点")) return;
      regression_checks_.push_back({{"input","morph-after-subdivision"},{"geometry_updates",state.adapter.geometry_updates}});
      parameters_->query(QStringLiteral("Scale（%）"));auto *s=spin("transform/general_scale");if(!check(s!=nullptr,"找不到缩放控件")) return;s->setValue(100.01);++test_stage_;return;
    }
    if(test_stage_==9) {
      auto *s=spin("transform/general_scale");if(!check(s&&s->text()=="100.01"&&std::abs(root_scale()*snapshot_.values[0].transform.general_scale*.02*100-100.01)<.00003,"ERC 缩放写入或数值精度错误")) return;
      regression_checks_.push_back({{"input","absolute-scale-entry-after-ERC"},{"display",s->text().toStdString()}});
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"edit-regression.png").wstring()));finish_test(true);
    }
  }
  void focus_selection() {
    if(!renderer_||!document_) return;
    const auto state=renderer_->status();
    focus_pending_=state.selection_generation!=document_->generation||state.selected_target!=selected_||state.selected_joint!=selected_joint_||state.selections!=tree_selection(hierarchy_)||state.applied_revision!=snapshot_.revision;
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
  std::map<int,ir::Vec3> head_test_positions_;
  std::vector<size_t> head_test_occluders_;
  bool options_test_=false;int options_wait_=0;CameraState options_camera_;size_t options_geometry_=0;uint64_t options_epoch_=0;
  bool navigation_test_=false;
  bool interaction_test_=false;
  int interaction_case_=0,interaction_step_=0,interaction_target_=-1;
  double interaction_begin_=0,interaction_stop_=0,interaction_first_=0,interaction_idle_=0;
  RenderStatus interaction_before_;
  RenderStatus interaction_initial_;
  nlohmann::json interaction_checks_=nlohmann::json::array();
  int interaction_boundary_=0;
  double interaction_boundary_at_=0;
  RenderStatus interaction_boundary_before_;
  ir::RenderOptions interaction_options_;
  QByteArray interaction_layout_;
  nlohmann::json interaction_boundaries_=nlohmann::json::array();
  bool keep_open_after_test_=false;
  RenderStatus navigation_before_;
  CameraState navigation_after_input_;
  qint64 navigation_at_=0;
  nlohmann::json navigation_checks_=nlohmann::json::array();
  bool attachment_test_=false;std::vector<std::array<int,3>> attachment_cases_;nlohmann::json attachment_report_=nlohmann::json::array();
  bool capture_test_=false;
  QStringList selection_test_labels_;bool focus_only_test_=false;size_t selection_test_case_=0;
  int selection_test_key_=-1,selection_test_instance_=-1,selection_probe_=0;
  uint64_t selection_test_epoch_=0,selection_test_clicks_=0;QPoint selection_probe_point_;
  nlohmann::json selection_checks_=nlohmann::json::array();
  bool capture_head_=false;
  int capture_samples_=16;
  double capture_seconds_=0; qint64 capture_started_=0;bool capture_timed_=false;
  nlohmann::json capture_records_=nlohmann::json::array();
  QString visibility_label_;
  QStringList visibility_labels_;int visibility_case_=0;
  nlohmann::json visibility_checks_=nlohmann::json::array();
  int visibility_target_=-1;
  bool visibility_initial_=true;
  size_t visibility_geometry_updates_=0;
  size_t visibility_geometry_budget_=0;
  // Diagnostic baselines only; normal edits never save or restore child flags.
  std::vector<bool> visibility_previous_,visibility_original_,visibility_initial_values_;
  size_t visibility_sessions_=0,visibility_morph_evaluations_=0;
  bool visibility_local_graft_=false;
  QStringList capture_targets_;
  bool capture_front_=false;
  std::optional<std::array<float,6>> capture_view_;
  bool lifecycle_test_=false;
  std::filesystem::path scene_reopen_file_;
  std::filesystem::path wear_test_file_,wear_test_material_;
  size_t wear_test_material_operations_=0;
  std::filesystem::path rebuild_test_file_;
  size_t rebuild_host_=0,rebuild_first_=0;
  int rebuild_level_=0,rebuild_render_level_=0;
  double rebuild_begin_=0;
  RenderStatus rebuild_before_;
  nlohmann::json rebuild_checks_=nlohmann::json::array();
  bool subdivision_stress_test_=false;
  qint64 subdivision_recovery_started_=0;
  uint64_t subdivision_camera_epoch_=0;
  size_t subdivision_target_=0;
  int subdivision_original_=0;
  RenderStatus subdivision_initial_;
  std::map<int,size_t> subdivision_triangles_;
  nlohmann::json subdivision_checks_=nlohmann::json::array();
  size_t wear_test_first_=0,wear_test_host_=0;
  nlohmann::json scene_reopen_checks_=nlohmann::json::array();
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
  QWidget *light_power_row_=nullptr;
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
  void send() {if(powerpose_) powerpose_->cancel();if(document_){sync_geograft_visibility(hierarchy_,*document_,snapshot_);if(selected_>=0)sync_object_transform(*document_,snapshot_,size_t(selected_));}snapshot_.revision=next_revision();renderer_->edit(submitted_snapshot());}
  GroundSelection ground_objects() const {
    if(loading_||!document_)return {};
    const ObjectHierarchy nodes(*document_);std::vector<std::string> ids;
    for(auto *item:hierarchy_->selectedItems()) {
      const int key=item->data(0,Qt::UserRole).toInt();if(item->data(0,Qt::UserRole+2).toInt()>=0)continue;
      if(key==-4){const auto id=item->data(0,Qt::UserRole+3).toString().toStdString();if(group_node(*document_,id))ids.push_back(id);}
      else if(key>=0&&size_t(key)<nodes.targets.size())ids.push_back(nodes.targets[key]);
      else if(key<=-5&&size_t(-5-key)<nodes.instances.size())ids.push_back(nodes.instances[-5-key]);
    }
    return ground_selection(*document_,std::move(ids));
  }
  int ground_target() const {
    if(loading_||!document_||hierarchy_->selectedItems().empty())return -1;
    if(hierarchy_->selectedItems().size()>1||!selected_group_.empty())return ground_objects().instances.empty()?-1:-4;
    if(selected_light_>=0)return -1;
    if(selected_>=0&&size_t(selected_)<document_->catalog.targets.size())return selected_;
    if(selected_<=-5){const auto i=size_t(-5-selected_);const auto &scene=document_->loaded.scene;if(i<scene.instances.size()&&scene.instances[i].prototype>=0){const runtime::InstanceGroups groups(scene);return runtime::instance_selection(groups.roots[i]);}}
    return -1;
  }
  std::string ground_id(int target) const {return target>=0?document_->catalog.targets.at(size_t(target)).id:document_->loaded.scene.instances.at(size_t(-5-target)).id;}
  double ground_ratio(int target) const {if(target>=0)return snapshot_.values.at(size_t(target)).ground_alignment_ratio;if(target<=-5){const auto found=snapshot_.instance_ground.find(ground_id(target));if(found!=snapshot_.instance_ground.end())return found->second.ratio;}return 0;}
  double ground_offset(int target) const {if(target>=0)return snapshot_.values.at(size_t(target)).ground_alignment_offset_cm;if(target<=-5){const auto found=snapshot_.instance_ground.find(ground_id(target));if(found!=snapshot_.instance_ground.end())return found->second.offset_cm;}return 0;}
  bool ground_body_only(int target) const {if(target>=0)return snapshot_.values.at(size_t(target)).ground_alignment_body_only;if(target<=-5){const auto found=snapshot_.instance_ground.find(ground_id(target));if(found!=snapshot_.instance_ground.end())return found->second.body_only;}return false;}
  void request_ground() {
    if(ground_panel_)ground_panel_->commit();
    const int target=ground_target();if(target==-1)return;
    ground_pending_=target==-4?"selection":ground_id(target);ground_pending_selection_=target==-4?ground_objects():GroundSelection{};ground_generation_=document_->generation;
    statusBar()->showMessage(QStringLiteral("正在按对象当前世界包围盒对齐到地面…"));
  }
  void apply_ground(const RenderStatus &state) {
    if(ground_pending_.empty()) return;
    auto edit=history_edit(QStringLiteral("对齐到地面"));
    const int index=ground_target();
    if(!document_||ground_generation_!=document_->generation||index==-1||(index==-4?ground_pending_selection_.instances.empty()||ground_objects()!=ground_pending_selection_:!ground_pending_selection_.instances.empty()||ground_id(index)!=ground_pending_)) {ground_pending_.clear();return;}
    if(state.generation!=document_->generation) return;
    if(!state.edit_error.empty()||!state.resource_error.empty()) {ground_pending_.clear();statusBar()->showMessage(QStringLiteral("对象求值失败，未执行地面对齐。"),5000);return;}
    if(state.applied_revision!=snapshot_.revision||state.pose_dragging||state.pose_restoring||state.pending_payloads) return;
    if(index==-4){
      if(state.selection_generation!=document_->generation||state.selections!=tree_selection(hierarchy_)||state.instance_bounds.size()!=document_->loaded.scene.instances.size())return;
      ground_pending_.clear();try{if(align_ground_selection(*document_,snapshot_,ground_pending_selection_,state.instance_bounds,state.target_world,state.group_worlds)){send();select(selected_,selected_joint_,selected_light_);}statusBar()->showMessage(QStringLiteral("已按整体包围盒对齐到地面"),5000);}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);}return;
    }
    if(index<=-5){
      if(state.selection_generation!=document_->generation||state.selected_target!=index||state.selections!=tree_selection(hierarchy_))return;
      ground_pending_.clear();try{const auto shift=ground_vertical_shift(state.selection_bounds,ground_ratio(index),ground_offset(index));if(shift!=0){auto values=snapshot_.instance_ground;values[ground_id(index)].offset_m+=shift;validate_instance_ground(document_->loaded.scene,values);snapshot_.instance_ground=std::move(values);send();}statusBar()->showMessage(QStringLiteral("实例已对齐到地面"),5000);}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);}return;
    }
    if(size_t(index)>=state.target_world.size()||size_t(index)>=state.bounds.size())return;
    const auto &target=document_->catalog.targets[index];ground_pending_.clear();
    try {
        auto &value=snapshot_.values[index];const auto next=ground_aligned_transform(target,value.transform,document_->loaded.scene.instances[target.instance].transform,state.target_world[index],ground_bounds(*document_,size_t(index),state.bounds,state.visible,value.ground_alignment_body_only,&state.target_world,&snapshot_),value.ground_alignment_ratio,value.ground_alignment_offset_cm);
        if(next.translation_cm!=value.transform.translation_cm) {value.transform=next;send();select(selected_,selected_joint_,selected_light_);}
        statusBar()->showMessage(QStringLiteral("已对齐到地面：%1（%2%，固定偏移 %3 cm）").arg(text(target.label)).arg(value.ground_alignment_ratio*100).arg(value.ground_alignment_offset_cm),5000);
      } catch(const std::exception &e) {statusBar()->showMessage(text(e.what()),5000);}
  }
  void prune_pending_parameters() {
    std::erase_if(pending_parameters_,[&](const auto &p) {for(const auto &t:document_->catalog.targets) if(t.id==p.first.first) for(const auto &m:t.morphs) if(m.id==p.first.second&&m.unsupported.empty()) return false;return true;});
    apply_parameters_->setEnabled(!pending_parameters_.empty());
  }
  void apply_parameters() {if(pending_parameters_.empty()) return;auto edit=history_edit(QStringLiteral("应用暂存参数"));pending_parameters_.clear();apply_parameters_->setEnabled(false);send();}
  void refresh_parameter_catalog() {
    if(loading_||!document_||selected_<0) return;
    const auto previous=document_;const auto selected=size_t(selected_);const auto roots=roots_;loading_=true;refresh_parameters_->setEnabled(false);
    progress(QStringLiteral("正在刷新所选对象及穿戴物的参数目录…"));
    loader_=std::jthread([this,previous,selected,roots](std::stop_token stop) {
      try {
        auto updated=editor::refresh_parameters(*previous,selected,roots,[this,stop](const std::string &message){if(stop.stop_requested()) throw std::runtime_error("已取消刷新");progress(text(message));});
        QMetaObject::invokeMethod(this,[this,previous,updated] {
          if(closing_)return;loading_=false;if(document_!=previous) return;
          auto edit=history_edit(QStringLiteral("刷新参数目录"));
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
  void sync_selection() {
    if(history_&&!restoring_history_)history_->finish_gesture();
    {QSignalBlocker block(hierarchy_);for(QTreeWidgetItemIterator it(hierarchy_);*it;++it){auto font=(*it)->font(0);if(font.bold()!=(*it)->isSelected()){font.setBold((*it)->isSelected());(*it)->setFont(0,font);}}}
    auto *item=active_selection(hierarchy_);
    select(item?item->data(0,Qt::UserRole).toInt():-1,item?item->data(0,Qt::UserRole+1).toInt():-1,item?item->data(0,Qt::UserRole+2).toInt():-1);
  }
  void choose(int target,int joint=-1,bool toggle=false) {
    if(document_&&target>=0&&joint<0)target=int(ObjectTargets(*document_).primary.at(size_t(target)));
    for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) if((*it)->data(0,Qt::UserRole).toInt()==target&&(*it)->data(0,Qt::UserRole+1).toInt()==joint&&(*it)->data(0,Qt::UserRole+2).toInt()<0) {
      {QSignalBlocker block(hierarchy_);for(auto *p=(*it)->parent();p;p=p->parent()) p->setExpanded(true);choose_item(hierarchy_,*it,toggle);hierarchy_->scrollToItem(*it);}sync_selection();return;
    }
    {QSignalBlocker block(hierarchy_);choose_item(hierarchy_,nullptr,toggle);}sync_selection();
  }
  void rebuild_hierarchy() {
    QSignalBlocker block(hierarchy_);hierarchy_->clear();
    if(renderer_) renderer_->select(document_?document_->generation:0,-1,-1,{});
    std::map<std::string,QTreeWidgetItem *> objects;
    std::set<std::string> containers;
    auto identify=[](QTreeWidgetItem *item,int target,int joint=-1,int light=-1) {item->setData(0,Qt::UserRole,target);item->setData(0,Qt::UserRole+1,joint);item->setData(0,Qt::UserRole+2,light);};
    if(!document_)return;
    if(physics_only_&&physics_only_->isChecked()){
      for(size_t i=0;i<document_->catalog.targets.size();++i)if(snapshot_.values[i].physics.enabled&&physics_eligible(*document_,i)){
        const auto &target=document_->catalog.targets[i];auto *item=new QTreeWidgetItem(hierarchy_,{text(target.label)});identify(item,int(i));item->setData(0,Qt::UserRole+3,text(target.id));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,snapshot_.values[i].visible?Qt::Checked:Qt::Unchecked);
      }sync_geograft_visibility(hierarchy_,*document_,snapshot_);return;
    }
    std::vector<QTreeWidgetItem *> items;const ObjectTargets object_targets(*document_);
    for(const auto &node:document_->loaded.nodes) if(node.group) {
      auto *item=new QTreeWidgetItem(hierarchy_,{text(node.label.empty()?node.id:node.label)});identify(item,-4);objects[node.id]=item;
      item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,node_properties(*document_,snapshot_,node.id).visible?Qt::Checked:Qt::Unchecked);
      item->setToolTip(0,QStringLiteral("场景组"));
      containers.insert(node.id);
    }
    for(size_t i=0;i<document_->catalog.targets.size();++i) {
      if(object_targets.primary[i]!=i){items.push_back(items.at(object_targets.primary[i]));continue;}
      const auto &target=document_->catalog.targets[i];auto *item=new QTreeWidgetItem(hierarchy_,{text(target.label)});identify(item,int(i));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,snapshot_.values[i].visible?Qt::Checked:Qt::Unchecked);items.push_back(item);
      QVariantList members;for(auto member:object_targets.members[i])members.push_back(int(member));item->setData(0,Qt::UserRole+4,members);
      for(const auto &object:document_->loaded.objects) if(object.instance==target.instance) objects[object.id]=item;
      for(const auto &skin:document_->skeletons.skins) if(skin.instance==target.instance) {
        std::vector<QTreeWidgetItem *> bones;
        for(size_t j=0;j<skin.joints.size();++j) {const auto &joint=skin.joints[j];auto *bone=joint.parent<0?item:new QTreeWidgetItem(bones[size_t(joint.parent)],{text(joint.label)});if(joint.parent>=0) identify(bone,int(i),int(j));bones.push_back(bone);if(!joint.scene_id.empty()) objects.emplace(joint.scene_id,bone);}
      }
    }
    const runtime::InstanceGroups instance_groups(document_->loaded.scene);
    for(uint32_t i=0;i<document_->loaded.scene.instances.size();++i) {
      const auto &instance=document_->loaded.scene.instances[i];if(instance.prototype<0) continue;
      if(instance_groups.roots[i]!=i) continue;
      auto &parent=objects[instance.instance_node];
      if(!parent) {
        auto n=std::find_if(document_->loaded.nodes.begin(),document_->loaded.nodes.end(),[&](const auto &n){return n.id==instance.instance_node;});
        parent=new QTreeWidgetItem(hierarchy_,{text(n==document_->loaded.nodes.end()?instance.instance_node:n->label)});identify(parent,-4);containers.insert(instance.instance_node);
      }
      auto *item=instance.instance_group==instance.instance_node?parent:new QTreeWidgetItem(parent,{text(instance.instance_label)});
      identify(item,runtime::instance_selection(i));item->setData(0,Qt::UserRole+3,text(instance.instance_group));item->setToolTip(0,QStringLiteral("Instance · ")+text(instance.instance_label));
    }
    // 组自身也要挂在原父组下；先建全体节点，再连接，避免依赖文件顺序。
    for(const auto &[id,item]:objects) item->setData(0,Qt::UserRole+3,text(id));
    for(const auto &node:document_->loaded.nodes) if(containers.contains(node.id)) {
      auto parent=node.parent;std::set<std::string> visited{node.id};
      while(parent.starts_with('#')&&visited.insert(parent.substr(1)).second) {
        const auto id=parent.substr(1);
        if(objects.contains(id)) {auto *item=objects.at(node.id);hierarchy_->takeTopLevelItem(hierarchy_->indexOfTopLevelItem(item));objects.at(id)->addChild(item);break;}
        auto found=std::find_if(document_->loaded.nodes.begin(),document_->loaded.nodes.end(),[&](const auto &n){return n.id==id;});if(found==document_->loaded.nodes.end()) break;parent=found->parent;
      }
    }
    for(size_t i=0;i<items.size();++i) {
      if(object_targets.primary[i]!=i)continue;
      const auto &target=document_->catalog.targets[i];auto ancestors=target.ancestors;if(ancestors.empty()) ancestors.push_back(target.parent);
      for(const auto &parent:ancestors) if(parent.starts_with('#')&&objects.contains(parent.substr(1))&&objects[parent.substr(1)]!=items[i]) {
        hierarchy_->takeTopLevelItem(hierarchy_->indexOfTopLevelItem(items[i]));objects[parent.substr(1)]->addChild(items[i]);break;
      }
    }
    for(int option=0;option<2;++option) {const auto &node=option==0?snapshot_.options.environment:snapshot_.options.tonemapper;if(!node.id.empty()) {auto *item=new QTreeWidgetItem(hierarchy_,{text(node.label)});identify(item,option==0?-2:-3);}}
    for(size_t i=0;i<snapshot_.lights.size();++i) {auto *item=new QTreeWidgetItem(hierarchy_,{QStringLiteral("灯光 · ")+text(snapshot_.lights[i].id)});identify(item,-1,-1,int(i));}
    sync_geograft_visibility(hierarchy_,*document_,snapshot_);
  }
  nlohmann::json hierarchy_report() const {
    auto rows=nlohmann::json::array();
    for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) {
      auto *item=*it;rows.push_back({{"id",item->data(0,Qt::UserRole+3).toString().toStdString()},{"label",item->text(0).toStdString()},
        {"parent",item->parent()?item->parent()->data(0,Qt::UserRole+3).toString().toStdString():""},{"target",item->data(0,Qt::UserRole).toInt()}});
    }
    return rows;
  }
  void set_visible(size_t target,bool visible) {
    if(loading_||!document_||snapshot_.values.at(target).visible==visible) return;
    if(!geograft_visibility_editable(*document_,snapshot_,target)){sync_geograft_visibility(hierarchy_,*document_,snapshot_);return;}
    auto edit=history_edit(QStringLiteral("切换对象显隐"));
    const ObjectTargets objects(*document_);target=objects.primary.at(target);for(auto member:objects.members[target])snapshot_.values[member].visible=visible;
    {QSignalBlocker block(hierarchy_);for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) if((*it)->data(0,Qt::UserRole).toInt()==int(target)&&(*it)->data(0,Qt::UserRole+1).toInt()<0) (*it)->setCheckState(0,visible?Qt::Checked:Qt::Unchecked);}
    send();
    parameters_->refresh(0);
  }
  void set_selectable(size_t target,bool value) {
    if(loading_||!document_||water::find(document_->waters,document_->catalog.targets.at(target).id)||snapshot_.values.at(target).selectable==value)return;
    auto edit=history_edit(QStringLiteral("切换对象可选中"));const ObjectTargets objects(*document_);target=objects.primary.at(target);
    for(auto member:objects.members[target])snapshot_.values[member].selectable=value;send();parameters_->refresh(0);
  }
  void set_node_property(const std::string &id,bool selectable,bool value) {
    if(loading_||!document_||!group_node(*document_,id))return;
    auto state=node_properties(*document_,snapshot_,id);auto &flag=selectable?state.selectable:state.visible;if(flag==value)return;
    auto edit=history_edit(selectable?QStringLiteral("切换组可选中"):QStringLiteral("切换组显隐"));flag=value;snapshot_.node_properties[id]=state;
    {QSignalBlocker block(hierarchy_);for(QTreeWidgetItemIterator it(hierarchy_);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()==-4&&(*it)->data(0,Qt::UserRole+3).toString().toStdString()==id)(*it)->setCheckState(0,state.visible?Qt::Checked:Qt::Unchecked);}
    send();parameters_->refresh(0);
  }
  void append_node_controls(std::vector<ParameterControl> &controls,int target,const std::string &id={}) {
    for(bool selectable:{false,true}){ParameterControl c;c.id=selectable?"Selectable":"Visible";c.favorite_name=c.id;c.label=selectable?"可选中 · Selectable":"可见 · Visible";c.group="/General/Display";c.choices={"关闭","开启"};c.minimum=0;c.maximum=1;c.initial=id.empty()?(selectable?document_->catalog.targets.at(size_t(target)).initial_selectable:document_->catalog.targets.at(size_t(target)).initial_visible.value_or(true)):(selectable?group_node(*document_,id)->selectable:group_node(*document_,id)->visible);
      c.detail=selectable?"关闭后不能在视口中点击选中，也不会在鼠标悬停时黄色高亮；仍可从场景树选择并重新开启。":"父对象或组关闭时隐藏其子对象，重新开启后保留子对象各自的显隐设置。";
      const bool native_water=id.empty()&&water::find(document_->waters,document_->catalog.targets.at(size_t(target)).id);
      if(selectable&&native_water)c.detail="水体不参与视口点击选中和悬停高亮；可从场景树选择并调整水体参数。";
      c.enabled=selectable?!native_water:!id.empty()||geograft_visibility_editable(*document_,snapshot_,size_t(target));
      c.read=[this,target,id,selectable]{if(!id.empty()){const auto state=node_properties(*document_,snapshot_,id);return double(selectable?state.selectable:state.visible);}const auto &v=snapshot_.values.at(size_t(target));return double(selectable?v.selectable:v.visible);};
      c.write=[this,target,id,selectable](double value){if(!id.empty())set_node_property(id,selectable,value!=0);else if(selectable)set_selectable(size_t(target),value!=0);else set_visible(size_t(target),value!=0);};controls.push_back(std::move(c));
    }
  }
  void add_light() {
    if(loading_) return;
    auto edit=history_edit(QStringLiteral("创建面光源"));
    auto next=document_?std::make_shared<Document>(*document_):std::make_shared<Document>();next->generation=++generation_;
    next->loaded.scene.lights=snapshot_.lights;
    ir::AreaLight light;size_t id=next->generation;auto exists=[&] {return std::any_of(next->loaded.scene.lights.begin(),next->loaded.scene.lights.end(),[&](const auto &old) {return old.id==light.id;});};
    do {light.id="Area "+std::to_string(id++);} while(exists());light.transform=ir::Transform::translate({0,-2,3});
    next->loaded.scene.lights.push_back(light);document_=next;snapshot_.generation=next->generation;snapshot_.lights=next->loaded.scene.lights;++snapshot_.revision;
    rebuild_hierarchy();renderer_->set_document(document_,submitted_snapshot(),false);hierarchy_->setCurrentItem(hierarchy_->topLevelItem(hierarchy_->topLevelItemCount()-1));
  }
  void delete_selection() {
    if(loading_||!document_) return;
    const auto selection=tree_deletion_selection(hierarchy_);if(selection.empty())return;
    auto edit=history_edit(QStringLiteral("删除对象及子对象"));
    try {
      auto next=std::make_shared<Document>(*document_);auto snapshot=snapshot_;size_t removed=0;
      removed=remove_selection(*next,snapshot,selection.targets,selection.nodes,selection.lights);
      std::erase_if(snapshot.subdivision_levels,[&](const auto &entry) {return std::none_of(next->loaded.scene.meshes.begin(),next->loaded.scene.meshes.end(),[&](const auto &mesh){return mesh.id==entry.first;});});
      next->generation=++generation_;snapshot.generation=next->generation;
      parameters_->bind(nullptr,nullptr);document_=std::move(next);snapshot_=std::move(snapshot);
      select(-1);rebuild_hierarchy();frame_pending_=false;pose_report_=nullptr;pose_status_->clear();load_error_.clear();
      prune_pending_parameters();renderer_->set_document(document_,submitted_snapshot(),false);
      statusBar()->showMessage(removed?QStringLiteral("已删除 %1 个对象及其关联资源").arg(removed):QStringLiteral("已删除灯光"));
    } catch(const std::exception &e) {QMessageBox::warning(this,QStringLiteral("无法删除对象"),text(e.what()));}
  }
  void clear_scene() {
    if(loading_) return;
    auto edit=history_edit(QStringLiteral("新建空场景"));
    scene_identity_=++scene_serial_;
    extension_file_.clear();pose_pins_.clear();renderer_->pose_pins({});
    pending_parameters_.clear();apply_parameters_->setEnabled(false);
    parameters_->bind(nullptr,nullptr);document_=std::make_shared<Document>();document_->generation=++generation_;
    snapshot_=initial_snapshot(*document_);select(-1);rebuild_hierarchy();frame_pending_=false;pose_report_=nullptr;pose_status_->clear();load_error_.clear();
    renderer_->set_document(document_,snapshot_,false);
  }
  void apply_combined_file(const std::filesystem::path &file,const nlohmann::json &data) {
    auto edit=history_edit(QStringLiteral("应用复合预设"));
    if(loading_||!document_||selected_<0) throw std::runtime_error("请先选中复合预设的目标对象");
    const auto skin=selected_skin();if(skin<0) throw std::runtime_error("姿势 / 形态通道需要带骨骼的目标角色");
    auto next=std::make_shared<Document>(*document_);auto snapshot=snapshot_;
    auto applied=daz::apply_pose(daz::parse_pose(data),next->skeletons.skins.at(size_t(skin)),snapshot.poses.at(size_t(skin)),next->catalog.targets.at(size_t(selected_)),snapshot.values.at(size_t(selected_)));
    apply_materials(*next,size_t(selected_),daz::load(file,{roots_,false}),&snapshot);
    snapshot.poses[size_t(skin)]=std::move(applied.joints);snapshot.values[size_t(selected_)]=std::move(applied.properties);
    next->generation=++generation_;snapshot.generation=next->generation;++snapshot.revision;next->loaded.scene.lights=snapshot.lights;
    parameters_->bind(nullptr,nullptr);document_=std::move(next);snapshot_=std::move(snapshot);pose_report_=std::move(applied.report);
    std::ofstream(output_/"pose-report.json")<<pose_report_.dump(2);
    pose_status_->setText(QStringLiteral("已应用复合 DUF 的材质与姿势 / 形态；%1 项通道未应用。").arg(pose_report_["unapplied"].size()));
    select(selected_,selected_joint_);renderer_->set_document(document_,submitted_snapshot(),false);
    if(!self_test_) browser_->record_use(QString::fromStdWString(file.wstring()),content_category(data));
  }
  void apply_surface_material_file(const std::filesystem::path &file,const std::vector<MaterialSurface> &surfaces){
    if(loading_||!document_)return;
    auto edit=history_edit(QStringLiteral("应用表面材质预设"));
    try{const auto asset=daz::content_asset(file,roots_);auto preset=daz::load(asset,{roots_,false});auto next=std::make_shared<Document>(*document_);auto snapshot=snapshot_;
      apply_surface_materials(*next,snapshot,preset,surfaces);next->generation=++generation_;next->loaded.scene.lights=snapshot.lights;document_=std::move(next);snapshot_=std::move(snapshot);snapshot_.generation=document_->generation;++snapshot_.revision;
      select(selected_,selected_joint_,selected_light_);renderer_->set_document(document_,submitted_snapshot(),false);if(!self_test_)browser_->record_use(QString::fromStdWString(file.wstring()),content_category(*daz::document_view(asset)));
    }catch(const std::exception &e){QMessageBox::warning(this,QStringLiteral("材质预设应用失败"),text(e.what()));}
  }
  void apply_material_file(const std::filesystem::path &file) {
    auto edit=history_edit(QStringLiteral("应用材质预设"));
    if(loading_||selected_<0||!document_) throw std::runtime_error("请先选中材质预设的目标对象");
    auto preset=daz::load(file,{roots_,false});auto next=std::make_shared<Document>(*document_);next->generation=++generation_;
    auto snapshot=snapshot_;apply_materials(*next,size_t(selected_),preset,&snapshot);snapshot_=std::move(snapshot);
    next->loaded.scene.lights=snapshot_.lights;document_=next;snapshot_.generation=next->generation;++snapshot_.revision;select(selected_,selected_joint_);renderer_->set_document(document_,submitted_snapshot(),false);
    if(!self_test_) browser_->record_use(QString::fromStdWString(file.wstring()),content_category(*daz::document_view(file)));
  }
  int selected_skin() const {
    if(selected_<0||!document_) return -1;const auto instance=document_->catalog.targets[size_t(selected_)].instance;
    for(size_t i=0;i<document_->skeletons.skins.size();++i) if(document_->skeletons.skins[i].instance==instance) return int(i);
    return -1;
  }
  bool apply_pose_file(const std::filesystem::path &file,bool partial=false) {
    auto edit=history_edit(partial?QStringLiteral("应用局部姿势"):QStringLiteral("应用姿势与形态预设"));
    try {
      if(loading_) throw std::runtime_error("请等待场景加载完成后应用姿势");
      const auto index=selected_skin();if(index<0) throw std::runtime_error("请先选中一个带骨骼蒙皮的角色");
      if(partial&&(selected_joint_<0||hierarchy_->selectedItems().size()!=1))throw std::runtime_error("局部姿势需要单选一个角色的骨骼部位");
      const auto &skin=document_->skeletons.skins[size_t(index)];
      const auto data=daz::document_view(file);
      auto applied=daz::apply_pose(daz::parse_pose(*data),skin,snapshot_.poses[size_t(index)],document_->catalog.targets[size_t(selected_)],snapshot_.values[size_t(selected_)],partial?selected_joint_:-1);
      snapshot_.poses[size_t(index)]=std::move(applied.joints);snapshot_.values[size_t(selected_)]=std::move(applied.properties);pose_report_=applied.report;
      std::ofstream(output_/"pose-report.json")<<pose_report_.dump(2);
      const auto skipped=pose_report_["unapplied"].size();
      pose_status_->setText(QStringLiteral("预设：%1\n已应用 %2 个骨骼通道、%3 个 Morph 通道；%4 项未应用。").arg(QString::fromStdWString(file.stem().wstring())).arg(pose_report_["applied_bone_channels"].get<int>()).arg(pose_report_["applied_morph_channels"].get<int>()).arg(skipped));
      if(partial)pose_status_->setText(QStringLiteral("局部姿势：%1 及其后代；保留范围外 %2 项通道。\n").arg(text(skin.joints[size_t(selected_joint_)].label)).arg(pose_report_["excluded_channels"].get<int>())+pose_status_->text());
      pose_status_->setToolTip(QString::fromStdWString(file.wstring()));select(selected_,selected_joint_);send();frame_pending_=self_test_;
      if(!self_test_) browser_->record_use(QString::fromStdWString(file.wstring()),content_category(*data));return true;
    } catch(const std::exception &e) {
      if(self_test_) finish_test(false,e.what());else QMessageBox::warning(this,QStringLiteral("无法应用预设"),text(e.what()));return false;
    }
  }
  void reset_pose() {
    auto edit=history_edit(QStringLiteral("重置姿势"));
    pose_pins_.clear();renderer_->pose_pins({});
    if(loading_) return;const auto index=selected_skin();if(index<0) return;
    snapshot_.poses[size_t(index)]=document_->skeletons.skins[size_t(index)].initial;send();frame_pending_=true;
    pose_status_->setText(QStringLiteral("已恢复载入时的骨骼姿势；Morph 保持当前值。"));pose_report_=nullptr;
  }
  void open_asset(const std::filesystem::path &entry,bool partial=false) {
    if(loading_) {statusBar()->showMessage(QStringLiteral("正在加载，请稍候…"));return;}
    try {const auto file=daz::content_asset(entry,roots_);if(daz::entry_key(file.extension())==".dufex"){load(file,false,false,{},entry);return;}const auto data=daz::read_document_file(file);
      const auto contents=daz::inspect_contents(data);
      if(contents.instantiate) {
        const bool wear=contents.requires_selection||data.value("asset_info",nlohmann::json::object()).value("type","")=="wearable";
        std::string target;
        if(wear) {
          if(!document_) throw std::runtime_error("请先加载并选中兼容角色，再添加服装、头发或角色附件");
          int selected=selected_;
          if(selected<0) {
            for(size_t t=0;t<document_->catalog.targets.size();++t) {
              bool character=false;try {character=attachment_host(*document_,t)==t;} catch(const std::exception &) {}
              if(character) {if(selected>=0) throw std::runtime_error("场景有多个角色，请先选择穿戴目标");selected=int(t);}
            }
          }
          if(selected<0) throw std::runtime_error("请先选中兼容角色");
          target=document_->catalog.targets.at(attachment_host(*document_,size_t(selected))).id;
        }
        // 默认空场景的首次导入沿用正常打开流程；已有对象或环境编辑时才追加。
        const bool append=document_&&(!document_->loaded.scene.instances.empty()||!snapshot_.lights.empty()||!document_->source_file.empty()||!document_->operations.empty()||ir::options_json(snapshot_.options)!=ir::options_json(ir::RenderOptions{}));
        load(file,false,append,target,entry);
      }
      else if(partial&&contents.properties) apply_pose_file(file,true);
      else if(contents.properties&&contents.materials) apply_combined_file(file,data);
      else if(contents.properties) apply_pose_file(file,partial);
      else if(contents.materials) apply_material_file(file);
      else throw std::runtime_error("DUF 中没有当前可实例化或应用的内容；资产定义需要 scene 实例引用");
    } catch(const std::exception &e) {if(self_test_) finish_test(false,e.what());else QMessageBox::warning(this,QStringLiteral("无法打开 DUF"),text(e.what()));}
  }
  void change_attachment(bool detach,int requested_host=-1) {
    if(loading_||!document_||selected_<0) return;
    try {
      int host=detach?-1:requested_host;
      if(!detach&&host<0) {
        QStringList names;std::vector<size_t> targets;
        for(size_t t=0;t<document_->catalog.targets.size();++t) {
          bool character=false;try {character=attachment_host(*document_,t)==t;} catch(const std::exception &) {}
          if(character) {targets.push_back(t);names.push_back(text(document_->catalog.targets[t].label+" ["+document_->catalog.targets[t].id+"]"));}
        }
        if(targets.empty()) throw std::runtime_error("场景中没有可绑定的角色");
        bool accepted=false;const auto choice=QInputDialog::getItem(this,QStringLiteral("绑定 / 更换附件目标"),QStringLiteral("目标角色（同次加载的整套附件一起更换）："),names,0,false,&accepted);
        if(!accepted) return;host=int(targets.at(size_t(names.indexOf(choice))));
      }
      const auto follower=selected_;const auto previous=document_;const auto generation=++generation_;
      loading_=true;open_->setEnabled(false);project_action_->setEnabled(false);statusBar()->showMessage(QStringLiteral("正在更新附件绑定…"));
      loader_=std::jthread([this,previous,follower,host,detach,generation](std::stop_token) {
        try {
          auto next=std::make_shared<Document>(*previous);fit_attachment(*next,size_t(follower),host);next->generation=generation;
          QMetaObject::invokeMethod(this,[this,next,follower,detach] {
            if(closing_)return;auto edit=history_edit(detach?QStringLiteral("解除附件挂接"):QStringLiteral("更换附件绑定目标"));
            parameters_->bind(nullptr,nullptr);document_=next;snapshot_.generation=document_->generation;++snapshot_.revision;
            loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);
            rebuild_hierarchy();renderer_->set_document(document_,submitted_snapshot(),false);choose(follower);
            statusBar()->showMessage(detach?QStringLiteral("已解除挂接并更新角色表面；附件自身参数保留。") : QStringLiteral("已更新附件目标。"));
          },Qt::QueuedConnection);
        } catch(const std::exception &e) {
          const std::string error=e.what();QMetaObject::invokeMethod(this,[this,error] {
            loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);
            if(self_test_) finish_test(false,error);else QMessageBox::warning(this,QStringLiteral("无法更新附件"),text(error));
          },Qt::QueuedConnection);
        }
      });
    } catch(const std::exception &e) {if(self_test_) finish_test(false,e.what());else QMessageBox::warning(this,QStringLiteral("无法更新附件"),text(e.what()));}
  }
  void copy_selected_outfit() {
    if(loading_)return;
    try {
      if(!document_||selected_<0||selected_joint_>=0)throw std::runtime_error("请先在场景中选中一个角色模型，再使用复制穿搭");
      const auto characters=outfit_characters(*document_);if(std::find(characters.begin(),characters.end(),size_t(selected_))==characters.end())throw std::runtime_error("请先选中 Genesis 8 / 8.1 角色模型");
      OutfitDialog dialog(*document_,snapshot_,size_t(selected_),this);if(dialog.exec()!=QDialog::Accepted)return;
      const auto source=dialog.source();const auto clothing=dialog.clothing(),hosts=dialog.hosts();const auto previous=document_;const auto state=snapshot_;const auto generation=++generation_;
      loading_=true;open_->setEnabled(false);project_action_->setEnabled(false);statusBar()->showMessage(QStringLiteral("正在复制服装并绑定到目标角色…"));
      loader_=std::jthread([this,previous,state,source,clothing,hosts,generation](std::stop_token){
        try {
          auto next=std::make_shared<Document>(*previous);auto snapshot=state;const auto result=copy_outfit(*next,snapshot,source,clothing,hosts);next->generation=generation;snapshot.generation=generation;
          QMetaObject::invokeMethod(this,[this,next,snapshot=std::move(snapshot),source,result]()mutable{
            if(closing_)return;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);
            if(result.copied){auto edit=history_edit(QStringLiteral("复制穿搭"));parameters_->bind(nullptr,nullptr);document_=next;snapshot_=std::move(snapshot);snapshot_.revision=next_revision();rebuild_hierarchy();renderer_->set_document(document_,submitted_snapshot(),false);choose(int(source));}
            statusBar()->showMessage(QStringLiteral("已复制 %1 件服装，跳过 %2 件已穿戴的服装。").arg(qulonglong(result.copied)).arg(qulonglong(result.skipped)));
          },Qt::QueuedConnection);
        }catch(const std::exception &e){const std::string error=e.what();QMetaObject::invokeMethod(this,[this,error]{if(closing_)return;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);QMessageBox::warning(this,QStringLiteral("无法复制穿搭"),text(error));},Qt::QueuedConnection);}
      });
    }catch(const std::exception &e){QMessageBox::warning(this,QStringLiteral("无法复制穿搭"),text(e.what()));}
  }
  void update_libraries() {
    roots_.clear();for(const auto &root:project_.content_roots) roots_.push_back(file_path(root));
    browser_->set_roots(project_.content_roots);
  }
  void project_settings() {
    if(loading_) return;
    auto roots=project_.content_roots;auto render=application_settings_.render;
    application_settings_.ui_percent=ui_scale_->percent();application_settings_.viewport=viewport_settings_->value();
    edit_project_settings(this,project_,application_settings_,{},!self_test_,[&](bool saved) {
      if(history_)history_->set_limit(project_.history_limit);
      if(roots!=project_.content_roots) update_libraries();
      if(ui_scale_->percent()!=application_settings_.ui_percent) ui_scale_->set_percent(application_settings_.ui_percent);
      viewport_settings_->set(application_settings_.viewport);
      renderer_->render_quality(application_settings_.render);
      renderer_->physics_options(project_.physics);
      const auto message=saved?QStringLiteral("项目与应用设置已保存。"):QStringLiteral("设置已应用，尚未保存。");
      statusBar()->showMessage(message+(render!=application_settings_.render?QStringLiteral("正在释放旧渲染资源并应用新画质…"):QString{}));
      roots=project_.content_roots;render=application_settings_.render;
    });
  }
  void set_morph(size_t morph,double value) {
    if(selected_<0) return;
    auto edit=history_edit(QStringLiteral("修改参数：")+text(document_->catalog.targets[size_t(selected_)].morphs.at(morph).label));
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
  void set_subdivision(size_t target,int value) {
    if(loading_||!document_||subdivision_level(*document_,snapshot_,target)==value) return;
    auto edit=history_edit(QStringLiteral("修改细分等级"));
    const auto begin=now();
    const auto &mesh=subdivision_mesh(*document_,target);
    try {runtime::validate_subdivision_budget(mesh,value);} catch(const std::exception &e) {load_error_=text(e.what());statusBar()->showMessage(load_error_);return;}
    snapshot_.subdivision_levels[mesh.id]=value;load_error_.clear();send();
    if(!rebuild_test_file_.empty()) renderer_->trace("subdivision_ui_submit",(now()-begin)*1000);
  }
  void rebuild_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000) {finish_test(false,"场景重建诊断超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()) {finish_test(false,state.error+state.edit_error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4) return;
    static const char *names[]={"viewport_subdivision_change","viewport_subdivision_restore","subdivision_base","subdivision_restore","wearable_import","wearable_delete_one"};
    if(rebuild_begin_) {
      if(state.requested_epoch<=rebuild_before_.requested_epoch) return;
      const int completed=test_stage_-1;
      if(completed<4&&state.mesh_hashes!=rebuild_before_.mesh_hashes) {finish_test(false,"细分设置改变了基础形变结果");return;}
      if(state.sessions!=rebuild_before_.sessions) {finish_test(false,"局部修改重新创建了渲染会话");return;}
      if(completed<4&&state.collision.evaluations!=rebuild_before_.collision.evaluations) {finish_test(false,"细分修改重复计算碰撞");return;}
      rebuild_checks_.push_back({{"input",names[completed]},{"begin",rebuild_begin_},{"observed_present",state.present_time},{"before_epoch",rebuild_before_.requested_epoch},{"final_epoch",state.presented_epoch},
        {"sessions_added",state.sessions-rebuild_before_.sessions},{"triangles_before",rebuild_before_.adapter.unique_triangles},{"triangles_after",state.adapter.unique_triangles},
        {"targets",document_->catalog.targets.size()},{"base_meshes_unchanged",state.mesh_hashes==rebuild_before_.mesh_hashes}});
      std::ofstream(output_/"rebuild-checks.json")<<rebuild_checks_.dump(2);
      rebuild_begin_=0;
    }
    if(test_stage_==6) {finish_test(true);return;}
    if(test_stage_==0) {
      bool found=false;for(size_t t=0;t<document_->catalog.targets.size();++t) try {if(attachment_host(*document_,t)==t) {rebuild_host_=t;found=true;break;}} catch(const std::exception &) {}
      if(!found) {finish_test(false,"诊断场景没有可穿戴角色");return;}
      const auto &mesh=document_->loaded.scene.meshes.at(document_->loaded.scene.instances.at(document_->catalog.targets[rebuild_host_].instance).mesh);
      if(!mesh.subdivision.enabled) {finish_test(false,"诊断需要启用细分的角色");return;}
      rebuild_level_=subdivision_level(*document_,snapshot_,rebuild_host_);rebuild_render_level_=mesh.subdivision.render_level;rebuild_first_=document_->catalog.targets.size();
    }
    choose(int(rebuild_host_));rebuild_before_=state;rebuild_begin_=now();renderer_->trace(names[test_stage_]);
    const int action=test_stage_++;
    if(action==0) set_subdivision(rebuild_host_,rebuild_level_==1?2:1);
    else if(action==1) set_subdivision(rebuild_host_,rebuild_level_);
    else if(action==2) set_subdivision(rebuild_host_,0);
    else if(action==3) set_subdivision(rebuild_host_,rebuild_level_);
    else if(action==4) open_asset(rebuild_test_file_);
    else {
      if(document_->catalog.targets.size()<=rebuild_first_) {finish_test(false,"诊断服装未追加");return;}
      choose(int(rebuild_first_));delete_->trigger();
    }
  }
  void subdivision_stress_tick(const RenderStatus &state) {
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000) {finish_test(false,"细分反复切换验证超时");return;}
    if(test_stage_==0&&!state.error.empty()&&state.error.find("细分")!=std::string::npos&&document_&&!loading_) {
      if(subdivision_recovery_started_) {if(QDateTime::currentMSecsSinceEpoch()-subdivision_recovery_started_>30000) finish_test(false,"降低等级后没有从细分错误恢复");return;}
      subdivision_recovery_started_=QDateTime::currentMSecsSinceEpoch();
      subdivision_checks_.push_back({{"input","recover-initial-subdivision-error"},{"error",state.error}});
      for(size_t t=0;t<document_->catalog.targets.size();++t) if(!document_->loaded.scene.meshes[document_->loaded.scene.instances[document_->catalog.targets[t].instance].mesh].polygons.empty()) {set_subdivision(t,1);return;}
    }
    if(!state.error.empty()||!state.edit_error.empty()) {finish_test(false,state.error+state.edit_error);return;}
    if(loading_||!document_||state.generation!=document_->generation||state.applied_revision!=snapshot_.revision||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<4) return;
    constexpr int levels[]={0,1,2,3,2,1,0,2,3,0,1,2};
    if(!subdivision_camera_epoch_) {renderer_->orbit(.2f,0);subdivision_camera_epoch_=renderer_->input_camera().epoch;return;}
    if(state.camera.epoch<subdivision_camera_epoch_) return;
    if(test_stage_==0) {
      bool found=false;for(size_t t=0;t<document_->catalog.targets.size();++t) {
        const auto &mesh=document_->loaded.scene.meshes[document_->loaded.scene.instances[document_->catalog.targets[t].instance].mesh];
        if(!mesh.polygons.empty()) {subdivision_target_=t;found=true;break;}
      }
      if(!found) {finish_test(false,"细分验证场景没有多边形模型");return;}
      subdivision_initial_=state;subdivision_original_=subdivision_level(*document_,snapshot_,subdivision_target_);
      choose(int(subdivision_target_));parameters_->query(QStringLiteral("渲染细分等级"));
    }
    QDoubleSpinBox *spin=nullptr;
    for(auto *s:parameters_->findChildren<QDoubleSpinBox *>()) if(s->property("parameterId").toString()=="SubDRenderLevel") spin=s;
    if(!spin) {finish_test(false,"缺少唯一的渲染细分控件");return;}
    if(test_stage_>0) {
      const int level=subdivision_level(*document_,snapshot_,subdivision_target_);
      if(state.sessions!=subdivision_initial_.sessions||state.mesh_hashes!=subdivision_initial_.mesh_hashes||state.collision.evaluations!=subdivision_initial_.collision.evaluations||state.generation!=subdivision_initial_.generation) {finish_test(false,"细分修改重建会话、基础形变或碰撞");return;}
      if(state.adapter.camera_updates!=subdivision_initial_.adapter.camera_updates) {finish_test(false,"细分修改重置了用户相机");return;}
      const auto [it,inserted]=subdivision_triangles_.emplace(level,state.adapter.unique_triangles);
      if(!inserted&&it->second!=state.adapter.unique_triangles) {finish_test(false,"恢复细分等级后的三角形数量不一致");return;}
      PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory));
      subdivision_checks_.push_back({{"stage",test_stage_},{"level",level},{"triangles",state.adapter.unique_triangles},{"private_bytes",memory.PrivateUsage},{"sessions",state.sessions}});
    }
    if(test_stage_<int(std::size(levels))) {spin->setValue(levels[test_stage_++]);return;}
    if(test_stage_==int(std::size(levels))) {
      // 实际 Qt 控件连续输入，验证快照合并以及无变化输入。
      const auto revision=snapshot_.revision;set_subdivision(subdivision_target_,2);
      if(snapshot_.revision!=revision) {finish_test(false,"相同细分值重复提交");return;}
      for(int level:{1,3,0,2}) spin->setValue(level);
      ++test_stage_;return;
    }
    if(test_stage_==int(std::size(levels))+1) {
      const auto &mesh=document_->loaded.scene.meshes[document_->loaded.scene.instances[document_->catalog.targets[subdivision_target_].instance].mesh];
      bool excessive=false;try {runtime::validate_subdivision_budget(mesh,6);} catch(const std::exception &) {excessive=true;}
      if(excessive) {
        const auto revision=snapshot_.revision;spin->setValue(6);
        if(snapshot_.revision!=revision||spin->value()!=2||load_error_.isEmpty()) {finish_test(false,"过高细分未保留原值或给出原因");return;}
        subdivision_checks_.push_back({{"input","over-budget-rejected"},{"retained_level",2}});load_error_.clear();
      }
      spin->setValue(subdivision_original_);++test_stage_;return;
    }
    screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"subdivision-control.png").wstring()));finish_test(true);
  }
  std::vector<runtime::JointPose> effective_roots_;
  uint64_t effective_generation_=0;
  double root_scale(int axis=-1) const {
    if(!document_||effective_generation_!=document_->generation||selected_<0||size_t(selected_)>=effective_roots_.size()) return 1;
    const auto &p=effective_roots_[size_t(selected_)];return axis<0?p.general_scale:axis==0?p.scale.x:axis==1?p.scale.y:p.scale.z;
  }
  void bind_widget_settings(QWidget *panel,const std::string &owner) {
    if(!panel)return;auto *binding=parameter_widgets::context(panel);
    binding->read=[this,binding](const std::string &key,const runtime::ParameterSettings &defaults){
      const auto found=snapshot_.parameter_settings.find(binding->owner);if(found!=snapshot_.parameter_settings.end())if(auto p=found->second.find(key);p!=found->second.end())return p->second;return defaults;
    };
    binding->edit=[this,binding](const std::string &key,const runtime::ParameterSettings &value,const std::function<void()> &apply){
      if(loading_||!document_)return;auto edit=history_edit(QStringLiteral("修改参数范围与精度"));snapshot_.parameter_settings[binding->owner][key]=value;apply();
    };
    binding->bind(owner);
  }
  void select(int index,int joint=-1,int light=-1) {
    if(powerpose_) powerpose_->cancel();
    parameters_->bind_favorites(nullptr);initialize_favorites();
    selected_group_.clear();if(index==-4&&document_){auto *item=active_selection(hierarchy_);if(item){const auto id=item->data(0,Qt::UserRole+3).toString().toStdString();if(group_node(*document_,id))selected_group_=id;}}
    selected_=index;selected_joint_=joint;selected_light_=light;light_power_row_->setVisible(light>=0);
    std::string settings_owner;
    if(document_){if(light>=0)settings_owner=snapshot_.lights.at(size_t(light)).id;else if(index>=0)settings_owner=document_->catalog.targets.at(size_t(index)).id;else if(index==-2)settings_owner="@environment";else if(index==-3)settings_owner="@tonemapper";else if(auto *item=active_selection(hierarchy_))settings_owner=item->data(0,Qt::UserRole+3).toString().toStdString();}
    for(auto *p:std::initializer_list<QWidget *>{parameters_,cloud_panel_,water_panel_,ground_panel_,physics_panel_,extension_panel_,light_power_row_})bind_widget_settings(p,settings_owner);
    if(environment_tools_)environment_tools_->setVisible(index==-2);
    bind_water();bind_cloud();
    parameters_->setVisible(!selected_water()&&!selected_cloud());
    for(auto *control:std::initializer_list<QWidget *>{refresh_parameters_,retry_parameters_,manual_morph_,apply_parameters_})if(control)control->setVisible(!selected_water()&&!selected_cloud());
    if(materials_){auto *item=active_selection(hierarchy_);materials_->bind(document_,&snapshot_,index,index==-4&&item?item->data(0,Qt::UserRole+3).toString().toStdString():std::string{});}
    bind_extension();bind_physics();bind_geograft();
    pose_status_->setVisible(!selected_water()&&!selected_cloud());
    if(chrome) {const int target=ground_target();chrome->bind_ground(target!=-1);if(ground_panel_)ground_panel_->bind(target!=-1,ground_ratio(target),ground_offset(target),ground_body_only(target),target==-4,selected_water()&&target>=0);}
    if(delete_) delete_->setEnabled(!loading_&&document_&&!tree_deletion_selection(hierarchy_).empty());
    if(renderer_) renderer_->select(document_?document_->generation:0,light<0?index:-1,joint,tree_selection(hierarchy_),index>=0&&light<0&&hierarchy_->selectedItems().size()==1,hierarchy_->selectedItems().size()==1?selected_group_:std::string{});
    if(renderer_) refresh_powerpose(renderer_->status());
    if(light>=0&&document_) {
      const auto &l=snapshot_.lights.at(size_t(light));light_base_=l.transform;light_base_.value[3]=light_base_.value[7]=light_base_.value[11]=0;selection_->setText(text(l.id));parameters_->bind(nullptr,nullptr);
      parameters_->bind_favorites(&*snapshot_.control_favorites,"light/"+l.id);
      const float data[]={l.transform.value[3]*100,l.transform.value[11]*100,-l.transform.value[7]*100,0,0,0,100,100,100};
      for(int i=0;i<9;++i) {QSignalBlocker block(transform_[i]);transform_[i]->setValue(data[i]);transform_[i]->setEnabled(i<6);}
      std::vector<ParameterControl> light_controls;for(int i=0;i<6;++i) {ParameterControl c;c.id="light/"+std::to_string(i);c.label=std::string(1,"XYZ"[i%3])+(i<3?" Translate":" Rotate");c.group=i<3?"/General/Transforms/Translation":"/General/Transforms/Rotation";c.read=[this,i]{return transform_[i]->value();};c.write=[this,i](double v){transform_[i]->setValue(v);};c.slider_minimum=-180;c.slider_maximum=180;light_controls.push_back(std::move(c));}parameters_->bind_controls(std::move(light_controls));
      QSignalBlocker block(light_power_);light_power_->setValue(std::max({l.power.x,l.power.y,l.power.z}));return;
    }
    if(document_&&(index==-2||index==-3)) {
      auto *node=index==-2?&snapshot_.options.environment:&snapshot_.options.tonemapper;selection_->setText(text(node->label));
      parameters_->bind_favorites(&*snapshot_.control_favorites,index==-2?"options/environment":"options/tonemapper");
      parameters_->bind_options(node,[this,node](size_t p,size_t c,double v){auto edit=history_edit(QStringLiteral("修改环境与色调：")+text(node->parameters.at(p).label));try {ir::set_option(*node,p,c,v);send();}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);}},
        [this,node](size_t p,const std::array<double,3> &v){try {auto next=*node;for(size_t c=0;c<3;++c)ir::set_option(next,p,c,v[c]);auto edit=history_edit(QStringLiteral("修改环境与色调颜色：")+text(node->parameters.at(p).label));*node=std::move(next);send();}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);}},
        [this,node](const std::array<size_t,3> &indices,const std::array<double,3> &v){try {auto next=*node;for(size_t k=0;k<3;++k)ir::set_option(next,indices[k],0,v[k]);auto edit=history_edit(QStringLiteral("地图设置经纬度与 UTC 时区"));*node=std::move(next);send();}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);}});return;
    }
    if(!selected_group_.empty()){bind_group();return;}
    if(index<0 || !document_) {auto *item=active_selection(hierarchy_);selection_->setText(index<=-4&&item?item->text(0):QStringLiteral("请先选择场景对象"));parameters_->bind(nullptr,nullptr);for(auto *spin:transform_) spin->setEnabled(false);return;}
    const auto &target=document_->catalog.targets[size_t(index)];selection_->setText(text(target.label));
    if(selected_water()||selected_cloud()){parameters_->bind(nullptr,nullptr);return;}
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
      append_node_controls(controls,index);
      ParameterControl general;general.id="transform/general_scale";general.favorite_name="Scale";general.label="Scale（%）";general.group="/General/Transforms/Scale";general.minimum=.01;general.maximum=10000;general.slider_minimum=1;general.slider_maximum=300;const float initial=asset?asset->general_scale:1;general.settings=asset?asset->numeric_settings[9]:std::nullopt;general.float_backed=false;general.read=[this,initial]{return runtime::decimal_derived_float(double(initial)*root_scale())*snapshot_.values[size_t(selected_)].transform.general_scale*100;};general.write=[this,initial](double value){auto edit=history_edit(QStringLiteral("修改整体缩放"));const double base=runtime::decimal_derived_float(double(initial)*root_scale())*100;if(base!=0) {snapshot_.values[size_t(selected_)].transform.general_scale=value/base;send();}};controls.push_back(std::move(general));
      for(int i=0;i<9;++i) {ParameterControl c;c.id="transform/"+std::to_string(i);c.favorite_name=std::string(1,"XYZ"[i%3])+(i<3?"Translate":i<6?"Rotate":"Scale");c.label=std::string(1,"XYZ"[i%3])+std::string(i<3?" Translate（厘米）":i<6?" Rotate（度）":" Scale（%）");c.group=i<3?"/General/Transforms/Translation":i<6?"/General/Transforms/Rotation":"/General/Transforms/Scale";c.float_backed=true;c.minimum=transform_[i]->minimum();c.maximum=transform_[i]->maximum();c.step=.1;c.settings=asset?asset->numeric_settings[i]:std::nullopt;c.slider_minimum=i<3?-200:i<6?-180:1;c.slider_maximum=i<3?200:i<6?180:300;const float base=saved[i];c.read=[this,i,base]{return i<6?transform_[i]->value()+base:transform_[i]->value()*base*root_scale(i-6);};c.write=[this,i,base](double v){const double scale=double(base)*(i>=6?root_scale(i-6):1);if(i<6||scale!=0) transform_[i]->setValue(i<6?v-base:v/scale);};controls.push_back(std::move(c));}
    } else if(const int skin=selected_skin();skin>=0) {
      const auto &skeleton=document_->skeletons.skins[size_t(skin)];
      for(int i=0;i<10;++i) {const auto &channel=skeleton.joints[size_t(joint)].channels[i];if(!channel.present) continue;
        const double factor=channel.percent?100.:1.;ParameterControl c;c.id="joint/"+std::to_string(i);c.favorite_name=i==9?"Scale":std::string(1,"XYZ"[i%3])+(i<3?"Translate":i<6?"Rotate":"Scale");c.label=channel.label+(channel.percent?"（%）":i>=3&&i<6?"（度）":"");c.group=channel.group;
        c.enabled=runtime::editable_channel(skeleton,size_t(joint),i);c.visible=channel.visible;c.float_backed=true;c.minimum=channel.minimum*factor;c.maximum=channel.maximum*factor;
        c.slider_minimum=c.minimum;c.slider_maximum=c.maximum;const double saved_value=runtime::joint_value(snapshot_.poses.at(skin).at(joint),i)*factor;c.minimum=std::min(c.minimum,saved_value);c.maximum=std::max(c.maximum,saved_value);c.step=std::max(.000001,double(channel.step)*factor);c.initial=channel.initial*factor;c.settings=runtime::ParameterSettings{channel.clamped,channel.minimum_authored?runtime::decimal_float(channel.minimum)*factor:0,channel.maximum_authored?runtime::decimal_float(channel.maximum)*factor:1,channel.step_authored&&channel.step>0?runtime::decimal_float(channel.step)*factor:.1};
        c.detail=skeleton.joints[size_t(joint)].id+" / "+channel.label+(channel.locked?"\n资产锁定此通道":skeleton.static_local_weights?"\n此资产需要尚未支持的 TriAx 轴权重":i>=6&&skeleton.separate_scale_weights?"\n此资产需要尚未支持的独立缩放权重":"\n编辑骨骼输入，联动 ERC、JCM 和穿戴物");
        c.read=[this,skin,joint,i,factor]{return runtime::joint_value(snapshot_.poses.at(skin).at(joint),i)*factor;};
        c.write=[this,skin,joint,i,factor](double v) {auto edit=history_edit(QStringLiteral("修改骨骼参数"));try {runtime::set_joint_value(document_->skeletons.skins.at(skin),snapshot_.poses.at(skin),size_t(joint),i,float(v/factor),false);constrain_pins(skin);send();}catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);}};
        controls.push_back(std::move(c));}
      for(bool angle:{false,true}) {ParameterControl pin;pin.id=angle?"pose/pin-angle":"pose/pin";pin.label=angle?"固定角度（IK）":"固定位置（IK）";pin.group="/Pose/IK";pin.choices={"关闭","固定"};pin.enabled=!runtime::ik_chain(skeleton,joint).empty();
        pin.detail=angle?"固定开启时的世界朝向；仍可拖动位置。与固定位置独立，可同时开启。":"固定开启时的世界位置；与固定角度独立。";
        pin.read=[this,skin,joint,angle]{return std::any_of(pose_pins_.begin(),pose_pins_.end(),[&](const auto &p){return p.skin==skin&&p.joint==joint&&(angle?p.angle:p.position);})?1.:0.;};pin.write=[this,skin,joint,angle](double v){pin_joint(skin,joint,v!=0,angle);};controls.push_back(std::move(pin));}
    }
    if(joint<0&&!document_->loaded.scene.meshes.at(document_->loaded.scene.instances.at(target.instance).mesh).polygons.empty()) {
      ParameterControl c;c.id="SubDRenderLevel";c.favorite_name="SubDRenderLevel";c.label="渲染细分等级";c.group="/General/Mesh Resolution";
      c.minimum=0;c.maximum=6;c.step=1;c.enforce_limits=true;c.slider_minimum=0;c.slider_maximum=4;
      c.detail="0 为基础网格。此等级同时用于当前预览与最终渲染；过高等级会按网格预算拒绝。";
      c.read=[this,index] {return double(subdivision_level(*document_,snapshot_,size_t(index)));};
      c.write=[this,index](double value) {set_subdivision(size_t(index),int(value));};controls.push_back(std::move(c));
    }
    parameters_->bind_favorites(&*values.favorites,node);parameters_->set_extra(std::move(controls));parameters_->bind(&target,&values,node);
  }
  void reset_selected() {
    auto edit=history_edit(QStringLiteral("重置选中对象"));
    pose_pins_.clear();renderer_->pose_pins({});
    if(!selected_group_.empty()){set_group_value(selected_group_,{});bind_group();return;}
    if(selected_<0) return;
    auto &value=snapshot_.values[size_t(selected_)];value.transform={};value.unlimited_morphs.clear();
    const auto extension_kind=value.extension.kind;value.extension={};value.extension.kind=extension_kind;value.physics={};value.graft_enabled=true;
    const auto &target=document_->catalog.targets[size_t(selected_)];
    std::erase_if(pending_parameters_,[&](const auto &p){return p.first.first==target.id;});apply_parameters_->setEnabled(!pending_parameters_.empty());
    for(size_t m=0;m<value.morphs.size();++m) value.morphs[m]=target.morphs[m].evaluable||target.morphs[m].unsupported.empty()?target.morphs[m].initial:0;
    runtime::sync_aliases(target,value);
    const auto skin=selected_skin();if(skin>=0) {snapshot_.poses[size_t(skin)]=document_->skeletons.skins[size_t(skin)].initial;frame_pending_=true;pose_status_->setText(QStringLiteral("已重置选中角色的姿势与形态。"));pose_report_=nullptr;}
    select(selected_);send();
  }
  void finish_test(bool pass,const std::string &error={}) {
    if(workflow_test_||joint_selection_test_) SetCursorPos(workflow_cursor_.x,workflow_cursor_.y);
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
    if(interaction_test_) {report["scope"]="interaction-latency";report["checks"]=interaction_checks_;report["sessions"]=status.sessions;
      report["local_geometry_restored"]=status.mesh_hashes==interaction_initial_.mesh_hashes;report["instance_transforms_restored"]=status.instance_transforms==interaction_initial_.instance_transforms;report["boundaries"]=interaction_boundaries_;}
    if(!rebuild_test_file_.empty()) {report["scope"]="scene-rebuild-latency";report["checks"]=rebuild_checks_;report["sessions"]=status.sessions;report["initial_subdivision_level"]=rebuild_level_;report["initial_render_subdivision_level"]=rebuild_render_level_;}
    if(subdivision_stress_test_) {report["scope"]="subdivision-repeat-coalesce-budget-and-restore";report["checks"]=subdivision_checks_;report["sessions"]=status.sessions;}
    if(pose_edit_test_) {report["scope"]="native-FK-left-button-IK-selection-proxy-full-quality-cancel";report["checks"]=pose_checks_;}
    if(edit_resume_test_) {report["scope"]="powerpose-morph-reedit-before-beauty";report["checks"]=er_checks_;}
    if(powerpose_test_&&!edit_resume_test_) {report["scope"]="powerpose-qt-proxy-commit-cancel-native-limits-pins";report["checks"]=pp_checks_;}
    if(gizmo_test_) {report["scope"]="gizmo-local-world-proxy-commit-cancel-restore";report["checks"]=gz_checks_;}
    if(group_test_) {report["scope"]="group-transform-properties-gizmo-save-undo";report["checks"]=gt_checks_;}
    if(extension_test_){report["scope"]=extension_module_target_.empty()?"native-growth-weight-and-dufex":"weight-module-assignment";report["checks"]=extension_checks_;}
    if(dufex_test_)report["scope"]="dufex-v2-save-thumbnail-camera-reopen-native-independence";
    if(empty_test_)report["scope"]="empty-startup-refresh";
    if(city_test_){report["scope"]="city-generation-lod-materials-history-reopen";report["checks"]=city_checks_;report["city_stage"]=city_test_stage_;}
    if(water_test_){report["scope"]=water_scene_test_?"water-full-scene-create-lod-time-undo":"water-menu-coast-time-history-save";report["checks"]=water_checks_;report["water_stage"]=water_test_stage_;}
    if(cloud_test_){report["scope"]="cloud-menu-static-time-history-save";report["checks"]=cloud_checks_;report["cloud_stage"]=cloud_test_stage_;}
    if(capture_test_&&document_) {report["scope"]="scene-render";report["instances"]=document_->catalog.targets.size();report["skins"]=document_->skeletons.skins.size();}
    if(!selection_test_labels_.empty()) {report["scope"]=focus_only_test_?"large-scene-key-and-side-button-focus":"instance-and-graft-ray-tree-selection";report["checks"]=selection_checks_;}
    if(edit_regression_test_) {report["scope"]="multi-selection-focus-subdivision-ERC-scale";report["checks"]=regression_checks_;}
    if(joint_selection_test_) {report["scope"]="same-figure-joint-ctrl-selection-and-focus";report["checks"]=regression_checks_;}
    if(!visibility_label_.isEmpty()) {report["scope"]="hierarchy-visibility-toggle-restore";report["visible"]=status.visible;report["checks"]=visibility_checks_;}
    if(lifecycle_test_) {report["scope"]="append-delete-clear-replace-resource-lifetime-and-render-error-recovery";report["samples"]=lifecycle_samples_;report["retired_document_expired"]=retired_document_.expired();}
    if(!wear_test_file_.empty()&&document_) {report["scope"]="wearable-browser-import-detach-rebind";report["added_targets"]=document_->catalog.targets.size()-wear_test_first_;report["attachment_groups"]=document_->attachments.size();if(!wear_test_material_.empty())report["material_link_applied"]=test_stage_==5&&document_->operations.size()>wear_test_material_operations_;}
    report["hierarchy"]=hierarchy_report();report["options"]=ir::options_json(snapshot_.options);
    report["graft_seams"]=nlohmann::json::array();for(const auto &g:status.graft_seams) report["graft_seams"].push_back({{"follower",document_->loaded.scene.instances.at(g.follower).id},{"source",document_->loaded.scene.instances.at(g.source).id},{"pairs",g.pairs},{"max_gap_m",g.max_gap_m}});
    if(!scene_reopen_file_.empty()) {report["scope"]="open-new-empty-open-from-content-browser";report["checks"]=scene_reopen_checks_;}
    if(!reload_file_.empty()) report["scope"]="background-scene-replacement-generation-isolation";
    if(pose_test_) report["scope"]="pose-apply-reset-camera-no-skin-evaluation";
    if(formula_test_) {report["scope"]="parameter-slider-regression-with-ERC-JCM";report["parameter_checks"]=parameter_checks_;}
    report["project_file"]=project_.file.toUtf8().toStdString();report["content_roots"]=nlohmann::json::array();
    for(const auto &root:project_.content_roots) report["content_roots"].push_back(root.toUtf8().toStdString());
    report["named_parameters"]=nlohmann::json::array();
    if(document_) for(const auto &target:document_->catalog.targets) for(const auto &m:target.morphs)
      if(m.label=="Arms Length" || m.label=="Chest Scale" || m.label=="Eyes Closed" || m.label=="HS Sanny Shy")
        report["named_parameters"].push_back({{"label",m.label},{"group",m.group},{"kind",m.kind},{"source",m.source}});
    const auto work_area=screen()->availableGeometry();
    report["available_work_area"]={work_area.x(),work_area.y(),work_area.width(),work_area.height()};
    report["device_pixel_ratio"]=devicePixelRatioF();
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
  void selection_tick(const RenderStatus &state) {
    std::ofstream(output_/"selection-progress.json")<<nlohmann::json({{"stage",test_stage_},{"target",selection_test_key_},{"instance",selection_test_instance_},{"selected",state.selected_target},{"hovered",state.hovered},{"probe",selection_probe_},{"camera_epoch",state.camera.epoch},{"samples",state.samples}}).dump();
    if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000) {finish_test(false,"选取 / 聚焦验证超时");return;}
    if(!document_||loading_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<4) return;
    const auto &scene=document_->loaded.scene;
    const auto hwnd=FindWindowExW(reinterpret_cast<HWND>(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
    auto save=[&](const std::string &name) {screen()->grabWindow(winId()).save(QString::fromStdWString((output_/(name+".png")).wstring()));};
    if(test_stage_==0) {
      const auto label=selection_test_labels_.at(qsizetype(selection_test_case_));selection_test_key_=-1;
      for(size_t t=0;t<document_->catalog.targets.size();++t) if(text(document_->catalog.targets[t].label)==label) {selection_test_key_=int(t);selection_test_instance_=int(document_->catalog.targets[t].instance);break;}
      if(selection_test_key_==-1) for(size_t i=0;i<scene.instances.size();++i) if(scene.instances[i].prototype>=0&&text(scene.instances[i].id).contains(label)) {const runtime::InstanceGroups groups(scene);selection_test_instance_=int(groups.roots[i]);selection_test_key_=runtime::instance_selection(uint32_t(selection_test_instance_));break;}
      if(selection_test_key_==-1&&!focus_only_test_)for(const auto &node:document_->loaded.nodes)if(node.group&&(text(node.label)==label||text(node.id)==label)){
        for(QTreeWidgetItemIterator it(hierarchy_);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()==-4&&(*it)->data(0,Qt::UserRole+3).toString()==text(node.id)){
          const auto begin=std::chrono::steady_clock::now();hierarchy_->setCurrentItem(*it,0,QItemSelectionModel::ClearAndSelect);
          const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
          if(selected_group_!=node.id){finish_test(false,"组选择没有更新参数面板");return;}
          selection_checks_.push_back({{"group",node.id},{"ui_selection_ms",elapsed},{"material_surfaces",materials_->selected_surfaces().size()}});selection_test_key_=-4;test_stage_=90;return;
        }
      }
      if(selection_test_key_==-1) {finish_test(false,"找不到选取验证对象："+label.toStdString());return;}
      choose(selection_test_key_);if(selected_!=selection_test_key_) {finish_test(false,"场景树缺少实例 / 插件选择项");return;}test_stage_=1;return;
    }
    if(test_stage_==90){if(state.selected_target!=-4||state.selection_bounds.empty)return;save("selected-group");if(++selection_test_case_<size_t(selection_test_labels_.size())){test_stage_=0;return;}finish_test(true);return;}
    if(test_stage_==1) {
      if(state.selected_target!=selection_test_key_||state.selection_bounds.empty) return;
      selection_test_epoch_=state.camera.epoch;options_geometry_=state.adapter.geometry_updates;
      if(focus_only_test_) {SendMessageW(hwnd,WM_KEYDOWN,'F',0);SendMessageW(hwnd,WM_KEYUP,'F',0);test_stage_=5;return;}
      const auto center=state.selection_bounds.center();const auto &m=scene.instances.at(size_t(selection_test_instance_)).transform.value;
      renderer_->camera_view({center.x,center.y,center.z,std::max(.01f,state.selection_bounds.extent()*2),std::atan2(-m[1],m[5]),.15f});choose(-1);selection_probe_=0;test_stage_=2;return;
    }
    if(test_stage_==2) {
      if(state.camera.epoch<=selection_test_epoch_||state.selected_target!=-1) return;
      if(selection_probe_&&state.pointer_x==selection_probe_point_.x()&&state.pointer_y==selection_probe_point_.y()&&state.hovered==selection_test_instance_) {
        selection_test_clicks_=state.clicks;renderer_->pointer(state.pointer_x,state.pointer_y,true);test_stage_=3;return;
      }
      if(selection_probe_&&!(state.pointer_x==selection_probe_point_.x()&&state.pointer_y==selection_probe_point_.y())) {renderer_->pointer(selection_probe_point_.x(),selection_probe_point_.y());return;}
      if(selection_probe_>225) {finish_test(false,"聚焦后射线未命中验证对象："+scene.instances.at(size_t(selection_test_instance_)).id);return;}
      const int n=selection_probe_++;selection_probe_point_={n?state.width*(2+(n-1)%15)/18:state.width/2,n?state.height*(2+(n-1)/15)/18:state.height/2};renderer_->pointer(selection_probe_point_.x(),selection_probe_point_.y());return;
    }
    if(test_stage_==3) {
      if(state.clicks<=selection_test_clicks_||state.selected_target!=selection_test_key_) return;
      if(selected_!=selection_test_key_||!hierarchy_->currentItem()||hierarchy_->currentItem()->data(0,Qt::UserRole).toInt()!=selection_test_key_||state.selection_bounds.empty) {finish_test(false,"点击后的树选择和包围盒不一致");return;}
      selection_checks_.push_back({{"id",scene.instances.at(size_t(selection_test_instance_)).id},{"target",selection_test_key_},{"ray_hit",state.hovered},{"tree_selected",true},{"highlight_triangles",state.hovered_triangles}});save("selected-"+std::to_string(selection_test_case_));
      if(state.adapter.geometry_updates!=options_geometry_) {finish_test(false,"选择对象触发了网格重建");return;}
      if(++selection_test_case_<size_t(selection_test_labels_.size())) {test_stage_=0;return;}finish_test(true);return;
    }
    if(test_stage_==5||test_stage_==6) {
      if(state.camera.epoch<=selection_test_epoch_) return;
      if(state.camera.distance<100000||state.adapter.geometry_updates!=options_geometry_) {finish_test(false,"大型对象聚焦距离错误或触发几何更新");return;}
      const auto pixmap=screen()->grabWindow(winId());const auto origin=host_->mapTo(this,QPoint{});const auto pixels=pixmap.copy(QRect(origin,QSize(state.width,state.height))).toImage();
      int colored=0;for(int y=0;y<pixels.height();y+=4) for(int x=0;x<pixels.width();x+=4) {const auto c=pixels.pixelColor(x,y);colored+=std::max({c.red(),c.green(),c.blue()})>30;}
      if(colored<100) {finish_test(false,"超远距离聚焦得到黑图");return;}
      selection_checks_.push_back({{"input",test_stage_==5?"F":"XBUTTON1"},{"distance_m",state.camera.distance},{"nonblack_probes",colored},{"geometry_updates",state.adapter.geometry_updates}});save(test_stage_==5?"focus-F":"focus-side");
      if(test_stage_==6) {finish_test(true);return;}
      renderer_->orbit(10,0);selection_test_epoch_=state.camera.epoch;test_stage_=7;return;
    }
    if(test_stage_==7&&state.camera.epoch>selection_test_epoch_) {
      selection_test_epoch_=state.camera.epoch;const auto point=MAKELPARAM(200,200);
      SendMessageW(hwnd,WM_XBUTTONDOWN,MAKEWPARAM(0,XBUTTON1),point);SendMessageW(hwnd,WM_XBUTTONUP,MAKEWPARAM(0,XBUTTON1),point);test_stage_=6;
    }
  }
  void head_selection_tick(const RenderStatus &state) {
    auto &skin=document_->skeletons.skins.at(size_t(std::max(0,head_test_skin_)));
    std::ofstream(output_/"selection-progress.json")<<nlohmann::json({{"stage",test_stage_},{"camera_epoch",state.camera.epoch},{"frame_epoch",head_test_epoch_},{"selected_target",state.selected_target},{"selected_joint",state.selected_joint},{"requested_pointer",{probe_point_.x(),probe_point_.y()}},{"actual_pointer",{state.pointer_x,state.pointer_y}},{"detail",state.hovered_detail_joint},{"probe",probe_index_}}).dump();
    auto position=[&](int joint) {return head_test_positions_.at(joint);};
    auto project=[&](ir::Vec3 p) {const auto m=state.camera.matrix();p={p.x-m[3],p.y-m[7],p.z-m[11]};const float z=m[2]*p.x+m[6]*p.y+m[10]*p.z,e=std::tan(.4f);return QPoint(qRound((1+(m[0]*p.x+m[4]*p.y+m[8]*p.z)/z/e/std::max(1.f,float(state.width)/state.height))*state.width*.5f-.5f),qRound((1-(m[1]*p.x+m[5]*p.y+m[9]*p.z)/z/e/std::max(1.f,float(state.height)/state.width))*state.height*.5f-.5f));};
    auto move_probe=[&] {const int n=probe_index_++;const int x=n==0?0:((n-1)%21-10)*4,y=n==0?0:((n-1)/21-10)*4;probe_point_=probe_center_+QPoint(x,y);probe_point_.setX(std::clamp(probe_point_.x(),1,state.width-2));probe_point_.setY(std::clamp(probe_point_.y(),1,state.height-2));renderer_->pointer(probe_point_.x(),probe_point_.y());};
    auto body_probe=[&] {const int n=probe_index_++;probe_point_={state.width/2+(n?((n-1)%21-10)*state.width/24:0),state.height/2+(n?((n-1)/21-10)*state.height/24:0)};renderer_->pointer(probe_point_.x(),probe_point_.y());};
    auto at_probe=[&] {return state.pointer_x==probe_point_.x()&&state.pointer_y==probe_point_.y();};
    auto click=[&] {workflow_click_=state.clicks;renderer_->pointer(probe_point_.x(),probe_point_.y(),true);};
    auto record=[&](const char *name) {hover_checks_.push_back({{"mode",name},{"selected_target",selected_},{"selected_joint",selected_joint_},{"hovered_joint",state.hovered_joint},{"highlight_triangles",state.hovered_triangles}});};
    auto lip=[&](int joint) {if(joint<0||size_t(joint)>=skin.joints.size()) return false;const auto &name=skin.joints[size_t(joint)].id;return name.find("LipUpper")!=std::string::npos||name.find("LipLower")!=std::string::npos;};
    auto restore_accessories=[&] {if(head_test_occluders_.empty()) {finish_test(true);return;}for(auto t:head_test_occluders_) snapshot_.values[t].visible=true;send();test_stage_=104;};
    if(test_stage_==104) {
      for(auto t:head_test_occluders_) if(!state.visible.at(document_->catalog.targets[t].instance)) {finish_test(false,"测试附件可见性未恢复");return;}
      record("accessory_visibility_restored");finish_test(true);return;
    }
    if(test_stage_>=100&&test_stage_<=102) {
      const int joint=test_stage_==100?head_test_joint_:test_stage_==101?eye_test_joint_:lip_test_joint_;
      if(state.selected_target!=workflow_target_||state.selected_joint!=joint) return;
      if(state.selection_bounds.empty) {finish_test(false,"测试部位没有求值后的几何区域");return;}
      head_test_positions_[joint]=state.selection_bounds.center();
      if(test_stage_==102) test_stage_=0;
      else {++test_stage_;choose(workflow_target_,test_stage_==101?eye_test_joint_:lip_test_joint_);return;}
    }
    if(test_stage_==0) {
      head_test_skin_=0;workflow_target_=0;for(size_t t=0;t<document_->catalog.targets.size();++t) if(document_->catalog.targets[t].instance==skin.instance) workflow_target_=int(t);
      for(size_t j=0;j<skin.joints.size();++j) {const auto &id=skin.joints[j].id;if(id=="head") head_test_joint_=int(j);if(id=="lEye") eye_test_joint_=int(j);if(id=="LipUpperMiddle") lip_test_joint_=int(j);}
      if(head_test_joint_<0||eye_test_joint_<0||lip_test_joint_<0) {finish_test(false,"头部分级测试缺少头 / 眼 / 唇节点");return;}
      // 保存的场景可含坐姿和体型；从渲染器求值后的区域定位，不能用未变形的绑定网格。
      if(head_test_positions_.empty()) {choose(workflow_target_,head_test_joint_);test_stage_=100;return;}
      choose(-1);auto c=position(head_test_joint_);ir::Bounds face;face.add({c.x-.14f,c.y-.14f,c.z-.19f});face.add({c.x+.14f,c.y+.14f,c.z+.19f});head_test_epoch_=state.camera.epoch;renderer_->frame(face);
      const auto mouth=position(lip_test_joint_);
      renderer_->orbit((.3f-std::atan2(mouth.x-c.x,c.y-mouth.y))/.005f,0); // 由求值后的面部确定朝向，包含 Hip / Head 的保存姿势。
      test_evaluations_=state.evaluation.morph_evaluations;test_skin_evaluations_=state.skinning.evaluations;test_stage_=1;
    } else if(test_stage_==1) {
      if(state.camera.epoch==head_test_epoch_||state.selected_target!=-1) return;probe_center_=project(position(eye_test_joint_));probe_index_=0;move_probe();test_stage_=2;
    } else if(test_stage_==2) {
      // 独立镜片仍应遮挡射线；记录这一行为，临时隐藏实际挡住眼睛的挂接附件后继续部位检查。
      if(at_probe()&&state.hovered>=0&&state.hovered!=int(skin.instance)) for(size_t t=0;t<document_->catalog.targets.size();++t) {
        const auto &target=document_->catalog.targets[t];if(int(target.instance)!=state.hovered||!snapshot_.values[t].visible) continue;
        auto ancestors=target.ancestors;if(ancestors.empty()) ancestors.push_back(target.parent);bool attached=false;
        for(const auto &a:ancestors) for(const auto &joint:skin.joints) attached|=!joint.scene_id.empty()&&a=="#"+joint.scene_id;
        if(attached) {record("accessory_occludes_eye");head_test_occluders_.push_back(t);snapshot_.values[t].visible=false;send();probe_index_=0;move_probe();return;}
      }
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
      if(garment<0) {restore_accessories();return;}choose(garment);if(selected_!=garment) {finish_test(false,"绑定服装无法通过场景树选择");return;}
      record("bound_clothing_tree_selection");head_test_epoch_=state.camera.epoch;renderer_->frame(state.bounds.at(size_t(workflow_target_)));test_stage_=8;
    } else if(test_stage_==8) {
      if(state.camera.epoch==head_test_epoch_) return;probe_index_=0;body_probe();test_stage_=9;
    } else if(test_stage_==9) {
      if(!at_probe()) return;if(state.hovered!=int(skin.instance)) {if(probe_index_>441) {finish_test(false,"穿戴场景没有命中角色");return;}body_probe();return;}
      record("ray_through_bound_clothing");click();test_stage_=10;
    } else if(test_stage_==10) {
      if(state.clicks<=workflow_click_) return;if(selected_!=workflow_target_||selected_joint_!=-1) {finish_test(false,"点击穿戴区域没有选择角色");return;}
      if(state.evaluation.morph_evaluations!=test_evaluations_||state.skinning.evaluations!=test_skin_evaluations_) {finish_test(false,"分级选择触发了变形");return;}record("body_selected_through_clothing");restore_accessories();
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
    // 新增撤销会有意保留旧文档；此诊断检查清空历史之后的资源释放。
    if(history_)history_->clear();
    if(!retired_document_.expired()) {finish_test(false,"释放历史后旧文档仍被持有");return;}
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
  void interaction_boundaries(const RenderStatus &state,bool full) {
    if(interaction_boundary_at_&&now()-interaction_boundary_at_>20) {finish_test(false,"停靠或持续编辑边界验证超时");return;}
    auto *panel=findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"));
    if(!panel) {finish_test(false,"找不到属性面板");return;}
    if(interaction_boundary_==0) {
      if(!full) return;interaction_layout_=saveState(1);interaction_boundary_before_=state;
      panel->setFloating(true);panel->move(screen()->availableGeometry().topLeft()+QPoint(40,40));interaction_boundary_at_=now();++interaction_boundary_;
    } else if(interaction_boundary_==1) {
      if(!full||now()-interaction_boundary_at_<.5) return;
      if(state.sessions!=interaction_boundary_before_.sessions) {finish_test(false,"浮动面板重建了会话");return;}
      interaction_boundaries_.push_back({{"input","dock_float"},{"sessions_added",0}});interaction_boundary_before_=state;
      panel->move(panel->pos()+QPoint(60,30));interaction_boundary_at_=now();++interaction_boundary_;
    } else if(interaction_boundary_==2) {
      if(now()-interaction_boundary_at_<.5) return;
      if(!full||state.width!=interaction_boundary_before_.width||state.height!=interaction_boundary_before_.height||state.requested_epoch!=interaction_boundary_before_.requested_epoch) {finish_test(false,"只移动浮动面板触发了渲染重置");return;}
      interaction_boundaries_.push_back({{"input","floating_panel_move"},{"render_resets",0}});
      panel->setFloating(false);restoreState(interaction_layout_,1);interaction_boundary_at_=now();++interaction_boundary_;
    } else if(interaction_boundary_==3) {
      if(!full||now()-interaction_boundary_at_<.5) return;
      interaction_boundaries_.push_back({{"input","dock_restore"},{"sessions_added",state.sessions-interaction_boundary_before_.sessions}});
      renderer_->interaction(true);transform_[0]->setValue(1.25);interaction_boundary_at_=now();++interaction_boundary_;
    } else if(interaction_boundary_==4) {
      if(now()-interaction_boundary_at_<.7) return;
      if(state.presented_revision!=snapshot_.revision||!state.preview||state.render_width!=std::max(1,state.width/4)||state.render_height!=std::max(1,state.height/4)) {finish_test(false,"持续编辑静止持有期间没有保持预览");return;}
      interaction_boundaries_.push_back({{"input","edit_hold"},{"preview",true}});renderer_->interaction(false);interaction_boundary_at_=now();++interaction_boundary_;
    } else if(interaction_boundary_==5) {
      if(!full) return;interaction_boundaries_.push_back({{"input","edit_release"},{"full_resolution",true}});
      transform_[0]->setValue(0);interaction_boundary_at_=now();++interaction_boundary_;
    } else if(interaction_boundary_==6) {
      if(!full) return;interaction_boundary_before_=state;interaction_options_=snapshot_.options;
      auto &node=snapshot_.options.tonemapper;
      for(size_t p=0;p<node.parameters.size();++p) if(node.parameters[p].id=="Exposure Value") {
        renderer_->interaction(true);ir::set_option(node,p,0,node.parameters[p].value[0]+1);send();
        interaction_boundary_at_=now();++interaction_boundary_;return;
      }
      interaction_boundary_=8;
    } else if(interaction_boundary_==7) {
      if(!full||now()-interaction_boundary_at_<.3) return;
      if(state.requested_epoch!=interaction_boundary_before_.requested_epoch) {finish_test(false,"仅显示色调编辑重新启动了采样");return;}
      interaction_boundaries_.push_back({{"input","tonemapper_drag"},{"render_resets",0}});renderer_->interaction(false);
      snapshot_.options=interaction_options_;send();interaction_boundary_at_=now();++interaction_boundary_;
    } else {
      if(!full) return;screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"interaction-final.png").wstring()));
      if(state.requested_epoch!=interaction_boundary_before_.requested_epoch) {finish_test(false,"恢复色调重新启动了采样");return;}
      const bool restored=state.mesh_hashes==interaction_initial_.mesh_hashes&&state.instance_transforms==interaction_initial_.instance_transforms;
      finish_test(restored,restored?"":"局部几何或实例变换未恢复");
    }
  }
  void interaction_tick(const RenderStatus &state) {
    static const char *names[]={"resize_minus_6","dock_splitter_60","camera_orbit","camera_pan","camera_zoom","figure_translate","figure_rotate","figure_restore","resize_grow_capacity","resize_shrink_capacity"};
    if(now()-interaction_idle_>180&&interaction_begin_>0) {finish_test(false,"交互计时超时");return;}
    if(!state.error.empty()||!state.edit_error.empty()) {finish_test(false,state.error+state.edit_error);return;}
    if(!document_||state.generation!=document_->generation) return;
    const bool full=state.frames>0&&!state.preview&&state.presented_epoch==state.requested_epoch&&state.presented_revision==snapshot_.revision&&state.render_width==state.width&&state.render_height==state.height&&!resize_at_;
    if(interaction_case_==std::size(names)) {interaction_boundaries(state,full);return;}
    const auto hwnd=FindWindowExW(reinterpret_cast<HWND>(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
    auto input=[&](UINT message,WPARAM w=0,LPARAM l=0) {SendMessageW(hwnd,message,w,l);};
    auto record=[&] {std::ofstream(output_/"interaction-progress.json")<<nlohmann::json({{"case",interaction_case_},{"step",interaction_step_},{"begin",interaction_begin_},{"stop",interaction_stop_},{"checks",interaction_checks_}}).dump(2);};
    if(interaction_begin_==0) {
      if(!full) {interaction_idle_=now();return;}
      if(now()-interaction_idle_<.6) return;
      if(interaction_target_<0) {
        interaction_initial_=state;
        for(const auto &skin:document_->skeletons.skins) for(size_t t=0;t<document_->catalog.targets.size();++t) {
          const auto &target=document_->catalog.targets[t];if(target.instance==skin.instance&&target.conform_target.empty()&&interaction_target_<0) interaction_target_=int(t);
        }
        if(interaction_target_<0&&!snapshot_.values.empty()) interaction_target_=0;
        if(interaction_target_<0) {finish_test(false,"没有可移动的对象");return;}
        choose(interaction_target_);
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"interaction-initial.png").wstring()));
      }
      // 短操作只发送到副屏原生视口或当前对象的编辑入口。
      activateWindow();SetForegroundWindow(reinterpret_cast<HWND>(winId()));SetFocus(hwnd);
      POINT cursor{220,210};ClientToScreen(hwnd,&cursor);SetCursorPos(cursor.x,cursor.y);
      MSG pending{};while(PeekMessageW(&pending,hwnd,WM_MOUSEMOVE,WM_MOUSEMOVE,PM_REMOVE)) DispatchMessageW(&pending);
      interaction_before_=state;interaction_begin_=now();interaction_stop_=interaction_first_=0;interaction_step_=0;
      renderer_->trace(names[interaction_case_]);
      if(interaction_case_==0) {resize(width()-6,height());interaction_stop_=now();}
      else if(interaction_case_==1) {
        auto *d=findChild<QDockWidget *>("Viewport");if(!d) {finish_test(false,"找不到视口停靠面板");return;}resizeDocks({d},{d->width()+60},Qt::Horizontal);interaction_stop_=now();
      }
      else if(interaction_case_==2||interaction_case_==3) input(interaction_case_==2?WM_RBUTTONDOWN:WM_MBUTTONDOWN,interaction_case_==2?MK_RBUTTON:MK_MBUTTON,MAKELPARAM(200,200));
      else if(interaction_case_==7) {transform_[0]->setValue(0);transform_[4]->setValue(0);interaction_stop_=now();}
      else if(interaction_case_==8||interaction_case_==9) {
        auto *d=findChild<QDockWidget *>("Viewport");if(!d) {finish_test(false,"找不到视口停靠面板");return;}
        // 主窗口变宽可能只分配给场景树；直接调整停靠分隔，确保测试输出缓冲扩容。
        resizeDocks({d},{d->width()+(interaction_case_==8?320:-320)},Qt::Horizontal);interaction_stop_=now();
      }
      record();return;
    }
    if(!interaction_first_&&state.presented_epoch>interaction_before_.requested_epoch) interaction_first_=state.present_time;
    if(!interaction_stop_) {
      // 连续两秒相机输入，避免八次离散输入把大场景导航测量限制在 16 Hz。
      const bool camera_sweep=interaction_case_==2;
      const int steps=camera_sweep?120:8;const double period=camera_sweep?1.0/60:.06;
      if(interaction_step_<steps&&now()-interaction_begin_>=interaction_step_*period) {
        ++interaction_step_;
        if(interaction_case_==2||interaction_case_==3) {const int offset=camera_sweep?int(16*std::sin(interaction_step_*3.14159265/60)):interaction_step_*2;input(WM_MOUSEMOVE,interaction_case_==2?MK_RBUTTON:MK_MBUTTON,MAKELPARAM(200+offset,200+offset/2));}
        else if(interaction_case_==4) {POINT p{200,200};ClientToScreen(hwnd,&p);input(WM_MOUSEWHEEL,MAKEWPARAM(0,15),MAKELPARAM(p.x,p.y));}
        else if(interaction_case_==5) transform_[0]->setValue(interaction_step_*1.25);
        else if(interaction_case_==6) transform_[4]->setValue(interaction_step_*1.25);
      }
      if(interaction_step_==steps&&now()-interaction_begin_>=steps*period) {
        if(interaction_case_==2||interaction_case_==3) input(interaction_case_==2?WM_RBUTTONUP:WM_MBUTTONUP,0,MAKELPARAM(216,208));
        interaction_stop_=now();renderer_->trace("interaction_stop");record();
      }
      return;
    }
    if(!full||state.presented_epoch<=interaction_before_.requested_epoch||state.present_time<interaction_stop_||state.camera.epoch!=renderer_->input_camera().epoch) return;
    interaction_checks_.push_back({{"input",names[interaction_case_]},{"begin",interaction_begin_},{"stop",interaction_stop_},{"first_present",interaction_first_},{"full_present",state.present_time},
      {"first_ms",(interaction_first_-interaction_begin_)*1000},{"restore_ms",(state.present_time-interaction_stop_)*1000},
      {"before_epoch",interaction_before_.requested_epoch},{"final_epoch",state.presented_epoch},{"final_revision",snapshot_.revision},{"size",{state.width,state.height}},
      {"sessions_added",state.sessions-interaction_before_.sessions},{"geometry_updates",state.adapter.geometry_updates-interaction_before_.adapter.geometry_updates},
      {"instance_updates",state.adapter.instance_updates-interaction_before_.adapter.instance_updates},{"collision_evaluations",state.collision.evaluations-interaction_before_.collision.evaluations},
      {"morph_evaluations",state.evaluation.morph_evaluations-interaction_before_.evaluation.morph_evaluations},{"skin_evaluations",state.skinning.evaluations-interaction_before_.skinning.evaluations}});
    ++interaction_case_;interaction_begin_=0;interaction_idle_=now();record();
  }
  void tick() {
    if(saving_extension_)return;
    city_tick();
    water_tick();
    cloud_tick();
    update_history_actions();
    if(recovery_)recovery_error_=recovery_->error();
    update_measurement();
    synchronize_pins();
    const auto focus_state=renderer_->status();if(focus_state.focus_requests!=focus_requests_||focus_pending_) {focus_requests_=focus_state.focus_requests;focus_selection();}
    const QSize size(qRound(host_->width()*host_->devicePixelRatioF()),qRound(host_->height()*host_->devicePixelRatioF()));
    if(size!=viewport_size_) {viewport_size_=size;renderer_->resize(size.width(),size.height());resize_at_=0;}
    if(resize_at_&&QDateTime::currentMSecsSinceEpoch()>=resize_at_) {resize_at_=0;renderer_->resize(size.width(),size.height());}
    const auto state=renderer_->status();
    accept_pose_commit(state);
    if(state.ground_requests!=ground_requests_) {ground_requests_=state.ground_requests;request_ground();}
    apply_ground(state);
    refresh_powerpose(state);
    if(history_test_){history_test_tick(state);return;}
    if(city_test_){city_test_tick(state);return;}
    if(water_test_){water_test_tick(state);return;}
    if(cloud_test_){cloud_test_tick(state);return;}
    if(!render_plan_.empty()) {render_profile_tick(state);return;}
    if(gizmo_test_) {gizmo_tick(state);return;}
    if(group_test_) {group_test_tick(state);return;}
    if(node_properties_test_) {node_properties_tick(state);return;}
    if(ground_test_) {ground_test_tick(state);return;}
    if(!cg_label_.empty()) {collective_ground_tick(state);return;}
    if(feedback_test_) {feedback_tick(state);return;}
    if(edit_resume_test_) {edit_resume_tick(state);return;}
    if(powerpose_test_) {powerpose_tick(state);return;}
    if(pose_edit_test_) {pose_edit_tick(state);return;}
    if(!rebuild_test_file_.empty()) {rebuild_tick(state);return;}
    if(subdivision_stress_test_) {subdivision_stress_tick(state);return;}
    if(interaction_test_) {interaction_tick(state);return;}
    if(navigation_test_) {navigation_tick(state);return;}
    if(refresh_parameters_) refresh_parameters_->setEnabled(!loading_&&document_&&selected_>=0&&!selected_water()&&!selected_cloud());
    if(materials_)materials_->setEnabled(!loading_);
    if(retry_parameters_) retry_parameters_->setEnabled(document_&&!state.resource_error.empty());
    static int resources_tick=0;if(++resources_tick%5==0) parameters_->resource_states();
    if(delete_) delete_->setEnabled(!loading_&&document_&&!tree_deletion_selection(hierarchy_).empty());
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
    if(!scene_reopen_file_.empty()) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000) {finish_test(false,"新建后重新打开场景验证超时");return;}
      if(!state.error.empty()||!state.edit_error.empty()||!state.resource_error.empty()) {finish_test(false,state.error+state.edit_error+state.resource_error);return;}
      if(loading_||!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<8) return;
      if(test_stage_==0) {
        scene_reopen_checks_.push_back({{"stage","first_scene"},{"objects",document_->loaded.scene.instances.size()},{"options",ir::options_json(snapshot_.options)}});clear_scene();++test_stage_;return;
      }
      if(test_stage_==1) {
        if(!document_->loaded.scene.instances.empty()||!snapshot_.options.environment.id.empty()||!snapshot_.options.tonemapper.id.empty()) {finish_test(false,"新建空场景残留旧数据");return;}
        scene_reopen_checks_.push_back({{"stage","new_empty"},{"objects",0}});open_asset(scene_reopen_file_);++test_stage_;return;
      }
      if(ir::options_json(snapshot_.options)!=ir::options_json(document_->loaded.scene.options)||snapshot_.options.environment.id.empty()||snapshot_.options.tonemapper.id.empty()) {finish_test(false,"新建后打开没有采用源场景的环境与色调设置");return;}
      std::map<std::string,QTreeWidgetItem *> by_id;for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) {const auto id=(*it)->data(0,Qt::UserRole+3).toString().toStdString();if(!id.empty()) by_id[id]=*it;}
      for(const auto &node:document_->loaded.nodes) {
        if(node.group&&!by_id.contains(node.id)) {finish_test(false,"场景树缺少 Group："+node.id);return;}
        if(!by_id.contains(node.id)) continue;
        auto parent=node.parent;std::set<std::string> visited{node.id};QTreeWidgetItem *expected=nullptr;
        while(parent.starts_with('#')&&visited.insert(parent.substr(1)).second) {
          const auto id=parent.substr(1);if(by_id.contains(id)&&by_id.at(id)!=by_id.at(node.id)) {expected=by_id.at(id);break;}
          auto p=std::find_if(document_->loaded.nodes.begin(),document_->loaded.nodes.end(),[&](const auto &n){return n.id==id;});if(p==document_->loaded.nodes.end()) break;parent=p->parent;
        }
        if(by_id.at(node.id)->parent()!=expected) {finish_test(false,"场景树父节点不匹配："+node.id);return;}
      }
      scene_reopen_checks_.push_back({{"stage","reopened"},{"objects",document_->loaded.scene.instances.size()},{"hierarchy_valid",true},{"source_options_restored",true}});
      for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) if((*it)->data(0,Qt::UserRole).toInt()==-4) (*it)->setExpanded(true);
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"reopened-scene.png").wstring()));finish_test(true);return;
    }
    if(!loading_&&document_&&state.generation==document_->generation&&state.clicks!=clicks_) {clicks_=state.clicks;choose(state.hit_target,state.hit_joint,state.hit_toggle);}
    if(state.graphics_recovering) {statusBar()->showMessage(QStringLiteral("正在恢复渲染（%1/2）… 场景修改已保留").arg(std::max(1u,state.graphics_attempts)));return;}
    if(!state.error.empty()) {statusBar()->showMessage(QStringLiteral("渲染错误：")+text(state.error));if(self_test_) finish_test(false,state.error);return;}
    if(!state.edit_error.empty()) {statusBar()->showMessage(QStringLiteral("本次编辑未应用：")+text(state.edit_error));if(self_test_) finish_test(false,state.edit_error);return;}
    if(document_&&state.generation==document_->generation&&state.applied_revision==snapshot_.revision) {effective_roots_=state.effective_roots;effective_generation_=state.generation;if(selected_>=0&&size_t(selected_)<state.effective.size()) parameters_->evaluated(state.effective[size_t(selected_)]);}
    if(state.pose_dragging) statusBar()->showMessage((state.pose_gizmo?QStringLiteral("Gizmo 拖动 · 对象变换预览 · 松开后更新场景与渲染质量 · Esc 取消 · %1 ms").arg(state.pose_solve_ms,0,'f',1):QStringLiteral("%1 拖动 · 基础网格预览 · 松开后更新服装、头发和完整质量 · Esc 取消 · %2 ms").arg(state.pose_powerpose?QStringLiteral("PowerPose"):QStringLiteral("IK")).arg(state.pose_solve_ms,0,'f',1)));
    else if(state.pose_restoring) statusBar()->showMessage(state.pose_gizmo?QStringLiteral("正在更新对象变换与渲染质量…"):QStringLiteral("正在恢复姿势的完整形变、服装、头发和渲染质量…"));
    else if(!load_error_.isEmpty()) statusBar()->showMessage(load_error_);
    else if(!recovery_error_.isEmpty()) statusBar()->showMessage(QStringLiteral("自动恢复保存失败：")+recovery_error_);
    else if(!state.resource_error.empty()) statusBar()->showMessage(QStringLiteral("Morph 未应用：")+text(state.resource_error)+QStringLiteral("；可重试加载或刷新参数目录"));
    else if(!state.physics_error.empty()) statusBar()->showMessage(QStringLiteral("快速物理：")+text(state.physics_error));
    else if(state.physics_busy) statusBar()->showMessage(QStringLiteral("快速物理解算中 · 按指定轮数推进 · 画面异步更新"));
    else if(state.pending_payloads) statusBar()->showMessage(QStringLiteral("正在异步载入 %1 项 Morph 数据，完成后应用最新输入…").arg(state.pending_payloads));
    else if(!pending_parameters_.empty()) statusBar()->showMessage(QStringLiteral("有 %1 项参数更改待应用").arg(pending_parameters_.size()));
    else if(!loading_&&document_&&(state.generation!=document_->generation||state.presented_revision!=snapshot_.revision)) statusBar()->showMessage(QStringLiteral("正在更新场景…"));
    else if(!loading_) statusBar()->showMessage(QStringLiteral("OptiX · %1 samples · 网格 %2 · 顶点更新 %3 · 蒙皮求值 %4 · 发丝 %5").arg(state.samples).arg(state.adapter.meshes).arg(state.adapter.geometry_updates).arg(state.skinning.evaluations).arg(state.adapter.curves));
    if(frame_pending_&&document_&&state.generation==document_->generation&&state.applied_revision==snapshot_.revision&&selected_>=0&&size_t(selected_)<state.bounds.size()) {renderer_->frame(state.bounds[size_t(selected_)]);frame_pending_=false;return;}
    if(!self_test_) return;
    if(physics_ui_test_){physics_ui_tick(state);return;}
    if(empty_test_){empty_tick(state);return;}
    if(dufex_test_){dufex_tick(state);return;}
    if(extension_test_){extension_tick(state);return;}
    if(!wear_test_file_.empty()) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>600000) {finish_test(false,"自动穿戴界面验证超时");return;}
      if(loading_||!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.samples<8) return;
      if(test_stage_==0) {
        bool found=false;for(size_t t=0;t<document_->catalog.targets.size();++t) try {if(attachment_host(*document_,t)==t) {wear_test_host_=t;found=true;break;}} catch(const std::exception &) {}
        if(!found) {finish_test(false,"没有自动穿戴测试角色");return;}
        choose(int(wear_test_host_));wear_test_first_=document_->catalog.targets.size();snapshot_.values[wear_test_host_].transform.translation_cm.x=20;
        for(size_t s=0;s<document_->skeletons.skins.size();++s) if(document_->skeletons.skins[s].instance==document_->catalog.targets[wear_test_host_].instance)
          for(size_t j=0;j<document_->skeletons.skins[s].joints.size();++j) if(document_->skeletons.skins[s].joints[j].name=="head") snapshot_.poses[s][j].rotation_degrees.y=12;
        send();++test_stage_;return;
      }
      if(test_stage_==1) {++test_stage_;open_asset(wear_test_file_);return;}
      if(document_->catalog.targets.size()<=wear_test_first_||document_->attachments.empty()||snapshot_.values[wear_test_host_].transform.translation_cm.x!=20) {finish_test(false,"自动穿戴丢失角色状态或附件绑定");return;}
      if(test_stage_==2) {
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"wear-attached.png").wstring()));
        choose(int(wear_test_first_));++test_stage_;change_attachment(true);return;
      }
      if(test_stage_==3) {
        if(!document_->attachments.back().host.empty()) {finish_test(false,"界面解除挂接没有生效");return;}
        ++test_stage_;change_attachment(false,int(wear_test_host_));return;
      }
      if(document_->attachments.back().host.empty()) {finish_test(false,"界面重新绑定没有生效");return;}
      for(const auto &g:state.graft_seams) if(g.max_gap_m>1e-5) {finish_test(false,"自动挂接后的接缝不连续");return;}
      if(!wear_test_material_.empty()) {
        if(test_stage_==4){wear_test_material_operations_=document_->operations.size();choose(int(wear_test_first_));++test_stage_;open_asset(wear_test_material_);return;}
        if(document_->operations.size()<=wear_test_material_operations_){finish_test(false,"穿戴后材质链接没有应用");return;}
      }
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/"wear-rebound.png").wstring()));finish_test(true);return;
    }
    if(edit_regression_test_) {edit_regression_tick(state);return;}
    if(joint_selection_test_) {joint_selection_tick(state);return;}
    if(!selection_test_labels_.empty()) {selection_tick(state);return;}
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
      const auto prefix=visibility_labels_.size()>1?"visible-"+std::to_string(visibility_case_):std::string("visible");
      if(test_stage_==0) {
        for(size_t t=0;t<document_->catalog.targets.size();++t) if(text(document_->catalog.targets[t].label)==visibility_label_) visibility_target_=int(t);
        if(visibility_target_<0) {finish_test(false,"可见性测试对象缺失");return;}
        choose(visibility_target_);visibility_initial_=snapshot_.values[size_t(visibility_target_)].visible;visibility_geometry_updates_=state.adapter.geometry_updates;
        visibility_previous_=state.visible;visibility_geometry_budget_=0;
        visibility_original_=state.visible;visibility_initial_values_.clear();for(const auto &v:snapshot_.values)visibility_initial_values_.push_back(v.visible);
        findChild<QDockWidget *>(QStringLiteral("对象属性与 Morph"))->raise();
        visibility_sessions_=state.sessions;visibility_morph_evaluations_=state.evaluation.morph_evaluations;
        const auto &scene=document_->loaded.scene;auto index=document_->catalog.targets[size_t(visibility_target_)].instance;
        if(scene.instances[index].prototype>=0) index=uint32_t(scene.instances[index].prototype);
        visibility_local_graft_=scene.instances[index].graft_source>=0||std::any_of(scene.instances.begin(),scene.instances.end(),[&](const auto &i){return i.graft_source==int(index);});
        screen()->grabWindow(winId()).save(QString::fromStdWString((output_/(prefix+"-initial.png")).wstring()));
        hierarchy_->currentItem()->setCheckState(0,visibility_initial_?Qt::Unchecked:Qt::Checked);test_stage_=1;return;
      }
      const auto instance=document_->catalog.targets[size_t(visibility_target_)].instance;
      const bool expected=test_stage_==1?!visibility_initial_:visibility_initial_;
      const auto &targets=document_->catalog.targets;
      const auto effective=runtime::effective_visibility(targets,runtime::visibility_children(document_->loaded.scene,targets),snapshot_.values);
      auto expected_visibility=visibility_original_;
      for(size_t t=0;t<targets.size();++t) {
        const bool local=int(t)==visibility_target_?expected:visibility_initial_values_[t];
        if(snapshot_.values[t].visible!=local){finish_test(false,"切换父级改写了子对象自身的显隐状态");return;}
        expected_visibility.at(targets[t].instance)=effective[t];
      }
      if(state.visible!=expected_visibility) {
        finish_test(false,"角色关联对象未同步显隐，或影响了无关对象");return;
      }
      bool tree_matches=true;
      for(QTreeWidgetItemIterator it(hierarchy_);*it;++it) {
        const int t=(*it)->data(0,Qt::UserRole).toInt();if(t<0||(*it)->data(0,Qt::UserRole+1).toInt()>=0)continue;
        const bool local=snapshot_.values[size_t(t)].visible;
        tree_matches&=(*it)->checkState(0)==(local?Qt::Checked:Qt::Unchecked);
      }
      if(!tree_matches) {
        finish_test(false,"场景树、对象状态和渲染可见性不同步");return;
      }
      // 重新显示会按需恢复 GPU 几何；仍限制在本次实际改变显隐的对象范围。
      size_t affected=0;for(size_t i=0;i<state.visible.size();++i) if(state.visible[i]!=visibility_previous_[i]) {++visibility_geometry_budget_;++affected;}
      visibility_previous_=state.visible;
      const auto budget=visibility_geometry_budget_;
      if(state.adapter.geometry_updates-visibility_geometry_updates_>budget||state.sessions!=visibility_sessions_||state.evaluation.morph_evaluations!=visibility_morph_evaluations_) {
        finish_test(false,"显隐更新超出所属 GeoGraft 组合，或重新建立会话／求值 Morph");return;
      }
      screen()->grabWindow(winId()).save(QString::fromStdWString((output_/(prefix+(test_stage_==1?"-toggled.png":"-restored.png"))).wstring()));
      if(test_stage_==1) {hierarchy_->currentItem()->setCheckState(0,visibility_initial_?Qt::Checked:Qt::Unchecked);test_stage_=2;return;}
      if(state.visible!=visibility_original_){finish_test(false,"父级恢复后未保留原先混合的子对象显隐");return;}
      visibility_checks_.push_back({{"label",visibility_label_.toStdString()},{"instance",instance},{"affected_targets",affected},{"local_visibility_preserved",true},{"toggle_restore",true},{"local_graft",visibility_local_graft_},{"geometry_updates",state.adapter.geometry_updates-visibility_geometry_updates_},{"sessions",state.sessions}});
      if(++visibility_case_<visibility_labels_.size()) {visibility_label_=visibility_labels_[visibility_case_];visibility_target_=-1;test_stage_=0;return;}
      finish_test(true);return;
    }
    if(capture_test_) {
      if(QDateTime::currentMSecsSinceEpoch()-test_started_>900000) {finish_test(false,"场景显示验证超时");return;}
      // 切换镜头后不能用上一视角的完成状态通过截图验收。
      if(state.camera.epoch!=renderer_->input_camera().epoch||state.preview)return;
      if(document_&&state.generation==document_->generation&&state.presented_revision==snapshot_.revision&&state.presented_epoch==state.requested_epoch&&state.samples>=(capture_seconds_>0?1:capture_samples_)) {
        if(test_stage_==0&&capture_view_) {choose(-1);renderer_->camera_view(*capture_view_);capture_started_=QDateTime::currentMSecsSinceEpoch();test_stage_=1;return;}
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
  void render_profile_after_empty(const std::filesystem::path &file) {start_profile_after_empty(file);}
  void edit_regression_test() {edit_regression_test_=self_test_=true;}
  void history_test() {history_test_=self_test_=true;renderer_->automated_pointer();}
  void pose_edit_test(int level=-1) {pose_edit_test_=self_test_=true;pose_test_level_=level;renderer_->automated_pointer();}
  void edit_resume_test() {powerpose_test();edit_resume_test_=true;}
  void powerpose_test() {powerpose_test_=self_test_=true;renderer_->automated_pointer();powerpose_->automated_input();}
  void gizmo_test(bool extreme=false) {gizmo_scale_test_=extreme;gizmo_test_=self_test_=true;renderer_->automated_pointer();}
  void group_transform_test() {group_test_=self_test_=true;renderer_->automated_pointer();}
  void node_properties_test() {node_properties_test_=self_test_=true;renderer_->automated_pointer();}
  void group_motion_test(const std::string &label) {group_transform_test();gt_motion_label_=label;}
  void collective_ground_test(const std::string &label) {cg_label_=label;self_test_=true;renderer_->automated_pointer();}
  void ground_test() {ground_test_=self_test_=true;renderer_->automated_pointer();}
  void joint_selection_test() {joint_selection_test_=self_test_=true;renderer_->automated_pointer();GetCursorPos(&workflow_cursor_);}
  void workflow_test() {workflow_test_=true;self_test_=true;GetCursorPos(&workflow_cursor_);}
  void head_selection_test() {workflow_test();head_selection_test_=true;}
  void capture_test(QStringList targets={},bool front=false,bool head=false,int samples=16,double seconds=0) {capture_test_=true;self_test_=true;capture_targets_=std::move(targets);capture_front_=front;capture_head_=head;capture_samples_=std::clamp(samples,1,1<<20);capture_seconds_=seconds;}
  void render_profile(const std::filesystem::path &file) {std::ifstream(file)>>render_plan_;if(!render_plan_.is_array()||render_plan_.empty()) throw std::runtime_error("性能实验计划必须是非空数组");self_test_=true;}
  void selection_test(QStringList labels,bool focus_only=false) {self_test_=true;selection_test_labels_=std::move(labels);focus_only_test_=focus_only;}
  void capture_view(const QString &value) {const auto parts=value.split(',');if(parts.size()!=6) throw std::runtime_error("capture-view 需要 x,y,z,distance,yaw,pitch 六个数值（米/弧度）");std::array<float,6> view;
    for(int i=0;i<6;++i) {bool ok=false;view[i]=parts[i].toFloat(&ok);if(!ok||!std::isfinite(view[i])) throw std::runtime_error("capture-view 数值无效");}
    if(view[3]<=0||std::abs(view[5])>=1.56f) throw std::runtime_error("capture-view 距离或俯仰无效");capture_view_=view;
  }
  void visibility_test(QStringList labels) {visibility_labels_=std::move(labels);visibility_label_=visibility_labels_.front();self_test_=true;}
  void lifecycle_test(const std::filesystem::path &first,const std::filesystem::path &second,int rounds) {if(rounds<1||rounds>100) throw std::runtime_error("生命周期验证轮数应为 1 到 100");lifecycle_rounds_=rounds;lifecycle_test_=true;self_test_=true;lifecycle_first_=first;lifecycle_second_=second;}
  void scene_reopen_test(const std::filesystem::path &file) {scene_reopen_file_=file;self_test_=true;}
  void test_parameters(const QStringList &names) {if(names.empty()) return;formula_names_.clear();for(const auto &name:names) formula_names_.push_back(name.toUtf8().toStdString());}
  Editor(const std::filesystem::path &output,ProjectSettings project,bool self_test,std::filesystem::path reload_file,std::filesystem::path pose_file={},bool pose_test=false,bool formula_test=false,SamplingSettings sampling={}):project_(std::move(project)),output_(output),reload_file_(std::move(reload_file)),pose_file_(std::move(pose_file)),pose_test_(pose_test),formula_test_(formula_test),self_test_(self_test||pose_test||formula_test) {
    setWindowTitle(QStringLiteral("DazFastViewer · 场景与形态编辑器"));setAttribute(Qt::WA_ShowWithoutActivating);
    setDockOptions(AnimatedDocks|AllowNestedDocks|AllowTabbedDocks);
    auto *central=new QWidget;auto *layout=new QVBoxLayout(central);layout->setContentsMargins(4,4,4,4);
    host_=new QWidget;host_->setAttribute(Qt::WA_NativeWindow);host_->setAttribute(Qt::WA_DontCreateNativeAncestors);host_->setMinimumSize(160,120);host_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    layout->addWidget(host_,1);auto *viewport_dock=dock(QStringLiteral("视口"),central,Qt::RightDockWidgetArea);viewport_dock->setObjectName("Viewport");
    browser_=new ContentBrowser;browser_->open_asset=[this](const QString &file){open_asset(file_path(file),QApplication::keyboardModifiers().testFlag(Qt::ControlModifier));};browser_->apply_pose=[this](const QString &file,bool partial){open_asset(file_path(file),partial);};update_libraries();
    auto *explorer_dock=dock(QStringLiteral("内容浏览器"),browser_,Qt::LeftDockWidgetArea);
    hierarchy_=new QTreeWidget;hierarchy_->setSelectionMode(QAbstractItemView::ExtendedSelection);hierarchy_->setHeaderLabel(QStringLiteral("场景对象"));hierarchy_->setMinimumWidth(240);hierarchy_->setIndentation(12);hierarchy_->header()->setStretchLastSection(false);hierarchy_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    hierarchy_->setItemDelegate(new GeograftVisibilityDelegate(hierarchy_));
    auto *hierarchy_panel=new QWidget;auto *hierarchy_layout=new QVBoxLayout(hierarchy_panel);hierarchy_layout->setContentsMargins(0,0,0,0);physics_only_=new QCheckBox(QStringLiteral("仅显示物理模型"));physics_only_->setObjectName("PhysicsOnlyModels");hierarchy_layout->addWidget(physics_only_);hierarchy_layout->addWidget(hierarchy_,1);connect(physics_only_,&QCheckBox::toggled,this,[this]{refresh_physics_filter();});
    auto *hierarchy_dock=dock(QStringLiteral("场景层次"),hierarchy_panel,Qt::LeftDockWidgetArea);tabifyDockWidget(explorer_dock,hierarchy_dock);hierarchy_dock->raise();
    auto *property_scroll=new QScrollArea;property_scroll->setObjectName("ObjectPropertiesScroll");property_scroll->setWidgetResizable(true);property_scroll->setFrameShape(QFrame::NoFrame);auto *panel=new QWidget;auto *properties=new QVBoxLayout(panel);properties->setAlignment(Qt::AlignTop);panel->setMinimumWidth(380);property_scroll->setWidget(panel);
    selection_=new QLabel(QStringLiteral("请先加载并选择对象"));selection_->setWordWrap(true);properties->addWidget(selection_);
    auto *form=new QFormLayout;
    const QString names[]={QStringLiteral("位移 X（厘米）"),QStringLiteral("位移 Y（厘米）"),QStringLiteral("位移 Z（厘米）"),QStringLiteral("旋转 X（度）"),QStringLiteral("旋转 Y（度）"),QStringLiteral("旋转 Z（度）"),QStringLiteral("缩放 X（%）"),QStringLiteral("缩放 Y（%）"),QStringLiteral("缩放 Z（%）")};
    for(int i=0;i<9;++i) {
      auto *spin=new QDoubleSpinBox;transform_[i]=spin;spin->setDecimals(6);spin->setRange(-std::numeric_limits<float>::max(),std::numeric_limits<float>::max());spin->setValue(i>=6?100:0);
      spin->setEnabled(false);spin->setKeyboardTracking(false);form->addRow(names[i],spin);
      connect(spin,&QDoubleSpinBox::valueChanged,this,[this](double) {
        auto edit=history_edit(QStringLiteral("修改对象变换"));
        if(selected_light_>=0) {auto &light=snapshot_.lights[size_t(selected_light_)];runtime::TransformValues v;v.translation_cm={float(transform_[0]->value()),float(transform_[1]->value()),float(transform_[2]->value())};v.rotation_degrees={float(transform_[3]->value()),float(transform_[4]->value()),float(transform_[5]->value())};light.transform=runtime::make_transform(v)*light_base_;send();return;}
        if(selected_<0||selected_joint_>=0) return;auto &t=snapshot_.values[size_t(selected_)].transform;
        t.translation_cm={float(transform_[0]->value()),float(transform_[1]->value()),float(transform_[2]->value())};
        t.rotation_degrees={float(transform_[3]->value()),float(transform_[4]->value()),float(transform_[5]->value())};
        t.scale={float(transform_[6]->value()/100),float(transform_[7]->value()/100),float(transform_[8]->value()/100)};send();
      });
    }
    auto *legacy_transforms=new QWidget(panel);legacy_transforms->setLayout(form);legacy_transforms->hide();
    light_power_=parameter_widgets::number("LightPower",true);light_power_->setRange(-std::numeric_limits<float>::max(),std::numeric_limits<float>::max());light_power_->setPrefix(QStringLiteral("灯光功率 "));light_power_->setKeyboardTracking(false);light_power_->setObjectName("LightPower");light_power_row_=new QWidget;auto *power_layout=new QVBoxLayout(light_power_row_);power_layout->setContentsMargins(0,0,0,0);power_layout->addWidget(light_power_);parameter_widgets::decorate(light_power_row_,"light/");light_power_row_->hide();properties->addWidget(light_power_row_);
    light_power_->setProperty("historyInput",true);
    connect(light_power_,&QDoubleSpinBox::valueChanged,this,[this](double value) {if(selected_light_<0) return;auto edit=history_edit(QStringLiteral("修改灯光功率"));auto &p=snapshot_.lights[size_t(selected_light_)].power;const auto previous=std::max({p.x,p.y,p.z});const float ratio=previous>0?float(value)/previous:0;p=previous>0?ir::Vec3{p.x*ratio,p.y*ratio,p.z*ratio}:ir::Vec3{float(value),float(value),float(value)};send();});
    pose_status_=new QLabel(QStringLiteral("双击加载姿势、形态或材质；单选骨骼后 Ctrl+左键双击姿势，仅应用该部位及其后代。"));pose_status_->setWordWrap(true);properties->addWidget(pose_status_);
    connect(hierarchy_,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem *item,int column) {const int target=item->data(0,Qt::UserRole).toInt();if(column!=0)return;if(target>=0&&item->data(0,Qt::UserRole+1).toInt()<0)set_visible(size_t(target),item->checkState(0)==Qt::Checked);else if(target==-4)set_node_property(item->data(0,Qt::UserRole+3).toString().toStdString(),false,item->checkState(0)==Qt::Checked);});
    auto *reset=new QPushButton(QStringLiteral("重置选中对象"));properties->addWidget(reset);connect(reset,&QPushButton::clicked,this,[this] {reset_selected();});
    extension_panel_=new ExtensionPanel;properties->addWidget(extension_panel_);extension_panel_->changed=[this](runtime::ObjectExtension v,bool shape,double step){change_extension(v,shape,step);};
    physics_panel_=new PhysicsPanel;properties->addWidget(physics_panel_);physics_panel_->changed=[this](runtime::PhysicsObjectSettings value){change_physics(std::move(value));};
    physics_panel_->simulate=[this](bool reset){simulate_physics(reset);};physics_panel_->hovered=[this](int slot){std::vector<MaterialSurface> surfaces;if(document_&&selected_>=0&&slot>=0)surfaces.push_back({document_->catalog.targets[size_t(selected_)].instance,size_t(slot)});if(renderer_)renderer_->hover_materials(document_?document_->generation:0,surfaces);};
    ground_panel_=new GroundPanel;properties->addWidget(ground_panel_);ground_panel_->align=[this]{request_ground();};
    ground_panel_->changed=[this](double ratio,double offset,bool body_only){const int target=ground_target();if(target==-1||target==-4)return;auto edit=history_edit(QStringLiteral("修改地面对齐设置"));if(target>=0){auto &v=snapshot_.values[target];v.ground_alignment_ratio=ratio;v.ground_alignment_offset_cm=offset;v.ground_alignment_body_only=body_only;}else{auto &v=snapshot_.instance_ground[ground_id(target)];v.ratio=ratio;v.offset_cm=offset;v.body_only=body_only;}};
    geograft_panel_=new GeograftPanel;properties->addWidget(geograft_panel_);geograft_panel_->changed=[this](size_t target,bool enabled){
      if(loading_||!document_||target>=snapshot_.values.size())return;auto &value=snapshot_.values[target];if(value.graft_enabled==enabled)return;
      auto edit=history_edit(enabled?QStringLiteral("启用 Geograft"):QStringLiteral("停用 Geograft"));value.graft_enabled=enabled;send();
    };
    auto *morph_header=new QToolButton;morph_header->setObjectName("MorphCollapse");morph_header->setText(QStringLiteral("参数"));morph_header->setCheckable(true);morph_header->setChecked(true);morph_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);morph_header->setArrowType(Qt::DownArrow);properties->addWidget(morph_header);
    auto *morph_body=new QWidget;auto *morph_layout=new QVBoxLayout(morph_body);morph_layout->setContentsMargins(0,0,0,0);properties->addWidget(morph_body);
    parameters_=new ParameterPanel;parameters_->shared_scroll(property_scroll);parameters_->changed=[this](size_t index,double value) {set_morph(index,value);};morph_layout->addWidget(parameters_);
    water_panel_=new water::Panel;morph_layout->addWidget(water_panel_);
    cloud_panel_=new cloud::Panel;morph_layout->addWidget(cloud_panel_);cloud_panel_->refresh_candidates=[this]{bind_cloud();};cloud_panel_->changed=[this](cloud::Config config){if(const auto *v=selected_cloud())generate_cloud(std::move(config),v->id);};
    water_panel_->refresh_candidates=[this]{bind_water();};
    water_panel_->changed=[this](water::Config config,bool recalculate){if(const auto *w=selected_water())generate_water(std::move(config),w->id,recalculate);};water_panel_->cancel=[this]{loader_.request_stop();};
    environment_tools_=new QWidget;auto *environment_actions=new QHBoxLayout(environment_tools_);environment_actions->setContentsMargins(0,0,0,0);auto *environment_image=new QPushButton(QStringLiteral("选择环境贴图…"));environment_image->setObjectName("EnvironmentImage");auto *clear_environment_image=new QPushButton(QStringLiteral("清除贴图"));environment_actions->addWidget(environment_image);environment_actions->addWidget(clear_environment_image);morph_layout->addWidget(environment_tools_);environment_tools_->hide();
    connect(environment_image,&QPushButton::clicked,this,[this]{if(loading_||!document_)return;auto file=QFileDialog::getOpenFileName(this,QStringLiteral("环境贴图"),{},QStringLiteral("环境图像 (*.hdr *.exr *.png *.jpg *.jpeg *.tif *.tiff)"));if(file.isEmpty())return;auto edit=history_edit(QStringLiteral("选择环境贴图"));snapshot_.options.environment_file=file_path(file);for(auto &p:snapshot_.options.environment.parameters)if(p.id=="Environment Map")p.image_uri=file.toStdString();select(-2);send();});
    connect(clear_environment_image,&QPushButton::clicked,this,[this]{if(loading_||!document_)return;auto edit=history_edit(QStringLiteral("清除环境贴图"));snapshot_.options.environment_file.clear();for(auto &p:snapshot_.options.environment.parameters)if(p.id=="Environment Map")p.image_uri.clear();select(-2);send();});
    parameters_->favorite_changed=[this](const std::string &node,const std::string &id,bool enabled){
      if(loading_||!document_)return;
      auto edit=history_edit(QStringLiteral("修改参数收藏"));
      auto &favorites=selected_>=0&&selected_light_<0?snapshot_.values.at(size_t(selected_)).favorites:snapshot_.control_favorites;
      if(favorites)favorites->nodes[node][id]=enabled;
    };
    connect(morph_header,&QToolButton::toggled,this,[this,morph_header,morph_body](bool on){morph_body->setVisible(on);morph_header->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});
    auto *parameter_actions=new QHBoxLayout;
    refresh_parameters_=new QPushButton(QStringLiteral("刷新参数目录"));retry_parameters_=new QPushButton(QStringLiteral("重试加载"));retry_parameters_->setEnabled(false);
    parameter_actions->addWidget(refresh_parameters_);parameter_actions->addWidget(retry_parameters_);morph_layout->addLayout(parameter_actions);
    connect(refresh_parameters_,&QPushButton::clicked,this,[this]{load_error_.clear();refresh_parameter_catalog();});
    connect(retry_parameters_,&QPushButton::clicked,this,[this]{renderer_->retry_resources();send();});
    auto *apply_actions=new QHBoxLayout;manual_morph_=new QCheckBox(QStringLiteral("手动应用参数"));apply_parameters_=new QPushButton(QStringLiteral("应用"));apply_parameters_->setEnabled(false);
    apply_actions->addWidget(manual_morph_);apply_actions->addWidget(apply_parameters_);morph_layout->addLayout(apply_actions);
    connect(apply_parameters_,&QPushButton::clicked,this,[this]{if(history_)history_->finish_gesture();apply_parameters();});connect(manual_morph_,&QCheckBox::toggled,this,[this](bool manual){if(history_)history_->finish_gesture();if(!manual&&!pending_parameters_.empty()){QSignalBlocker block(manual_morph_);manual_morph_->setChecked(true);auto edit=history_edit(QStringLiteral("关闭手动模式并应用参数"));manual_morph_->setChecked(false);apply_parameters();}checkpoint();});
    auto *property_dock=dock(QStringLiteral("对象属性与 Morph"),property_scroll,Qt::RightDockWidgetArea);splitDockWidget(viewport_dock,property_dock,Qt::Horizontal);
    materials_=new MaterialPanel;materials_->changed=[this]{if(!loading_&&renderer_)send();};materials_->interaction_changed=[this](bool active){history_interaction(active,quintptr(materials_));};
    materials_->edit_requested=[this](const QString &name,const std::function<void()> &change){if(loading_)return;auto edit=history_edit(name);change();};
    materials_->preset_requested=[this](const auto &file,const auto &surfaces){apply_surface_material_file(file,surfaces);};
    materials_->paste_requested=[this](const auto &copy,const auto &surfaces){
      if(loading_||!document_||surfaces.empty())return;auto edit=history_edit(QStringLiteral("粘贴材质"));
      try{const auto selection=materials_->selection_ids();auto next=std::make_shared<Document>(*document_);auto snapshot=snapshot_;const auto warnings=paste_material(*next,snapshot,copy,surfaces);
        next->generation=++generation_;next->loaded.scene.lights=snapshot.lights;document_=std::move(next);snapshot_=std::move(snapshot);snapshot_.generation=document_->generation;++snapshot_.revision;
        select(selected_,selected_joint_,selected_light_);materials_->restore_selection(selection);renderer_->set_document(document_,submitted_snapshot(),false);
        if(!warnings.empty()){QStringList messages;for(const auto &warning:warnings)messages.push_back(text(warning));QMessageBox::information(this,QStringLiteral("材质已粘贴"),messages.join("\n"));}
      }catch(const std::exception &e){QMessageBox::warning(this,QStringLiteral("材质粘贴失败"),text(e.what()));}
    };
    auto change_uv=[this](const daz::MaterialUVSet &uv,const std::vector<MaterialSurface> &surfaces,bool reset){
      if(loading_||!document_||surfaces.empty())return;auto edit=history_edit(reset?QStringLiteral("还原 UV Set"):QStringLiteral("切换 UV Set"));
      try{const auto selection=materials_->selection_ids();auto next=std::make_shared<Document>(*document_);
        if(!(reset?reset_material_uv(*next,surfaces):change_material_uv(*next,uv,surfaces)))return;
        next->generation=++generation_;next->loaded.scene.lights=snapshot_.lights;document_=std::move(next);snapshot_.generation=document_->generation;++snapshot_.revision;
        select(selected_,selected_joint_,selected_light_);materials_->restore_selection(selection);renderer_->set_document(document_,submitted_snapshot(),false);
        statusBar()->showMessage(reset?QStringLiteral("已还原所选表面的 UV Set"):QStringLiteral("已切换 UV Set：")+text(uv.label),5000);
      }catch(const std::exception &e){QMessageBox::warning(this,QStringLiteral("UV Set 切换失败"),text(e.what()));}
    };
    materials_->uv_requested=[change_uv](const auto &uv,const auto &surfaces){change_uv(uv,surfaces,false);};
    materials_->uv_reset_requested=[change_uv](const auto &surfaces){change_uv({},surfaces,true);};
    materials_->locate_file=[this](const QString &file){if(browser_->locate(file)){if(auto *dock=qobject_cast<QDockWidget *>(browser_->parentWidget())){dock->show();dock->raise();}}else statusBar()->showMessage(QStringLiteral("文件不存在：%1").arg(file),5000);};
    materials_->hovered=[this](const auto &surfaces){if(renderer_)renderer_->hover_materials(document_?document_->generation:0,surfaces);};
    auto *material_dock=dock(QStringLiteral("材质"),materials_,Qt::RightDockWidgetArea);material_dock->setObjectName("Materials");tabifyDockWidget(property_dock,material_dock);
    powerpose_=new PowerPosePanel;auto *powerpose_dock=dock(QStringLiteral("PowerPose"),powerpose_,Qt::RightDockWidgetArea);tabifyDockWidget(property_dock,powerpose_dock);powerpose_dock->raise();
    powerpose_->selection=[this](const auto &p){select_powerpose(p);};
    powerpose_->input=[this](auto input){if(renderer_) renderer_->powerpose(std::move(input));};
    powerpose_->action=[this](const auto &p,int operation,bool enabled){powerpose_action(p,operation,enabled);};
    install_chrome();
    chrome->changed=[this](GizmoSettings settings){if(renderer_) renderer_->gizmo(settings);};
    connect(chrome->ground_action(),&QAction::triggered,this,[this]{request_ground();});
    connect(chrome->weight_action(),&QAction::triggered,this,[this]{enable_extension();});
    auto *file_menu=chrome->menus()->addMenu(QStringLiteral("文件"));open_=file_menu->addAction(QStringLiteral("添加 / 应用 DUF…"));open_->setShortcut(QKeySequence::Open);
    connect(open_,&QAction::triggered,this,[this] {const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("加载角色、场景或姿势"),{},QStringLiteral("DAZ 资源 (*.duf *.dufex *.djl *.dse)"));if(!file.isEmpty()) open_asset(file_path(file));});
    auto *save=file_menu->addAction(QStringLiteral("保存场景修改"));save->setObjectName("SaveScene");save->setShortcut(QKeySequence::Save);save->setAutoRepeat(false);addAction(save);connect(save,&QAction::triggered,this,[this]{save_extension();});
    connect(file_menu->addAction(QStringLiteral("场景修改另存为…")),&QAction::triggered,this,[this]{save_extension(true);});
    connect(file_menu->addAction(QStringLiteral("近期使用…")),&QAction::triggered,this,[this,explorer_dock]{explorer_dock->show();explorer_dock->raise();browser_->show_recent();});
    auto *project_menu=chrome->menus()->addMenu(QStringLiteral("项目"));project_action_=project_menu->addAction(QStringLiteral("项目设置…"));
    connect(project_action_,&QAction::triggered,this,[this] {project_settings();});
    connect(file_menu->addAction(QStringLiteral("退出")),&QAction::triggered,this,&QWidget::close);
    auto *focus_action=new QAction(QStringLiteral("聚焦选中对象（F）"),this);hierarchy_->addAction(focus_action);focus_action->setShortcut(QKeySequence(Qt::Key_F));focus_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);connect(focus_action,&QAction::triggered,this,[this]{focus_selection();});
    auto *edit=chrome->menus()->addMenu(QStringLiteral("编辑"));initialize_history(edit);edit->addAction(focus_action);connect(edit->addAction(QStringLiteral("重置选中对象")),&QAction::triggered,this,[this] {reset_selected();});
    edit->addAction(chrome->ground_action());edit->addAction(chrome->weight_action());
    auto *units=edit->addAction(QStringLiteral("测量单位…"));units->setObjectName("MeasurementUnits");connect(units,&QAction::triggered,this,[this]{measurement_units();});
    auto *attachment_menu=chrome->menus()->addMenu(QStringLiteral("穿戴与附件"));
    auto *copy_outfit_action=attachment_menu->addAction(QStringLiteral("复制穿搭"));copy_outfit_action->setObjectName("CopyOutfit");connect(copy_outfit_action,&QAction::triggered,this,[this]{copy_selected_outfit();});
    auto *fit=attachment_menu->addAction(QStringLiteral("绑定到角色 / 更换目标…"));connect(fit,&QAction::triggered,this,[this]{change_attachment(false);});
    auto *detach=attachment_menu->addAction(QStringLiteral("解除挂接"));detach->setToolTip(QStringLiteral("整套附件恢复独立载入位置，角色被遮盖的表面随绑定关系重新计算"));connect(detach,&QAction::triggered,this,[this]{change_attachment(true);});
    delete_=edit->addAction(QStringLiteral("删除选中对象及其子对象"));delete_->setShortcut(QKeySequence::Delete);delete_->setEnabled(false);
    delete_->setToolTip(QStringLiteral("删除选中模型、子对象及绑定的穿戴物；选择骨骼部位时删除所属模型"));
    connect(delete_,&QAction::triggered,this,[this] {delete_selection();});
    hierarchy_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hierarchy_,&QWidget::customContextMenuRequested,this,[this,fit,detach,explorer_dock](const QPoint &point) {
      auto *item=hierarchy_->itemAt(point);if(!item)return;if(item->isSelected())hierarchy_->setCurrentItem(item,0,QItemSelectionModel::NoUpdate);else hierarchy_->setCurrentItem(item);QMenu menu(this);menu.addAction(fit);menu.addAction(detach);
      ContentOrigin origin;const int target=item->data(0,Qt::UserRole).toInt();
      if(document_){int instance=target>=0&&size_t(target)<document_->catalog.targets.size()?int(document_->catalog.targets[size_t(target)].instance):(target<=-5?-5-target:-1);
        if(instance>=0&&size_t(instance)<document_->loaded.scene.instances.size()){while(document_->loaded.scene.instances[size_t(instance)].prototype>=0)instance=document_->loaded.scene.instances[size_t(instance)].prototype;for(const auto &object:document_->loaded.objects)if(object.instance==uint32_t(instance)){origin.scene_file=QString::fromStdWString(object.source_file.wstring());origin.node=text(object.source_node.empty()?object.id:object.source_node);origin.label=text(object.label);origin.geometries.emplace_back(QString::fromStdWString(object.geometry_file.wstring()),text(object.geometry_id));for(const auto &source:object.geometry_sources)origin.geometries.emplace_back(QString::fromStdWString(source.file.wstring()),text(source.id));break;}}}
      menu.addSeparator();auto *locate=menu.addAction(QStringLiteral("在内容库中定位"));locate->setObjectName("LocateSceneContent");locate->setEnabled(!origin.geometries.empty());locate->setToolTip(QStringLiteral("查找此模型对应的资源，展开所在文件夹并选中资源图标"));menu.addSeparator();menu.addAction(delete_);
      if(menu.exec(hierarchy_->viewport()->mapToGlobal(point))==locate){explorer_dock->show();explorer_dock->raise();browser_->locate_asset(origin);}
    });
    connect(file_menu->addAction(QStringLiteral("新建空场景")),&QAction::triggered,this,[this] {clear_scene();});
    connect(file_menu->addAction(QStringLiteral("打开场景（替换）…")),&QAction::triggered,this,[this] {const auto file=QFileDialog::getOpenFileName(this,QStringLiteral("打开场景"),{},QStringLiteral("DAZ 场景 (*.duf *.dufex)"));if(!file.isEmpty()) load(file_path(file));});
    auto *create=chrome->menus()->addMenu(QStringLiteral("创建"));
    auto *create_environment=create->addAction(QStringLiteral("环境设置"));create_environment->setObjectName("CreateEnvironment");connect(create_environment,&QAction::triggered,this,[this]{create_options(true);});
    auto *create_tone=create->addAction(QStringLiteral("色调设置"));create_tone->setObjectName("CreateTonemapper");connect(create_tone,&QAction::triggered,this,[this]{create_options(false);});
    auto *create_water=create->addAction(QStringLiteral("水体"));create_water->setObjectName("CreateWater");connect(create_water,&QAction::triggered,this,[this]{generate_water({}, {},false);});
    auto *create_cloud=create->addAction(QStringLiteral("体积云"));create_cloud->setObjectName("CreateCloud");connect(create_cloud,&QAction::triggered,this,[this]{generate_cloud({});});
    install_city_ui(create);
    connect(create->addAction(QStringLiteral("面光源")),&QAction::triggered,this,[this] {add_light();});
    auto *view=chrome->menus()->addMenu(QStringLiteral("视图"));for(auto *d:findChildren<QDockWidget *>()) view->addAction(d->toggleViewAction());
    connect(view->addAction(QStringLiteral("重启渲染")),&QAction::triggered,this,[this] {if(renderer_)renderer_->restart_render();});
    connect(view->addAction(QStringLiteral("框选当前对象")),&QAction::triggered,this,[this] {frame_pending_=true;});
    connect(hierarchy_,&QTreeWidget::itemSelectionChanged,this,[this] {sync_selection();});
    connect(hierarchy_,&QTreeWidget::currentItemChanged,this,[this] {sync_selection();});
    chrome->add_layout_actions(view);
    connect(view->addAction(QStringLiteral("恢复默认布局")),&QAction::triggered,this,[this] {restoreState(default_layout_,1);chrome->reset_modules();});
    QScreen *initial_screen=QGuiApplication::primaryScreen();
    if(!initial_screen) throw std::runtime_error("找不到可用显示器");
    // Prefer the secondary screen only for automated interaction diagnostics.
    if(self_test_) for(auto *screen:QGuiApplication::screens()) if(screen!=initial_screen) {initial_screen=screen;break;}
    const QRect available=initial_screen->availableGeometry();
    resize(QSize(1580,920).boundedTo(available.size()));move(available.center()-QPoint(width()/2,height()/2));
    resizeDocks({viewport_dock,property_dock},{850,330},Qt::Horizontal);default_layout_=saveState(1);
    auto fit_windows=[this] {
      QList<QRect> areas;for(auto *screen:QGuiApplication::screens()) areas.push_back(screen->availableGeometry());
      if(areas.isEmpty()) return;
      const auto preferred=QGuiApplication::primaryScreen()->availableGeometry();
      auto fit=[&](QWidget *window) {
        const auto bounds=visible_window_geometry(QRect(window->pos(),window->size()),areas,preferred);
        window->resize(bounds.size());window->move(bounds.topLeft());
      };
      if(!isMaximized()&&!isFullScreen()) fit(this);
      for(auto *dock:findChildren<QDockWidget *>()) if(dock->isFloating()&&!dock->isMaximized()&&!dock->isFullScreen()) fit(dock);
    };
    if(!self_test_) application_settings_=ApplicationSettings::load();
    if(!self_test_&&!sampling.quality_override) static_cast<RenderQuality &>(sampling)=application_settings_.render;
    else application_settings_.render=sampling;
    // 状态栏也须在缩放与停靠恢复之前创建，否则首次消息改变栏高后会重新分配面板尺寸。
    statusBar()->ensurePolished();
    ui_scale_=new UiScale(false,{},this);ui_scale_->set_percent(application_settings_.ui_percent);
    // 先应用字体和尺寸，再恢复用户停靠布局，避免首次样式刷新覆盖保存的尺寸。
    if(!self_test_) {new DialogLayouts({},this);QSettings settings;powerpose_->restore_template(settings.value("powerpose/template","Body").toString());}
    viewport_settings_=new ViewportSettings(false,{},this);viewport_settings_->set(application_settings_.viewport);
    viewport_settings_->changed=[this](ViewportQuality value){if(renderer_) renderer_->quality(value);};
    fit_windows();
    auto watch_screen=[this,fit_windows](QScreen *screen) {
      connect(screen,&QScreen::availableGeometryChanged,this,[this,fit_windows]{QTimer::singleShot(0,this,fit_windows);});
    };
    for(auto *screen:QGuiApplication::screens()) watch_screen(screen);
    connect(qApp,&QGuiApplication::screenAdded,this,[watch_screen](QScreen *screen){watch_screen(screen);});
    connect(qApp,&QGuiApplication::screenRemoved,this,[this,fit_windows]{QTimer::singleShot(0,this,fit_windows);});
    if(!self_test_){QSettings settings;restore_layout(settings);}else show();
    // Native frame margins and the first dock layout settle after the initial show.
    QTimer::singleShot(0,this,fit_windows);
    // 样式首次重建后重新落实默认停靠宽度；已保存的用户布局仍由 restoreState 负责。
    if(self_test_||QSettings().value("window/docks").toByteArray().isEmpty()) resizeDocks({viewport_dock,property_dock},{850,330},Qt::Horizontal);
    renderer_=std::make_unique<Renderer>(reinterpret_cast<HWND>(host_->winId()),qRound(host_->width()*host_->devicePixelRatioF()),qRound(host_->height()*host_->devicePixelRatioF()),output_,sampling);
    renderer_->physics_options(project_.physics);
    renderer_->quality(viewport_settings_->value());
    if(!self_test_){try{measurement_scale_=runtime::measurement_scale(QSettings().value("measurement/scale","1").toString().toStdString());}catch(...){measurement_scale_="1";}}
    extension_panel_->measurement_scale(measurement_scale_);
    renderer_->history_requests([this](bool redo){history_move(redo);});
    renderer_->delete_requests([this]{QMetaObject::invokeMethod(this,[this]{if(!closing_&&!loading_)delete_->trigger();},Qt::QueuedConnection);});
    renderer_->save_requests([this,save]{QMetaObject::invokeMethod(this,[this,save]{if(!closing_&&!loading_&&!saving_extension_&&!QApplication::activeModalWidget())save->trigger();},Qt::QueuedConnection);});
    parameters_->interaction_changed=[this](bool active){history_interaction(active,quintptr(parameters_));};
    connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state){if(state!=Qt::ApplicationActive){if(history_input_)history_input_->finish();if(renderer_)renderer_->interaction(false);}});
    auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this] {tick();});timer->start(50);
  }
  void closeEvent(QCloseEvent *event) override {if(renderer_)accept_pose_commit(renderer_->status());if(auto *focus=QApplication::focusWidget())focus->clearFocus();if(history_input_)history_input_->finish();closing_=true;checkpoint();if(!self_test_) {QSettings settings;save_layout(settings);settings.setValue("powerpose/template",powerpose_->template_name());settings.sync();browser_->save();}QMainWindow::closeEvent(event);}
  ~Editor() override {closing_=true;loader_.request_stop();if(loader_.joinable()) loader_.join();if(history_input_){delete history_input_;history_input_=nullptr;}if(history_)history_->finish_gesture();auto final_state=recovery_?std::optional<EditState>(capture_edit()):std::nullopt;renderer_.reset();if(recovery_&&final_state){recovery_->checkpoint(std::move(*final_state),roots_,true);recovery_->flush();}}
  void options_test() {options_test_=self_test_=true;}
  void navigation_test() {navigation_test_=self_test_=true;}
  void interaction_test() {interaction_test_=self_test_=true;for(auto *timer:findChildren<QTimer *>()) if(timer->interval()==50) timer->setInterval(16);}
  void keep_open_after_test() {keep_open_after_test_=true;}
  void attachment_test() {attachment_test_=self_test_=true;}
  void lazy_test() {lazy_test_=self_test_=true;}
  void wear_test(const std::filesystem::path &file,const std::filesystem::path &material={}) {wear_test_file_=file;wear_test_material_=material;self_test_=true;}
  void rebuild_test(const std::filesystem::path &file) {rebuild_test_file_=file;self_test_=true;}
  void subdivision_stress_test() {subdivision_stress_test_=true;self_test_=true;}
  void extension_test(){extension_test_=true;self_test_=true;}
  void weight_module_test(std::string target){extension_module_target_=std::move(target);extension_test();}
  void dufex_test(){dufex_test_=true;self_test_=true;}
  void feedback_test(){feedback_test_=true;self_test_=true;}
  void city_test(){city_test_=self_test_=true;}
  void water_test(){water_test_=self_test_=true;}
  void cloud_test(){cloud_test_=self_test_=true;}
  void water_scene_test(){water_scene_test_=water_test_=self_test_=true;}
  void empty_scene(){clear_scene();if(history_){history_->clear();history_->mark_saved();}checkpoint();}
  void physics_ui_test(){physics_ui_test_=self_test_=true;}
  void empty_test(){empty_test_=self_test_=true;}
  void load(const std::filesystem::path &file,bool preserve=false,bool append=false,std::string attachment_target={},std::filesystem::path entry={}) {
    if(loading_) {statusBar()->showMessage(QStringLiteral("正在加载，请稍候…"));return;}
    if(history_)history_->finish_gesture();
    loading_=true;load_error_.clear();open_->setEnabled(false);project_action_->setEnabled(false);statusBar()->showMessage(QStringLiteral("正在后台解析场景与参数依赖…"));
    // 新建产生有效 Document，但它还不是需要保留环境的已有场景。
    const bool empty=!document_||(document_->loaded.nodes.empty()&&document_->loaded.scene.instances.empty()&&snapshot_.lights.empty()&&snapshot_.options.environment.id.empty()&&snapshot_.options.tonemapper.id.empty()&&snapshot_.options.environment_file.empty());
    const auto generation=++generation_;const auto roots=roots_;const auto previous_document=append&&!empty?document_:nullptr;
    const bool profiling=!rebuild_test_file_.empty();
    loader_=std::jthread([this,file,roots,generation,preserve,previous_document,attachment_target,entry,profiling](std::stop_token stop) {
      try {
        diagnostics::EventProfile profile(profiling,[this](const char *name,double ms){renderer_->trace(name,ms);});
        diagnostics::Scope loading_scope("asset_load");
        auto document=std::make_shared<Document>();document->generation=generation;document->source_file=std::filesystem::absolute(file);
        const bool has_sidecar=daz::entry_key(file.extension())==".dufex";const auto sidecar=has_sidecar?file:std::filesystem::path{};
        Snapshot loaded_snapshot;
        if(has_sidecar){progress(QStringLiteral("正在加载 DUFEX 场景修改…"));auto restored=load_scene_extension(sidecar,roots,generation,[this,stop](const std::string &message){if(stop.stop_requested())throw std::runtime_error("已取消加载");progress(text(message));});document=std::move(restored.document);loaded_snapshot=std::move(restored.snapshot);}
        else {
        {diagnostics::Scope scope("geometry_materials");document->loaded=daz::load(file,{roots,false,!attachment_target.empty()});}
        std::vector<std::filesystem::path> resolved;for(const auto &p:document->loaded.report["content_roots"]) resolved.push_back(std::filesystem::u8path(p.get<std::string>()));
        progress(QStringLiteral("正在发现 Morph 与读取场景参数…"));
        {diagnostics::Scope scope("morph_discovery");document->catalog=daz::discover_morphs(document->loaded,resolved,[this,stop](const std::string &message) {if(stop.stop_requested()) throw std::runtime_error("已取消加载");progress(text(message));},true);}
        progress(QStringLiteral("正在解析骨架与场景姿势…"));
        {diagnostics::Scope scope("skeleton_load");document->skeletons=daz::load_skeletons(document->loaded);}
        progress(QStringLiteral("正在编译 Formula / ERC…"));
        {diagnostics::Scope scope("formula_compile");document->formulas=daz::enable_formulas(document->catalog,document->skeletons);}
        if(!previous_document&&document->loaded.scene.lights.empty()){
          const auto instances=document->loaded.scene.instances.size();ir::add_studio(document->loaded.scene);
          if(document->loaded.scene.instances.size()!=instances)document->operations.push_back({{"op","studio"}});
        }
        loaded_snapshot=initial_snapshot(*document);normalize_object_extensions(*document,loaded_snapshot);
        }
        std::ofstream(output_/"asset-report.json")<<document->loaded.report.dump(2);
        std::ofstream(output_/"morph-catalog.json")<<document->catalog.report.dump(2);
        std::ofstream(output_/"skeleton-report.json")<<document->skeletons.report.dump(2);
        std::ofstream(output_/"formula-report.json")<<document->formulas.report.dump(2);
        const auto category=has_sidecar?QStringLiteral("scene"):content_category(*daz::document_view(file));
        release_load_data(*document);
        if(previous_document) {
          diagnostics::Scope scope("merge_bind");
          auto merged=std::make_shared<Document>(*previous_document);merged->generation=generation;const auto first=merged->catalog.targets.size();
          const auto first_skin=merged->skeletons.skins.size(),first_light=merged->loaded.scene.lights.size();
          append_document(*merged,std::move(*document));
          const std::string prefix=merged->operations.back().at("prefix");auto merged_snapshot=initial_snapshot(*merged);
          for(size_t t=0;t<loaded_snapshot.values.size();++t)merged_snapshot.values.at(first+t)=loaded_snapshot.values[t];
          for(size_t s=0;s<loaded_snapshot.poses.size();++s)merged_snapshot.poses.at(first_skin+s)=loaded_snapshot.poses[s];
          for(auto pin:loaded_snapshot.pose_pins){pin.skin+=int(first_skin);merged_snapshot.pose_pins.push_back(pin);}
          for(const auto &[id,level]:loaded_snapshot.subdivision_levels)merged_snapshot.subdivision_levels[prefix+id]=level;
          for(const auto &[id,patch]:loaded_snapshot.material_overrides)merged_snapshot.material_overrides[prefix+id]=patch;
          for(const auto &[id,value]:loaded_snapshot.group_transforms)merged_snapshot.group_transforms[prefix+id]=value;
          for(const auto &[id,value]:loaded_snapshot.node_properties)merged_snapshot.node_properties[prefix+id]=value;
          for(const auto &[id,value]:loaded_snapshot.instance_ground)merged_snapshot.instance_ground[prefix+id]=value;
          for(const auto &[id,value]:loaded_snapshot.city_views)merged_snapshot.city_views[prefix+id]=value;
          for(const auto &[id,value]:loaded_snapshot.parameter_settings)if(!id.starts_with("@"))merged_snapshot.parameter_settings[prefix+id]=value;
          for(size_t l=0;l<loaded_snapshot.lights.size();++l){auto value=loaded_snapshot.lights[l];value.id=prefix+value.id;merged_snapshot.lights.at(first_light+l)=value;}
          if(loaded_snapshot.control_favorites){
            merged_snapshot.control_favorites.emplace();const auto &favorites=*loaded_snapshot.control_favorites;
            for(const auto &light:loaded_snapshot.lights)for(int i=0;i<6;++i){
              const auto id="light/"+std::to_string(i);const auto node=favorites.nodes.find("light/"+light.id);
              const bool enabled=node!=favorites.nodes.end()&&node->second.contains(id)?node->second.at(id):favorites.fallback.contains(id);
              merged_snapshot.control_favorites->nodes["light/"+prefix+light.id][id]=enabled;
            }
          }
          loaded_snapshot=std::move(merged_snapshot);
          if(!attachment_target.empty()) {
            auto host=std::find_if(merged->catalog.targets.begin(),merged->catalog.targets.begin()+first,[&](const auto &t){return t.id==attachment_target;});
            if(host==merged->catalog.targets.begin()+first) throw std::runtime_error("穿戴目标已不存在，请重新选择角色");
            progress(QStringLiteral("正在绑定服装 / 头发 / 角色附件…"));attach_import(*merged,first,size_t(host-merged->catalog.targets.begin()));
          }
          document=std::move(merged);
        }
        QMetaObject::invokeMethod(this,[this,document,preserve,previous_document,file,category,entry,sidecar,loaded_snapshot=std::move(loaded_snapshot)] {
          if(closing_)return;auto edit=history_edit(previous_document?QStringLiteral("添加资源"):QStringLiteral("打开场景"));
          if(!preserve&&!previous_document)scene_identity_=++scene_serial_;
          const auto old=document_;const auto previous=snapshot_;parameters_->bind(nullptr,nullptr);
          if(!preserve&&!previous_document) {pending_parameters_.clear();apply_parameters_->setEnabled(false);}
          document_=document;loading_=false;open_->setEnabled(true);project_action_->setEnabled(true);snapshot_=loaded_snapshot;snapshot_.values.clear();snapshot_.poses.clear();snapshot_.options=(preserve||previous_document)?previous.options:loaded_snapshot.options;if(preserve||previous_document){for(const auto &entry:previous.subdivision_levels){snapshot_.subdivision_levels[entry.first]=entry.second;}}snapshot_.generation=document->generation;snapshot_.revision=1;
          if(!previous_document)extension_file_=sidecar;
          if(preserve||previous_document){for(const auto &entry:previous.city_views)snapshot_.city_views[entry.first]=entry.second;}
          if(preserve||previous_document)for(const auto &w:previous.water_overrides)if(water::find(document_->waters,w->id))snapshot_.water_overrides.push_back(w);
          if(preserve||previous_document)for(const auto &v:previous.cloud_overrides)if(cloud::find(document_->clouds,v->id))snapshot_.cloud_overrides.push_back(v);
          if(preserve||previous_document){for(const auto &[id,patch]:previous.material_overrides)snapshot_.material_overrides[id]=patch;prune_material_overrides(document_->loaded.scene,snapshot_.material_overrides);}
          if(preserve||previous_document){for(const auto &[id,value]:previous.group_transforms)snapshot_.group_transforms[id]=value;prune_group_transforms(*document_,snapshot_.group_transforms);}
          if(preserve||previous_document){for(const auto &[id,value]:previous.node_properties)snapshot_.node_properties[id]=value;prune_node_properties(*document_,snapshot_.node_properties);}
          if(preserve||previous_document){for(const auto &[id,value]:previous.instance_ground)snapshot_.instance_ground[id]=value;prune_instance_ground(document_->loaded.scene,snapshot_.instance_ground);}
          if(preserve||previous_document){auto settings=std::move(snapshot_.parameter_settings);snapshot_.parameter_settings=previous.parameter_settings;if(previous_document)for(auto &entry:settings)if(!entry.first.starts_with("@"))snapshot_.parameter_settings[entry.first]=std::move(entry.second);auto incoming=std::move(snapshot_.control_favorites);snapshot_.control_favorites=previous.control_favorites;
            if(previous_document&&incoming){if(!snapshot_.control_favorites)snapshot_.control_favorites.emplace();for(auto &[node,values]:incoming->nodes)snapshot_.control_favorites->nodes[node]=std::move(values);}}
          if(previous_document) for(size_t l=0;l<previous.lights.size();++l) snapshot_.lights[l]=previous.lights[l];
          frame_pending_=false;pose_report_=nullptr;pose_status_->setText(QStringLiteral("单击姿势应用全身；单选骨骼后 Ctrl+左键双击姿势，仅应用该部位及其后代。形态 DUF 可双击应用。"));
          for(const auto &skin:document_->skeletons.skins) {
            auto pose=loaded_snapshot.poses.at(snapshot_.poses.size());
            if((preserve||previous_document)&&old) for(size_t s=0;s<old->skeletons.skins.size();++s) if(old->skeletons.skins[s].id==skin.id) {
              std::map<std::string,runtime::JointPose> by_id;for(size_t j=0;j<old->skeletons.skins[s].joints.size();++j) by_id[old->skeletons.skins[s].joints[j].id]=previous.poses[s][j];
              for(size_t j=0;j<skin.joints.size();++j) if(by_id.contains(skin.joints[j].id)) pose[j]=by_id.at(skin.joints[j].id);
            }
            snapshot_.poses.push_back(std::move(pose));
          }
          for(const auto &target:document_->catalog.targets) {
            auto values=loaded_snapshot.values.at(snapshot_.values.size());
            if((preserve||previous_document) && old) for(size_t t=0;t<old->catalog.targets.size();++t) if(old->catalog.targets[t].id==target.id) {
              values.favorites=previous.values[t].favorites;values.extension=previous.values[t].extension;values.unlimited_morphs=previous.values[t].unlimited_morphs;values.transform=previous.values[t].transform;values.visible=previous.values[t].visible;values.graft_enabled=previous.values[t].graft_enabled;values.ground_alignment_ratio=previous.values[t].ground_alignment_ratio;values.ground_alignment_offset_cm=previous.values[t].ground_alignment_offset_cm;values.ground_alignment_body_only=previous.values[t].ground_alignment_body_only;std::map<std::string,float> weights;
              for(size_t m=0;m<old->catalog.targets[t].morphs.size();++m) weights[old->catalog.targets[t].morphs[m].id]=previous.values[t].morphs[m];
              for(size_t m=0;m<target.morphs.size();++m) if((target.morphs[m].evaluable||target.morphs[m].unsupported.empty())&&weights.contains(target.morphs[m].id)) values.morphs[m]=weights[target.morphs[m].id];
            }
            runtime::sync_aliases(target,values);snapshot_.values.push_back(std::move(values));
          }
          rebuild_hierarchy();
          if(preserve||previous_document){auto incoming=snapshot_.pose_pins;snapshot_.pose_pins=pose_pins_;snapshot_.pose_pins.insert(snapshot_.pose_pins.end(),incoming.begin(),incoming.end());}
          pose_pins_=snapshot_.pose_pins;pins_generation_=document_->generation;renderer_->pose_pins(pose_pins_);
          renderer_->set_document(document_,submitted_snapshot(),!previous_document);select(-1);
          if(!previous_document&&snapshot_.view)renderer_->camera_view(*snapshot_.view);
          if(capture_view_) renderer_->camera_view(*capture_view_);
          const auto first=previous_document?previous_document->catalog.targets.size():0;
          if(first<document_->catalog.targets.size()) choose(int(first));else if(hierarchy_->topLevelItemCount()) hierarchy_->setCurrentItem(hierarchy_->topLevelItem(0));
          if(!self_test_&&!preserve) browser_->record_use(QString::fromStdWString((entry.empty()?file:entry).wstring()),category);
          if(!entry.empty()&&entry!=file) {
            pose_status_->setText(QStringLiteral("已挂接 HD Nipples 基础附件。皮肤纹理生成、碰撞优化和 dForce 配套脚本尚未支持；可应用已准备的材质预设。"));
          }
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
  parser.addOption({"history-test",QStringLiteral("副屏验证撤销、重做、场景替换和原生快捷键")});
  parser.addOption({"city-test",QStringLiteral("验证城市生成、LOD、共享材质、撤销和保存重开")});
  parser.addOption({"water-test",QStringLiteral("验证水体菜单、交界、静态时间、撤销和保存重开")});
  parser.addOption({"cloud-test",QStringLiteral("验证体积云菜单、静态时间、撤销和保存重开")});
  parser.addOption({"water-scene-test",QStringLiteral("在完整场景验证创建水体、近远切换、时间修改和撤销")});
  parser.addOption({"feedback-test",QStringLiteral("副屏验证分辨率切换、升采样和 UI 快捷键")});
  parser.addOption({"edit-regression-test",QStringLiteral("副屏验证多选聚焦、细分及 ERC 缩放")});
  parser.addOption({"joint-selection-test",QStringLiteral("副屏验证同角色左右指尖 Ctrl 多选及聚焦")});
  parser.addOption({"workflow-test",QStringLiteral("验证射线、部位 Morph 与视口缩放后退出")});
  parser.addOption({"head-selection-test",QStringLiteral("验证三级头部选择及绑定服装射线后退出")});
  parser.addOption({"options-test",QStringLiteral("验证环境 / 色调参数及 F / WASDQE 导航后退出")});
  parser.addOption({"navigation-test",QStringLiteral("验证移动预览、第一人称转头与静止恢复后退出")});
  parser.addOption({"interaction-test",QStringLiteral("副屏短操作：记录尺寸、停靠面板、相机及角色移动的延迟")});
  parser.addOption({"keep-open-after-test",QStringLiteral("导航验收完成后保留客户端供手动体验")});
  parser.addOption({"attachment-test",QStringLiteral("逐一验证 Head 附件的场景树父节点并截图")});
  parser.addOption({"capture-test",QStringLiteral("场景显示验证后截图退出")});
  parser.addOption({"render-profile",QStringLiteral("头部特写与材质消融实验计划 JSON"),"file"});
  parser.addOption({"render-profile-start-empty",QStringLiteral("性能诊断先渲染空场景，再按普通打开流程加载文件")});
  parser.addOption({"selection-test",QStringLiteral("验证指定标签或实例 ID 的射线点击与树选择，可重复"),"label"});
  parser.addOption({"focus-test",QStringLiteral("验证大型对象的 F 与侧键聚焦"),"label"});
  parser.addOption({"capture-target",QStringLiteral("截图时框选的对象标签，可重复"),"label"});
  parser.addOption({"capture-front",QStringLiteral("截图时从框选对象正面观察")});
  parser.addOption({"capture-view",QStringLiteral("截图固定视角 x,y,z,distance,yaw,pitch（米/弧度）"),"view"});
  parser.addOption({"capture-head",QStringLiteral("截图时框选角色头部")});
  parser.addOption({"capture-samples",QStringLiteral("截图前累积样本数"),"count","16"});
  parser.addOption({"capture-seconds",QStringLiteral("定时记录原始画面，再继续至目标样本或四倍时长"),"seconds","0"});
  parser.addOption({"sampling-settings",QStringLiteral("采样诊断配置 JSON；降噪始终禁用"),"file"});
  parser.addOption({"visibility-test",QStringLiteral("验证指定对象的场景树显隐开关"),"label"});
  parser.addOption({"lifecycle-test",QStringLiteral("验证反复增删与替换场景，指定第二个测试 DUF"),"file"});
  parser.addOption({"lifecycle-rounds",QStringLiteral("生命周期验证轮数"),"count","8"});
  parser.addOption({"scene-reopen-test",QStringLiteral("验证打开、新建空场景、从内容库再次打开指定场景"),"file"});
  parser.addOption({"wear-test",QStringLiteral("验证移动 / 摆姿势后自动穿戴、解除及重新绑定"),"file"});
  parser.addOption({"wear-material-test",QStringLiteral("穿戴验证后应用指定的材质 DUF / DJL 链接"),"file"});
  parser.addOption({"rebuild-test",QStringLiteral("副屏测量细分修改、自动穿戴和删除的全场景重建耗时"),"file"});
  parser.addOption({"subdivision-stress-test",QStringLiteral("副屏验证细分反复切换、快速输入及预算限制")});
  parser.addOption({"pose",QStringLiteral("加载角色后应用的单帧姿势 DUF"),"file"});
  parser.addOption({"pose-test",QStringLiteral("验证姿势、恢复与相机后自动退出"),"file"});
  parser.addOption({"pose-edit-test",QStringLiteral("副屏验证原生 FK 参数、左键 IK、选择门槛、预览和取消")});
  parser.addOption({"edit-resume-test",QStringLiteral("Verify consecutive PowerPose and Morph edits before beauty")});
  parser.addOption({"powerpose-test",QStringLiteral("副屏验证 PowerPose 三页、灰模、提交、取消和固定约束")});
  parser.addOption({"gizmo-scale-test",QStringLiteral("验证 Seo Hyun 角色 Scale=100000 后的 XYZ 平移和撤销重做")});
  parser.addOption({"gizmo-test",QStringLiteral("副屏验证三轴工具、Local / World、提交与取消")});
  parser.addOption({"group-transform-test",QStringLiteral("副屏验证 Group 参数、鼠标变换、保存与撤销")});
  parser.addOption({"node-properties-test",QStringLiteral("验证节点显隐、可选中、组及多选删除")});
  parser.addOption({"group-motion-test",QStringLiteral("逐个核对指定 Group 平移预览和提交后的世界矩阵"),"label"});
  parser.addOption({"collective-ground-test",QStringLiteral("验证指定 Group 和多选模型整体 Ctrl+D 与撤销重做"),"label"});
  parser.addOption({"ground-test",QStringLiteral("副屏验证普通对象与 Instance 的 Ctrl+D 地面对齐")});
  parser.addOption({"physics-ui-test",QStringLiteral("验证物理属性公共滚动、收起布局和模型筛选")});
  parser.addOption({"empty-test",QStringLiteral("验证默认空场景及移动缩放后的视口刷新")});
  parser.addOption({"extension-test",QStringLiteral("副屏验证生长、重量、密度、折叠与 DUFEX 保存重开")});
  parser.addOption({"weight-module-test",QStringLiteral("验证指定节点的角色生长／道具密度模块分配"),"label"});
  parser.addOption({"dufex-test",QStringLiteral("副屏验证 DUFEX v2、缩略图、观察相机及原 DUF 独立打开")});
  parser.addOption({"pose-test-level",QStringLiteral("FK / IK 验证时使用的宿主细分等级"),"level","-1"});
  parser.addOption({"formula-test",QStringLiteral("验证指定 Morph 滑块、ERC 与恢复后退出")});
  parser.addOption({"lazy-test",QStringLiteral("验证异步 Morph、手动应用和参数目录刷新后退出")});
  parser.addOption({"test-parameter",QStringLiteral("指定滑块验证参数，可重复，与 --formula-test 配合"),"name"});
  parser.addOption({"reload-test",QStringLiteral("验证后台场景替换后退出"),"file"});parser.process(app);
  const auto output=parser.isSet("output")?file_path(parser.value("output")):file_path(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))/"artifacts"/("editor-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz").toStdString());
  std::filesystem::create_directories(output);
  try {
    SamplingSettings sampling;
    sampling.rebuild_probe=parser.isSet("rebuild-test");
    sampling.interaction_probe=parser.isSet("interaction-test")||sampling.rebuild_probe||parser.isSet("subdivision-stress-test")||(parser.isSet("powerpose-test")||parser.isSet("edit-resume-test"))||(parser.isSet("gizmo-test")||parser.isSet("gizmo-scale-test"))||parser.isSet("group-transform-test")||parser.isSet("node-properties-test")||parser.isSet("group-motion-test")||parser.isSet("collective-ground-test")||parser.isSet("ground-test");
    if(parser.isSet("sampling-settings")) {sampling.quality_override=true;nlohmann::json j;std::ifstream(file_path(parser.value("sampling-settings")))>>j;
      sampling.samples=j.value("samples",sampling.samples);sampling.adaptive_threshold=j.value("adaptive_threshold",sampling.adaptive_threshold);sampling.blue_noise=j.value("blue_noise",sampling.blue_noise);
      sampling.min_bounces=j.value("min_bounces",sampling.min_bounces);sampling.transparent_min_bounces=j.value("transparent_min_bounces",sampling.transparent_min_bounces);
      sampling.prune_hidden=j.value("prune_hidden",sampling.prune_hidden);sampling.texture_limit=j.value("texture_limit",sampling.texture_limit);
      sampling.subsurface=j.value("subsurface",sampling.subsurface);sampling.bump_and_normal=j.value("bump_and_normal",sampling.bump_and_normal);
      sampling.transparent_bounces=j.value("transparent_bounces",sampling.transparent_bounces);
      if(sampling.transparent_bounces<1||sampling.transparent_bounces>32) throw std::runtime_error("透明层数上限无效");
      sampling.update_interval_seconds=j.value("update_interval_seconds",sampling.update_interval_seconds);
      if(!std::isfinite(sampling.update_interval_seconds)||sampling.update_interval_seconds<0||sampling.update_interval_seconds>2) throw std::runtime_error("显示更新间隔无效");
      if(sampling.texture_limit<0||sampling.texture_limit>16384) throw std::runtime_error("纹理诊断尺寸无效");
      if(sampling.samples<1||sampling.samples>(1<<20)||!std::isfinite(sampling.adaptive_threshold)||sampling.adaptive_threshold<0||sampling.adaptive_threshold>1||sampling.min_bounces<0||sampling.min_bounces>8||sampling.transparent_min_bounces<0||sampling.transparent_min_bounces>32) throw std::runtime_error("采样诊断参数无效");
    }
    bool capture_seconds_valid=false;const double capture_seconds=parser.value("capture-seconds").toDouble(&capture_seconds_valid);
    if(!capture_seconds_valid||!std::isfinite(capture_seconds)||capture_seconds<0||capture_seconds>180) throw std::runtime_error("截图诊断时长无效");
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);
    ccl::path_init(app.applicationDirPath().toStdString(),dfv::cycles_user_directory());
    QString project_file=parser.value("project");
    if(!parser.isSet("project")) {
      const auto legacy=QDir(app.applicationDirPath()).absoluteFilePath("../DazFastViewer.project.json");
      if(QFileInfo::exists(legacy)) project_file=legacy;
      else {
        const auto settings_dir=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if(!QDir().mkpath(settings_dir)) throw std::runtime_error("无法创建用户设置目录");
        project_file=QDir(settings_dir).filePath("DazFastViewer.project.json");
      }
    }
    auto project=ProjectSettings::load(project_file);
    project.content_roots=ProjectSettings::normalize(parser.values("content-root")+project.content_roots);
    Editor editor(output,std::move(project),parser.isSet("self-test")||parser.isSet("extension-test")||parser.isSet("weight-module-test")||parser.isSet("cloud-test")||parser.isSet("water-test")||parser.isSet("water-scene-test")||parser.isSet("city-test")||parser.isSet("physics-ui-test")||parser.isSet("render-profile")||(parser.isSet("gizmo-test")||parser.isSet("gizmo-scale-test"))||parser.isSet("group-transform-test")||parser.isSet("node-properties-test")||parser.isSet("group-motion-test")||parser.isSet("collective-ground-test")||(parser.isSet("powerpose-test")||parser.isSet("edit-resume-test"))||parser.isSet("rebuild-test")||parser.isSet("wear-test")||parser.isSet("reload-test")||parser.isSet("lifecycle-test")||parser.isSet("scene-reopen-test")||parser.isSet("lazy-test")||parser.isSet("interaction-test"),parser.isSet("reload-test")?file_path(parser.value("reload-test")):std::filesystem::path{},
      parser.isSet("pose-test")?file_path(parser.value("pose-test")):parser.isSet("pose")?file_path(parser.value("pose")):std::filesystem::path{},parser.isSet("pose-test"),parser.isSet("formula-test"),sampling);
    editor.test_parameters(parser.values("test-parameter"));
    if(parser.isSet("history-test"))editor.history_test();
    if(parser.isSet("city-test"))editor.city_test();
    if(parser.isSet("water-test"))editor.water_test();
    if(parser.isSet("cloud-test"))editor.cloud_test();
    if(parser.isSet("water-scene-test"))editor.water_scene_test();
    if(parser.isSet("edit-regression-test")) editor.edit_regression_test();
    if(parser.isSet("pose-edit-test")) editor.pose_edit_test(parser.value("pose-test-level").toInt());
    if((parser.isSet("powerpose-test")||parser.isSet("edit-resume-test"))) editor.powerpose_test();
    if(parser.isSet("edit-resume-test")) editor.edit_resume_test();
    if((parser.isSet("gizmo-test")||parser.isSet("gizmo-scale-test"))) editor.gizmo_test(parser.isSet("gizmo-scale-test"));
    if(parser.isSet("group-transform-test")) editor.group_transform_test();
    if(parser.isSet("node-properties-test")) editor.node_properties_test();
    if(parser.isSet("group-motion-test")) editor.group_motion_test(parser.value("group-motion-test").toStdString());
    if(parser.isSet("collective-ground-test")) editor.collective_ground_test(parser.value("collective-ground-test").toStdString());
    if(parser.isSet("extension-test")) editor.extension_test();
    if(parser.isSet("weight-module-test")) editor.weight_module_test(parser.value("weight-module-test").toStdString());
    if(parser.isSet("dufex-test")) editor.dufex_test();
    if(parser.isSet("feedback-test")) editor.feedback_test();
    if(parser.isSet("physics-ui-test"))editor.physics_ui_test();
    if(parser.isSet("empty-test")) editor.empty_test();
    if(parser.isSet("joint-selection-test")) editor.joint_selection_test();
    if(parser.isSet("options-test")) editor.options_test();
    if(parser.isSet("navigation-test")) editor.navigation_test();
    if(parser.isSet("interaction-test")) editor.interaction_test();
    if(parser.isSet("keep-open-after-test")) editor.keep_open_after_test();
    if(parser.isSet("attachment-test")) editor.attachment_test();
    if(parser.isSet("lazy-test")) editor.lazy_test();
    if(parser.isSet("workflow-test")) editor.workflow_test();
    if(parser.isSet("head-selection-test")) editor.head_selection_test();
    if(parser.isSet("capture-test")) editor.capture_test(parser.values("capture-target"),parser.isSet("capture-front"),parser.isSet("capture-head"),parser.value("capture-samples").toInt(),capture_seconds);
    if(parser.isSet("ground-test")) editor.ground_test();
    if(parser.isSet("render-profile")) editor.render_profile(file_path(parser.value("render-profile")));
    if(parser.isSet("selection-test")) editor.selection_test(parser.values("selection-test"));
    if(parser.isSet("focus-test")) editor.selection_test(parser.values("focus-test"),true);
    if(parser.isSet("capture-view")) editor.capture_view(parser.value("capture-view"));
    if(parser.isSet("visibility-test")) editor.visibility_test(parser.values("visibility-test"));
    if(parser.isSet("lifecycle-test")) editor.lifecycle_test(file_path(parser.value("file")),file_path(parser.value("lifecycle-test")),parser.value("lifecycle-rounds").toInt());
    if(parser.isSet("scene-reopen-test")) editor.scene_reopen_test(file_path(parser.value("scene-reopen-test")));
    if(parser.isSet("wear-test")) editor.wear_test(file_path(parser.value("wear-test")),parser.isSet("wear-material-test")?file_path(parser.value("wear-material-test")):std::filesystem::path{});
    if(parser.isSet("rebuild-test")) editor.rebuild_test(file_path(parser.value("rebuild-test")));
    if(parser.isSet("subdivision-stress-test")) editor.subdivision_stress_test();
    if(parser.isSet("render-profile-start-empty")) {
      if(!parser.isSet("render-profile")||!parser.isSet("file")) throw std::runtime_error("空场景启动诊断需要 --render-profile 和 --file");
      editor.render_profile_after_empty(file_path(parser.value("file")));
    }
    else if(editor.start_recovery()) {}
    else if(parser.isSet("file")) editor.load(file_path(parser.value("file")));
    else editor.empty_scene();
    return app.exec();
  } catch(const std::exception &e) {std::ofstream(output/"error.txt")<<e.what();return 1;}
}
