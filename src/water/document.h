#pragma once
#include "water/model.h"
namespace dfv::editor {struct Document;struct Snapshot;}
namespace dfv::water {
void install(editor::Document &,std::shared_ptr<const Water>,bool record=true);
Waters effective(const editor::Document &,const editor::Snapshot &);
std::vector<Candidate> candidates(const editor::Document &,const std::vector<ir::Bounds> &instance_bounds);
bool materials(const Waters &,ir::Scene &,ir::Delta *delta=nullptr);
uint64_t input_stamp(const editor::Document &,const editor::Snapshot &);
struct Inputs {std::vector<Obstacle> objects;std::vector<std::string> warnings;};
// Called on the render owner thread. Copy only collision geometry, never a Document.
Inputs inputs(const editor::Document &,const ir::Scene &evaluated,const Water &,const Progress &progress={});
std::shared_ptr<const Cache> calculate(const Water &,const Inputs &,uint64_t stamp,const Progress &progress={});
std::shared_ptr<const Cache> recalculate(const editor::Document &,const editor::Snapshot &,const Water &,const Progress &progress={});
bool adapt(const Waters &,ir::Scene &,ir::Vec3 world_eye,const Progress &progress={});
class Runtime {
  std::map<std::string,MeshCache> meshes_;
public:
  void clear(){meshes_.clear();}
  bool adapt(const Waters &,ir::Scene &,ir::Vec3 world_eye,const Progress &progress={});
};
}
