#include "editor/geograft_materials.h"
#include "editor/scene_extension.h"
#include "daz/content_entry.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace dfv;using namespace dfv::editor;using J=nlohmann::json;namespace fs=std::filesystem;
static void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F f,const char *message){bool failed=false;try{f();}catch(const std::exception &){failed=true;}check(failed,message);}
static Document fixture(const fs::path &folder){
  Document d;auto &s=d.loaded.scene;d.loaded.report={{"content_roots",{folder.generic_string()}}};
  const std::vector<std::vector<std::string>> slots={{"Body","Arms"},{"GP_Torso","GP_Vagina"},{"Body","GoldenPalace_2254_GP_Torso"},{"Breast","Arms","HD Nipples L","HD Nipples R"},{"Body","Arms"},{"GP_Torso","GP_Vagina"}};
  for(size_t n=0;n<slots.size();++n){const auto name="object"+std::to_string(n);ir::Mesh mesh;mesh.id=name+"/mesh";mesh.positions={{0,0,0},{1,0,0},{0,1,0}};mesh.material_slots=slots[n];mesh.source_polygon_count=1;ir::Triangle tri;tri.vertices={0,1,2};mesh.triangles.push_back(tri);s.meshes.push_back(mesh);ir::Instance i;i.id=name;i.mesh=uint32_t(n);if(n==2){i.shell_source=1;i.shell_root=0;}for(size_t slot=0;slot<slots[n].size();++slot){ir::Material m;m.id=name+"/"+slots[n][slot];m.base_color=n==0?ir::Vec3{.3f,.4f,.5f}:ir::Vec3{1,1,1};m.roughness=.37f;m.opacity=n==2?.4f:1;i.materials.push_back(uint32_t(s.materials.size()));s.materials.push_back(m);}s.instances.push_back(i);
    runtime::Target t;t.id=name+"/geometry";t.instance=uint32_t(n);t.label=name;if(n!=0&&n!=4){t.conform_target=n==5?"#object4":"#object0";t.parent=t.conform_target;}d.catalog.targets.push_back(t);daz::AssetObject o;o.id=name;o.instance=uint32_t(n);o.figure=true;o.auto_fit_base=n==0||n==4?"/Genesis 8.1/Female":"";o.conform_target=t.conform_target;o.parent=t.parent;o.geometry_file=folder/(n==3?"data/3feetwolf/HD Nipples for G8F - 2.0/HD Nipples - 2.0/mesh.dsf":"data/Meipex/GoldenPalace/GoldenPalace_Genitalia_v2/mesh.dsf");d.loaded.objects.push_back(o);
  }d.formulas.graphs.resize(slots.size());return d;
}
static void unit(const fs::path &folder){
  fs::create_directories(folder);auto d=fixture(folder);auto s=initial_snapshot(d);const auto before=d.loaded.scene;
  s.material_overrides["object0"]["Body"]["roughness"]=.81;
  check(copy_golden_palace_materials(d,s,1)==3,"没有完整匹配 GP 本体和覆盖层");
  const auto &scene=d.loaded.scene;const auto &graft=scene.materials.at(scene.instances[1].materials[0]);const auto &shell=scene.materials.at(scene.instances[2].materials[1]);
  check(graft.base_color==ir::Vec3{.3f,.4f,.5f}&&graft.roughness==.81f,"未使用当前编辑后的宿主材质");check(shell.opacity==.4f&&shell.base_color==graft.base_color,"覆盖层透明度被覆盖");check(scene.materials.at(scene.instances[2].materials[0]).base_color==ir::Vec3{1,1,1},"修改了覆盖层无关表面");check(scene.materials.at(scene.instances[5].materials[0]).base_color==ir::Vec3{1,1,1},"串改另一个角色");check(d.operations.size()==3,"材质结果未记录为可重放操作");
  const auto root=folder/"People/Genesis 8 Female/Anatomy";
  check(daz::native_material_action(root/"3feetwolf/HD Nipples - 2.0/HD Textures/Apply HD Textures.dse")==daz::NativeMaterialAction::hd_apply,"应用入口未识别");check(daz::supported_content_entry(root/"Golden Palace v2/9-Genesis 8_1/3-GoldenPalace_Material Copy_v2-G8F 8_1.dse"),"GP 入口未识别");check(!daz::supported_content_entry(root/"unrelated/Apply HD Textures.dse"),"无关同名脚本被接受");
  const auto identity=hd_texture_identity(d,s,0);auto cache=folder/"cache";auto baked=cache/"new";fs::create_directories(baked);for(const char *type:{"D","SSS","SSS_Gray","S","B","NM","Disp"})for(char side:{'L','R'})std::ofstream(baked/(std::string("NipplesMap")+type+"_"+side+".jpg"))<<"fixture";
  HDBakeJob job;job.directory=baked;job.identity=identity;finish_hd_bake(job);check(find_hd_textures(d,s,0,{},cache)==std::vector<fs::path>{baked},"当前材质未匹配烘焙索引");s.material_overrides["object0"]["Body"]["roughness"]=.4;check(find_hd_textures(d,s,0,{},cache).empty(),"旧材质缓存被误用");fs::remove(baked/"NipplesMapNM_R.jpg");rejects([&]{finish_hd_bake(job);},"缺失右法线图仍报告完成");
  const auto product=folder/"Runtime/Textures/3feetwolf/HDNipplesforG8F-2.0",resources=folder/"data/3feetwolf/HD Nipples for G8F - 2.0/Resources",uv=resources.parent_path()/"HD Nipples - 2.0/UV Sets/3feetwolf/Base/HD UVs.dsf";
  fs::create_directories(resources);fs::create_directories(uv.parent_path());std::ofstream(uv)<<J{{"uv_set_library",J::array({{{"id","HD UVs"},{"vertex_count",3},{"uvs",{{"count",3},{"values",{{.2,.2},{.8,.2},{.2,.8}}}}}}})}};
  std::ofstream(uv.parent_path()/"default.dsf")<<J{{"uv_set_library",J::array({{{"id","default"},{"vertex_count",3},{"uvs",{{"count",3},{"values",{{0,0},{1,0},{0,1}}}}}}})}};
  fs::create_directories(product/"BaseColors");fs::create_directories(product/"Masks");fs::create_directories(product/"Templates/NotObviousGlands");std::ofstream(resources/"preprocessing.exe")<<"test only";
  for(const auto *name:{"D","SSS","S","B","NM","Disp"})std::ofstream(product/"BaseColors"/(std::string("Base")+name+".jpg"))<<"neutral";
  for(char side:{'L','R'}){std::ofstream(product/"Masks"/(std::string("MediumMask_")+side+".tif"))<<"mask";for(const auto *name:{"D_1","SSS_Color_1","SSS_Gray","S","B","NM","Disp"})std::ofstream(product/"Templates/NotObviousGlands"/(std::string("Default")+name+"_"+side+".jpg"))<<"template";}
  for(int i:{1,2})std::ofstream(resources/(std::to_string(i)+".DAT"))<<"[1.25,0.75],\n[1.5,0.75],\n[1.5,0.5],\n[1.25,0.5],\n";for(int i:{3,4})std::ofstream(resources/(std::to_string(i)+".DAT"))<<"0,1,2,3\n";for(char side:{'a','b'})std::ofstream(resources/(std::string("5")+side+".DAT"))<<"0\n";
  auto plan=prepare_hd_bake(d,s,0,{folder},folder/"job",{});check(plan.inputs.size()==6&&plan.arguments.size()==8,"烘焙进程参数或输入通道不完整");std::ifstream coordinates(plan.directory/"7a.DAT");std::string first;std::getline(coordinates,first);check(first=="1024,3072","未根据原始 UV 表重建当前任务的映射");
  std::ofstream(baked/"NipplesMapNM_R.jpg")<<"fixture";check(apply_hd_textures(d,s,0,baked,{folder})==2,"HD 左右表面未应用");const auto &left=scene.materials.at(scene.instances[3].materials[2]);const auto &right=scene.materials.at(scene.instances[3].materials[3]);check(daz::material_uv_set(left).label=="HD UVs","没有切换 HD UV");check(scene.textures.at(left.color_texture).file.filename()=="NipplesMapD_L.jpg"&&scene.textures.at(right.normal_texture).file.filename()=="NipplesMapNM_R.jpg","左右贴图错位");check(scene.textures.at(left.color_texture).colorspace==ir::ColorSpace::srgb&&scene.textures.at(left.normal_texture).colorspace==ir::ColorSpace::linear,"数据和颜色贴图空间混用");
  d.catalog.targets[3].conform_target="#object4";d.loaded.objects[3].conform_target="#object4";rejects([&]{prepare_hd_bake(d,s,0,{},folder/"another-job",{});},"无本角色附件仍允许烘焙");
}
#include "geograft_update.inl"
#include "geograft_uv.inl"
static Document load(const fs::path &p,const std::vector<fs::path> &roots,bool defer=false){Document d;d.source_file=p;d.loaded=daz::load(p,{roots,false,defer});d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);return d;}
static size_t attach(Document &d,Snapshot &s,const fs::path &file,size_t host,const std::vector<fs::path> &roots){std::cout<<"load "<<file.filename().string()<<std::endl;const auto first=d.catalog.targets.size();append_document(d,load(file,roots,true));attach_import(d,first,host);s=initial_snapshot(d);return first;}
static J signature(const Document &d,const Snapshot &s){J r=J::object();for(size_t i=0;i<d.loaded.scene.instances.size();++i){const auto &instance=d.loaded.scene.instances[i];const auto &mesh=d.loaded.scene.meshes.at(instance.mesh);for(size_t n=0;n<mesh.material_slots.size();++n)r[instance.id][mesh.material_slots[n]]=copy_material(d.loaded.scene,s.material_overrides,{i,n});}return r;}
static void real(const fs::path &source,const fs::path &folder,const fs::path &maps){
  const std::vector<fs::path> roots={"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};auto d=load(fs::absolute(source),roots);auto s=initial_snapshot(d);size_t host=0;for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].character){host=t;break;}
  if(std::none_of(d.loaded.objects.begin(),d.loaded.objects.end(),[](const auto &o){return daz::entry_key(o.geometry_file).find("/meipex/goldenpalace/")!=std::string::npos;}))attach(d,s,"H:/G3/People/Genesis 8 Female/Anatomy/Golden Palace v2/9-Genesis 8_1/1-GoldenPalace_Genitalia_v2.duf",host,roots);
  if(std::none_of(d.loaded.objects.begin(),d.loaded.objects.end(),[](const auto &o){return daz::entry_key(o.geometry_file).find("/3feetwolf/hd nipples for g8f - 2.0/")!=std::string::npos;}))attach(d,s,"H:/G3/data/3feetwolf/HD Nipples for G8F - 2.0/HD Nipples - 2.0/HD Nipples for G8F - 2.0.duf",host,roots);
  auto old=d;auto old_snapshot=s;const auto original=signature(d,s);const auto gp=copy_golden_palace_materials(d,s,host);
  const auto hd=apply_hd_textures(d,s,host,fs::absolute(maps),roots);d.loaded.scene.validate();check(signature(old,old_snapshot)==original,"操作破坏了撤销所需原文档");
  fs::create_directories(folder);const auto save=folder/"applied.dufex";save_scene_extension(save,d,s);auto restored=load_scene_extension(save,roots,0);check(signature(*restored.document,restored.snapshot)==signature(d,s),"保存重载材质或 UV 不一致");
  auto job=prepare_hd_bake(d,s,host,roots,folder/("bake-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())),{});
  J report={{"status","PASS"},{"gp_surfaces",gp},{"hd_surfaces",hd},{"bake_directory",job.directory.generic_string()},{"executable",job.executable.generic_string()},{"identity",job.identity},{"inputs",J::array()},{"arguments",J::array()}};for(auto p:job.inputs)report["inputs"].push_back(p.generic_string());for(auto p:job.arguments)report["arguments"].push_back(p.generic_string());std::ofstream(folder/"result.json")<<report.dump(2);std::cout<<"PASS real surfaces GP="<<gp<<" HD="<<hd<<std::endl;
}
#include "geograft_inner.inl"
int main(int argc,char **argv){try{auto folder=fs::temp_directory_path()/("dfv-geograft-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));unit(folder);update_unit(folder);uv_unit(folder/"uv");if(argc>2&&std::string(argv[1])=="--update")update_real(fs::u8path(argv[2]));else if(argc>2&&std::string(argv[1])=="--uv-update")uv_real(fs::u8path(argv[2]));else if(argc>2&&std::string(argv[1])=="--hd-inner-recovery")hd_inner_recovery(fs::u8path(argv[2]));else if(argc>1)real(fs::u8path(argv[1]),fs::absolute("artifacts/geograft-native"),argc>2?fs::u8path(argv[2]):fs::path("artifacts/hd-bake-probe"));std::cout<<"PASS\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
