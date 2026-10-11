#include "editor/scene_extension.h"
#include "runtime/garment_deformation.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>

using namespace dfv;
using namespace dfv::runtime;
using J=nlohmann::json;
static void check(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
static double length(ir::Vec3 a,ir::Vec3 b){return std::sqrt(std::pow(double(a.x)-b.x,2)+std::pow(double(a.y)-b.y,2)+std::pow(double(a.z)-b.z,2));}
static FormulaGraph graph(const Target &t){FormulaGraph g;for(size_t m=0;m<t.morphs.size();++m){Channel c;c.binding={Property::morph,uint32_t(m),0};g.morph_channels.push_back(int(g.channels.size()));g.channels.push_back(c);}g.prepare();return g;}
static ir::Mesh strip() {
  ir::Mesh m;m.polygon_groups={"default","lFoot","lToe","rFoot","rToe"};
  for(int side=0;side<2;++side) {
    const auto begin=uint32_t(m.positions.size());
    for(int x=0;x<=12;++x)for(int y=0;y<2;++y)m.positions.push_back({float(x)*.01f,float(y)*.02f+side*.1f,0});
    for(uint32_t x=0;x<12;++x){ir::Triangle t;t.polygon_group=(x<6?1:2)+side*2;t.vertices={begin+x*2,begin+x*2+1,begin+x*2+2};m.triangles.push_back(t);t.vertices={begin+x*2+1,begin+x*2+3,begin+x*2+2};m.triangles.push_back(t);}
  }return m;
}
static void unit() {
  const auto mesh=strip();const auto regions=deformation_regions(mesh);check(regions.size()==2&&regions[0].id=="Foot"&&regions[1].id=="Toes","空组或左右部位归并错误");
  GarmentDeformation constraint(mesh);std::vector<ir::Vec3> offsets(mesh.positions.size());
  for(size_t v=0;v<offsets.size();++v){const auto p=mesh.positions[v];offsets[v]={p.x*.3f,0,p.x>.06f?float((v%3)*.01f):0};}
  const auto raw=offsets;constraint.constrain({},mesh.positions,offsets);check(offsets==raw,"默认开启改变结果");
  constraint.constrain({"Toes"},mesh.positions,offsets);
  auto point=[&](uint32_t v){auto p=mesh.positions[v],d=offsets[v];return ir::Vec3{p.x+d.x,p.y+d.y,p.z+d.z};};
  for(const auto &f:mesh.triangles)if(f.polygon_group==2||f.polygon_group==4)for(int k=0;k<3;++k)check(std::abs(length(point(f.vertices[k]),point(f.vertices[(k+1)%3]))-length(mesh.positions[f.vertices[k]],mesh.positions[f.vertices[(k+1)%3]]))<1e-6,"关闭区域仍发生局部伸缩");
  check(offsets[8]!=raw[8],"相邻开启区域没有过渡");
  auto contacts=raw;constraint.constrain({"Toes"},mesh.positions,contacts,true,false);check(contacts[8]==raw[8],"保形过程削弱了开启部位的防穿插修正");
  offsets=raw;constraint.constrain({"Foot","Toes"},mesh.positions,offsets);
  for(const auto &f:mesh.triangles)for(int k=0;k<3;++k)check(std::abs(length(point(f.vertices[k]),point(f.vertices[(k+1)%3]))-length(mesh.positions[f.vertices[k]],mesh.positions[f.vertices[(k+1)%3]]))<1e-6,"同时关闭相邻区域产生接缝");
  // 同位置的 UV 拆点共用约束，但保留原拓扑编号。
  auto split=mesh;split.positions.push_back(split.positions[12]);for(auto &f:split.triangles)if(f.polygon_group==2)for(auto &v:f.vertices)if(v==12)v=uint32_t(split.positions.size()-1);
  auto split_offsets=raw;split_offsets.push_back(raw[12]);GarmentDeformation(split).constrain({"Toes"},split.positions,split_offsets);check(split_offsets[12]==split_offsets.back(),"UV 拆点的边界被撕开");
  split_offsets.back().z+=.01f;GarmentDeformation(split).constrain({"Toes"},split.positions,split_offsets);check(split_offsets[12]==split_offsets.back(),"碰撞分别推动 UV 拆点后约束没有恢复连续性");
  auto foot_split=mesh;foot_split.positions.push_back(mesh.positions[0]);auto foot_offsets=raw;foot_offsets.push_back({0,0,.01f});GarmentDeformation(foot_split).constrain({"Toes"},foot_split.positions,foot_offsets,true,false);check(foot_offsets[0]==foot_offsets.back(),"防穿插撕开了仍开启形变部位的 UV 拆点");
  // 未焊接但贴近鞋面的拼片也必须跟随边界，不能只保护完全重合的拆点。
  auto layered=mesh;auto layered_offsets=raw;ir::Triangle patch;patch.polygon_group=1;
  for(int k=0;k<3;++k){auto p=mesh.positions[12+k];p.z+=.000002f;patch.vertices[k]=uint32_t(layered.positions.size());layered.positions.push_back(p);layered_offsets.push_back(raw[12+k]);}layered.triangles.push_back(patch);
  GarmentDeformation(layered).constrain({"Toes"},layered.positions,layered_offsets);
  for(int k=0;k<3;++k){const auto a=uint32_t(12+k),b=patch.vertices[k];auto p=layered.positions[a],q=layered.positions[b],u=layered_offsets[a],v=layered_offsets[b];check(length({p.x+u.x,p.y+u.y,p.z+u.z},{q.x+v.x,q.y+v.y,q.z+v.z})<.000003,"近距离搭接拼片被局部约束拉开");}
  // 同时覆盖投射、原生匹配、衣物自身调整、缓存、嵌套跟随及重开。
  for(bool authored:{false,true}) {
    editor::Document d;auto &scene=d.loaded.scene;scene.meshes={mesh,mesh,mesh};scene.instances.resize(3);
    for(uint32_t i=0;i<3;++i){scene.instances[i].mesh=i;scene.instances[i].id="instance"+std::to_string(i);}
    Morph shape;shape.id=shape.channel_id="Shape";shape.evaluable=shape.auto_follow=true;for(uint32_t v=0;v<raw.size();++v)shape.offsets.push_back({v,raw[v]});
    Target body;body.id="body/mesh";body.morphs={shape};Target cloth;cloth.id="cloth/mesh";cloth.instance=1;cloth.conform_target=authored?"body":"#body";
    Morph own;own.id=own.channel_id="OwnFit";own.evaluable=true;own.initial=.5f;own.offsets={{20,{0,0,.004f}}};cloth.morphs={own};if(authored)cloth.morphs.push_back(shape);
    auto outer=cloth;outer.id="outer/mesh";outer.instance=2;outer.conform_target="#cloth";
    d.catalog.targets={body,cloth,outer};d.formulas.graphs={graph(body),graph(cloth),graph(outer)};
    auto s=editor::initial_snapshot(d);s.values[0].morphs[0]=1;auto initial=scene;
    DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(s.values,{});const auto baseline=scene.meshes;
    s.values[1].disabled_deformation_regions={"Toes"};runtime.evaluate(s.values,{});const auto locked=scene.meshes;
    check(locked[1].positions!=baseline[1].positions&&locked[0].positions==baseline[0].positions,"形变开关未生效或污染宿主");
    check(locked[2].positions!=baseline[2].positions,"嵌套穿戴物没有收到修正");
    check(runtime.evaluate(s.values,{}).meshes.empty(),"重复求值发生漂移");
    auto reference=mesh.positions;reference[20].z+=.002f;
    for(const auto &f:mesh.triangles)if(f.polygon_group==2||f.polygon_group==4)for(int k=0;k<3;++k)check(std::abs(length(locked[1].positions[f.vertices[k]],locked[1].positions[f.vertices[(k+1)%3]])-length(reference[f.vertices[k]],reference[f.vertices[(k+1)%3]]))<2e-6,"自动 Morph 抑制破坏衣物自身适配");
    auto saved=editor::snapshot_json(d,s);auto restored=editor::initial_snapshot(d);editor::apply_snapshot_json(d,restored,saved);check(restored.values[1].disabled_deformation_regions==s.values[1].disabled_deformation_regions,"快照丢失部位开关");
    auto reopened=initial;DeformationRuntime fresh(reopened,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);fresh.evaluate(restored.values,{});check(reopened.meshes[1].positions==locked[1].positions,"重开后几何不一致");
    saved["objects"][cloth.id].erase("disabled_deformation_regions");editor::apply_snapshot_json(d,restored,saved);check(restored.values[1].disabled_deformation_regions.empty(),"旧文件默认关闭了部位形变");
    s.values[1].disabled_deformation_regions.clear();runtime.evaluate(s.values,{});for(size_t i=0;i<3;++i)check(scene.meshes[i].positions==baseline[i].positions,"重新开启没有精确恢复");
    s.values[1].disabled_deformation_regions={"Toes"};s.values[1].morphs[0]=1;runtime.evaluate(s.values,{});check(scene.meshes[1].positions!=locked[1].positions,"关闭时自身 Morph 更新未使缓存失效");
    for(uint32_t i=0;i<3;++i) {
      Skin skin;skin.instance=i;Joint root;root.id="root";Joint foot;foot.id=foot.name="lFoot";foot.parent=0;Joint toe;toe.id=toe.name="lToe";toe.parent=1;skin.joints={root,foot,toe};skin.initial.resize(3);skin.weights.resize(mesh.positions.size(),{{1,1}});
      for(size_t v=0;v<mesh.positions.size();++v)if(mesh.positions[v].x>=.06f){const double weight=v%2?.8:.4;skin.weights[v]={{1,1-weight},{2,weight}};}
      d.skeletons.skins.push_back(skin);d.formulas.graphs[i].skin=int(i);s.poses.push_back(skin.initial);
    }
    auto posed=initial;DeformationRuntime pose_runtime(posed,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);pose_runtime.defer_collision();pose_runtime.evaluate(s.values,s.poses);const auto unskinned=pose_runtime.skin_source(1);const auto rest=posed.meshes[1].positions;
    s.poses[0][0].general_scale=.2f;s.poses[0][1].rotation_degrees.x=35;s.poses[0][2].scale={2,.5f,1};s.poses[0][2].rotation_degrees.x=25;pose_runtime.evaluate(s.values,s.poses);
    check(posed.meshes[1].positions!=rest&&pose_runtime.skin_source(1)==unskinned,"关闭局部形变阻断姿势跟随或重复改变蒙皮前网格");
    check(pose_runtime.effective_poses()[1][0].general_scale==.2f&&pose_runtime.effective_poses()[1][1].rotation_degrees.x==35,"关闭部位后丢失角色缩放或抬脚姿势");
    for(uint32_t group:{2u,4u}){double low=1e30,high=0;for(const auto &f:mesh.triangles)if(f.polygon_group==group)for(int k=0;k<3;++k){const auto a=f.vertices[k],b=f.vertices[(k+1)%3];const double scale=length(posed.meshes[1].positions[a],posed.meshes[1].positions[b])/length(unskinned[a],unskinned[b]);low=std::min(low,scale);high=std::max(high,scale);}check(high-low<1e-5,"Toes 关闭后仍被脚趾局部缩放和不同权重拉坏");}
    auto safe=initial;DeformationRuntime safe_runtime(safe,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);safe_runtime.evaluate(s.values,s.poses);check(safe_runtime.collision_stats().evaluations>0,"关闭形变后未进行防穿插");check(safe_runtime.evaluate(s.values,s.poses).meshes.empty(),"防穿插重复求值发生漂移");
    const auto safe_positions=safe.meshes[1].positions;
    for(uint32_t group:{2u,4u}){double low=1e30,high=0;for(const auto &f:mesh.triangles)if(f.polygon_group==group)for(int k=0;k<3;++k){const auto a=f.vertices[k],b=f.vertices[(k+1)%3];if(mesh.positions[a].x<.1f||mesh.positions[b].x<.1f)continue;const double scale=length(safe_positions[a],safe_positions[b])/length(unskinned[a],unskinned[b]);low=std::min(low,scale);high=std::max(high,scale);}check(low<1e30&&high-low<1e-5,"防穿插重新把保形部位内部顶出褶皱");}
    s.values[1].disabled_deformation_regions.clear();safe_runtime.evaluate(s.values,s.poses);s.values[1].disabled_deformation_regions={"Toes"};safe_runtime.evaluate(s.values,s.poses);check(safe.meshes[1].positions==safe_positions,"动态启用防穿插与首次加载结果不一致");
  }
  std::cout<<"Garment regions / continuity / authored and projected morph / own fit / nested follow / restore: PASS\n";
}
static J positions(const std::vector<ir::Vec3> &points){J a=J::array();for(auto p:points)a.push_back({p.x,p.y,p.z});return a;}
static void actual(const std::filesystem::path &file,const std::filesystem::path &output,bool diagnose=false,size_t minimum=6,bool skirts=false) {
  const std::vector<std::filesystem::path> roots={L"H:/G1",L"H:/G3",L"C:/Users/Public/Documents/My DAZ 3D Library"};
  editor::RestoredScene restored;
  if(file.extension()==L".dufex")restored=editor::load_scene_extension(file,roots,1,[](const auto &s){std::cout<<s<<std::endl;});
  else {restored.document=std::make_shared<editor::Document>();auto &d=*restored.document;d.source_file=file;d.generation=1;d.loaded=daz::load(file,{roots,false});d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);restored.snapshot=editor::initial_snapshot(d);}
  auto &d=*restored.document;auto s=restored.snapshot;
  auto uncorrected=d.loaded.scene;DeformationRuntime preview(uncorrected,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);preview.defer_collision();preview.evaluate(s.values,s.poses);
  if(file.extension()==L".duf")editor::save_scene_extension(output/"source.dufex",d,s);
  auto scene=d.loaded.scene;DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(s.values,s.poses);
  std::cout<<"baseline ready\n"<<std::flush;const auto baseline=scene.meshes;const auto original=s;J report={{"source",file.generic_string()},{"cases",J::array()}};
  size_t tested=0;std::set<std::string> labels;
  std::vector<size_t> order(d.catalog.targets.size());std::iota(order.begin(),order.end(),0);
  std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return (d.catalog.targets[a].label=="Loafers")>(d.catalog.targets[b].label=="Loafers");});
  for(size_t t:order) {
    const auto &target=d.catalog.targets[t];const auto &instance=scene.instances[target.instance];const auto &mesh=d.loaded.scene.meshes[instance.mesh];
    const auto regions=deformation_regions(mesh);if(target.conform_target.empty()||!mesh.graft_vertex_pairs.empty()||instance.shell_source>=0||regions.empty())continue;
    const auto label=target.label;bool wanted=false;for(auto word:{"Pump","Loafer","Sneaker","Slipper","Socks","Stocking","Jeans","Skirt","Tshirt","Tank","Dress"})wanted|=label.find(word)!=std::string::npos;
    if(!wanted&&!target.dynamic_cloth)continue;
    if(diagnose&&(skirts?label!="Skirt"&&label!="Skirt (2)":label!="SCU Pumps"&&label!="RN Casual Slippers A"&&label!="Loafers"))continue;
    int skin=-1;for(size_t k=0;k<d.skeletons.skins.size();++k)if(d.skeletons.skins[k].instance==target.instance)skin=int(k);
    J item={{"id",target.id},{"label",label},{"vertices",mesh.positions.size()},{"regions",J::array()},{"trials",J::array()}};
    for(const auto &r:regions)item["regions"].push_back(r.id);
    J dump={{"label",label},{"base",positions(mesh.positions)},{"before",positions(baseline[instance.mesh].positions)},{"triangles",J::array()},{"groups",mesh.polygon_groups},{"transform",instance.transform.value}};
    dump["uncorrected"]=positions(uncorrected.meshes[instance.mesh].positions);
    if(skin>=0)dump["morphed"]=positions(preview.skin_source(size_t(skin)));
    dump["graft_target_vertices"]=mesh.graft_target_vertices;dump["graft_pairs"]=mesh.graft_vertex_pairs.size();dump["hidden_polygons"]=mesh.hidden_polygons;dump["conform_target"]=target.conform_target;
    dump["collision"]={{"enabled",target.smoothing.enabled},{"target",target.smoothing.collision_target},{"iterations",target.smoothing.collision_iterations},{"smoothing",target.smoothing.smoothing_iterations}};
    dump["dynamic_cloth"]=target.dynamic_cloth;dump["push_modifiers"]=J::array();for(const auto &p:target.push_modifiers)dump["push_modifiers"].push_back({{"offset_m",p.offset_m},{"post_smooth",p.post_smooth},{"weighted",!p.weights.empty()}});
    dump["morphs"]=J::array();for(size_t m=0;m<target.morphs.size();++m)if(runtime.effective()[t][m]!=0)dump["morphs"].push_back({{"id",target.morphs[m].id},{"value",runtime.effective()[t][m]},{"input",s.values[t].morphs[m]}});
    for(const auto &f:mesh.triangles)dump["triangles"].push_back({f.vertices[0],f.vertices[1],f.vertices[2],f.polygon_group});
    if(diagnose) {
      auto own=mesh.positions;for(size_t m=0;m<target.morphs.size();++m)if(runtime.effective()[t][m]!=0)for(const auto &v:target.morphs[m].data()){own[v.vertex].x+=v.delta.x*runtime.effective()[t][m];own[v.vertex].y+=v.delta.y*runtime.effective()[t][m];own[v.vertex].z+=v.delta.z*runtime.effective()[t][m];}dump["own"]=positions(own);
      for(const auto &link:runtime.conform_links())if(link.follower==t) {
        const auto &host=scene.instances[d.catalog.targets[link.source].instance];const auto &body=scene.meshes[host.mesh];
        dump["source_final"]=positions(body.positions);dump["source_transform"]=host.transform.value;dump["source_triangles"]=J::array();
        for(const auto &f:body.triangles)dump["source_triangles"].push_back({f.vertices[0],f.vertices[1],f.vertices[2],f.polygon_group});
        dump["source_base"]=positions(d.loaded.scene.meshes[scene.instances[d.catalog.targets[link.source].instance].mesh].positions);dump["surface"]=J::array();dump["source_morphs"]=J::array();
        for(const auto &b:link.surface)dump["surface"].push_back({{"vertices",b.vertices},{"bary",{b.barycentric.x,b.barycentric.y,b.barycentric.z}},{"offset",{b.offset_coordinates.x,b.offset_coordinates.y,b.offset_coordinates.z}},{"distance",b.distance}});
        for(auto m:link.projected_morphs)if(runtime.effective()[link.source][m]!=0){const auto &morph=d.catalog.targets[link.source].morphs[m];J item={{"id",morph.id},{"weight",runtime.effective()[link.source][m]},{"deltas",J::array()}};for(auto v:morph.data())item["deltas"].push_back({v.vertex,v.delta.x,v.delta.y,v.delta.z});dump["source_morphs"].push_back(std::move(item));}
      }
      std::ofstream(output/("model-"+std::to_string(t)+".json"))<<dump.dump();continue;
    }
    for(bool all:{false,true}) {
      auto &disabled=s.values[t].disabled_deformation_regions;disabled.clear();
      if(all)for(const auto &r:regions)disabled.insert(r.id);else disabled.insert(std::any_of(regions.begin(),regions.end(),[](const auto &r){return r.id=="Toes";})?"Toes":regions.front().id);
      runtime.evaluate(s.values,s.poses);const auto result=scene.meshes[instance.mesh].positions;
      double move=0,max_ratio=0;for(size_t v=0;v<result.size();++v){check(std::isfinite(result[v].x)&&std::isfinite(result[v].y)&&std::isfinite(result[v].z),"真实服装出现无效顶点");move=std::max(move,length(result[v],baseline[instance.mesh].positions[v]));}
      for(const auto &f:mesh.triangles)for(int k=0;k<3;++k){const auto a=f.vertices[k],b=f.vertices[(k+1)%3];const double before=length(baseline[instance.mesh].positions[a],baseline[instance.mesh].positions[b]);if(before>1e-7)max_ratio=std::max(max_ratio,length(result[a],result[b])/before);}
      check(runtime.evaluate(s.values,s.poses).meshes.empty(),"真实服装重复求值漂移");
      auto fresh_scene=d.loaded.scene;DeformationRuntime fresh(fresh_scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);fresh.evaluate(s.values,s.poses);check(fresh_scene.meshes[instance.mesh].positions==result,"真实模型切换与重新求值不一致");
      for(const auto &link:runtime.conform_links())if(link.follower==t)check(scene.meshes[scene.instances[d.catalog.targets[link.source].instance].mesh].positions==baseline[scene.instances[d.catalog.targets[link.source].instance].mesh].positions,"衣物开关修改了宿主");
      item["trials"].push_back({{"disabled",disabled},{"max_move_m",move},{"max_edge_ratio_to_before",max_ratio},{"repeat_stable",true},{"fresh_exact",true}});
      dump[all?"all_off":"region_off"]=positions(result);if(skin>=0)dump[all?"all_off_unskinned":"region_off_unskinned"]=positions(runtime.skin_source(size_t(skin)));
      s=original;runtime.evaluate(s.values,s.poses);check(scene.meshes[instance.mesh].positions==baseline[instance.mesh].positions,"真实服装重新开启不能精确复原");
    }
    std::ofstream(output/("model-"+std::to_string(t)+".json"))<<dump.dump();report["cases"].push_back(item);std::cout<<item.dump()<<std::endl;++tested;
    std::ofstream(output/"report.json")<<report.dump(2);
  }
  if(!diagnose)check(tested>=minimum,"真实服装覆盖不足");report["status"]="PASS";std::ofstream(output/"report.json")<<report.dump(2);
}
int wmain(int argc,wchar_t **argv){try{if(argc==4&&(std::wstring(argv[1])==L"--scene"||std::wstring(argv[1])==L"--diagnose"||std::wstring(argv[1])==L"--sample"||std::wstring(argv[1])==L"--fit-diagnose")){std::filesystem::create_directories(argv[3]);const bool skirts=std::wstring(argv[1])==L"--fit-diagnose";actual(argv[2],argv[3],std::wstring(argv[1])==L"--diagnose"||skirts,std::wstring(argv[1])==L"--sample"?1:6,skirts);}else unit();return 0;}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
