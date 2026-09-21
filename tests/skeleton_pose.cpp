#include "daz/skeleton.h"
#include "daz/pose.h"
#include "runtime/morph.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>

using namespace dfv;
using Json=nlohmann::json;
static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
template<class F> static void rejects(F f,const char *message) {bool rejected=false;try {f();} catch(const std::exception &) {rejected=true;}require(rejected,message);}
static bool near(ir::Vec3 a,ir::Vec3 b,double e=1e-6) {return std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)})<=e;}
static std::string utf8(const std::filesystem::path &p) {const auto u=p.generic_u8string();return {u.begin(),u.end()};}
static double distance(ir::Vec3 a,ir::Vec3 b) {return std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));}
static Json points(const std::vector<ir::Vec3> &positions) {Json result=Json::array();for(auto p:positions) result.push_back({p.x,p.y,p.z});return result;}
static runtime::Skin fixture() {
  runtime::Skin skin;runtime::Joint root;root.id="root";root.name="Figure";runtime::Joint bone;bone.id="oldId";bone.name="Bend";bone.aliases={"OldName"};bone.parent=0;bone.center_cm={100,0,0};
  skin.joints={root,bone};skin.initial.resize(2);skin.weights={{{1,1}}};return skin;
}
static void unit_tests() {
  auto skin=fixture();auto pose=skin.initial;const std::vector<ir::Vec3> base={{2,0,0}};
  require(near(runtime::deform(skin,pose,base)[0],base[0],0),"零姿势必须逐位恢复原顶点");
  pose[1].rotation_degrees.z=90;require(near(runtime::deform(skin,pose,base)[0],{1,0,1}),"骨骼枢轴或厘米 / Z 向上转换错误");
  pose[0].translation_cm={100,200,300};require(near(runtime::deform(skin,pose,base)[0],{2,-3,3}),"骨骼父节点平移错误");
  pose=skin.initial;pose[0].rotation_degrees.z=90;require(near(runtime::deform(skin,pose,base)[0],{0,0,2}),"子骨骼未继承父节点旋转与枢轴位移");
  pose=skin.initial;skin.joints[1].orientation_degrees={0,0,90};pose[1].rotation_degrees.x=90;
  require(near(runtime::deform(skin,pose,base)[0],{1,1,0}),"orientation 未正确包围局部骨骼旋转");
  skin=fixture();pose=skin.initial;pose[1].rotation_degrees={90,90,0};
  require(near(runtime::deform(skin,pose,{{1,0,1}})[0],{2,0,0}),"XYZ 欧拉旋转顺序错误");
  skin.joints[1].rotation_order="YXZ";require(near(runtime::deform(skin,pose,{{1,0,1}})[0],{1,-1,0}),"YXZ 欧拉旋转顺序错误");
  skin=fixture();skin.method=runtime::SkinMethod::linear;skin.joints[1].inherits_scale=false;pose=skin.initial;pose[0].scale={2,2,2};
  require(near(runtime::deform(skin,pose,base)[0],{3,0,0}),"非继承缩放必须保留骨骼起点的父变换");
  skin.joints[1].inherits_scale=true;require(near(runtime::deform(skin,pose,base)[0],{4,0,0}),"继承缩放错误");
  skin=fixture();skin.joints[1].parent=-1;skin.joints[1].center_cm={};skin.weights={{{0,2},{1,2}}};pose=skin.initial;pose[0].rotation_degrees.z=90;pose[1].rotation_degrees.z=-90;
  require(near(runtime::deform(skin,pose,{{1,0,0}})[0],{1,0,0}),"DQS 混合不应出现 LBS 体积塌陷");
  skin.method=runtime::SkinMethod::linear;require(near(runtime::deform(skin,pose,{{1,0,0}})[0],{0,0,0}),"LBS 权重归一化错误");
  skin.method=runtime::SkinMethod::dual_quaternion;pose[0].rotation_degrees.z=170;pose[1].rotation_degrees.z=-170;
  require(near(runtime::deform(skin,pose,{{1,0,0}})[0],{-1,0,0}),"DQS 未对齐四元数半球");
  auto invalid=pose;invalid[0].scale.x=0;rejects([&] {runtime::validate_pose(skin,invalid);},"零缩放不应应用");
  invalid=pose;invalid[0].rotation_degrees.x=std::numeric_limits<float>::quiet_NaN();rejects([&] {runtime::validate_pose(skin,invalid);},"NaN 姿势未拒绝");
  auto bad=skin;bad.joints[0].parent=1;rejects([&] {runtime::validate_pose(bad,pose);},"循环或乱序骨架未拒绝");
  bad=skin;bad.weights[0][0].joint=999;rejects([&] {runtime::deform(bad,pose,{{1,0,0}});},"越界权重未拒绝");
  skin=fixture();ir::Scene scene;ir::Mesh mesh;mesh.positions=base;scene.meshes={mesh,mesh};ir::Instance a,b;b.mesh=1;scene.instances={a,b};
  runtime::Target target;target.instance=0;runtime::Morph morph;morph.offsets={{0,{1,0,0}}};target.morphs={morph};auto other=target;other.instance=1;
  std::vector<runtime::Target> targets={target,other};std::vector<runtime::Skin> skins={skin,skin};skins[1].instance=1;
  runtime::MorphRuntime morphs(scene,targets);runtime::SkinningRuntime skinner(scene,skins);skinner.evaluate(morphs.evaluate());pose=skin.initial;pose[1].rotation_degrees.z=90;
  morphs.set_morph(0,0,1);skinner.set_pose(0,pose);auto changed=skinner.evaluate(morphs.evaluate());
  require(changed.meshes.size()==1&&near(scene.meshes[0].positions[0],{1,0,2}),"Morph 必须先于蒙皮求值且只发出一次 Delta");
  require(near(scene.meshes[1].positions[0],base[0],0),"姿势污染另一角色");
  const auto evaluations=skinner.stats().evaluations;require(!skinner.set_pose(0,pose)&&skinner.evaluate().meshes.empty()&&skinner.stats().evaluations==evaluations,"相同姿势产生多余求值");
  skinner.set_pose(0,skin.initial);skinner.evaluate();require(near(scene.meshes[0].positions[0],{3,0,0},0),"恢复姿势丢失当前 Morph");
  morphs.set_morph(0,0,0);skinner.evaluate(morphs.evaluate());require(near(scene.meshes[0].positions[0],base[0],0),"完整恢复产生累积误差");
  Json doc={{"asset_info",{{"type","preset_pose"}}},{"scene",{{"animations",Json::array({{{"url","name://@selection/B%65nd:?rotation/z/value"},{"keys",{{0,90}}}}})}}}};
  runtime::Properties values;values.morphs={0};auto applied=daz::apply_pose(daz::parse_pose(doc),skin,skin.initial,target,values);
  require(applied.report["fully_applied"]==true&&applied.joints[1].rotation_degrees.z==90,"name / URI 转义通道匹配错误");
  require(applied.joints[0].rotation_degrees.z==0,"姿势修改了未指定骨骼");
  auto scoped=target;scoped.morphs[0].channel_id="ControlID";scoped.morphs[0].channel_name="controlName";scoped.morphs[0].owner="root";
  auto alias=scoped.morphs[0];alias.owner="oldId";alias.kind="alias";alias.unsupported="alias";scoped.morphs.push_back(alias);
  auto control_doc=doc;control_doc["scene"]["animations"][0]["url"]="name://@selection#controlName:?value/value";control_doc["scene"]["animations"][0]["keys"]={{0,.5}};
  runtime::Properties scoped_values;scoped_values.morphs={0,0};auto control=daz::apply_pose(daz::parse_pose(control_doc),skin,skin.initial,scoped,scoped_values);
  require(control.report["fully_applied"]==true&&control.properties.morphs[0]==.5f&&control.properties.morphs[1]==0,"根节点控制器与子骨骼同名别名混淆");
  doc["scene"]["animations"][0]["url"]="id://@selection/oldId:?translation/x/value";
  require(daz::apply_pose(daz::parse_pose(doc),skin,skin.initial,target,values).joints[1].translation_cm.x==90,"id 通道匹配错误");
  doc["scene"]["animations"][0]["url"]="name://@selection/missing:?rotation/z/value";
  require(!daz::apply_pose(daz::parse_pose(doc),skin,skin.initial,target,values).report["fully_applied"].get<bool>(),"未知骨骼必须报告");
  doc["scene"]["animations"][0]["keys"].push_back({1,20});rejects([&] {daz::parse_pose(doc);},"多帧动画未拒绝");
  doc["scene"]["animations"][0]["keys"]={{0,1}};doc["scene"]["animations"][0]["url"]="name://@selection:?scale/x/value";
  doc["scene"]["animations"][0]["keys"]={{0,0}};rejects([&] {daz::apply_pose(daz::parse_pose(doc),skin,skin.initial,target,values);},"非法缩放姿势未整次拒绝");
  const auto folder=std::filesystem::temp_directory_path()/"dfv-skeleton-tests"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());std::filesystem::create_directories(folder);
  const auto dsf=folder/L"骨架.dsf",duf=folder/L"双角色.duf";
  Json asset={{"node_library",Json::array({{{"id","oldId"},{"name","Bend"},{"parent","#root"}},{{"id","root"},{"name","Figure"}}})},
    {"modifier_library",Json::array({{{"skin",{{"node","#root"},{"geometry","#geometry"},{"vertex_count",1},{"joints",Json::array({{{"node","#oldId"},{"node_weights",{{"count",1},{"values",{{0,1}}}}}}})}}}}})}};
  std::ofstream(dsf)<<asset.dump();
  Json saved={{"scene",{{"nodes",Json::array({{{"id","figure-a"}},{{"id","bone-a"},{"url","#oldId"},{"parent","#figure-a"},{"rotation",Json::array({{{"id","z"},{"current_value",90}}})}}})}}}};
  std::ofstream(duf)<<saved.dump();daz::LoadedScene input;input.scene.meshes={mesh,mesh};input.scene.instances={a,b};input.report["input"]=utf8(duf);
  input.objects={{0,"figure-a","A","","geometry",dsf,true},{1,"figure-b","B","","geometry",dsf,true}};
  auto catalog=daz::load_skeletons(input);require(catalog.skins.size()==2&&catalog.skins[0].initial[1].rotation_degrees.z==90&&catalog.skins[1].initial[1].rotation_degrees.z==0,"载入姿势跨 Figure 污染或骨架未拓扑排序");
  asset["modifier_library"][0]["skin"]["joints"][0]["node_weights"]["count"]=2;std::ofstream(dsf)<<asset.dump();rejects([&] {daz::load_skeletons(input);},"权重 count 不一致未拒绝");
  asset["modifier_library"][0]["skin"]["joints"][0]["node_weights"]["count"]=1;asset["node_library"][0]["parent"]="#missing";std::ofstream(dsf)<<asset.dump();rejects([&] {daz::load_skeletons(input);},"缺失父骨骼未拒绝");
}
static void actual(const std::filesystem::path &file,const std::filesystem::path &directory,const std::filesystem::path &output,const std::vector<std::filesystem::path> &roots) {
  auto loaded=daz::load(file,{roots,false});auto catalog=daz::load_skeletons(loaded);require(!catalog.skins.empty(),"未加载真实角色蒙皮");
  const auto &skin=catalog.skins[0];const auto mesh=loaded.scene.instances[skin.instance].mesh;const auto base=loaded.scene.meshes[mesh].positions;
  runtime::SkinningRuntime skinner(loaded.scene,catalog.skins);skinner.evaluate();runtime::Target target;target.instance=skin.instance;
  std::vector<std::filesystem::path> files;for(const auto &entry:std::filesystem::directory_iterator(directory)) if(entry.path().extension()==".duf") files.push_back(entry.path());std::sort(files.begin(),files.end());require(!files.empty(),"姿势目录为空");
  std::filesystem::create_directories(output);Json report={{"input",utf8(file)},{"skeleton",catalog.report},{"poses",Json::array()},{"scope","single-frame-body-skinning; ERC/JCM not evaluated"},{"golden","DAZ_STUDIO_NOT_RUN"}};
  for(const auto &path:files) {
    auto applied=daz::apply_pose(daz::read_pose(path),skin,skin.initial,target,{});
    for(const auto &c:applied.report["unapplied"]) require(!c.at("modifier").get<std::string>().empty(),"真实姿势存在未匹配骨骼通道");
    require(applied.report["applied_bone_channels"].get<int>()>100,"真实姿势匹配通道过少");
    skinner.set_pose(0,applied.joints);skinner.evaluate();const auto result=loaded.scene.meshes[mesh].positions;double maximum=0;size_t moved=0;ir::Bounds bounds;
    for(size_t i=0;i<base.size();++i) {const double d=distance(base[i],result[i]);require(std::isfinite(d),"蒙皮产生非有限顶点");maximum=std::max(maximum,d);if(d>1e-5) ++moved;bounds.add(result[i]);}
    require(moved>base.size()/2&&maximum>.1,"真实姿势未有效改变几何");
    const auto before=skinner.stats().evaluations;require(!skinner.set_pose(0,applied.joints)&&skinner.evaluate().meshes.empty()&&skinner.stats().evaluations==before,"重复姿势发生变形积累");
    const auto stem=path.stem().string();if(stem=="BH2 Basking"||stem=="BH2 Fetal Position on Pillow"||stem=="BH2 Stretch and Yawn") {
      Json data={{"pose",utf8(path)},{"dsf",utf8(loaded.objects[0].geometry_file)},{"source",points(base)},{"positions",points(result)}};std::ofstream(output/(path.stem().wstring()+L".mesh.json"))<<data.dump();
    }
    skinner.set_pose(0,skin.initial);skinner.evaluate();double restored=0;for(size_t i=0;i<base.size();++i) restored=std::max(restored,distance(base[i],loaded.scene.meshes[mesh].positions[i]));require(restored==0,"真实角色姿势恢复漂移");
    applied.report["moved_vertices"]=moved;applied.report["max_displacement_m"]=maximum;applied.report["restore_error_m"]=restored;applied.report["bounds_m"]={{bounds.minimum.x,bounds.minimum.y,bounds.minimum.z},{bounds.maximum.x,bounds.maximum.y,bounds.maximum.z}};
    report["poses"].push_back(applied.report);
  }
  report["status"]="PASS";report["count"]=files.size();report["skin_evaluations"]=skinner.stats().evaluations;std::ofstream(output/"batch-report.json")<<report.dump(2);
  std::cout<<"Actual pose batch: PASS, poses="<<files.size()<<", joints="<<skin.joints.size()<<", vertices="<<base.size()<<'\n';
}
int wmain(int argc,wchar_t **argv) {
  try {
    if(argc>1) {if(argc<4) throw std::runtime_error("用法：SkeletonPoseTest <角色.duf> <姿势目录> <输出目录> [内容库...] ");std::vector<std::filesystem::path> roots;for(int i=4;i<argc;++i) roots.emplace_back(argv[i]);actual(argv[1],argv[2],argv[3],roots);}
    else {unit_tests();std::cout<<"Skeleton / DQS / LBS / pose DUF / morph-before-skin / isolation / reset: PASS\n";}
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
