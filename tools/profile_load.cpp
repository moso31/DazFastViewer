#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include "diagnostics/load_profile.h"
#include "editor/document.h"
#include "runtime/picking.h"
#include <fstream>
#include <iostream>

// 无窗口复现编辑器的 CPU 加载与首次形变；不把结果称为 GPU 首帧耗时。
int main(int argc,char **argv) {
  using namespace dfv;using Json=nlohmann::json;namespace fs=std::filesystem;
  if(argc<4||argc>5) {std::cerr<<"SceneLoadProfile <scene.duf> <project.json> <output-directory> [--eager]\n";return 2;}
  const bool lazy=argc==4;
  SetConsoleOutputCP(CP_UTF8);
  const fs::path output=fs::u8path(argv[3]);fs::create_directories(output);
  diagnostics::LoadProfile profile;diagnostics::active=&profile;
  Json result={{"input",argv[1]},{"lazy",lazy},{"scope","CPU load and initial deformation; excludes Qt and Cycles/GPU; nested timings exclude background worker threads"},{"stages",Json::array()}};
  const auto start=diagnostics::Clock::now();
  auto elapsed=[&] {return std::chrono::duration<double>(diagnostics::Clock::now()-start).count();};
  auto save=[&] {
    result["timings"]=Json::object();for(const auto &[name,t]:profile.timings) result["timings"][name]={{"seconds",t.seconds},{"self_seconds",t.self_seconds},{"calls",t.calls}};
    result["files"]=Json::object();for(const auto &[name,t]:profile.files) result["files"][name]={{"seconds",t.seconds},{"calls",t.calls},{"bytes",t.bytes},{"unpacked_bytes",t.unpacked_bytes}};
    std::ofstream(output/"profile.json")<<result.dump(2);
  };
  auto stage=[&](const char *name,auto &&work) {
    std::cout<<elapsed()<<" START "<<name<<std::endl;const double begin=elapsed();
    {diagnostics::Scope scope(name);work();}
    const double end=elapsed();PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory));
    FILETIME created,exited,kernel,user;GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);
    auto seconds=[](FILETIME t) {ULARGE_INTEGER v;v.LowPart=t.dwLowDateTime;v.HighPart=t.dwHighDateTime;return double(v.QuadPart)*1e-7;};
    result["stages"].push_back({{"name",name},{"start",begin},{"seconds",end-begin},{"private_bytes",memory.PrivateUsage},{"working_set",memory.WorkingSetSize},{"peak_working_set",memory.PeakWorkingSetSize},{"cpu_seconds",seconds(kernel)+seconds(user)}});
    std::cout<<end<<" END "<<name<<" seconds="<<end-begin<<" private_MiB="<<memory.PrivateUsage/1048576.0<<std::endl;
  };
  try {
    Json project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;
    for(const auto &root:project.at("content_roots")) roots.push_back(fs::u8path(root.get<std::string>()));
    editor::Document document;document.generation=1;
    stage("geometry_materials",[&] {document.loaded=daz::load(fs::u8path(argv[1]),{roots,false});});
    roots.clear();for(const auto &root:document.loaded.report.at("content_roots")) roots.push_back(fs::u8path(root.get<std::string>()));
    stage("morph_discovery",[&] {document.catalog=daz::discover_morphs(document.loaded,roots,[&](const std::string &s) {std::cout<<elapsed()<<" "<<s<<std::endl;},lazy);});
    stage("skeletons",[&] {document.skeletons=daz::load_skeletons(document.loaded);});
    stage("formula_compile",[&] {document.formulas=daz::enable_formulas(document.catalog,document.skeletons);});
    stage("reports_write",[&] {
      std::ofstream(output/"asset-report.json")<<document.loaded.report.dump(2);
      std::ofstream(output/"morph-catalog.json")<<document.catalog.report.dump(2);
      std::ofstream(output/"skeleton-report.json")<<document.skeletons.report.dump(2);
      std::ofstream(output/"formula-report.json")<<document.formulas.report.dump(2);
    });
    result["files_scanned"]=document.catalog.report.at("files_scanned");
    result["objects"]=Json::array();
    for(const auto &t:document.catalog.targets) {
      size_t offsets=0,initial=0;for(const auto &m:t.morphs) {offsets+=m.offset_count();initial+=m.initial!=0;}
      const auto &mesh=document.loaded.scene.meshes.at(document.loaded.scene.instances.at(t.instance).mesh);
      result["objects"].push_back({{"id",t.id},{"label",t.label},{"morphs",t.morphs.size()},{"initial_nonzero",initial},{"offsets",offsets},{"vertices",mesh.positions.size()},{"triangles",mesh.triangles.size()},{"curves",mesh.curves.size()}});
    }
    stage("release_load_data",[&] {editor::release_load_data(document);});
    editor::Snapshot snapshot;stage("initial_snapshot",[&] {snapshot=editor::initial_snapshot(document);});
    ir::Scene render_scene;stage("render_scene_copy",[&] {render_scene=document.loaded.scene;});
    std::vector<runtime::JointRegions> regions(render_scene.instances.size());
    stage("joint_regions",[&] {for(const auto &skin:document.skeletons.skins) regions.at(skin.instance)=runtime::joint_regions(render_scene.meshes.at(render_scene.instances.at(skin.instance).mesh),skin);});
    std::unique_ptr<runtime::DeformationRuntime> runtime;
    stage("deformation_construct",[&] {runtime=std::make_unique<runtime::DeformationRuntime>(render_scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);});
    stage("initial_deformation",[&] {runtime->evaluate(snapshot.values,snapshot.poses);});
    runtime::PickingScene picking;stage("picking_index",[&] {picking.update(render_scene,runtime::viewport_pick_mask(render_scene.instances.size(),document.catalog.targets));});
    result["total_seconds"]=elapsed();result["status"]="PASS";
    result["conform_bindings"]=runtime->conform_stats().bindings;result["offsets_visited"]=runtime->morph_stats().offsets_visited;
    size_t payloads=0,ready=0,resident_bytes=0;std::set<runtime::MorphPayload *> shared;
    for(const auto &t:document.catalog.targets) for(const auto &m:t.morphs) if(m.payload&&shared.insert(m.payload.get()).second) {++payloads;if(auto data=m.payload->acquire()) {++ready;resident_bytes+=data->size()*sizeof(runtime::SparseOffset);}}
    result["payloads"]={{"unique",payloads},{"ready",ready},{"resident_bytes",resident_bytes}};
    auto hash=[](const auto &values) {uint64_t h=14695981039346656037ull;const auto *bytes=reinterpret_cast<const uint8_t *>(values.data());for(size_t i=0;i<values.size()*sizeof(values[0]);++i) {h^=bytes[i];h*=1099511628211ull;}return std::to_string(h);};
    result["geometry_hashes"]=Json::array();for(const auto &mesh:render_scene.meshes) result["geometry_hashes"].push_back(hash(mesh.positions));
    result["weight_hashes"]=Json::array();for(const auto &weights:runtime->effective()) result["weight_hashes"].push_back(hash(weights));
    size_t effective=0;for(const auto &weights:runtime->effective()) for(auto w:weights) effective+=w!=0;result["effective_nonzero"]=effective;
    save();diagnostics::active=nullptr;return 0;
  } catch(const std::exception &e) {result["status"]="FAIL";result["error"]=e.what();result["total_seconds"]=elapsed();save();diagnostics::active=nullptr;std::cerr<<e.what()<<std::endl;return 1;}
}
