#pragma once
#include "render_ir/scene.h"
#include "runtime/morph_data.h"
#include "runtime/rigid_follow.h"
#include <set>
#include <map>

namespace dfv::runtime {
struct Morph {
  std::string id,label,group,source,unsupported;
  // 参数目录包含控制器和子节点别名；发现能力与求值能力分开记录。
  std::string kind="sparse",owner,channel_id,alias_target,channel_name;
  std::string value_type="float",intrinsic_error;
  std::string limitation;
  bool evaluable=false,locked=false;
  bool scene_channel=false;
  int alias_morph=-1;
  size_t formula_count=0,missing_dependencies=0;
  // 部分商业资源用 -1 表示没有声明目标总数，不能转换成 size_t。
  int64_t source_vertex_count=0;
  size_t source_offset_count=0,repaired_references=0;
  std::string geometry_validation;
  float minimum=0,maximum=1,initial=0,step=.01f;
  bool clamped=true,visible=true,auto_follow=false;
  std::vector<SparseOffset> offsets;
  std::shared_ptr<MorphPayload> payload;
  size_t offset_count() const {return payload?payload->count:offsets.size();}
  bool has_offsets() const {return offset_count()!=0;}
  OffsetView data() const {return payload?OffsetView(payload->ensure()):OffsetView(offsets);}
};
struct Target {
  std::string id,label,parent;
  // DUF 节点收藏：空键为对象本身，其他键为骨骼资产 ID；空集合也代表已保存的列表。
  std::string favorite_scope;
  std::map<std::string,std::set<std::string>> favorites;
  uint32_t instance=0;
  std::vector<Morph> morphs;
  std::string conform_target;
  ir::MeshSmoothing smoothing;
  std::vector<std::string> ancestors;
  ir::Transform edit_frame,translation_frame;
  ir::Vec3 base_rotation_degrees{};
  std::string rotation_order="XYZ";
  bool has_edit_frame=false;
  RigidFollow rigid_follow;
  bool attachment_bind_rest=false;
};
struct TransformValues {
  ir::Vec3 translation_cm{},rotation_degrees{},scale{1,1,1};
  float general_scale=1;
};
struct Properties {
  std::vector<float> morphs;TransformValues transform;bool visible=true;std::set<std::string> unlimited_morphs;
  double ground_alignment_ratio=0; // 每个角色独立的操作设置，修改比例本身不触发几何求值。
};
void validate_transform(const TransformValues &value);
ir::Transform make_transform(const TransformValues &value,const std::string &rotation_order="XYZ");
ir::Transform parameter_transform(const TransformValues &value,const Target &target,const ir::Transform &loaded_transform);
struct EvaluationStats {uint64_t morph_evaluations=0,offsets_visited=0,transform_evaluations=0;};
// 单工作线程拥有；Qt 只发送属性快照，不访问求值中的网格。
class MorphRuntime {
  ir::Scene &scene_;
  const std::vector<Target> &targets_;
  std::vector<Properties> values_;
  std::vector<std::vector<ir::Vec3>> bases_;
  std::vector<std::vector<ir::Vec3>> follow_offsets_;
  std::vector<ir::Transform> transforms_;
  std::vector<ir::Transform> attachments_;
  std::vector<int> parents_;
  std::vector<std::set<size_t>> active_;
  std::set<size_t> dirty_meshes_,dirty_transforms_;
  EvaluationStats stats_;
  ir::Transform local_transform(size_t target) const;
  void dirty_transform(size_t target);
public:
  MorphRuntime(ir::Scene &scene,const std::vector<Target> &targets);
  bool set_morph(size_t target,size_t morph,float value,bool enforce_limits=true);
  bool set_transform(size_t target,const TransformValues &value);
  void set_visible(size_t target,bool visible);
  void bind_parent(size_t target,size_t parent);
  void set_attachment(size_t target,const ir::Transform &delta);
  bool set_follow_offsets(size_t target,const std::vector<ir::Vec3> &offsets);
  ir::Delta evaluate();
  // 先消去共同祖先，再求相对矩阵；共同刚性移动不会制造浮点差异。
  ir::Transform relative_transform(uint32_t source,uint32_t follower) const;
  const auto &values() const {return values_;}
  const auto &stats() const {return stats_;}
};
}
