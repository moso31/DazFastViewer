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
    const auto multi=directory/"multi-member.duf";const auto half=bytes.size()/2;
    gzip=gzopen(multi.string().c_str(),"wb");require(gzip!=nullptr,"多段 gzip 创建失败");
    require(gzwrite(gzip,bytes.data(),unsigned(half))==int(half) && gzclose(gzip)==Z_OK,"第一段 gzip 写失败");
    gzip=gzopen(multi.string().c_str(),"ab");require(gzip!=nullptr,"第二段 gzip 创建失败");
    require(gzwrite(gzip,bytes.data()+half,unsigned(bytes.size()-half))==int(bytes.size()-half) && gzclose(gzip)==Z_OK,"第二段 gzip 写失败");
    gzip=gzopen(multi.string().c_str(),"ab");require(gzip!=nullptr && gzclose(gzip)==Z_OK,"空 gzip 段写失败");
    require(dfv::daz::load(multi).scene.meshes[0].triangles.size()==2,"多段及空段 gzip 未兼容");
    {std::ofstream tail(multi,std::ios::binary|std::ios::app);tail<<"invalid-tail";}
    bool invalid_tail=false;try {dfv::daz::read_document_file(multi);} catch(...) {invalid_tail=true;}
    require(invalid_tail,"gzip 尾部垃圾未拒绝");
    // 合成凹凸输入验证强度覆盖、厘米到米、数据色彩空间，以及无高度范围时的诊断。
    std::ofstream(directory/"height.png",std::ios::binary)<<"fixture-path-only";
    auto bump_duf=duf;
    bump_duf["material_library"][0]["extra"]=Json::parse(R"([{"channels":[
      {"channel":{"id":"Bump Strength","value":0.5,"current_value":2,"image_file":"height.png"}},
      {"channel":{"id":"Bump Minimum","value":-0.2}},
      {"channel":{"id":"Bump Maximum","value":0.3}},
      {"channel":{"id":"Refraction Index","value":1.33}}
    ]}])");
    const auto bump_path=directory/"bump.duf";write(bump_path,bump_duf);
    auto bumped=dfv::daz::load(bump_path,{{directory},true});const auto &bump=bumped.scene.materials[0];
    require(bump.bump_strength==2 && std::abs(bump.bump_distance-.005f)<1e-7f,"凹凸强度覆盖或厘米转换错误");
    require(bump.bump_texture>=0 && bumped.scene.textures.at(bump.bump_texture).colorspace==dfv::ir::ColorSpace::linear,"凹凸贴图必须按数据读取");
    require(bump.ior==1.5f,"非透射材质错误使用折射 IOR");
    auto &channels=bump_duf["material_library"][0]["extra"][0]["channels"];
    channels.erase(channels.begin()+1,channels.begin()+3);write(bump_path,bump_duf);
    auto approximate=dfv::daz::load(bump_path);
    require(approximate.report["warnings"][0]["code"]=="bump_distance_approximation","近似凹凸距离缺少诊断");
    auto bad_bump=bump;bad_bump.bump_texture=999;
    bool bump_rejected=false;try {dfv::ir::validate(bad_bump,1);} catch(const std::exception &) {bump_rejected=true;}
    require(bump_rejected,"越界凹凸贴图没有拒绝");
    bad_bump=bump;bad_bump.bump_distance=std::numeric_limits<float>::quiet_NaN();
    bump_rejected=false;try {dfv::ir::validate(bad_bump,1);} catch(const std::exception &) {bump_rejected=true;}
    require(bump_rejected,"非有限凹凸距离没有拒绝");
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
