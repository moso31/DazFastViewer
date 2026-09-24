#include "editor/document.h"
#include "runtime/subdivision.h"
#include "bench/scene_export.h"
#include <fstream>
#include <iostream>
#include <map>
#include <cmath>
#include <unordered_map>

// 专项诊断工具：比较实际细分边界，并输出去材质／合并拓扑白模对照。
// 合并白模仅用于定位原因，不进入编辑器的增量同步或附件生命周期路径。

using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
using Edge=std::pair<uint32_t,uint32_t>;
static Edge edge(uint32_t a,uint32_t b) {return {std::min(a,b),std::max(a,b)};}
static ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
static float dot(ir::Vec3 a,ir::Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
static ir::Vec3 cross(ir::Vec3 a,ir::Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double length(ir::Vec3 a) {return std::sqrt(double(dot(a,a)));}
static J vec(ir::Vec3 v) {return {v.x,v.y,v.z};}
static double distance(ir::Vec3 p,const std::vector<ir::Vec3> &line) {
  double best=1e30;for(size_t i=1;i<line.size();++i) {const auto v=sub(line[i],line[i-1]),d=sub(p,line[i-1]);const float t=std::clamp(dot(d,v)/std::max(dot(v,v),1e-30f),0.f,1.f);best=std::min(best,length({d.x-t*v.x,d.y-t*v.y,d.z-t*v.z}));}return best;
}
struct Prepared {
  std::vector<ir::Vec3> points,normals;
  std::vector<ir::Triangle> triangles;
  std::map<Edge,std::vector<uint32_t>> paths;
  struct UV {std::array<ir::Vec2,2> uv;uint32_t material;};
  std::map<Edge,UV> uv;
};
static Prepared prepare(const ir::Scene &scene,uint32_t instance,int level) {
  const auto &object=scene.instances[instance];auto mesh=scene.meshes[object.mesh];mesh.subdivision.enabled=level>0;mesh.subdivision.level=mesh.subdivision.render_level=level;
  runtime::Subdivision subdiv(mesh,false,true);Prepared out;out.points=subdiv.evaluate(mesh.positions);
  for(auto &p:out.points) p=object.transform.point(p);
  if(subdiv.active()) {out.triangles=subdiv.triangles();for(const auto &b:subdiv.boundaries()) {auto path=b.vertices;if(b.a>b.b) std::reverse(path.begin(),path.end());out.paths[edge(b.a,b.b)]=std::move(path);}}
  else {
    for(const auto &t:mesh.triangles) if(mesh.draws(t)) out.triangles.push_back(t);
    std::map<Edge,int> counts;for(size_t i=0;i<mesh.polygons.size();++i) if(!std::binary_search(mesh.hidden_polygons.begin(),mesh.hidden_polygons.end(),uint32_t(i))) {const auto &p=mesh.polygons[i];for(size_t k=0;k<p.vertices.size();++k) ++counts[edge(p.vertices[k],p.vertices[(k+1)%p.vertices.size()])];}
    for(auto [e,count]:counts) if(count==1) out.paths[e]={e.first,e.second};
  }
  std::set<Edge> wanted;for(const auto &[e,path]:out.paths) for(size_t i=1;i<path.size();++i) wanted.insert(edge(path[i-1],path[i]));
  out.normals.resize(out.points.size());
  for(const auto &t:out.triangles) {
    const auto n=cross(sub(out.points[t.vertices[1]],out.points[t.vertices[0]]),sub(out.points[t.vertices[2]],out.points[t.vertices[0]]));
    for(auto v:t.vertices) {out.normals[v].x+=n.x;out.normals[v].y+=n.y;out.normals[v].z+=n.z;}
    for(size_t k=0;k<3;++k) {const auto a=t.vertices[k],b=t.vertices[(k+1)%3];if(wanted.contains(edge(a,b))) {
      auto ua=t.uv[k],ub=t.uv[(k+1)%3];if(a>b) std::swap(ua,ub);out.uv[edge(a,b)]={{ua,ub},object.materials.at(t.material_slot)};
    }}
  }
  return out;
}
static void flat_mesh(J &duf,const std::string &id,const std::vector<ir::Vec3> &positions,const std::vector<ir::Triangle> &triangles) {
  J points=J::array(),faces=J::array();for(auto p:positions) points.push_back({p.x*100,p.z*100,-p.y*100});for(const auto &t:triangles) faces.push_back({0,0,t.vertices[0],t.vertices[1],t.vertices[2]});
  duf["geometry_library"].push_back({{"id",id},{"vertices",{{"count",points.size()},{"values",points}}},{"polygon_material_groups",{{"count",1},{"values",{"Skin"}}}},{"polylist",{{"count",faces.size()},{"values",faces}}},{"default_uv_set","#uv-"+id}});
  J uv=J::array();for(size_t i=0;i<positions.size();++i) uv.push_back({0,0});duf["uv_set_library"].push_back({{"id","uv-"+id},{"vertex_count",positions.size()},{"uvs",{{"count",uv.size()},{"values",uv}}}});
  duf["scene"]["nodes"].push_back({{"id",id},{"geometries",J::array({{{"id",id+"-shape"},{"url","#"+id}}})}});
  duf["scene"]["materials"].push_back({{"id",id+"-mat"},{"geometry","#"+id+"-shape"},{"groups",{"Skin"}},{"diffuse",{{"channel",{{"value",{1.,1.,1.}}}}}}});
}
static J flat_scene() {return {{"asset_info",{{"type","scene"}}},{"geometry_library",J::array()},{"uv_set_library",J::array()},{"scene",{{"nodes",J::array()},{"materials",J::array()}}}};}
static ir::Mesh joint_mesh(const ir::Scene &scene,uint32_t host,const std::vector<runtime::GraftSeam> &seams,int level) {
  const auto &object=scene.instances[host];const auto &body=scene.meshes[object.mesh];ir::Mesh out=body;
  for(auto &p:out.positions) p=object.transform.point(p);out.polygons.clear();out.triangles.clear();out.hidden_polygons.clear();out.creases.clear();out.corners.clear();
  for(size_t i=0;i<body.polygons.size();++i) if(!std::binary_search(body.hidden_polygons.begin(),body.hidden_polygons.end(),uint32_t(i))) out.polygons.push_back(body.polygons[i]);
  for(const auto &g:seams) if(g.source==host) {
    const auto &f=scene.instances[g.follower];const auto &mesh=scene.meshes[f.mesh];std::vector<uint32_t> mapping(mesh.positions.size(),UINT32_MAX);
    for(auto pair:mesh.graft_vertex_pairs) mapping[pair[0]]=pair[1];
    for(size_t i=0;i<mapping.size();++i) if(mapping[i]==UINT32_MAX) {mapping[i]=uint32_t(out.positions.size());out.positions.push_back(f.transform.point(mesh.positions[i]));}
    for(size_t i=0;i<mesh.polygons.size();++i) if(!std::binary_search(mesh.hidden_polygons.begin(),mesh.hidden_polygons.end(),uint32_t(i))) {auto p=mesh.polygons[i];for(auto &v:p.vertices) v=mapping[v];p.material_slot=0;out.polygons.push_back(std::move(p));}
  }
  for(size_t i=0;i<out.polygons.size();++i) {const auto &p=out.polygons[i];for(size_t k=1;k+1<p.vertices.size();++k) {ir::Triangle t;t.vertices={p.vertices[0],p.vertices[k],p.vertices[k+1]};t.uv={p.uv[0],p.uv[k],p.uv[k+1]};t.source_polygon=uint32_t(i);out.triangles.push_back(t);}}
  out.subdivision.enabled=level>0;out.subdivision.level=out.subdivision.render_level=level;return out;
}
int main(int argc,char **argv) {try {
  if(argc!=4) throw std::runtime_error("GraftSeamProbe <scene.duf> <project.json> <output>");
  const fs::path output=fs::u8path(argv[3]);fs::create_directories(output);J project;std::ifstream(fs::u8path(argv[2]))>>project;std::vector<fs::path> roots;for(const auto &r:project["content_roots"]) roots.push_back(fs::u8path(r.get<std::string>()));
  editor::Document document;document.loaded=daz::load(fs::u8path(argv[1]),{roots,false});document.catalog=daz::discover_morphs(document.loaded,roots,{},true);document.skeletons=daz::load_skeletons(document.loaded);document.formulas=daz::enable_formulas(document.catalog,document.skeletons);
  auto snapshot=editor::initial_snapshot(document);auto scene=document.loaded.scene;runtime::DeformationRuntime runtime(scene,document.catalog.targets,document.skeletons.skins,document.formulas.graphs);runtime.evaluate(snapshot.values,snapshot.poses);editor::apply_subdivision_levels(scene,{});
  const auto seams=runtime.graft_seams();J report={{"input",argv[1]},{"cases",J::array()},{"base_seams",J::array()}};
  for(const auto &g:seams) report["base_seams"].push_back({{"follower",scene.instances[g.follower].id},{"source",scene.instances[g.source].id},{"pairs",g.pairs},{"max_gap_m",g.max_gap_m}});
  // 导出原始 IR 材质和最终形变网格，保留 UV／贴图核查依据。
  std::ofstream(output/"evaluated-scene.json")<<scene_json(scene,256).dump();
  // 与渲染适配器一致，统计自动 Bump 高度所依据的世界／UV 面积；图片尺寸在后处理核对。
  report["bump_density"]=J::array();
  for(uint32_t material=0;material<scene.materials.size();++material) {
    const auto &value=scene.materials[material];if(!value.bump_from_texel_density||value.bump_texture<0||value.bump_strength<=0) continue;
    double world=0,uv=0;
    for(const auto &object:scene.instances) if(object.prototype<0) for(const auto &t:scene.meshes[object.mesh].triangles) if(object.materials[t.material_slot]==material) {
      const auto &p=scene.meshes[object.mesh].positions;const auto a=object.transform.point(p[t.vertices[0]]),b=object.transform.point(p[t.vertices[1]]),c=object.transform.point(p[t.vertices[2]]);
      const double x1=b.x-a.x,y1=b.y-a.y,z1=b.z-a.z,x2=c.x-a.x,y2=c.y-a.y,z2=c.z-a.z;
      world+=std::hypot(y1*z2-z1*y2,z1*x2-x1*z2,x1*y2-y1*x2);
      uv+=std::abs(((t.uv[1].x-t.uv[0].x)*(t.uv[2].y-t.uv[0].y)-(t.uv[1].y-t.uv[0].y)*(t.uv[2].x-t.uv[0].x))*value.uv_scale.x*value.uv_scale.y);
    }
    report["bump_density"].push_back({{"material",value.id},{"texture",value.bump_texture},{"world_double_area",world},{"uv_double_area",uv}});
  }
  for(int level:{-1,0,1,2}) {
    std::map<uint32_t,Prepared> prepared;auto get=[&](uint32_t i)->Prepared & {auto found=prepared.find(i);if(found!=prepared.end()) return found->second;const auto &m=scene.meshes[scene.instances[i].mesh];return prepared.emplace(i,prepare(scene,i,level<0?m.subdivision.level:level)).first->second;};
    J group={{"level",level},{"seams",J::array()}};J flat=flat_scene();std::set<uint32_t> exported;
    for(const auto &g:seams) {
      const auto &f=scene.instances[g.follower],&h=scene.instances[g.source];const auto &mesh=scene.meshes[f.mesh];auto &a=get(g.follower),&b=get(g.source);
      std::map<uint32_t,uint32_t> pairs;for(auto pair:mesh.graft_vertex_pairs) pairs[pair[0]]=pair[1];
      double maximum=0,normal_max=0;size_t matched=0,unmatched=0;J edges=J::array();
      for(const auto &[e,path]:a.paths) if(pairs.contains(e.first)&&pairs.contains(e.second)) {
        const auto ha=pairs[e.first],hb=pairs[e.second];auto found=b.paths.find(edge(ha,hb));if(found==b.paths.end()) {++unmatched;continue;}
        ++matched;auto target=found->second;if(ha>hb) std::reverse(target.begin(),target.end());std::vector<ir::Vec3> ap,bp;for(auto v:path) ap.push_back(a.points[v]);for(auto v:target) bp.push_back(b.points[v]);
        double gap=0;for(auto p:ap) gap=std::max(gap,distance(p,bp));for(auto p:bp) gap=std::max(gap,distance(p,ap));maximum=std::max(maximum,gap);
        const auto an=a.normals[path.front()],bn=b.normals[target.front()];const double angle=std::acos(std::clamp(double(dot(an,bn))/std::max(length(an)*length(bn),1e-30),-1.,1.))*180/3.141592653589793;normal_max=std::max(normal_max,angle);
        auto au=a.uv.at(edge(path[0],path[1])),bu=b.uv.at(edge(target[0],target[1]));if(path[0]>path[1]) std::swap(au.uv[0],au.uv[1]);if(target[0]>target[1]) std::swap(bu.uv[0],bu.uv[1]);
        auto am=scene.materials.at(au.material),bm=scene.materials.at(bu.material);am.id.clear();bm.id.clear();
        edges.push_back({{"graft_edge",{e.first,e.second}},{"host_edge",{ha,hb}},{"gap_m",gap},{"normal_angle_degrees",angle},{"graft_uv",{au.uv[0].x,au.uv[0].y}},{"host_uv",{bu.uv[0].x,bu.uv[0].y}},{"graft_material",au.material},{"host_material",bu.material},{"graft_start",vec(ap.front())},{"host_start",vec(bp.front())}});
        edges.back()["material_equal_except_id"]=(am==bm);
      }
      group["seams"].push_back({{"follower",f.id},{"source",h.id},{"graft_level",level<0?mesh.subdivision.level:level},{"host_level",level<0?scene.meshes[h.mesh].subdivision.level:level},{"matched_edges",matched},{"unmatched_edges",unmatched},{"max_boundary_gap_m",maximum},{"max_normal_angle_degrees",normal_max},{"edges",edges}});
      if(h.id.find("Female")!=std::string::npos) for(auto i:{g.source,g.follower}) if(exported.insert(i).second) {auto &p=get(i);flat_mesh(flat,"object-"+std::to_string(i),p.points,p.triangles);}
    }
    std::ofstream(output/("white-"+std::to_string(level)+".duf"))<<flat.dump();report["cases"].push_back(std::move(group));std::cout<<"level "<<level<<" done\n";
  }
  for(const auto &g:seams) if(scene.instances[g.source].id.find("Female")!=std::string::npos) {
    auto mesh=joint_mesh(scene,g.source,seams,2);runtime::Subdivision subdivision(mesh);J flat=flat_scene();flat_mesh(flat,"joint",subdivision.evaluate(mesh.positions),subdivision.triangles());std::ofstream(output/"white-joint-2.duf")<<flat.dump();break;
  }
  report["status"]="PASS";std::ofstream(output/"graft-seams.json")<<report.dump(2);return 0;
}catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}}
