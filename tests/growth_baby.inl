static void baby_growth(){
  using namespace runtime;
  auto d=growth_document();auto &t=d.catalog.targets[0];auto &g=d.formulas.graphs[0];
  const auto root=g.expressions[0].output;
  auto morph=[&](const char *name){Morph m;m.id=m.label=name;m.evaluable=true;m.clamped=true;const auto index=uint32_t(t.morphs.size());t.morphs.push_back(m);Channel c;c.binding.index=index;c.clamped=true;g.morph_channels.push_back(int(g.channels.size()));g.channels.push_back(c);return index;};
  const auto baby=morph("Genesis 8 Baby"),scale=morph("G8Baby_Scale"),shape=morph("G8Baby_Shape");
  auto link=[&](uint32_t owner,uint32_t input,uint32_t output,double coefficient){Expression e;e.owner=owner;e.output=output;e.linear_input=g.morph_channels[input];e.coefficient=coefficient;g.expressions.push_back(e);};
  link(baby,baby,uint32_t(g.morph_channels[scale]),1);link(baby,baby,uint32_t(g.morph_channels[shape]),1);link(scale,scale,root,-.75);
  t.morphs[shape].offsets.push_back({6,{0,0,.1f}});
  g.prepare();d.skeletons.skins[0].root_general_scale=.4f;d.skeletons.skins[0].weights.resize(8,{{0,1}});
  auto s=editor::initial_snapshot(d);s.values[0].extension.kind=ExtensionKind::growth;
  auto scene=d.loaded.scene;DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  auto percent=[&](){return decimal_derived_float(runtime.resolve_poses(s.values,s.poses)[0][0].general_scale*d.skeletons.skins[0].root_general_scale)*s.values[0].transform.general_scale*100;};
  near(percent(),39,1e-5,"初始根缩放错误");
  editor::edit_growth(d,s,0,s.values[0].extension);
  require(!s.values[0].extension.baby_enabled&&!s.values[0].extension.baby_scale_isolated&&s.values[0].morphs[baby]==0,"默认生长启用了婴儿期");
  // 禁用且未接管时，保留用户手工设定的 Baby 及其原公式。
  auto manual=s;manual.values[0].morphs[baby]=.2f;editor::edit_growth(d,manual,0,manual.values[0].extension);
  require(manual.values[0].morphs[baby]==.2f&&!manual.values[0].extension.baby_scale_isolated,"默认禁用状态改写了手工 Baby");
  s.values[0].extension.baby_enabled=true;
  editor::edit_growth(d,s,0,s.values[0].extension);const auto original=s.values[0];
  for(double end:{3.,5.,7.}){
    for(double age:{0.,.25,1.,2.,3.,5.,7.,20.,0.,3.}){
      auto next=s.values[0].extension;next.baby_end=end;next.age=age;
      const auto missing=editor::edit_growth(d,s,0,next);require(missing.empty(),"完整 Baby 夹具报缺失参数");
      const double w=.6*std::max(0.,1-age/end);near(s.values[0].morphs[baby],w,1e-6,"Baby remap 权重错误");
      near(percent(),39+(age-3)*5,2e-5,"Baby ERC 与生长缩放重复叠加");
      runtime.evaluate(s.values,s.poses);near(runtime.effective()[0][shape],w,1e-6,"Baby 子形态联动丢失");
      for(auto p:scene.meshes[0].positions)require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"婴儿期顶点无效");
      for(size_t m=0;m<19;++m)near(s.values[0].morphs[m],growth_values(age,.5)[m].value*(1-w),1e-6,"旧生长 Morph 混合权重错误");
      const auto weight=editor::measure_object(d,s,0);require(std::isfinite(weight.height_cm)&&weight.height_cm>0&&std::isfinite(weight.kg)&&weight.kg>0,"婴儿期身高体重无效");
    }
  }
  for(double start:{0.,.35,.8,1.})for(double age:{0.,1.5,3.}){
    auto next=original.extension;next.baby_end=3;next.baby_strength=start;next.age=age;editor::edit_growth(d,s,0,next);
    const double w=start*(1-age/3);near(s.values[0].morphs[baby],w,1e-6,"自定义 Baby 起始强度未参与衰减");
    for(size_t m=0;m<19;++m)near(s.values[0].morphs[m],growth_values(age,.5)[m].value*(1-w),1e-6,"自定义起始强度未同步混合权重");
    near(percent(),39+(age-3)*5,2e-5,"改变起始强度联动改变了实际 Scale");
  }
  auto next=original.extension;next.age=0;next.baby_blend=false;editor::edit_growth(d,s,0,next);
  for(size_t m=0;m<19;++m)near(s.values[0].morphs[m],growth_values(0,.5)[m].value,1e-6,"关闭混合未恢复叠加模式");
  auto direct=s;direct.values[0].extension.baby_scale_isolated=false;
  require(runtime.resolve_poses(direct.values,direct.poses)[0][0].general_scale<0,"夹具未覆盖 Baby 导致负根缩放的情况");
  require(runtime.resolve_poses(s.values,s.poses)[0][0].general_scale>0,"切回隔离状态未更新公式缓存");
  // 开关只改变形体，不改变实际 Scale；关闭后仍保留配置与安全隔离。
  next.baby_blend=true;next.baby_begin=.5;next.baby_end=5;next.baby_strength=.65;editor::edit_growth(d,s,0,next);const auto enabled=s.values[0];
  next.baby_enabled=false;editor::edit_growth(d,s,0,next);
  require(s.values[0].morphs[baby]==0&&s.values[0].extension.baby_scale_isolated&&growth_baby_weight(next)==0,"禁用婴儿期未清零 Baby 或丢失隔离");
  for(size_t m=0;m<19;++m)near(s.values[0].morphs[m],growth_values(0,.5)[m].value,1e-6,"禁用婴儿期未恢复原生长形变");
  near(percent(),24,2e-5,"禁用婴儿期改变了实际 Scale");
  const auto disabled_weight=editor::measure_object(d,s,0);require(disabled_weight.height_cm>0&&std::isfinite(disabled_weight.kg),"禁用婴儿期后测量无效");
  next=s.values[0].extension;next.baby_enabled=true;editor::edit_growth(d,s,0,next);
  require(s.values[0].extension==enabled.extension&&s.values[0].morphs==enabled.morphs,"重新启用未恢复原配置与形体");near(percent(),24,2e-5,"重新启用婴儿期改变了实际 Scale");
  for(int i=0;i<50;++i){editor::edit_growth(d,s,0,original.extension);auto zero=original.extension;zero.age=0;editor::edit_growth(d,s,0,zero);}
  editor::edit_growth(d,s,0,original.extension);near(s.values[0].transform.general_scale,original.transform.general_scale,1e-9,"婴儿期往返缩放漂移");require(s.values[0].morphs==original.morphs,"婴儿期往返形体未恢复");
  for(int mode=0;mode<7;++mode){const auto before=s;auto invalid=original.extension;
    if(mode==0)invalid.baby_end=invalid.baby_begin;else if(mode==1)invalid.baby_begin=NAN;else if(mode==2){invalid.age=0;invalid.sensitivity=1000;}else if(mode==3)invalid.baby_strength=NAN;else if(mode==4)invalid.baby_strength=-.1;else if(mode==5)invalid.baby_strength=1.1;else{invalid.age=0;g.expressions[0].code={{Op::constant,-1}};g.prepare();}
    bool rejected=false;try{editor::edit_growth(d,s,0,invalid);}catch(...){rejected=true;}require(rejected&&s.values==before.values&&s.poses==before.poses,"非法婴儿期编辑没有原子拒绝");
  }
  auto no_baby=growth_document();auto fallback=editor::initial_snapshot(no_baby);auto zero=fallback.values[0].extension;zero.kind=ExtensionKind::growth;zero.age=0;
  require(editor::edit_growth(no_baby,fallback,0,zero).empty(),"默认禁用状态仍提示缺少 Baby");zero.baby_enabled=true;
  const auto missing=editor::edit_growth(no_baby,fallback,0,zero);require(missing==std::vector<std::string>{"Genesis 8 Baby"},"缺少 Baby 时未明确提示");
  for(size_t m=0;m<19;++m)near(fallback.values[0].morphs[m],growth_values(0,.5)[m].value,1e-6,"缺少 Baby 仍衰减了旧生长形体");
}
