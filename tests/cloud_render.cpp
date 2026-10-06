#include "cloud/document.h"
#include "editor/document.h"
#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "bench/output.h"
#include "bench/camera.h"
#include "bench/fixtures.h"
#include "scene/scene.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <chrono>
#include <iostream>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/cloud/gpu");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  editor::Document document;auto cloud=std::make_shared<cloud::Cloud>();cloud->id="cloud";auto &config=cloud->config;config.width=config.length=500;config.height=0;config.thickness=120;config.scale=110;config.coverage=.72;config.density=.06;config.steps=96;config.sources={"meteor"};config.collisions=false;config.padding=18;config.softness=12;config.time=4;config.velocity_x=25;config.velocity_z=-45;cloud::install(document,cloud);
  auto &source=document.loaded.scene;auto meteor=*cloud;meteor.config.width=meteor.config.length=meteor.config.thickness=40;auto shape=cloud::mesh(meteor);shape.id="meteor";source.meshes.push_back(shape);ir::Material rock;rock.id="rock";rock.base_color={.13f,.06f,.025f};source.materials.push_back(rock);ir::Instance object;object.id="meteor/mesh";object.instance_node="meteor";object.mesh=1;object.materials={1};object.transform=ir::Transform::translate({25,0,35});source.instances.push_back(object);
  if(argc>2){J geometry;std::ifstream(fs::u8path(argv[2]))>>geometry;auto &mesh=source.meshes[1];mesh.positions.clear();mesh.triangles.clear();for(const auto &p:geometry.at("positions"))mesh.positions.push_back({p[0].get<float>(),p[1].get<float>(),p[2].get<float>()});for(const auto &t:geometry.at("triangles")){ir::Triangle face;face.vertices=t.get<std::array<uint32_t,3>>();mesh.triangles.push_back(face);}}
  source.environment={.3f,.4f,.6f};ir::AreaLight sun;sun.id="sun";sun.kind=ir::LightKind::distant;sun.power={3,2.7f,2.3f};sun.angle=.025;sun.transform.value={1,0,0,0,0,.8f,.6f,0,0,-.6f,.8f,0};source.lights.push_back(sun);
  CameraState camera;camera.target={0,0,55};camera.distance=690;camera.yaw=.1f;camera.pitch=.7f;source.camera=render_camera(camera,480,320);cloud::Runtime runtime;runtime.apply(document,{cloud},source);
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=24;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;
  auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(4);scene.integrator->set_max_volume_bounce(1);
  CyclesAdapter adapter(scene,false,true);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=480;buffers.height=buffers.full_height=320;J records=J::array();std::vector<float> baseline;
  auto render=[&](const char *name){const auto begin=std::chrono::steady_clock::now();ir::Delta delta;runtime.apply(document,{cloud},source,&delta);{ccl::thread_scoped_lock lock(scene.mutex);adapter.apply(delta);}const auto synced=std::chrono::steady_clock::now();auto folder=output/name;fs::create_directories(folder);auto driver=std::make_unique<Output>(folder);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Cloud render failed");double luminance=0,difference=0;for(size_t i=0;i<result->linear_pixels.size();i+=4){const auto &p=result->linear_pixels;check(std::isfinite(p[i])&&std::isfinite(p[i+1])&&std::isfinite(p[i+2]),"Nonfinite cloud pixel");luminance+=p[i]*.2126+p[i+1]*.7152+p[i+2]*.0722;if(!baseline.empty())difference+=std::abs(p[i]-baseline[i])+std::abs(p[i+1]-baseline[i+1])+std::abs(p[i+2]-baseline[i+2]);}if(baseline.empty())baseline=result->linear_pixels;
    records.push_back({{"stage",name},{"seconds_including_sync",std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()},{"adapter_sync_ms",std::chrono::duration<double,std::milli>(synced-begin).count()},{"mean_luminance",luminance/(480*320)},{"mean_rgb_difference_from_cloud",difference/(480*320*3)},{"geometry_updates",adapter.stats().geometry_updates},{"triangles",adapter.stats().triangles},{"colliders",source.materials[0].cloud->colliders.size()},{"device_peak_bytes",session.stats.mem_peak}});std::cout<<records.back().dump()<<std::endl;std::ofstream(output/"checks.json")<<records.dump(2);
  };
  render("cloud");config.collisions=true;config.padding=config.softness=config.time=0;render("zero-padding-hull");config.softness=12;render("inward-feather");config.padding=18;config.time=4;render("meteor-wake");check(records.back().at("mean_rgb_difference_from_cloud").get<double>()>.0001,"collision has no rendered effect");config.time=9;render("time-9");config.density=0;render("density-zero");check(records.back().at("mean_rgb_difference_from_cloud").get<double>()>.005,"cloud density has no rendered effect");
  config.density=.06;for(int i=1;i<16;++i){auto other=object;const auto id="meteor-"+std::to_string(i);other.id=id+"/mesh";other.instance_node=id;other.transform=ir::Transform::translate({float((i%4)*85-125),float((i/4)*85-125),35});source.instances.push_back(other);config.sources.push_back(id);}
  runtime.apply(document,{cloud},source);{ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}render("sixteen-colliders");check(source.materials[0].cloud->colliders.size()==16,"GPU collision budget fixture incomplete");
  camera.target={0,0,60};camera.distance=30;camera.pitch=0;source.camera=render_camera(camera,480,320);{ccl::thread_scoped_lock lock(scene.mutex);ir::Delta delta;delta.camera=source.camera;adapter.apply(delta);}render("inside-cloud");
  config.collisions=false;config.coverage=0;render("coverage-zero");check(std::isfinite(records.back().at("mean_luminance").get<double>()),"zero coverage invalid");
  std::ofstream(output/"result.json")<<J{{"result","PASS"},{"device",devices.front().description},{"resolution",{480,320}},{"samples",params.samples},{"viewport_fps_measured",false},{"stages",records}}.dump(2);return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}}
