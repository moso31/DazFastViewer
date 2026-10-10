#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <map>

namespace dfv::water {
// Metres, seconds and degrees. Time is an authored sample, never a running clock.
struct Source {
  std::string id;
  bool volume=false;
  bool operator==(const Source &) const=default;
};
struct Config {
  double width=60000,length=60000,x=0,y=0,level=0;
  double depth=20,clarity=12,wave_height=.6,wavelength=24,steepness=.35,direction=25;
  double time=0,ripples=.22,roughness=.08,foam_width=1.5,foam_strength=.65,precision=2;
  double density=1,foam_uv_scale=1;
  ir::Vec3 color{.018f,.12f,.16f};
  uint32_t seed=1337;
  bool coast=false,scan_scene=true;
  bool manual_lod=false;
  int lod_level=0; // 0 为当前密度的最高精度；每级将网格间距扩大一倍。
  std::vector<Source> sources;
  bool operator==(const Config &) const=default;
};
struct Wave {double x=0,y=0,z=0,foam=0;};
// Dyadic contact cells belong to the interaction grid, not the camera's mesh.
// Samples are signed clearance at four corners (CCW) and the centre, in metres.
struct Cell {
  uint32_t x=0,y=0;int level=0;
  std::array<double,5> clearance{};
  bool operator==(const Cell &) const=default;
};
struct Cache {
  uint64_t stamp=0;
  std::vector<Cell> cells;
  std::vector<std::string> warnings;
  size_t objects=0;
  double sampling_spacing=0;
};
struct Water {
  std::string id;
  Config config;
  std::shared_ptr<const Cache> cache;
};
using Waters=std::vector<std::shared_ptr<const Water>>;
// 精确复用同一组四叉树叶节点；不改变采样间距或水面精度。
struct MeshCache {
  struct Entry {std::vector<Cell> leaves;std::shared_ptr<const ir::Mesh> mesh;};
  std::string id;
  Config config;
  std::shared_ptr<const Cache> coast;
  std::map<double,Entry> entries;
  size_t builds=0,hits=0;
};
struct Candidate {std::string id,label;double volume=0;};
using Progress=std::function<void(const std::string &)>;
void validate(const Config &);
struct LodLayout {int maximum_level=0,depth=0;uint64_t side=1,vertices=5,triangles=4,bytes=0;};
LodLayout lod_layout(const Config &);
void check_manual_lod_budget(const Water &);
Wave wave(const Config &,double x,double y);
bool same_shape(const Config &,const Config &);
nlohmann::json json(const Water &);
std::shared_ptr<const Water> from_json(const nlohmann::json &);
std::shared_ptr<const Water> prefixed(const Water &,const std::string &);
const Water *find(const Waters &,const std::string &instance_or_target);
ir::Material material(const Water &);
// Placement uses the undeformed surface, independent of waves and camera LOD.
ir::Bounds reference_bounds(const Water &,const ir::Transform &world);
// Camera coordinates are water-local. Cache cells always retain their resolution.
ir::Mesh mesh(const Water &,ir::Vec3 eye={0,0,10},const Progress &progress={});
std::shared_ptr<const ir::Mesh> cached_mesh(const Water &,ir::Vec3 eye,MeshCache &,const Progress &progress={});
struct Obstacle {std::string id;ir::Mesh mesh;ir::Transform transform;bool volume=false;};
std::shared_ptr<const Cache> calculate(const Water &,const std::vector<Obstacle> &,uint64_t stamp,const Progress &progress={});
}
