#pragma once
#include "bench/graft_fixture.h"
#include "bench/output.h"
#include "cycles/adapter.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/mesh.h"
#include "scene/pass.h"
#include "scene/integrator.h"
#include "session/session.h"
#include <fstream>

namespace dfv {
// Compare actual GPU shading at identical framing. Constant-height vertex tests
// alone cannot catch a scale error in the bump derived from displacement.
inline nlohmann::json displacement_scale_check(const ccl::DeviceInfo &device,
                                               const std::filesystem::path &output) {
  using namespace ccl;using J=nlohmann::json;
  auto require=[](bool ok,const char *why){if(!ok)throw std::runtime_error(why);};
  std::filesystem::create_directories(output);
  const auto file=output/"height.exr";
  std::vector<float> pixels(64*64);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x)
    pixels[y*64+x]=.5f+.25f*std::sin(float(x)*.5890486225f)+.25f*std::cos(float(y)*.3926990817f);
  auto image=OIIO::ImageOutput::create(file.string());
  require(image&&image->open(file.string(),OIIO::ImageSpec(64,64,1,OIIO::TypeDesc::FLOAT))&&
          image->write_image(OIIO::TypeDesc::FLOAT,pixels.data())&&image->close(),"Scale fixture texture write failed");
  auto source=bench::graft_fixture();
  ir::Texture texture;texture.file=file;texture.colorspace=ir::ColorSpace::linear;source.textures.push_back(texture);
  for(auto &material:source.materials) {
    material.base_color={.55f,.3f,.2f};material.roughness=.55f;
    material.displacement_texture=0;material.displacement_min=-.015f;material.displacement_max=.015f;material.displacement_strength=1;
  }
  // Include an ordinary mesh as well as the joined graft. The fixture's root
  // already contains nonuniform scale, and both surfaces use shared shaders.
  auto mesh=source.meshes[0];mesh.id="ordinary";mesh.hidden_polygons.clear();source.meshes.push_back(mesh);
  auto instance=source.instances[0];instance.id="ordinary";instance.mesh=2;instance.transform.value[3]+=7;source.instances.push_back(instance);
  source.environment={.03f,.03f,.03f};
  ir::AreaLight sun;sun.id="sun";sun.kind=ir::LightKind::distant;sun.power={3,3,3};sun.angle=.01f;source.lights.push_back(sun);
  CameraState camera;camera.target={6,2,.4f};camera.distance=15;camera.pitch=.8f;camera.yaw=.12f;
  source.camera=render_camera(camera,256,160);
  const auto initial=source;
  SessionParams params;params.device=device;params.background=params.headless=true;params.samples=64;params.threads=4;
  params.use_resolution_divider=false;params.use_auto_tile=false;
  SceneParams sp;sp.background=true;sp.bvh_type=BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;
  Session session(params,sp);auto &scene=*session.scene;
  auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);
  scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);
  CyclesAdapter adapter(scene);adapter.load(source);
  BufferParams buffers;buffers.width=buffers.full_width=256;buffers.height=buffers.full_height=160;
  std::vector<float> reference;J checks=J::array();int stage=0;
  for(float factor:{1.f,100.f,.01f,10000.f,1.f}) {
    const auto folder=output/std::to_string(stage);std::filesystem::create_directories(folder);
    auto next=initial;ir::Transform scaling;scaling.value[0]=scaling.value[5]=scaling.value[10]=factor;
    ir::Delta delta;
    for(uint32_t i=0;i<next.instances.size();++i){next.instances[i].transform=scaling*initial.instances[i].transform;delta.instances.push_back({i,next.instances[i].transform});}
    auto view=camera;view.target={camera.target.x*factor,camera.target.y*factor,camera.target.z*factor};view.distance*=factor;
    next.camera=render_camera(view,256,160);delta.camera=next.camera;
    {thread_scoped_lock lock(scene.mutex);if(stage%2)adapter.apply(delta);else adapter.synchronize(next);}
    auto result=std::make_unique<Output>(folder);auto *written=result.get();session.set_output_driver(std::move(result));
    session.reset(params,buffers);session.start();session.wait();
    require(!session.progress.get_error(),session.progress.get_error_message().c_str());require(written->written&&written->error.empty(),"Scale regression render failed");
    if(reference.empty())reference=written->linear_pixels;
    double error=0,energy=0;size_t changed=0;
    for(size_t p=0;p<reference.size();p+=4)for(size_t c=0;c<3;++c) {
      const double difference=std::abs(double(reference[p+c])-written->linear_pixels[p+c]);
      error+=difference;energy+=std::abs(reference[p+c]);changed+=difference>.05;
    }
    const double relative=error/std::max(energy,1e-12);
    checks.push_back({{"stage",stage},{"scale",factor},{"relative_l1_error",relative},{"channels_over_005",changed}});
    std::ofstream(output/"checks.json")<<checks.dump(2);
    require(relative<.02,"Displacement shading changed with uniform object/camera scaling (see scale/checks.json)");
    ++stage;
  }
  return checks;
}
}
