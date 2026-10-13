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
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dfv;namespace fs=std::filesystem;using J=nlohmann::json;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){const auto output=fs::absolute(argc>1?argv[1]:"artifacts/thin-walled-material");fs::create_directories(output);try{
  auto ocio=OCIO_NAMESPACE::Config::CreateRaw()->createEditableCopy();ocio->setRole("scene_linear","raw");OCIO_NAMESPACE::SetCurrentConfig(ocio);ccl::path_init(fs::absolute(argv[0]).parent_path().string(),cycles_user_directory());
  const auto devices=ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);check(!devices.empty(),"OptiX unavailable");
  ir::Scene source;source.environment={1,1,1};ir::Material material;material.id="thin-card";material.thin_walled=true;material.base_color={0,0,0};material.translucency_color={0,0,0};material.roughness=.3f;source.materials.push_back(material);
  ir::Mesh mesh;mesh.id="card";mesh.material_slots={"thin-card"};mesh.positions={{-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1}};
  ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};a.uv={ir::Vec2{0,0},ir::Vec2{1,0},ir::Vec2{1,1}};b.uv={ir::Vec2{0,0},ir::Vec2{1,1},ir::Vec2{0,1}};mesh.triangles={a,b};source.meshes.push_back(mesh);
  ir::Instance instance;instance.id="card";instance.materials={0};source.instances.push_back(instance);CameraState camera;camera.target={0,0,0};camera.yaw=camera.pitch=0;camera.distance=3;source.camera=render_camera(camera,128,128);
  ccl::SessionParams params;params.device=devices.front();params.background=params.headless=true;params.samples=128;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  ccl::SceneParams sp;sp.background=true;sp.bvh_type=ccl::BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;ccl::Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<ccl::Pass>();pass->set_name(ccl::ustring("combined"));pass->set_type(ccl::PASS_COMBINED);scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);scene.integrator->set_transparent_max_bounce(64);
  // 关闭体积 SSS 后，薄壁透光仍应保留；它不能吞掉外层反射。
  RenderQuality quality;quality.subsurface=false;CyclesAdapter adapter(scene,false,false,quality);adapter.load(source);ccl::BufferParams buffers;buffers.width=buffers.full_width=buffers.height=buffers.full_height=128;J stages=J::array();
  auto render=[&](const char *name){
    {ccl::thread_scoped_lock lock(scene.mutex);adapter.synchronize(source);}const auto folder=output/name;fs::create_directories(folder);auto driver=std::make_unique<Output>(folder);auto *result=driver.get();session.set_output_driver(std::move(driver));session.reset(params,buffers);session.start();session.wait();check(!session.progress.get_error(),session.progress.get_error_message().c_str());check(result->written&&result->error.empty(),"Render failed");
    double mean=0;for(int y=40;y<88;++y)for(int x=40;x<88;++x)for(int c=0;c<3;++c)mean+=result->linear_pixels[(y*128+x)*4+c];mean/=48*48*3;stages.push_back({{"name",name},{"mean",mean}});return mean;
  };
  auto &m=source.materials[0];m.weighted_glossy=true;m.specular=1;const auto glossy=render("glossy");m.translucency=.8f;const auto glossy_translucent=render("glossy-translucent");
  check(glossy>.5&&std::abs(glossy-glossy_translucent)<.01,"薄壁透光错误衰减了加权高光");
  m.specular=0;m.translucency=0;m.coat=1;m.coat_mode=1;const auto coat=render("coat");m.translucency=1;const auto coat_translucent=render("coat-translucent");
  check(coat>.5&&std::abs(coat-coat_translucent)<.01,"薄壁透光错误衰减了 Top Coat");
  m.coat=.2f;m.specular=.19f;m.translucency=.8f;const auto layered=render("layered");
  // 黑色的反射/透光底层没有能量；两个白色外层应保留各自权重。
  check(std::abs(layered-(.2+.8*.19))<.04,"薄壁叠层反射能量不正确");
  m.coat=m.specular=0;m.translucency=0;const auto black=render("black");m.opacity=0;const auto cutout=render("cutout");
  check(black<.01&&cutout>.95,"最终 Cutout 未作用于整个表面");
  m.opacity=1;m.translucency=1;m.translucency_color={.4f,.4f,.4f};const auto transmitted=render("diffuse-transmission");check(std::abs(transmitted-.4)<.02,"薄壁漫透射被禁用或权重错误");
  m.translucency=0;m.base_color={.7f,.02f,.01f};m.separate_refraction=true;m.refraction_color={1,1,1};m.transmission=.1f;
  const auto partial=render("partial-refraction");m.transmission=1;const auto glass=render("full-refraction");
  check(partial<.5&&glass>.9,"少量白色折射覆盖了整个基础色，或全透射错误染上基础色");
  auto normal_map=[&](const char *name,unsigned char blue){const auto path=output/name;std::ofstream file(path,std::ios::binary);file<<"P6\n4 4\n255\n";for(int i=0;i<16;++i){const unsigned char pixel[]={128,128,blue};file.write(reinterpret_cast<const char *>(pixel),3);}file.close();ir::Texture t;t.file=path;t.id=path.generic_string();t.colorspace=ir::ColorSpace::linear;source.textures.push_back(t);return int(source.textures.size()-1);};
  const auto negative=normal_map("negative-normal.ppm",32),positive=normal_map("positive-normal.ppm",223);m.transmission=0;m.base_color={.4f,.4f,.4f};m.normal_texture=negative;const auto folded=render("negative-normal");m.normal_texture=positive;const auto forward=render("positive-normal");
  check(folded>.35&&std::abs(folded-forward)<.01,"DAZ 法线贴图的负 Z 导致黑斑，或折回半球后丢失细节");
  source.options.environment_file=output/"missing-environment.hdr";const auto missing_environment=render("missing-environment");
  check(std::abs(missing_environment-forward)<.01,"不存在的 HDRI 将紫色占位色带入了环境照明");
  std::ofstream(output/"checks.json")<<J{{"result","PASS"},{"device",devices.front().description},{"samples",params.samples},{"stages",stages}}.dump(2);return 0;
}catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<std::endl;return 1;}}
