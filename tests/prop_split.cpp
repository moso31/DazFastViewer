#include "editor/prop_split.h"
#include "editor/scene_extension.h"
#include "editor/group_transforms.h"
#include "daz/documents.h"
#include <fstream>
#include <iostream>
#include <cmath>

using namespace dfv;
using namespace dfv::editor;
using J=nlohmann::json;
namespace fs=std::filesystem;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
template<class F>static void rejects(F fn){bool rejected=false;try{fn();}catch(const std::exception &){rejected=true;}check(rejected,"无效拆分没有被拒绝");}
static Document load(const fs::path &file){Document d;d.generation=1;d.source_file=file;d.loaded=daz::load(file,{{file.parent_path()}});d.catalog=daz::discover_morphs(d.loaded,{file.parent_path()});d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);return d;}
static fs::path fixture(const fs::path &folder){
  const std::vector<std::string> bones={"lThigh","lShin","lFoot","lToe","rThigh","rShin","rFoot","rToe"};
  J nodes=J::array({{{"id","shoe"},{"type","figure"},{"name","shoe"},{"label","Tall boots"}}}),scene_nodes=J::array();
  scene_nodes.push_back({{"id","shoe"},{"url","#shoe"},{"geometries",J::array({{{"id","shape"},{"url","#mesh"}}})}});
  for(size_t j=0;j<bones.size();++j){const auto parent=j%4==0?"shoe":bones[j-1];nodes.push_back({{"id",bones[j]},{"name",bones[j]},{"label",bones[j]},{"type","bone"},{"parent","#"+parent}});scene_nodes.push_back({{"id",bones[j]},{"url","#"+bones[j]},{"parent","#"+parent}});}
  J vertices=J::array(),faces=J::array(),joints=J::array();for(size_t j=0;j<bones.size();++j){const int offset=int(vertices.size());const float x=j<4?-30.f:30.f,y=float(j%4)*15;vertices.push_back({x,y,0});vertices.push_back({x+10,y,0});vertices.push_back({x+10,y+10,0});vertices.push_back({x,y+10,0});faces.push_back({j,0,offset,offset+1,offset+2,offset+3});J weights=J::array();for(int v=0;v<4;++v)weights.push_back({offset+v,1});joints.push_back({{"node","#"+bones[j]},{"node_weights",{{"count",4},{"values",weights}}}});}
  J native={{"file_version","0.6.0.0"},{"node_library",nodes},{"geometry_library",J::array({{{"id","mesh"},{"vertices",{{"count",vertices.size()},{"values",vertices}}},{"polygon_groups",{{"count",bones.size()},{"values",bones}}},{"polygon_material_groups",{{"count",1},{"values",J::array({"Leather"})}}},{"polylist",{{"count",faces.size()},{"values",faces}}}}})},
    {"modifier_library",J::array({{{"id","skin"},{"skin",{{"node","#shoe"},{"geometry","#mesh"},{"vertex_count",vertices.size()},{"joints",joints}}}}})},{"scene",{{"nodes",scene_nodes}}}};
  native["scene"]["materials"]=J::array({{{"id","leather"},{"geometry","#shape"},{"groups",J::array({"Leather"})},{"diffuse",{{"channel",{{"value",{.2,.3,.4}}}}}}}});
  J uvs=J::array();for(size_t i=0;i<bones.size();++i)for(const auto &uv:J::array({{0,0},{1,0},{1,1},{0,1}}))uvs.push_back(uv);
  native["geometry_library"][0]["default_uv_set"]="#uv";native["uv_set_library"]=J::array({{{"id","uv"},{"vertex_count",vertices.size()},{"uvs",{{"count",uvs.size()},{"values",uvs}}}}});
  const auto file=folder/"boots.duf";std::ofstream(file)<<native.dump();return file;
}
using FaceKey=std::array<int64_t,16>;
static std::vector<FaceKey> faces(const ir::Scene &scene,const std::vector<size_t> &instances){
  std::vector<FaceKey> result;for(auto i:instances){const auto &instance=scene.instances.at(i);const auto &mesh=scene.meshes.at(instance.mesh);for(const auto &f:mesh.triangles){FaceKey key{};for(size_t c=0;c<3;++c){const auto p=instance.transform.point(mesh.positions.at(f.vertices[c]));key[c*5]=std::llround(p.x*1e5);key[c*5+1]=std::llround(p.y*1e5);key[c*5+2]=std::llround(p.z*1e5);key[c*5+3]=std::llround(f.uv[c].x*1e6);key[c*5+4]=std::llround(f.uv[c].y*1e6);}key[15]=f.material_slot;result.push_back(key);}}std::sort(result.begin(),result.end());return result;
}
static ir::Scene evaluate(const Document &d,const Snapshot &s){auto source=transformed_groups(std::make_shared<Document>(d),s.group_transforms);auto scene=source->loaded.scene;runtime::DeformationRuntime runtime(scene,source->catalog.targets,source->skeletons.skins,source->formulas.graphs);runtime.evaluate(s.values,s.poses);return scene;}
static J exercise(Document &d,Snapshot &s,size_t target,const fs::path &folder,bool save){
  const auto original_values=s.values;const auto original_poses=s.poses;const auto root=prepare_prop_copy(d,s,target);
  // 姿势与空间轴均不决定左右，来源骨骼区域应保持稳定。
  s.values[root].transform.rotation_degrees={13,27,42};s.values[root].transform.translation_cm={17,-8,5};freeze_prop_copy(d,s,root);
  check(s.poses==original_poses,"固定 Prop 改动原角色姿势");for(size_t t=0;t<original_values.size();++t)check(s.values[t]==original_values[t],"固定 Prop 改动原对象");
  const auto instance=d.catalog.targets[root].instance;const auto regions=prop_split_source(d.loaded.scene.meshes.at(d.loaded.scene.instances[instance].mesh));auto plan=prop_split_sides(regions);const auto counts=prop_split_counts(regions,plan);
  std::ofstream(folder/("preview-"+std::to_string(target)+".json"))<<prop_mesh_json(d.loaded.scene.meshes.at(d.loaded.scene.instances[instance].mesh)).dump();
  check(regions.bones&&counts[0]>0&&counts[1]>0&&counts[2]==0,"左右快捷分组遗漏腿部或其他区域");
  s.values[root].transform.translation_cm.x+=23;s.values[root].transform.general_scale=1.4;
  const auto slot=d.loaded.scene.meshes.at(d.loaded.scene.instances[instance].mesh).material_slots.front();s.material_overrides[d.loaded.scene.instances[instance].id][slot]["roughness"]=.37;
  const auto expected=faces(evaluate(d,s),{instance});const auto before=d;const auto before_state=s;auto split=split_static_prop(d,s,root,plan);check(split.size()==2&&!s.values[root].visible,"拆分数量或原件状态错误");
  std::vector<size_t> indices;for(auto t:split){const auto i=d.catalog.targets[t].instance;indices.push_back(i);check(d.catalog.targets[t].static_prop&&d.catalog.targets[t].parent.empty(),"拆分结果没有成为独立 Prop");check(s.material_overrides.at(d.loaded.scene.instances[i].id).at(slot).at("roughness")==.37,"拆分丢失材质覆盖");}
  check(faces(evaluate(d,s),indices)==expected,"拆分丢失面、位置、UV 或材质槽");
  auto replay=before;auto replay_state=before_state;restore_split_prop(replay,replay_state,d.operations.back());check(snapshot_json(replay,replay_state)==snapshot_json(d,s),"拆分操作重放不一致");check(faces(evaluate(replay,replay_state),indices)==expected,"操作重放改变外形");
  auto moved=s;moved.values[split[0]].transform.translation_cm.x+=10;const auto motion=evaluate(d,moved),still=evaluate(d,s);check(faces(motion,{indices[1]})==faces(still,{indices[1]})&&faces(motion,{indices[0]})!=faces(still,{indices[0]}),"两个部分不能独立移动");
  if(save){const auto file=folder/"split.dufex";save_scene_extension(file,d,s);auto restored=load_scene_extension(file,{folder,"H:/G1","H:/G3"},2);check(snapshot_json(*restored.document,restored.snapshot)==snapshot_json(d,s),"拆分保存重开改变状态");check(faces(evaluate(*restored.document,restored.snapshot),indices)==expected,"拆分保存重开改变外形");}
  return {{"label",d.catalog.targets[target].label},{"left_faces",counts[0]},{"right_faces",counts[1]},{"unassigned",counts[2]},{"regions",regions.regions.size()},{"result","PASS"}};
}
int main(int argc,char **argv){try{
  const auto folder=fs::absolute(argc>2?fs::u8path(argv[2]):fs::path("prop-split-test"));fs::create_directories(folder);
  if(argc>1){auto restored=load_scene_extension(fs::u8path(argv[1]),{"H:/G1","H:/G3"},1,[](const std::string &s){std::cout<<s<<std::endl;});auto &d=*restored.document;auto &s=restored.snapshot;J report=J::array();std::vector<size_t> shoes;
    for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].label=="Shoes_Hippusuta"||d.catalog.targets[t].label=="SCU Pumps"||d.catalog.targets[t].label=="RO Pumps")shoes.push_back(t);
    check(shoes.size()==3,"7.dufex 未找到三款预期鞋子");for(auto t:shoes){std::cout<<"TEST "<<d.catalog.targets[t].label<<std::endl;report.push_back(exercise(d,s,t,folder,false));}
    save_scene_extension(folder/"7-split.dufex",d,s);auto reopened=load_scene_extension(folder/"7-split.dufex",{"H:/G1","H:/G3"},2);check(snapshot_json(*reopened.document,reopened.snapshot)==snapshot_json(d,s),"真实场景拆分存档重开不一致");
    std::ofstream(folder/"report.json")<<report.dump(2);std::cout<<report.dump(2)<<std::endl;return 0;
  }
  auto d=load(fixture(folder));auto s=initial_snapshot(d);s.subdivision_levels[subdivision_mesh(d,0).id]=2;std::cout<<exercise(d,s,0,folder,true).dump()<<std::endl;
  const auto static_target=d.catalog.targets.size()-2;const auto saved=snapshot_json(d,s);auto missing=prop_split_sides(prop_split_source(d.loaded.scene.meshes.at(d.loaded.scene.instances[d.catalog.targets[static_target].instance].mesh)));missing[0].regions.clear();rejects([&]{split_static_prop(d,s,static_target,missing);});check(snapshot_json(d,s)==saved,"未分配区域导致失败时修改了场景");
  const auto &mesh=d.loaded.scene.meshes.at(d.loaded.scene.instances.back().mesh);auto source=prop_split_source(mesh);auto invalid=prop_split_sides(source);invalid[1].regions=invalid[0].regions;rejects([&]{prop_split_counts(source,invalid);});
  auto legacy=mesh;legacy.prop_regions.clear();legacy.prop_face_regions.clear();legacy.prop_curve_regions.clear();const auto islands=prop_split_source(legacy);check(!islands.bones&&islands.regions.size()==4,"旧 Prop 独立网格块回退错误");
  auto corrupt=mesh;corrupt.prop_regions[0].parent=0;rejects([&]{prop_split_source(corrupt);});
  // 曲线归属和压缩顶点必须一并保留。
  ir::Mesh curves;curves.id="curves";curves.positions={{0,0,0},{0,0,1},{1,0,0},{1,0,1}};curves.curves={{{0,1},0,{}},{{2,3},0,{}}};runtime::Skin skin;skin.joints.resize(2);skin.joints[0].id=skin.joints[0].name="lFoot";skin.joints[1].id=skin.joints[1].name="rFoot";skin.weights={{{0,1}},{{0,1}},{{1,1}},{{1,1}}};auto frozen=curves;capture_prop_regions(frozen,curves,&skin);check(prop_split_counts(prop_split_source(frozen),prop_split_sides(prop_split_source(frozen)))==std::vector<size_t>({1,1,0}),"曲线左右归属丢失");
  runtime::Joint buckle;buckle.id=buckle.name="Buckle";buckle.parent=0;skin.joints.push_back(buckle);skin.weights[0]=skin.weights[1]={{2,1}};capture_prop_regions(frozen,curves,&skin);check(frozen.prop_regions[2].side==-1,"没有继承父骨骼的左右半区");
  std::cout<<"PASS Prop split: multi-region sides, subdivision, independent transforms, materials, replay, DUFEX, legacy topology, curves\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
