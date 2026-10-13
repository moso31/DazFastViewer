#include "editor/scene_extension.h"
#include "editor/presets.h"
#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "bench/output.h"
#include "bench/fixtures.h"
#include "scene/scene.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <fstream>
#include <iostream>
#include <thread>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
int main(int argc,char **argv){try{
  if(argc<3)throw std::runtime_error("用法：ToenailProbe 场景.dufex 输出目录 [Jasmine.duf]");
  const auto output=fs::absolute(fs::u8path(argv[2]));fs::create_directories(output);
  if(argc>3&&std::string(argv[3])=="--uv-only"){
    auto loaded=daz::load(fs::u8path(argv[1]),{{L"H:/G1",L"H:/G3",L"C:/Users/Public/Documents/My DAZ 3D Library"},false});J report;
    for(const auto &mesh:loaded.scene.meshes){const auto slot=std::find(mesh.material_slots.begin(),mesh.material_slots.end(),"Toenails");if(slot==mesh.material_slots.end())continue;
      runtime::Subdivision sub(mesh);report={{"id",mesh.id},{"level",mesh.subdivision.level},{"nails",J::array()}};
      for(const auto &t:sub.active()?sub.triangles():mesh.triangles)if(t.material_slot==size_t(slot-mesh.material_slots.begin()))report["nails"].push_back({{"cage",t.source_polygon},{"uv",{{t.uv[0].x,t.uv[0].y},{t.uv[1].x,t.uv[1].y},{t.uv[2].x,t.uv[2].y}}}});break;
    }
    std::ofstream(output/"dfv-uv-check.json")<<report.dump();return 0;
  }
  const bool scene_view=argc>3&&std::string(argv[3])=="--scene-view";const int width=scene_view?1280:768,height=scene_view?740:576;
  auto loaded=editor::load_scene_extension(fs::u8path(argv[1]),{L"H:/G1",L"H:/G3"},1,[](const auto &s){std::cout<<s<<std::endl;});auto &d=*loaded.document;auto &s=loaded.snapshot;
  size_t target=d.catalog.targets.size();for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].label=="Seo Hyun G8.1F (2)")target=t;
  if(target==d.catalog.targets.size())throw std::runtime_error("目标不存在");const auto instance=d.catalog.targets[target].instance;
  editor::save_preset(output/"character-material.duf",d,s,target,editor::PresetKind::material);
  editor::save_preset(output/"character-pose.duf",d,s,target,editor::PresetKind::pose);
  editor::save_preset(output/"character-morph.duf",d,s,target,editor::PresetKind::morph);
  auto source=d.loaded.scene;runtime::DeformationRuntime deformation(source,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);deformation.defer_collision();
  for(;;){const auto p=deformation.prepare(s.values,s.poses);if(!p.error.empty())throw std::runtime_error(p.error);if(!p.pending)break;std::this_thread::sleep_for(std::chrono::milliseconds(25));}
  deformation.evaluate(s.values,s.poses);editor::apply_material_overrides(source,d.loaded.scene,s.material_overrides);editor::apply_subdivision_levels(source,s.subdivision_levels);
  auto mesh=source.meshes.at(source.instances.at(instance).mesh);auto object=source.instances.at(instance);if(!scene_view)object.transform={};object.mesh=0;object.visible=true;object.graft_source=object.shell_source=object.prototype=-1;
  source.instances={object};source.meshes={mesh};if(scene_view){source.lights=s.lights;source.options=s.options;}else{source.lights.clear();source.environment={.7f,.7f,.7f};source.options={};}
  auto compact=[&]{editor::Document scratch;scratch.loaded.scene=std::move(source);editor::collect_resources(scratch);source=std::move(scratch.loaded.scene);};compact();
  size_t nails=0;for(size_t slot=0;slot<mesh.material_slots.size();++slot)if(mesh.material_slots[slot]=="Toenails")nails=slot;
  ir::Bounds bounds;for(const auto &t:mesh.triangles)if(t.material_slot==nails)for(auto v:t.vertices)if(mesh.positions[v].x>0)bounds.add(mesh.positions[v]);
  const auto center=bounds.center();CameraState camera;camera.target={center.x,center.y,center.z};camera.distance=.12f;camera.yaw=0;camera.pitch=1.1f;
  if(scene_view&&s.view){const auto &v=*s.view;camera.target={v[0],v[1],v[2]};camera.distance=v[3];camera.yaw=v[4];camera.pitch=v[5];}source.camera=render_camera(camera,width,height);
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);if(devices.empty())throw std::runtime_error("OptiX unavailable");
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=scene_view?256:32;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);
  CyclesAdapter adapter(scene);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=width;buffers.height=buffers.full_height=height;
  J report={{"target",d.catalog.targets[target].id},{"center",{center.x,center.y,center.z}},{"stages",J::array()}};
  auto render=[&](const char *name){
    {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}const auto folder=output/name;fs::create_directories(folder);auto driver=std::make_unique<Output>(folder,false,source.options);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();if(session.progress.get_error()||!result->written||!result->error.empty())throw std::runtime_error("Render failed: "+session.progress.get_error_message());
    J values=J::object();const auto &m=source.materials[source.instances[0].materials[nails]];for(const auto &p:editor::material_parameters())values[p.id]=editor::material_value(m,source.textures,p);report["stages"].push_back({{"name",name},{"nails",values}});std::ofstream(output/"report.json")<<report.dump(2);std::cout<<name<<std::endl;
  };
  render("saved");const auto original=source.materials;
  for(auto &m:source.materials)m.normal_texture=-1;render("no-normal");
  for(auto &m:source.materials)m.bump_texture=-1;render("no-normal-bump");source.materials=original;
  if(scene_view){for(auto &m:source.materials)m.bump_texture=-1;render("no-bump");source.materials=original;for(auto &m:source.materials)m.normal_strength*=.25f;render("quarter-normal");source.materials=original;}
  if(argc>3&&!scene_view){const auto preset=daz::load(fs::u8path(argv[3]),{{L"H:/G1",L"H:/G3"},false});editor::apply_materials(d,target,preset,&s);auto full=d.loaded.scene;editor::apply_material_overrides(full,d.loaded.scene,s.material_overrides);
    auto object=full.instances.at(instance);auto updated=full.meshes.at(object.mesh);updated.positions=mesh.positions;updated.subdivision=mesh.subdivision;source.materials=full.materials;source.textures=full.textures;source.meshes={updated};object.mesh=0;object.transform={};object.visible=true;object.prototype=object.graft_source=object.shell_source=-1;source.instances={object};compact();render("jasmine");
  }
  return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
