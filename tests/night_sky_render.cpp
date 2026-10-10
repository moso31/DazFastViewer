#include "cycles/adapter.h"
#include "cycles/runtime_paths.h"
#include "render_ir/default_options.h"
#include "render_ir/night_sky.h"
#include "bench/output.h"
#include "bench/camera.h"
#include "scene/scene.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/film.h"
#include "scene/object.h"
#include "cycles/night_sky_model.h"
#include "session/session.h"
#include "util/path.h"
#include <OpenColorIO/OpenColorIO.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numbers>
#include <nlohmann/json.hpp>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/night-sky/gpu");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const bool cpu=argc>2&&std::string(argv[2])=="cpu";const auto devices=ccl::Device::available_devices(cpu?ccl::DEVICE_MASK_CPU:ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"渲染设备不可用");
  ir::Scene source;source.options.environment=ir::default_options(true);
  auto set=[&](const char *id,double v){for(size_t i=0;i<source.options.environment.parameters.size();++i)if(source.options.environment.parameters[i].id==id){ir::set_option(source.options.environment,i,0,v);return;}throw std::runtime_error(id);};
  set("Environment Mode",2);set("DFV Night Enabled",1);set("SS Day",2460498);set("SS Time",0);set("SS Latitude",30);set("DFV Night Moon Intensity",0);
  ir::Material material;material.id="white";material.base_color={.8f,.8f,.8f};material.roughness=1;material.specular=0;source.materials.push_back(material);
  auto quad=[&](const std::string &id,std::vector<ir::Vec3> positions){ir::Mesh m;m.id=id;m.positions=std::move(positions);m.material_slots={"white"};ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};m.triangles={a,b};source.meshes.push_back(m);ir::Instance i;i.id=id;i.mesh=uint32_t(source.meshes.size()-1);i.materials={0};source.instances.push_back(i);};
  quad("ground",{{-100,-100,0},{100,-100,0},{100,100,0},{-100,100,0}});
  quad("cube-top",{{-1,-1,2},{1,-1,2},{1,1,2},{-1,1,2}});
  quad("cube-front",{{-1,-1,0},{1,-1,0},{1,-1,2},{-1,-1,2}});
  quad("cube-back",{{1,1,0},{-1,1,0},{-1,1,2},{1,1,2}});
  quad("cube-left",{{-1,1,0},{-1,-1,0},{-1,-1,2},{-1,1,2}});
  quad("cube-right",{{1,-1,0},{1,1,0},{1,1,2},{1,-1,2}});
  source.camera.width=640;source.camera.height=400;source.camera.fov=1.4f;
  auto point=[&](ir::Vec3 d){const Vec3 f=normalized({d.x,d.y,d.z}),r=normalized(cross(f,{0,0,1})),u=cross(r,f);source.camera.transform.value={r.x,u.x,f.x,0,r.y,u.y,f.y,-6,r.z,u.z,f.z,3};};
  point(ir::night_astronomy(source.options.environment).equatorial_to_world({-.05487556f,-.87343709f,-.48383502f}));
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=cpu?4:16;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;
  auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_max_bounce(3);
  CyclesAdapter adapter(scene);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=source.camera.width;buffers.height=buffers.full_height=source.camera.height;J records=J::array();
  std::vector<ccl::ImageHandle> handles;
  auto capture=[&]{std::vector<ccl::ImageHandle> h;for(auto *node:scene.default_background->graph->nodes)if(auto *tex=dynamic_cast<ccl::EnvironmentTextureNode *>(node))h.push_back(tex->handle);return h;};
  auto render=[&](const char *name){const auto start=std::chrono::steady_clock::now();auto dir=output/name;fs::create_directories(dir);auto driver=std::make_unique<Output>(dir,false,source.options);auto *result=driver.get();session.set_output_driver(std::move(driver));{ccl::thread_scoped_lock lock(scene.mutex);ir::Delta d;d.options=source.options;d.camera=source.camera;adapter.apply(d);}const auto h=capture();if(handles.empty())handles=h;else if(ir::night_enabled(source.options.environment))check(h==handles,"调参重新上传了星图/银河纹理");const auto sync=std::chrono::steady_clock::now();session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"夜景渲染失败");const auto end=std::chrono::steady_clock::now();size_t lights=0;for(const auto *g:scene.geometry)if(g->is_light())++lights;check(lights==1,"夜景创建了额外逐星光源");records.push_back({{"stage",name},{"lights",lights},{"samples",params.samples},{"width",buffers.width},{"height",buffers.height},{"quality",2},{"sync_ms",std::chrono::duration<double,std::milli>(sync-start).count()},{"render_ms",std::chrono::duration<double,std::milli>(end-sync).count()}});return result->linear_pixels;};
  auto mean=[](const std::vector<float> &p){double sum=0;for(size_t i=0;i<p.size();i+=4)sum+=p[i]+p[i+1]+p[i+2];return sum/(p.size()/4);};
  const auto base=render("milky-way");check(mean(base)>.001,"夜空没有绘制");check(handles.size()==2,"程序化星空应只使用月球颜色/地形两张纹理");
  set("DFV Night Stars Intensity",0);const auto no_stars=render("no-stars");check(mean(base)>mean(no_stars),"独立星光强度无效");
  set("DFV Night Milky Way Detail",2);const auto contrast=render("galaxy-contrast");check(std::abs(mean(contrast)-mean(no_stars))>.0001,"银河细节对比度无效");set("DFV Night Milky Way Detail",1);
  set("DFV Night Milky Way Intensity",0);const auto no_galaxy=render("no-galaxy");check(mean(no_stars)>mean(no_galaxy)*1.1,"独立银河强度无效");
  set("DFV Night Stars Intensity",1);set("DFV Night Milky Way Intensity",1);set("DFV Night Star Density",.15);const auto sparse=render("bright-stars-only");check(mean(sparse)<mean(base),"程序化星空密度未生效");
  set("DFV Night Star Density",1);set("DFV Night Rotation",70);const auto rotated=render("rotated");check(std::abs(mean(rotated)-mean(base))>.0001,"星空旋转无效");
  set("DFV Night Rotation",0);set("SS Time",3600);render("one-hour-later");
  set("SS Day",2460335);set("SS Time",0);set("DFV Night Moon Intensity",1);set("DFV Night Moon Scale",6);const auto moon=ir::night_astronomy(source.options.environment);point(moon.moon);source.camera.fov=.075f;
  const auto fullmoon=render("full-moon");
  set("DFV Night Moon Lighting Gain",4);const auto boosted_disc=render("moon-disc-gain4");
  double disc_error=0;for(size_t i=0;i<fullmoon.size();++i)disc_error=std::max(disc_error,double(std::abs(fullmoon[i]-boosted_disc[i])));records.push_back({{"stage","moon-lighting-disc-check"},{"max_pixel_error",disc_error}});check(disc_error<1e-6,"月光照明增益改变了直接可见月盘的像素");
  set("DFV Night Moon Lighting Gain",1);set("DFV Night Moon Intensity",0);const auto darkmoon=render("moon-off");check(mean(fullmoon)>mean(darkmoon)*3,"月盘不发光");
  set("DFV Night Moon Intensity",1);set("SS Day",2460341);point(ir::night_astronomy(source.options.environment).moon);render("waning-moon");
  set("SS Day",2460335);set("SS Time",0);set("DFV Night Stars Intensity",0);set("DFV Night Milky Way Intensity",0);set("DFV Night Sky Intensity",0);source.camera.fov=1;point({0,1,-.3f});params.samples=cpu?8:64;
  const auto moonlit=render("moonlit-geometry");
  set("DFV Night Moon Lighting Gain",4);const auto boosted_ground=render("moonlit-geometry-gain4");const auto light_ratio=mean(boosted_ground)/mean(moonlit);records.push_back({{"stage","moon-lighting-ground-check"},{"gain4_ratio",light_ratio}});check(light_ratio>3.8&&light_ratio<4.2,"月光照明增益未成比例照亮地面");
  set("DFV Night Moon Lighting Gain",0);const auto no_moonlight=render("moonlit-geometry-gain0");check(mean(no_moonlight)<mean(moonlit)*.01,"照明增益 0 未熄灭月光");set("DFV Night Moon Lighting Gain",1);
  set("DFV Night Moon Intensity",0);const auto unlit=render("unlit-geometry");check(mean(moonlit)>mean(unlit)+.0001,"月亮只绘制了背景，没有照亮模型");
  set("DFV Night Milky Way Intensity",3);set("DFV Night Stars Intensity",2);
  const auto full=render("full-environment-geometry");check(mean(full)>mean(unlit)+.0001,"完整星空环境没有照亮模型");
  set("Draw Dome",0);const auto hidden=render("hidden-dome-geometry");const auto center=[&](const std::vector<float> &pixels){double v=0;for(int y=180;y<230;++y)for(int x=270;x<370;++x)v+=pixels[(size_t(y)*640+x)*4];return v;};check(std::abs(center(hidden)-center(full))<.05*std::max(center(full),.01),"隐藏穹顶改变了模型受光");
  if(!cpu){
    set("Draw Dome",1);set("DFV Night Milky Way Intensity",1);set("DFV Night Stars Intensity",1);set("DFV Night Sky Intensity",1);set("SS Day",2460498);set("SS Time",0);
    source.camera.width=buffers.width=buffers.full_width=1280;source.camera.height=buffers.height=buffers.full_height=800;
    point(ir::night_astronomy(source.options.environment).equatorial_to_world({-.05487556f,-.87343709f,-.48383502f}));source.camera.fov=1.4f;render("galaxy-wide");
    source.camera.fov=.25f;render("galaxy-close");
    set("SS Day",2460335);set("DFV Night Milky Way Intensity",0);
    const double ra=101.287*std::numbers::pi/180,dec=-16.716*std::numbers::pi/180;
    point(ir::night_astronomy(source.options.environment).equatorial_to_world({float(std::cos(dec)*std::cos(ra)),float(std::cos(dec)*std::sin(ra)),float(std::sin(dec))}));
    source.camera.fov=.15f;render("stars-close");source.camera.fov=1.4f;render("stars-wide");
  }
  set("DFV Night Enabled",0);render("disabled");check(adapter.stats().geometry_updates==0,"夜景调参重建了模型");
  if(!cpu){
    // 单像素中心采样，对照网页的 800x450 固定相机，隔离地平线、月亮和场景曝光。
    source.camera.width=buffers.width=buffers.full_width=800;source.camera.height=buffers.height=buffers.full_height=450;
    source.camera.fov=2*std::atan((450.f/800/2)/.35f);
    const auto position=ccl::make_float3(-10*std::cos(.2f)*std::sin(.25f),10*std::sin(.2f),-10*std::cos(.2f)*std::cos(.25f));
    const auto f=ccl::normalize(-position),r=ccl::normalize(ccl::cross(f,ccl::make_float3(0,1,0))),u=ccl::normalize(ccl::cross(r,f));
    source.camera.transform.value={r.x,u.x,f.x,0,r.y,u.y,f.y,0,r.z,u.z,f.z,0};params.samples=1;
    {
      ccl::thread_scoped_lock lock(scene.mutex);ir::Delta delta;delta.camera=source.camera;adapter.apply(delta);
      for(auto *object:scene.objects){object->set_visibility(0);object->tag_update(&scene);}
      scene.film->set_filter_type(ccl::FILTER_BOX);scene.film->set_filter_width(0);
      auto graph=std::make_unique<ccl::ShaderGraph>();auto *coord=graph->create_node<ccl::TextureCoordinateNode>();auto *eq=graph->create_node<ccl::CombineXYZNode>();
      for(int k=0;k<3;++k){const auto basis=ccl::dfv_night_reference_direction(ccl::make_float3(k==0?1.f:0,k==1?1.f:0,k==2?1.f:0));auto *dot=graph->create_node<ccl::VectorMathNode>();dot->set_math_type(ccl::NODE_VECTOR_MATH_DOT_PRODUCT);dot->set_vector2(basis);graph->connect(coord->output("Generated"),dot->input("Vector1"));graph->connect(dot->output("Value"),eq->input(k==0?"X":k==1?"Y":"Z"));}
      auto *sky=graph->create_node<ccl::DfvNightSkyNode>();graph->connect(eq->output("Vector"),sky->input("Vector"));auto *sum=graph->create_node<ccl::VectorMathNode>();sum->set_math_type(ccl::NODE_VECTOR_MATH_ADD);graph->connect(sky->output("Galaxy"),sum->input("Vector1"));graph->connect(sky->output("Stars"),sum->input("Vector2"));auto *bg=graph->create_node<ccl::BackgroundNode>();bg->set_strength(1);graph->connect(sum->output("Vector"),bg->input("Color"));graph->connect(bg->output("Background"),graph->output()->input("Surface"));scene.default_background->set_graph(std::move(graph));scene.default_background->tag_update(&scene);
    }
    const auto dir=output/"reference-camera";fs::create_directories(dir);auto driver=std::make_unique<Output>(dir);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error()&&result->written&&result->error.empty(),"网页相机 GPU 对照渲染失败");
    std::ifstream reference(output.parent_path()/"reference/reference-linear.bin",std::ios::binary);check(bool(reference),"缺少 CPU 参考图");std::vector<float> expected(800*450*3);reference.read(reinterpret_cast<char *>(expected.data()),std::streamsize(expected.size()*sizeof(float)));check(bool(reference),"参考图尺寸错误");double error=0;
    for(size_t p=0;p<800*450;++p)for(size_t k=0;k<3;++k)error+=std::abs(result->linear_pixels[p*4+k]-expected[p*3+k]);error/=expected.size();records.push_back({{"stage","reference-camera"},{"linear_rgb_mean_error",error}});check(error<.01,"GPU 星空偏离网页参考相机/颜色");
  }
  std::ofstream(output/"result.json")<<J{{"result","PASS"},{"device",devices.front().description},{"stages",records}}.dump(2);std::cout<<"Night rendering PASS\n";return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}}
