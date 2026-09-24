#include "runtime/subdivision.h"
#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/stencilTableFactory.h>
#include <map>
#include <tuple>
#include <numeric>
#include <stdexcept>
#include <set>
#include <cmath>

namespace dfv::runtime {
void validate_subdivision_budget(const ir::Mesh &mesh,int level) {
  if(level<0||level>6) throw std::runtime_error("细分等级超出支持范围 0–6");
  if(level==0||mesh.triangles.empty()) return;
  if(mesh.polygons.empty()) throw std::runtime_error("该网格缺少细分所需的多边形拓扑");
  uint64_t faces=0;
  for(const auto &p:mesh.polygons) {
    if(p.vertices.size()<3||p.uv.size()!=p.vertices.size()) throw std::runtime_error("细分多边形或 UV 无效");
    std::set<uint32_t> unique;
    for(size_t i=0;i<p.vertices.size();++i) if(p.vertices[i]>=mesh.positions.size()||!unique.insert(p.vertices[i]).second||!std::isfinite(p.uv[i].x)||!std::isfinite(p.uv[i].y)) throw std::runtime_error("细分多边形包含无效或重复顶点／UV");
    faces+=mesh.subdivision.algorithm==2?(p.vertices.size()-2)*4:p.vertices.size();
  }
  for(int i=1;i<level&&faces<=4000000;++i) faces*=4;
  if(faces>4000000) throw std::runtime_error("该细分等级超过单网格 400 万细分面的预算，请降低等级；当前画面保持不变");
  for(const auto &c:mesh.creases) if(c.a>=mesh.positions.size()||c.b>=mesh.positions.size()||c.a==c.b||!std::isfinite(c.weight)||c.weight<0) throw std::runtime_error("细分边折痕索引或权重无效");
  for(auto [v,w]:mesh.corners) if(v>=mesh.positions.size()||!std::isfinite(w)||w<0) throw std::runtime_error("细分角点索引或权重无效");
}
namespace osd=OpenSubdiv;
struct Subdivision::Data {
  std::unique_ptr<const osd::Far::StencilTable> stencils;
  std::vector<ir::Triangle> triangles;
  size_t cage_size=0;
};
namespace {
struct UV {
  float x=0,y=0;
  void Clear() {x=y=0;}
  void AddWithWeight(const UV &v,float w) {x+=v.x*w;y+=v.y*w;}
};
}
Subdivision::Subdivision(const ir::Mesh &mesh,bool final_render) {
  const auto &settings=mesh.subdivision;
  const int level=final_render?std::max(settings.level,settings.render_level):settings.level;
  if(!settings.enabled||level==0||mesh.triangles.empty()) return;
  validate_subdivision_budget(mesh,level);
  if(level<0||level>6) throw std::runtime_error("SubD 级别超出当前支持范围 0–6："+mesh.id);
  if(settings.algorithm<0||settings.algorithm>3||settings.edge_interpolation<0||settings.edge_interpolation>2) throw std::runtime_error("未知 SubD 算法或边界插值模式："+mesh.id);
  if(mesh.polygons.empty()) throw std::runtime_error("细分缺少原始多边形拓扑："+mesh.id);
  std::vector<int> sizes,vertices,uv_indices,parents,holes,crease_indices,corner_indices;
  std::vector<float> crease_weights,corner_weights;
  std::vector<UV> uv_values;
  std::map<std::tuple<uint32_t,float,float>,int> uv_map;
  auto add_face=[&](const ir::Polygon &polygon,uint32_t source,std::span<const int> corners) {
    if(std::binary_search(mesh.hidden_polygons.begin(),mesh.hidden_polygons.end(),source)) holes.push_back(int(sizes.size()));
    sizes.push_back(int(corners.size()));parents.push_back(int(source));
    for(int c:corners) {
      const auto v=polygon.vertices.at(c);const auto uv=polygon.uv.at(c);
      if(v>=mesh.positions.size()) throw std::runtime_error("SubD 顶点索引越界");
      vertices.push_back(int(v));const auto key=std::make_tuple(v,uv.x,uv.y);
      auto [it,inserted]=uv_map.emplace(key,int(uv_values.size()));if(inserted) uv_values.push_back({uv.x,uv.y});uv_indices.push_back(it->second);
    }
  };
  for(uint32_t p=0;p<mesh.polygons.size();++p) {
    const auto &polygon=mesh.polygons[p];
    if(polygon.vertices.size()<3||polygon.uv.size()!=polygon.vertices.size()) throw std::runtime_error("SubD 多边形或 UV 无效");
    if(settings.algorithm==2) {for(int k=1;k+1<int(polygon.vertices.size());++k) {const int corners[]={0,k,k+1};add_face(polygon,p,corners);}}
    else {std::vector<int> corners(polygon.vertices.size());std::iota(corners.begin(),corners.end(),0);add_face(polygon,p,corners);}
  }
  // 显式拒绝无法承受的设置，不静默降低用户保存的级别。
  // 开销已在建立 UV 映射前检查。
  for(const auto &c:mesh.creases) {crease_indices.push_back(int(c.a));crease_indices.push_back(int(c.b));crease_weights.push_back(c.weight);}
  for(const auto &[v,w]:mesh.corners) {corner_indices.push_back(int(v));corner_weights.push_back(w);}
  osd::Far::TopologyDescriptor descriptor;
  descriptor.numVertices=int(mesh.positions.size());descriptor.numFaces=int(sizes.size());descriptor.numVertsPerFace=sizes.data();descriptor.vertIndicesPerFace=vertices.data();
  descriptor.numHoles=int(holes.size());descriptor.holeIndices=holes.data();
  descriptor.numCreases=int(crease_weights.size());descriptor.creaseVertexIndexPairs=crease_indices.data();descriptor.creaseWeights=crease_weights.data();
  descriptor.numCorners=int(corner_weights.size());descriptor.cornerVertexIndices=corner_indices.data();descriptor.cornerWeights=corner_weights.data();
  osd::Far::TopologyDescriptor::FVarChannel channel;channel.numValues=int(uv_values.size());channel.valueIndices=uv_indices.data();descriptor.numFVarChannels=1;descriptor.fvarChannels=&channel;
  osd::Sdc::Options options;
  options.SetVtxBoundaryInterpolation(settings.edge_interpolation==1?osd::Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER:settings.edge_interpolation==2?osd::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY:osd::Sdc::Options::VTX_BOUNDARY_NONE);
  options.SetFVarLinearInterpolation(osd::Sdc::Options::FVAR_LINEAR_ALL);
  const auto scheme=settings.algorithm==1?osd::Sdc::SCHEME_BILINEAR:settings.algorithm==2?osd::Sdc::SCHEME_LOOP:osd::Sdc::SCHEME_CATMARK;
  using Factory=osd::Far::TopologyRefinerFactory<osd::Far::TopologyDescriptor>;
  Factory::Options factory_options(scheme,options);factory_options.validateFullTopology=true;
  std::unique_ptr<osd::Far::TopologyRefiner> refiner(Factory::Create(descriptor,factory_options));
  if(!refiner) throw std::runtime_error("OpenSubdiv 无法建立拓扑："+mesh.id);
  osd::Far::TopologyRefiner::UniformOptions uniform(level);uniform.fullTopologyInLastLevel=true;refiner->RefineUniform(uniform);
  osd::Far::PrimvarRefiner interpolate(*refiner);
  for(int l=1;l<=level;++l) {
    const auto &topology=refiner->GetLevel(l);std::vector<UV> next(topology.GetNumFVarValues());interpolate.InterpolateFaceVarying(l,uv_values,next);uv_values=std::move(next);
    std::vector<int> next_parents(topology.GetNumFaces());for(int f=0;f<topology.GetNumFaces();++f) next_parents[f]=parents.at(topology.GetFaceParentFace(f));parents=std::move(next_parents);
  }
  auto data=std::make_shared<Data>();data->cage_size=mesh.positions.size();
  osd::Far::StencilTableFactory::Options stencil_options;stencil_options.generateOffsets=true;stencil_options.generateIntermediateLevels=false;
  data->stencils.reset(osd::Far::StencilTableFactory::Create(*refiner,stencil_options));
  if(!data->stencils) throw std::runtime_error("OpenSubdiv 无法建立顶点模板");
  const auto &topology=refiner->GetLevel(level);
  for(int f=0;f<topology.GetNumFaces();++f) {
    const auto source=uint32_t(parents.at(f));if(std::binary_search(mesh.hidden_polygons.begin(),mesh.hidden_polygons.end(),source)) continue;
    if(topology.IsFaceHole(f)) continue;
    const auto indices=topology.GetFaceVertices(f),uvs=topology.GetFaceFVarValues(f);const auto &polygon=mesh.polygons[source];
    for(int k=1;k+1<indices.size();++k) {
      ir::Triangle triangle;triangle.source_polygon=source;triangle.material_slot=polygon.material_slot;triangle.polygon_group=polygon.polygon_group;
      const int corners[]={0,k,k+1};for(int c=0;c<3;++c) {triangle.vertices[c]=uint32_t(indices[corners[c]]);const auto uv=uv_values.at(uvs[corners[c]]);triangle.uv[c]={uv.x,uv.y};}
      data->triangles.push_back(triangle);
    }
  }
  data_=std::move(data);
}
std::vector<ir::Vec3> Subdivision::evaluate(std::span<const ir::Vec3> cage) const {
  if(!data_) return {cage.begin(),cage.end()};
  if(cage.size()!=data_->cage_size) throw std::runtime_error("细分控制网格顶点数改变");
  const auto &table=*data_->stencils;std::vector<ir::Vec3> result(table.GetNumStencils());
  const auto &sizes=table.GetSizes();const auto &offsets=table.GetOffsets();const auto &indices=table.GetControlIndices();const auto &weights=table.GetWeights();
  for(size_t v=0;v<result.size();++v) for(int i=offsets[v];i<offsets[v]+sizes[v];++i) {const auto &p=cage[indices[i]];const float w=weights[i];result[v].x+=p.x*w;result[v].y+=p.y*w;result[v].z+=p.z*w;}
  return result;
}
const std::vector<ir::Triangle> &Subdivision::triangles() const {return data_->triangles;}
}
