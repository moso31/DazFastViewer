// 真实副屏检查：独立渲染与界面参数，以及原生视口不再响应旧缩放快捷键。
bool feedback_test_=false;
int feedback_stage_=0;
double feedback_at_=0;
double feedback_status_at_=0;
double feedback_ui_until_=0;
uint64_t feedback_sessions_=0,feedback_geometry_=0,feedback_epoch_=0;
nlohmann::json feedback_checks_=nlohmann::json::array();
void feedback_tick(const RenderStatus &state) {
  if(!feedback_at_)feedback_at_=now();
  if(now()-feedback_status_at_>.5){feedback_status_at_=now();std::ofstream(output_/"feedback-state.json")<<nlohmann::json{{"stage",feedback_stage_},{"ui",ui_scale_->percent()},{"seconds",now()-feedback_at_},{"viewport",{state.width,state.height}},{"render",{state.render_width,state.render_height}},{"epoch",state.requested_epoch},{"presented",state.presented_epoch},{"preview",state.preview},{"samples",state.samples}}.dump(2);}
  if(now()-feedback_at_>240){finish_test(false,"视口分辨率或界面缩放检查超时，阶段 "+std::to_string(feedback_stage_)+"，UI "+std::to_string(ui_scale_->percent()));return;}
  if(!state.error.empty()||!state.edit_error.empty()){finish_test(false,state.error+state.edit_error);return;}
  if(now()<feedback_ui_until_)return;
  if(loading_||!document_||state.generation!=document_->generation||state.presented_revision!=snapshot_.revision||state.presented_epoch!=state.requested_epoch||state.preview||state.samples<8)return;
  auto save=[&](const char *name){screen()->grabWindow(winId()).save(QString::fromStdWString((output_/(std::string(name)+".png")).wstring()));
    feedback_checks_.push_back({{"case",name},{"ui_percent",ui_scale_->percent()},{"render_percent",state.quality.percent},{"viewport",{state.width,state.height}},{"sampled",{state.render_width,state.render_height}},{"epoch",state.requested_epoch},{"sessions",state.sessions}});
    std::ofstream(output_/"feedback-checks.json")<<feedback_checks_.dump(2);
    nlohmann::json layout;for(auto *widget:{static_cast<QWidget *>(chrome),static_cast<QWidget *>(chrome->menus())})layout[widget->objectName().toStdString()]={{"height",widget->height()},{"minimum",widget->minimumHeight()},{"hint",widget->sizeHint().height()},{"font",widget->font().pointSizeF()}};
    std::ofstream(output_/(std::string(name)+"-layout.json"))<<layout.dump(2);
  };
  const auto expected=render_size(state.width,state.height,viewport_settings_->value().percent);
  if(expected!=std::pair{state.render_width,state.render_height}||state.quality!=viewport_settings_->value())return;
  auto percent=[&](int value){auto quality=viewport_settings_->value();quality.percent=value;viewport_settings_->set(quality);++feedback_stage_;};
  if(feedback_stage_==0){feedback_sessions_=state.sessions;feedback_geometry_=state.adapter.geometry_updates;save("render-100");percent(50);return;}
  if(state.sessions!=feedback_sessions_||state.adapter.geometry_updates!=feedback_geometry_){finish_test(false,"渲染比例或 UI 缩放导致会话或几何重建");return;}
  if(feedback_stage_==1){save("render-50");percent(67);return;}
  if(feedback_stage_==2){save("render-67");percent(75);return;}
  if(feedback_stage_==3){save("render-75");feedback_epoch_=state.requested_epoch;auto quality=viewport_settings_->value();quality.reconstruction=Reconstruction::bilinear;viewport_settings_->set(quality);++feedback_stage_;return;}
  if(feedback_stage_==4){if(state.requested_epoch!=feedback_epoch_){finish_test(false,"切换升采样算法丢弃了累计样本");return;}save("render-75-bilinear");percent(100);return;}
  if(feedback_stage_==5){save("render-restored-100");
    if(findChild<QMenu *>("ApplicationSettingsMenu")){finish_test(false,"设置功能尚未迁移到项目选项卡");return;}
    ui_scale_->set_percent(150);feedback_ui_until_=now()+.3;++feedback_stage_;return;}
  if(feedback_stage_==6){save("ui-150");ui_scale_->set_percent(130);feedback_ui_until_=now()+.3;++feedback_stage_;return;}
  auto shortcut=[&](WORD key){
    const HWND viewport=FindWindowExW(reinterpret_cast<HWND>(host_->winId()),nullptr,L"DfvCyclesBench",nullptr);
    if(!viewport)throw std::runtime_error("找不到原生视口用于快捷键检查");
    // 仅投递到本进程副屏视口；不把测试键盘事件发送到用户当前的前台应用。
    std::array<BYTE,256> previous{},pressed{};GetKeyboardState(previous.data());pressed=previous;pressed[VK_CONTROL]=0x80;SetKeyboardState(pressed.data());
    PostMessageW(viewport,WM_KEYDOWN,key,1);PostMessageW(viewport,WM_KEYUP,key,1LL<<31);
    QTimer::singleShot(100,this,[previous]() mutable {SetKeyboardState(previous.data());});
    feedback_ui_until_=now()+.3;
  };
  if(feedback_stage_==7){shortcut(VK_OEM_PLUS);++feedback_stage_;return;}
  if(feedback_stage_>=8&&feedback_stage_<=10&&ui_scale_->percent()!=130){finish_test(false,"原生视口仍响应旧 UI 缩放快捷键");return;}
  if(feedback_stage_==8){save("native-ctrl-plus-ignored");shortcut(VK_OEM_MINUS);++feedback_stage_;return;}
  if(feedback_stage_==9){save("native-ctrl-minus-ignored");shortcut('0');++feedback_stage_;return;}
  if(feedback_stage_==10){save("native-ctrl-0-ignored");ui_scale_->set_percent(100);feedback_ui_until_=now()+.3;++feedback_stage_;return;}
  if(feedback_stage_==11){if(ui_scale_->percent()!=100){finish_test(false,"界面恢复 100% 失败");return;}save("ui-reset-100");finish_test(true);}
}
