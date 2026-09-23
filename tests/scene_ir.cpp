#include "daz/loader.h"
#include "runtime/picking.h"
#include <zlib.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

using Json=nlohmann::json;
namespace fs=std::filesystem;
void require(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
void write(const fs::path &p,const Json &j) {std::ofstream file(p);file<<j.dump();}
int main() {
  try {
    dfv::ir::Transform tiny;tiny.value[0]=tiny.value[5]=tiny.value[10]=.00001f;
    const auto restored=dfv::ir::inverse(tiny).point(tiny.point({1,2,3}));require(std::abs(restored.x-1)<1e-6f&&std::abs(restored.z-3)<1e-6f,"小比例实例被误判为不可逆");
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
    // 场景中的 LIE 引用覆盖 DSF 底图，并保留图层顺序、透明度和显式 gamma。
    auto lie=duf;lie["material_library"][0]["diffuse"]["channel"]["image_file"]="height.png";lie["material_library"][0]["diffuse"]["channel"]["image"]=nullptr;
    lie["image_library"]=Json::parse(R"([{"id":"face stack","map_gamma":2.2,"map":[
      {"url":"height.png"},{"url":"height.png","operation":"multiply","transparency":0.9},
      {"url":"missing-inactive.png","active":false}]}])");
    lie["scene"]["materials"][0]["diffuse"]={{"channel",{{"image","#face%20stack"}}}};
    write(path,lie);const auto layered=dfv::daz::load(path);
    const auto &lt=layered.scene.textures.at(layered.scene.materials[0].color_texture);
    require(lt.layers.size()==2&&lt.gamma==2.2f&&lt.layers[1].operation=="multiply"&&lt.layers[1].opacity==.9f,"LIE 引用、图层顺序或透明度丢失");
    require(layered.scene.textures.at(layered.scene.materials[1].color_texture).layers.empty(),"一个材质的 LIE 覆盖泄漏到另一个材质");
    lie["scene"]["materials"][0]["diffuse"]["channel"]={{"image_file","height.png"}};write(path,lie);
    require(dfv::daz::load(path).scene.textures[0].layers.empty(),"image_file 覆盖未清除旧 image 引用");
    auto sss_duf=duf;sss_duf["material_library"][0]["extra"]=Json::parse(R"([{"channels":[
      {"channel":{"id":"Translucency Weight","value":0.5}},
      {"channel":{"id":"SSS Mode","value":0}},
      {"channel":{"id":"SSS Amount","value":0.5}},
      {"channel":{"id":"SSS Color","value":[0,0,0]}},
      {"channel":{"id":"Transmitted Color","value":[1,1,1]}},
      {"channel":{"id":"Scattering Measurement Distance","value":0.1}}
    ]}])");
    write(path,sss_duf);const auto mono=dfv::daz::load(path).scene.materials[0];
    require(std::abs(mono.subsurface_radius.x-.002f)<1e-7f&&mono.subsurface_color.x==1&&mono.separate_subsurface_color,"Mono SSS 错误使用隐藏的 SSS Color，或厘米尺度错误");
    sss_duf["material_library"][0]["extra"][0]["channels"][1]["channel"]["value"]=1;write(path,sss_duf);
    require(dfv::daz::load(path).scene.materials[0].subsurface_radius.x<mono.subsurface_radius.x/10,"Chromatic SSS 未按颜色衰减计算");
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
    auto displacement=duf;
    displacement["material_library"][0]["extra"]=Json::parse(R"([{"channels":[
      {"channel":{"id":"Displacement Strength","value":0,"current_value":2,"image_file":"height.png"}},
      {"channel":{"id":"Minimum Displacement","value":0.3}},
      {"channel":{"id":"Maximum Displacement","value":-0.2}}
    ]}])");
    write(path,displacement);auto displaced=dfv::daz::load(path);const auto &disp=displaced.scene.materials[0];
    require(disp.displacement_texture>=0&&displaced.scene.textures.at(disp.displacement_texture).colorspace==dfv::ir::ColorSpace::linear,"置换贴图未按线性数据导入");
    require(disp.displacement_strength==2&&std::abs(disp.displacement_min-.003f)<1e-7f&&std::abs(disp.displacement_max+.002f)<1e-7f,"置换强度、反向范围或厘米单位丢失");
    displacement["scene"]["materials"][0]["extra"]=Json::parse(R"([{"channels":[{"channel":{"id":"Displacement Active","value":false}}]}])");
    write(path,displacement);require(dfv::daz::load(path).scene.materials[0].displacement_strength==0,"关闭置换未生效");
    displacement=duf;auto &old_disp=displacement["material_library"][0];
    old_disp["displacement"]={{"channel",{{"value",.5},{"image_file","height.png"}}}};
    old_disp["displacement_min"]={{"channel",{{"value",-.2}}}};old_disp["displacement_max"]={{"channel",{{"value",.4}}}};
    write(path,displacement);const auto legacy_disp=dfv::daz::load(path).scene.materials[0];
    require(legacy_disp.displacement_texture>=0&&legacy_disp.displacement_strength==.5f&&std::abs(legacy_disp.displacement_max-.004f)<1e-7f,"旧材质置换通道没有导入");
    auto legacy=duf;
    legacy["material_library"][0]["transparency"]={{"channel",{{"value",1},{"current_value",.2},{"image_file","height.png"}}}};
    legacy["material_library"][0]["glossiness"]={{"channel",{{"value",.9}}}};
    legacy["material_library"][0]["u_scale"]={{"channel",{{"value",3}}}};
    write(path,legacy);const auto legacy_material=dfv::daz::load(path).scene.materials[0];
    require(legacy_material.opacity==.2f&&legacy_material.opacity_texture>=0&&legacy_material.roughness_from_glossiness&&legacy_material.roughness==.9f&&legacy_material.uv_scale.x==3,"旧 DAZ 材质不透明度、贴图或光泽/平铺丢失");
    legacy["material_library"][0]["extra"]=Json::parse(R"([{"channels":[{"channel":{"id":"Cutout Opacity","value":0.7}},{"channel":{"id":"Glossy Roughness","value":0.4}}]}])");
    write(path,legacy);const auto modern_override=dfv::daz::load(path).scene.materials[0];
    require(modern_override.opacity==.7f&&!modern_override.roughness_from_glossiness&&modern_override.roughness==.4f,"Iray 通道没有优先于旧材质通道");
    legacy["material_library"][0]["extra"][0]["channels"].push_back({{"channel",{{"id","Glossy Color"},{"value",{1,1,1}},{"image_file","height.png"}}}});
    legacy["material_library"][0]["extra"][0]["channels"].push_back({{"channel",{{"id","Top Coat Color"},{"value",{1,1,1}},{"image_file","height.png"}}}});
    write(path,legacy);const auto colored_gloss=dfv::daz::load(path);
    require(colored_gloss.scene.materials[0].specular_color_texture>=0&&colored_gloss.scene.materials[0].coat_color_texture>=0&&colored_gloss.scene.textures.at(colored_gloss.scene.materials[0].specular_color_texture).colorspace==dfv::ir::ColorSpace::srgb,"头发高光/涂层色贴图没有映射");
    auto mdl=duf;mdl["material_library"][0].erase("diffuse");
    mdl["material_library"][0]["extra"]=Json::parse(R"([{"channels":[{"channel":{"id":"Diffuse Color","type":"image","image_file":"height.png"}},{"channel":{"id":"base_roughness","value":0.15}}]}])");
    write(path,mdl);const auto wood=dfv::daz::load(path).scene.materials[0];
    require(wood.color_texture>=0&&wood.roughness==.15f,"MDL Walnut 颜色贴图被错误退化为白色");
    mdl["material_library"][0]["bump_min"]={{"channel",{{"value",.1}}}};mdl["material_library"][0]["bump_max"]={{"channel",{{"value",-.1}}}};
    write(path,mdl);const auto inverted=dfv::daz::load(path).scene.materials[0];
    require(inverted.bump_invert&&std::abs(inverted.bump_distance-.002f)<1e-7f,"反向凹凸范围没有转换为正距离和反转标记");
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
    auto fidelity=duf;
    fidelity["scene"]["nodes"][1]["conform_target"]="#one";
    fidelity["scene"]["nodes"][1]["translation"]={{{"id","y"},{"current_value",12}}};
    fidelity["scene"]["nodes"][1]["extra"]=Json::parse(R"([{"type":"studio_node_channels","channels":[{"channel":{"id":"Visible","current_value":false}}]}])");

    fidelity["modifier_library"]=Json::parse(R"([{"id":"smooth","extra":[{"type":"studio/modifier/smoothing"},{"type":"studio_modifier_channels","channels":[{"channel":{"id":"Enable Smoothing","value":true}},{"channel":{"id":"Collision Iterations","value":5}},{"channel":{"id":"Collision Item","node":"#one"}}]}]}])");
    fidelity["scene"]["modifiers"]=Json::parse(R"([{"id":"smooth-instance","url":"#smooth","parent":"#two","extra":[{"type":"studio_modifier_channels","channels":[{"channel":{"id":"Collision Iterations","current_value":3}}]}]}])");
    write(path,fidelity);auto fitted=dfv::daz::load(path);
    require(fitted.objects[1].smoothing.enabled&&fitted.objects[1].smoothing.collision_target=="#one"&&fitted.objects[1].smoothing.collision_iterations==3,"碰撞修改器默认值或场景覆盖未保留");

    require(!fitted.scene.instances[1].visible,"保存的 Visible=false 被丢失");
    require(std::abs(fitted.scene.instances[1].transform.value[3]-2)<1e-6f,"根层级 Fit To 未继承目标变换");
    require(std::abs(fitted.scene.instances[1].transform.value[11])<1e-6f,"Fit To 叠加了旧位移");
    auto rigid=duf;
    rigid["scene"]["nodes"][0]["center_point"]={{{"id","y"},{"value",20}}};
    rigid["scene"]["nodes"][1]["parent"]="#follow";
    rigid["scene"]["nodes"][1]["translation"]={{{"id","y"},{"current_value",-70}}};
    rigid["scene"]["nodes"].push_back(Json::parse(R"({"id":"follow","type":"node","parent":"#one","translation":[{"id":"y","current_value":50}],"extra":[{"type":"studio/node/rigid_follow","vertex_count":4,"rigidity_group":{"rotation_mode":"full","scale_modes":["none","none","none"],"reference_vertices":{"count":3,"values":[0,1,2]}}},{"type":"studio_node_channels","channels":[{"channel":{"id":"Follow Target","node":"#one"}}]}]})"));
    write(path,rigid);const auto attached=dfv::daz::load(path);
    require(attached.objects[1].rigid_follow.target=="#one"&&attached.objects[1].rigid_follow.vertices.size()==3,"刚性挂接信息没有传递到子网格");
    const auto attachment_origin=attached.scene.instances[1].transform.point({});
    require(std::abs(attachment_origin.x-2)<1e-6f&&std::abs(attachment_origin.z)<1e-6f,"刚性挂接未使用 Follow Target 节点原点");
    write(path,fidelity);
    auto strands=asset;strands["geometry_library"][0]["polylist"]={{"count",0},{"values",Json::array()}};
    strands["geometry_library"][0]["polyline_list"]={{"count",1},{"values",{{0,0,0,1,2,3}}}};
    write(asset_path,strands);auto hair=dfv::daz::load(path);
    require(hair.scene.meshes[0].curves.size()==1&&hair.scene.meshes[0].triangles.empty(),"纯发丝曲线被静默丢弃");
    require(hair.scene.meshes[0].curves[0].vertices==std::vector<uint32_t>({0,1,2,3}),"发丝源顶点编号没有保留");
    auto bad_curve=hair.scene;bad_curve.meshes[0].curves[0].vertices[0]=99;
    rejected=false;try {bad_curve.validate();} catch(...) {rejected=true;}require(rejected,"发丝越界索引没有拒绝");
    write(asset_path,asset);
    fidelity["material_library"][0]["diffuse"]["channel"]["value"]={.5,.5,.5};
    fidelity["material_library"][0]["extra"]=Json::parse(R"([{"channels":[
      {"channel":{"id":"Thin Walled","value":true}},
      {"channel":{"id":"Refraction Weight","value":1}},
      {"channel":{"id":"Refraction Color","value":[1,1,1]}},
      {"channel":{"id":"Refraction Roughness","value":0}},
      {"channel":{"id":"Share Glossy Inputs","value":false}}
    ]}])");
    write(path,fidelity);const auto glass=dfv::daz::load(path).scene.materials[0];
    require(glass.thin_walled&&glass.transmission==1&&glass.base_color.x==1&&glass.roughness==0,"薄壁眼部材质被当成深色实体玻璃");
    fidelity["material_library"][0]["extra"][0]["channels"]=Json::parse(R"([
      {"channel":{"id":"Thin Walled","value":false}},
      {"channel":{"id":"Translucency Weight","value":0.6,"image_file":"height.png"}},
      {"channel":{"id":"Dual Lobe Specular Weight","value":0.4}},
      {"channel":{"id":"Top Coat Weight","value":0.2}},
      {"channel":{"id":"Scattering Measurement Distance","value":0.02}},
      {"channel":{"id":"SSS Mode","value":0}},
      {"channel":{"id":"SSS Amount","value":1}},
      {"channel":{"id":"Transmitted Measurement Distance","value":0.125}}
    ])");
    write(path,fidelity);const auto skin=dfv::daz::load(path).scene.materials[0];
    require(skin.subsurface==.6f&&skin.translucency_texture>=0&&skin.dual_weight==.4f&&skin.coat==.2f,"皮肤散射 / 双瓣高光 / 涂层参数没有读取");
    require(skin.base_color.x>.21f&&skin.base_color.x<.22f&&skin.subsurface_radius.x<.02f,"颜色没有线性化或散射距离单位错误");
    auto instances=duf;
    instances["scene"]["nodes"].push_back(Json::parse(R"({"id":"copy","translation":[{"id":"x","value":500}],"extra":[{"type":"studio/node/instance"},{"channels":[{"channel":{"id":"Instance Target","node":"#one"}}]}]})"));
    instances["scene"]["nodes"].push_back(Json::parse(R"({"id":"crowd","extra":[{"type":"studio/node/group_instance","instance_items":[{"name":"a","translation":[700,0,0]},{"name":"b","translation":[900,0,0],"scale":[2,2,2]}]},{"channels":[{"channel":{"id":"Instance Target","node":"#one"}}]}]})"));
    write(path,instances);auto instanced=dfv::daz::load(path);
    require(instanced.scene.instances.size()==5&&instanced.scene.meshes.size()==1,"普通/打包实例没有复用几何");
    std::vector<float> locations;for(const auto &i:instanced.scene.instances) if(i.prototype>=0) locations.push_back(i.transform.point({}).x);
    std::sort(locations.begin(),locations.end());require(locations==std::vector<float>({5,7,9}),"实例混入源对象世界位移");
    instanced.scene.meshes.push_back(instanced.scene.meshes[0]);instanced.scene.instances[0].mesh=1;dfv::daz::sync_instance_meshes(instanced.scene);
    for(const auto &i:instanced.scene.instances) if(i.prototype>=0) require(i.mesh==1,"实例没有跟随源对象独立形变网格");
    auto visibility_scene=instances;
    const auto invisible=Json::parse(R"([{"channels":[{"channel":{"id":"Visible","current_value":false}}]}])");
    visibility_scene["scene"]["nodes"][0]["extra"]=invisible;
    visibility_scene["scene"]["nodes"][1]["extra"]=invisible;visibility_scene["scene"]["nodes"][1]["parent"]="#one";
    write(path,visibility_scene);const auto visibility_copies=dfv::daz::load(path);
    for(const auto &i:visibility_copies.scene.instances) if(i.prototype>=0) require(i.visible==(i.prototype==0),"实例可见性没有区分隐藏原型根节点与内部零件");
    visibility_scene["scene"]["nodes"][1]["extra"]=Json::array();write(path,visibility_scene);
    const auto inherited_visibility=dfv::daz::load(path);
    const dfv::runtime::InstanceGroups instance_groups(inherited_visibility.scene);
    std::set<uint32_t> roots;for(uint32_t i=0;i<inherited_visibility.scene.instances.size();++i) if(inherited_visibility.scene.instances[i].prototype>=0) roots.insert(instance_groups.roots[i]);
    require(roots.size()==3,"Instance 一级未合并零件，或把不同散布条目合并了");
    for(auto root:roots) {require(instance_groups.members[root].size()==2,"实例的子零件没有归入同一选择组");const auto b=instance_groups.bounds(inherited_visibility.scene,root);require(!b.empty&&b.extent()>0,"实例整组聚焦没有包含几何");}
    require(!inherited_visibility.scene.instances[0].visible&&!inherited_visibility.scene.instances[1].visible,"父节点隐藏没有作用于子对象");
    for(const auto &i:inherited_visibility.scene.instances) if(i.prototype>=0) require(i.visible,"隐藏原型的父节点错误隐藏其独立实例");
    instances["scene"]["nodes"][2]["extra"][1]["channels"][0]["channel"]["node"]="#copy";write(path,instances);
    rejected=false;try {dfv::daz::load(path);} catch(...) {rejected=true;}require(rejected,"实例循环没有拒绝");
    auto missing=duf;missing["material_library"][0]["diffuse"]["channel"]["image_file"]="/textures/absent.png";write(path,missing);
    const auto fallback=dfv::daz::load(path);require(fallback.scene.materials[0].color_texture==-1&&fallback.report["warnings"][0]["code"]=="texture_missing","缺失贴图没有降级并报告");
    rejected=false;try {dfv::daz::load(path,{{},true});} catch(...) {rejected=true;}require(rejected,"严格模式接受缺失贴图");
    auto graft_asset=asset;auto graft=graft_asset["geometry_library"][0];graft["id"]="graft";
    graft["graft"]={{"vertex_count",4},{"poly_count",1},{"vertex_pairs",{{"count",2},{"values",{{0,2},{1,3}}}}},{"hidden_polys",{{"count",1},{"values",{0}}}}};graft_asset["geometry_library"].push_back(graft);write(asset_path,graft_asset);
    auto graft_scene=duf;auto graft_node=graft_scene["scene"]["nodes"][1];graft_node["id"]="graft";graft_node["conform_target"]="#one";
    graft_node["geometries"][0]["id"]="graft-shape";graft_node["geometries"][0]["url"]="/data/mesh%20%C3%A4.dsf#graft";
    graft_node["extra"]=Json::parse(R"([{"channels":[{"channel":{"id":"Visible","current_value":false}}]}])");graft_scene["scene"]["nodes"].push_back(graft_node);
    auto binding=graft_scene["scene"]["materials"][1];binding["id"]="graft-mat";binding["geometry"]="#graft-shape";graft_scene["scene"]["materials"].push_back(binding);
    write(path,graft_scene);auto grafted=dfv::daz::load(path);const auto &host=grafted.scene.meshes[grafted.scene.instances[0].mesh];
    require(host.triangles.size()==2&&!host.draws(host.triangles[0])&&!host.draws(host.triangles[1]),"隐藏插件恢复宿主面，或破坏原始拓扑");
    const auto &other=grafted.scene.meshes[grafted.scene.instances[1].mesh];require(other.draws(other.triangles[0]),"遮盖泄漏给同资产另一角色");
    require(grafted.scene.meshes[grafted.scene.instances.back().mesh].graft_vertex_pairs==std::vector<std::array<uint32_t,2>>({{0,2},{1,3}}),"GeoGraft 顶点对丢失或顺序颠倒");
    dfv::runtime::PickingScene picking;picking.update(grafted.scene);require(picking.ray({2.5f,-1,.5f},{0,1,0}).instance<0,"已遮盖表面仍被选取");
    grafted.objects.pop_back();dfv::daz::apply_graft_masks(grafted);require(grafted.scene.meshes[grafted.scene.instances[0].mesh].hidden_polygons.empty(),"删除插件未恢复宿主面");
    auto bad_graft=graft_asset;bad_graft["geometry_library"][1]["graft"]["vertex_pairs"]["values"][0][1]=4;write(asset_path,bad_graft);
    rejected=false;try {dfv::daz::load(path);} catch(...) {rejected=true;}require(rejected,"GeoGraft 接缝目标顶点越界没有拒绝");
    write(asset_path,asset);
    auto grouped=duf;grouped["node_library"]={{{"id","group-def"},{"label","Classroom"},{"extra",{{{"type","studio/node/group_node"}}}}}};
    grouped["scene"]["nodes"].push_back({{"id","outer"},{"label","School"},{"extra",{{{"type","studio/node/group_node"}}}}});
    grouped["scene"]["nodes"].push_back({{"id","inner"},{"url","#group-def"},{"parent","#outer"}});grouped["scene"]["nodes"][0]["parent"]="#inner";
    write(path,grouped);const auto groups=dfv::daz::load(path);size_t group_count=0;
    for(const auto &n:groups.nodes) if(n.group) {++group_count;if(n.id=="inner") require(n.parent=="#outer"&&n.label=="Classroom","Group 丢失继承标签或父节点");}
    require(group_count==2&&groups.objects[0].parent=="#inner","空 Group 或其子对象被平摊");
    write(path,duf);
    asset["geometry_library"][0]["vertices"]["count"]=5;write(asset_path,asset);
    rejected=false;try {(void)dfv::daz::load(path);} catch(const std::exception &) {rejected=true;}require(rejected,"无效 DSON count 没有拒绝");
    std::cout<<"IR references / UTF-8 URI / gzip / material binding / UV seams / transforms: PASS\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
