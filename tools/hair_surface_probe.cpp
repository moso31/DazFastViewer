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
#include <set>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/hair-surface/probe");fs::create_directories(output);try{
  check(argc>=5,"Usage: HairSurfaceProbe output scene.dufex figure-label hair-label [content-roots...]");
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  std::vector<fs::path> roots;for(int i=5;i<argc;++i)roots.push_back(fs::u8path(argv[i]));
  auto restored=editor::load_scene_extension(fs::u8path(argv[2]),roots,1,[](const auto &m){std::cout<<m<<std::endl;});
  auto &document=*restored.document;const auto &snapshot=restored.snapshot;auto source=document.loaded.scene;size_t host=SIZE_MAX,hair=SIZE_MAX;
  for(const auto &t:document.catalog.targets){if(t.label==argv[3]){check(host==SIZE_MAX,"Figure label is not unique");host=t.instance;}if(t.label==argv[4]){check(hair==SIZE_MAX,"Hair label is not unique");hair=t.instance;}}
  check(host!=SIZE_MAX&&hair!=SIZE_MAX,"Missing figure or hair");
  runtime::DeformationRuntime deformation(source,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);for(;;){const auto p=deformation.prepare(snapshot.values,snapshot.poses);check(p.error.empty(),p.error.c_str());if(!p.pending)break;check(std::chrono::steady_clock::now()<deadline,"Morph preparation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  deformation.evaluate(snapshot.values,snapshot.poses);editor::apply_subdivision_levels(source,snapshot.subdivision_levels);editor::apply_material_overrides(source,document.loaded.scene,snapshot.material_overrides);source.options=snapshot.options;
  const auto &object=source.instances[host];const auto &mesh=source.meshes[object.mesh];ir::Bounds bounds;
  for(const auto &skin:document.skeletons.skins)if(skin.instance==host){const auto regions=runtime::joint_regions(mesh,skin);for(size_t f=0;f<mesh.triangles.size();++f)if(regions.body[f]==regions.head&&mesh.draws(mesh.triangles[f]))for(auto v:mesh.triangles[f].vertices)bounds.add(object.transform.point(mesh.positions[v]));}
  check(!bounds.empty,"Empty head bounds");CameraState camera;camera.target={bounds.center().x,bounds.center().y,bounds.center().z};camera.distance=bounds.extent()*1.5f;camera.yaw=-.655f;camera.pitch=-.375f;source.camera=render_camera(camera,640,640);
  J metadata={{"camera",{camera.target.x,camera.target.y,camera.target.z,camera.distance,camera.yaw,camera.pitch}},{"hair_instances",J::array()},{"materials",J::array()}};
  for(const auto &i:source.instances)if(i.mesh==source.instances[hair].mesh)metadata["hair_instances"].push_back({{"id",i.id},{"visible",i.visible}});
  const auto &hm=source.meshes[source.instances[hair].mesh];std::set<std::array<uint32_t,3>> faces;size_t duplicates=0;for(const auto &t:hm.triangles){auto v=t.vertices;std::sort(v.begin(),v.end());duplicates+=!faces.insert(v).second;}
  metadata["triangles"]=hm.triangles.size();metadata["duplicate_faces"]=duplicates;
  for(auto index:source.instances[hair].materials){const auto &m=source.materials[index];metadata["materials"].push_back({{"id",m.id},{"opacity",m.opacity},{"opacity_texture",m.opacity_texture<0?std::string{}:source.textures[m.opacity_texture].file.string()},{"base_color",{m.base_color.x,m.base_color.y,m.base_color.z}},{"translucency",m.translucency},{"specular",m.specular},{"weighted_glossy",m.weighted_glossy},{"coat",m.coat},{"bump_strength",m.bump_strength}});}
  std::ofstream(output/"scene.json")<<metadata.dump(2);std::cout<<metadata.dump(2)<<std::endl;
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=128;params.threads=8;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(12);scene.integrator->set_transparent_max_bounce(16);
  RenderQuality quality;quality.subsurface=false;CyclesAdapter adapter(scene,false,true,quality);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=buffers.height=buffers.full_height=640;
  auto render=[&](const char *name){std::cout<<"Rendering "<<name<<std::endl;{ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}const auto folder=output/name;fs::create_directories(folder);auto driver=std::make_unique<Output>(folder,false,source.options);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");};
  render("baseline");scene.integrator->set_transparent_max_bounce(32);render("transparent-32");scene.integrator->set_transparent_max_bounce(64);render("transparent-64");scene.integrator->set_transparent_max_bounce(128);render("transparent-128");scene.integrator->set_transparent_max_bounce(16);
  const auto materials=source.materials;for(auto index:source.instances[hair].materials){auto &m=source.materials[index];m.bump_texture=m.normal_texture=-1;}render("no-bump");source.materials=materials;
  for(auto index:source.instances[hair].materials)source.materials[index].translucency=0;render("no-translucency");source.materials=materials;
  for(auto index:source.instances[hair].materials)source.materials[index].opacity*=.5f;render("half-opacity");source.materials=materials;
  return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<std::endl;return 1;}}
