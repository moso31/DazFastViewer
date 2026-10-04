#pragma once
#include "bench/output.h"
#include "editor/document.h"
#include "runtime/picking.h"
#include "cycles/adapter.h"
#include "scene/scene.h"
#include "scene/integrator.h"
#include "scene/pass.h"
#include "scene/shader.h"
#include "session/session.h"
#include <fstream>
#include <thread>

namespace dfv {
inline int graft_scale_check(const ccl::DeviceInfo &device,const std::filesystem::path &file,
                             const std::vector<std::filesystem::path> &roots,const std::filesystem::path &output) {
  using namespace ccl;using J=nlohmann::json;
  auto require=[](bool ok,const char *why){if(!ok)throw std::runtime_error(why);};
  std::filesystem::create_directories(output);
  editor::Document document;document.loaded=daz::load(file,{roots,false});
  document.catalog=daz::discover_morphs(document.loaded,roots,[](const auto &message){std::cout<<message<<std::endl;},true);
  document.skeletons=daz::load_skeletons(document.loaded);document.formulas=daz::enable_formulas(document.catalog,document.skeletons);
  auto snapshot=editor::initial_snapshot(document);auto source=document.loaded.scene;
  size_t target=SIZE_MAX,skin_index=SIZE_MAX,joint=SIZE_MAX;
  for(size_t t=0;t<document.catalog.targets.size();++t)if(document.catalog.targets[t].label=="big_01")target=t;
  require(target!=SIZE_MAX,"Scale check requires test7 big_01");
  const auto host=document.catalog.targets[target].instance;
  for(size_t s=0;s<document.skeletons.skins.size();++s){const auto &skin=document.skeletons.skins[s];
    if(source.instances[skin.instance].graft_source!=int(host))continue;
    for(size_t j=0;j<skin.joints.size();++j)if(skin.joints[j].name=="Right Breast 2"){skin_index=s;joint=j;}}
  require(skin_index!=SIZE_MAX,"Scale check requires Right Breast 2 on big_01 graft");
  const auto &skin=document.skeletons.skins[skin_index];const auto regions=runtime::joint_regions(source.meshes[source.instances[skin.instance].mesh],skin);
  runtime::DeformationRuntime deformation(source,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
  while(true){const auto p=deformation.prepare(snapshot.values,snapshot.poses);require(p.error.empty(),p.error.c_str());if(!p.pending)break;
    require(std::chrono::steady_clock::now()<deadline,"Morph preparation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  deformation.evaluate(snapshot.values,snapshot.poses);
  double scale_base=100;
  for(const auto &object:document.loaded.objects)if(object.instance==host)scale_base*=object.general_scale;
  for(size_t s=0;s<document.skeletons.skins.size();++s)if(document.skeletons.skins[s].instance==host)
    for(size_t j=0;j<document.skeletons.skins[s].joints.size();++j)if(document.skeletons.skins[s].joints[j].parent<0)
      scale_base*=deformation.effective_poses()[s][j].general_scale;
  require(scale_base>0,"Invalid figure scale");snapshot.values[target].transform.general_scale=80/scale_base;
  deformation.evaluate(snapshot.values,snapshot.poses);ir::add_studio(source);
  auto camera=[&]{const auto &i=source.instances[skin.instance];const auto &m=source.meshes[i.mesh];ir::Bounds b;
    for(size_t f=0;f<m.triangles.size();++f){bool selected=false;int j=regions.detail[f];for(int depth=0;j>=0&&depth<256;++depth){selected|=j==int(joint);j=regions.parents[size_t(j)];}
      if(selected&&m.draws(m.triangles[f]))for(auto v:m.triangles[f].vertices)b.add(i.transform.point(m.positions[v]));}
    require(!b.empty,"Empty bone bounds");CameraState c;c.target={b.center().x,b.center().y,b.center().z};c.distance=b.extent()*2;c.yaw=2.897493f;c.pitch=.05f;return render_camera(c,384,384);};
  source.camera=camera();
  SessionParams params;params.device=device;params.background=params.headless=true;params.samples=64;params.threads=8;params.use_resolution_divider=false;params.use_auto_tile=false;
  SceneParams sp;sp.background=true;sp.bvh_type=BVH_TYPE_DYNAMIC;sp.use_texture_cache=false;sp.auto_texture_cache=false;
  Session session(params,sp);auto &scene=*session.scene;auto *pass=scene.create_node<Pass>();pass->set_name(ustring("combined"));pass->set_type(PASS_COMBINED);
  scene.integrator->set_use_denoise(false);scene.integrator->set_use_adaptive_sampling(false);
  CyclesAdapter adapter(scene,false,true);adapter.load(source);BufferParams buffers;buffers.width=buffers.full_width=buffers.height=buffers.full_height=384;
  J checks=J::array();double reference_mean=0;std::vector<float> reference;
  auto run=[&](const char *name){std::cout<<"Scale stage: "<<name<<std::endl;const auto folder=output/name;std::filesystem::create_directories(folder);
    auto result=std::make_unique<Output>(folder,false,source.options);auto *written=result.get();session.set_output_driver(std::move(result));
    session.reset(params,buffers);session.start();session.wait();require(!session.progress.get_error(),session.progress.get_error_message().c_str());require(written->written&&written->error.empty(),"Scale render failed");
    double mean=0;size_t dark=0;for(size_t p=0;p<written->linear_pixels.size();p+=4){const float l=(written->linear_pixels[p]+written->linear_pixels[p+1]+written->linear_pixels[p+2])/3;mean+=l;dark+=l<.01f;}
    mean/=384*384;if(reference.empty()){reference=written->linear_pixels;reference_mean=mean;}
    double error=0,energy=0;for(size_t p=0;p<reference.size();p+=4)for(size_t c=0;c<3;++c){error+=std::abs(double(reference[p+c])-written->linear_pixels[p+c]);energy+=std::abs(reference[p+c]);}
    const double relative=error/std::max(energy,1e-12),ratio=mean/reference_mean;
    const double percent=snapshot.values[target].transform.general_scale*scale_base;
    checks.push_back({{"stage",name},{"mean",mean},{"mean_ratio",ratio},{"relative_l1_error",relative},{"dark",dark},{"scale_percent",percent},{"host_transform",source.instances[host].transform.value}});
    const bool passed=ratio>.9&&ratio<1.1&&(percent>100||relative<.02);
    std::ofstream(output/"graft-scale-check.json")<<J({{"status",passed?"RUNNING":"FAIL"},{"checks",checks}}).dump(2);
    require(passed,"Figure scale changed displacement shading or failed to restore the original image");};
  auto scale=[&](double percent,bool synchronize=false){snapshot.values[target].transform.general_scale=percent/scale_base;auto delta=deformation.evaluate(snapshot.values,snapshot.poses);source.camera=camera();delta.camera=source.camera;
    thread_scoped_lock lock(scene.mutex);if(synchronize)adapter.synchronize(source);else adapter.apply(delta);};
  run("baseline-80");scale(8000);run("scaled-8000");scale(80);run("restored-80");
  scale(8000,true);run("synchronized-8000");scale(80,true);run("synchronized-80");
  std::ofstream(output/"graft-scale-check.json")<<J({{"status","PASS"},{"checks",checks},{"device",device.id}}).dump(2);return 0;
}
}
