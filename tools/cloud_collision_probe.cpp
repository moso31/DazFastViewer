#include "cloud/document.h"
#include "editor/document.h"
#include "editor/object_hierarchy.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

// Read-only real-asset validation, including evaluated morphs/skin and large scale.
int main(int argc,char **argv){using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
  if(argc!=4){std::cerr<<"CloudCollisionProbe scene.duf project.json output-folder\n";return 2;}
  const auto output=fs::absolute(fs::u8path(argv[3]));fs::create_directories(output);
  try{
    J project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;for(const auto &p:project.at("content_roots"))roots.push_back(fs::u8path(p.get<std::string>()));
    editor::Document d;d.generation=1;d.loaded=daz::load(fs::u8path(argv[1]),{roots,false});
    d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);editor::release_load_data(d);
    size_t target=0;for(;target<d.catalog.targets.size();++target)if(d.catalog.targets[target].label.find("Seo Hyun")!=std::string::npos)break;
    if(target==d.catalog.targets.size())throw std::runtime_error("Seo Hyun target missing");
    const editor::ObjectHierarchy hierarchy(d);auto cloud=std::make_shared<cloud::Cloud>();cloud->id="probe-cloud";auto &cfg=cloud->config;cfg.sources={hierarchy.targets.at(target)};cfg.width=cfg.length=1e6;cfg.height=-50000;cfg.thickness=1e5;cfg.padding=cfg.softness=0;cloud::install(d,cloud,false);
    auto snapshot=editor::initial_snapshot(d);auto scene=d.loaded.scene;runtime::DeformationRuntime deform(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
    cloud::Runtime collisions;J checks=J::array();
    for(double scale:{1.,1000.}){
      snapshot.values.at(target).transform.general_scale=scale;
      for(;;){const auto ready=deform.prepare(snapshot.values,snapshot.poses);if(!ready.error.empty())throw std::runtime_error(ready.error);if(!ready.pending)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
      auto delta=deform.evaluate(snapshot.values,snapshot.poses);const auto start=std::chrono::steady_clock::now();collisions.apply(d,{cloud},scene,&delta);const double milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      const auto &volume=*scene.materials.back().cloud;if(volume.colliders.size()!=1)throw std::runtime_error("evaluated character hull missing");const auto c=volume.colliders[0];
      ir::Bounds bounds;std::vector<ir::Vec3> world;std::vector<std::array<uint32_t,3>> triangles;
      for(size_t i=0;i<hierarchy.instances.size();++i){const auto &object=scene.instances[i];if(!object.visible||!hierarchy.contains(cfg.sources[0],hierarchy.instances[i]))continue;
        if(std::all_of(object.materials.begin(),object.materials.end(),[&](auto m){return scene.materials[m].opacity<=0;}))continue;
        const auto &mesh=scene.meshes[object.mesh];const uint32_t offset=uint32_t(world.size());for(auto p:mesh.positions)world.push_back(object.transform.point(p));
        for(const auto &f:mesh.triangles)if(mesh.draws(f)){triangles.push_back({f.vertices[0]+offset,f.vertices[1]+offset,f.vertices[2]+offset});for(auto v:f.vertices)bounds.add(world[offset+v]);}
        for(const auto &curve:mesh.curves)for(auto v:curve.vertices)bounds.add(world[offset+v]);
      }
      const auto lo=bounds.minimum,hi=bounds.maximum,center=bounds.center();size_t removed=0,retained=0,old_removed=0;
      for(int z=0;z<29;++z)for(int y=0;y<29;++y)for(int x=0;x<29;++x){const float u=(x+.5f)/29,v=(y+.5f)/29,w=(z+.5f)/29;const ir::Vec3 p{lo.x+(hi.x-lo.x)*(u*1.8f-.4f),lo.y+(hi.y-lo.y)*(v*1.8f-.4f),lo.z+(hi.z-lo.z)*(w*1.8f-.4f)};
        const double qx=(p.x-center.x)/((hi.x-lo.x)*.8660254),qy=(p.y-center.y)/((hi.y-lo.y)*.8660254),qz=(p.z-center.z)/((hi.z-lo.z)*.8660254);const bool old=qx*qx+qy*qy+qz*qz<1,new_hole=cloud::transmission(c,p)<.5f;
        old_removed+=old;removed+=new_hole;retained+=old&&!new_hole;
        if(new_hole&&(p.x<lo.x||p.x>hi.x||p.y<lo.y||p.y>hi.y||p.z<lo.z||p.z>hi.z))throw std::runtime_error("zero padding still removes outside source AABB");
      }
      if(!removed||!retained||c.planes.size()>ir::max_cloud_planes)throw std::runtime_error("convex fit/budget regression");
      const auto builds=collisions.hull_builds();cfg.time=5;delta={};collisions.apply(d,{cloud},scene,&delta);if(collisions.hull_builds()!=builds||!delta.meshes.empty())throw std::runtime_error("time scrub rebuilt hull/geometry");cfg.time=0;
      checks.push_back({{"scale_multiplier",scale},{"source_vertices",world.size()},{"hull_planes",c.planes.size()},{"update_ms",milliseconds},{"hull_builds",builds},{"old_ellipsoid_removed_samples",old_removed},{"convex_removed_samples",removed},{"cloud_samples_restored",retained}});std::cout<<checks.back().dump()<<std::endl;
      if(scale==1){J geometry={{"positions",J::array()},{"triangles",J::array()}};const float factor=40/(hi.z-lo.z);for(auto p:world)geometry["positions"].push_back({(p.x-center.x)*factor,(p.y-center.y)*factor,(p.z-center.z)*factor});for(const auto &f:triangles)geometry["triangles"].push_back(f);std::ofstream(output/"character.json")<<geometry.dump();}
    }
    std::ofstream(output/"result.json")<<J{{"result","PASS"},{"scene",argv[1]},{"label",d.catalog.targets[target].label},{"checks",checks}}.dump(2);return 0;
  }catch(const std::exception &e){std::ofstream(output/"error.txt")<<e.what();std::cerr<<e.what()<<'\n';return 1;}
}
