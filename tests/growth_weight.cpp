#include "editor/object_extension.h"
#include "editor/scene_extension.h"
#include "runtime/growth.h"
#include "runtime/measurement_units.h"
#include "runtime/decimal_float.h"
#include "daz/documents.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cmath>

using namespace dfv;using J=nlohmann::json;namespace fs=std::filesystem;
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static void near(double a,double b,double tolerance,const char *message){if(std::abs(a-b)>tolerance)throw std::runtime_error(std::string(message)+": "+std::to_string(a)+" / "+std::to_string(b));}
static ir::Mesh cube(){
  ir::Mesh m;m.id="cube";m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};m.polygon_groups={"lHand","other"};
  const std::vector<std::vector<uint32_t>> faces={{0,3,2,1},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7},{4,5,6,7}};
  for(size_t i=0;i<faces.size();++i){ir::Polygon p;p.vertices=faces[i];p.polygon_group=i==5?1:0;m.polygons.push_back(p);}
  return m;
}
static void rules(){
  using namespace runtime;
  near(decimal_derived_float(.90000004),.9,0,"float 派生缩放尾数未清理");near(decimal_derived_float(.1234567),.1234567,1e-8,"派生缩放丢失有效精度");
  require(measurement_height(999.99)=="999.99 cm"&&measurement_height(1000)=="10 m"&&measurement_height(999999)=="9999.99 m"&&measurement_height(1000000)=="10 km","身高单位边界");
  require(measurement_mass(.99999)=="999.99 g"&&measurement_mass(1)=="1 kg"&&measurement_mass(999.99)=="999.99 kg"&&measurement_mass(1000)=="1 t","重量单位边界");
  require(measurement_height(175,"2")=="350 cm"&&measurement_mass(70,"2")=="560 kg"&&measurement_mass(.0005)=="0.5 g","线性与立方倍率");
  require(measurement_mass(1.005)=="1.01 kg"&&measurement_mass(0)=="0 g","十进制舍入");
  require(measurement_height(175,"1e2000")=="1.75e1997 km"&&measurement_mass(70,"1e2000")=="7e5998 t","大倍率溢出");
  require(measurement_height(175,"1e-2000")=="1.75e-1998 cm"&&measurement_mass(70,"1e-2000")=="7e-5996 g","小倍率下溢");
  require(measurement_mass(1,"1.000000000000000000000000000000000000001")=="1 kg","高精度倍率");
  for(const auto *bad:{"0","-1","NaN","1e10001","1e","..1"}){bool rejected=false;try{measurement_scale(bad);}catch(...){rejected=true;}require(rejected,"非法倍率未拒绝");}
  const auto values=runtime::growth_values(10.5,.375);require(values.size()==19,"生长表数量错误");
  auto get=[&](const char *name){for(const auto &v:values)if(v.label==name)return v.value;throw std::runtime_error("missing table");};
  near(get("Head Propagating Scale"),-.09,1e-12,"小数年龄插值");near(get("Foot Propagating Scale"),.135,1e-12,"体格强度和年龄双线性插值");near(get("Body Size"),.015,1e-12,"体格强度过渡");
  for(const auto [strength,body]:std::vector<std::pair<double,double>>{{-.25,-.125},{1.25,.625}}){const auto outside=growth_values(20,strength);for(const auto &v:outside){require(std::isfinite(v.value),"范围外体格产生无效值");if(v.label=="Body Size")near(v.value,body,1e-12,"范围外体格没有沿边缘区间延伸");}}
  ObjectExtension unlimited;unlimited.age_step=25;unlimited.strength=-2;unlimited.sensitivity=200000;validate_extension(unlimited);
  const auto lo=runtime::calibrate_weight(17.28792285,141.26960385),hi=runtime::calibrate_weight(54.59738645,159.74123669);
  near(lo.kg,25,1e-8,"低段标定锚点");near(hi.kg,54.59738645*25/22.54333905,1e-8,"高段标定锚点");
  near(runtime::calibrate_weight(17.28792285*8,141.26960385*2).kg,200,1e-8,"等比放大遵循立方关系");
  require(runtime::calibrate_weight(30,150).branch=="过渡段","过渡标定未覆盖");
  bool rejected=false;try{runtime::growth_values(NAN,.5);}catch(...){rejected=true;}require(rejected,"非有限年龄未拒绝");
}
static void volume(){
  const auto m=cube();runtime::WeightTopology topology(m);ir::Transform transform;auto r=topology.measure(m.positions,transform,false,7850);
  near(r.liters,1000,1e-10,"立方体体积");near(r.kg,7850,1e-10,"密度质量换算");require(r.boundary_edges==0,"闭合拓扑识别");
  r=topology.measure(m.positions,transform,true);near(r.height_cm,100,1e-8,"标准身高");near(r.parts[0].liters,1000,1e-8,"部位边界封口");require(r.parts[0].boundary_edges==4,"部位开口边计数");near(r.parts[0].kg,r.kg,1e-8,"局部共用全身系数");require(r.parts.size()==13&&r.parts[1].triangles==0,"单侧部位数量或缺失面组错误");
  for(const auto &part:r.parts)require(part.label.find("左")==std::string::npos&&part.label.find("右")==std::string::npos,"单侧名称残留左右字样");
  auto paired=m;paired.polygon_groups.push_back("rHand");for(auto p:m.positions)paired.positions.push_back({p.x*2+3,p.y*2,p.z*2});
  for(auto face:m.polygons){for(auto &i:face.vertices)i+=uint32_t(m.positions.size());face.polygon_group=2;paired.polygons.push_back(face);}
  const auto both=runtime::WeightTopology(paired).measure(paired.positions,{},true);near(both.liters,9000,1e-8,"全身体积漏掉另一侧");near(both.parts[0].liters,1000,1e-8,"单侧统计混入另一侧或取了平均值");near(both.parts[0].kg,both.kg/9,1e-8,"单侧重量误乘二");
  transform.value={-2,0,0,10000000,0,3,0,-10000000,0,0,4,9999999};r=topology.measure(m.positions,transform,true);
  near(r.liters,24000,1e-7,"镜像非等比缩放及远原点平移");near(r.parts[0].liters,24000,1e-7,"镜像封口");near(r.height_cm,400,1e-7,"非等比身高");
  auto open=m;open.polygons.pop_back();runtime::WeightTopology opened(open);require(opened.measure(open.positions,{},false).boundary_edges==4,"开放网格未诊断");
}
static editor::Document growth_document(){
  editor::Document d;d.generation=1;d.loaded.scene.meshes={cube()};ir::Instance instance;instance.id="figure/mesh";d.loaded.scene.instances={instance};
  runtime::Target t;t.id=instance.id;t.instance=0;for(const auto &v:runtime::growth_values(3,.5)){runtime::Morph m;m.id=m.label=v.label;m.evaluable=true;m.minimum=-2;m.maximum=2;t.morphs.push_back(m);}d.catalog.targets={t};
  runtime::Skin skin;skin.id="figure";for(const char *name:{"hip","head","lHand","rHand","lFoot","rFoot"}){runtime::Joint j;j.id=name;skin.joints.push_back(j);}skin.initial.resize(skin.joints.size());d.skeletons.skins={skin};daz::AssetObject actor;actor.figure=true;actor.content_type="Actor/Character";d.loaded.objects={actor};
  runtime::FormulaGraph graph;graph.skin=0;for(size_t i=0;i<t.morphs.size();++i){runtime::Channel c;c.binding.index=uint32_t(i);graph.morph_channels.push_back(int(graph.channels.size()));graph.channels.push_back(c);}
  runtime::Channel root;root.binding={runtime::Property::general_scale,0,0};root.initial=1;graph.channels.push_back(root);
  // 模拟旧 DAZ 通道中的 -1% ERC，用实际缩放校准验证不再需要脚本补偿。
  runtime::Expression e;e.owner=0;e.output=uint32_t(graph.channels.size()-1);e.code={{runtime::Op::constant,-.01}};graph.expressions.push_back(e);graph.prepare();d.formulas.graphs={graph};return d;
}
static void growth(){
  {auto d=growth_document();for(auto &j:d.skeletons.skins[0].joints){if(j.id=="lHand")j.id="l_hand";if(j.id=="rHand")j.id="r_hand";if(j.id=="lFoot")j.id="l_foot";if(j.id=="rFoot")j.id="r_foot";}require(editor::growth_character(d,0),"Genesis 9 的骨骼命名未被识别");}
  {auto d=growth_document();require(editor::growth_character(d,0),"角色元数据未被识别");d.loaded.objects[0].content_type="Prop";require(!editor::growth_character(d,0),"带骨架道具被误判角色");d.loaded.objects[0].content_type="Actor";d.skeletons.skins[0].joints.resize(2);d.skeletons.skins[0].joints[0].id="Hull";d.skeletons.skins[0].joints[1].id="Oars";require(!editor::growth_character(d,0),"Rowboat 的 Actor 标签覆盖了道具骨架");auto s=editor::initial_snapshot(d);s.values[0].extension.kind=runtime::ExtensionKind::growth;editor::normalize_object_extensions(d,s);require(s.values[0].extension.kind==runtime::ExtensionKind::density,"旧道具生长模块未迁移为密度");d.loaded.objects[0].content_type.clear();require(!editor::growth_character(d,0),"单链骨架被误判角色");d.loaded.objects[0].content_type="Follower/Hair";require(!editor::growth_character(d,0),"头发被误判角色");}
  auto d=growth_document();auto s=editor::initial_snapshot(d);s.values[0].extension.kind=runtime::ExtensionKind::growth;s.values[0].transform.translation_cm.y=23;
  {auto target=d.catalog.targets[0];auto &head=*std::find_if(target.morphs.begin(),target.morphs.end(),[](const auto &m){return m.label=="Head Propagating Scale";});head.scene_channel=true;head.id="native-head";head.initial=-.25f;
    auto duplicate=head;duplicate.id="vendor-head";duplicate.scene_channel=false;duplicate.initial=0;target.morphs.insert(target.morphs.begin(),duplicate);
    runtime::Properties values;values.extension.kind=runtime::ExtensionKind::growth;values.extension.age=4;for(const auto &m:target.morphs)values.morphs.push_back(m.initial);
    runtime::apply_growth(target,values);require(values.morphs.front()==0,"生长误写同名商业控制器，导致头部缩放重复叠加");
    const auto found=std::find_if(target.morphs.begin(),target.morphs.end(),[](const auto &m){return m.id=="native-head";});near(values.morphs[size_t(found-target.morphs.begin())],-.25,0,"未使用 DUF 实际引用的头部控制器");}
  editor::edit_growth(d,s,0,s.values[0].extension);const auto initial=s.values[0];
  {auto test=s;test.values[0].transform.general_scale=.9/.99;auto next=test.values[0].extension;next.sensitivity=2;next.age+=1;editor::edit_growth(d,test,0,next);near(runtime::decimal_float(.99)*test.values[0].transform.general_scale*100,92,1e-12,"90 + 2 未精确得到 92");}
  for(int i=0;i<100;++i){auto v=s.values[0].extension;v.age+=.125;editor::edit_growth(d,s,0,v);v.age-=.125;editor::edit_growth(d,s,0,v);}
  near(s.values[0].transform.general_scale,initial.transform.general_scale,1e-6,"年龄加减累计缩放漂移");near(s.values[0].transform.translation_cm.y,23,1e-5,"年龄加减 Y 位置漂移");require(s.values[0].morphs==initial.morphs,"年龄往返 Morph 未恢复");
  auto v=s.values[0].extension;v.age=20;editor::edit_growth(d,s,0,v);const auto before=s.values[0].transform.general_scale;editor::edit_growth(d,s,0,v);near(s.values[0].transform.general_scale,before,1e-7,"年龄上边界重复缩放");
  auto bad=v;bad.sensitivity=-100000;bad.age=19;bool rejected=false;try{editor::edit_growth(d,s,0,bad,false,10);}catch(...){rejected=true;}require(rejected&&s.values[0].extension==v,"非法缩放非原子");
}
static fs::path write_source(const fs::path &folder){
  const auto mesh=cube();J vertices=J::array(),polygons=J::array();for(auto v:mesh.positions)vertices.push_back({v.x*100,v.z*100,-v.y*100});for(auto &f:mesh.polygons)polygons.push_back({0,0,f.vertices[0],f.vertices[1],f.vertices[2],f.vertices[3]});
  J data={{"geometry_library",J::array({{{"id","mesh"},{"vertices",{{"count",8},{"values",vertices}}},{"polygon_material_groups",{{"count",1},{"values",{"Surface"}}}},{"polylist",{{"count",6},{"values",polygons}}}}})},{"node_library",J::array({{{"id","object"},{"type","node"}}})},{"scene",{{"nodes",J::array()}}}};
  for(const auto &id:{"a","b"})data["scene"]["nodes"].push_back({{"id",id},{"url","#object"},{"label","同名对象"},{"geometries",J::array({{{"id",std::string(id)+"Shape"},{"url","#mesh"}}})},{"extra",J::array({{{"type","studio_node_channels"},{"channels",J::array({{{"channel",{{"id","Age"},{"value",18}}}},{{"channel",{{"id","Density"},{"value",99000}}}}})}}})}});
  data["material_library"]=J::array({{{"id","mat"},{"diffuse",{{"channel",{{"id","diffuse"},{"value",{.5,.5,.5}}}}}}}});
  data["geometry_library"][0]["default_uv_set"]="#uv";data["uv_set_library"]=J::array({{{"id","uv"},{"vertex_count",8},{"uvs",{{"count",8},{"values",{{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1}}}}}}});
  data["scene"]["nodes"][0]["general_scale"]={{"id","general_scale"},{"current_value",.5}};
  data["scene"]["materials"]=J::array();for(const auto &id:{"a","b"})data["scene"]["materials"].push_back({{"id",std::string(id)+"Material"},{"url","#mat"},{"geometry",std::string("#")+id+"Shape"},{"groups",{"Surface"}}});
  auto file=folder/"source.duf";std::ofstream(file)<<data.dump();return file;
}
static editor::Document load(const fs::path &file){editor::Document d;d.source_file=file;d.generation=1;std::vector<fs::path> roots={file.parent_path(),"H:/g1","H:/g3","C:/Users/Public/Documents/My DAZ 3D Library","C:/Users/xatia/Documents/DAZ 3D/Studio/My Library"};d.loaded=daz::load(file,{roots,false});d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);return d;}
static void serialization(const fs::path &folder){
  const auto file=write_source(folder);const auto original=daz::read_document_file(file);auto d=load(file);
  ir::add_studio(d.loaded.scene);d.operations.push_back({{"op","studio"}});require(d.loaded.scene.instances.back().id=="preview-floor","未生成预览地面");
  auto s=editor::initial_snapshot(d);require(s.values[0].extension.kind==runtime::ExtensionKind::density&&s.values[0].extension.density==99000&&s.values[0].extension.age==3,"原生密度未继承，或道具误继承角色年龄");
  s.values[0].extension.kind=runtime::ExtensionKind::density;s.values[0].extension.density=7800;s.values[1].extension.kind=runtime::ExtensionKind::density;s.values[1].extension.density=2300;s.values[0].transform.translation_cm={12,23,34};s.values[1].visible=false;s.values[0].ground_alignment_ratio=.03;
  const auto isolated=editor::measure_object(d,s,0);const auto &target=d.catalog.targets[0];const auto loaded_world=d.loaded.scene.instances.at(target.instance).transform;
  const auto actual=runtime::parameter_transform(s.values[0].transform,target,loaded_world)*loaded_world;
  const auto supplied=editor::measure_object(d,s,0,actual);near(supplied.kg,isolated.kg,1e-6,"当前世界变换重复计算了保存缩放");
  runtime::TransformValues parent;parent.general_scale=2;parent.rotation_degrees={13,29,53};parent.translation_cm={10000,20000,30000};const auto inherited=editor::measure_object(d,s,0,runtime::make_transform(parent)*actual);near(inherited.kg,isolated.kg*8,.01,"父级世界缩放未正确计入重量");
  {
    editor::MeasurementService worker;auto document=std::make_shared<const editor::Document>(d);auto posed=std::make_shared<std::vector<ir::Vec3>>(d.loaded.scene.meshes.at(d.loaded.scene.instances[target.instance].mesh).positions);for(auto &v:*posed){v.x*=2;v.y*=2;v.z*=2;}
    worker.request(1,document,s,0,actual,posed);auto density=s;density.values[0].extension.density*=.5;worker.request(2,document,density,0,actual,posed);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);editor::MeasurementService::Result result;
    do{result=worker.result();if(result.serial==2)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}while(std::chrono::steady_clock::now()<deadline);
    require(result.serial==2&&result.error.empty(),"后台测量未返回最新输入");near(result.weight.kg,isolated.kg*4,.01,"普通物体没有使用当前形变网格或新密度");
    worker.request(3,{}, {},0);
  }
  s.values[0].favorites.emplace();s.values[1].favorites.emplace();s.control_favorites.emplace();
  s.values[0].favorites->nodes[""]["transform/general_scale"]=false;s.values[1].favorites->nodes[""]["transform/general_scale"]=true;
  s.values[0].favorites->nodes["head"]["joint/3"]=true;s.values[0].favorites->nodes["head"]["joint/4"]=false;
  s.control_favorites->nodes["light/"+s.lights[0].id]["light/0"]=true;
  auto saved=editor::snapshot_json(d,s);auto bad=saved;bad["objects"][d.catalog.targets[1].id]["extension"]["density"]=-1;bool rejected=false;try{editor::apply_snapshot_json(d,s,bad);}catch(...){rejected=true;}require(rejected&&editor::snapshot_json(d,s)==saved,"失败覆盖留下部分修改");
  auto legacy=saved;legacy.erase("control_favorites");for(auto &object:legacy["objects"])object.erase("favorites");auto old=editor::initial_snapshot(d);editor::apply_snapshot_json(d,old,legacy);
  require(!old.control_favorites&&!old.values[0].favorites,"旧版扩展未保留收藏迁移入口");
  bad=saved;bad["objects"][d.catalog.targets[1].id]["favorites"]["nodes"][""]["transform/general_scale"]="invalid";rejected=false;try{editor::apply_snapshot_json(d,s,bad);}catch(...){rejected=true;}require(rejected&&editor::snapshot_json(d,s)==saved,"错误收藏格式产生部分覆盖");
  const auto sidecar=editor::extension_path(file);editor::save_scene_extension(sidecar,d,s);auto restored=editor::load_scene_extension(sidecar,{folder},77);require(editor::snapshot_json(*restored.document,restored.snapshot)==saved,"保存重开数据不一致");require(daz::read_document_file(file)==original,"修改了源 DUF");
  require(restored.document->loaded.scene.instances.size()==d.loaded.scene.instances.size()&&restored.document->loaded.scene.meshes.size()==d.loaded.scene.meshes.size(),"保存重开遗失预览地面");
  const auto &floor=d.loaded.scene.meshes.back();const auto &restored_floor=restored.document->loaded.scene.meshes.back();require(floor.id==restored_floor.id&&floor.positions.size()==restored_floor.positions.size(),"预览地面身份未恢复");
  for(size_t i=0;i<floor.positions.size();++i){near(floor.positions[i].x,restored_floor.positions[i].x,0,"地面 X 未恢复");near(floor.positions[i].y,restored_floor.positions[i].y,0,"地面 Y 未恢复");near(floor.positions[i].z,restored_floor.positions[i].z,0,"地面 Z 未恢复");}
  auto second=load(file);d.generation=2;const auto first=d.catalog.targets.size();editor::append_document(d,std::move(second));auto merged=editor::initial_snapshot(d);merged.values[first].favorites=s.values[0].favorites;merged.values[first+1].favorites=s.values[1].favorites;merged.values[first].extension.kind=runtime::ExtensionKind::density;merged.values[first].extension.density=4567;editor::remove_target(d,merged,0);
  editor::save_scene_extension(sidecar,d,merged);restored=editor::load_scene_extension(sidecar,{folder},88);require(editor::snapshot_json(*restored.document,restored.snapshot)==editor::snapshot_json(d,merged),"追加删除的身份或编辑值未恢复");
  const auto prior=daz::read_document_file(sidecar);auto invalid=merged;invalid.values[0].extension.density=-1;rejected=false;try{editor::save_scene_extension(sidecar,d,invalid);}catch(...){rejected=true;}require(rejected&&daz::read_document_file(sidecar)==prior,"失败保存损坏已有文件");
}
int main(int argc,char **argv){
  try{rules();volume();growth();const auto folder=fs::temp_directory_path()/("dfv-growth-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder);serialization(folder);fs::remove_all(folder);
    if(argc>2&&std::string(argv[2])=="--classify"){
      const auto scene=editor::load_scene_extension(fs::u8path(argv[1]),{"H:/g1","H:/g3"},1);J report=J::array();
      for(size_t t=0;t<scene.document->catalog.targets.size();++t){const auto &target=scene.document->catalog.targets[t];if(target.label.find("Rowboat")!=std::string::npos){require(!editor::growth_character(*scene.document,t),"真实 Rowboat 被误判为角色");report.push_back({{"id",target.id},{"label",target.label},{"character",false},{"extension_kind",int(scene.snapshot.values[t].extension.kind)}});}}
      require(!report.empty(),"场景中未找到 Rowboat");std::cout<<report.dump(2)<<'\n';return 0;
    }
    if(argc>1){const auto file=fs::u8path(argv[1]);auto d=load(file);auto s=editor::initial_snapshot(d);J report=J::array();
      for(size_t t=0;t<d.catalog.targets.size();++t){const bool character=editor::growth_character(d,t);if(!character)continue;s.values[t].extension.kind=runtime::ExtensionKind::growth;const auto start=std::chrono::steady_clock::now();auto missing=editor::edit_growth(d,s,t,s.values[t].extension);const auto measured=editor::measure_object(d,s,t);report.push_back({{"id",d.catalog.targets[t].id},{"height_cm",measured.height_cm},{"kg",measured.kg},{"liters",measured.liters},{"missing",missing},{"notes",measured.notes},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}});}
      std::cout<<report.dump(2)<<'\n';
    }
    std::cout<<"PASS growth, volume, calibration, isolation, atomic overlay, replay\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
