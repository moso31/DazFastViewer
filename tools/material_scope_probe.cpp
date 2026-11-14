#include "editor/scene_extension.h"
#include "editor/node_properties.h"
#include "cycles/adapter.h"
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

// 用保存的场景和相机对照材质编辑；只在指定诊断目录写副本和渲染证据。
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){try{
  check(argc==6,"MaterialScopeProbe scene.dufex output runtime-dir project.json plan.json");
  const auto output=fs::absolute(argv[2]);fs::create_directories(output);
  J project,plan;std::ifstream(argv[4])>>project;std::ifstream(argv[5])>>plan;
  std::vector<fs::path> roots;for(const auto &p:project.at("content_roots"))roots.push_back(fs::u8path(p.get<std::string>()));
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[3]).string(),(output/"cache").string());
  std::cout<<"load"<<std::endl;auto restored=editor::load_scene_extension(fs::u8path(argv[1]),roots,1);
  auto &d=*restored.document;const auto &saved=restored.snapshot;auto source=d.loaded.scene;
  runtime::DeformationRuntime deformation(source,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
  for(;;){const auto p=deformation.prepare(saved.values,saved.poses);check(p.error.empty(),p.error.c_str());if(!p.pending)break;check(std::chrono::steady_clock::now()<deadline,"Morph timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  deformation.evaluate(editor::scene_properties(d,saved),saved.poses);editor::apply_subdivision_levels(source,saved.subdivision_levels);editor::apply_instance_node_visibility(d,saved,source);editor::apply_local_shadow_groups(d,saved,source);
  source.options=saved.options;source.lights=saved.lights;
  CameraState camera;if(saved.view){const auto &v=*saved.view;camera.target={v[0],v[1],v[2]};camera.distance=v[3];camera.yaw=v[4];camera.pitch=v[5];}else {const auto b=source.bounds();camera.target={b.center().x,b.center().y,b.center().z};camera.distance=b.extent()*1.6f;camera.yaw=0;camera.pitch=.1f;}
  source.camera=render_camera(camera,640,640);
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=64;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;
  ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);
  scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_seed(1337);scene.integrator->set_max_bounce(12);scene.integrator->set_max_transmission_bounce(8);scene.integrator->set_transparent_max_bounce(32);
  CyclesAdapter adapter(scene);ccl::BufferParams buffers;buffers.width=buffers.full_width=buffers.height=buffers.full_height=640;
  J report=J::array();
  for(const auto &entry:plan){
    auto snapshot=saved;const auto name=entry.at("name").get<std::string>();std::cout<<name<<std::endl;
    const auto edits=entry.value("materials",J::object());
    for(const auto &[id,slots]:edits.items())for(const auto &[slot,patch]:slots.items())for(const auto &[key,value]:patch.items())snapshot.material_overrides[id][slot][key]=value;
    editor::apply_material_overrides(source,d.loaded.scene,snapshot.material_overrides);
    J materials=J::array();for(const auto &i:source.instances)for(size_t slot=0;slot<i.materials.size();++slot){const auto &m=source.materials.at(i.materials[slot]);J values;for(const auto &p:editor::material_parameters())values[p.id]=editor::material_value(m,source.textures,p);materials.push_back({{"instance",i.id},{"slot",source.meshes.at(i.mesh).material_slots.at(slot)},{"values",values},{"definition",m.source_definition.empty()?J{}:J::parse(m.source_definition)}});}
    const auto folder=output/name;fs::create_directories(folder);std::ofstream(folder/"materials.json")<<materials.dump(2);
    if(entry.value("save",false))editor::save_scene_extension(folder/"scene.dufex",d,snapshot);
    {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}
    params.samples=entry.value("samples",64);auto driver=std::make_unique<Output>(folder,false,source.options);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");
    double mean=0;for(size_t k=0;k<result->linear_pixels.size();++k)if(k%4!=3)mean+=result->linear_pixels[k];mean/=640*640*3;
    check(mean>1e-5,"Black render");J record={{"name",name},{"mean",mean},{"samples",params.samples}};
    if(entry.contains("reference")){
      auto input=OIIO::ImageInput::open(entry.at("reference").get<std::string>());check(bool(input),"Missing reference EXR");
      const auto spec=input->spec();check(spec.width==640&&spec.height==640&&spec.nchannels==4,"Reference dimensions differ");
      std::vector<float> previous(result->linear_pixels.size());check(input->read_image(0,0,0,4,OIIO::TypeDesc::FLOAT,previous.data()),"Read reference failed");
      double maximum=0,error=0;for(size_t k=0;k<previous.size();++k){const auto delta=std::abs(double(previous[k])-result->linear_pixels[k]);maximum=std::max(maximum,delta);error+=delta;}
      record["comparison"]={{"max_absolute",maximum},{"mean_absolute",error/previous.size()}};
      check(maximum<1e-4,"Saved scene render changed");
    }
    report.push_back(record);std::ofstream(output/"report.json")<<report.dump(2);
  }
  return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}

