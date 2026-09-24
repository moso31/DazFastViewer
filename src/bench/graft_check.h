#pragma once
#include "bench/graft_fixture.h"
#include "bench/output.h"
#include "cycles/adapter.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/mesh.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include <fstream>

namespace dfv {
inline int graft_check(const ccl::DeviceInfo &device,const std::filesystem::path &output) {
  using namespace ccl;using J=nlohmann::json;std::filesystem::create_directories(output);
  auto require=[](bool v,const char *why) {if(!v) throw std::runtime_error(why);};
  auto source=bench::graft_fixture();
  for(uint32_t p=0;p<2;++p) {auto i=source.instances[p];i.id+="-copy";i.prototype=int(p);i.graft_source=-1;i.instance_group="copy";i.transform=ir::Transform::translate({8,0,0})*i.transform;source.instances.push_back(i);}
  auto unrelated=source.meshes[0];unrelated.id="unrelated";unrelated.hidden_polygons.clear();source.meshes.push_back(unrelated);
  auto other=source.instances[0];other.id="unrelated";other.mesh=2;other.transform=ir::Transform::translate({0,6,0})*other.transform;source.instances.push_back(other);
  CameraState camera;camera.target={6,3,0};camera.distance=25;camera.pitch=.5f;source.camera=render_camera(camera,160,120);ir::add_studio(source);
  const auto initial=source;
  SessionParams params;params.device=device;params.background=params.headless=true;params.samples=4;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  SceneParams sp;sp.background=true;sp.bvh_type=BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;
  Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);scene.integrator->set_use_denoise(false);
  CyclesAdapter adapter(scene);adapter.load(source);BufferParams buffers;buffers.width=buffers.full_width=160;buffers.height=buffers.full_height=120;
  auto mesh_named=[&](const std::string &name)->Mesh * {for(auto *o:scene.objects) if(o->name.string()==name&&o->get_geometry()->is_mesh()) return static_cast<Mesh *>(o->get_geometry());return nullptr;};
  Mesh *unrelated_mesh=mesh_named("unrelated");require(unrelated_mesh,"缺少无关对象");
  J checks=J::array();
  auto run=[&](const char *name) {
    const auto folder=output/name;std::filesystem::create_directories(folder);auto result=std::make_unique<Output>(folder);auto *written=result.get();session.set_output_driver(std::move(result));
    session.reset(params,buffers);session.start();session.wait();require(!session.progress.get_error(),session.progress.get_error_message().c_str());require(written->written&&written->error.empty(),"GeoGraft GPU 图像未输出");
    float maximum=0,normal_error=0;size_t count=0;
    for(const auto &group:runtime::graft_groups(source)) {
      runtime::GraftSurface expected(source,group,false);auto *mesh=mesh_named(source.instances[group[0]].id+"/graft/original");require(mesh,"GeoGraft 没有合并为单个渲染对象");
      require(mesh->num_verts()==expected.positions().size(),"GPU 共同细分顶点数量错误");
      const auto *n=mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);require(n,"GPU 丢失共同法线");
      for(size_t v=0;v<expected.positions().size();++v) {const auto a=mesh->get_position()[v];const auto b=expected.positions()[v];maximum=std::max(maximum,std::hypot(a.x-b.x,a.y-b.y,a.z-b.z));const auto en=expected.normals()[v];if(std::hypot(en.x,en.y,en.z)>.5f) {const auto actual=n->data<packed_normal>()[v].decode();normal_error=std::max(normal_error,std::hypot(actual.x-en.x,actual.y-en.y,actual.z-en.z));}}
      size_t face=0,offset=0;const auto *uv=mesh->attributes.find(ATTR_STD_UV)->data<float2>();
      for(size_t p=0;p<group.size();++p) {const auto &object=source.instances[group[p]];if(object.visible) for(const auto &t:expected.triangles(p)) {require(mesh->get_shader()[face]==int(offset+t.material_slot),"共同网格材质槽映射错误");for(size_t c=0;c<3;++c) require(std::abs(uv[face*3+c].x-t.uv[c].x)<1e-6&&std::abs(uv[face*3+c].y-t.uv[c].y)<1e-6,"共同网格面角 UV 丢失");++face;}offset+=object.materials.size();}
      require(face==mesh->num_triangles(),"附件显隐后的面数量错误");count+=face;
    }
    require(maximum<2e-5&&normal_error<.0002f,"GPU 几何／法线与共同曲面不一致");require(mesh_named("unrelated")==unrelated_mesh,"附件编辑重建了无关对象");
    checks.push_back({{"stage",name},{"max_position_error_m",maximum},{"max_normal_error",normal_error},{"triangles",count}});
    std::ofstream(output/"graft-check.json")<<J({{"status","RUNNING"},{"checks",checks}}).dump(2);
  };
  auto apply=[&](const ir::Delta &d) {thread_scoped_lock lock(scene.mutex);adapter.apply(d);for(auto &e:d.meshes) source.meshes[e.index].positions=e.positions;for(auto &e:d.instances) source.instances[e.index].transform=e.transform;for(auto &e:d.visibility) source.instances[e.index].visible=e.visible;for(auto &e:d.materials) source.materials[e.index]=e.value;};
  auto sync=[&](ir::Scene next) {thread_scoped_lock lock(scene.mutex);const bool changed=adapter.synchronize(next);source=std::move(next);return changed;};
  run("initial");require(mesh_named("body/graft/original")==mesh_named("body/graft/copy"),"同形同材质实例没有共享组合网格");require(!sync(source),"静止快照重复提交组合网格");
  ir::Delta morph;auto body=source.meshes[0].positions,patch=source.meshes[1].positions;body[5].z+=.12f;patch[4].z+=.17f;morph.meshes={{0,body},{1,patch}};apply(morph);run("host-and-graft-morph");
  ir::Delta hide;hide.visibility={{1,false}};apply(hide);run("hide-only-original-graft");require(mesh_named("body/graft/original")!=mesh_named("body/graft/copy"),"显隐污染了共享实例");
  ir::Delta show_and_morph;show_and_morph.visibility={{1,true}};body[6].z-=.1f;show_and_morph.meshes={{0,body}};apply(show_and_morph);run("visibility-with-morph");require(mesh_named("body/graft/original")==mesh_named("body/graft/copy"),"恢复显隐未复用共同几何");
  ir::Delta scatter;for(uint32_t i=0;i<2;++i) {auto m=source.materials[i];m.subsurface=.35f;m.base_color={.7f,.4f,.3f};scatter.materials.push_back({i,m});}apply(scatter);run("sss-material-edit");
  for(int level:{0,1,2}) {auto next=source;next.meshes[0].subdivision.enabled=level>0;next.meshes[0].subdivision.level=next.meshes[0].subdivision.render_level=level;sync(next);run(("subdivision-"+std::to_string(level)).c_str());}
  ir::Delta move;for(uint32_t i=0;i<2;++i) move.instances.push_back({i,ir::Transform::translate({1,0,0})*source.instances[i].transform});apply(move);run("move-whole-figure");
  auto invalid=source;invalid.meshes[0].subdivision.level=7;bool rejected=false;try {thread_scoped_lock lock(scene.mutex);adapter.synchronize(invalid);}catch(...) {rejected=true;}require(rejected,"非法细分没有拒绝");run("reject-invalid-level");
  auto detached=initial;detached.instances[1].graft_source=-1;detached.meshes[0].hidden_polygons.clear();sync(detached);run("detach");require(mesh_named("body")&&mesh_named("graft"),"解除后未恢复独立对象");
  sync(initial);run("reattach");
  auto removed=initial;removed.instances.erase(removed.instances.begin()+2,removed.instances.begin()+4);removed.instances.erase(removed.instances.begin()+1);removed.meshes[0].hidden_polygons.clear();sync(removed);run("delete-graft-and-copies");require(!mesh_named("body/graft/original"),"删除后残留组合对象");
  sync(initial);run("restore-all");
  std::ofstream(output/"graft-check.json")<<J({{"status","PASS"},{"checks",checks},{"device",device.id}}).dump(2);std::cout<<"GeoGraft GPU checks: PASS ("<<checks.size()<<" stages)\n";return 0;
}
}
