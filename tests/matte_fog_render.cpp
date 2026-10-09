#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "render_ir/matte_fog.h"
#include "bench/output.h"
#include "scene/scene.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
#include "matte_fog_transport.inl"
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/matte-fog/gpu");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const bool cpu=argc>2&&std::string(argv[2])=="cpu";
  const auto devices=ccl::Device::available_devices(cpu?ccl::DEVICE_MASK_CPU:ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"Render device unavailable");
  ir::Scene source;source.options.environment.id="environment";ir::ensure_matte_fog_options(source.options.environment);
  auto set=[&](const std::string &id,std::vector<double> v){for(auto &p:source.options.environment.parameters)if(p.id==id){p.value=v;return;}ir::Option p;p.id=id;p.value=v;source.options.environment.parameters.push_back(p);};
  set("Environment Mode",{0});set("Draw Dome",{1});set("Environment Tint",{.12,.24,.4});
  set("Matte Fog Visibility",{1000});set("Matte Fog Brightness Relative to Environment",{0});set("Matte Fog Brightness Tint",{.7,.75,.8});
  source.camera.width=192;source.camera.height=128;source.camera.fov=1;
  source.camera.transform.value={1,0,0,0, 0,0,1,0, 0,1,0,0};
  // Four angularly equal panels: black/white pairs at 10 m and 500 m.
  for(int i=0;i<4;++i){const float d=i<2?10:500,x=(-.6f+.4f*i)*d;
    ir::Material m;m.id="panel-"+std::to_string(i);m.base_color={0,0,0};m.roughness=1;m.specular=0;
    m.emission_color={i%2?.5f:0,i%2?.5f:0,i%2?.5f:0};m.emission_luminance=30000;m.emission_two_sided=true;source.materials.push_back(m);
    ir::Mesh mesh;mesh.id=m.id;mesh.material_slots={m.id};mesh.smooth=false;
    mesh.positions={{x-.13f*d,d,-.18f*d},{x+.13f*d,d,-.18f*d},{x+.13f*d,d,.18f*d},{x-.13f*d,d,.18f*d}};
    ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};mesh.triangles={a,b};source.meshes.push_back(mesh);
    ir::Instance instance;instance.id=m.id;instance.mesh=i;instance.materials={uint32_t(i)};source.instances.push_back(instance);
  }
  // A fully transparent sheet in front of every object must be invisible to fog.
  auto sheet=source.meshes[0];sheet.id="cutout";sheet.material_slots={"cutout"};sheet.positions={{-5,5,-4},{5,5,-4},{5,5,4},{-5,5,4}};source.meshes.push_back(sheet);
  auto cutout=source.materials[0];cutout.id="cutout";cutout.opacity=0;source.materials.push_back(cutout);
  ir::Instance sheet_instance;sheet_instance.id="cutout";sheet_instance.mesh=4;sheet_instance.materials={4};sheet_instance.visible=false;source.instances.push_back(sheet_instance);
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=64;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;
  auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(4);
  CyclesAdapter adapter(scene);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=192;buffers.height=buffers.full_height=128;J records=J::array();
  float sheet_opacity=0;
  auto render=[&](const char *name){auto dir=output/name;fs::create_directories(dir);auto driver=std::make_unique<Output>(dir,false,source.options);auto *result=driver.get();session.set_output_driver(std::move(driver));{ccl::thread_scoped_lock lock(scene.mutex);ir::Delta d;d.options=source.options;d.camera=source.camera;for(size_t i=0;i<source.instances.size();++i){d.visibility.emplace_back(i,source.instances[i].visible);d.instances.emplace_back(i,source.instances[i].transform);}if(sheet_opacity!=source.materials[4].opacity){d.materials={{4,source.materials[4]}};sheet_opacity=source.materials[4].opacity;}adapter.apply(d);}session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Fog render failed");records.push_back({{"stage",name},{"geometry_updates",adapter.stats().geometry_updates}});return result->linear_pixels;};
  auto pixel=[](const auto &p,int x,int y,int c=0){return p[(y*192+x)*4+c];};
  auto mean=[&](const auto &p,int x,int y,int c=0){double sum=0;for(int yy=y-2;yy<=y+2;++yy)for(int xx=x-2;xx<=x+2;++xx)sum+=pixel(p,xx,yy,c);return sum/25;};
  const auto off=render("off");set("Matte Fog",{1});const auto fog=render("distance");
  // Project x/d using horizontal half field = tan(fov/2)*aspect.
  auto px=[](float angle){return int(96+angle/(std::tan(.5f)*1.5f)*96);};
  const int near_black=px(-.6f),near_white=px(-.2f),far_black=px(.2f),far_white=px(.6f);
  const double near_contrast=mean(fog,near_white,64)-mean(fog,near_black,64),far_contrast=mean(fog,far_white,64)-mean(fog,far_black,64);
  check(mean(fog,far_black,64)>mean(fog,near_black,64)+.3,"Distant geometry has no visible fog");
  check(far_contrast<near_contrast*.3,"Distance fog did not reduce distant contrast");
  check(std::abs(mean(fog,far_black,64)-.7*(1-std::exp(-3.912023*std::hypot(500.,100.)/1000)))<.01,"Fog does not use metric visibility");
  check(mean(fog,96,6)>mean(off,96,6)+.4,"Sky is outside the shared atmosphere");
  source.instances[4].visible=true;const auto transparent=render("transparent-sheet");
  check(std::abs(mean(transparent,far_black,64)-mean(fog,far_black,64))<.005,"Transparent sheet changed fog depth");
  check(std::abs(mean(transparent,96,6)-mean(fog,96,6))<1e-5,"Transparent sheet fogged sky");
  source.materials[4].opacity=.5;const auto half=render("half-opacity-sheet");const double sheet_fog=.7*(1-std::exp(-3.912023*std::hypot(5.,1.)/1000));
  check(std::abs(mean(half,far_black,64)-.5*(sheet_fog+mean(fog,far_black,64)))<.04,"Partial cutout compositing incorrect");source.materials[4].opacity=0;
  source.camera.transform.value[7]=-500;const auto moved=render("camera-500m-back");check(mean(moved,px(.1f),64)>mean(fog,far_black,64)+.05,"Camera movement did not update fog depth");source.camera.transform.value[7]=0;
  set("DFV Matte Fog Start",{100});const auto start=render("start-100m");check(std::abs(mean(start,near_black,64)-mean(off,near_black,64))<.005,"Start distance fogged near geometry");
  set("DFV Matte Fog Start",{0});set("Matte Fog Visibility Tint",{.25,1,1});const auto tint=render("visibility-tint");check(mean(tint,far_black,64)>mean(fog,far_black,64)+.07,"Visibility tint ignored");
  set("Matte Fog Visibility Tint",{1,1,1});set("DFV Matte Fog Scale Height",{50});const auto thin=render("thin-height-layer");
  check(mean(thin,96,6)<mean(fog,96,6)-.2,"Thin atmosphere did not clear the upper sky");
  check(mean(thin,96,64)>mean(thin,96,6)+.2,"Height atmosphere has no horizon gradient");
  source.camera.transform.value[11]=1000;const auto high=render("camera-above-layer");
  check(std::abs(mean(high,96,6)-mean(off,96,6))<.002,"Camera above fog still sees uniform fogged upper sky");
  check(mean(high,96,122)>.68,"Downward ray missed the atmosphere below the camera");source.camera.transform.value[11]=0;
  set("DFV Matte Fog Scale Height",{1000});set("Matte Fog Brightness Relative to Environment",{1});const auto relative=render("relative");
  set("Environment Intensity",{2});const auto brighter=render("relative-2x");check(mean(brighter,far_black,64)>mean(relative,far_black,64)*1.8,"Relative fog brightness did not follow environment");
  set("Environment Intensity",{1});
  const auto hdri=output/"fixture.exr";auto image=OIIO::ImageOutput::create(hdri.string());std::vector<float> hdr_pixels(16*8*3);for(size_t i=0;i<hdr_pixels.size()/2;++i)hdr_pixels[i]=4;
  check(image&&image->open(hdri.string(),OIIO::ImageSpec(16,8,3,OIIO::TypeDesc::FLOAT))&&image->write_image(OIIO::TypeDesc::FLOAT,hdr_pixels.data())&&image->close(),"HDRI fixture failed");source.options.environment_file=hdri;
  const auto hdr=render("relative-hdri");check(mean(hdr,far_black,64)>mean(relative,far_black,64)*1.8,"Relative fog ignored HDRI illumination");
  set("Draw Dome",{0});const auto hidden=render("hdri-hidden");check(std::abs(mean(hidden,far_black,64)-mean(hdr,far_black,64))<.01,"Draw Dome changed fog illumination");
  set("Draw Dome",{1});source.options.environment_file.clear();
  set("Environment Intensity",{1});set("Matte Fog",{0});const auto restored=render("disabled-again");check(std::abs(mean(restored,far_black,64)-mean(off,far_black,64))<.005,"Disabling fog left stale state");
  check(adapter.stats().geometry_updates==0,"Fog edit rebuilt geometry");
  set("Matte Fog",{1});set("Matte Fog Brightness Relative to Environment",{0});set("Matte Fog Visibility",{5000});
  source.instances[4].visible=false;
  // 100000x geometry, distant observer, far from world origin. The far panels
  // and unobstructed sky must converge to the same atmosphere at the horizon.
  for(int i=0;i<4;++i){auto &t=source.instances[i].transform.value;t[0]=t[5]=t[10]=100000;t[3]=100000;t[7]=-200000;}
  source.camera.transform.value[3]=100000;source.camera.transform.value[7]=-200000;source.camera.transform.value[11]=338;
  const auto giant=render("giant-offset");
  for(int i=0;i<4;++i)source.instances[i].visible=false;
  const auto giant_sky=render("giant-offset-sky");
  check(std::abs(mean(giant,far_black,58)-mean(giant_sky,far_black,58))<.01&&std::abs(mean(giant,far_white,58)-mean(giant_sky,far_white,58))<.01,"Distant giant surfaces do not converge to the same sky fog");
  for(int i=0;i<4;++i){source.instances[i].visible=true;source.instances[i].transform.value[3]=source.instances[i].transform.value[7]=0;}
  source.camera.transform.value[3]=source.camera.transform.value[7]=0;const auto origin=render("giant-at-origin");
  check(std::abs(mean(origin,far_black,58)-mean(giant,far_black,58))<.002,"Horizontal world offset changed atmosphere");
  const auto transport=fog_transport(output/"transport",devices.front());
  std::ofstream(output/"result.json")<<J{{"result","PASS"},{"device",devices.front().description},{"near_contrast",near_contrast},{"far_contrast",far_contrast},{"stages",records},{"transport",transport}}.dump(2);std::cout<<"Matte fog rendering: PASS\n";return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}}
