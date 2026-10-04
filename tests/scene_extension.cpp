#include "editor/scene_extension.h"
#include "daz/documents.h"
#include "daz/content_entry.h"
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

using namespace dfv;
using namespace dfv::editor;
using J=nlohmann::json;
namespace fs=std::filesystem;
static void check(bool v,const char *message){if(!v)throw std::runtime_error(message);}
template<class F> static void rejects(F f){bool failed=false;try{f();}catch(const std::exception &){failed=true;}check(failed,"invalid scene accepted");}
static void write(const fs::path &file,const J &j){std::ofstream(file,std::ios::binary)<<j.dump();}
static Document load(const fs::path &file,const fs::path &root,std::vector<fs::path> roots={}){roots.insert(roots.begin(),root);Document d;d.source_file=file;d.loaded=daz::load(file,{roots});d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);return d;}
static ir::Scene evaluate(const Document &d,const Snapshot &s){
  auto scene=d.loaded.scene;runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  for(;;){auto resources=runtime.prepare(s.values,s.poses);if(!resources.error.empty())throw std::runtime_error(resources.error);if(!resources.pending)break;check(std::chrono::steady_clock::now()<deadline,"lazy morph timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  runtime.evaluate(s.values,s.poses);return scene;
}
static void equal_geometry(const ir::Scene &a,const ir::Scene &b){
  check(a.instances.size()==b.instances.size(),"instance count changed");
  for(size_t n=0;n<a.instances.size();++n){const auto &x=a.instances[n],&y=b.instances[n];check(x.id==y.id&&x.transform==y.transform&&x.visible==y.visible,"instance state changed");
    const auto &m=a.meshes[x.mesh],&k=b.meshes[y.mesh];check(m.positions==k.positions&&m.hidden_polygons==k.hidden_polygons&&m.graft_vertex_pairs==k.graft_vertex_pairs,"evaluated geometry changed");}
}
static void roundtrip(const fs::path &folder){
  const auto original_folder=folder/fs::u8path("原始场景");fs::create_directory(original_folder);const auto file=original_folder/"source.duf";
  auto native=J::parse(R"({
    "asset_info":{"type":"scene"},"vendor_extension":{"unknown":[1,"preserve",{"curves":[[0,2],[1,3]]}]},
    "node_library":[{"id":"figure","type":"figure"},{"id":"bone","type":"bone","parent":"#figure"}],
    "geometry_library":[{"id":"mesh","vertices":{"count":4,"values":[[0,0,0],[100,0,0],[100,100,0],[0,100,0]]},"polygon_material_groups":{"count":1,"values":["Skin"]},"polylist":{"count":1,"values":[[0,0,0,1,2,3]]},"default_uv_set":"#uv"}],
    "uv_set_library":[{"id":"uv","vertex_count":4,"uvs":{"count":4,"values":[[0,0],[1,0],[1,1],[0,1]]}}],
    "modifier_library":[{"id":"skin","skin":{"node":"#figure","geometry":"#mesh","vertex_count":4,"joints":[{"node":"#bone","node_weights":{"count":4,"values":[[0,1],[1,1],[2,1],[3,1]]}}]}},
      {"id":"Raise","parent":"#mesh","channel":{"id":"value","type":"float","value":0,"min":0,"max":1},"morph":{"vertex_count":4,"deltas":{"count":1,"values":[[1,20,0,0]]}}}],
    "scene":{"nodes":[{"id":"group","type":"node"},{"id":"figure","url":"#figure","parent":"#group","geometries":[{"id":"shape","url":"#mesh"}]},{"id":"bone","url":"#bone","parent":"#figure"},
      {"id":"copy","translation":[{"id":"x","value":250}],"extra":[{"type":"studio/node/instance"},{"channels":[{"channel":{"id":"Instance Target","node":"#figure"}}]}]}],
      "modifiers":[{"id":"raise","url":"#Raise","parent":"#figure","channel":{"current_value":0.1}}],
      "materials":[{"id":"skinmat","geometry":"#shape","groups":["Skin"],"diffuse":{"channel":{"value":[0.2,0.4,0.6]}}}],
      "animations":[{"url":"figure:?rotation/x","keys":[[0,10],[1,20]]}],"extra":[{"type":"vendor/unknown","data":{"values":[1,2,3]}}]}
  })");
  native["scene"]["nodes"][0]["extra"]=J::array({{{"type","studio/node/group_node"}}});
  write(file,native);auto d=load(file,folder);check(d.catalog.targets.size()==1&&d.skeletons.skins.size()==1,"fixture identities changed");
  ir::add_studio(d.loaded.scene);d.operations.push_back({{"op","studio"}});
  auto s=initial_snapshot(d);s.values[0].transform.translation_cm={11,22,33};s.values[0].morphs[0]=.7f;s.poses[0][1].rotation_degrees.z=42;
  s.view=std::array<float,6>{1,2,3,4,.5f,.2f};s.group_transforms["group"].translation_cm.x=7;s.material_overrides[d.loaded.scene.instances[0].id]["Skin"]["roughness"]=.63;
  s.subdivision_levels[subdivision_mesh(d,0).id]=2;for(const auto &i:d.loaded.scene.instances)if(i.prototype>=0)s.instance_ground[i.id].offset_m=.24;
  s.pose_pins.push_back({});s.pose_pins.back().skin=0;s.pose_pins.back().joint=1;s.pose_pins.back().world={1,2,3};
  const auto saved=folder/"standalone.dufex";save_scene_extension(saved,d,s);const auto encoded=daz::read_document_file(saved);
  check(encoded.at("version")==2&&!encoded.at("state").contains("cities"),"v2 schema wrong");
  for(const auto &o:encoded.at("state").at("objects"))check(!o.contains("physics"),"physics persisted in v2");
  check(daz::supported_content_entry(saved)&&daz::content_asset(saved,{folder})==saved,"DUFEX content resolution failed");
  const auto before=snapshot_json(d,s);
  // Plain v1 is still readable, and its first v2 save freezes its source.
  auto v1=scene_extension_json(d,s);v1.erase("archives");v1.erase("source_archive");write(folder/"legacy.dufex",v1);
  auto old=load_scene_extension(folder/"legacy.dufex",{folder},3);check(snapshot_json(*old.document,old.snapshot)==before,"v1 compatibility failed");
  // Same original path, different revisions: both imports retain independent inline geometry and lazy morph data.
  native["geometry_library"][0]["vertices"]["values"][1][0]=130;native["modifier_library"][1]["morph"]["deltas"]["values"][0][1]=60;write(file,native);
  auto incoming=load(file,folder);d.generation=4;append_document(d,std::move(incoming),"second/");auto combined=initial_snapshot(d);combined.values[0].morphs[0]=.7f;combined.values[1].morphs[0]=.7f;
  fs::remove(file);fs::remove(original_folder);
  // Evaluate only after deletion, so no warm payload can conceal a disk dependency.
  const auto expected=evaluate(d,combined);save_scene_extension(folder/"combined.dufex",d,combined);
  auto combined_restore=load_scene_extension(folder/"combined.dufex",{folder},5);equal_geometry(expected,evaluate(*combined_restore.document,combined_restore.snapshot));
  auto refreshed=refresh_parameters(*combined_restore.document,1,{folder});equal_geometry(expected,evaluate(*refreshed,combined_restore.snapshot));
  // Two revisions of one material preset can supply different inline UVs to two figures.
  const auto preset_file=folder/"uv-preset.duf";auto preset=J::parse(R"({"asset_info":{"type":"preset_material"},"uv_set_library":[{"id":"Alt","vertex_count":4,"uvs":{"count":4,"values":[[0.25,0.75],[1,0],[1,1],[0,1]]}}],"scene":{"materials":[{"id":"Skin","groups":["Skin"],"uv_set":"#Alt"}]}})");
  auto &material_doc=*combined_restore.document;auto &material_state=combined_restore.snapshot;
  const MaterialSurface first{material_doc.catalog.targets[0].instance,0},second{material_doc.catalog.targets[1].instance,0};
  write(preset_file,preset);apply_surface_materials(material_doc,material_state,daz::load(preset_file,{{folder}}),{first});
  preset["uv_set_library"][0]["uvs"]["values"][0][0]=.75;write(preset_file,preset);apply_surface_materials(material_doc,material_state,daz::load(preset_file,{{folder}}),{second});fs::remove(preset_file);
  save_scene_extension(folder/"uv.dufex",material_doc,material_state);auto uv_restore=load_scene_extension(folder/"uv.dufex",{folder},6);
  for(auto surface:{first,second}){
    const auto &scene=uv_restore.document->loaded.scene;const auto &i=scene.instances[surface.instance];const auto uv=daz::material_uv_set(scene.materials[i.materials[0]]);
    const auto a=material_archive(*uv_restore.document,surface,uv.owner);check(a&&a->find(preset_file),"surface preset archive lost");
    auto catalog=daz::discover_material_uv_sets(uv_restore.document->loaded,surface.instance,{{folder}},{a});check(catalog.warnings.empty()&&catalog.sets.size()>=2,"frozen inline UV cannot be rediscovered");
    check(scene.meshes[i.mesh].triangles[0].uv[0].x==(surface.instance==first.instance?.25f:.75f),"same-path material revisions mixed");
  }
  auto reopen=[&]{return load_scene_extension(saved,{folder},9);};auto concurrent=std::async(std::launch::async,reopen);auto restored=reopen();auto parallel=concurrent.get();
  check(snapshot_json(*restored.document,restored.snapshot)==before&&snapshot_json(*parallel.document,parallel.snapshot)==before,"independent restore changed state");
  const auto root=encoded.at("source_archive").get<std::string>();const auto &raw=encoded.at("archives").at(root).begin().value();
  check(J::parse(raw.at("document").get<std::string>()).at("vendor_extension")==native.at("vendor_extension"),"unknown original data lost");
  equal_geometry(evaluate(*restored.document,restored.snapshot),evaluate(*parallel.document,parallel.snapshot));
  save_scene_extension(folder/"resaved.dufex",*restored.document,restored.snapshot);auto twice=load_scene_extension(folder/"resaved.dufex",{folder},10);check(snapshot_json(*twice.document,twice.snapshot)==before,"second generation save changed state");
  auto invalid=encoded;invalid["archives"][root].begin().value()["document"]="{}";rejects([&]{restore_scene_extension(invalid,folder,{folder},1);});
  invalid=encoded;invalid["structure"]["instances"].begin().value()["prototype"]="missing";rejects([&]{restore_scene_extension(invalid,folder,{folder},1);});
  auto bad=restored.snapshot;bad.values[0].transform.general_scale=0;rejects([&]{save_scene_extension(saved,*restored.document,bad);});check(daz::read_document_file(saved)==encoded,"failed save changed prior file");
  bad=restored.snapshot;bad.values[0].physics.enabled=true;save_scene_extension(folder/"no-physics.dufex",*restored.document,bad);auto no_physics=load_scene_extension(folder/"no-physics.dufex",{folder},1);check(!no_physics.snapshot.values[0].physics.enabled,"physics restored from v2");
}
int main(int argc,char **argv){try{
  const auto folder=fs::temp_directory_path()/("dfv-dufex-v2-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder);roundtrip(folder);
  if(argc>1){std::vector<fs::path> roots;for(int i=2;i<argc;++i)roots.push_back(fs::u8path(argv[i]));const auto file=fs::u8path(argv[1]);auto d=load(file,file.parent_path(),roots);auto s=initial_snapshot(d);for(auto &v:s.values)v.transform.translation_cm.x+=.1f;const auto saved=folder/"real.dufex";save_scene_extension(saved,d,s);auto restored=load_scene_extension(saved,roots,2);check(snapshot_json(d,s)==snapshot_json(*restored.document,restored.snapshot),"real scene state mismatch");size_t grafts=0,shells=0;for(const auto &i:d.loaded.scene.instances){grafts+=i.graft_source>=0;shells+=i.shell_source>=0;}std::cout<<"real scene: "<<d.loaded.scene.instances.size()<<" instances, "<<d.skeletons.skins.size()<<" skins, "<<grafts<<" grafts, "<<shells<<" shells, "<<fs::file_size(saved)<<" bytes\n";}
  fs::remove_all(folder);std::cout<<"PASS DUFEX v2 frozen sources, independent versions, lazy morphs, poses, instances, camera, unknown data, v1 and corruption\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
