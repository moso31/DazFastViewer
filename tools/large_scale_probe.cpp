#include "editor/scene_extension.h"
#include "editor/node_properties.h"
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
#include "util/log.h"
#include <OpenColorIO/OpenColorIO.h>
#include <chrono>
#include <iostream>
#include <thread>

// 在相同像素构图下隔离绝对尺寸、凹凸与透明材质的 GPU 成本。只写诊断目录。
// scale 是线性倍率（界面百分比 / 100），不是相对已保存快照的倍率。
// target_only 只缩放角色及附件；normalize_world 再同比缩放全部物体与相机。
// 固定 384×384、恒定环境光、关闭 SSS；快照只求值一次，不包含编辑器水体 LOD。
// DFV_PROBE_STATIC_BVH 测试静态构建，必须单用例冷启动，防止烘焙变换影响后续对照。
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){try{
  check(argc>=5,"LargeScaleProbe scene.dufex figure-label output runtime-dir [plan.json] [project.json]");
  const fs::path output=fs::absolute(argv[3]);fs::create_directories(output);
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);
  ccl::path_init(fs::absolute(argv[4]).string(),(output/"cache").string());
  std::vector<fs::path> roots;if(argc>6){J project;std::ifstream(argv[6])>>project;for(const auto &p:project.at("content_roots"))roots.push_back(fs::u8path(p.get<std::string>()));}
  std::cout<<"Loading scene"<<std::endl;auto restored=editor::load_scene_extension(fs::u8path(argv[1]),roots,1);
  auto &d=*restored.document;auto &snapshot=restored.snapshot;auto source=d.loaded.scene;size_t target=SIZE_MAX;
  for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].label==argv[2])target=t;
  check(target!=SIZE_MAX,"Missing figure");const auto host=d.catalog.targets[target].instance;
  runtime::DeformationRuntime deformation(source,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
  for(;;){const auto p=deformation.prepare(snapshot.values,snapshot.poses);check(p.error.empty(),p.error.c_str());if(!p.pending)break;check(std::chrono::steady_clock::now()<deadline,"Morph preparation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  std::cout<<"Evaluating scene"<<std::endl;deformation.evaluate(editor::scene_properties(d,snapshot),snapshot.poses);
  editor::apply_subdivision_levels(source,snapshot.subdivision_levels);editor::apply_material_overrides(source,d.loaded.scene,snapshot.material_overrides);
  size_t skin=SIZE_MAX,joint=SIZE_MAX;for(size_t s=0;s<d.skeletons.skins.size();++s)if(d.skeletons.skins[s].instance==host){skin=s;for(size_t k=0;k<d.skeletons.skins[s].joints.size();++k)if(d.skeletons.skins[s].joints[k].name=="Head"||d.skeletons.skins[s].joints[k].name=="head")joint=k;}
  check(skin!=SIZE_MAX&&joint!=SIZE_MAX,"Missing head joint");const auto &instance=source.instances[host];const auto &mesh=source.meshes[instance.mesh];const auto regions=runtime::joint_regions(mesh,d.skeletons.skins[skin]);ir::Bounds bounds;
  for(size_t f=0;f<mesh.triangles.size();++f){int k=regions.detail[f];bool selected=false;for(int depth=0;k>=0&&depth<256;++depth){selected|=k==int(joint);k=regions.parents[size_t(k)];}if(selected&&mesh.draws(mesh.triangles[f]))for(auto v:mesh.triangles[f].vertices)bounds.add(instance.transform.point(mesh.positions[v]));}
  check(!bounds.empty,"Empty head bounds");const auto center=bounds.center();const auto matrix=instance.transform.value;const float base_scale=std::hypot(matrix[0],matrix[4],matrix[8]);check(base_scale>0,"Zero scale");
  const auto root_node="#"+source.instances[host].id.substr(0,source.instances[host].id.find('/'));
  std::vector<bool> attached(source.instances.size());attached[host]=true;
  std::vector<std::string> labels(source.instances.size());
  for(const auto &t:d.catalog.targets){labels[t.instance]=t.label;if(t.parent==root_node||t.conform_target==root_node||std::find(t.ancestors.begin(),t.ancestors.end(),root_node)!=t.ancestors.end())attached[t.instance]=true;}
  source.lights.clear();source.environment={.8f,.8f,.8f};source.options={};const auto initial=source;
  J plan=J::array();if(argc>5){std::ifstream(argv[5])>>plan;}else for(float scale:{1.f,1000.f,100000.f,10000000.f,1.f})for(const char *mode:{"original","no_bump","matte"})plan.push_back({{"scale",scale},{"mode",mode}});
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=32;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  const bool static_bvh=std::getenv("DFV_PROBE_STATIC_BVH")!=nullptr;
  check(!static_bvh||plan.size()==1,"Static BVH requires a single case per process because Cycles bakes transforms");
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=static_bvh?ccl::BVH_TYPE_STATIC:ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;sp.texture_limit=1024;
  ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);
  scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_seed(1337);scene.integrator->set_max_bounce(8);scene.integrator->set_max_diffuse_bounce(4);scene.integrator->set_max_glossy_bounce(4);scene.integrator->set_max_transmission_bounce(8);scene.integrator->set_transparent_max_bounce(16);
  RenderQuality quality;quality.subsurface=false;quality.texture_limit=1024;CyclesAdapter adapter(scene,false,true,quality);
  ccl::BufferParams buffers;buffers.width=buffers.full_width=384;buffers.height=buffers.full_height=384;
  double path_ms=0;session.dfv_event=[&](const char *name,uint64_t,double ms){if(std::string_view(name)=="path_trace")path_ms+=ms;};J records=J::array();int index=0;
  for(const auto &entry:plan){
    params.samples=entry.value("samples",32);
    source=initial;const double factor=entry.at("scale").get<double>()/base_scale;const auto mode=entry.value("mode",std::string("original"));
    const auto scope=entry.value("scope",std::string("body"));const auto hide=entry.value("hide",std::vector<std::string>{});
    for(const auto &name:hide)check(std::find(labels.begin(),labels.end(),name)!=labels.end(),("Unknown hidden object: "+name).c_str());
    const bool target_only=entry.value("target_only",false);const double normalize=entry.value("normalize_world",1.0);
    for(size_t i=0;i<source.instances.size();++i){auto &object=source.instances[i];object.visible=object.visible&&(scope=="all"||(scope=="attached"?attached[i]:(i==host||object.graft_source==int(host))))&&std::find(hide.begin(),hide.end(),labels[i])==hide.end();auto &v=object.transform.value;const double object_factor=target_only&&!attached[i]?1.0:factor;for(int k:{0,1,2,4,5,6,8,9,10})v[k]=float(v[k]*object_factor/normalize);int axis=0;for(float c:{center.x,center.y,center.z}){const int k=3+4*axis++;const double pivot=target_only?matrix[k]:0;v[k]=float(((double(v[k])-pivot)*object_factor-(double(c)-pivot)*factor)/normalize);}}
    for(auto &m:source.materials){m.subsurface=0;m.translucency_texture=-1;if(mode=="no_bump"){m.bump_texture=m.normal_texture=-1;}if(mode=="opaque"){m.opacity=1;m.opacity_texture=-1;m.transmission=0;m.transmission_texture=-1;}if(mode=="matte"){const auto id=m.id;m={};m.id=id;m.base_color={.5f,.5f,.5f};m.roughness=1;m.specular=0;}}
    const float bake=entry.value("mesh_scale",1.f);
    if(bake!=1){for(auto &m:source.meshes){for(auto *points:{&m.positions,&m.displacement_rest})for(auto &v:*points){v.x*=bake;v.y*=bake;v.z*=bake;}}for(auto &o:source.instances)for(int k:{0,1,2,4,5,6,8,9,10})o.transform.value[k]/=bake;for(auto &m:source.materials){m.bump_distance*=bake;m.displacement_min*=bake;m.displacement_max*=bake;m.hair_root_radius*=bake;m.hair_tip_radius*=bake;}}
    CameraState camera;camera.target={0,0,0};camera.distance=float(bounds.extent()*factor*entry.value("distance_scale",1.5)/normalize);camera.yaw=std::atan2(-matrix[1],matrix[5]);camera.pitch=-.04f;source.camera=render_camera(camera,384,384);
    std::cout<<"CASE "<<index<<" "<<entry.dump()<<std::endl;
    {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}
    const auto folder=output/std::to_string(index);fs::create_directories(folder);auto driver=std::make_unique<Output>(folder);auto *result=driver.get();session.set_output_driver(std::move(driver));
    if(std::getenv("DFV_PROFILE_KERNELS"))ccl::log_level_set(ccl::LOG_LEVEL_TRACE);
    path_ms=0;const auto begin=std::chrono::steady_clock::now();session.reset(params,buffers);session.start();session.wait();ccl::log_level_set(ccl::LOG_LEVEL_INFO_IMPORTANT);check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();records.push_back({{"case",index++},{"settings",entry},{"static_bvh",static_bvh},{"path_ms",path_ms},{"ms_per_sample",path_ms/params.samples},{"wall_seconds",seconds},{"triangles",adapter.stats().triangles},{"base_scale",base_scale},{"camera_distance",camera.distance},{"device_bytes",session.stats.mem_used}});
    std::ofstream(output/"results.json")<<records.dump(2);std::cout<<records.back().dump()<<std::endl;
  }
  return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
