#include "editor/document.h"
#include "daz/pose.h"
#include "runtime/picking.h"
#include <iostream>
#include <fstream>
#include <chrono>

using namespace dfv;
static void require(bool v,const char *message) {if(!v) throw std::runtime_error(message);}
static void embedded_geometry() {
  using J=nlohmann::json;namespace fs=std::filesystem;
  const auto folder=fs::temp_directory_path()/("dfv-derived-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder/"data/Test/Morphs");
  J asset=J::parse(R"({"node_library":[{"id":"figure","type":"figure"},{"id":"bone","type":"bone","parent":"#figure"}],
    "geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[0,0,0],[100,0,0],[0,100,0]]},"polygon_material_groups":{"count":1,"values":["Skin"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],
    "uv_set_library":[{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}],
    "modifier_library":[{"skin":{"node":"#figure","geometry":"#mesh","vertex_count":3,"joints":[{"node":"#bone","node_weights":{"count":3,"values":[[0,1],[1,1],[2,1]]}}]}}]})");
  std::ofstream(folder/"data/Test/figure.dsf")<<asset.dump();
  std::ofstream(folder/"data/Test/Morphs/raise.dsf")<<R"({"modifier_library":[{"id":"Raise","parent":"/data/Test/figure.dsf#mesh","channel":{"type":"float","value":0,"min":0,"max":1},"morph":{"vertex_count":3,"deltas":{"count":1,"values":[[1,20,0,0]]}}}]})";
  auto geometry=asset["geometry_library"][0];geometry["id"]="embedded";geometry["source"]="/data/Test/figure.dsf#mesh";geometry["default_uv_set"]="/data/Test/figure.dsf#uv";geometry["vertices"]["values"][0][0]=10;
  J duf={{"geometry_library",{geometry}},{"scene",{{"nodes",J::array()},{"materials",J::array()},{"modifiers",J::array()}}}};
  for(int i=0;i<2;++i) {
    const auto id="person"+std::to_string(i),shape="shape"+std::to_string(i);
    duf["scene"]["nodes"].push_back({{"id",id},{"url","/data/Test/figure.dsf#figure"},{"geometries",{{{"id",shape},{"url","#embedded"}}}}});
    duf["scene"]["nodes"].push_back({{"id","bone"+std::to_string(i)},{"parent","#"+id},{"url","/data/Test/figure.dsf#bone"},{"rotation",{{{"id","z"},{"current_value",i?0:90}}}}});
    duf["scene"]["nodes"][i*2]["extra"]={{{"type","studio_node_channels"},{"favorites",i?J::array():J::array({"Raise/Value"})}}};
    duf["scene"]["nodes"].back()["extra"]={{{"type","studio_node_channels"},{"favorites",{i?"YRotate":"ZRotate"}}}};
    duf["scene"]["materials"].push_back({{"id","mat"+std::to_string(i)},{"geometry","#"+shape},{"groups",{"Skin"}}});
    duf["scene"]["modifiers"].push_back({{"url","/data/Test/Morphs/raise.dsf#Raise"},{"parent","#"+id},{"channel",{{"current_value",i?.1:.5}}}});
  }
  const auto file=folder/"embedded.duf";std::ofstream(file)<<duf.dump();editor::Document document;document.loaded=daz::load(file,{{folder},false});
  require(document.loaded.objects[0].geometry_sources.size()==1&&std::abs(document.loaded.scene.meshes[0].positions[0].x-.1f)<1e-6f,"内嵌几何 source 未解析或覆盖了本地顶点");
  document.catalog=daz::discover_morphs(document.loaded,{folder});document.skeletons=daz::load_skeletons(document.loaded);document.formulas=daz::enable_formulas(document.catalog,document.skeletons);
  require(document.catalog.targets[0].morphs.size()==1&&document.skeletons.skins.size()==2,"内嵌几何丢失继承的 Morph 或骨架");
  auto snapshot=editor::initial_snapshot(document);auto rendered=document.loaded.scene;runtime::DeformationRuntime runtime(rendered,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);
  require(std::abs(rendered.meshes[rendered.instances[0].mesh].positions[1].z-1.1f)<1e-5f&&std::abs(rendered.meshes[rendered.instances[1].mesh].positions[1].x-1.02f)<1e-5f,"内嵌几何的保存 Morph / 姿势未应用或跨实例污染");
  auto appended=document;appended.generation=5;editor::append_document(appended,document);
  auto refreshed=editor::refresh_parameters(appended,2,{folder});
  require(refreshed->catalog.targets.size()==4&&refreshed->catalog.targets[2].id==appended.catalog.targets[2].id&&refreshed->catalog.targets[2].morphs[0].initial==.5f&&refreshed->formulas.graphs[2].skin==2,"追加角色刷新丢失来源、场景覆盖或蒙皮映射");
  require(refreshed->catalog.targets[2].favorites==document.catalog.targets[0].favorites&&refreshed->catalog.targets[3].favorites==document.catalog.targets[1].favorites&&refreshed->catalog.targets[2].favorites.at("bone").contains("ZRotate"),"追加后刷新丢失或混用了源场景收藏");
  duf["geometry_library"][0]["polylist"]["values"][0][3]=2;duf["geometry_library"][0]["polylist"]["values"][0][4]=1;std::ofstream(file)<<duf.dump();auto incompatible=daz::load(file,{{folder},false});
  require(incompatible.objects[0].geometry_sources.empty()&&!incompatible.report["warnings"].empty(),"不同拓扑错误继承了权重索引");
  duf["geometry_library"][0]["source"]="#embedded";std::ofstream(file)<<duf.dump();bool rejected=false;try {daz::load(file,{{folder},false});} catch(const std::exception &) {rejected=true;}require(rejected,"派生几何循环没有拒绝");
  fs::remove_all(folder);
}
static void joint_overrides() {
  using J=nlohmann::json;namespace fs=std::filesystem;
  const auto folder=fs::temp_directory_path()/("dfv-joint-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder);
  auto asset=J::parse(R"({"node_library":[{"id":"figure","type":"figure"},{"id":"hinge","type":"bone","parent":"#figure","center_point":[{"id":"x","value":0}]}],
    "geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[100,0,0],[100,10,0],[100,0,10]]},"polygon_material_groups":{"count":1,"values":["Frame"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],
    "uv_set_library":[{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}],
    "modifier_library":[{"skin":{"node":"#figure","geometry":"#mesh","vertex_count":3,"joints":[{"node":"#hinge","node_weights":{"count":3,"values":[[0,1],[1,1],[2,1]]}}]}}]})");
  std::ofstream(folder/"asset.dsf")<<asset.dump();J scene={{"scene",{{"nodes",J::array()},{"materials",J::array()}}}};
  for(int i=0;i<2;++i) {
    const auto id="glasses"+std::to_string(i),shape="shape"+std::to_string(i);
    scene["scene"]["nodes"].push_back({{"id",id},{"url","asset.dsf#figure"},{"center_point",{{{"id","y"},{"current_value",5}}}},{"geometries",{{{"id",shape},{"url","asset.dsf#mesh"}}}}});
    J bone={{"id","hinge"+std::to_string(i)},{"url","asset.dsf#hinge"},{"parent","#"+id},{"rotation",{{{"id","x"},{"current_value",90}}}},{"preview",{{"center_point",{999,999,999}}}}};
    if(i==0) {bone["center_point"]={{{"id","x"},{"current_value",10}}};bone["orientation"]={{{"id","z"},{"current_value",90}}};bone["end_point"]={{{"id","x"},{"current_value",20}}};bone["rotation_order"]="ZYX";bone["inherits_scale"]=false;}
    scene["scene"]["nodes"].push_back(bone);scene["scene"]["materials"].push_back({{"id","material"+std::to_string(i)},{"geometry","#"+shape},{"groups",{"Frame"}}});
  }
  const auto file=folder/"scene.duf";std::ofstream(file)<<scene.dump();const auto loaded=daz::load(file,{{folder},false});const auto catalog=daz::load_skeletons(loaded);
  require(catalog.skins.size()==2,"同一眼镜资产的两个实例丢失");const auto &a=catalog.skins[0],&b=catalog.skins[1];
  require(a.joints[0].center_cm.y==5&&a.joints[1].center_cm.x==10&&a.joints[1].end_cm.x==20&&a.joints[1].rotation_order=="ZYX"&&!a.joints[1].inherits_scale,"保存的关节定义未覆盖资产");
  const auto pa=runtime::deform(a,a.initial,loaded.scene.meshes[loaded.scene.instances[a.instance].mesh].positions)[0],pb=runtime::deform(b,b.initial,loaded.scene.meshes[loaded.scene.instances[b.instance].mesh].positions)[0];
  require(std::abs(pa.x-.1f)<1e-6f&&std::abs(pa.y-.9f)<1e-6f&&std::abs(pa.z)<1e-6f,"铰链没有绕场景保存的中心和轴向旋转");
  require(pb.x==1&&pb.y==0&&pb.z==0&&b.joints[1].center_cm.x==0,"关节覆盖跨实例污染或读取了 preview 缓存");
  fs::remove_all(folder);
}
static void scene_channel_overrides() {
  using J=nlohmann::json;namespace fs=std::filesystem;
  const auto folder=fs::temp_directory_path()/("dfv-channel-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder/"data/Morphs");
  const auto asset=J::parse(R"({"node_library":[{"id":"figure","type":"figure"},{"id":"head","type":"bone","parent":"#figure"}],
    "geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[0,0,0],[100,0,0],[0,100,0]]},"polygon_material_groups":{"count":1,"values":["Skin"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],
    "uv_set_library":[{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}],
    "modifier_library":[{"skin":{"node":"#figure","geometry":"#mesh","vertex_count":3,"joints":[{"node":"#head","node_weights":{"count":3,"values":[[0,1],[1,1],[2,1]]}}]}}]})");
  std::ofstream(folder/"data/figure.dsf")<<asset.dump();
  std::ofstream(folder/"data/Morphs/shape.dsf")<<R"({"modifier_library":[{"id":"Shape","parent":"/data/figure.dsf#mesh",
    "channel":{"type":"float","value":0,"min":0,"max":1,"clamped":true,"step_size":0.01},
    "morph":{"vertex_count":3,"deltas":{"count":1,"values":[[1,0,20,0]]}},
    "formulas":[{"output":"head:/data/figure.dsf#head?center_point/y","operations":[{"op":"push","url":"#Shape?value"},{"op":"push","val":10},{"op":"mult"}]}]}]})";
  J scene={{"scene",{{"nodes",J::array()},{"materials",J::array()},{"modifiers",J::array()}}}};
  const std::array<J,4> channels={J{{"current_value",-.2},{"min",-.2},{"step_size",.1}},J{{"current_value",-.2}},J{{"current_value",2},{"clamped",false}},J{{"current_value",2},{"max",1.5}}};
  for(size_t i=0;i<channels.size();++i) {
    const auto id="person"+std::to_string(i),shape="shape"+std::to_string(i);
    scene["scene"]["nodes"].push_back({{"id",id},{"url","/data/figure.dsf#figure"},{"geometries",{{{"id",shape},{"url","/data/figure.dsf#mesh"}}}}});
    scene["scene"]["nodes"].push_back({{"id","head"+std::to_string(i)},{"parent","#"+id},{"url","/data/figure.dsf#head"}});
    scene["scene"]["materials"].push_back({{"id","mat"+std::to_string(i)},{"geometry","#"+shape},{"groups",{"Skin"}}});
    scene["scene"]["modifiers"].push_back({{"url","/data/Morphs/shape.dsf#Shape"},{"parent","#"+(i%2?id:shape)},{"channel",channels[i]}});
  }
  const auto file=folder/"scene.duf";std::ofstream(file)<<scene.dump();
  // 改过限制的实例既在缓存建立前出现，也在缓存复用后出现；同步与延迟目录结果相同。
  for(bool lazy:{false,true}) for(bool reverse:{false,true}) {
    editor::Document document;document.loaded=daz::load(file,{{folder},false});if(reverse) std::reverse(document.loaded.objects.begin(),document.loaded.objects.end());
    document.catalog=daz::discover_morphs(document.loaded,{folder},{},lazy);document.skeletons=daz::load_skeletons(document.loaded);document.formulas=daz::enable_formulas(document.catalog,document.skeletons);
    const auto snapshot=editor::initial_snapshot(document);auto rendered=document.loaded.scene;runtime::DeformationRuntime runtime(rendered,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);
    const float expected[]={-.2f,0,2,1.5f};
    for(size_t t=0;t<4;++t) {
      const auto i=document.catalog.targets[t].instance;const auto &m=document.catalog.targets[t].morphs[0];const auto s=size_t(document.formulas.graphs[t].skin);
      require(std::abs(runtime.effective()[t][0]-expected[i])<1e-6f,"场景 min / max / clamped 覆盖未生效或污染另一实例");
      require(std::abs(runtime.effective_poses()[s][1].center_offset_cm.y-10*expected[i])<1e-6f,"场景通道限制未用于 Head 中心 ERC");
      require(std::abs(rendered.meshes[rendered.instances[i].mesh].positions[1].z-.2f*expected[i])<1e-6f,"场景通道限制未用于网格形态");
      require(m.step==(i==0?.1f:.01f)&&m.minimum==(i==0?-.2f:0)&&m.maximum==(i==3?1.5f:1)&&m.clamped==(i!=2),"通道未覆盖字段没有继承资产默认值");
      require(document.catalog.report["targets"][t]["morphs"][0]["clamped"]==m.clamped,"参数诊断未反映场景限制");
    }
  }
  fs::remove_all(folder);
}
static void lifecycle() {
  editor::Document document;document.generation=1;
  auto &scene=document.loaded.scene;ir::Mesh mesh;mesh.material_slots={"Skin"};mesh.positions={{0,0,0},{1,0,0},{0,1,0}};ir::Triangle face;face.vertices={0,1,2};mesh.triangles={face};
  scene.textures={{"a","a.png"},{"b","b.png"}};scene.materials.resize(2);scene.materials[0].color_texture=0;scene.materials[1].color_texture=1;
  for(size_t i=0;i<4;++i) {
    const auto id=std::string(1,char('a'+i));scene.meshes.push_back(mesh);ir::Instance instance;instance.id=id+"/mesh";instance.mesh=uint32_t(i);instance.materials={i==3?1u:0u};scene.instances.push_back(instance);
    runtime::Target target;target.id=instance.id;target.instance=uint32_t(i);document.catalog.targets.push_back(target);document.formulas.graphs.emplace_back();
    daz::AssetObject object;object.id=id;object.instance=uint32_t(i);document.loaded.objects.push_back(object);
  }
  document.loaded.nodes={{"a",""},{"head","#a"},{"b","#a"},{"c","#head"},{"d",""}};
  document.loaded.objects[1].conform_target=document.catalog.targets[1].conform_target="#a";
  document.loaded.objects[2].parent=document.catalog.targets[2].parent="#head";
  for(auto i:{0u,3u}) {runtime::Skin skin;skin.id=std::string(1,char('a'+i));skin.instance=i;runtime::Joint root;root.id="root";skin.joints={root};skin.initial.resize(1);skin.weights={{{0,1}},{{0,1}},{{0,1}}};document.formulas.graphs[i].skin=int(document.skeletons.skins.size());document.skeletons.skins.push_back(skin);}
  auto snapshot=editor::initial_snapshot(document);snapshot.values[3].transform.translation_cm.x=123;snapshot.poses[1][0].rotation_degrees.z=17;
  auto clothes_only=document;auto clothes_snapshot=snapshot;require(editor::remove_target(clothes_only,clothes_snapshot,1)==1&&clothes_only.catalog.targets.size()==3,"删除衣物连带删除人物");
  require(editor::remove_target(document,snapshot,0)==3,"删除人物没有清理 Fit To 或骨骼附件");
  require(document.catalog.targets.size()==1&&document.catalog.targets[0].id=="d/mesh"&&document.catalog.targets[0].instance==0,"删除误伤无关模型或未重映射实例");
  require(document.formulas.graphs[0].skin==0&&document.skeletons.skins[0].instance==0&&snapshot.values[0].transform.translation_cm.x==123&&snapshot.poses[0][0].rotation_degrees.z==17,"删除丢失其他角色的公式 / 姿势 / 变换");
  require(scene.meshes.size()==1&&scene.materials.size()==1&&scene.textures.size()==1&&scene.textures[0].id=="b"&&scene.materials[0].color_texture==0,"删除后仍持有孤立资源");
  auto rendered=scene;runtime::DeformationRuntime runtime(rendered,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);
  auto source=document;source.generation=0;
  {
    auto copies=source;auto copy=copies.loaded.scene.instances.front();copy.prototype=0;copy.id="copy/part";copy.instance_group=copy.instance_node="copy";copy.instance_label="Copy";copies.loaded.scene.instances.push_back(copy);copies.loaded.nodes.push_back({"copy",""});
    auto appended=copies;appended.generation=2;editor::append_document(appended,copies);
    const runtime::InstanceGroups groups(appended.loaded.scene);
    require(groups.roots[1]!=groups.roots[3]&&groups.members[1].size()==1&&groups.members[3].size()==1,"追加相同场景后 Instance 选择错误合并");
    const auto &added=appended.loaded.scene.instances[3];require(added.instance_group==added.instance_node&&added.instance_label=="Copy","追加实例丢失节点级选择身份");
  }
  auto lamp=document;auto lamp_snapshot=snapshot;ir::AreaLight light;light.id="lamp";lamp_snapshot.lights={light};lamp.loaded.nodes.push_back({"lamp",""});lamp.loaded.nodes[0].parent="#lamp";
  require(editor::remove_light(lamp,lamp_snapshot,0)==1&&lamp.loaded.nodes.empty()&&lamp_snapshot.lights.empty()&&lamp.loaded.scene.meshes.empty(),"删除灯光保留了节点或子对象");
  for(int iteration=0;iteration<64;++iteration) {
    document.generation=uint64_t(iteration+2);editor::append_document(document,source);snapshot=editor::initial_snapshot(document);
    require(editor::remove_target(document,snapshot,1)==1&&scene.meshes.size()==1&&scene.materials.size()==1&&scene.textures.size()==1&&document.loaded.nodes.size()==1,"反复增删保留了历史模型资源");
  }
  daz::LoadedScene preset;preset.scene.textures={{"new","new.png"}};preset.scene.materials.resize(1);preset.scene.materials[0].color_texture=0;preset.report["materials"]={{{"groups",{"Skin"}}}};
  for(int i=0;i<64;++i) editor::apply_materials(document,0,preset);
  require(scene.materials.size()==1&&scene.textures.size()==1,"反复换材质累积旧贴图或材质");
  preset.report["materials"][0]["groups"]={"missing"};bool rejected=false;try {editor::apply_materials(document,0,preset);} catch(const std::exception &) {rejected=true;}
  require(rejected&&scene.materials.size()==1&&scene.textures.size()==1,"不匹配的材质污染文档");
  editor::remove_target(document,snapshot,0);require(scene.instances.empty()&&scene.meshes.empty()&&scene.materials.empty()&&scene.textures.empty()&&document.catalog.targets.empty()&&document.skeletons.skins.empty()&&document.formulas.graphs.empty(),"删除最后一个对象未释放全部资源");
  rendered=scene;runtime::DeformationRuntime empty(rendered,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);empty.evaluate(snapshot.values,snapshot.poses);
}
static void graft_seams() {
  editor::Document document;auto &scene=document.loaded.scene;
  ir::Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{0,0,1}};ir::Triangle face;face.vertices={0,1,2};mesh.triangles={face};mesh.source_polygon_count=1;
  scene.meshes={mesh,mesh,mesh};scene.instances.resize(3);
  for(uint32_t i=0;i<3;++i) {scene.instances[i].mesh=i;scene.instances[i].id="node"+std::to_string(i)+"/mesh";}
  scene.instances[0].transform.value={2,0,0,.3f,0,1,0,-.2f,0,0,.5f,.7f};
  scene.instances[1].transform.value={0,1,0,-.4f,-1,0,0,.2f,0,0,2,.1f};
  for(uint32_t i=1;i<3;++i) {auto &m=scene.meshes[i];m.positions[0].x+=.07f;m.graft_target_vertices=3;m.graft_target_polygons=1;m.graft_vertex_pairs={{0,0},{1,1}};}
  // 故意逆序：嵌套插件先出现在目录中，仍必须按宿主依赖求值。
  for(uint32_t i:{2u,1u,0u}) {runtime::Target t;t.instance=i;t.id=scene.instances[i].id;if(i) t.conform_target="#node"+std::to_string(i-1);document.catalog.targets.push_back(t);document.formulas.graphs.emplace_back();}
  runtime::Morph morph;morph.id=morph.channel_id="shape";morph.evaluable=true;morph.offsets={{0,{.1f,.2f,.3f}}};document.catalog.targets[2].morphs={morph};
  auto &graph=document.formulas.graphs[2];graph.channels.emplace_back();graph.morph_channels={0};graph.prepare();
  runtime::Skin skin;skin.instance=0;skin.joints.emplace_back();skin.initial.resize(1);skin.weights={{{0,1}},{{0,1}},{{0,1}}};document.skeletons.skins={skin};graph.skin=0;
  auto snapshot=editor::initial_snapshot(document);runtime::DeformationRuntime runtime(scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);
  auto check=[&] {const auto seams=runtime.graft_seams();require(seams.size()==2,"嵌套接缝未绑定");for(const auto &s:seams) require(s.pairs==2&&s.max_gap_m<1e-5,"最终世界坐标的 GeoGraft 接缝存在缝隙");};
  auto delta=runtime.evaluate(snapshot.values,snapshot.poses);check();require(delta.meshes.size()>=2,"初始接缝未提交网格 Delta");
  snapshot.values[2].morphs[0]=.7f;snapshot.poses[0][0].rotation_degrees.z=27;runtime.evaluate(snapshot.values,snapshot.poses);check();
  snapshot.values[2].transform.translation_cm={70,-15,23};snapshot.values[2].transform.rotation_degrees.y=36;
  delta=runtime.evaluate(snapshot.values,snapshot.poses);check();require(delta.meshes.empty(),"共同祖先移动不应重写接缝几何");
  snapshot.values[1].transform.translation_cm.x=9;delta=runtime.evaluate(snapshot.values,snapshot.poses);check();require(!delta.meshes.empty(),"插件相对变换没有重新锁定接缝");
  snapshot.values[1].visible=false;delta=runtime.evaluate(snapshot.values,snapshot.poses);check();require(delta.meshes.empty()&&!delta.visibility.empty(),"隐藏插件重写几何或丢失可见性");
  snapshot=editor::initial_snapshot(document);snapshot.values[2].transform={};snapshot.values[1].transform={};
  runtime.evaluate(snapshot.values,snapshot.poses);check();
  for(int repeat=0;repeat<8;++repeat) {delta=runtime.evaluate(snapshot.values,snapshot.poses);check();require(delta.meshes.empty(),"静止接缝反复求值或累积漂移");}
}
static void unit() {
  graft_seams();
  {
    ir::Scene scene;ir::Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{0,1,0}};ir::Triangle face;face.vertices={0,1,2};mesh.triangles={face};scene.meshes={mesh};scene.instances.resize(2);
    scene.instances[1].transform.value={2,0,0,-.2f,0,3,0,-.3f,0,0,.5f,2};
    runtime::PickingScene pick;pick.update(scene);const auto builds=pick.stats().mesh_builds;
    auto hit=pick.ray({.2f,.2f,10},{0,0,-1});require(hit.instance==1&&std::abs(hit.distance-8)<1e-6f,"局部射线丢失非均匀缩放后的距离");
    ir::Delta delta;scene.instances[1].transform.value[3]=10;delta.instances.push_back({1,scene.instances[1].transform});pick.apply(scene,delta);
    require(pick.stats().mesh_builds==builds&&pick.ray({.2f,.2f,10},{0,0,-1}).instance==0,"实例移动重建三角面或没有更新包围盒");
    delta={};scene.instances[0].visible=false;delta.visibility.push_back({0,false});pick.apply(scene,delta);
    require(pick.ray({.2f,.2f,10},{0,0,-1}).instance<0&&pick.stats().mesh_builds==builds,"隐藏对象仍可选取或重建了几何");
    scene.instances[0].visible=true;for(auto &p:scene.meshes[0].positions) p.z=1;delta={};delta.visibility.push_back({0,true});delta.meshes.push_back({0,scene.meshes[0].positions});pick.apply(scene,delta);
    hit=pick.ray({.2f,.2f,10},{0,0,-1});require(hit.instance==0&&std::abs(hit.distance-9)<1e-6f&&pick.stats().mesh_builds==builds+1,"共享网格形变没有正确更新选取数据");
  }
  scene_channel_overrides();
  joint_overrides();
  embedded_geometry();
  lifecycle();
  ir::Scene scene;ir::Mesh mesh;mesh.positions={{-1,0,-1},{1,0,-1},{0,0,1}};ir::Triangle triangle;triangle.vertices={0,1,2};mesh.triangles={triangle};mesh.material_slots={"Skin"};mesh.polygon_groups={"head"};scene.meshes={mesh};scene.materials.resize(1);
  ir::Instance a;a.materials={0};scene.instances={a,a};scene.instances[1].transform=ir::Transform::translate({0,1,0});
  runtime::PickingScene picking;picking.update(scene);require(picking.ray({0,-2,0},{0,1,0}).instance==0,"射线未选最近对象");require(picking.ray({4,-2,0},{0,1,0}).instance==-1,"射线空白误命中");
  scene.instances[0].visible=false;picking.update(scene);require(picking.ray({0,-2,0},{0,1,0}).instance==1,"隐藏对象仍遮挡视口选取");scene.instances[0].visible=true;
  scene.instances[0].transform=ir::Transform::translate({4,0,0});picking.update(scene);require(picking.ray({0,-2,0},{0,1,0}).instance==1,"变换后命中未更新");
  runtime::Skin skin;runtime::Joint joint;joint.id="head";skin.joints={joint};require(runtime::hit_joint(mesh,0,skin)==0,"多边形组未映射到头部");
  auto parts_mesh=mesh;parts_mesh.polygon_groups.push_back("leftHandAlias");auto hand_face=triangle;hand_face.polygon_group=1;auto weighted_face=triangle;weighted_face.polygon_group=99;parts_mesh.triangles={triangle,hand_face,weighted_face};
  auto parts_skin=skin;runtime::Joint hand;hand.id="lHand";hand.aliases={"leftHandAlias"};parts_skin.joints.push_back(hand);parts_skin.weights={{{0,.3},{1,.7}},{{0,.3},{1,.7}},{{0,.3},{1,.7}}};
  const auto parts=runtime::joint_regions(parts_mesh,parts_skin);require(parts.detail==std::vector<int>({0,1,1}),"悬停区域未共用组 / 别名 / 蒙皮权重的部位映射");
  std::vector<runtime::JointRegions> regions={parts,parts,{}};
  require(runtime::hover_region({0,1},-1,-1,regions).joint==-1,"未选中角色没有整体高亮");
  require(runtime::hover_region({0,0},0,-1,regions).joint==0&&runtime::hover_region({0,1},0,-1,regions).joint==1,"已选中角色没有按悬停位置切换部位");
  require(runtime::hover_region({1,1},0,-1,regions).joint==-1&&runtime::hover_region({0,0},1,-1,regions).joint==-1,"切换角色后仍沿用旧角色部位高亮");
  require(runtime::hover_region({2,0},2,-1,regions).instance==2&&runtime::hover_region({2,0},2,-1,regions).joint==-1,"普通模型错误进入二级悬停");
  require(runtime::hover_region({},0,-1,regions).instance==-1,"空白仍有悬停高亮");regions[0].detail[0]=regions[0].body[0]=-1;require(runtime::hover_region({0,0},0,-1,regions).instance==-1,"未解析部位错误高亮整个人物");
  auto head_mesh=mesh;head_mesh.polygon_groups={"Head","lEye","lHand"};head_mesh.positions.resize(9);head_mesh.triangles={triangle,triangle,triangle,triangle};head_mesh.triangles[1].vertices={3,4,5};head_mesh.triangles[1].polygon_group=1;head_mesh.triangles[2].vertices={6,7,8};head_mesh.triangles[3].polygon_group=2;
  auto head_skin=skin;runtime::Joint eye;eye.id="lEye";eye.parent=0;runtime::Joint lip;lip.id="LipUpperMiddle";lip.parent=0;head_skin.joints.insert(head_skin.joints.end(),{eye,lip,hand});head_skin.weights.resize(9);
  for(size_t v=0;v<9;++v) head_skin.weights[v]={{uint32_t(v/3),1}};
  const auto head_parts=runtime::joint_regions(head_mesh,head_skin);require(head_parts.detail==std::vector<int>({0,1,2,3})&&head_parts.body==std::vector<int>({0,0,0,3}),"头部没有聚合眼睛和嘴唇，或忽略明确的区域边界");
  regions={head_parts,head_parts};require(runtime::hover_region({0,1},-1,-1,regions).joint==-1,"头部首次点击没有选整体");
  require(runtime::hover_region({0,1},0,-1,regions).joint==0&&runtime::hover_region({0,2},0,-1,regions).joint==0,"二次点击眼睛 / 嘴唇没有先选整个头部");
  require(runtime::hover_region({0,1},0,0,regions).joint==1&&runtime::hover_region({0,2},0,1,regions).joint==2,"头部选中后不能进入眼睛 / 嘴唇");
  require(runtime::hover_region({0,1},0,3,regions).joint==0&&runtime::hover_region({1,1},0,1,regions).joint==-1,"头部细选上下文泄漏到手部或其他角色");
  runtime::JointRegions fingers;fingers.detail=fingers.body={1,2};fingers.parents={-1,0,0};
  regions={fingers,fingers,{}};
  std::vector<runtime::HoverRegion> selected={{0,1}};
  require(runtime::selection_region({0,1},{0,1},selected,regions,true).joint==2,"Ctrl 选右手指尖退回整个角色");
  selected.push_back({0,2});
  require(runtime::selection_region({0,0},{0,2},selected,regions,true).joint==1,"Ctrl 再点左手指尖无法取消具体骨骼");
  selected.push_back({1,-1});
  require(runtime::selection_region({0,1},{1,-1},selected,regions,true).joint==2,"活动项切到另一角色后丢失手指多选上下文");
  require(runtime::selection_region({1,0},{1,-1},selected,regions,true).joint==-1,"Ctrl 不能取消整体选中的角色");
  require(runtime::selection_region({2,0},{0,1},selected,regions,true).instance==2,"骨骼多选时不能增选普通模型");
  require(runtime::selection_region({}, {0,1},selected,regions,true).instance==-1,"Ctrl 空白错误命中角色");
  selected={{0,-1}};
  require(runtime::selection_region({0,0},{0,-1},selected,regions,true).joint==-1,"Ctrl 全选角色错误降级为骨骼");
  require(runtime::selection_region({1,0},{0,-1},selected,regions,true).joint==-1,"Ctrl 未选中角色错误进入骨骼选择");
  require(runtime::selection_region({0,0},{0,-1},selected,regions,false).joint==1,"普通点击无法从整体进入骨骼选择");
  regions={head_parts,head_parts};selected={{0,3},{0,1},{1,-1}};
  require(runtime::selection_region({0,2},{1,-1},selected,regions,true).joint==2,"手部和眼睛多选时不能增选嘴唇");
  std::reverse(selected.begin(),selected.end());
  require(runtime::selection_region({0,2},{1,-1},selected,regions,true).joint==2,"头部细选受集合顺序影响");
  selected={{0,3},{1,1}};
  require(runtime::selection_region({0,2},{1,1},selected,regions,true).joint==0,"其他角色的眼睛选择跳过当前角色 Head 层级");
  auto worn_scene=scene;worn_scene.instances[0].transform={};worn_scene.instances[1].transform=ir::Transform::translate({0,1,0});
  runtime::Target garment;garment.instance=0;garment.id="garment/mesh";garment.conform_target="#person";runtime::Target person;person.instance=1;person.id="person/mesh";
  picking.update(worn_scene,runtime::viewport_pick_mask(worn_scene,{garment,person}));require(picking.ray({0,-2,0},{0,1,0}).instance==1,"绑定服装仍遮挡射线，无法命中身后角色");
  worn_scene.meshes[0].graft_target_vertices=3;picking.update(worn_scene,runtime::viewport_pick_mask(worn_scene,{garment,person}));require(picking.ray({0,-2,0},{0,1,0}).instance==0,"GeoGraft 被服装穿透掩码误排除");worn_scene.meshes[0].graft_target_vertices=0;
  garment.conform_target.clear();garment.parent="#person";picking.update(worn_scene,runtime::viewport_pick_mask(worn_scene,{garment,person}));require(picking.ray({0,-2,0},{0,1,0}).instance==0,"未绑定的服装不可选，或普通 parent 被误当 Fit To");
  require(runtime::parameter_on_node("head","/Pose Controls","head"),"头部别名不可见");require(!runtime::parameter_on_node("lHand","/Actor/Hands","head"),"头部混入手部控制器");
  editor::Document first;first.generation=2;first.loaded.scene=scene;editor::Document second=first;
  runtime::Target target;target.instance=0;target.id="figure";second.catalog.targets={target};second.skeletons.skins={skin};runtime::FormulaGraph graph;graph.skin=0;second.formulas.graphs={graph};
  editor::append_document(first,std::move(second));require(first.catalog.targets[0].instance==2&&first.loaded.scene.instances[2].mesh==1,"合并资源索引错误");require(first.catalog.targets[0].id!="figure","合并未隔离实例身份");
  nlohmann::json shape={{"asset_info",{{"type","preset_shape"}}},{"scene",{{"animations",{{{"url","name://@selection#Face:?value/value"},{"keys",{{0,0}}}}}}}}};
  require(daz::parse_pose(shape).channels[0].value==0,"Shape 显式归零未保留");
  auto combined=shape;combined["asset_info"]["type"]="preset_character";combined["scene"]["materials"]={{{"id","Skin"},{"groups",{"Skin"}}}};
  const auto content=daz::inspect_contents(combined);require(!content.instantiate&&content.materials&&content.properties&&daz::parse_pose(combined).channels[0].value==0,"复合 DUF 被类型字符串拒绝或只执行一种用途");
  combined["scene"]["nodes"]={{{"id","figure"},{"geometries",{{{"id","mesh"},{"url","#geometry"}}}}}};require(daz::inspect_contents(combined).instantiate,"复合 DUF 的几何内容没有优先实例化");
  editor::Document figure;figure.loaded.scene.meshes={mesh};figure.loaded.scene.instances={a};figure.loaded.scene.materials.resize(1);
  runtime::Morph morph;morph.id="Face";morph.channel_id=morph.channel_name="Face";morph.offsets={{0,{.1f,0,0}}};target.morphs={morph};target.id="figure/geometry";figure.catalog.targets={target};
  auto duplicate=figure;figure.generation=7;editor::append_document(figure,std::move(duplicate));
  runtime::MorphRuntime deformation(figure.loaded.scene,figure.catalog.targets);deformation.set_morph(0,0,.5f);deformation.evaluate();
  require(figure.loaded.scene.meshes[1].positions[0].x==mesh.positions[0].x,"合并后的角色共享了可编辑顶点");
  runtime::TransformValues transform;transform.translation_cm.x=100;deformation.set_transform(1,transform);deformation.evaluate();require(figure.loaded.scene.instances[0].transform.value[3]==0,"合并后的角色共享了变换");
  skin.initial.resize(1);runtime::Properties values;values.morphs={1};auto applied=daz::apply_pose(daz::parse_pose(shape),skin,skin.initial,target,values);require(applied.properties.morphs[0]==0,"Shape 未清除显式零值");
  figure.catalog.targets[1].parent="#figure";runtime::MorphRuntime hierarchy(figure.loaded.scene,figure.catalog.targets);hierarchy.set_transform(0,transform);require(hierarchy.evaluate().instances.size()==2,"父对象移动未更新子对象");
  const auto temporary=std::filesystem::temp_directory_path()/("dfv-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(temporary);
  const auto preset_file=temporary/"material.duf";std::ofstream(preset_file)<<R"({"asset_info":{"type":"preset_material"},"scene":{"materials":[{"id":"Red","geometry":"#selection","groups":["Skin"],"diffuse":{"channel":{"value":[1,0,0]}}}]}})";
  const auto preset=daz::load(preset_file,{{temporary},false});const auto previous=figure.loaded.scene.materials.at(figure.loaded.scene.instances[1].materials[0]);editor::apply_materials(figure,0,preset);
  const auto &unchanged=figure.loaded.scene.materials.at(figure.loaded.scene.instances[1].materials[0]);require(unchanged.id==previous.id&&unchanged.base_color.x==previous.base_color.x&&unchanged.base_color.y==previous.base_color.y&&unchanged.roughness==previous.roughness,"材质预设污染其他实例");require(figure.loaded.scene.materials[figure.loaded.scene.instances[0].materials[0]].base_color.x==1,"材质预设未应用");
  const auto light_file=temporary/"light.duf";std::ofstream(light_file)<<R"({"asset_info":{"type":"scene"},"scene":{"nodes":[{"id":"Spot","type":"light","color":[0.8,0.5,0.1],"spot":{"intensity":4,"falloff_angle":60}}]}})";
  const auto lights=daz::load(light_file,{{temporary},false});require(lights.scene.lights.size()==1&&lights.scene.lights[0].kind==ir::LightKind::spot&&lights.scene.lights[0].power.x==3.2f,"DSON 灯光导入错误");
  std::filesystem::remove(preset_file);std::filesystem::remove(light_file);std::filesystem::remove(temporary);
  std::cout<<"Scene append / picking / occlusion / groups / shape: PASS\n";
}
static void actual_selection(const std::filesystem::path &file,const std::filesystem::path &output) {
  const std::vector<std::filesystem::path> roots={L"H:/G1",L"H:/G3",L"C:/Users/Public/Documents/My DAZ 3D Library",L"C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};
  const auto loaded=daz::load(file,{roots,false});const auto skeletons=daz::load_skeletons(loaded);const auto &scene=loaded.scene;
  std::vector<runtime::Target> targets;for(const auto &object:loaded.objects) {runtime::Target t;t.instance=object.instance;t.id=scene.instances[t.instance].id;t.conform_target=object.conform_target;targets.push_back(t);}
  const auto mask=runtime::viewport_pick_mask(scene,targets);runtime::PickingScene all,filtered;all.update(scene);filtered.update(scene,mask);
  nlohmann::json report={{"status","PASS"},{"head",nlohmann::json::array()},{"garments",nlohmann::json::array()}};
  for(const auto &skin:skeletons.skins) {if(!mask[skin.instance]) continue;const auto &mesh=scene.meshes[scene.instances[skin.instance].mesh];const auto regions=runtime::joint_regions(mesh,skin);if(regions.head<0) continue;
    const auto head_count=std::count(regions.body.begin(),regions.body.end(),regions.head);require(head_count>0,"真实角色没有完整头部区域");bool eye=false,lip=false;
    const std::set<std::string> head_groups={"Head","LowerJaw","UpperJaw","Tongue","lEye","rEye"};size_t grouped=0;
    for(const auto &triangle:mesh.triangles) if(triangle.polygon_group<mesh.polygon_groups.size()&&head_groups.contains(mesh.polygon_groups[triangle.polygon_group])) ++grouped;
    require(size_t(head_count)==grouped,"真实头部区域没有遵循 DAZ 的头 / 颈多边形组边界");
    for(size_t t=0;t<regions.detail.size();++t) if(regions.body[t]==regions.head&&regions.detail[t]!=regions.head) {
      const int joint=regions.detail[t];const auto &id=skin.joints[size_t(joint)].id;eye|=id=="lEye"||id=="rEye";const auto slot=mesh.triangles[t].material_slot;
      lip|=slot<mesh.material_slots.size()&&mesh.material_slots[slot]=="Lips"&&(id.find("LipUpper")!=std::string::npos||id.find("LipLower")!=std::string::npos);
      const std::vector<runtime::JointRegions> one{regions};require(runtime::hover_region({0,int(t)},0,-1,one).joint==regions.head,"真实面部第二次选择没有聚合 Head");require(runtime::hover_region({0,int(t)},0,regions.head,one).joint==joint,"真实面部第三次选择没有细分");}
    require(eye&&lip,"真实头部缺少眼球或嘴唇细分");report["head"].push_back({{"instance",skin.instance},{"head_triangles",head_count},{"eyes",eye},{"lips",lip}});
  }
  bool unbound_checked=false;
  for(const auto &object:loaded.objects) {
    if(!object.id.starts_with("Shorts_")&&!object.id.starts_with("SportsBra_")) continue;
    require(!mask[object.instance],"test2 的绑定服装仍可由视口命中");uint32_t source=uint32_t(-1);for(const auto &o:loaded.objects) if("#"+o.id==object.conform_target) source=o.instance;require(source<scene.instances.size(),"服装目标实例丢失");
    const auto &host=scene.instances[source];ir::Bounds bounds;for(auto p:scene.meshes[host.mesh].positions) bounds.add(host.transform.point(p));const auto center=bounds.center();
    const auto &cloth=scene.instances[object.instance];const auto &mesh=scene.meshes[cloth.mesh];size_t checked=0;
    for(size_t t=0;t<mesh.triangles.size();t+=std::max(size_t(1),mesh.triangles.size()/128)) {
      ir::Vec3 p;for(auto v:mesh.triangles[t].vertices) {const auto q=cloth.transform.point(mesh.positions[v]);p.x+=q.x/3;p.y+=q.y/3;p.z+=q.z/3;}
      const auto dir=normalized({center.x-p.x,center.y-p.y,center.z-p.z});const ir::Vec3 direction{dir.x,dir.y,dir.z},origin{p.x-dir.x*.25f,p.y-dir.y*.25f,p.z-dir.z*.25f};
      if(all.ray(origin,direction).instance!=int(object.instance)||filtered.ray(origin,direction).instance!=int(source)) continue;++checked;
      if(!unbound_checked) {auto detached=mask;detached[object.instance]=1;runtime::PickingScene unbound;unbound.update(scene,detached);require(unbound.ray(origin,direction).instance==int(object.instance),"解除绑定后的真实服装仍无法被选取");unbound_checked=true;}
    }
    require(checked>0,"未验证服装前景射线能穿过并命中所属人物");report["garments"].push_back({{"object",object.id},{"target",source},{"rays",checked}});
  }
  require(report["head"].size()==4&&report["garments"].size()==8&&unbound_checked,"test2 头部 / 服装验证数量不足");report["unbound_pickable"]=true;
  std::filesystem::create_directories(output.parent_path());std::ofstream(output)<<report.dump(2);std::cout<<"Real head hierarchy / 8 bound garments / unbound pick: PASS\n";
}
int wmain(int argc,wchar_t **argv) {
  try {
    if(argc==1) {unit();return 0;}
    if(argc==4&&std::wstring(argv[1])==L"--selection") {actual_selection(argv[2],argv[3]);return 0;}
    std::vector<std::filesystem::path> roots={L"H:/G1",L"H:/G3",L"C:/Users/Public/Documents/My DAZ 3D Library",L"C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};
    if(argc==4&&std::wstring(argv[1])==L"--fidelity") {
      using J=nlohmann::json;editor::Document doc;doc.loaded=daz::load(argv[2],{roots,false});
      doc.catalog=daz::discover_morphs(doc.loaded,roots,[](const auto &m) {std::cout<<m<<std::endl;},true);
      doc.skeletons=daz::load_skeletons(doc.loaded);doc.formulas=daz::enable_formulas(doc.catalog,doc.skeletons);
      auto snapshot=editor::initial_snapshot(doc);auto scene=doc.loaded.scene;
      runtime::DeformationRuntime runtime(scene,doc.catalog.targets,doc.skeletons.skins,doc.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);
      auto target=[&](const std::string &label) {for(size_t t=0;t<doc.catalog.targets.size();++t) if(doc.catalog.targets[t].label==label) return t;throw std::runtime_error("测试对象缺失："+label);};
      const auto a=target("A"),b=target("B"),hair=target("FE Low Ponytail Base Hair"),shoe=target("RNKids Blooms Slippers"),hidden=target("HYLongHair");
      auto instance=[&](size_t t)->ir::Instance & {return scene.instances.at(doc.catalog.targets.at(t).instance);};
      require(!instance(hidden).visible,"真实隐藏头发仍可见");
      const auto &curves=scene.meshes.at(instance(hair).mesh).curves;require(curves.size()==35638,"真实发丝数量不一致");
      const auto shoe_bind=ir::inverse(instance(a).transform)*instance(shoe).transform;
      require(std::abs(shoe_bind.value[3])<1e-5f&&std::abs(shoe_bind.value[7])<1e-5f&&std::abs(shoe_bind.value[11])<1e-5f,"Fit To 重复叠加了拖鞋的旧位移");
      J report={{"status","PASS"},{"curves",curves.size()},{"objects",J::array()},{"materials",J::array()}};
      for(size_t t=0;t<doc.catalog.targets.size();++t) {const auto &i=instance(t);ir::Bounds bounds;for(auto p:scene.meshes[i.mesh].positions) bounds.add(i.transform.point(p));
        report["objects"].push_back({{"label",doc.catalog.targets[t].label},{"visible",i.visible},{"bounds",{{bounds.minimum.x,bounds.minimum.y,bounds.minimum.z},{bounds.maximum.x,bounds.maximum.y,bounds.maximum.z}}}});}
      for(const auto &m:scene.materials) if(m.hair||m.subsurface>0||m.transmission>0) report["materials"].push_back({{"id",m.id},{"hair",m.hair},{"subsurface",m.subsurface},{"thin_walled",m.thin_walled},{"transmission",m.transmission},{"radius_m",m.hair_root_radius}});
      const auto original=snapshot;const auto shoe_initial=instance(shoe).transform;const auto b_initial=instance(b).transform;
      const auto doll=target("FE Low Ponytail Base Hair Doll"),button=target("SU Fashion Long Jeans Button");
      const auto doll_initial=instance(doll).transform,button_initial=instance(button).transform;
      const auto &doll_mesh=scene.meshes.at(instance(doll).mesh);ir::Bounds doll_bounds;for(auto p:doll_mesh.positions) doll_bounds.add(instance(doll).transform.point(p));
      require(doll_bounds.minimum.z>1.6f&&doll_bounds.maximum.z<1.85f,"嵌套发饰的初始位置偏离头部");
      const auto idle=runtime.evaluate(snapshot.values,snapshot.poses);require(idle.meshes.empty()&&idle.instances.empty(),"静止场景仍产生几何更新");
      snapshot.values[a].transform.translation_cm.x=10;runtime.evaluate(snapshot.values,snapshot.poses);
      require(std::abs(instance(shoe).transform.value[3]-shoe_initial.value[3]-.1f)<1e-5f&&instance(b).transform.value==b_initial.value,"拖鞋未跟随 A，或影响 B");
      require(std::abs(instance(doll).transform.value[3]-doll_initial.value[3]-.1f)<1e-5f&&std::abs(instance(button).transform.value[3]-button_initial.value[3]-.1f)<1e-5f,"经过无网格父节点的纽扣 / 发饰没有随角色移动");
      snapshot=original;runtime.evaluate(snapshot.values,snapshot.poses);require(instance(shoe).transform.value==shoe_initial.value,"拖鞋恢复存在漂移");
      snapshot.values[hidden].visible=true;auto visible=runtime.evaluate(snapshot.values,snapshot.poses);require(instance(hidden).visible&&!visible.visibility.empty()&&visible.meshes.empty(),"真实隐藏物体不能增量显示");
      snapshot=original;runtime.evaluate(snapshot.values,snapshot.poses);
      const auto hair_before=scene.meshes.at(instance(hair).mesh).positions;const auto b_before=scene.meshes.at(instance(b).mesh).positions;
      const int sk=doc.formulas.graphs[a].skin;require(sk>=0,"A 没有骨架");const auto &skin=doc.skeletons.skins[size_t(sk)];
      size_t head=0;while(head<skin.joints.size()&&skin.joints[head].name!="head"&&skin.joints[head].id!="head") ++head;require(head<skin.joints.size(),"A 没有头骨");
      snapshot.poses[size_t(sk)][head].rotation_degrees.y+=10;runtime.evaluate(snapshot.values,snapshot.poses);
      require(instance(doll).transform.value!=doll_initial.value,"发饰没有跟随头部姿势");
      double movement=0;const auto &after=scene.meshes.at(instance(hair).mesh).positions;for(size_t v=0;v<after.size();++v) movement=std::max(movement,double(std::abs(after[v].x-hair_before[v].x)+std::abs(after[v].y-hair_before[v].y)+std::abs(after[v].z-hair_before[v].z)));
      require(movement>.001,"发丝没有随头骨变形");report["hair_pose_displacement_l1_m"]=movement;
      const auto &other=scene.meshes.at(instance(b).mesh).positions;for(size_t v=0;v<other.size();++v) require(other[v].x==b_before[v].x&&other[v].y==b_before[v].y&&other[v].z==b_before[v].z,"A 发丝测试影响 B");
      snapshot=original;runtime.evaluate(snapshot.values,snapshot.poses);const auto &reset=scene.meshes.at(instance(hair).mesh).positions;for(size_t v=0;v<reset.size();++v) require(reset[v].x==hair_before[v].x&&reset[v].y==hair_before[v].y&&reset[v].z==hair_before[v].z,"发丝姿势恢复存在漂移");
      size_t abdomen=0;while(abdomen<skin.joints.size()&&skin.joints[abdomen].name!="abdomenLower") ++abdomen;require(abdomen<skin.joints.size(),"缺少腹部骨骼");
      snapshot.poses[size_t(sk)][abdomen].rotation_degrees.x+=10;runtime.evaluate(snapshot.values,snapshot.poses);
      require(instance(button).transform.value!=button_initial.value,"纽扣没有跟随腹部姿势");
      snapshot=original;runtime.evaluate(snapshot.values,snapshot.poses);
      require(instance(doll).transform.value==doll_initial.value&&instance(button).transform.value==button_initial.value,"附件恢复漂移");
      report["accessories_translation_pose_reset"]="PASS";report["fitted_shoe_transform"]="PASS";
      std::ofstream(std::filesystem::path(argv[3]))<<report.dump(2);std::cout<<"Scene fidelity: PASS"<<std::endl;return 0;
    }
    if(argc>=4&&std::wstring(argv[1])==L"--geometry") {
      using J=nlohmann::json;editor::Document document;document.loaded=daz::load(argv[2],{roots,false});
      if(argc>4) {std::set<std::string> selected;for(int i=4;i<argc;++i) {auto u=std::filesystem::path(argv[i]).u8string();selected.emplace(u.begin(),u.end());}
        std::erase_if(document.loaded.objects,[&](const auto &o) {return !selected.contains(o.id)&&!selected.contains(o.label);});}
      document.catalog=daz::discover_morphs(document.loaded,roots,[](const auto &s) {std::cout<<s<<std::endl;},true);
      document.skeletons=daz::load_skeletons(document.loaded);document.formulas=daz::enable_formulas(document.catalog,document.skeletons);
      const auto snapshot=editor::initial_snapshot(document);auto scene=document.loaded.scene;
      runtime::DeformationRuntime runtime(scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);
      auto uncorrected=document.loaded.scene;
      {auto targets=document.catalog.targets;for(auto &t:targets) t.smoothing.enabled=false;
        runtime::DeformationRuntime without_collision(uncorrected,targets,document.skeletons.skins,document.formulas.graphs);without_collision.evaluate(snapshot.values,snapshot.poses);}
      auto vec=[](ir::Vec3 v) {return J::array({v.x,v.y,v.z});};
      auto vertices=[&](const std::vector<ir::Vec3> &positions,const ir::Transform &matrix) {J a=J::array();for(auto v:positions) a.push_back(vec(matrix.point(v)));return a;};
      auto pose=[&](const runtime::JointPose &p) {return J{{"translation",vec(p.translation_cm)},{"rotation",vec(p.rotation_degrees)},{"scale",vec(p.scale)},{"general_scale",p.general_scale},{"center_offset",vec(p.center_offset_cm)},{"orientation_offset",vec(p.orientation_offset_degrees)}};};
      const auto repeated=runtime.evaluate(snapshot.values,snapshot.poses);
      require(repeated.meshes.empty()&&repeated.instances.empty(),"相同几何诊断输入产生重复更新");
      J output={{"input",document.loaded.report["input"]},{"units","meters_z_up"},{"collision_evaluations",runtime.collision_stats().evaluations},{"collision_corrected_vertices",runtime.collision_stats().corrected_vertices},{"objects",J::array()}};
      for(size_t t=0;t<document.catalog.targets.size();++t) {
        const auto &target=document.catalog.targets[t];const auto &instance=scene.instances[target.instance];const auto &base=document.loaded.scene.meshes[document.loaded.scene.instances[target.instance].mesh];
        J item={{"id",target.id},{"label",target.label},{"transform",instance.transform.value},{"base",vertices(base.positions,{})},{"world",vertices(scene.meshes[instance.mesh].positions,instance.transform)},{"channels",J::array()},{"bones",J::array()}};
        item["triangles"]=J::array();for(const auto &face:base.triangles) item["triangles"].push_back(face.vertices);
        item["collision_target"]=target.smoothing.collision_target;
        item["uncorrected_world"]=vertices(uncorrected.meshes[instance.mesh].positions,uncorrected.instances[target.instance].transform);
        auto morphed=base.positions;
        for(size_t m=0;m<target.morphs.size();++m) {const auto &morph=target.morphs[m];const auto effective=runtime.effective()[t][m];
          if(effective!=0||snapshot.values[t].morphs[m]!=0) item["channels"].push_back({{"id",morph.id},{"name",morph.channel_id},{"initial",snapshot.values[t].morphs[m]},{"effective",effective},{"error",morph.unsupported},{"offsets",morph.offset_count()}});
          if(effective!=0) for(const auto &offset:morph.data()) {auto &v=morphed[offset.vertex];v.x+=effective*offset.delta.x;v.y+=effective*offset.delta.y;v.z+=effective*offset.delta.z;}}
        item["morphed"]=vertices(morphed,{});
        const auto s=document.formulas.graphs[t].skin;if(s>=0) {const auto &skin=document.skeletons.skins[size_t(s)];
          for(size_t j=0;j<skin.joints.size();++j) {const auto &bone=skin.joints[j];item["bones"].push_back({{"id",bone.id},{"name",bone.name},{"parent",bone.parent},{"center",vec(bone.center_cm)},{"orientation",vec(bone.orientation_degrees)},{"initial",pose(skin.initial[j])},{"effective",pose(runtime.effective_poses()[size_t(s)][j])}});}}
        output["objects"].push_back(std::move(item));
      }
      const std::filesystem::path destination=argv[3];if(!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path());
      std::ofstream stream(destination);stream<<output.dump();if(!stream) throw std::runtime_error("几何诊断写入失败");std::cout<<"Geometry diagnostic exported"<<std::endl;return 0;
    }
    if(argc==4&&std::wstring(argv[1])==L"--skeleton") {auto loaded=daz::load(argv[2],{roots,false});auto skins=daz::load_skeletons(loaded);std::ofstream(std::filesystem::path(argv[3]))<<skins.report.dump(2);std::cout<<"Skeletons: "<<skins.skins.size()<<std::endl;return 0;}
    if(argc==4&&std::wstring(argv[1])==L"--bindings") {
      auto loaded=daz::load(argv[2],{roots,false});auto skins=daz::load_skeletons(loaded);std::vector<runtime::Target> targets;std::vector<runtime::FormulaGraph> graphs;
      for(const auto &object:loaded.objects) {runtime::Target t;t.instance=object.instance;t.id=loaded.scene.instances[t.instance].id;t.conform_target=object.conform_target;targets.push_back(t);runtime::FormulaGraph g;
        for(size_t s=0;s<skins.skins.size();++s) if(skins.skins[s].instance==t.instance) g.skin=int(s);graphs.push_back(g);}
      runtime::ConformRuntime bindings(loaded.scene,targets,skins.skins,graphs);auto report=skins.report;report["conform_links"]=bindings.links().size();report["bound_vertices"]=bindings.stats().bindings;report["status"]="PASS";
      std::ofstream(std::filesystem::path(argv[3]))<<report.dump(2);std::cout<<"Skeleton bindings: PASS ("<<bindings.links().size()<<" links)"<<std::endl;return 0;
    }
    editor::Document document;const auto start=std::chrono::steady_clock::now();nlohmann::json times;
    auto phase=[&](const char *name) {times[name]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::cout<<name<<": "<<times[name]<<std::endl;};
    document.loaded=daz::load(argv[1],{roots,false});phase("geometry");document.catalog=daz::discover_morphs(document.loaded,roots,[](const auto &message) {std::cout<<message<<std::endl;});phase("morphs");
    document.skeletons=daz::load_skeletons(document.loaded);phase("skeletons");document.formulas=daz::enable_formulas(document.catalog,document.skeletons);phase("formulas");
    auto snapshot=editor::initial_snapshot(document);auto scene=document.loaded.scene;
    runtime::DeformationRuntime runtime(scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);phase("evaluation");
    nlohmann::json report={{"status","PASS"},{"elapsed",times},{"instances",scene.instances.size()},{"skins",document.skeletons.skins.size()},{"asset",document.loaded.report},{"skeleton",document.skeletons.report}};
    if(argc>3) {const auto &skin=document.skeletons.skins.at(0);size_t target=0;while(document.catalog.targets.at(target).instance!=skin.instance) ++target;
      const auto applied=daz::apply_pose(daz::read_pose(argv[3]),skin,snapshot.poses[0],document.catalog.targets[target],snapshot.values[target]);
      std::vector<std::vector<ir::Vec3>> before;for(const auto &mesh:scene.meshes) before.push_back(mesh.positions);
      snapshot.poses[0]=applied.joints;snapshot.values[target]=applied.properties;runtime.evaluate(snapshot.values,snapshot.poses);report["preset"]=applied.report;
      const auto edited_mesh=scene.instances[skin.instance].mesh;double displacement=0;std::set<size_t> affected{target};
      for(size_t pass=0;pass<document.catalog.targets.size();++pass) for(const auto &link:runtime.conform_links()) if(affected.contains(link.source)) affected.insert(link.follower);
      std::set<uint32_t> affected_meshes;for(auto t:affected) affected_meshes.insert(scene.instances[document.catalog.targets[t].instance].mesh);
      for(size_t m=0;m<scene.meshes.size();++m) for(size_t v=0;v<before[m].size();++v) {const auto a=before[m][v],b=scene.meshes[m].positions[v];const auto distance=std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z);
        if(!affected_meshes.contains(uint32_t(m))) require(distance==0,"预设污染无关角色或穿戴物");if(m==edited_mesh) displacement=std::max(displacement,double(distance));}
      require(displacement>0,"真实预设未产生形变");report["preset_displacement_l1_m"]=displacement;report["unrelated_meshes_unchanged"]=true;report["affected_targets"]=affected;phase("preset");}
    if(argc>2) {std::filesystem::create_directories(std::filesystem::path(argv[2]).parent_path());std::ofstream(std::filesystem::path(argv[2]))<<report.dump(2);}
    std::cout<<"Scene workflow: PASS\n";return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<std::endl;return 1;}
}
