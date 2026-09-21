#include "daz/morphs.h"
#include "daz/skeleton.h"
#include "runtime/deformation.h"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace dfv;
using J=nlohmann::json;
namespace fs=std::filesystem;
static void require(bool valid,const char *message) {if(!valid) throw std::runtime_error(message);}
static void write(const fs::path &p,const J &value) {fs::create_directories(p.parent_path());std::ofstream(p)<<value.dump();}
static J parameter(const std::string &id,const std::string &parent="geometry") {
  return {{"id",id},{"parent","/data/figure.dsf#"+parent},{"channel",{{"type","float"},{"label",id},{"value",0},{"min",0},{"max",1}}}};
}
static J shape(const std::string &id,int count=2,const std::string &parent="geometry") {
  auto p=parameter(id,parent);p["morph"]={{"vertex_count",count},{"deltas",{{"count",1},{"values",{{0,10,0,0}}}}}};return p;
}
static J controller(const std::string &id,const std::string &url) {
  auto p=parameter(id,"figure");p["formulas"]={{{"output",url},{"operations",{{{"op","push"},{"url","figure:#"+id+"?value"}}}}}};return p;
}
static const runtime::Morph &find(const daz::MorphCatalog &catalog,const std::string &id) {
  for(const auto &m:catalog.targets[0].morphs) if(m.channel_id==id) return m;throw std::runtime_error("缺少测试参数："+id);
}
int main() {
  try {
    const auto base=fs::current_path()/"morph-compatibility-data"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto first=base/"first",second=base/"second";
    write(first/"data/figure.dsf",{{"node_library",{{{"id","figure"},{"type","figure"}},{{"id","head"},{"parent","#figure"}}}},
      {"modifier_library",{{{"id","skin"},{"skin",{{"geometry","#geometry"},{"node","#figure"}}}}}}});
    auto unknown=shape("unknown",-1,"figure");write(first/"data/Morphs/vendor/unknown.dsf",{{"modifier_library",{unknown}}});
    write(first/"data/Morphs/vendor/unproven.dsf",{{"modifier_library",{shape("unproven",-1,"head")}}});
    write(first/"data/Morphs/vendor/mismatch.dsf",{{"modifier_library",{shape("mismatch",3,"figure")}}});
    write(first/"data/Morphs/vendor/negative.dsf",{{"modifier_library",{shape("negative",-2,"figure")}}});
    auto bad=unknown;bad["id"]="bad_index";bad["morph"]["deltas"]["values"][0][0]=2;write(first/"data/Morphs/vendor/bad.dsf",{{"modifier_library",{bad}}});
    auto leaf=shape("Shared");write(first/"data/Morphs/vendor/NewName.dsf",{{"modifier_library",{leaf}}});
    auto other=leaf;other["morph"]["deltas"]["values"][0][1]=30;write(first/"data/Morphs/other/NewName.dsf",{{"modifier_library",{other}}});
    write(second/"data/Morphs/vendor/NewName.dsf",{{"modifier_library",{other}}});
    auto control=controller("control","figure:/data/Morphs/vendor/OldName.dsf#Shared?value");write(first/"data/Morphs/controllers/control.dsf",{{"modifier_library",{control}}});
    auto alias=parameter("alias","head");alias["channel"]["type"]="alias";alias["channel"]["target_channel"]="figure:/data/Morphs/vendor/OldName.dsf#Shared?value";
    write(first/"data/Morphs/controllers/alias.dsf",{{"modifier_library",{alias}}});
    // 存在的空文件和同目录歧义必须停止恢复；不能改为全局 ID 匹配。
    write(first/"data/Morphs/vendor/Empty.dsf",{{"file_version","0.6.0.0"}});
    write(first/"data/Morphs/controllers/empty.dsf",{{"modifier_library",{controller("empty","figure:/data/Morphs/vendor/Empty.dsf#Shared?value")}}});
    write(first/"data/Morphs/ambiguous/a.dsf",{{"modifier_library",{leaf}}});write(first/"data/Morphs/ambiguous/b.dsf",{{"modifier_library",{other}}});
    write(first/"data/Morphs/controllers/ambiguous.dsf",{{"modifier_library",{controller("ambiguous","figure:/data/Morphs/ambiguous/missing.dsf#Shared?value")}}});
    write(first/"data/Morphs/controllers/wrong_directory.dsf",{{"modifier_library",{controller("wrong_directory","figure:/data/Morphs/absent/missing.dsf#Shared?value")}}});
    write(first/"data/Morphs/controllers/blocked.dsf",{{"modifier_library",{controller("blocked","figure:/data/Morphs/vendor/mismatch.dsf#mismatch?value")}}});
    // 文件搬迁后，用完整声明 URI 加内部 ID 恢复；其他目录中的同名参数不能参与猜测。
    const std::string logical="/data/Morphs/Original%20Product/Leaf.dsf";
    write(first/"data/Morphs/moved/leaf.dsf",{{"asset_info",{{"id",logical}}},{"modifier_library",{leaf}}});
    write(second/"data/Morphs/moved/leaf.dsf",{{"asset_info",{{"id",logical}}},{"modifier_library",{other}}});
    write(first/"data/Morphs/controllers/moved.dsf",{{"modifier_library",{controller("moved","figure:"+logical+"#Shared?value")}}});
    auto moved_alias=alias;moved_alias["id"]="moved_alias";moved_alias["channel"]["target_channel"]="figure:"+logical+"#Shared?value";
    write(first/"data/Morphs/controllers/moved_alias.dsf",{{"modifier_library",{moved_alias}}});
    const std::string collision="/data/Morphs/Missing/collision.dsf";
    write(first/"data/Morphs/moved/collision_a.dsf",{{"asset_info",{{"id",collision}}},{"modifier_library",{leaf}}});
    write(first/"data/Morphs/moved/collision_b.dsf",{{"asset_info",{{"id",collision}}},{"modifier_library",{other}}});
    write(first/"data/Morphs/controllers/collision.dsf",{{"modifier_library",{controller("collision","figure:"+collision+"#Shared?value")}}});
    write(first/"data/Morphs/moved/empty_redirect.dsf",{{"asset_info",{{"id","/data/Morphs/vendor/Empty.dsf"}}},{"modifier_library",{leaf}}});
    auto foreign=leaf;foreign["parent"]="/data/figure.dsf#other_geometry";
    write(first/"data/Morphs/moved/foreign.dsf",{{"asset_info",{{"id","/data/Morphs/Foreign/leaf.dsf"}}},{"modifier_library",{foreign}}});
    write(first/"data/Morphs/controllers/foreign_control.dsf",{{"modifier_library",{controller("foreign_control","figure:/data/Morphs/Foreign/leaf.dsf#Shared?value")}}});
    daz::LoadedScene loaded;ir::Mesh mesh;mesh.positions={{0,0,0},{1,1,1}};loaded.scene.meshes={mesh};loaded.scene.instances.emplace_back();
    loaded.objects.push_back({0,"figure","Figure","","geometry",first/"data/figure.dsf",true});
    auto catalog=daz::discover_morphs(loaded,{first,second});
    require(find(catalog,"unknown").offsets.size()==1&&find(catalog,"unknown").source_vertex_count==-1&&find(catalog,"unknown").geometry_validation=="skin_binding_node","-1 计数或角色到几何绑定未修复");
    require(find(catalog,"unproven").offsets.empty()&&!find(catalog,"unproven").intrinsic_error.empty(),"未经验证的节点不能仅凭索引范围套用");
    require(!find(catalog,"mismatch").intrinsic_error.empty()&&!find(catalog,"negative").intrinsic_error.empty(),"明确不匹配或其他负计数未拒绝");
    require(catalog.report["diagnostics"].size()==1,"-1 计数绕过了实际差值索引校验");
    require(find(catalog,"control").repaired_references==1&&find(catalog,"alias").repaired_references==1,"控制器或别名引用没有统一恢复");
    auto compiled=daz::enable_formulas(catalog,{});
    require(find(catalog,"control").unsupported.empty()&&find(catalog,"alias").unsupported.empty(),"恢复后的参数仍未启用");
    require(find(catalog,"moved").unsupported.empty()&&find(catalog,"moved_alias").unsupported.empty(),"逻辑资产地址未恢复搬迁后的控制器或别名");
    require(!find(catalog,"collision").unsupported.empty()&&!find(catalog,"foreign_control").unsupported.empty(),"逻辑地址歧义或其他角色的参数没有拒绝");
    require(!find(catalog,"empty").unsupported.empty()&&!find(catalog,"ambiguous").unsupported.empty()&&!find(catalog,"wrong_directory").unsupported.empty(),"空覆盖、歧义或跨目录误匹配");
    require(find(catalog,"blocked").unsupported.find("mismatch")!=std::string::npos,"总控制器没有显示下游阻碍");
    runtime::Properties values;for(const auto &m:catalog.targets[0].morphs) values.morphs.push_back(m.initial);
    size_t index=0;while(catalog.targets[0].morphs[index].channel_id!="control") ++index;values.morphs[index]=.5f;
    const std::vector<runtime::Skin> skins;runtime::DeformationRuntime runtime(loaded.scene,catalog.targets,skins,compiled.graphs);
    runtime.evaluate({values},{});require(std::abs(loaded.scene.meshes[0].positions[0].x-.05f)<1e-6,"恢复到了错误目录、错误优先级或重复同名参数");
    values.morphs[index]=0;runtime.evaluate({values},{});require(loaded.scene.meshes[0].positions[0].x==0,"恢复后的参数归零漂移");
    index=0;while(catalog.targets[0].morphs[index].channel_id!="moved") ++index;values.morphs[index]=.5f;
    runtime.evaluate({values},{});require(std::abs(loaded.scene.meshes[0].positions[0].x-.05f)<1e-6,"逻辑地址恢复到了错误参数或未遵循根目录优先级");
    values.morphs[index]=0;runtime.evaluate({values},{});require(loaded.scene.meshes[0].positions[0].x==0,"逻辑地址恢复后的参数归零漂移");
    auto asset=daz::read_document_file(first/"data/figure.dsf");auto another=asset["modifier_library"][0];another["id"]="other_skin";another["skin"]["geometry"]="#other_geometry";asset["modifier_library"].push_back(another);
    write(first/"data/figure.dsf",asset);const auto ambiguous_geometry=daz::discover_morphs(loaded,{first,second});
    require(!find(ambiguous_geometry,"unknown").intrinsic_error.empty(),"同一 Figure 的多几何歧义没有阻止节点 Morph 套用");
    std::cout<<"Morph compatibility / signed counts / binding identity / scoped URI repair / ambiguity / reset: PASS\n";return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
