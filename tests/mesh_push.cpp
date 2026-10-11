#include "editor/scene_extension.h"
#include "runtime/mesh_push.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>

using namespace dfv;
using J=nlohmann::json;
namespace fs=std::filesystem;
static void check(bool v,const char *reason){if(!v)throw std::runtime_error(reason);}
static bool near(float a,float b){return std::abs(a-b)<1e-6f;}
static void runtime_cases() {
  ir::Mesh plane;plane.positions={{0,0,0},{1,0,0},{0,1,0}};ir::Triangle f;f.vertices={0,1,2};plane.triangles={f};
  auto split=plane;split.positions.push_back(plane.positions[0]);f.vertices={3,1,2};split.triangles.push_back(f);
  auto points=split.positions;runtime::apply_mesh_push(split,points,{{.002f,false,{}}},false);check(points[0]==points[3]&&near(points[0].z,.002f),"Push 外扩撕开同位置拆点");
  points=plane.positions;runtime::apply_mesh_push(plane,points,{{-.004f,false,{1,.5f,0}}},false);check(near(points[0].z,-.004f)&&near(points[1].z,-.002f)&&points[2]==plane.positions[2],"Push 负值或权重图错误");
  {
    ir::Scene scene;auto cloth=plane;for(auto &p:cloth.positions)p.z=-.01f;scene.meshes={plane,cloth};scene.instances.resize(2);scene.instances[0].id="body/mesh";scene.instances[1].id="cloth/mesh";scene.instances[1].mesh=1;
    runtime::Target body;body.id="body/mesh";runtime::Target garment;garment.id="cloth/mesh";garment.instance=1;garment.smoothing.enabled=true;garment.smoothing.collision_target="#body";std::vector<runtime::Target> targets{body,garment};
    runtime::CollisionRuntime first(scene,targets);first.evaluate({});runtime::CollisionRuntime rebuilt(scene,targets);rebuilt.reuse(first);
    check(rebuilt.evaluate({}).meshes.empty(),"重建碰撞时重复修正了未修改的服装");
    for(auto &p:scene.meshes[0].positions)p.z=-.5f;ir::Delta edit;edit.meshes.push_back({0,scene.meshes[0].positions});rebuilt.evaluate(edit);
    check(scene.meshes[1].positions==cloth.positions,"重建碰撞丢失原始输入，身体移开后衣物不能恢复");
  }
  for(bool dynamic:{false,true})for(bool post:{false,true}) {
    editor::Document d;auto &scene=d.loaded.scene;auto cloth=plane;for(auto &p:cloth.positions)p.z=-.001f;scene.meshes={plane,cloth};scene.instances.resize(2);scene.instances[0].mesh=0;scene.instances[1].mesh=1;
    runtime::Target body;body.id="body/mesh";runtime::Target garment;garment.id="cloth/mesh";garment.instance=1;garment.conform_target="#body";garment.dynamic_cloth=dynamic;
    garment.push_modifiers={{.002f,post,{}}};if(!dynamic){garment.smoothing.enabled=true;garment.smoothing.collision_target="#body";}
    d.catalog.targets={body,garment};
    for(uint32_t i=0;i<2;++i){runtime::Skin skin;skin.instance=i;runtime::Joint root;root.id="root";skin.joints={root};skin.initial.resize(1);skin.weights.resize(3,{{0,1}});d.skeletons.skins.push_back(skin);runtime::FormulaGraph graph;graph.skin=int(i);graph.prepare();d.formulas.graphs.push_back(graph);}
    auto state=editor::initial_snapshot(d);runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(state.values,state.poses);
    const auto initial=scene.meshes[1].positions;check(near(initial[0].z,post?.0025f:.001f),"Push 未按平滑前后顺序执行，或动态衣物没有自动避让");
    check(runtime.evaluate(state.values,state.poses).meshes.empty()&&scene.meshes[1].positions==initial,"重复求值累加 Push 或无效更新");
    state.values[0].transform.translation_cm.x=25;runtime.evaluate(state.values,state.poses);check(scene.meshes[1].positions==initial,"共同实例移动改变了 Push 距离");
    state.poses[0][0].translation_cm.y=3;runtime.evaluate(state.values,state.poses);check(scene.meshes[1].positions!=initial,"姿势更新没有重算 Push");
    state.poses[0][0].translation_cm.y=0;runtime.evaluate(state.values,state.poses);check(scene.meshes[1].positions==initial,"姿势复原后 Push 漂移");
    if(dynamic)check(state.values[1].disabled_deformation_regions.empty(),"静态避让依赖手动关闭形变");
  }
}
static void import_cases() {
  const auto folder=fs::temp_directory_path()/"dfv-push-tests"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(folder);
  auto source=J::parse(R"({
    "geometry_library":[{"id":"mesh","vertices":{"count":3,"values":[[0,0,0],[100,0,0],[0,0,-100]]},"polygon_material_groups":{"count":1,"values":["surface"]},"polylist":{"count":1,"values":[[0,0,0,1,2]]},"default_uv_set":"#uv"}],
    "uv_set_library":[{"id":"uv","vertex_count":3,"uvs":{"count":3,"values":[[0,0],[1,0],[0,1]]}}],
    "modifier_library":[{"id":"push","extra":[{"type":"studio/modifier/push","push_post_smooth":true},{"type":"studio_modifier_channels","channels":[{"channel":{"id":"Value","value":0,"current_value":0.2371787}}]}]},{"id":"top-push","channel":{"id":"Value","value":0.4},"extra":[{"type":"studio/modifier/push"}]},{"id":"dynamic","extra":[{"type":"studio/modifier/dynamic_simulation"}]}],
    "scene":{"nodes":[{"id":"cloth","presentation":{"type":"Follower/Wardrobe"},"geometries":[{"id":"shape","url":"#mesh"}]},{"id":"other","geometries":[{"id":"shape2","url":"#mesh"}]}],"materials":[{"id":"material","geometry":"#shape","groups":["surface"]},{"id":"material2","geometry":"#shape2","groups":["surface"]}],"modifiers":[{"id":"push1","url":"#push","parent":"#cloth","extra":[{"type":"studio/modifier/push"}]},{"id":"push2","url":"#top-push","parent":"#shape2","channel":{"current_value":-0.2}},{"id":"sim","url":"#dynamic","parent":"#cloth"}]}
  })");
  const auto file=folder/"push.duf";std::ofstream(file)<<source.dump();editor::Document d;d.source_file=file;d.loaded=daz::load(file,{{folder}});
  check(d.loaded.objects[0].push_modifiers.size()==1&&near(d.loaded.objects[0].push_modifiers[0].offset_m,.002371787f)&&d.loaded.objects[0].push_modifiers[0].post_smooth,"内嵌 Push 的保存值、单位或顺序丢失");
  check(d.loaded.objects[1].push_modifiers.size()==1&&near(d.loaded.objects[1].push_modifiers[0].offset_m,-.002f),"顶层通道覆盖或几何父引用错误");
  check(d.loaded.objects[0].dynamic_cloth&&!d.loaded.objects[1].dynamic_cloth,"dForce 服装标记污染其他实例");
  d.catalog=daz::discover_morphs(d.loaded,{folder},{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);
  auto snapshot=editor::initial_snapshot(d);const auto saved=folder/"push.dufex";editor::save_scene_extension(saved,d,snapshot);fs::remove(file);const auto restored=editor::load_scene_extension(saved,{folder},7);
  check(restored.document->catalog.targets[0].push_modifiers[0].offset_m==d.catalog.targets[0].push_modifiers[0].offset_m&&restored.document->catalog.targets[0].dynamic_cloth,"DUFEX 重开丢失 Push 或 dForce 设置");
}
int main(){try{runtime_cases();import_cases();std::cout<<"Push import / stage / weights / seams / pose / cache / dForce collision / DUFEX: PASS\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
