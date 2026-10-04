#include "city/model.h"
#include "daz/documents.h"
#include "daz/loader.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace dfv::city {
namespace { std::string path(const std::filesystem::path &p){const auto s=p.generic_u8string();return {s.begin(),s.end()};} }
void validate(const Config &c){
  for(double x:{c.block_size,c.road_width,c.density,c.origin_x,c.origin_y,c.origin_z,c.distances[0],c.distances[1],c.distances[2]})if(!std::isfinite(x))throw std::runtime_error("城市参数必须为有限数值");
  if(c.blocks_x<1||c.blocks_y<1||c.blocks_x>32||c.blocks_y>32||c.lots<1||c.lots>6||int64_t(c.blocks_x)*c.blocks_y*c.lots*c.lots>20000)throw std::runtime_error("城市范围超出首版预算（最多 32×32 街区、20000 地块）");
  if(c.block_size<40||c.block_size>1000||c.road_width<2||c.road_width>100||c.density<=0||c.density>1)throw std::runtime_error("街区边长应为 40–1000 米，道路宽度 2–100 米，密度 0–1");
  if(c.distances[0]<20||c.distances[1]<=c.distances[0]*1.3||c.distances[2]<=c.distances[1]*1.3||c.distances[2]>1000000)throw std::runtime_error("LOD 距离必须递增，且相邻档至少相差 30%");
  if(std::max({std::abs(c.origin_x),std::abs(c.origin_y),std::abs(c.origin_z)})>100000)throw std::runtime_error("首版城市原点范围为 ±100 公里");
  std::set<std::string> unique;for(const auto &a:c.assets){const auto p=std::filesystem::u8path(a);if(p.has_parent_path()||p.extension()!=L".duf"||!unique.insert(a).second)throw std::runtime_error("城市资源必须是目录内不重复的建筑 DUF 文件名");}
}
void validate(const View &v){if(v.forced < -1||v.forced>4)throw std::runtime_error("城市 LOD 应为自动或 0–4");}
nlohmann::json json(const Config &c){validate(c);return {{"version",1},{"directory",path(c.directory)},{"assets",c.assets},{"seed",c.seed},{"blocks_x",c.blocks_x},{"blocks_y",c.blocks_y},{"lots",c.lots},{"block_size",c.block_size},{"road_width",c.road_width},{"density",c.density},{"origin",{c.origin_x,c.origin_y,c.origin_z}},{"distances",c.distances}};}
Config config_from_json(const nlohmann::json &j){if(j.at("version")!=1)throw std::runtime_error("不支持的城市配方版本");Config c;c.directory=std::filesystem::u8path(j.at("directory").get<std::string>());c.assets=j.at("assets").get<std::vector<std::string>>();c.seed=j.at("seed");c.blocks_x=j.at("blocks_x");c.blocks_y=j.at("blocks_y");c.lots=j.at("lots");c.block_size=j.at("block_size");c.road_width=j.at("road_width");c.density=j.at("density");const auto o=j.at("origin").get<std::array<double,3>>();c.origin_x=o[0];c.origin_y=o[1];c.origin_z=o[2];c.distances=j.at("distances").get<std::array<double,3>>();validate(c);return c;}
nlohmann::json json(const Views &v){auto j=nlohmann::json::object();for(const auto &[id,x]:v){validate(x);j[id]={{"forced",x.forced},{"enabled",x.enabled},{"locked",x.locked}};}return j;}
Views views_from_json(const nlohmann::json &j){Views v;for(const auto &[id,x]:j.items()){auto &a=v[id];a.forced=x.at("forced");a.enabled=x.at("enabled");a.locked=x.at("locked");validate(a);}return v;}
std::shared_ptr<const City> prefixed(const City &src,const std::string &p){auto c=std::make_shared<City>(src);c->id=p+c->id;for(auto &b:c->buildings)b.id=p+b.id;for(auto &b:c->cells){b.id=p+b.id;b.ground=p+b.ground;for(auto &level:b.instances)for(auto &id:level)id=p+id;}for(auto &r:c->regions){if(!r.proxy.empty())r.proxy=p+r.proxy;for(auto &s:r.landmarks)s=p+s;}for(auto &m:c->materials){m.instance=p+m.instance;m.source=p+m.source;}for(auto &s:c->material_sources)s=p+s;return c;}
bool managed_instance(const Cities &cities,const std::string &id){for(const auto &c:cities)if(id.starts_with(c->id+"/"))return true;return false;}
bool material_entry(const Cities &cities,const std::string &id){for(const auto &c:cities)if(id.starts_with(c->id+"/"))return std::find(c->material_sources.begin(),c->material_sources.end(),id)!=c->material_sources.end();return true;}
std::vector<std::filesystem::path> discover(const std::filesystem::path &folder){
  if(!std::filesystem::is_directory(folder))throw std::runtime_error("建筑目录不存在");std::vector<std::filesystem::path> paths;
  for(const auto &e:std::filesystem::directory_iterator(folder))if(e.is_regular_file()&&e.path().extension()==L".duf"){
    auto d=daz::document_view(e.path());if(d->contains("scene")&&d->at("scene").contains("nodes"))for(const auto &n:d->at("scene").at("nodes"))if(n.contains("geometries")&&!n.at("geometries").empty()){paths.push_back(e.path());break;}
  }
  std::sort(paths.begin(),paths.end());if(paths.empty())throw std::runtime_error("目录中没有建筑模型 DUF（材质预设不计入）");if(paths.size()>64)throw std::runtime_error("首版最多使用 64 个建筑原型，请选择较小的资源目录");return paths;
}
std::filesystem::path default_directory(const std::vector<std::filesystem::path> &roots){const std::filesystem::path tail=L"Environments/PhilW/Night and Day City/03 Buildings";for(const auto &r:roots)if(std::filesystem::is_directory(r/tail))return r/tail;const auto p=std::filesystem::path(L"G:/G3")/tail;return std::filesystem::is_directory(p)?p:std::filesystem::path{};}
}
