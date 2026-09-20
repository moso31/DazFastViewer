#include "runtime/morph.h"
#include "daz/morphs.h"
#include <chrono>
#include <fstream>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
static void require(bool condition,const char *message) {if(!condition) throw std::runtime_error(message);}
int wmain(int argc,wchar_t **argv) {
  try {
    using namespace dfv;
    if(argc>=2) {
      auto loaded=daz::load(std::filesystem::path(argv[1]));std::vector<std::filesystem::path> roots;
      for(const auto &r:loaded.report["content_roots"]) roots.push_back(std::filesystem::u8path(r.get<std::string>()));
      auto catalog=daz::discover_morphs(loaded,roots);runtime::MorphRuntime runtime(loaded.scene,catalog.targets);
      const auto &target=catalog.targets.at(0);size_t selected=target.morphs.size();
      for(size_t i=0;i<target.morphs.size();++i) if(target.morphs[i].label=="Bodybuilder Size" && target.morphs[i].unsupported.empty()) selected=i;
      require(selected<target.morphs.size(),"真实角色没有发现兼容 Bodybuilder Size Morph");
      const auto mesh=loaded.scene.instances[target.instance].mesh;const auto base=loaded.scene.meshes[mesh].positions;
      const auto raw=daz::read_document_file(std::filesystem::u8path(target.morphs[selected].source));
      const auto &modifier=raw.at("modifier_library").at(0);double maximum=0;
      for(float weight:{.5f,1.0f,0.0f}) {
        runtime.set_morph(0,selected,weight);auto changed=runtime.evaluate();require(changed.meshes.size()==1,"真实 Morph 没有生成顶点 Delta");
        auto expected=base;
        for(const auto &row:modifier.at("morph").at("deltas").at("values")) {
          auto &p=expected.at(row[0].get<size_t>());p.x+=row[1].get<float>()*.01f*weight;p.y-=row[3].get<float>()*.01f*weight;p.z+=row[2].get<float>()*.01f*weight;
        }
        for(size_t i=0;i<base.size();++i) {
          const auto a=loaded.scene.meshes[mesh].positions[i],b=expected[i];
          maximum=std::max(maximum,double(std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)})));
        }
      }
      require(maximum<1e-6,"真实 Morph 与原始 DSF 数值不符");
      nlohmann::json result={{"status","PASS"},{"morph",target.morphs[selected].label},{"offset_count",target.morphs[selected].offsets.size()},
        {"weights",{.5,1,0}},{"maximum_coordinate_error_m",maximum},{"catalog",catalog.report},{"golden","DAZ_STUDIO_NOT_RUN"}};
      if(argc>=3) std::ofstream(std::filesystem::path(argv[2]))<<result.dump(2);
      std::cout<<"Actual Genesis Morph, weights 0.5 / 1 / 0: PASS, max error "<<maximum<<'\n';return 0;
    }
    ir::Scene scene;ir::Mesh mesh;mesh.positions={{1,2,3},{4,5,6}};scene.meshes={mesh,mesh};
    ir::Instance a,b;b.mesh=1;scene.instances={a,b};
    runtime::Morph m;m.minimum=-1;m.maximum=2;m.offsets={{0,{.1f,.2f,.3f}}};
    runtime::Morph n=m;n.offsets={{0,{-.2f,.4f,0}},{1,{.5f,0,0}}};
    runtime::Target first;first.morphs={m,n};runtime::Target second=first;second.instance=1;
    std::vector<runtime::Target> targets={first,second};runtime::MorphRuntime runtime(scene,targets);
    require(runtime.evaluate().meshes.empty(),"零权重不应求值");
    runtime.set_morph(0,0,1);runtime.set_morph(0,1,-.5f);auto delta=runtime.evaluate();
    require(delta.meshes.size()==1 && std::abs(scene.meshes[0].positions[0].x-1.2f)<1e-6,"重叠稀疏 Morph 叠加错误");
    require(scene.meshes[1].positions[0].x==1,"Morph 污染其他实例");
    require(!runtime.set_morph(0,0,1) && runtime.evaluate().meshes.empty(),"重复权重产生多余求值");
    require(runtime.stats().offsets_visited==3,"未按 Active Set 遍历差值");
    runtime.set_morph(0,0,99);require(runtime.values()[0].morphs[0]==2,"clamp 失效");
    runtime.set_morph(0,0,0);runtime.set_morph(0,1,0);runtime.evaluate();
    require(scene.meshes[0].positions[0].x==1 && scene.meshes[0].positions[1].x==4,"恢复零值产生漂移");
    runtime::TransformValues transform;transform.translation_cm={100,200,300};runtime.set_transform(0,transform);
    const auto before=runtime.stats().morph_evaluations;delta=runtime.evaluate();
    const auto p=delta.instances.at(0).transform.point({0,0,0});
    require(p.x==1 && p.y==-3 && p.z==2 && delta.meshes.empty(),"变换单位、坐标系或脏传播错误");
    require(runtime.stats().morph_evaluations==before,"对象变换触发了 Morph 求值");
    bool rejected=false;try {runtime.set_morph(0,0,std::numeric_limits<float>::quiet_NaN());} catch(...) {rejected=true;}
    require(rejected,"NaN 权重未拒绝");
    targets[0].morphs[0].offsets[0].vertex=999;rejected=false;
    try {runtime::MorphRuntime invalid(scene,targets);} catch(...) {rejected=true;}
    require(rejected,"错误拓扑未拒绝");
    namespace fs=std::filesystem;
    const auto directory=fs::current_path()/"morph-test-data"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(directory/"data"/"Morphs");std::ofstream(directory/"data"/"figure.dsf")<<"{}";
    auto fixture=nlohmann::json::parse(R"({"modifier_library":[{"id":"shape","parent":"/data/figure.dsf#geometry","channel":{"type":"float","label":"Test","value":0,"min":-1,"max":1},"morph":{"vertex_count":2,"deltas":{"count":1,"values":[[1,10,20,30]]}}}]})");
    const auto fixture_path=directory/"data"/"Morphs"/fs::u8path("中文形态.dsf");std::ofstream(fixture_path)<<fixture.dump();
    daz::LoadedScene input;input.scene.meshes={mesh};input.scene.instances={a};input.objects.push_back({0,"figure","角色","","geometry",directory/"data"/"figure.dsf",true});
    auto catalog=daz::discover_morphs(input,{directory});require(catalog.targets[0].morphs.size()==1,"中文路径 Morph 没有发现");
    const auto offset=catalog.targets[0].morphs[0].offsets[0].delta;
    require(std::abs(offset.x-.1f)<1e-6 && std::abs(offset.y+.3f)<1e-6 && std::abs(offset.z-.2f)<1e-6,"DSF 差值坐标转换错误");
    const auto scene_file=directory/"saved.duf";
    std::ofstream(scene_file)<<R"({"scene":{"modifiers":[{"url":"/data/Morphs/中文形态.dsf#shape","parent":"#figure","channel":{"current_value":0.25}}]}})";
    const auto scene_name=scene_file.generic_u8string();input.report["input"]=std::string(scene_name.begin(),scene_name.end());
    require(daz::discover_morphs(input,{directory}).targets[0].morphs[0].initial==.25f,"场景保存的 Morph 权重没有覆盖默认值");
    fixture["modifier_library"][0]["parent"]="/data/other.dsf#geometry";std::ofstream(fixture_path)<<fixture.dump();
    require(daz::discover_morphs(input,{directory}).targets[0].morphs.empty(),"仅凭顶点数错误匹配另一角色 Morph");
    std::cout<<"Sparse Morph / active set / reset / instance isolation / transform dirtiness: PASS\n";
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
