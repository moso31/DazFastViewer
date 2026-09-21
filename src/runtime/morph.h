#pragma once
#include "render_ir/scene.h"
#include <set>

namespace dfv::runtime {
struct SparseOffset {uint32_t vertex=0;ir::Vec3 delta;};
struct Morph {
  std::string id,label,group,source,unsupported;
  // 参数目录包含控制器和子节点别名；发现能力与求值能力分开记录。
  std::string kind="sparse",owner,channel_id,alias_target,channel_name;
  std::string value_type="float",intrinsic_error;
  bool evaluable=false,locked=false;
  int alias_morph=-1;
  size_t formula_count=0,missing_dependencies=0;
  // 部分商业资源用 -1 表示没有声明目标总数，不能转换成 size_t。
  int64_t source_vertex_count=0;
  size_t source_offset_count=0,repaired_references=0;
  std::string geometry_validation;
  float minimum=0,maximum=1,initial=0,step=.01f;
  bool clamped=true,visible=true,auto_follow=false;
  std::vector<SparseOffset> offsets;
};
struct Target {
  std::string id,label,parent;
  uint32_t instance=0;
  std::vector<Morph> morphs;
  std::string conform_target;
};
struct TransformValues {
  ir::Vec3 translation_cm{},rotation_degrees{},scale{1,1,1};
};
struct Properties {std::vector<float> morphs;TransformValues transform;};
void validate_transform(const TransformValues &value);
ir::Transform make_transform(const TransformValues &value);
struct EvaluationStats {uint64_t morph_evaluations=0,offsets_visited=0,transform_evaluations=0;};
// 单工作线程拥有；Qt 只发送属性快照，不访问求值中的网格。
class MorphRuntime {
  ir::Scene &scene_;
  const std::vector<Target> &targets_;
  std::vector<Properties> values_;
  std::vector<std::vector<ir::Vec3>> bases_;
  std::vector<std::vector<ir::Vec3>> follow_offsets_;
  std::vector<ir::Transform> transforms_;
  std::vector<int> parents_;
  std::vector<std::set<size_t>> active_;
  std::set<size_t> dirty_meshes_,dirty_transforms_;
  EvaluationStats stats_;
public:
  MorphRuntime(ir::Scene &scene,const std::vector<Target> &targets);
  bool set_morph(size_t target,size_t morph,float value);
  bool set_transform(size_t target,const TransformValues &value);
  bool set_follow_offsets(size_t target,const std::vector<ir::Vec3> &offsets);
  ir::Delta evaluate();
  const auto &values() const {return values_;}
  const auto &stats() const {return stats_;}
};
}
