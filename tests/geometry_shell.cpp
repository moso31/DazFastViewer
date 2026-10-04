#include "editor/document.h"
#include "editor/object_hierarchy.h"
#include "runtime/geometry_shell.h"
#include "daz/documents.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <cmath>
using namespace dfv;
using J=nlohmann::json;namespace fs=std::filesystem;
static void require(bool value,const char *reason){if(!value)throw std::runtime_error(reason);}
static bool near(ir::Vec3 a,ir::Vec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-6;}
int main(){try{
  const auto folder=fs::temp_directory_path()/"dfv-shell-tests"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(folder/"data");
  auto document=J::parse(R"({
    "geometry_library":[
      {"id":"host","vertices":{"count":4,"values":[[0,0,0],[100,0,0],[100,0,-100],[0,0,-100]]},
       "polygon_groups":{"count":2,"values":["keep","hide"]},"polygon_material_groups":{"count":1,"values":["surface"]},
       "polylist":{"count":2,"values":[[0,0,0,1,2],[1,0,0,2,3]]},"default_uv_set":"#uv"},
      {"id":"shell","vertices":{"count":0,"values":[]},"polygon_material_groups":{"count":0,"values":[]},"polylist":{"count":0,"values":[]},"extra":[{"type":"studio/geometry/shell"}]}],
    "uv_set_library":[{"id":"uv","vertex_count":4,"uvs":{"count":4,"values":[[0,0],[1,0],[1,1],[0,1]]}}],
    "modifier_library":[{"id":"offset","channel":{"id":"offset","type":"float","value":0.1},"extra":[{"type":"studio/modifier/push"}]}],
    "scene":{"nodes":[
      {"id":"overlay","parent":"#body","geometries":[{"id":"shell-shape","url":"#shell"}],"extra":[{"type":"studio/node/shell"},{"type":"studio_node_channels","channels":[{"channel":{"id":"Shell Node","node":"#body"}},{"channel":{"id":"facet_group_hide_vis","value":false}}]}]},
      {"id":"body","geometries":[{"id":"body-shape","url":"#host"}]}],
      "materials":[{"id":"body-material","geometry":"#body-shape","groups":["surface"],"diffuse":{"channel":{"value":[1,0,0]}}},
                   {"id":"shell-material","geometry":"#shell-shape","groups":["surface"],"diffuse":{"channel":{"value":[0,0,1]}}}],
      "modifiers":[{"id":"saved-offset","url":"#offset","parent":"#overlay","channel":{"current_value":0.2}}]}
  })");
  const auto file=folder/"shell.duf";std::ofstream(file)<<document.dump();
  editor::Document d;d.loaded=daz::load(file,{{folder}});require(d.loaded.objects.size()==2,"Shell 没有重建为独立物体");
  const auto &shell=d.loaded.scene.instances[1];const auto &mesh=d.loaded.scene.meshes[shell.mesh];
  require(shell.shell_source==0&&std::abs(shell.shell_offset-.002f)<1e-7,"Shell 宿主或保存的厘米偏移未应用");
  require(mesh.hidden_polygons==std::vector<uint32_t>{1},"Shell 面组显隐丢失");
  require(d.loaded.scene.materials[shell.materials[0]].base_color.z==1,"Shell 材质被宿主覆盖");
  require(near(mesh.positions[0],{0,0,.002f})&&mesh.triangles[0].uv[1]==ir::Vec2{1,0},"Shell 偏移或 UV 错误");
  {auto alternate=document;alternate["modifier_library"][0].erase("channel");alternate["modifier_library"][0]["extra"].push_back({{"type","studio_modifier_channels"},{"channels",J::array({{{"channel",{{"id","Value"},{"current_value",.015}}}}})}});
    alternate["scene"]["modifiers"][0].erase("channel");const auto path=folder/"channel-offset.duf";std::ofstream(path)<<alternate.dump();const auto loaded=daz::load(path,{{folder}});
    require(std::abs(loaded.scene.instances[1].shell_offset-.00015f)<1e-8,"Studio Modifier Channels 中的偏移未导入");}
  d.catalog=daz::discover_morphs(d.loaded,{folder},{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);
  require(d.catalog.targets[1].morphs.empty(),"Shell 不应再次独立应用宿主 Morph");
  {auto parts=d;auto instance=parts.loaded.scene.instances[1];instance.id+="-part-1";parts.loaded.scene.instances.push_back(instance);auto object=parts.loaded.objects[1];object.instance=2;parts.loaded.objects.push_back(object);auto target=parts.catalog.targets[1];target.instance=2;target.id=instance.id;parts.catalog.targets.push_back(target);
    const editor::ObjectTargets grouped(parts);require(grouped.primary==std::vector<size_t>{0,1,1}&&grouped.members[1]==std::vector<size_t>{1,2},"同一 Shell 的渲染分片未合并");
    auto snapshot=editor::initial_snapshot(parts);snapshot.values[1].transform.translation_cm.x=12;editor::sync_object_transform(parts,snapshot,1);require(snapshot.values[2].transform==snapshot.values[1].transform&&snapshot.values[0].transform.translation_cm.x==0,"Shell 变换没有覆盖全部分片或影响宿主");
    parts.loaded.objects.back().id="another-shell";require(editor::ObjectTargets(parts).primary[2]==2,"不同身份但同名 Shell 被错误合并");}
  auto scene=d.loaded.scene;scene.meshes[scene.instances[0].mesh].positions[0].z=1;ir::Delta input;input.meshes.push_back({scene.instances[0].mesh,scene.meshes[scene.instances[0].mesh].positions});
  auto delta=runtime::update_geometry_shells(scene,input);require(delta.meshes.size()==2&&scene.meshes[shell.mesh].positions[0].z>.99,"宿主形变未传给 Shell");
  require(runtime::update_geometry_shells(scene).meshes.empty(),"静止 Shell 重复求值");
  {auto moved=d.loaded.scene;moved.instances[1].transform=ir::Transform::translate({2,0,0});ir::Delta edit;edit.instances.push_back({1,moved.instances[1].transform});runtime::update_geometry_shells(moved,edit);
    require(near(moved.instances[1].transform.point(moved.meshes[shell.mesh].positions[0]),{2,0,.002f}),"Shell 自身变换被跟随逻辑抵消");}
  {auto empty=d.loaded.scene;ir::Mesh points;points.id="container-points";points.positions={{0,0,0},{1,0,0}};ir::Instance instance;instance.id="container";instance.mesh=uint32_t(empty.meshes.size());empty.meshes.push_back(points);empty.instances.push_back(instance);empty.validate();
    empty.meshes.back().triangles.push_back({});bool invalid=false;try{empty.validate();}catch(...){invalid=true;}require(invalid,"有面但无材质槽的损坏网格被误接受");}
  scene.instances[0].shell_source=1;bool rejected=false;try{runtime::update_geometry_shells(scene,{},true);}catch(...){rejected=true;}require(rejected,"Shell 循环没有被拒绝");
  editor::Document combined;editor::append_document(combined,d,"a/");editor::append_document(combined,d,"b/");combined.loaded.scene.validate();
  require(combined.loaded.scene.instances[3].shell_source==2,"追加场景没有重映射 Shell 宿主");
  auto snapshot=editor::initial_snapshot(combined);editor::remove_target(combined,snapshot,0);combined.loaded.scene.validate();require(combined.loaded.scene.instances.size()==2&&combined.loaded.scene.instances[1].shell_source==0,"删除宿主未清理其 Shell 或污染另一个角色");
  {auto parts=d;auto &s=parts.loaded.scene;
    for(size_t i=0;i<2;++i){auto object=parts.loaded.objects[i];auto instance=s.instances[i];auto mesh=s.meshes[instance.mesh];auto target=parts.catalog.targets[i];
      const auto index=uint32_t(s.instances.size());const auto name=i?"overlay/extra-part":"graft/shape";mesh.id=name;instance.id=name;instance.mesh=uint32_t(s.meshes.size());object.instance=index;target.instance=index;target.id=name;
      if(i){instance.shell_source=2;instance.shell_root=0;}else{object.id="graft";object.parent="#body";target.parent="#body";}
      s.meshes.push_back(mesh);s.instances.push_back(instance);parts.loaded.objects.push_back(object);parts.catalog.targets.push_back(target);parts.formulas.graphs.push_back(parts.formulas.graphs[i]);
    }
    auto state=editor::initial_snapshot(parts);editor::remove_target(parts,state,2);s.validate();require(parts.loaded.objects.size()==2&&parts.catalog.targets.size()==2&&s.instances.size()==2,"删除插件后其 Shell 部件留下悬空对象引用");
  }
  auto batch=daz::prefetch_documents(std::vector<fs::path>{file,folder/"missing.dsf",file});require(batch[0].get()->contains("scene")&&batch[2].get()==batch[0].get(),"批量元数据结果或共享句柄错误");
  rejected=false;try{batch[1].get();}catch(...){rejected=true;}require(rejected,"批量解析吞掉了资源错误");
  document["asset_info"]={{"id","updated-source-version"}};std::ofstream(file)<<document.dump();require(daz::document_view(file)->at("asset_info").at("id")=="updated-source-version","共享解析文档没有检查资源版本");
  std::cout<<"Geometry Shell / offset / visibility / UV / deformation / append / removal / batch errors: PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
