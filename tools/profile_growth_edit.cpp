#include "editor/scene_extension.h"
#include "editor/object_extension.h"
#include "editor/node_properties.h"
#include "diagnostics/load_profile.h"
#include <fstream>
#include <iostream>
#include <thread>

// 无窗口、只读诊断真实 DUFEX 的调参路径；不包含 Qt 绘制或 GPU 提交。
int main(int argc,char **argv) {try {
  using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
  if(argc!=5)throw std::runtime_error("GrowthEditProfile <scene.dufex> <project.json> <target-id-or-label> <output>");
  const fs::path output=fs::u8path(argv[4]);fs::create_directories(output);
  J project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;
  for(const auto &v:project.at("content_roots"))roots.push_back(fs::u8path(v.get<std::string>()));
  J report={{"input",argv[1]},{"scope","CPU edit, deformation, collision and measurement; excludes Qt and GPU"},{"stages",J::array()}};
  auto save=[&]{std::ofstream(output/"profile.json")<<report.dump(2);};
  auto stage=[&](const std::string &name,auto work){
    std::cout<<"START "<<name<<std::endl;
    diagnostics::LoadProfile profile;diagnostics::active=&profile;const auto begin=diagnostics::Clock::now();
    try{work();}catch(...){diagnostics::active=nullptr;throw;}
    const double seconds=std::chrono::duration<double>(diagnostics::Clock::now()-begin).count();diagnostics::active=nullptr;
    J timings=J::object();for(const auto &[key,v]:profile.timings)timings[key]={{"seconds",v.seconds},{"self_seconds",v.self_seconds},{"calls",v.calls}};
    report["stages"].push_back({{"name",name},{"seconds",seconds},{"timings",timings}});save();std::cout<<"END "<<name<<" "<<seconds<<" s"<<std::endl;
  };
  editor::RestoredScene restored;stage("load",[&]{restored=editor::load_scene_extension(fs::u8path(argv[1]),roots,1);});
  auto &d=*restored.document;auto &s=restored.snapshot;
  size_t selected=d.catalog.targets.size();
  report["objects"]=J::array();
  for(size_t t=0;t<d.catalog.targets.size();++t){const auto &v=d.catalog.targets[t];const auto &instance=d.loaded.scene.instances.at(v.instance);const auto &mesh=d.loaded.scene.meshes.at(instance.mesh);
    report["objects"].push_back({{"id",v.id},{"label",v.label},{"instance_id",instance.id},{"fit",v.conform_target},{"parent",v.parent},{"visible",s.values[t].visible},{"vertices",mesh.positions.size()},{"triangles",mesh.triangles.size()},{"morphs",v.morphs.size()},{"formulas",d.formulas.graphs[t].expressions.size()},{"smoothing",v.smoothing.enabled},{"collision_iterations",v.smoothing.collision_iterations},{"smoothing_iterations",v.smoothing.smoothing_iterations},{"dynamic_cloth",v.dynamic_cloth}});
    if(v.id==argv[3]||v.label==argv[3]){if(selected!=d.catalog.targets.size())throw std::runtime_error("目标名称不唯一，请使用 id");selected=t;}
  }
  if(selected==d.catalog.targets.size()||!editor::growth_character(d,selected))throw std::runtime_error("找不到可调参角色");
  report["selected"]=d.catalog.targets[selected].id;report["initial_age"]=s.values[selected].extension.age;save();
  editor::Snapshot copied;stage("snapshot_copy",[&]{copied=s;});
  J serialized;stage("snapshot_json",[&]{serialized=editor::snapshot_json(d,s);});
  ir::Scene full_scene,preview_scene;std::unique_ptr<runtime::DeformationRuntime> full,preview;
  stage("scene_copy",[&]{full_scene=d.loaded.scene;});
  stage("construct",[&]{full=std::make_unique<runtime::DeformationRuntime>(full_scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);});
  auto prepare=[&](auto &runtime){size_t polls=0;for(;;){auto p=runtime.prepare(s.values,s.poses);++polls;if(!p.error.empty())throw std::runtime_error(p.error);if(!p.pending)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}report["prepare_polls"].push_back(polls);};
  stage("initial_payloads",[&]{prepare(*full);});
  stage("initial_full",[&]{full->evaluate(editor::scene_properties(d,s),s.poses);});
  stage("preview_clone",[&]{preview_scene=full_scene;preview=std::make_unique<runtime::DeformationRuntime>(*full,preview_scene);preview->defer_collision();});
  auto measurement=editor::make_object_measurement(restored.document,selected);
  stage("initial_measurement",[&]{const auto w=measurement(s,{});report["initial_height_cm"]=w.height_cm;});
  const auto original=s.values[selected].extension;
  for(int k=0;k<5;++k){const std::string name=k==0?"age_half":k==1?"age_one":k==2?"age_eight":k==3?"strength":"scale_only";
    if(k==4)stage(name+"/edit",[&]{s.values[selected].transform.general_scale*=1.01;});
    else {auto next=s.values[selected].extension;if(k<2)next.age=original.age+(k+1)*.5;else if(k==2)next.age=8;else next.strength=original.strength>.5?.25:.75;
      stage(name+"/edit",[&]{const auto before=s.values[selected].morphs;report["missing"][name]=editor::edit_growth(d,s,selected,next);size_t changed=0;for(size_t m=0;m<before.size();++m)changed+=before[m]!=s.values[selected].morphs[m];report["changed_morphs"][name]=changed;});}
    stage(name+"/prepare",[&]{prepare(*preview);});
    ir::Delta delta;stage(name+"/preview",[&]{delta=preview->evaluate(editor::scene_properties(d,s),s.poses);});
    report["deltas"][name]={{"meshes",delta.meshes.size()},{"instances",delta.instances.size()}};
    stage(name+"/full",[&]{full->evaluate(editor::scene_properties(d,s),s.poses);});
    stage(name+"/measurement",[&]{const auto w=measurement(s,{});report["measurements"][name]={{"height_cm",w.height_cm},{"kg",w.kg}};});
  }
  stage("unchanged/full",[&]{full->evaluate(editor::scene_properties(d,s),s.poses);});
  stage("unchanged/measurement",[&]{measurement(s,{});});
  report["status"]="passed";save();return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
