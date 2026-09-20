#include "daz/loader.h"
#include <zlib.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using Json=nlohmann::json;
namespace fs=std::filesystem;
void require(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
void write(const fs::path &p,const Json &j) {std::ofstream file(p);file<<j.dump();}
int main() {
  try {
    const auto directory=fs::current_path()/"dson-test-data"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(directory/"data");
    Json asset=Json::parse(R"({
      "node_library":[{"id":"root","type":"node","rotation_order":"XYZ"}],
      "geometry_library":[{"id":"mesh","type":"polygon_mesh",
        "vertices":{"count":4,"values":[[0,0,0],[100,0,0],[100,100,0],[0,100,0]]},
        "polygon_material_groups":{"count":1,"values":["surface"]},
        "polylist":{"count":1,"values":[[0,0,0,1,2,3]]},
        "default_uv_set":"#uv"}],
      "uv_set_library":[{"id":"uv","vertex_count":4,"uvs":{"count":5,"values":[[0,0],[1,0],[1,1],[0,1],[0.25,0.75]]},
        "polygon_vertex_indices":[[0,1,4]]}]
    })");
    const auto asset_path=directory/"data"/fs::u8path("mesh \xc3\xa4.dsf");
    write(asset_path,asset);
    Json duf=Json::parse(R"({
      "material_library":[{"id":"mat","diffuse":{"channel":{"id":"diffuse","value":[1,1,1]}}}],
      "scene":{"nodes":[
        {"id":"one","url":"/data/mesh%20%C3%A4.dsf#root","translation":[{"id":"x","current_value":200}],
         "geometries":[{"id":"shape1","url":"/data/mesh%20%C3%A4.dsf#mesh"}]},
        {"id":"two","url":"/data/mesh%20%C3%A4.dsf#root","geometries":[{"id":"shape2","url":"/data/mesh%20%C3%A4.dsf#mesh"}]}],
        "materials":[{"id":"mat1","url":"#mat","geometry":"#shape1","groups":["surface"]},
                     {"id":"mat2","url":"#mat","geometry":"#shape2","groups":["surface"]}]}
    })");
    const auto path=directory/"scene.duf";write(path,duf);
    auto loaded=dfv::daz::load(path,{{directory},true});
    require(loaded.scene.meshes.size()==1 && loaded.scene.instances.size()==2,"共享网格没有保留实例关系");
    const auto &mesh=loaded.scene.meshes[0];require(mesh.triangles.size()==2,"四边形三角化错误");
    require(std::abs(mesh.triangles[0].uv[1].x-.25f)<1e-6f && std::abs(mesh.triangles[0].uv[1].y-.75f)<1e-6f,"UV 接缝没有按几何顶点索引覆盖");
    require(std::abs(mesh.positions[2].x-1)<1e-6f && std::abs(mesh.positions[2].z-1)<1e-6f,"单位或坐标系转换错误");
    require(std::abs(loaded.scene.instances[0].transform.point({0,0,0}).x-2)<1e-6f,"实例 current_value 变换没有应用");
    const auto gzpath=directory/"compressed.duf";const auto bytes=duf.dump();
    auto gzip=gzopen(gzpath.string().c_str(),"wb");require(gzip!=nullptr,"gzip fixture 创建失败");
    require(gzwrite(gzip,bytes.data(),unsigned(bytes.size()))==int(bytes.size()),"gzip fixture 写失败");require(gzclose(gzip)==Z_OK,"gzip fixture 关闭失败");
    require(dfv::daz::load(gzpath).scene.meshes[0].triangles.size()==2,"gzip DUF 解析错误");
    auto bad=loaded.scene;bad.meshes[0].triangles[0].vertices[0]=999;
    bool rejected=false;try {bad.validate();} catch(const std::exception &) {rejected=true;}require(rejected,"IR 越界索引没有拒绝");
    auto camera=loaded.scene.camera;camera.transform.value[0]=std::numeric_limits<float>::quiet_NaN();
    rejected=false;try {dfv::ir::validate(camera);} catch(const std::exception &) {rejected=true;}require(rejected,"非有限相机变换没有拒绝");
    auto material=loaded.scene.materials[0];material.roughness=std::numeric_limits<float>::infinity();
    rejected=false;try {dfv::ir::validate(material,0);} catch(const std::exception &) {rejected=true;}require(rejected,"非有限材质参数没有拒绝");
    asset["geometry_library"][0]["vertices"]["count"]=5;write(asset_path,asset);
    rejected=false;try {(void)dfv::daz::load(path);} catch(const std::exception &) {rejected=true;}require(rejected,"无效 DSON count 没有拒绝");
    std::cout<<"IR references / UTF-8 URI / gzip / material binding / UV seams / transforms: PASS\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
