#pragma once
#include "city/model.h"
#include "daz/loader.h"
namespace dfv::city {
struct Generated {daz::LoadedScene loaded;std::shared_ptr<const City> city;};
// A complete temporary result is produced before editor state is touched.
Generated generate(Config config,const std::string &id,const std::vector<std::filesystem::path> &roots,const Progress &progress={});
// Exposed to tests and synthetic benchmarks, without requiring a content library.
struct Asset {std::string name;ir::Mesh detailed,simple;std::vector<ir::Material> materials;std::vector<ir::Texture> textures;ir::Bounds bounds;};
Generated generate(Config config,const std::string &id,std::vector<Asset> assets,const Progress &progress={});
Asset prepare_asset(const std::filesystem::path &,const std::vector<std::filesystem::path> &roots);
}
