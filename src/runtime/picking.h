#pragma once
#include "runtime/skeleton.h"
#include "bench/camera.h"
#include <limits>

namespace dfv::runtime {
struct Target;
struct PickHit {int instance=-1,triangle=-1;float distance=std::numeric_limits<float>::max();};
struct PickingStats {uint64_t mesh_builds=0,instance_updates=0;};
class PickingScene {
  struct Face {ir::Vec3 a,b,c;int instance,triangle;};
  struct Branch {ir::Bounds bounds;int begin=0,end=0,left=-1,right=-1;};
  struct MeshTree {std::vector<Face> faces;std::vector<Branch> branches;};
  struct Instance {uint32_t mesh=0;ir::Transform inverse;ir::Bounds bounds;bool visible=false,pickable=true;};
  std::vector<MeshTree> meshes_;
  std::vector<Instance> instances_;
  std::vector<uint8_t> used_meshes_;
  PickingStats stats_;
  int build(MeshTree &mesh,int begin,int end);
  void mesh(const ir::Scene &scene,uint32_t index);
  void instance(const ir::Scene &scene,uint32_t index);
public:
  void update(const ir::Scene &scene,const std::vector<uint8_t> &pickable={});
  void apply(const ir::Scene &scene,const ir::Delta &delta);
  const auto &stats() const {return stats_;}
  PickHit ray(ir::Vec3 origin,ir::Vec3 direction) const;
  PickHit screen(const CameraState &camera,int x,int y,int width,int height) const;
};
int hit_joint(const ir::Mesh &mesh,int triangle,const Skin &skin);
struct JointRegions {
  std::vector<int> detail,body,parents;
  int head=-1;
  bool within_head(int joint) const;
};
JointRegions joint_regions(const ir::Mesh &mesh,const Skin &skin);
std::vector<uint8_t> viewport_pick_mask(size_t instances,const std::vector<Target> &targets);
struct HoverRegion {int instance=-1,joint=-1;};
HoverRegion hover_region(const PickHit &hit,int selected_instance,int selected_joint,const std::vector<JointRegions> &regions);
bool parameter_on_node(const std::string &owner,const std::string &group,const std::string &node);
}
