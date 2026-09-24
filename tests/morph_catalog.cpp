#include "daz/morphs.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using J=nlohmann::json;
namespace fs=std::filesystem;
static void require(bool condition,const char *message) {if(!condition) throw std::runtime_error(message);}
static void write(const fs::path &p,const J &j) {fs::create_directories(p.parent_path());std::ofstream(p)<<j.dump(2);}
static J control(const char *id,const char *label,const char *parent) {
  return {{"id",id},{"parent",parent},{"group","/Pose Controls/Head/Expressions/子级"},{"channel",{{"id","value"},{"type","float"},{"label",label},{"value",0},{"min",0},{"max",1}}}};
}
int wmain(int argc,wchar_t **argv) {
  try {
    using namespace dfv;
    if(argc>=4) {
      daz::LoadOptions options;bool lazy=false;for(int i=3;i<argc;++i) {if(std::wstring(argv[i])==L"--lazy") lazy=true;else options.content_roots.emplace_back(argv[i]);}
      auto loaded=daz::load(fs::path(argv[1]),options);std::vector<fs::path> roots;
      for(const auto &root:loaded.report["content_roots"]) roots.push_back(fs::u8path(root.get<std::string>()));
      const auto started=std::chrono::steady_clock::now();auto catalog=daz::discover_morphs(loaded,roots,{},lazy);
      catalog.report["elapsed_seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
      catalog.report["input"]=loaded.report["input"];std::ofstream(fs::path(argv[2]))<<catalog.report.dump(2);
      std::cout<<"Catalog inspected: "<<catalog.targets.at(0).morphs.size()<<" parameters, "<<catalog.report["diagnostics"].size()<<" diagnostics\n";return 0;
    }
    const auto base=fs::current_path()/"morph-catalog-data"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto first=base/"first",second=base/fs::u8path("第二内容库");
    const J geometry={{"node_library",J::array({{{"id","figure"},{"type","figure"}},{{"id","head"},{"parent","#figure"}}})}};
    write(first/"data/figure.dsf",geometry);write(second/"data/figure.dsf",geometry);
    auto shape=control("shape","Shape","/data/figure.dsf#geometry");shape["morph"]={{"vertex_count",2},{"deltas",{{"count",1},{"values",{{0,1,2,3}}}}}};
    write(first/"data/Morphs/vendor/deep/shape.dsf",{{"modifier_library",{shape}}});
    auto lower_priority=shape;lower_priority["channel"]["label"]="Wrong priority";
    write(second/"data/Morphs/vendor/deep/shape.dsf",{{"modifier_library",{lower_priority}}});
    auto controller=control("controller","Controller","/data/figure.dsf#figure");
    controller["formulas"]={{{"output","figure:/data/External/other.dsf#external?value"},{"operations",{{{"op","push"},{"url","figure:#controller?value"}}}}}};
    write(second/"data/Morphs/vendor/deep/controller.dsf",{{"modifier_library",{controller}}});
    auto external=control("external","External","/data/figure.dsf#figure");
    write(second/"data/External/other.dsf",{{"modifier_library",{external}}});
    auto alias=control("shape","Alias","/data/figure.dsf#head");alias["channel"]["type"]="alias";
    alias["channel"]["target_channel"]="figure:/data/Morphs/vendor/deep/shape.dsf#shape?value";
    write(second/"data/Morphs/vendor/alias.dsf",{{"modifier_library",{alias}}});
    auto missing=controller;missing["id"]="missing";missing["formulas"][0]["output"]="figure:/data/missing.dsf#absent?value";
    write(second/"data/Morphs/vendor/missing.dsf",{{"modifier_library",{missing}}});
    daz::LoadedScene loaded;ir::Mesh mesh;mesh.positions={{0,0,0},{1,1,1}};loaded.scene.meshes={mesh};loaded.scene.instances.emplace_back();
    loaded.objects.push_back({0,"figure","Figure","","geometry",first/"data/figure.dsf",true});
    auto catalog=daz::discover_morphs(loaded,{first,second});const auto &morphs=catalog.targets.at(0).morphs;
    require(morphs.size()==5,"多根目录、子目录或外部依赖参数遗漏");
    auto find=[&](const char *label)->const runtime::Morph & {for(const auto &m:morphs) if(m.label==label) return m;throw std::runtime_error("缺少预期参数");};
    require(find("Shape").unsupported.empty(),"直接 Morph 被错误禁用");
    require(find("Controller").kind=="control" && find("Controller").missing_dependencies==0,"纯公式控制器 / 外部依赖没有识别");
    require(find("Alias").kind=="alias" && find("Alias").owner=="head" && find("Alias").missing_dependencies==0,"子节点别名或相同 ID 被错误丢弃");
    require(find("Controller").group=="/Pose Controls/Head/Expressions/子级","UI 分组丢失");
    require(catalog.report["file_overrides"].size()==1,"重复资源优先级没有记录");
    bool saw_missing=false;for(const auto &m:morphs) if(m.channel_id=="missing") saw_missing=m.missing_dependencies>0;
    require(saw_missing,"缺失依赖未诊断");
    require(catalog.report["diagnostics"].empty(),"合法目录产生解析错误");
    auto incompatible=shape;incompatible["id"]="different_topology";incompatible["channel"]["label"]="Different topology";incompatible["morph"]["vertex_count"]=3;
    write(second/"data/Morphs/vendor/topology.dsf",{{"modifier_library",{incompatible}}});
    const auto with_incompatible=daz::discover_morphs(loaded,{first,second});bool retained=false;
    for(const auto &m:with_incompatible.targets[0].morphs) if(m.channel_id=="different_topology") retained=m.kind=="unverified_sparse" && !m.unsupported.empty() && m.offsets.empty();
    require(retained,"不兼容形态没有保留为可诊断的参数");
    // G8.1 的空覆盖不能被公式依赖扫描重新引入。
    const auto family=first/"data/DAZ 3D/Genesis 8";
    const J g={{"geometry_library",{{{"id","geometry"},{"vertices",{{"count",2}}},{"polylist",{{"values",J::array()}}}}}}};
    write(family/"Female/Genesis8Female.dsf",g);write(family/"Female 8_1/Genesis8_1Female.dsf",g);
    auto old=shape;old["parent"]="/data/DAZ%203D/Genesis%208/Female/Genesis8Female.dsf#geometry";
    write(family/"Female/Morphs/vendor/disabled.dsf",{{"modifier_library",{old}}});
    write(family/"Female 8_1/Morphs/vendor/disabled.dsf",{{"file_version","0.6.0.0"}});
    auto follower=old;follower["id"]="follower";follower.erase("morph");follower["formulas"]={{{"output","figure:/data/DAZ%203D/Genesis%208/Female/Morphs/vendor/disabled.dsf#shape?value"},{"operations",{{{"op","push"},{"val",1}}}}}};
    write(family/"Female/Morphs/vendor/follower.dsf",{{"modifier_library",{follower}}});
    loaded.objects[0].geometry_file=family/"Female 8_1/Genesis8_1Female.dsf";
    catalog=daz::discover_morphs(loaded,{first,second});require(catalog.targets[0].morphs.size()==1 && catalog.targets[0].morphs[0].channel_id=="follower","空覆盖被依赖扫描绕过");
    require(catalog.targets[0].morphs[0].missing_dependencies==1,"被屏蔽引用未明确报告");
    const auto lazy=daz::discover_morphs(loaded,{first,second},{},true);
    require(lazy.report==catalog.report,"元数据缓存改变了 G8.1 空覆盖或依赖诊断");
    std::cout<<"Multi-library discovery / priority / groups / node alias / external dependency / empty override: PASS\n";
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
