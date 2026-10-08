#pragma once
#include "bench/displacement_scale_check.h"

namespace dfv {
// 凹凸贴图必须在冷加载、增量缩放及完整同步之间保持同一外观。
inline int bump_scale_check(const ccl::DeviceInfo &device,const std::filesystem::path &output) {
  using namespace ccl;using J=nlohmann::json;
  auto require=[](bool ok,const char *why){if(!ok)throw std::runtime_error(why);};
  std::filesystem::create_directories(output);
  const auto file=output/"height.exr";std::vector<float> pixels(64*64);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x)pixels[y*64+x]=.5f+.25f*std::sin(float(x)*.5890486225f)+.25f*std::cos(float(y)*.3926990817f);
  auto image=OIIO::ImageOutput::create(file.string());
  require(image&&image->open(file.string(),OIIO::ImageSpec(64,64,1,OIIO::TypeDesc::FLOAT))&&image->write_image(OIIO::TypeDesc::FLOAT,pixels.data())&&image->close(),"凹凸测试贴图写入失败");
  auto initial=bench::graft_fixture();ir::Texture texture;texture.file=file;texture.colorspace=ir::ColorSpace::linear;initial.textures.push_back(texture);
  for(size_t i=0;i<initial.materials.size();++i){auto &m=initial.materials[i];m.base_color={.55f,.3f,.2f};m.roughness=.55f;m.bump_texture=0;m.bump_strength=1;m.bump_distance=.03f;m.bump_from_texel_density=i==0;}
  auto mesh=initial.meshes[0];mesh.id="ordinary";mesh.hidden_polygons.clear();initial.meshes.push_back(mesh);
  auto instance=initial.instances[0];instance.id="ordinary";instance.mesh=2;instance.transform.value[3]+=7;initial.instances.push_back(instance);
  initial.environment={.03f,.03f,.03f};ir::AreaLight sun;sun.id="sun";sun.kind=ir::LightKind::distant;sun.power={3,3,3};sun.angle=.01f;initial.lights.push_back(sun);
  CameraState camera;camera.target={6,2,.4f};camera.distance=15;camera.pitch=.8f;camera.yaw=.12f;
  auto scaled=[&](float factor){auto source=initial;ir::Transform scale;scale.value[0]=scale.value[5]=scale.value[10]=factor;
    for(auto &i:source.instances)i.transform=scale*i.transform;
    auto view=camera;view.target={camera.target.x*factor,camera.target.y*factor,camera.target.z*factor};view.distance*=factor;source.camera=render_camera(view,256,160);return source;};
  SessionParams params;params.device=device;params.background=params.headless=true;params.samples=64;params.threads=4;params.use_resolution_divider=false;params.use_auto_tile=false;
  SceneParams sp;sp.background=true;sp.bvh_type=BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;
  BufferParams buffers;buffers.width=buffers.full_width=256;buffers.height=buffers.full_height=160;
  std::vector<float> reference;J checks=J::array();bool passed=true;
  for(float cold:{1.f,10000.f}){
    Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);
    scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);CyclesAdapter adapter(scene);adapter.load(scaled(cold));
    int stage=0;for(float factor:{cold,100.f,1.f,.01f,10000.f,1.f}){
      const auto source=scaled(factor);const auto folder=output/(std::to_string(int(cold))+"-"+std::to_string(stage));std::filesystem::create_directories(folder);
      if(stage){thread_scoped_lock lock(scene.mutex);if(stage%2){ir::Delta delta;delta.camera=source.camera;for(uint32_t i=0;i<source.instances.size();++i)delta.instances.push_back({i,source.instances[i].transform});adapter.apply(delta);}else adapter.synchronize(source);}
      auto result=std::make_unique<Output>(folder);auto *written=result.get();session.set_output_driver(std::move(result));session.reset(params,buffers);session.start();session.wait();
      require(!session.progress.get_error(),session.progress.get_error_message().c_str());require(written->written&&written->error.empty(),"凹凸缩放测试渲染失败");
      if(reference.empty())reference=written->linear_pixels;double error=0,energy=0;
      for(size_t p=0;p<reference.size();p+=4)for(size_t c=0;c<3;++c){error+=std::abs(double(reference[p+c])-written->linear_pixels[p+c]);energy+=std::abs(reference[p+c]);}
      const double relative=error/std::max(energy,1e-12);passed&=relative<.02;
      checks.push_back({{"cold_scale",cold},{"stage",stage},{"scale",factor},{"relative_l1_error",relative}});++stage;
    }
  }
  std::ofstream(output/"bump-scale-check.json")<<J({{"status",passed?"PASS":"FAIL"},{"checks",checks}}).dump(2);
  require(passed,"凹凸外观随冷启动或对象倍率变化，详见 bump-scale-check.json");return 0;
}
}
