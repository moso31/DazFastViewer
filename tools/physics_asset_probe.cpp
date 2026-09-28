#include "editor/physics_service.h"
#include "editor/object_extension.h"
#include <fstream>
#include <iostream>
#include <thread>
// 本机资产只读验收；不保存源 DUF，不启动渲染。
int main(int argc,char **argv){using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
  if(argc!=4){std::cerr<<"PhysicsAssetProbe scene.duf project.json report.json\n";return 2;}
  J report=J::array();try{
    J project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;for(const auto &p:project.at("content_roots"))roots.push_back(fs::u8path(p.get<std::string>()));
    auto d=std::make_shared<editor::Document>();d->generation=1;d->loaded=daz::load(fs::u8path(argv[1]),{roots,false});std::cout<<"loaded\n"<<std::flush;
    d->catalog=daz::discover_morphs(d->loaded,roots,{},true);d->skeletons=daz::load_skeletons(d->loaded);d->formulas=daz::enable_formulas(d->catalog,d->skeletons);editor::release_load_data(*d);auto snapshot=editor::initial_snapshot(*d);std::cout<<"catalog "<<d->catalog.targets.size()<<'\n'<<std::flush;
    for(size_t t=0;t<d->catalog.targets.size();++t){const auto &target=d->catalog.targets[t];const auto &mesh=d->loaded.scene.meshes[d->loaded.scene.instances[target.instance].mesh];
      if(!editor::physics_eligible(*d,t))continue;auto kind=editor::physics_kind(*d,t);
      if(target.label.find("Skirt")==std::string::npos&&target.label.find("Bangs 5")==std::string::npos&&target.label.find("Bun 5")==std::string::npos&&kind!=runtime::PhysicsKind::hair_curves)continue;
      std::cout<<"simulate "<<target.label<<' '<<mesh.positions.size()<<'\n'<<std::flush;auto input=snapshot;input.values[t].visible=true;input.values[t].physics.enabled=true;input.values[t].physics.run_sequence=1;input.values[t].physics.rounds=120;editor::PhysicsService service;service.request(d,input,{});
      const auto begin=std::chrono::steady_clock::now();std::shared_ptr<editor::PhysicsResult> result;size_t frames=0;double first_ms=0,maximum_step=0;
      while(!result||result->stats.steps<120){service.request_frame();if(auto frame=service.take()){
        if(!frame->error.empty())throw std::runtime_error(target.label+": "+frame->error);
        if(!frames)first_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        ++frames;maximum_step=std::max(maximum_step,frame->stats.solve_ms);result=std::move(frame);
      }
        if(std::chrono::steady_clock::now()-begin>std::chrono::minutes(3)){service.cancel();throw std::runtime_error("asset timeout");}std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      J item={{"label",target.label},{"vertices",mesh.positions.size()},{"kind",int(kind)},{"error",result->error},{"first_frame_ms",first_ms},{"frames",frames},{"steps",result->stats.steps},{"sampled_step_max_ms",maximum_step},{"elapsed_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()},{"particles",result->stats.particles},{"prepare_ms",result->stats.prepare_ms},{"solve_ms",result->stats.solve_ms}};
      if(!result->error.empty())throw std::runtime_error(target.label+": "+result->error);if(result->delta.meshes.empty())throw std::runtime_error("缺少解算结果");
      double maximum=0;size_t moved=0,fixed=0;for(const auto &delta:result->delta.meshes){const auto original=std::find_if(result->restore.meshes.begin(),result->restore.meshes.end(),[&](const auto &e){return e.index==delta.index;});if(original==result->restore.meshes.end())throw std::runtime_error("缺少恢复数据");
        for(size_t v=0;v<delta.positions.size();++v){const auto a=original->positions[v],b=delta.positions[v];double distance=std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);if(!std::isfinite(distance))throw std::runtime_error("非有限顶点");maximum=std::max(maximum,distance);fixed+=distance<1e-6;moved+=distance>.001;}}
      item["max_move_m"]=maximum;item["moved"]=moved;item["fixed"]=fixed;report.push_back(item);std::ofstream(fs::u8path(argv[3]))<<report.dump(2);std::cout<<item.dump()<<'\n'<<std::flush;
    }
    if(report.size()<3)throw std::runtime_error("验收资产不足三类");return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
