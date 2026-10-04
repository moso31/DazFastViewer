#include "city/runtime.h"
#include <cmath>
#include <unordered_set>
namespace dfv::city {
void Runtime::bind(const Cities &cities,const ir::Scene &scene){cities_=cities;indices_.clear();levels_.clear();stats={};for(uint32_t i=0;i<scene.instances.size();++i)indices_.emplace(scene.instances[i].id,i);}
bool Runtime::apply(ir::Scene &s,const Views &views,ir::Delta *delta){
  stats={};bool layout=false;const auto eye=s.camera.transform.point({});
  auto visibility=[&](const std::string &id,bool visible){if(id.empty())return;const auto i=indices_.at(id);auto &v=s.instances[i];if(v.visible!=visible){v.visible=visible;if(delta)delta->visibility.push_back({i,visible});}};
  for(const auto &entry:cities_){const auto &c=*entry;auto found=views.find(c.id);const View view=found==views.end()?View{}:found->second;validate(view);auto &levels=levels_[c.id];const bool initial=levels.size()!=c.cells.size();if(initial)levels.assign(c.cells.size(),1);
    for(size_t ci=0;ci<c.cells.size();++ci){const auto &cell=c.cells[ci];const auto center=cell.bounds.center();const auto &ground=s.instances.at(indices_.at(cell.ground));ir::Bounds bounds;
      for(int k=0;k<8;++k)bounds.add(ground.transform.point({(k&1?cell.bounds.maximum.x:cell.bounds.minimum.x)-center.x,(k&2?cell.bounds.maximum.y:cell.bounds.minimum.y)-center.y,k&4?cell.bounds.maximum.z:cell.bounds.minimum.z}));
      const double dx=std::max({double(bounds.minimum.x-eye.x),0.,double(eye.x-bounds.maximum.x)}),dy=std::max({double(bounds.minimum.y-eye.y),0.,double(eye.y-bounds.maximum.y)}),dz=std::max({double(bounds.minimum.z-eye.z),0.,double(eye.z-bounds.maximum.z)});
      const double projection=std::tan(s.camera.fov*.5)/std::tan(.4)*900./std::max(1,std::min(s.camera.width,s.camera.height));const double distance=std::sqrt(dx*dx+dy*dy+dz*dz)*projection;
      auto &level=levels[ci];if(view.forced>=0)level=std::max(1,view.forced);else if(!view.locked||initial){if(initial){level=1;while(level<4&&distance>c.config.distances[level-1])++level;}else {while(level<4&&distance>c.config.distances[level-1]*1.15)++level;while(level>1&&distance<c.config.distances[level-2]*.85)--level;}}
    }
    auto active=levels;for(const auto &region:c.regions){const bool far=view.enabled&&std::all_of(region.cells.begin(),region.cells.end(),[&](size_t ci){return levels[ci]==4;});visibility(region.proxy,far);for(const auto &id:region.landmarks)visibility(id,far);if(!far)for(auto ci:region.cells)if(active[ci]==4)active[ci]=3;}
    for(size_t ci=0;ci<c.cells.size();++ci){const auto &cell=c.cells[ci];visibility(cell.ground,view.enabled);for(int l=0;l<3;++l)for(const auto &id:cell.instances[l])visibility(id,view.enabled&&active[ci]==l+1);if(view.enabled){++stats.cells[active[ci]-1];stats.buildings+=cell.buildings;}}
    // Keep all repeated buildings on the same shader binding after editing a shared source.
    for(const auto &link:c.materials){auto &target=s.instances.at(indices_.at(link.instance));const auto &source=s.instances.at(indices_.at(link.source));if(target.materials!=source.materials){target.materials=source.materials;layout=true;}}
  }
  std::unordered_set<uint32_t> meshes;for(const auto &[id,index]:indices_){const auto &i=s.instances[index];if(!i.visible||!managed_instance(cities_,id))continue;++stats.visible_instances;const auto tris=s.meshes.at(i.mesh).triangles.size();stats.triangles+=tris;if(meshes.insert(i.mesh).second)stats.unique_triangles+=tris;}
  return layout;
}
}
