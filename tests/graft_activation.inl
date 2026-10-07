#include "runtime/deformation.h"
#include "runtime/picking.h"
#include "editor/scene_extension.h"
#include "editor/render_edit_queue.h"

// 只读核对真实 DUFEX：角色本体能找到附件开关，停用恢复其遮罩面，启用恢复原状态。
static void actual_graft_activation(const char *file,const char *project_file,const char *label) {
  namespace fs=std::filesystem;using J=nlohmann::json;
  J project;std::ifstream(fs::u8path(project_file))>>project;std::vector<fs::path> roots;
  for(const auto &root:project.at("content_roots"))roots.push_back(fs::u8path(root.get<std::string>()));
  auto restored=editor::load_scene_extension(fs::u8path(file),roots,1);const auto &d=*restored.document;auto &snapshot=restored.snapshot;
  const auto selected=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.label==label;});
  require(selected!=d.catalog.targets.end(),"真实场景未找到指定 Geograft");const auto target=size_t(selected-d.catalog.targets.begin());
  auto scene=d.loaded.scene;const auto follower=selected->instance;const auto source=scene.instances.at(follower).graft_source;
  require(source>=0,"指定附件没有 Geograft 宿主");const auto host=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.instance==uint32_t(source);});
  require(host!=d.catalog.targets.end(),"真实角色缺少参数目标");const auto host_target=size_t(host-d.catalog.targets.begin());
  const auto controls=editor::geograft_targets(d,host_target);require(std::find(controls.begin(),controls.end(),target)!=controls.end(),"角色本体的 Geograft 组遗漏指定附件");
  runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);
  const auto host_mesh=scene.instances.at(size_t(source)).mesh;const auto &graft=scene.meshes.at(scene.instances.at(follower).mesh);
  const auto masked=scene.meshes.at(host_mesh).hidden_polygons;std::set<uint32_t> remaining(masked.begin(),masked.end());
  for(auto face:graft.graft_hidden_polygons)remaining.erase(face);
  for(size_t t=0;t<d.catalog.targets.size();++t)if(t!=target&&d.catalog.targets[t].conform_target==selected->conform_target&&snapshot.values[t].graft_enabled) {
    const auto &mesh=scene.meshes.at(scene.instances.at(d.catalog.targets[t].instance).mesh);remaining.insert(mesh.graft_hidden_polygons.begin(),mesh.graft_hidden_polygons.end());
  }
  std::vector<std::vector<uint32_t>> before;for(const auto &mesh:scene.meshes)before.push_back(mesh.hidden_polygons);
  snapshot.values[target].graft_enabled=false;const auto disabled=runtime.evaluate(snapshot.values,snapshot.poses);
  const auto unmasked=scene.meshes.at(host_mesh).hidden_polygons;
  require(unmasked==std::vector<uint32_t>(remaining.begin(),remaining.end())&&unmasked.size()<masked.size(),"真实场景停用没有恢复指定身体遮罩面");
  require(!scene.instances.at(follower).visible&&!scene.instances.at(follower).graft_enabled&&!disabled.masks.empty(),"真实场景没有提交附件停用和身体遮罩更新");
  for(const auto &other:d.catalog.targets)if(other.character&&other.instance!=uint32_t(source)) {const auto mesh=scene.instances.at(other.instance).mesh;require(scene.meshes.at(mesh).hidden_polygons==before.at(mesh),"停用修改了另一角色的遮罩");}
  snapshot.values[target].graft_enabled=true;runtime.evaluate(snapshot.values,snapshot.poses);
  require(scene.meshes.at(host_mesh).hidden_polygons==masked&&scene.instances.at(follower).graft_enabled&&scene.instances.at(follower).visible==snapshot.values[target].visible,"真实场景重新启用没有恢复原状态");
  J report={{"status","PASS"},{"file",file},{"label",selected->label},{"host",host->label},{"host_group_contains_graft",true},
    {"vertex_pairs",graft.graft_vertex_pairs.size()},{"graft_mask_polygons",graft.graft_hidden_polygons.size()},
    {"hidden_before",masked.size()},{"hidden_disabled",unmasked.size()},{"restored_polygons",masked.size()-unmasked.size()},
    {"other_characters_unchanged",true},{"reenabled_restored",true}};
  std::cout<<report.dump(2)<<'\n';
}

static editor::Document activation_document(ir::Scene scene=bench::graft_fixture()) {
  editor::Document d;d.generation=1;d.loaded.scene=std::move(scene);
  for(uint32_t i=0;i<d.loaded.scene.instances.size();++i) {
    const auto &instance=d.loaded.scene.instances[i];if(instance.prototype>=0||instance.shell_source>=0)continue;
    runtime::Target t;t.id=instance.id+"/mesh";t.label=instance.id;t.instance=i;t.character=i==0;
    if(instance.graft_source>=0)t.conform_target="#"+d.loaded.scene.instances[size_t(instance.graft_source)].id;
    d.catalog.targets.push_back(t);
  }
  d.formulas.graphs.resize(d.catalog.targets.size());for(auto &g:d.formulas.graphs)g.prepare();return d;
}
static void graft_activation_checks() {
  auto d=activation_document();auto snapshot=editor::initial_snapshot(d);auto scene=d.loaded.scene;
  runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(snapshot.values,{});
  require(editor::geograft_targets(d,0)==std::vector<size_t>{1}&&editor::geograft_targets(d,1)==std::vector<size_t>{1},"宿主和插件没有找到相同的 Geograft 开关");
  snapshot.values[1].visible=false;auto hidden=runtime.evaluate(snapshot.values,{});
  require(hidden.masks.empty()&&scene.meshes[0].hidden_polygons==std::vector<uint32_t>{4},"普通显隐错误解除了 Geograft 遮罩");
  runtime::PickingScene picking;picking.update(scene);const auto origin=scene.instances[0].transform.point({1.5f,1.5f,2});
  require(picking.ray(origin,{0,0,-1}).instance<0,"隐藏插件后的原始空洞夹具不正确");
  snapshot.values[1].graft_enabled=false;auto disabled=runtime.evaluate(snapshot.values,{});picking.apply(scene,disabled);
  require(!disabled.masks.empty()&&!disabled.grafts.empty()&&scene.meshes[0].hidden_polygons.empty()&&!scene.instances[1].visible&&!scene.instances[1].graft_enabled,"停用没有恢复宿主面或移除插件");
  require(snapshot.values[1].visible==false&&runtime::graft_groups(scene).empty()&&picking.ray(origin,{0,0,-1}).instance==0,"停用覆盖了显隐设置或恢复表面无法拾取");
  auto expected=d.loaded.scene.meshes[0];expected.hidden_polygons.clear();runtime::Subdivision full(expected,false),restored(scene.meshes[0],false);
  require(full.triangles()==restored.triangles()&&full.evaluate(expected.positions)==restored.evaluate(scene.meshes[0].positions),"停用后的细分曲面不等于原角色");
  const auto stable=runtime.evaluate(snapshot.values,{});require(stable.masks.empty()&&stable.grafts.empty(),"停用状态反复发布拓扑");
  const auto saved=editor::snapshot_json(d,snapshot);auto reopened=editor::initial_snapshot(d);editor::apply_snapshot_json(d,reopened,saved);
  require(reopened.values==snapshot.values,"保存重开丢失停用状态");
  auto legacy=saved;legacy["objects"][d.catalog.targets[1].id].erase("graft_enabled");editor::apply_snapshot_json(d,reopened,legacy);require(reopened.values[1].graft_enabled,"旧场景没有默认启用插件");
  editor::RenderEditQueue queue;queue.merge(disabled);
  snapshot.values[1].graft_enabled=true;auto enabled=runtime.evaluate(snapshot.values,{});queue.merge(enabled);
  require(!scene.instances[1].visible&&scene.meshes[0].hidden_polygons==std::vector<uint32_t>{4}&&runtime::graft_groups(scene).size()==1,"重新启用没有保留隐藏状态或恢复遮罩");
  require(queue.delta.grafts.size()==1&&queue.delta.grafts[0].enabled&&queue.delta.masks.size()==1&&queue.delta.masks[0].hidden_polygons==std::vector<uint32_t>{4},"渲染队列未合并最后的启用状态");
  snapshot.values[1].visible=true;runtime.evaluate(snapshot.values,{});require(scene.instances[1].visible,"重新启用后无法恢复显隐");

  // 停用时仍可修改宿主形态，重新启用时接缝必须焊接到最新表面。
  auto shaped=activation_document();runtime::Morph morph;morph.id="shape";morph.label="Shape";morph.evaluable=true;morph.offsets={{5,{0,0,.2f}}};shaped.catalog.targets[0].morphs={morph};
  auto &graph=shaped.formulas.graphs[0];graph.channels.resize(1);graph.morph_channels={0};graph.prepare();
  auto values=editor::initial_snapshot(shaped);auto shaped_scene=shaped.loaded.scene;runtime::DeformationRuntime shaping(shaped_scene,shaped.catalog.targets,shaped.skeletons.skins,shaped.formulas.graphs);
  values.values[1].graft_enabled=false;shaping.evaluate(values.values,{});values.values[0].morphs[0]=.75f;shaping.evaluate(values.values,{});values.values[1].graft_enabled=true;shaping.evaluate(values.values,{});
  require(shaping.graft_seams().size()==1&&shaping.graft_seams()[0].max_gap_m<1e-5&&shaped_scene.meshes[0].positions!=shaped.loaded.scene.meshes[0].positions,"重新启用没有对齐已变化的宿主");

  // 同一角色的另一个插件及第二个角色的遮罩不能随第一个插件一起消失。
  auto multiple=bench::graft_fixture();auto second=multiple.meshes[1];second.id="second";second.graft_hidden_polygons={0};multiple.meshes.push_back(second);
  auto second_instance=multiple.instances[1];second_instance.id="second";second_instance.mesh=2;multiple.instances.push_back(second_instance);multiple.meshes[0].hidden_polygons={0,4};
  auto other_body=multiple.meshes[0];other_body.id="other-body";other_body.hidden_polygons={4};multiple.meshes.push_back(other_body);
  auto other_host=multiple.instances[0];other_host.id="other-body";other_host.mesh=3;multiple.instances.push_back(other_host);
  auto other_patch=multiple.meshes[1];other_patch.id="other-patch";multiple.meshes.push_back(other_patch);
  auto other_instance=multiple.instances[1];other_instance.id="other-patch";other_instance.mesh=4;other_instance.graft_source=3;multiple.instances.push_back(other_instance);
  auto md=activation_document(multiple);auto ms=editor::initial_snapshot(md);runtime::DeformationRuntime mr(multiple,md.catalog.targets,md.skeletons.skins,md.formulas.graphs);mr.evaluate(ms.values,{});ms.values[1].graft_enabled=false;mr.evaluate(ms.values,{});
  require(multiple.meshes[0].hidden_polygons==std::vector<uint32_t>{0}&&multiple.meshes[3].hidden_polygons==std::vector<uint32_t>{4}&&multiple.instances[2].visible&&multiple.instances[4].visible,"停用污染了其他插件或另一角色");

  // DAZ Instance 和 Shell 同步停用，Shell 自身隐藏面仍保留。
  auto shells=bench::graft_fixture();auto copy=shells.instances[1];copy.id="copy";copy.prototype=1;copy.graft_source=-1;shells.instances.push_back(copy);
  for(uint32_t part=0;part<2;++part){auto mesh=shells.meshes[part];mesh.id+="/shell";if(part==0){mesh.shell_hidden_polygons={1};mesh.hidden_polygons={1,4};}shells.meshes.push_back(mesh);auto instance=shells.instances[part];instance.id+="/shell";instance.mesh=uint32_t(shells.meshes.size()-1);instance.shell_source=int(part);instance.shell_root=0;instance.graft_source=part?3:-1;shells.instances.push_back(instance);}
  auto sd=activation_document(shells);auto ss=editor::initial_snapshot(sd);runtime::DeformationRuntime sr(shells,sd.catalog.targets,sd.skeletons.skins,sd.formulas.graphs);sr.evaluate(ss.values,{});ss.values[1].graft_enabled=false;sr.evaluate(ss.values,{});
  require(!shells.instances[2].visible&&!shells.instances[4].visible&&shells.instances[3].visible&&shells.meshes[2].hidden_polygons==std::vector<uint32_t>{1},"停用丢失 Shell 自身遮罩或留下实例插件");
  ss.values[1].graft_enabled=true;sr.evaluate(ss.values,{});require(shells.instances[2].visible&&shells.instances[4].visible&&shells.meshes[2].hidden_polygons==std::vector<uint32_t>({1,4}),"实例和 Shell 重新启用失败");

  auto nested=bench::graft_fixture();auto nested_mesh=nested.meshes[1];nested_mesh.id="nested";nested_mesh.graft_target_vertices=5;nested_mesh.graft_target_polygons=4;nested_mesh.graft_vertex_pairs={{0,0},{1,1},{2,4}};nested_mesh.graft_hidden_polygons={0};nested_mesh.positions={nested_mesh.positions[0],nested_mesh.positions[1],nested_mesh.positions[4]};nested_mesh.polygons.resize(1);nested_mesh.polygons[0].vertices={0,1,2};nested_mesh.polygons[0].uv.resize(3);nested_mesh.triangles.resize(1);nested_mesh.triangles[0].vertices={0,1,2};nested_mesh.source_polygon_count=1;nested.meshes[1].hidden_polygons={0};nested.meshes.push_back(nested_mesh);
  auto nested_instance=nested.instances[1];nested_instance.id="nested";nested_instance.mesh=2;nested_instance.graft_source=1;nested.instances.push_back(nested_instance);
  auto nd=activation_document(nested);auto ns=editor::initial_snapshot(nd);runtime::DeformationRuntime nr(nested,nd.catalog.targets,nd.skeletons.skins,nd.formulas.graphs);nr.evaluate(ns.values,{});ns.values[1].graft_enabled=false;nr.evaluate(ns.values,{});
  require(!nested.instances[2].visible&&!nested.instances[2].graft_enabled&&ns.values[2].graft_enabled&&nested.meshes[0].hidden_polygons.empty()&&nested.meshes[1].hidden_polygons.empty()&&runtime::graft_groups(nested).empty(),"父插件停用没有抑制嵌套插件或改写了其独立开关");
  ns.values[1].graft_enabled=true;nr.evaluate(ns.values,{});require(nested.instances[2].visible&&runtime::graft_groups(nested)==std::vector<std::vector<uint32_t>>{{0,1,2}},"父插件重新启用没有恢复嵌套插件");
  ns.values[2].graft_enabled=false;nr.evaluate(ns.values,{});require(nested.instances[1].visible&&nested.meshes[0].hidden_polygons==std::vector<uint32_t>{4}&&nested.meshes[1].hidden_polygons.empty(),"单独停用嵌套插件影响了父插件");
  // 没有接缝的服装遮罩与插件重叠时，仍保留服装所需的遮盖面。
  auto masked=bench::graft_fixture();auto cloth=masked.meshes[1];cloth.id="cloth";cloth.graft_vertex_pairs.clear();masked.meshes.push_back(cloth);
  auto clothing=masked.instances[1];clothing.id="cloth";clothing.mesh=2;clothing.graft_source=-1;masked.instances.push_back(clothing);
  auto cd=activation_document(masked);cd.catalog.targets[2].conform_target="#body";auto cs=editor::initial_snapshot(cd);runtime::DeformationRuntime cr(masked,cd.catalog.targets,cd.skeletons.skins,cd.formulas.graphs);cs.values[1].graft_enabled=false;cr.evaluate(cs.values,{});
  require(masked.meshes[0].hidden_polygons==std::vector<uint32_t>{4}&&masked.instances[2].visible&&editor::geograft_targets(cd,0)==std::vector<size_t>{1},"停用破坏了无接缝服装遮罩");
}
