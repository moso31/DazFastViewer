#include "editor/scene_extension.h"
#include "editor/node_properties.h"
#include "water/document.h"
#include "cloud/document.h"
#include "diagnostics/load_profile.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

// 只读真实场景诊断：记录水体生成与服装各阶段的几何，原资源不回写。
int main(int argc,char **argv) {try {
  using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
  if(argc!=4)throw std::runtime_error("SceneFeedbackProbe <scene.dufex> <project.json> <output>");
  const fs::path output=fs::u8path(argv[3]);fs::create_directories(output);
  J project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;for(const auto &v:project.at("content_roots"))roots.push_back(fs::u8path(v.get<std::string>()));
  J report;diagnostics::LoadProfile profile;diagnostics::active=&profile;
  auto stage=[&](const char *name,auto work){std::cout<<name<<std::endl;const auto begin=std::chrono::steady_clock::now();work();report["seconds"][name]=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();std::cout<<report["seconds"][name]<<std::endl;};
  editor::RestoredScene restored; // 文档及完整保存快照。
  stage("load",[&]{restored=editor::load_scene_extension(fs::u8path(argv[1]),roots,1);});
  auto &d=*restored.document;auto &s=restored.snapshot;
  auto dump=[&](const ir::Scene &scene,const char *phase){for(const auto &t:d.catalog.targets)if(t.label=="HSO Pants"){
    const auto &instance=scene.instances.at(t.instance);const auto &m=scene.meshes.at(instance.mesh);const auto &base=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(t.instance).mesh);
    const auto name=std::to_string(t.instance)+"-"+phase;std::ofstream obj(output/(name+".obj"));for(auto p:m.positions){p=instance.transform.point(p);obj<<"v "<<p.x<<' '<<p.y<<' '<<p.z<<'\n';}for(const auto &f:m.triangles)obj<<"f "<<f.vertices[0]+1<<' '<<f.vertices[1]+1<<' '<<f.vertices[2]+1<<'\n';
    double longest=0,stretch=0;size_t stretched=0;auto length=[](auto a,auto b){return std::hypot(double(a.x-b.x),double(a.y-b.y),double(a.z-b.z));};
    for(const auto &f:m.triangles)for(int k=0;k<3;++k){const auto a=f.vertices[k],b=f.vertices[(k+1)%3];const double old=length(base.positions[a],base.positions[b]),now=length(m.positions[a],m.positions[b]);longest=std::max(longest,now);if(old>.0001){stretch=std::max(stretch,now/old);stretched+=now/old>5;}}
    report["pants"][name]={{"id",t.id},{"fit",t.conform_target},{"vertices",m.positions.size()},{"longest_local_edge",longest},{"max_stretch",stretch},{"edges_over_5x",stretched}};
  }};
  auto scene=d.loaded.scene;std::unique_ptr<runtime::DeformationRuntime> runtime;
  stage("construct",[&]{runtime=std::make_unique<runtime::DeformationRuntime>(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);});
  stage("payloads",[&]{for(;;){auto p=runtime->prepare(s.values,s.poses);if(!p.error.empty())throw std::runtime_error(p.error);if(!p.pending)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}});
  runtime->defer_collision();stage("deform",[&]{runtime->evaluate(editor::scene_properties(d,s),s.poses);});dump(scene,"deformed");
  stage("collision",[&]{auto full=d.loaded.scene;runtime::DeformationRuntime r(full,d.catalog.targets,d.skeletons.skins,d.formulas.graphs,runtime.get());r.evaluate(editor::scene_properties(d,s),s.poses);dump(full,"collision");});
  for(const auto &w:water::effective(d,s)){report["water"][w->id]={{"density",w->config.density},{"coast",w->config.coast}};stage("water_20_moves",[&]{for(int k=0;k<20;++k){auto m=water::mesh(*w,{float(k)*.1f,0,2});report["water"][w->id]["vertices"]=m.positions.size();}});water::MeshCache cache;stage("water_20_cached_moves",[&]{for(int k=0;k<20;++k)water::cached_mesh(*w,{float(k)*.1f,0,2},cache);});report["water"][w->id]["cache_builds"]=cache.builds;report["water"][w->id]["cache_hits"]=cache.hits;}
  for(const auto &[name,v]:profile.timings)report["timings"][name]={{"seconds",v.seconds},{"calls",v.calls}};
  std::ofstream(output/"report.json")<<report.dump(2);std::cout<<report.dump(2)<<std::endl;return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
