#include "editor/document.h"
#include "daz/documents.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

using namespace dfv;
using J=nlohmann::json;
namespace fs=std::filesystem;
static void require(bool v,const char *message) {if(!v) throw std::runtime_error(message);}
static void write(const fs::path &file,const J &value) {fs::create_directories(file.parent_path());std::ofstream(file)<<value.dump();}
static J parameter(const std::string &id) {return {{"id",id},{"parent","/data/figure.dsf#geometry"},{"channel",{{"type","float"},{"label",id},{"value",0},{"min",0},{"max",1}}}};}
static J shape(const std::string &id,int vertex,float delta) {auto p=parameter(id);p["morph"]={{"vertex_count",3},{"deltas",{{"count",1},{"values",{{vertex,delta,0,0}}}}}};return p;}
static size_t index(const runtime::Target &target,const std::string &id) {for(size_t i=0;i<target.morphs.size();++i) if(target.morphs[i].channel_id==id) return i;throw std::runtime_error("缺少测试参数："+id);}
static void ready(runtime::DeformationRuntime &r,const editor::Snapshot &s) {
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  for(;;) {const auto p=r.prepare(s.values,s.poses);require(p.error.empty(),p.error.c_str());if(!p.pending) return;require(std::chrono::steady_clock::now()<end,"异步资源等待超时");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
static bool equal(const ir::Scene &a,const ir::Scene &b) {
  if(a.meshes.size()!=b.meshes.size()) return false;
  for(size_t m=0;m<a.meshes.size();++m) {const auto &x=a.meshes[m].positions,&y=b.meshes[m].positions;if(x.size()!=y.size()||!std::equal(x.begin(),x.end(),y.begin(),[](auto p,auto q){return p.x==q.x&&p.y==q.y&&p.z==q.z;})) return false;}return true;
}
static void resources() {
  std::atomic<int> calls=0;auto release=std::make_shared<std::promise<void>>();auto gate=release->get_future().share();
  auto p=std::make_shared<runtime::MorphPayload>("shared",1,3,[&calls,gate]{++calls;gate.wait();return runtime::OffsetBuffer{{0,{1,0,0}}};});
  for(int i=0;i<100;++i) p->request();release->set_value();auto a=p->ensure(),b=p->ensure();
  require(calls==1&&a==b,"重复请求未合并为一个不可变缓冲区");require(!p->evict(),"淘汰了仍被求值器引用的差值");a.reset();b.reset();require(p->evict()>0,"非活动缓冲区不能淘汰");p->ensure();require(calls==2,"淘汰后的资源不能重新加载");
  std::atomic<int> tries=0;
  auto retry=std::make_shared<runtime::MorphPayload>("retry",1,3,[&]{if(++tries==1) throw std::runtime_error("临时 I/O 错误");return runtime::OffsetBuffer{{0,{1,0,0}}};});
  bool failed=false;try {retry->ensure();} catch(const std::exception &) {failed=true;}
  require(failed&&retry->state()==runtime::PayloadState::failed,"读取失败没有独立资源状态");retry->request(true);require(retry->ensure()->size()==1&&tries==2,"读取失败不能重试");
  auto invalid=std::make_shared<runtime::MorphPayload>("invalid",2,3,[]{return runtime::OffsetBuffer{{0,{1,0,0}},{0,{2,0,0}}};});
  failed=false;try {invalid->ensure();} catch(const std::exception &) {failed=true;}require(failed,"异步加载未校验重复顶点");
}
int main() {
  try {
    resources();
    const auto folder=fs::temp_directory_path()/("dfv-lazy-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto base=folder/"data/figure.dsf";
    write(base,J::parse(R"({"node_library":[{"id":"figure","type":"figure"}],"geometry_library":[{"id":"geometry","vertices":{"values":[[0,0,0],[100,0,0],[0,100,0]]}}]})"));
    auto a=shape("A",0,10),b=shape("B",1,20),control=parameter("Controller");
    control["formulas"]=J::array();for(const auto id:{"A","B"}) control["formulas"].push_back({{"output",std::string("figure:/data/Morphs/")+id+".dsf#"+id+"?value"},{"operations",{{{"op","push"},{"url","figure:#Controller?value"}}}}});
    write(folder/"data/Morphs/A.dsf",{{"modifier_library",{a}}});write(folder/"data/Morphs/B.dsf",{{"modifier_library",{b}}});write(folder/"data/Morphs/Controller.dsf",{{"modifier_library",{control}}});
    editor::Document d;d.generation=1;ir::Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{0,1,0}};ir::Triangle face;face.vertices={0,1,2};mesh.triangles={face};
    for(int i=0;i<2;++i) {const auto id="person"+std::to_string(i);d.loaded.scene.meshes.push_back(mesh);ir::Instance instance;instance.id=id+"/geometry";instance.mesh=uint32_t(i);d.loaded.scene.instances.push_back(instance);d.loaded.objects.push_back({uint32_t(i),id,id,"","geometry",base,true});d.loaded.objects.back().geometry_versions={{base,daz::file_version(base)}};}
    auto eager_loaded=d.loaded;auto eager=daz::discover_morphs(eager_loaded,{folder});d.catalog=daz::discover_morphs(d.loaded,{folder},{},true);
    require(d.catalog.report==eager.report,"按需目录改变了参数、默认值或诊断报告");
    d.formulas=daz::enable_formulas(d.catalog,d.skeletons);auto eager_formulas=daz::enable_formulas(eager,d.skeletons);
    require(d.formulas.report==eager_formulas.report,"按需目录改变了 ERC 编译结果");
    const auto ai=index(d.catalog.targets[0],"A"),bi=index(d.catalog.targets[0],"B"),ci=index(d.catalog.targets[0],"Controller");
    auto pa=d.catalog.targets[0].morphs[ai].payload,pb=d.catalog.targets[0].morphs[bi].payload;
    {
      auto division=eager;auto &source=division.formulas[0];source={};source.alias_symbols.resize(division.targets[0].morphs.size());
      J expression={{"output",division.targets[0].morphs[ai].id+"?value"},{"operations",{{{"op","push"},{"val",1}},{{"op","push"},{"val",1}},{{"op","push"},{"url",division.targets[0].morphs[ci].id+"?value"}},{{"op","sub"}},{{"op","div"}}}}};
      daz::append_formulas(source,uint32_t(ci),J::array({expression}),[](const std::string &address){return address;});auto graphs=daz::enable_formulas(division,d.skeletons);
      auto current=d.loaded.scene;runtime::DeformationRuntime value(current,division.targets,d.skeletons.skins,graphs.graphs);auto desired=editor::initial_snapshot(d);value.evaluate(desired.values,desired.poses);const auto valid=current;
      desired.values[0].morphs[ci]=1;for(int attempt=0;attempt<2;++attempt) {bool failed=false;try {value.prepare(desired.values,desired.poses);} catch(const std::exception &) {failed=true;}require(failed&&equal(current,valid),"准备阶段 ERC 失败后没有回滚，重复提交绕过了错误");}
      desired.values[0].morphs[ci]=.5f;ready(value,desired);value.evaluate(desired.values,desired.poses);require(value.effective()[0][ai]==2,"ERC 错误后无法恢复正常求值");
    }
    require(pa&&pa==d.catalog.targets[1].morphs[ai].payload&&pa->state()==runtime::PayloadState::unloaded,"零权重 Morph 被预加载或跨实例重复存储");
    {
      std::promise<void> release;auto gate=release.get_future().share();
      struct Release {std::promise<void> &promise;~Release(){try {promise.set_value();} catch(...) {}}} guard{release};
      auto delayed=d.catalog;auto block=std::make_shared<runtime::MorphPayload>("delayed",1,3,[gate]{gate.wait();return runtime::OffsetBuffer{{0,{.1f,0,0}}};});delayed.targets[0].morphs[ai].payload=block;
      auto pending_scene=d.loaded.scene;runtime::DeformationRuntime pending(pending_scene,delayed.targets,d.skeletons.skins,d.formulas.graphs);auto desired=editor::initial_snapshot(d);
      desired.values[0].morphs[ci]=.5f;require(pending.prepare(desired.values,desired.poses).pending>0,"阻塞资源被误报为已完成");
      desired.values[0].morphs[ci]=0;require(pending.prepare(desired.values,desired.poses).pending==0,"归零后仍等待过期资源");pending.evaluate(desired.values,desired.poses);
      require(equal(pending_scene,d.loaded.scene),"未就绪请求或归零写入了半成品几何");release.set_value();block->ensure();require(equal(pending_scene,d.loaded.scene),"过期后台请求修改了场景");
      desired.values[0].morphs[ci]=.2f;pending.prepare(desired.values,desired.poses);desired.values[0].morphs[ci]=.8f;ready(pending,desired);pending.evaluate(desired.values,desired.poses);require(pending.effective()[0][ai]==.8f,"未应用最新依赖权重");
    }
    auto scene=d.loaded.scene;runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);auto snapshot=editor::initial_snapshot(d);runtime.evaluate(snapshot.values,snapshot.poses);
    require(pa->state()==runtime::PayloadState::unloaded,"构建或初始零权重求值读入了全部差值");
    snapshot.values[0].morphs[ci]=.2f;runtime.prepare(snapshot.values,snapshot.poses);snapshot.values[0].morphs[ci]=.7f;ready(runtime,snapshot);
    require(equal(scene,d.loaded.scene),"准备依赖时提交了部分形变");require(pa->state()==runtime::PayloadState::ready&&pb->state()==runtime::PayloadState::ready,"纯控制器没有补齐所有差值依赖");
    runtime.evaluate(snapshot.values,snapshot.poses);
    auto eager_scene=eager_loaded.scene;runtime::DeformationRuntime eager_runtime(eager_scene,eager.targets,d.skeletons.skins,eager_formulas.graphs);eager_runtime.evaluate(snapshot.values,snapshot.poses);
    require(equal(scene,eager_scene)&&runtime.effective()==eager_runtime.effective(),"异步提交最新值与全量路径结果不同");require(scene.meshes[1].positions[0].x==0,"共享差值污染了其他实例的权重");
    snapshot.values[0].morphs[ci]=0;runtime.prepare(snapshot.values,snapshot.poses);runtime.evaluate(snapshot.values,snapshot.poses);require(equal(scene,d.loaded.scene),"归零后迟到的资源重新应用了旧值");
    write(folder/"data/Morphs/New.dsf",{{"modifier_library",{shape("New",2,30)}}});
    editor::release_load_data(d);auto refreshed=editor::refresh_parameters(d,0,{folder});
    require(refreshed->generation==d.generation&&refreshed->asset_revision==1&&refreshed->catalog.targets[0].morphs.size()==4&&refreshed->catalog.targets[1].morphs.size()==3,"目录刷新未发现新参数或误刷新无关角色");
    const auto ni=index(refreshed->catalog.targets[0],"New");auto old=refreshed->catalog.targets[0].morphs[ni].payload;
    write(folder/"data/Morphs/New.dsf",{{"modifier_library",{shape("New",2,123.5f)}}});
    bool failed=false;try {old->ensure();} catch(const std::exception &) {failed=true;}require(failed,"变化的资产被旧版本请求静默接收");
    auto changed=editor::refresh_parameters(*refreshed,0,{folder});auto fresh=changed->catalog.targets[0].morphs[index(changed->catalog.targets[0],"New")].payload;
    require(fresh!=old&&std::abs(fresh->ensure()->at(0).delta.x-1.235f)<1e-6f,"刷新未使缓存失效或未替换失败请求");
    auto corrupt=shape("Corrupt",0,1);corrupt["morph"]["deltas"]["count"]=0;write(folder/"data/Morphs/Corrupt.dsf",{{"modifier_library",{corrupt}}});auto invalid=daz::discover_morphs(d.loaded,{folder},{},true);require(!invalid.report["diagnostics"].empty(),"元数据扫描遗漏 deltas.count 错误");
    write(base,{{"node_library",J::array()}});failed=false;try {editor::refresh_parameters(d,0,{folder});} catch(const std::exception &) {failed=true;}require(failed,"基础几何变化后仍沿用旧索引");
    // 仅移除此测试创建的独立临时目录。
    fs::remove_all(folder);std::cout<<"Lazy metadata / shared resources / dependency closure / latest values / refresh / version / retry: PASS\n";
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
