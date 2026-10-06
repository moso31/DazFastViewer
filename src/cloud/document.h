#pragma once
#include "cloud/model.h"
#include "cloud/collision.h"
#include <map>
namespace dfv::editor {struct Document;struct Snapshot;}
namespace dfv::cloud {
void install(editor::Document &,std::shared_ptr<const Cloud>,bool record=true);
Clouds effective(const editor::Document &,const editor::Snapshot &);
std::vector<Candidate> candidates(const editor::Document &,const std::vector<ir::Bounds> &instance_bounds={});
// Owned by the render thread. Bounds are cached and invalidated by geometry deltas.
class Runtime {
  std::map<uint32_t,std::shared_ptr<const Hull>> meshes_;
  struct Member {
    uint32_t instance;
    ir::Transform transform;
    std::shared_ptr<const Hull> hull;
    bool operator==(const Member &) const=default;
  };
  struct Source {std::vector<Member> members;Hull hull;};
  std::map<std::string,Source> sources_;
  size_t hull_builds_=0;
public:
  void clear(){meshes_.clear();sources_.clear();}
  size_t hull_builds()const{return hull_builds_;}
  bool apply(const editor::Document &,const Clouds &,ir::Scene &,ir::Delta *delta=nullptr);
};
}
