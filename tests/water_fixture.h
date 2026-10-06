#pragma once
#include "water/document.h"
#include "editor/document.h"
#include <cmath>
#include <fstream>
namespace water_fixture {
inline dfv::ir::Mesh cylinder(float radius=3,float bottom=-8,float top=12,int segments=64){
  dfv::ir::Mesh m;m.id="column-mesh";m.material_slots={"Column"};m.smooth=true;
  for(float z:{bottom,top})for(int i=0;i<segments;++i){double a=i*6.283185307179586/segments;m.positions.push_back({radius*float(std::cos(a)),radius*float(std::sin(a)),z});}
  m.positions.push_back({0,0,bottom});m.positions.push_back({0,0,top});
  auto tri=[&](int a,int b,int c){dfv::ir::Triangle t;t.vertices={uint32_t(a),uint32_t(b),uint32_t(c)};t.source_polygon=uint32_t(m.triangles.size());m.triangles.push_back(t);};
  for(int i=0;i<segments;++i){int j=(i+1)%segments;tri(i,j,segments+j);tri(i,segments+j,segments+i);tri(segments*2,j,i);tri(segments*2+1,segments+i,segments+j);}m.source_polygon_count=uint32_t(m.triangles.size());return m;
}
inline nlohmann::json scene_json(){
  using J=nlohmann::json;auto m=cylinder();J points=J::array(),faces=J::array(),uv=J::array();for(auto p:m.positions){points.push_back({p.x*100,p.z*100,-p.y*100});uv.push_back({p.x/6,p.y/6});}for(const auto &t:m.triangles)faces.push_back({0,0,t.vertices[0],t.vertices[1],t.vertices[2]});
  J j={{"asset_info",{{"type","scene"}}},{"geometry_library",J::array({{{"id","mesh"},{"vertices",{{"count",points.size()},{"values",points}}},{"polygon_material_groups",{{"count",1},{"values",{"Column"}}}},{"polylist",{{"count",faces.size()},{"values",faces}}},{"default_uv_set","#uv"}}})},
    {"uv_set_library",J::array({{{"id","uv"},{"vertex_count",points.size()},{"uvs",{{"count",uv.size()},{"values",uv}}}}})},{"scene",{{"nodes",J::array()},{"materials",J::array()}}}};
  for(int i=0;i<3;++i){auto id="column-"+std::to_string(i),shape="shape-"+std::to_string(i);j["scene"]["nodes"].push_back({{"id",id},{"label","Column "+std::to_string(i+1)},{"type","node"},{"translation",J::array({{{"id","x"},{"value",(i-1)*1400}},{{"id","z"},{"value",i==1?-900:0}}})},{"geometries",J::array({{{"id",shape},{"url","#mesh"}}})}});j["scene"]["materials"].push_back({{"id","material-"+std::to_string(i)},{"geometry","#"+shape},{"groups",{"Column"}},{"diffuse",{{"channel",{{"value",{.25,.2,.13}}}}}}});}
  return j;
}
inline void write(const std::filesystem::path &file){std::filesystem::create_directories(file.parent_path());std::ofstream(file)<<scene_json().dump();}
inline dfv::editor::Document load(const std::filesystem::path &file){using namespace dfv;editor::Document d;d.source_file=std::filesystem::absolute(file);const std::vector<std::filesystem::path> roots{d.source_file.parent_path()};d.loaded=daz::load(file,{roots});d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);return d;}
}
