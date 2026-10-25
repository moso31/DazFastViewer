#include "editor/document.h"
#include "editor/scene_extension.h"
#include "daz/documents.h"
#include <fstream>
#include <iostream>

// 对照原场景缓存的骨骼位置与运行时各阶段；仅向诊断目录写入。
int main(int argc,char **argv){try{
  using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
  if(argc!=5&&argc!=6)throw std::runtime_error("FigureDeformationProbe <scene.duf> <project.json> <label substring> <output> [wearable.duf]");
  const fs::path file=fs::u8path(argv[1]),output=fs::u8path(argv[4]);fs::create_directories(output);
  J project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;for(const auto &v:project.at("content_roots"))roots.push_back(fs::u8path(v.get<std::string>()));
  auto load=[&](const fs::path &path,bool deferred){editor::Document result;result.source_file=path;result.loaded=daz::load(path,{roots,false,deferred});result.catalog=daz::discover_morphs(result.loaded,roots,{},true);result.skeletons=daz::load_skeletons(result.loaded);result.formulas=daz::enable_formulas(result.catalog,result.skeletons);return result;};
  std::cout<<"load"<<std::endl;auto d=load(file,false);
  if(argc==6){const auto first=d.catalog.targets.size();std::cout<<"append wearable"<<std::endl;editor::append_document(d,load(fs::u8path(argv[5]),true));editor::attach_import(d,first,0);}
  const auto snapshot=editor::initial_snapshot(d);auto scene=d.loaded.scene;std::cout<<"evaluate"<<std::endl;runtime::DeformationRuntime r(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);r.evaluate(snapshot.values,snapshot.poses);
  auto uncorrected=d.loaded.scene;runtime::DeformationRuntime raw(uncorrected,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);raw.defer_collision();raw.evaluate(snapshot.values,snapshot.poses);
  if(argc==6)editor::save_scene_extension(output/"fitted.dufex",d,snapshot);
  const auto saved=daz::read_document_file(file);J report=J::array();auto vec=[](ir::Vec3 v){return J::array({v.x,v.y,v.z});};
  std::vector<ir::Scene> collision_steps;
  if(argc==6)for(int iterations=1;iterations<=3;++iterations){auto step=uncorrected;auto targets=d.catalog.targets;for(auto &target:targets)target.smoothing.collision_iterations=iterations;
    runtime::CollisionRuntime collision(step,targets);for(size_t t=0;t<targets.size();++t)collision.reference_positions(targets[t].instance,d.loaded.scene.meshes[d.loaded.scene.instances[targets[t].instance].mesh].positions);collision.evaluate({});collision_steps.push_back(std::move(step));}
  for(size_t t=0;t<d.catalog.targets.size();++t){const auto &target=d.catalog.targets[t];if(target.label.find(argv[3])==std::string::npos)continue;const auto &instance=scene.instances[target.instance];const auto &mesh=scene.meshes[instance.mesh];
    J row={{"id",target.id},{"label",target.label},{"morphs",J::array()},{"joints",J::array()}};
    auto dump=[&](const std::vector<ir::Vec3> &positions,const char *stage){std::ofstream obj(output/(std::to_string(t)+"-"+stage+".obj"));for(auto p:positions)obj<<"v "<<p.x<<' '<<p.y<<' '<<p.z<<'\n';for(const auto &f:mesh.triangles)obj<<"f "<<f.vertices[0]+1<<' '<<f.vertices[1]+1<<' '<<f.vertices[2]+1<<'\n';};
    dump(d.loaded.scene.meshes[instance.mesh].positions,"base");dump(mesh.positions,"final");dump(uncorrected.meshes[instance.mesh].positions,"uncorrected");row["transform"]=instance.transform.value;
    for(size_t k=0;k<collision_steps.size();++k)dump(collision_steps[k].meshes[instance.mesh].positions,("collision-"+std::to_string(k+1)).c_str());
    for(size_t m=0;m<target.morphs.size();++m){const auto &p=target.morphs[m];if(snapshot.values[t].morphs[m]!=0||r.effective()[t][m]!=0)row["morphs"].push_back({{"id",p.id},{"label",p.label},{"input",snapshot.values[t].morphs[m]},{"effective",r.effective()[t][m]},{"offsets",p.offset_count()},{"unsupported",p.unsupported}});}
    for(size_t s=0;s<d.skeletons.skins.size();++s){const auto &skin=d.skeletons.skins[s];if(skin.instance!=target.instance)continue;dump(r.skin_source(s),"morphed");const runtime::ConformLink *link=nullptr;for(const auto &l:r.conform_links())if(l.follower==t)link=&l;
      for(size_t j=0;j<skin.joints.size();++j){const auto &joint=skin.joints[j];const auto &p=r.effective_poses()[s][j];J node={{"id",joint.id},{"scene_id",joint.scene_id},{"parent",joint.parent},{"mapped",link&&j<link->joints.size()?link->joints[j]:-1},{"center",vec(joint.center_cm)},{"center_offset",vec(p.center_offset_cm)},{"end",vec(joint.end_cm)},{"end_offset",vec(p.end_offset_cm)},{"rotation",vec(p.rotation_degrees)},{"translation",vec(p.translation_cm)},{"scale",vec(p.scale)},{"general_scale",p.general_scale}};
        for(const auto &n:saved.at("scene").at("nodes"))if(n.value("id","")==joint.scene_id)node["preview"]=n.value("preview",J::object());row["joints"].push_back(node);
      }
    }report.push_back(row);
  }
  std::ofstream(output/"report.json")<<report.dump(2);std::cout<<"done "<<report.size()<<std::endl;return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
