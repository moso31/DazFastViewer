#pragma once
#include "city/model.h"
#include <unordered_map>
namespace dfv::city {
struct Stats {
  size_t buildings=0,visible_instances=0,triangles=0,unique_triangles=0;
  std::array<size_t,4> cells{};
};
// Called only at a settled camera or an explicit edit. No scene work per rendered sample.
class Runtime {
  Cities cities_;
  std::unordered_map<std::string,uint32_t> indices_;
  std::map<std::string,std::vector<int>> levels_;
public:
  Stats stats;
  void bind(const Cities &,const ir::Scene &);
  bool apply(ir::Scene &,const Views &,ir::Delta *delta=nullptr);
};
}
