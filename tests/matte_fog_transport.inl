#include "bench/fixtures.h"
#include "water/model.h"

static J fog_transport(const fs::path &output,const ccl::DeviceInfo &device){
  constexpr int width=96,height=64;
  auto set=[](ir::Scene &s,const char *id,double value){for(auto &p:s.options.environment.parameters)if(p.id==id){p.value={value};return;}ir::Option p;p.id=id;p.value={value};s.options.environment.parameters.push_back(p);};
  auto base=[&]{ir::Scene s;s.options.environment.id="environment";ir::ensure_matte_fog_options(s.options.environment);set(s,"Environment Mode",0);set(s,"Draw Dome",1);set(s,"Matte Fog Visibility",50);set(s,"Matte Fog Brightness Relative to Environment",0);set(s,"Matte Fog Brightness",0);set(s,"DFV Matte Fog Scale Height",100000);
    CameraState camera;camera.target={0,0,0};camera.yaw=0;camera.pitch=std::atan(.5f);camera.distance=std::sqrt(5.f);s.camera=render_camera(camera,width,height);return s;};
  auto quad=[](ir::Scene &s,std::string id,std::vector<ir::Vec3> positions,ir::Material material){material.id=id;s.materials.push_back(material);ir::Mesh m;m.id=id;m.material_slots={id};m.positions=std::move(positions);ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};m.triangles={a,b};s.meshes.push_back(m);ir::Instance i;i.id=id;i.mesh=uint32_t(s.meshes.size()-1);i.materials={uint32_t(s.materials.size()-1)};s.instances.push_back(i);};
  auto render=[&](const ir::Scene &source,const char *name){
    const auto dir=output/name;fs::create_directories(dir);ccl::SessionParams params;params.device=device;params.background=params.headless=true;params.samples=256;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
    ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(4);
    CyclesAdapter adapter(scene);adapter.load(source);auto driver=std::make_unique<Output>(dir,false,source.options);auto *result=driver.get();session.set_output_driver(std::move(driver));ccl::BufferParams buffers;buffers.width=buffers.full_width=width;buffers.height=buffers.full_height=height;session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Transport render failed");
    std::array<double,3> average{};for(int y=height/2-2;y<=height/2+2;++y)for(int x=width/2-2;x<=width/2+2;++x)for(int c=0;c<3;++c){const float v=result->linear_pixels[(y*width+x)*4+c];check(std::isfinite(v),"Fog transport produced nonfinite pixels");average[c]+=v/25;}return average;
  };
  J records=J::array();
  for(bool native_water:{false,true}){
    auto s=base();set(s,"Environment Intensity",0);ir::Material mirror;mirror.base_color={1,1,1};mirror.metallic=1;mirror.roughness=0;
    if(native_water){water::Water w;w.config.wave_height=w.config.ripples=w.config.roughness=w.config.foam_strength=0;w.config.color={0,0,0};w.config.clarity=.01;mirror=water::material(w);}
    quad(s,"reflector",{{-100,-100,0},{100,-100,0},{100,100,0},{-100,100,0}},mirror);
    ir::Material lamp;lamp.base_color={0,0,0};lamp.specular=0;lamp.emission_color={1,1,1};lamp.emission_luminance=30000;lamp.emission_two_sided=true;
    quad(s,"reflected-panel",{{-10,30,5},{10,30,5},{10,30,25},{-10,30,25}},lamp);
    const auto clear=render(s,native_water?"water-reflection-clear":"mirror-reflection-clear");set(s,"Matte Fog",1);const auto fog=render(s,native_water?"water-reflection-fog":"mirror-reflection-fog");
    check(clear[0]>.005&&fog[0]<clear[0]*.25,"Reflected object did not use its fog path length");records.push_back({{"check",native_water?"water-object-reflection":"mirror-object-reflection"},{"clear",clear},{"fog",fog}});
    s.instances[1].visible=false;set(s,"Environment Intensity",1);ir::Option tint;tint.id="Environment Tint";tint.value={.05,.05,.05};s.options.environment.parameters.push_back(tint);set(s,"DFV Matte Fog Start",5);set(s,"DFV Matte Fog Scale Height",100);set(s,"Matte Fog Brightness",1);for(auto &p:s.options.environment.parameters)if(p.id=="Matte Fog Brightness Tint")p.value={.8,.1,.05};set(s,"Matte Fog",0);
    const auto sky_clear=render(s,native_water?"water-sky-clear":"mirror-sky-clear");set(s,"Matte Fog",1);const auto sky_fog=render(s,native_water?"water-sky-fog":"mirror-sky-fog");
    check(sky_fog[0]>.02&&sky_fog[0]>sky_clear[0]*4&&sky_fog[0]>sky_fog[1]*4,"Reflected sky did not acquire fog radiance");records.push_back({{"check",native_water?"water-sky-reflection":"mirror-sky-reflection"},{"clear",sky_clear},{"fog",sky_fog}});
  }
  for(bool sun_sky:{false,true}){
    auto s=base();set(s,"Environment Mode",sun_sky?2:3);set(s,"SS Sun Disk Intensity",1);set(s,"SS Latitude",0);set(s,"SS Time",43200);set(s,"DFV Matte Fog Start",10);set(s,"DFV Matte Fog Scale Height",100);set(s,"Matte Fog Visibility",20);ir::Material ground;ground.base_color={1,1,1};ground.specular=0;ground.roughness=1;quad(s,"ground",{{-100,-100,0},{100,-100,0},{100,100,0},{-100,100,0}},ground);
    if(!sun_sky){ir::AreaLight sun;sun.id="sun";sun.kind=ir::LightKind::distant;sun.power={3,3,3};sun.angle=0;s.lights.push_back(sun);}
    const auto clear=render(s,sun_sky?"sun-sky-clear":"sun-light-clear");set(s,"Matte Fog",1);const auto fog=render(s,sun_sky?"sun-sky-dense-fog":"sun-light-dense-fog");
    check(clear[0]>.05&&fog[0]<clear[0]*.01,"Dense fog failed to attenuate sunlight independently of camera fog");records.push_back({{"check",sun_sky?"sun-sky-light-extinction":"distant-light-extinction"},{"clear",clear},{"fog",fog}});
  }
  std::ofstream(output/"transport.json")<<records.dump(2);return records;
}
