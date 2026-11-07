static void real_growth_curve(const fs::path &file,const fs::path &output){
  auto d=load(fs::absolute(file));auto s=editor::initial_snapshot(d);auto scene=d.loaded.scene;
  runtime::DeformationRuntime deformation(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);J report=J::array();size_t count=0;
  for(size_t t=0;t<d.catalog.targets.size();++t){if(!editor::growth_character(d,t))continue;++count;
    const auto &target=d.catalog.targets[t];s.values[t].extension.kind=runtime::ExtensionKind::growth;const auto origin=s.values[t].transform.translation_cm;
    for(double adult:{8.,25.})for(bool baby:{false,true})for(auto mode:{runtime::GrowthMinimumMode::fit,runtime::GrowthMinimumMode::additive}){
      auto next=s.values[t].extension;next.curve.enabled=true;next.curve.minimum_growth=.75;next.curve.minimum_mode=mode;runtime::apply_growth_template(next.curve,int(t%3));runtime::resize_growth_curve(next.curve,adult);next.curve.puberty_weight=2.5;next.curve.height_in_cm=true;next.curve.growth_in_cm=true;runtime::move_growth_peak(next.curve,13.375);const auto shape=next.curve.shape;runtime::fit_growth_curve(next.curve);require(next.curve.shape==shape,"真实角色拟合覆盖自定义形状");next.baby_enabled=baby;
      double adult_height=0,adult_weight=0;
      for(double age:{0.,.5,1.,5.,adult*.743,adult,30.,50.,adult,0.}){
        next.age=age;const auto before=s.values;const auto missing=editor::edit_growth(d,s,t,next);
        for(size_t other=0;other<s.values.size();++other)if(other!=t)require(s.values[other]==before[other],"真实曲线编辑影响其他角色");
        require(s.values[t].transform.translation_cm==origin,"真实曲线修改角色位置");deformation.evaluate(s.values,s.poses);
        const auto skin=size_t(d.formulas.graphs[t].skin);const double actual=deformation.effective_poses()[skin][0].general_scale*d.skeletons.skins[skin].root_general_scale*s.values[t].transform.general_scale*100;
        const double expected=runtime::growth_curve_scale(next.curve,age);near(actual,expected,2e-4,"真实角色 ERC 缩放未补偿到曲线目标");
        for(const auto &poses:deformation.effective_poses())for(const auto &p:poses)require(std::isfinite(p.general_scale)&&p.general_scale>0&&p.scale.x>0&&p.scale.y>0&&p.scale.z>0,"真实曲线生成无效骨骼缩放");
        const auto measured=editor::measure_object(d,s,t);require(std::isfinite(measured.height_cm)&&measured.height_cm>0&&std::isfinite(measured.kg)&&measured.kg>0,"真实曲线身高体重无效");
        if(age==adult){adult_height=measured.height_cm;adult_weight=measured.kg;}if(age>adult){near(measured.height_cm,adult_height,1e-6,"成年后身高继续变化");near(measured.kg,adult_weight,1e-6,"成年后体重继续变化");}
        report.push_back({{"target",target.id},{"age",age},{"adult_age",adult},{"puberty_weight",next.curve.puberty_weight},{"baby",baby},{"mode",int(mode)},{"scale",actual},{"expected",expected},{"height_cm",measured.height_cm},{"kg",measured.kg},{"missing",missing}});
      }
    }
  }
  require(count>0,"真实曲线测试没有找到角色");fs::create_directories(output);const auto saved=output/"growth-curve.dufex";editor::save_scene_extension(saved,d,s);
  auto restored=editor::load_scene_extension(saved,{"H:/g1","H:/g3","C:/Users/Public/Documents/My DAZ 3D Library"},2);
  require(editor::snapshot_json(*restored.document,restored.snapshot)==editor::snapshot_json(d,s),"真实曲线保存重开不一致");
  std::ofstream(output/"probe.json")<<report.dump(2);std::cout<<"PASS real growth curve: "<<count<<" figures, "<<report.size()<<" edits, save/reopen\n";
}
