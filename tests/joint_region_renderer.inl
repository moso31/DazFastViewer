static int joint_region_renderer(QApplication &app) {
  const auto output=std::filesystem::absolute("artifacts/toe-selection/native");std::filesystem::create_directories(output);
  try {
    auto config=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();config->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(config);
    ccl::path_init(app.applicationDirPath().toStdString(),dfv::cycles_user_directory());
    auto d=std::make_shared<Document>();d->generation=1;auto &scene=d->loaded.scene;scene.materials.resize(1);
    runtime::Skin skin;skin.id="figure";
    auto bone=[&](const char *id,int parent) {runtime::Joint j;j.id=j.name=id;j.parent=parent;skin.joints.push_back(j);};
    bone("figure",-1);bone("lFoot",0);bone("lToe",1);bone("lBigToe",2);bone("lBigToe2",3);bone("lSmallToe1",2);bone("rToe",0);bone("rBigToe",6);
    skin.initial.resize(skin.joints.size());
    ir::Mesh mesh;mesh.id="mesh";mesh.material_slots={"Skin"};mesh.polygon_groups={"lToe","rToe"};
    auto patch=[&](float x,uint32_t group,uint32_t joint) {
      const auto v=uint32_t(mesh.positions.size());mesh.positions.insert(mesh.positions.end(),{{x,0,-.5f},{x+1,0,-.5f},{x+1,0,.5f},{x,0,.5f}});
      ir::Triangle a,b;a.vertices={v,v+1,v+2};b.vertices={v,v+2,v+3};a.polygon_group=b.polygon_group=group;mesh.triangles.insert(mesh.triangles.end(),{a,b});
      for(int i=0;i<4;++i)skin.weights.push_back({{joint,1}});
    };
    patch(4,0,4);patch(6,0,5);patch(8,1,7);scene.meshes={mesh};
    ir::Instance instance;instance.id="figure/mesh";instance.materials={0};instance.transform=ir::Transform::translate({10,2,1});scene.instances={instance};
    runtime::Target target;target.id=instance.id;d->catalog.targets={target};d->skeletons.skins={skin};d->formulas.graphs.resize(1);d->formulas.graphs[0].skin=0;
    auto snapshot=initial_snapshot(*d);QScreen *secondary=QGuiApplication::primaryScreen();for(auto *s:QGuiApplication::screens())if(s!=QGuiApplication::primaryScreen())secondary=s;
    QWidget host;host.resize(700,520);host.move(secondary->availableGeometry().topLeft()+QPoint(40,40));host.show();app.processEvents();
    SamplingSettings settings;settings.samples=8;settings.adaptive_threshold=0;
    Renderer renderer(reinterpret_cast<HWND>(host.winId()),host.width(),host.height(),output,settings);renderer.resize(host.width(),host.height());renderer.automated_pointer();
    renderer.set_document(d,snapshot,false);renderer.camera_view({16.5f,2,1,12,0,0});renderer.select(1,-1,-1,{},false);
    auto wait=[&](auto predicate,const char *why,double seconds=15) {
      const auto start=now();while(now()-start<seconds) {app.processEvents();const auto s=renderer.status();check(s.error.empty(),s.error.c_str());check(s.edit_error.empty(),s.edit_error.c_str());if(predicate(s))return s;QThread::msleep(1);}throw std::runtime_error(why);
    };
    wait([](const auto &s){return s.generation==1&&s.samples>=4&&!s.preview;},"细选夹具未呈现",90);
    auto project=[&](ir::Vec3 p) {
      const auto state=renderer.status();const auto m=state.camera.matrix();p={p.x-m[3],p.y-m[7],p.z-m[11]};const float z=m[2]*p.x+m[6]*p.y+m[10]*p.z,e=std::tan(.4f);
      return QPoint(qRound((1+(m[0]*p.x+m[4]*p.y+m[8]*p.z)/z/e/std::max(1.f,float(state.width)/state.height))*state.width*.5f-.5f),qRound((1-(m[1]*p.x+m[5]*p.y+m[9]*p.z)/z/e/std::max(1.f,float(state.height)/state.width))*state.height*.5f-.5f));
    };
    auto select=[&](int joint) {renderer.select(1,0,joint,{{0,joint,-1}},false);return wait([&](const auto &s){return s.selected_target==0&&s.selected_joint==joint;},"视口没有应用选择");};
    auto click=[&](ir::Vec3 p,int joint,size_t faces,bool toggle=false) {
      const auto point=project(p);renderer.pointer(point.x(),point.y(),false,toggle);
      wait([&](const auto &s){return s.pointer_x==point.x()&&s.pointer_y==point.y()&&s.hovered_joint==joint&&s.hovered_triangles==faces;},"悬停层级或网格高亮错误");
      const auto before=renderer.status().clicks;renderer.pointer(point.x(),point.y(),true,toggle);
      const auto hit=wait([&](const auto &s){return s.clicks>before;},"原生鼠标点击未处理");check(hit.hit_target==0&&hit.hit_joint==joint,"原生点击的骨骼层级错误");return hit;
    };
    click({14.5f,2,1},-1,6);select(-1);click({14.5f,2,1},2,4);select(2);click({14.5f,2,1},4,2);select(4);
    click({16.5f,2,1},5,2);select(5);click({18.5f,2,1},6,2);select(6);click({18.5f,2,1},7,2);select(7);
    renderer.select(1,0,7,{{0,7,-1},{0,4,-1}},false);wait([](const auto &s){return s.selections.size()==2;},"多选未应用");
    click({16.5f,2,1},5,2,true);renderer.pointer(-1,-1);select(3);
    const auto hwnd=FindWindowExW(HWND(host.winId()),nullptr,L"DfvCyclesBench",nullptr);check(hwnd,"缺少原生视口窗口");
    nlohmann::json focuses=nlohmann::json::array();
    for(int joint:{2,3,4,5,7}) {
      const auto s=select(joint);check(!s.selection_bounds.empty,"脚趾没有网格聚焦范围");
      SendMessageW(hwnd,WM_KEYDOWN,'F',0);SendMessageW(hwnd,WM_KEYUP,'F',0);
      const auto requested=wait([&](const auto &v){return v.focus_requests>s.focus_requests;},"原生 F 键未请求聚焦");
      renderer.focus(requested.selection_bounds);
      const auto focused=wait([&](const auto &v){return v.camera.epoch>requested.camera.epoch;},"F 聚焦未更新相机");
      const auto center=requested.selection_bounds.center();
      check(std::abs(focused.camera.target.x-center.x)+std::abs(focused.camera.target.y-center.y)+std::abs(focused.camera.target.z-center.z)<1e-5,"F 未指向网格面的世界中心");
      check(focused.camera.distance>1&&center.x>14,"F 仍聚焦骨骼中心的小范围");
      focuses.push_back({{"joint",skin.joints[joint].id},{"target",{center.x,center.y,center.z}},{"distance",focused.camera.distance}});
    }
    std::ofstream(output/"result.json")<<nlohmann::json{{"status","PASS"},{"clicks",renderer.status().clicks},{"focus",focuses}}.dump(2);
    std::cout<<"Native joint selection / highlight / mesh focus: PASS\n";return 0;
  } catch(const std::exception &e) {std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}
}
