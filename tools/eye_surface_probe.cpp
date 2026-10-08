#include "editor/scene_extension.h"
#include "runtime/picking.h"
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
#include <iostream>
#include <thread>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/eye-surface/probe");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  check(argc>=5,"Usage: EyeSurfaceProbe output scale scene.dufex figure-label [content-roots...]");std::vector<fs::path> roots;for(int i=5;i<argc;++i)roots.push_back(fs::u8path(argv[i]));
  auto restored=editor::load_scene_extension(fs::u8path(argv[3]),roots,1,[](const auto &m){std::cout<<m<<std::endl;});
  auto &document=*restored.document;auto &snapshot=restored.snapshot;auto source=document.loaded.scene;size_t target=SIZE_MAX;
  for(size_t t=0;t<document.catalog.targets.size();++t)if(document.catalog.targets[t].label==argv[4])target=t;
  check(target!=SIZE_MAX,"Missing figure");const auto host=document.catalog.targets[target].instance;
  snapshot.values[target].transform.general_scale=std::stof(argv[2]);
  runtime::DeformationRuntime deformation(source,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);for(;;){const auto p=deformation.prepare(snapshot.values,snapshot.poses);check(p.error.empty(),p.error.c_str());if(!p.pending)break;check(std::chrono::steady_clock::now()<deadline,"Morph preparation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  deformation.evaluate(snapshot.values,snapshot.poses);editor::apply_subdivision_levels(source,snapshot.subdivision_levels);editor::apply_material_overrides(source,document.loaded.scene,snapshot.material_overrides);
  size_t skin=SIZE_MAX,joint=SIZE_MAX;for(size_t s=0;s<document.skeletons.skins.size();++s)if(document.skeletons.skins[s].instance==host){skin=s;for(size_t j=0;j<document.skeletons.skins[s].joints.size();++j){const auto &bone=document.skeletons.skins[s].joints[j];if(bone.name=="Left Eye"||bone.name=="lEye")joint=j;}}
  check(skin!=SIZE_MAX&&joint!=SIZE_MAX,"Missing eye joint");const auto &object=source.instances[host];const auto &mesh=source.meshes[object.mesh];const auto regions=runtime::joint_regions(mesh,document.skeletons.skins[skin]);ir::Bounds bounds;
  for(size_t f=0;f<mesh.triangles.size();++f){int j=regions.detail[f];bool selected=false;for(int depth=0;j>=0&&depth<256;++depth){selected|=j==int(joint);j=regions.parents[size_t(j)];}if(selected&&mesh.draws(mesh.triangles[f]))for(auto v:mesh.triangles[f].vertices)bounds.add(object.transform.point(mesh.positions[v]));}
  check(!bounds.empty,"Empty eye bounds");CameraState camera;camera.target={bounds.center().x,bounds.center().y,bounds.center().z};camera.distance=bounds.extent()*.924f;camera.yaw=2.51506448f;camera.pitch=-.7f;source.camera=render_camera(camera,768,576);
  for(size_t i=0;i<source.instances.size();++i)source.instances[i].visible=i==host||source.instances[i].graft_source==int(host);
  source.lights.clear();source.environment={.8f,.8f,.8f};source.options={};
  J metadata={{"camera",{camera.target.x,camera.target.y,camera.target.z,camera.distance,camera.yaw,camera.pitch}},{"transform",object.transform.value},{"materials",J::array()}};
  for(auto index:object.materials){auto &m=source.materials[index];metadata["materials"].push_back({{"index",index},{"id",m.id},{"opacity",m.opacity},{"transmission",m.transmission},{"thin_walled",m.thin_walled},{"ior",m.ior},{"bump",m.bump_strength},{"bump_texture",m.bump_texture},{"normal_texture",m.normal_texture},{"normal_strength",m.normal_strength},{"subsurface",m.subsurface}});}
  std::ofstream(output/"scene.json")<<metadata.dump(2);std::cout<<metadata.dump(2)<<std::endl;
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=128;params.threads=8;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(12);
  RenderQuality quality;quality.subsurface=false;quality.texture_limit=1024;CyclesAdapter adapter(scene,false,true,quality);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=768;buffers.height=buffers.full_height=576;
  auto render=[&](const char *name){std::cout<<"Rendering "<<name<<std::endl;{ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}const auto folder=output/name;fs::create_directories(folder);auto driver=std::make_unique<Output>(folder,false,source.options);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");};
  render("world");const auto materials=source.materials;for(auto &m:source.materials){m.bump_texture=-1;m.normal_texture=-1;}render("world-no-bump");source.materials=materials;
  const auto center=bounds.center();auto shift=[&](ir::Transform &t){t.value[3]-=center.x;t.value[7]-=center.y;t.value[11]-=center.z;};for(auto &i:source.instances)shift(i.transform);shift(source.camera.transform);render("origin");
  for(auto &m:source.materials)if(m.id.find("EyeMoisture")!=std::string::npos||m.id.find("Cornea")!=std::string::npos)m.opacity=0;render("origin-no-outer");
  return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<std::endl;return 1;}}
