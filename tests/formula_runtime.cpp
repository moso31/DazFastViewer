#include "daz/morphs.h"
#include "daz/skeleton.h"
#include "daz/pose.h"
#include "runtime/deformation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>

using namespace dfv;
using namespace dfv::runtime;
using Json=nlohmann::json;
static void require(bool p,const std::string &message) {if(!p) throw std::runtime_error(message);}
template<class F> static void rejects(F f,const char *message) {bool bad=false;try {f();} catch(const std::exception &) {bad=true;}require(bad,message);}
static std::string utf8(const std::filesystem::path &p) {const auto u=p.generic_u8string();return {u.begin(),u.end()};}
static double difference(const std::vector<ir::Vec3> &a,const std::vector<ir::Vec3> &b) {double result=0;for(size_t i=0;i<a.size();++i) result=std::max(result,std::sqrt(std::pow(a[i].x-b[i].x,2)+std::pow(a[i].y-b[i].y,2)+std::pow(a[i].z-b[i].z,2)));return result;}
static void unit() {
  FormulaGraph g;g.channels.resize(4);g.channels[1].initial=.5;g.channels[2].initial=3;Expression sum;sum.output=2;sum.linear_input=0;sum.coefficient=2;Expression product;product.output=2;product.linear_input=1;product.multiply=true;g.expressions={sum,product};g.prepare();
  FormulaRuntime r(g);r.set(0,1);r.evaluate();require(r.values()[2]==2.5,"sum / mult 阶段或基础编辑值合并错误");
  auto count=r.stats().expressions;r.set(3,10);r.evaluate();require(count==r.stats().expressions,"无关参数触发公式重算");r.set(0,1);r.evaluate();require(count==r.stats().expressions,"相同输入重复求值");
  r.set(0,2);r.evaluate();require(r.values()[2]==3.5&&r.stats().expressions==count+1,"公式没有按脏依赖求值");
  g.channels[2].clamped=true;g.channels[2].maximum=2;FormulaRuntime clamped(g);clamped.set(0,100);clamped.evaluate();require(clamped.values()[2]==2,"最终通道没有 clamp");
  clamped.set(0,1);clamped.set(2,-2);clamped.evaluate();require(clamped.values()[2]==0,"ERC 抵消值在公式合并前被错误限幅");
  auto cycle=g;Expression reverse;reverse.output=0;reverse.linear_input=2;cycle.expressions.push_back(reverse);cycle.prepare();require(!cycle.channels[0].error.empty()&&!cycle.channels[2].error.empty()&&cycle.channels[1].error.empty(),"循环依赖没有局部隔离");
  daz::FormulaSource source;Json formulas=Json::parse(R"([{"output":"out","operations":[{"op":"push","url":"in"},{"op":"push","val":2},{"op":"add"},{"op":"push","val":3},{"op":"sub"},{"op":"push","val":2},{"op":"div"},{"op":"neg"}]}])");
  daz::append_formulas(source,0,formulas,[](const auto &a) {return a;});std::vector<double> values(source.symbols.size());values[source.interned.at("in")]=5;
  require(evaluate_expression(source.expressions[0],values)==-2,"RPN 非交换运算顺序错误");
  formulas[0]["operations"]=Json::parse(R"([{"op":"push","url":"in"},{"op":"push","val":[0,0,0,0,0]},{"op":"push","val":[1,1,0,0,0]},{"op":"push","val":2},{"op":"spline_tcb"}])");
  source={};daz::append_formulas(source,0,formulas,[](const auto &a) {return a;});values.assign(source.symbols.size(),0);values[source.interned.at("in")]=.5;require(std::abs(evaluate_expression(source.expressions[0],values)-.5)<1e-12,"TCB 双节点曲线错误");
  const Knot knots[]={{0,0},{1,1},{2,0}};require(std::abs(sample_spline(Op::spline_tcb,knots,3,.5)-.625)<1e-12,"TCB 切线错误");require(sample_spline(Op::spline_linear,knots,3,.5)==.5&&sample_spline(Op::spline_constant,knots,3,.5)==0,"线性 / 常量样条错误");
  require(sample_spline(Op::spline_tcb,knots,3,-1)==0&&sample_spline(Op::spline_tcb,knots,3,5)==0,"样条边界错误");
  formulas[0]["operations"]=Json::parse(R"([{"op":"mult"}])");rejects([&] {daz::append_formulas(source,0,formulas,[](const auto &a) {return a;});},"非法公式栈未拒绝");
  Skin skin;Joint root;Joint bone;bone.parent=0;bone.center_cm={100,0,0};skin.joints={root,bone};skin.initial.resize(2);skin.weights={{{1,1}}};auto pose=skin.initial;pose[1].scale.x=2;
  auto vertices=deform(skin,pose,{{2,0,0}});require(difference(vertices,{{3,0,0}})<1e-6,"两阶段 DQS 缩放错误");pose[1].rotation_degrees.z=90;require(difference(deform(skin,pose,{{2,0,0}}),{{1,0,2}})<1e-6,"DQS 缩放后旋转错误");
  pose=skin.initial;pose[1].center_offset_cm.x=100;pose[1].rotation_degrees.z=90;require(difference(deform(skin,pose,{{3,0,0}}),{{2,0,1}})<1e-6,"Morph 驱动骨骼中心未参与绑定矩阵");
  pose[1].rotation_degrees={};require(difference(deform(skin,pose,{{3,0,0}}),{{3,0,0}})==0,"零姿势的骨骼中心调整不应重复移动基础网格");

  // 两个实例分别绑定公式；节点 mult 必须加入图，拒绝编辑后保留最后有效状态。
  ir::Scene scene;ir::Mesh mesh;mesh.positions={{1,0,0}};scene.meshes={mesh,mesh};ir::Instance a,b;b.mesh=1;scene.instances={a,b};
  Morph control;control.kind="control";control.channel_id="shape";control.minimum=-2;control.maximum=2;control.offsets={{0,{.1f,0,0}}};
  Target first;first.morphs={control};Target second=first;second.instance=1;
  daz::MorphCatalog catalog;catalog.targets={first,second};catalog.formulas.resize(2);catalog.report={{"targets",Json::array({{{"morphs",Json::array({Json::object()})}},{{"morphs",Json::array({Json::object()})}}})}};
  auto expression=Json::parse(R"([{"output":"$node/root?scale/x","operations":[{"op":"push","url":"$local/#shape?value"}]}])");
  for(auto &s:catalog.formulas) daz::append_formulas(s,0,expression,[](const auto &uri) {return uri;});
  skin.joints[0].id="root";skin.joints[1].id="child";skin.joints[1].inherits_scale=false;skin.joints[1].center_cm={};
  daz::SkinCatalog bindings;bindings.skins={skin,skin};bindings.skins[1].instance=1;
  auto node=Json::parse(R"([{"output":"child:#child?scale/x","stage":"mult","operations":[{"op":"push","url":"root:#root?scale/x"}]}])");bindings.node_formulas={node,node};
  const auto compiled=daz::enable_formulas(catalog,bindings);require(compiled.report["targets"][0]["node_formula_failures"].empty(),"节点公式未解析");
  DeformationRuntime pipeline(scene,catalog.targets,bindings.skins,compiled.graphs);std::vector<Properties> inputs(2);for(auto &p:inputs) p.morphs={0};std::vector<std::vector<JointPose>> snapshots(2,skin.initial);
  pipeline.evaluate(inputs,snapshots);inputs[0].morphs[0]=1;pipeline.evaluate(inputs,snapshots);
  require(pipeline.effective_poses()[0][1].scale.x==2&&pipeline.effective_poses()[1][1].scale.x==1,"节点缩放公式遗漏或跨角色污染");
  require(std::abs(scene.meshes[0].positions[0].x-2.2f)<1e-6&&scene.meshes[1].positions[0].x==1,"Morph 与蒙皮顺序或实例隔离错误");
  const auto valid=scene.meshes[0].positions;inputs[0].morphs[0]=-1;rejects([&] {pipeline.evaluate(inputs,snapshots);},"非法 ERC 零缩放没有拒绝");
  require(difference(valid,scene.meshes[0].positions)==0&&pipeline.effective()[0][0]==1,"失败编辑破坏上一有效状态");
  inputs[0].morphs[0]=1;require(pipeline.evaluate(inputs,snapshots).meshes.empty(),"失败后公式输入未正确回滚");
  inputs[0].transform.scale.x=0;rejects([&] {pipeline.evaluate(inputs,snapshots);},"非法对象缩放没有拒绝");inputs[0].transform.scale.x=1;
  inputs[0].morphs[0]=0;pipeline.evaluate(inputs,snapshots);require(scene.meshes[0].positions[0].x==1,"失败后继续编辑或恢复产生漂移");
  // Figure 的保存缩放已在实例矩阵中，ERC 的加法仍须使用真实通道基值。
  scene.meshes={mesh};scene.instances={a};scene.instances[0].transform.value[0]=scene.instances[0].transform.value[5]=scene.instances[0].transform.value[10]=.635f;
  skin.joints={root};skin.joints[0].id="root";skin.initial.resize(1);skin.weights={{{0,1}}};skin.root_general_scale=.635f;
  control.initial=1;control.offsets.clear();first.morphs={control};catalog={};catalog.targets={first};catalog.formulas.resize(1);catalog.report={{"targets",{{{"morphs",{Json::object()}}}}}};
  expression=Json::parse(R"([{"output":"$node/root?scale/general","operations":[{"op":"push","url":"$local/#shape?value"},{"op":"push","val":-0.01},{"op":"mult"}]}])");
  // 缺失的附件输出必须有诊断，但不能抹掉上面的有效公式。
  auto attachment=expression[0];attachment["output"]="$missing/eyelash?value";expression.push_back(attachment);
  daz::append_formulas(catalog.formulas[0],0,expression,[](const auto &uri) {return uri;});bindings={};bindings.skins={skin};
  const auto scaled_graph=daz::enable_formulas(catalog,bindings);require(catalog.targets[0].morphs[0].evaluable&&!catalog.targets[0].morphs[0].limitation.empty(),"附件缺失错误禁用了人物有效驱动，或没有记录限制");
  require(scaled_graph.report["targets"][0]["node_formula_failures"].empty()&&scaled_graph.report["targets"][0]["modifier_formula_failures"].size()==1,"Morph 诊断被错误记录为骨架节点公式失败");
  DeformationRuntime scaled(scene,catalog.targets,bindings.skins,scaled_graph.graphs);inputs.resize(1);inputs[0].morphs={1};snapshots={skin.initial};scaled.evaluate(inputs,snapshots);
  require(std::abs(scene.instances[0].transform.point(scene.meshes[0].positions[0]).x-.625f)<1e-6f,"非单位 Figure 缩放被 ERC 按单位基值重复相乘");
}
static size_t parameter(const Target &target,const std::string &label,bool alias=false) {
  for(size_t i=0;i<target.morphs.size();++i) if((target.morphs[i].label==label||target.morphs[i].channel_id==label)&&(target.morphs[i].kind=="alias")==alias) return i;
  return target.morphs.size();
}
static Json vec(ir::Vec3 p) {return {p.x,p.y,p.z};}
static void export_reference(const std::filesystem::path &path,const daz::LoadedScene &loaded,const Target &target,const Skin &skin,const DeformationRuntime &runtime,const Morph *edited=nullptr,float input=0) {
  Json out={{"dsf",utf8(loaded.objects[0].geometry_file)},{"morphs",Json::array()},{"joints",Json::array()},{"positions",Json::array()}};
  if(edited) out["edited_parameter"]={{"id",edited->channel_id},{"source",edited->source},{"value",input}};
  for(size_t m=0;m<target.morphs.size();++m) if(!target.morphs[m].offsets.empty()&&runtime.effective()[0][m]!=0) out["morphs"].push_back({{"source",target.morphs[m].source},{"id",target.morphs[m].channel_id},{"weight",runtime.effective()[0][m]}});
  for(size_t j=0;j<skin.joints.size();++j) {const auto &p=runtime.effective_poses()[0][j];out["joints"].push_back({{"id",skin.joints[j].id},{"translation",vec(p.translation_cm)},{"rotation",vec(p.rotation_degrees)},{"scale",vec(p.scale)},
    {"general_scale",p.general_scale},{"center_offset",vec(p.center_offset_cm)},{"end_offset",vec(p.end_offset_cm)},{"orientation_offset",vec(p.orientation_offset_degrees)}});}
  const auto mesh=loaded.scene.instances[target.instance].mesh;for(const auto p:loaded.scene.meshes[mesh].positions) out["positions"].push_back(vec(p));std::ofstream(path)<<out.dump();
}
static void actual(const std::filesystem::path &file,const std::filesystem::path &output,const std::filesystem::path &pose_folder,const std::vector<std::filesystem::path> &roots,const std::filesystem::path &baseline) {
  std::filesystem::create_directories(output);auto loaded=daz::load(file,{roots,false});auto catalog=daz::discover_morphs(loaded,roots);auto skins=daz::load_skeletons(loaded);auto formulas=daz::enable_formulas(catalog,skins);
  std::ofstream(output/"formula-report.json")<<formulas.report.dump(2);std::ofstream(output/"morph-catalog.json")<<catalog.report.dump(2);
  const auto &target=catalog.targets.at(0);const auto &skin=skins.skins.at(0);std::vector<Properties> values;
  require(formulas.report["targets"][0]["node_formulas"].get<size_t>()>0&&formulas.report["targets"][0]["node_formula_failures"].empty(),"基础骨架节点公式没有完整绑定");
  for(const auto &t:catalog.targets) {Properties p;for(const auto &m:t.morphs) p.morphs.push_back(m.evaluable?m.initial:0);sync_aliases(t,p);values.push_back(p);}
  std::vector<std::vector<JointPose>> poses;for(const auto &s:skins.skins) poses.push_back(s.initial);
  DeformationRuntime runtime(loaded.scene,catalog.targets,skins.skins,formulas.graphs);runtime.evaluate(values,poses);const auto original=values;const auto original_pose=poses;
  const auto mesh=loaded.scene.instances[target.instance].mesh;const auto base=loaded.scene.meshes[mesh].positions;
  Json report={{"formula_catalog",formulas.report},{"parameters",Json::array()},{"poses",Json::array()},{"golden","DAZ_STUDIO_NOT_RUN"}};
  bool legacy=file.filename().wstring().find(L"8.1")==std::wstring::npos;
  std::vector<std::string> names={"Arms Length","Chest Scale","Eyes Closed","HS Sanny Shy","Flex Quad Left","Ren Yao","BGM Ava","BGM Big Girl Base","AW - Yuki Teen FBM","Mariko","L All Nails Length"};if(!legacy) names.push_back("Eye Blink");
  for(const auto &name:names) {
    const auto m=parameter(target,name);if(m==target.morphs.size()||!target.morphs[m].unsupported.empty()) {
      report["parameters"].push_back({{"name",name},{"status","UNSUPPORTED"},{"reason",m==target.morphs.size()?"not_present":target.morphs[m].unsupported}});
      require(!legacy&&name=="Eyes Closed","指定参数仍未启用："+name+" / "+(m==target.morphs.size()?"missing":target.morphs[m].unsupported));continue;
    }
    Json check={{"name",name},{"status","PASS"},{"samples",Json::array()}};
    for(float weight:{.5f,1.f,0.f}) {
      set_parameter(target,values[0],m,weight);const auto before=runtime.formula_stats().expressions;runtime.evaluate(values,poses);const double displacement=difference(base,loaded.scene.meshes[mesh].positions);
      require(weight==0?displacement<1e-6:displacement>1e-5,"参数没有变形或恢复漂移："+name);
      const auto count=runtime.formula_stats().expressions;require(runtime.evaluate(values,poses).meshes.empty()&&runtime.formula_stats().expressions==count,"重复参数产生额外求值");
      check["samples"].push_back({{"weight",weight},{"displacement_m",displacement},{"evaluated_formulas",count-before},{"effective",runtime.effective()[0][m]}});
      if(weight==.5f) export_reference(output/(target.morphs[m].channel_id+".deformation.json"),loaded,target,skin,runtime,&target.morphs[m],weight);
    }
    report["parameters"].push_back(check);values=original;runtime.evaluate(values,poses);
  }
  const auto eye=parameter(target,legacy?"Eyes Closed":"Eye Blink"),eye_alias=parameter(target,legacy?"Eyes Closed":"Eye Blink",true);
  if(eye<target.morphs.size()&&eye_alias<target.morphs.size()&&target.morphs[eye_alias].unsupported.empty()) {set_parameter(target,values[0],eye_alias,.5f);require(values[0].morphs[eye]==.5f,"别名没有更新原参数");runtime.evaluate(values,poses);require(difference(base,loaded.scene.meshes[mesh].positions)>1e-5,"别名没有实际变形");values=original;runtime.evaluate(values,poses);report["alias"]="PASS";}
  const auto flex=parameter(target,"Flex Quad Left"),gate=parameter(target,"Base Flexions");size_t shin=skin.joints.size();for(size_t j=0;j<skin.joints.size();++j) if(skin.joints[j].id=="lShin") shin=j;
  require(shin<skin.joints.size()&&flex<target.morphs.size()&&gate<target.morphs.size(),"缺少 JCM 验证依赖");require(values[0].morphs[gate]==1,"bool 默认开启值丢失");
  poses[0][shin].rotation_degrees.x=-5.5f;runtime.evaluate(values,poses);require(std::abs(runtime.effective()[0][flex]-.5f)<1e-5,"Flex Quad 自动 JCM 权重不正确");
  set_parameter(target,values[0],gate,0);runtime.evaluate(values,poses);require(runtime.effective()[0][flex]==0,"JCM 乘法开关没有生效");report["jcm_flex"]="PASS";
  values=original;poses=original_pose;runtime.evaluate(values,poses);
  std::vector<std::filesystem::path> files;for(const auto &entry:std::filesystem::directory_iterator(pose_folder)) if(entry.path().extension()==".duf") files.push_back(entry.path());std::sort(files.begin(),files.end());
  for(const auto &path:files) {
    auto applied=daz::apply_pose(daz::read_pose(path),skin,original_pose[0],target,original[0]);values=original;values[0]=applied.properties;poses=original_pose;poses[0]=applied.joints;runtime.evaluate(values,poses);
    size_t active=0;for(size_t m=0;m<target.morphs.size();++m) if(target.morphs[m].channel_id.starts_with("pJCM")&&std::abs(runtime.effective()[0][m])>1e-6) ++active;
    for(const auto &c:applied.report["unapplied"]) require(!legacy&&(c.value("modifier","")=="eCTRLEyesUpDown"||c.value("modifier","")=="eCTRLEyesSideSide"),"存在不符合目标代际支持范围的未应用参数");
    require(difference(base,loaded.scene.meshes[mesh].positions)>.1,"姿势没有改变角色");values=original;poses=original_pose;runtime.evaluate(values,poses);const auto restored=difference(base,loaded.scene.meshes[mesh].positions);require(restored<1e-6,"姿势加 JCM 恢复漂移");
    report["poses"].push_back({{"file",utf8(path.filename())},{"active_jcms",active},{"unapplied",applied.report["unapplied"]},{"restore_error_m",restored}});
  }
  if(!files.empty()) {
    auto applied=daz::apply_pose(daz::read_pose(files[0]),skin,original_pose[0],target,original[0]);values=original;values[0]=applied.properties;poses=original_pose;poses[0]=applied.joints;
    const auto arm=parameter(target,"Arms Length");set_parameter(target,values[0],arm,.5f);runtime.evaluate(values,poses);export_reference(output/"scaled-pose.deformation.json",loaded,target,skin,runtime,&target.morphs[arm],.5f);
    values=original;poses=original_pose;runtime.evaluate(values,poses);
    for(const auto &name:{"Ren Yao","BGM Ava","BGM Big Girl Base"}) {
      const auto index=parameter(target,name);values[0]=applied.properties;poses[0]=applied.joints;set_parameter(target,values[0],index,.5f);runtime.evaluate(values,poses);
      export_reference(output/(target.morphs[index].channel_id+"-pose.deformation.json"),loaded,target,skin,runtime,&target.morphs[index],.5f);
      values=original;poses=original_pose;runtime.evaluate(values,poses);require(difference(base,loaded.scene.meshes[mesh].positions)<1e-6,"体型与姿势组合恢复漂移");
    }
  }
  size_t rejected=0;
  if(!baseline.empty()) {
    const auto previous=Json::parse(std::ifstream(baseline));std::map<std::string,std::string> disabled;
    for(const auto &m:previous.at("targets").at(0).at("morphs")) if(!m.at("unsupported").get<std::string>().empty()) disabled[m.at("id")]=m.at("unsupported");
    report["new_parameters"]=Json::array();
    for(size_t m=0;m<target.morphs.size();++m) {
      const auto &p=target.morphs[m];if(!p.unsupported.empty()||!disabled.contains(p.id)) continue;
      Json item={{"id",p.id},{"name",p.label},{"previous_reason",disabled[p.id]},{"samples",Json::array()},{"status","PASS"}};
      for(const float candidate:{.5f,1.f}) {
        const float input=std::clamp(candidate,p.minimum,p.maximum);
        try {set_parameter(target,values[0],m,input);runtime.evaluate(values,poses);
          const auto distance=difference(base,loaded.scene.meshes[mesh].positions);require(std::isfinite(distance),"参数产生非有限位移");
          item["samples"].push_back({{"input",input},{"effective",runtime.effective()[0][m]},{"displacement_m",distance}});
        } catch(const std::exception &e) {item["status"]="REJECTED";item["error"]=e.what();++rejected;}
        values=original;poses=original_pose;runtime.evaluate(values,poses);require(difference(base,loaded.scene.meshes[mesh].positions)<1e-6,"新增参数恢复漂移");
      }
      report["new_parameters"].push_back(item);
    }
    report["new_parameter_count"]=report["new_parameters"].size();report["rejected_samples"]=rejected;
  }
  report["status"]=rejected?"FAIL":"PASS";report["pose_count"]=files.size();report["formula_evaluations"]=runtime.formula_stats().expressions;
  std::ofstream(output/"regression-report.json")<<report.dump(2);require(rejected==0,"部分新增参数求值被拒绝，见批量报告");std::cout<<"Formula / parameter compatibility / alias / JCM / poses: PASS\n";
}
int wmain(int argc,wchar_t **argv) {
  try {if(argc==1) {unit();std::cout<<"Formula graph / operations / splines / dirty propagation / cycles / scaled DQS: PASS\n";}
    else {if(argc<4) throw std::runtime_error("用法：FormulaRuntimeTest <角色.duf> <输出目录> <姿势目录> [内容库...] [--baseline 原参数目录.json]");std::vector<std::filesystem::path> roots;std::filesystem::path baseline;
      for(int i=4;i<argc;++i) {if(std::wstring(argv[i])==L"--baseline") {if(++i>=argc) throw std::runtime_error("缺少基线目录文件");baseline=argv[i];}else roots.emplace_back(argv[i]);}actual(argv[1],argv[2],argv[3],roots,baseline);}return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
