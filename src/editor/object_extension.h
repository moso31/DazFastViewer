#pragma once
#include "editor/document.h"
#include "runtime/weight.h"
#include <condition_variable>
#include <thread>

namespace dfv::editor {
bool growth_character(const Document &document,size_t target);
void normalize_object_extensions(const Document &document,Snapshot &snapshot);
// scale_step 为仅按敏感度缩放的带符号步长；年龄编辑由实际年龄差计算。
std::vector<std::string> edit_growth(const Document &document,Snapshot &snapshot,size_t target,runtime::ObjectExtension next,bool apply_shape=true,double scale_step=0);
runtime::WeightResult measure_object(const Document &document,const Snapshot &snapshot,size_t target,std::optional<ir::Transform> world={});
class MeasurementService {
public:
  struct Result {uint64_t serial=0;runtime::WeightResult weight;std::string error;};
private:
  struct Request {uint64_t serial=0;std::shared_ptr<const Document> document;Snapshot snapshot;size_t target=0;std::optional<ir::Transform> world;std::shared_ptr<const std::vector<ir::Vec3>> positions;};
  std::mutex mutex_;
  std::condition_variable_any ready_;
  Request pending_;
  Result result_;
  std::jthread worker_;
public:
  MeasurementService();
  ~MeasurementService();
  void request(uint64_t serial,std::shared_ptr<const Document> document,Snapshot snapshot,size_t target,std::optional<ir::Transform> world={},std::shared_ptr<const std::vector<ir::Vec3>> positions={});
  Result result();
};
}
