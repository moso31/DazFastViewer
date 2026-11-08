static void deferred_geometry_checks() {
  ir::Mesh body;body.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};body.triangles={{{0,1,2}},{{0,2,3}}};auto cloth=body;for(auto &p:cloth.positions)p.z=.05f;
  ir::Scene base;base.meshes={body,cloth,cloth};base.instances.resize(3);base.instances[1].mesh=1;base.instances[2].mesh=2;
  Target host;host.id="owner/mesh";host.morphs={morph("shape",{{0,{0,0,.2f}},{1,{0,0,.1f}}})};Target shirt;shirt.id="shirt/mesh";shirt.instance=1;shirt.conform_target="#owner";auto other=shirt;other.id="other/mesh";other.instance=2;
  std::vector<Target> targets={host,shirt,other};const std::vector<Skin> skins;std::vector<FormulaGraph> graphs={graph(host,-1),graph(shirt,-1),graph(other,-1)};
  std::vector<Properties> values(3);values[0].morphs={0};values[1].visible=values[2].visible=false;
  auto scene=base;DeformationRuntime runtime(scene,targets,skins,graphs);runtime.evaluate(values,{});const auto count=runtime.conform_stats().evaluations;
  values[0].morphs[0]=1;runtime.evaluate(values,{});require(runtime.conform_stats().evaluations==count,"无依赖的隐藏衣物仍在投射");
  values[1].visible=true;runtime.evaluate(values,{});auto reference=base;DeformationRuntime fresh(reference,targets,skins,graphs);fresh.evaluate(values,{});require(scene.meshes[1].positions==reference.meshes[1].positions,"重新显示衣物没有补算最新体型");
  values[1].visible=false;values[0].morphs[0]=.25f;runtime.evaluate(values,{});values[1].visible=true;runtime.evaluate(values,{});fresh.evaluate(values,{});require(scene.meshes[1].positions==reference.meshes[1].positions,"再次隐藏和显示积累了过期投射");
  values[1].visible=false;values[0].morphs[0]=.6f;runtime.evaluate(values,{});values[1].visible=true;values[1].transform.general_scale=0;rejects([&]{runtime.evaluate(values,{});},"重新显示时未验证无效变换");values[1].transform.general_scale=1;runtime.evaluate(values,{});fresh.evaluate(values,{});require(scene.meshes[1].positions==reference.meshes[1].positions,"重新显示失败后的重试没有补算附件");
  // 隐藏宿主供可见附件使用时不能延迟；共享实例也必须获得当前形体。
  auto dependencies=targets;dependencies[2].conform_target="#shirt";values[1].visible=false;values[2].visible=true;auto dependent_scene=base;DeformationRuntime dependent(dependent_scene,dependencies,skins,graphs);dependent.evaluate(values,{});require(dependent.conform_stats().evaluations==2,"可见附件的隐藏 Fit To 宿主未求值");
  auto shared=base;shared.instances[2].mesh=1;auto shared_targets=std::vector<Target>{host,shirt};auto shared_graphs=std::vector<FormulaGraph>{graphs[0],graphs[1]};auto shared_values=std::vector<Properties>{values[0],values[1]};DeformationRuntime instanced(shared,shared_targets,skins,shared_graphs);instanced.evaluate(shared_values,{});require(instanced.conform_stats().evaluations==1,"可见共享实例的隐藏源未求值");
  // 第一个附件完成、第二个附件平滑中取消，然后退回原输入。
  values[1].visible=values[2].visible=true;values[0].morphs[0]=0;auto cancel_scene=base;DeformationRuntime cancellable(cancel_scene,targets,skins,graphs);cancellable.evaluate(values,{});
  values[0].morphs[0]=1;int checks=0;bool cancelled=false;{EvaluationCancellation token([&]{return ++checks==30;});try{cancellable.evaluate(values,{});}catch(const EvaluationCancelled &){cancelled=true;}}
  require(cancelled,"没有在附件平滑过程中响应取消");values[0].morphs[0]=0;cancellable.evaluate(values,{});auto expected=base;DeformationRuntime complete(expected,targets,skins,graphs);complete.evaluate(values,{});
  for(size_t i=0;i<3;++i)require(cancel_scene.meshes[i].positions==expected.meshes[i].positions,"取消后退回原输入留下半成品缓存");
  values[0].morphs[0]=.7f;cancellable.evaluate(values,{});complete.evaluate(values,{});for(size_t i=0;i<3;++i)require(cancel_scene.meshes[i].positions==expected.meshes[i].positions,"取消后的新输入形体不一致");
  // 全场景存在碰撞，不代表无关对象的变化也需要后台修正。
  auto collision_targets=targets;collision_targets[1].conform_target.clear();collision_targets[1].smoothing.enabled=true;collision_targets[1].smoothing.collision_target="#owner";collision_targets[2].conform_target.clear();
  values[0].morphs[0]=0;auto preview_scene=base;DeformationRuntime preview(preview_scene,collision_targets,skins,graphs);preview.evaluate(values,{});preview.defer_collision();
  values[2].transform.translation_cm.x=10;auto delta=preview.evaluate(values,{});require(!preview.needs_refinement(delta),"无关对象变换触发整个场景的后台形变");
  values[0].morphs[0]=1;delta=preview.evaluate(values,{});require(preview.needs_refinement(delta),"碰撞宿主变化没有请求后台修正");
  values[1].visible=false;delta=preview.evaluate(values,{});require(!preview.needs_refinement(delta),"隐藏且无依赖的碰撞衣物仍请求后台修正");
  values[1].visible=true;delta=preview.evaluate(values,{});require(preview.needs_refinement(delta),"重新显示碰撞衣物没有请求完整修正");
}
