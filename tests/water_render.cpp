#include "water_fixture.h"
#include "editor/scene_extension.h"
#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "bench/fixtures.h"
#include "bench/output.h"
#include "scene/scene.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <iostream>
#include <chrono>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/water/gpu");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");auto fixture=output/"columns.duf";water_fixture::write(fixture);auto d=water_fixture::load(fixture);
  auto w=std::make_shared<water::Water>();w->id="water-render";w->config.coast=true;w->config.scan_scene=false;w->config.wave_height=1.4;w->config.foam_width=1.2;w->config.precision=.4;
  for(const auto &o:d.loaded.objects)w->config.sources.push_back({o.id,true});water::install(d,w);auto snap=editor::initial_snapshot(d);w->cache=water::recalculate(d,snap,*w);water::install(d,w);
  auto source=d.loaded.scene;source.environment={.35f,.45f,.65f};ir::AreaLight sun;sun.id="sun";sun.kind=ir::LightKind::distant;sun.power={3,2.7f,2.3f};sun.angle=.025;sun.transform.value={1,0,0,0,0,.8f,.6f,0,0,-.6f,.8f,0};source.lights.push_back(sun);
  CameraState camera;camera.target={0,1,0};camera.distance=85;camera.yaw=.15f;camera.pitch=.45f;source.camera=render_camera(camera,800,520);const auto eye=camera.eye();water::adapt({w},source,{eye.x,eye.y,eye.z});
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=64;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;
  auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(8);
  CyclesAdapter adapter(scene,false,true);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=800;buffers.height=buffers.full_height=520;J records=J::array();
  auto render=[&](const char *name){const auto started=std::chrono::steady_clock::now();auto folder=output/name;fs::create_directories(folder);{ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}auto image=std::make_unique<Output>(folder);auto *result=image.get();session.set_output_driver(std::move(image));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Water render output failed");double luminance=0;size_t white=0;for(size_t i=0;i<result->linear_pixels.size();i+=4){float r=result->linear_pixels[i],g=result->linear_pixels[i+1],b=result->linear_pixels[i+2];luminance+=r*.2126+g*.7152+b*.0722;white+=r>.95f&&g>.95f&&b>.95f;}records.push_back({{"stage",name},{"seconds_including_sync",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()},{"triangles",adapter.stats().triangles},{"mean_linear_luminance",luminance/(800*520)},{"white_fraction",double(white)/(800*520)},{"device_peak_bytes",session.stats.mem_peak}});std::ofstream(output/"checks.json")<<records.dump(2);std::cout<<records.back().dump()<<std::endl;};
  if(argc<=2){render("columns-near");camera.pitch=1.45f;source.camera=render_camera(camera,800,520);render("columns-top");
  camera.pitch=.18f;camera.distance=110;source.camera=render_camera(camera,800,520);render("columns-grazing");
  source.instances[0].transform.value[3]+=10;std::vector<water::Obstacle> obstacles;for(size_t i=0;i<3;++i)obstacles.push_back({source.instances[i].id,source.meshes[source.instances[i].mesh],source.instances[i].transform,true});w->cache=water::calculate(*w,obstacles,2);camera.pitch=.7f;source.camera=render_camera(camera,800,520);auto e=camera.eye();water::adapt({w},source,{e.x,e.y,e.z});render("columns-moved");
  w->config.time=6.5;w->cache=water::calculate(*w,obstacles,3);source.materials[source.instances.back().materials[0]]=water::material(*w);water::adapt({w},source,{e.x,e.y,e.z});render("time-6-5");
  camera.distance=90000;camera.pitch=.4f;source.camera=render_camera(camera,800,520);e=camera.eye();water::adapt({w},source,{e.x,e.y,e.z});render("ocean-far");
  }else{
    editor::Document real;real.source_file=fs::absolute(fs::u8path(argv[2]));const std::vector<fs::path> roots{"G:/G1","G:/G3"};real.loaded=daz::load(real.source_file,{roots});real.catalog=daz::discover_morphs(real.loaded,roots,{},true);real.skeletons=daz::load_skeletons(real.loaded);real.formulas=daz::enable_formulas(real.catalog,real.skeletons);
    source=real.loaded.scene;source.options={};source.environment={.35f,.45f,.65f};source.lights={sun};ir::Bounds islands,old_water;J geometry=J::array();int water_instance=-1,water_slot=-1;std::vector<water::Source> terrain;
    for(size_t i=0;i<source.instances.size();++i){const auto &v=source.instances[i];const auto &m=source.meshes[v.mesh];ir::Bounds b;for(auto p:m.positions)b.add(v.transform.point(p));geometry.push_back({{"id",v.id},{"label",v.instance_label},{"min",{b.minimum.x,b.minimum.y,b.minimum.z}},{"max",{b.maximum.x,b.maximum.y,b.maximum.z}},{"triangles",m.triangles.size()}});
      if(v.id.starts_with("Island ")&&v.id.find("Trees")==std::string::npos){terrain.push_back({v.id,false});islands.add(b.minimum);islands.add(b.maximum);}
      for(size_t slot=0;slot<m.material_slots.size();++slot)if(m.material_slots[slot]=="matWater"){water_instance=int(i);water_slot=int(slot);for(const auto &t:m.triangles)if(t.material_slot==slot)for(auto p:t.vertices)old_water.add(v.transform.point(m.positions[p]));}
    }
    std::ofstream(output/"island-geometry.json")<<geometry.dump(2);check(water_instance>=0&&!islands.empty,"Imported islands/water not found");
    camera.target={islands.center().x,islands.center().y,old_water.center().z};camera.distance=std::max(islands.maximum.x-islands.minimum.x,islands.maximum.y-islands.minimum.y)*1.15f;camera.pitch=1.45f;source.camera=render_camera(camera,800,520);render("islands-old-top");
    auto native=std::make_shared<water::Water>();native->id="native-islands-water";auto &c=native->config;c.width=old_water.maximum.x-old_water.minimum.x;c.length=old_water.maximum.y-old_water.minimum.y;c.x=old_water.center().x;c.y=old_water.center().y;c.level=old_water.center().z;c.coast=true;c.scan_scene=false;c.sources=terrain;c.precision=4;c.foam_width=4;c.wave_height=1.4;
    water::install(real,native,false);auto snapshot=editor::initial_snapshot(real);const auto &old=real.loaded.scene.instances.at(water_instance);snapshot.material_overrides[old.id][real.loaded.scene.meshes[old.mesh].material_slots.at(water_slot)]["opacity"]=0;
    native->cache=water::recalculate(real,snapshot,*native,[](const auto &message){std::cout<<message<<std::endl;});water::install(real,native);source=real.loaded.scene;editor::apply_material_overrides(source,real.loaded.scene,snapshot.material_overrides);source.options={};source.environment={.35f,.45f,.65f};source.lights={sun};source.camera=render_camera(camera,800,520);auto eye=camera.eye();water::adapt({native},source,{eye.x,eye.y,eye.z});render("islands-native-top");
    camera.pitch=.38f;source.camera=render_camera(camera,800,520);eye=camera.eye();water::adapt({native},source,{eye.x,eye.y,eye.z});render("islands-native-oblique");
    snapshot.options={};snapshot.lights={sun};editor::save_scene_extension(output/"native-islands.dufex",real,snapshot);
    std::ofstream(output/"island-water.json")<<J{{"config",water::json(*native)},{"comparison_lighting","fixed environment and sun; source materials retained"},{"source",real.source_file.string()}}.dump();
  }
  std::ofstream(output/"result.json")<<J{{"result","PASS"},{"device",devices.front().description},{"stages",records},{"samples",params.samples},{"resolution",{800,520}},{"viewport_fps_measured",false}}.dump(2);return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}}
