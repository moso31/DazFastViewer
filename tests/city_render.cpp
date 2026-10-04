#include "city/generator.h"
#include "city/runtime.h"
#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "bench/fixtures.h"
#include "bench/output.h"
#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute("artifacts/city/gpu");fs::create_directories(output);try{
  auto color=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();color->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(color);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX device unavailable");const auto device=devices.front();
  city::Config config;config.directory=argc>1?fs::u8path(argv[1]):city::default_directory({});config.assets={"Building5.duf","Building Tall2.duf","Building Tall1.duf"};
  auto generated=city::generate(config,"city-gpu",std::vector<fs::path>{"G:/G3"});auto source=std::move(generated.loaded.scene);source.environment={.5f,.55f,.65f};ir::AreaLight sun;sun.id="sun";sun.kind=ir::LightKind::distant;sun.power={2.5f,2.3f,2.1f};sun.transform.value={1,0,0,0,0,.8f,.6f,0,0,-.6f,.8f,0};source.lights.push_back(sun);
  CameraState camera;camera.target={0,0,8};camera.distance=700;camera.yaw=.4f;camera.pitch=.7f;source.camera=render_camera(camera,640,400);
  city::Runtime runtime;runtime.bind({generated.city},source);city::Views views;views[generated.city->id].forced=1;runtime.apply(source,views);
  ccl::SessionParams params;params.device=device;params.background=params.headless=true;params.samples=16;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;
  auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);
  CyclesAdapter adapter(scene,false,true);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=640;buffers.height=buffers.full_height=400;J records=J::array();
  for(int stage=1;stage<=5;++stage){int level=stage==5?1:stage;views[generated.city->id].forced=level;ir::Delta delta;runtime.apply(source,views,&delta);const auto started=std::chrono::steady_clock::now();{ccl::thread_scoped_lock lock(scene.mutex);adapter.apply(delta);}const auto folder=output/("LOD"+std::to_string(level)+(stage==5?"-return":""));fs::create_directories(folder);auto result=std::make_unique<Output>(folder);auto *written=result.get();session.set_output_driver(std::move(result));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(written->written&&written->error.empty(),"City render image failed");
    double error=0;if(level==4){for(const auto &region:generated.city->regions){if(region.proxy.empty())continue;auto instance=std::find_if(source.instances.begin(),source.instances.end(),[&](const auto &i){return i.id==region.proxy;});const auto &mesh=source.meshes.at(instance->mesh);ccl::Mesh *rendered=nullptr;for(auto *o:scene.objects)if(o->name==ccl::ustring(region.proxy))rendered=static_cast<ccl::Mesh *>(o->get_geometry());check(rendered&&rendered->num_verts()==mesh.positions.size(),"LOD4 proxy missing or subdivided unexpectedly");for(size_t v=0;v<mesh.positions.size();++v){const auto expected=instance->transform.point(mesh.positions[v]);const auto actual=rendered->get_position()[v];error=std::max(error,double(std::abs(expected.x-actual.x)+std::abs(expected.y-actual.y)+std::abs(expected.z-actual.z)));}}check(error<.015,"LOD4 actual displaced positions disagree with CPU geometry");}
    const auto &a=adapter.stats();records.push_back({{"lod",level},{"seconds_including_sync",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()},{"unique_triangles",a.unique_triangles},{"triangles",a.triangles},{"instances",a.instances},{"device_peak_bytes",session.stats.mem_peak},{"displacement_error_m",error}});std::ofstream(output/"checks.json")<<records.dump(2);std::cout<<records.back().dump()<<std::endl;
  }
  std::ofstream(output/"result.json")<<J{{"result","PASS"},{"device",device.description},{"stages",records},{"samples",params.samples},{"resolution",{640,400}},{"viewport_fps_measured",false}}.dump(2);return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}}
