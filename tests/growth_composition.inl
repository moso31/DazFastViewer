static double composition_value(double age,double strength,double fatness,const char *label){
  for(const auto &v:runtime::growth_values(age,strength,fatness))if(v.label==label)return v.value;
  throw std::runtime_error("缺少体型测试通道");
}
static void growth_composition(){
  using namespace runtime;
  for(double age:{0.,3.,8.,10.5,12.5,18.,20.,50.}){
    const auto old=growth_values(age,.5),neutral=growth_values(age,.5,.5);
    require(old.size()==neutral.size(),"中性形体参数数量变化");
    const auto frame=growth_values(age,1,.5),fat=growth_values(age,.5,1);
    for(size_t i=0;i<old.size();++i){
      require(old[i].label==neutral[i].label&&old[i].value==neutral[i].value,"中性形体未精确保留旧表中档");
      require(frame[i].value==neutral[i].value||fat[i].value==neutral[i].value,"体格和肥胖共同驱动了同一通道");
      if(old[i].label=="Breasts Size"||old[i].label=="Youth Morph")require(frame[i].value==neutral[i].value&&fat[i].value==neutral[i].value,"体型轴改动了年龄成熟度");
    }
  }
  near(composition_value(18,1,.5,"Body Size"),.25,1e-12,"增加体格错误地增加了体脂");
  near(composition_value(18,1,.5,"GHD Width"),.5,1e-12,"成年骨架宽度错误");
  near(composition_value(18,.5,1,"Body Size"),.5,1e-12,"成年肥胖身体丰满度错误");
  near(composition_value(18,.5,1,"BGM Big Girl Pear Shape"),.5,1e-12,"成年臀腿丰满度错误");
  near(composition_value(18,.5,1,"Weight"),.5,1e-12,"成年肥胖未补足正向 Weight");
  near(composition_value(3,.5,0,"Weight"),-.65,1e-12,"幼年偏瘦没有保留年龄基线");
  near(composition_value(3,.5,1,"Weight"),.35,1e-12,"幼年偏胖没有保留年龄基线");
  near(composition_value(18,.5,1.5,"Weight"),1,1e-12,"肥胖被截断到旧建议范围");
  near(composition_value(18,.5,-.25,"Weight"),-.75,1e-12,"负肥胖被截断");
  rejects([]{growth_values(18,.5,NAN);},"无效肥胖未拒绝");
  rejects([]{growth_values(18,INFINITY,.5);},"无效体格未拒绝");
  auto d=growth_document();auto state=editor::initial_snapshot(d);auto &v=state.values[0];v.extension.kind=ExtensionKind::growth;
  auto next=v.extension;next.age=18;next.strength=.25;next.fatness=1.25;editor::edit_growth(d,state,0,next);const auto expected=v;
  for(int i=0;i<30;++i){next.fatness=-.25;editor::edit_growth(d,state,0,next);next.strength=1;editor::edit_growth(d,state,0,next);editor::edit_growth(d,state,0,expected.extension);next=expected.extension;}
  require(v.morphs==expected.morphs&&v.extension==expected.extension,"反复体型编辑未恢复形体");
  near(v.transform.general_scale,expected.transform.general_scale,1e-12,"肥胖往返造成缩放漂移");
  const auto before=state;next.fatness=NAN;rejects([&]{editor::edit_growth(d,state,0,next);},"非法肥胖编辑未拒绝");require(state.values==before.values,"非法肥胖留下部分修改");
  auto index=[&](const char *label){const auto &m=d.catalog.targets[0].morphs;return size_t(std::find_if(m.begin(),m.end(),[&](const auto &x){return x.label==label;})-m.begin());};
  const auto weight=index("Weight"),body=index("Body Size");auto &target=d.catalog.targets[0];
  target.morphs[weight].clamped=true;target.morphs[weight].maximum=.4f;target.morphs[body].locked=true;
  v.extension.fatness=1;const auto original_body=v.morphs[body];const auto missing=apply_growth(target,v);
  near(v.morphs[weight],.4,1e-7,"肥胖写入未遵守资产限位");require(v.morphs[body]==original_body&&missing==std::vector<std::string>{"Body Size"},"锁定通道被改写或未提示");
  target.morphs[weight].clamped=false;target.morphs[body].locked=false;
  Morph baby;baby.id=baby.label="Genesis 8 Baby";baby.evaluable=true;target.morphs.push_back(baby);v.morphs.push_back(0);
  v.extension.age=3;v.extension.baby_enabled=true;v.extension.baby_end=6;v.extension.baby_strength=.6;apply_growth(target,v);
  near(v.morphs.back(),.3,1e-7,"肥胖改变 Baby 权重");near(v.morphs[weight],.35*.7,1e-7,"肥胖未参与 Baby 混合");
  v.extension.baby_blend=false;apply_growth(target,v);near(v.morphs[weight],.35,1e-7,"关闭 Baby 混合后肥胖仍被衰减");
  v.extension.curve.enabled=true;v.extension.curve.planned_age=8;v.extension.age=8;apply_growth(target,v);const auto capped=v.morphs;
  v.extension.age=50;apply_growth(target,v);require(v.morphs==capped,"规划年龄以后肥胖或年龄形体仍变化");
}

// 对照已验收预览的独立报告，而不是再调用被测算法计算期望值。
static void real_growth_composition(const fs::path &file,const fs::path &output,const fs::path &preview_report){
  J golden;std::ifstream(preview_report)>>golden;auto d=load(fs::absolute(file));auto original=editor::initial_snapshot(d);
  const auto id=golden.at("target").get<std::string>();const auto found=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.id==id;});
  require(found!=d.catalog.targets.end(),"实际场景缺少预览角色");const auto t=size_t(found-d.catalog.targets.begin());
  auto scene=d.loaded.scene;runtime::DeformationRuntime deformation(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  J results=J::array();fs::create_directories(output);auto saved=original;
  for(const auto &test:golden.at("cases")){
    if(test.at("legacy").get<bool>())continue;auto s=original;auto next=s.values[t].extension;next.kind=runtime::ExtensionKind::growth;
    next.age=test.at("age");next.strength=test.at("frame");next.fatness=test.at("fat");
    const auto missing=editor::edit_growth(d,s,t,next);require(missing.empty(),"实际场景缺少可用的体型 Morph");
    for(size_t other=0;other<s.values.size();++other)if(other!=t)require(s.values[other]==original.values[other],"体型编辑串改其他对象");
    for(const auto &[label,expected]:test.at("morphs").items()){
      int best=-1;size_t control=found->morphs.size();
      for(size_t m=0;m<found->morphs.size();++m){const auto &c=found->morphs[m];if(c.alias_morph>=0||c.label!=label)continue;const int rank=(c.scene_channel?4:0)+(c.owner.empty()||c.owner.find('/')==std::string::npos?2:0);if(rank>best){best=rank;control=m;}}
      require(control<found->morphs.size(),"预览对照通道不存在");near(s.values[t].morphs.at(control),expected.get<double>(),1e-7,"正式版 Morph 与验收预览不同");
    }
    deformation.evaluate(s.values,s.poses);
    for(const auto &mesh:scene.meshes)for(auto p:mesh.positions)require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"体型编辑生成无效顶点");
    for(const auto &poses:deformation.effective_poses())for(const auto &p:poses)require(std::isfinite(p.general_scale)&&p.general_scale>0,"体型编辑生成无效骨骼缩放");
    results.push_back({{"age",next.age},{"strength",next.strength},{"fatness",next.fatness},{"preview_morphs_equal",true},{"missing",missing}});
    if(next.age==18&&next.strength==.5&&next.fatness==1)saved=s;
  }
  require(!results.empty()&&saved.values[t].extension.fatness==1,"未覆盖已验收的成年肥胖预览");
  const auto path=output/"test8-fatness.dufex";editor::save_scene_extension(path,d,saved);
  auto reopened=editor::load_scene_extension(path,{"H:/g1","H:/g3","C:/Users/Public/Documents/My DAZ 3D Library"},2);
  require(editor::snapshot_json(*reopened.document,reopened.snapshot)==editor::snapshot_json(d,saved),"正式肥胖场景保存重开不一致");
  auto legacy=editor::snapshot_json(d,saved);for(auto &object:legacy["objects"])object["extension"].erase("fatness");auto migrated=original;editor::apply_snapshot_json(d,migrated,legacy);
  require(migrated.values[t].extension.fatness==.5&&migrated.values[t].morphs==saved.values[t].morphs,"旧存档默认肥胖错误或加载时改写形体");
  std::ofstream(output/"probe.json")<<J{{"source",file.generic_string()},{"cases",results},{"save_reopen",true},{"legacy_preserves_morphs",true}}.dump(2);
  std::cout<<"PASS real composition: "<<results.size()<<" preview comparisons, finite geometry, save/reopen, legacy\n";
}
