#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <memory>

namespace dfv::city {
// Persistent recipes use metres and stable cell/lot identities. Render resources are derived.
struct Config {
  std::filesystem::path directory;
  std::vector<std::string> assets;
  uint32_t seed=1337;
  int blocks_x=4,blocks_y=4,lots=4;
  double block_size=112,road_width=14,density=.85;
  double origin_x=0,origin_y=0,origin_z=0;
  std::array<double,3> distances{300,3000,30000};
  bool operator==(const Config &) const=default;
};
struct View {
  int forced=-1; // -1 automatic; 0 intentionally aliases 1.
  bool enabled=true,locked=false;
  bool operator==(const View &) const=default;
};
using Views=std::map<std::string,View>;
struct Building {
  std::string id;
  size_t asset=0,cell=0;
  ir::Transform transform;
  ir::Bounds bounds;
  bool landmark=false;
};
struct Cell {
  std::string id;
  ir::Bounds bounds;
  std::array<std::vector<std::string>,3> instances;
  std::string ground;
  size_t buildings=0;
};
struct Region {
  std::string proxy;
  std::vector<std::string> landmarks;
  std::vector<size_t> cells;
};
struct MaterialLink {std::string instance,source;};
struct City {
  std::string id;
  Config config;
  std::vector<Building> buildings;
  std::vector<Cell> cells;
  std::vector<Region> regions;
  std::vector<MaterialLink> materials;
  std::vector<std::string> material_sources;
  std::vector<std::string> warnings;
};
using Cities=std::vector<std::shared_ptr<const City>>;
void validate(const Config &);
void validate(const View &);
nlohmann::json json(const Config &);
Config config_from_json(const nlohmann::json &);
nlohmann::json json(const Views &);
Views views_from_json(const nlohmann::json &);
std::shared_ptr<const City> prefixed(const City &,const std::string &);
bool managed_instance(const Cities &,const std::string &);
bool material_entry(const Cities &,const std::string &);
std::vector<std::filesystem::path> discover(const std::filesystem::path &);
std::filesystem::path default_directory(const std::vector<std::filesystem::path> &);
using Progress=std::function<void(const std::string &)>;
}
