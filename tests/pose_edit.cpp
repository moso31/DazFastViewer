#include "runtime/pose_edit.h"
#include "runtime/morph.h"
#include "editor/pose_drag.h"
#include "daz/skeleton.h"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace dfv;
static void check(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
static double distance(ir::Vec3 a,ir::Vec3 b) {return std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));}
static runtime::Skin fixture() {
  runtime::Skin s;runtime::Joint root;root.name="Figure";s.joints.push_back(root);
  for(int i=0;i<3;++i) {runtime::Joint j;j.name=i==0?"lShldrBend":i==1?"lForearmBend":"lHand";j.parent=i;j.center_cm={float(i*40),0,0};j.end_cm={float((i+1)*40),0,0};
    for(int a=0;a<3;++a) {auto &c=j.channels[3+a];c.present=true;c.clamped=true;c.minimum=-160;c.maximum=160;}
    s.joints.push_back(j);
  }
  s.initial.resize(s.joints.size());return s;
}
static void unit() {
  auto s=fixture();auto p=s.initial;s.joints[2].channels[5].maximum=90;s.joints[2].channels[4].locked=true;
  runtime::set_joint_value(s,p,2,5,140);check(p[2].rotation_degrees.z==90,"FK 未执行通道限位");
  bool rejected=false;try {runtime::set_joint_value(s,p,2,4,20);}catch(...) {rejected=true;}check(rejected,"FK 修改了锁定通道");
  p=s.initial;const auto chain=runtime::ik_chain(s,3);check(chain.size()==3&&chain.back()==1,"手臂链越过肩膀");
  runtime::IkGoal goal{3,true,{.7f,0,.5f},1};auto result=runtime::solve_ik(s,p,chain,std::span(&goal,1),64);
  check(result.changed&&result.error<.001,"可达 IK 目标未收敛");check(p[0]==s.initial[0]&&p[2].rotation_degrees.y==0,"IK 修改了固定根或锁定轴");
  check(p[2].rotation_degrees.z<=90,"IK 越过关节限位");
  p=s.initial;goal.position={.8f,0,0};result=runtime::solve_ik(s,p,chain,std::span(&goal,1),64);check(result.error<.002,"共线内收目标未解开奇点");
  p=s.initial;goal.position={3,0,0};runtime::solve_ik(s,p,chain,std::span(&goal,1),64);check(distance(runtime::joint_point(s,p,3,true),{0,0,0})<=1.20001,"不可达 IK 拉长了骨骼");
  for(const auto &pose:p) check(pose.scale==ir::Vec3{1,1,1}&&pose.translation_cm==ir::Vec3{},"IK 改写缩放或平移");
  p=s.initial;goal.position=runtime::joint_point(s,p,3,true);p[0].translation_cm.x=10;
  result=runtime::solve_ik(s,p,chain,std::span(&goal,1),64);if(result.error>=.002) std::cerr<<"pin error="<<result.error<<" iterations="<<result.iterations<<'\n';check(result.error<.002&&p[0].translation_cm.x==10,"骨盆平移时固定点未补偿，或求解回退了骨盆输入");
  // 角度约束必须保留 Twist，并允许末端移动；单纯锁本地欧拉角不能通过父骨旋转用例。
  p=s.initial;runtime::IkGoal angle{3,true,{},20};angle.fix_position=false;angle.fix_orientation=true;angle.orientation=runtime::joint_orientation(s,p,3);
  std::array<runtime::IkGoal,2> oriented={runtime::IkGoal{3,true,{.95f,0,.3f},1},angle};
  result=runtime::solve_ik(s,p,chain,oriented,64);check(result.error<.002&&result.angle_error_degrees<.1,"固定角度拖动没有同时满足位置与朝向");
  p=s.initial;p[0].rotation_degrees={15,20,25};result=runtime::solve_ik(s,p,chain,std::span(&angle,1),64);
  check(result.angle_error_degrees<.1&&p[0].rotation_degrees==ir::Vec3{15,20,25},"固定角度未补偿父骨的三轴旋转");
  p=s.initial;auto both=angle;both.fix_position=true;both.position=runtime::joint_point(s,p,3,true);p[0].translation_cm.x=10;
  result=runtime::solve_ik(s,p,chain,std::span(&both,1),64);check(result.error<.002&&result.angle_error_degrees<.1,"同时固定位置和角度未补偿父骨平移");
  auto locked=s;for(auto &j:locked.joints) for(auto &c:j.channels) c.locked=true;p=s.initial;p[0].rotation_degrees.z=20;const auto before_locked=p;
  result=runtime::solve_ik(locked,p,chain,std::span(&angle,1),64);check(p==before_locked&&result.angle_error_degrees>19,"不可达角度没有报告残差，或修改了锁定轴");
  p=s.initial;p[0].rotation_degrees.z=179;result=runtime::solve_ik(s,p,chain,std::span(&angle,1),64);if(result.angle_error_degrees>=.1) std::cerr<<"angle error="<<result.angle_error_degrees<<" iterations="<<result.iterations<<'\n';check(result.angle_error_degrees<.1,"固定角度接近 180 度时未收敛");
  const auto world=runtime::make_transform({{3,4,5},{10,40,20},{2,.5f,3}});runtime::PosePin pin{0,3,{},false,true,runtime::rotation_frame(world)*angle.orientation};
  const auto converted=runtime::pin_goals(std::span(&pin,1),0,world);check(converted.size()==1&&!converted[0].fix_position&&converted[0].fix_orientation&&runtime::orientation_error_degrees(converted[0].orientation,angle.orientation)<.1,"世界角度受整体非均匀缩放影响");
  p=s.initial;runtime::solve_ik(s,p,chain,oriented,64);const auto resolve=[](const auto &input){auto out=input;out[3].rotation_degrees.z+=input[1].rotation_degrees.z*.2f;return out;};
  check(runtime::orientation_error_degrees(angle.orientation,runtime::joint_orientation(s,resolve(p),3))>.5,"动态 ERC 校准测试没有产生朝向偏差");
  result=runtime::refine_ik_input(s,p,chain,oriented,resolve);check(result.error<.002&&result.angle_error_degrees<.1,"提交校准没有消除动态 ERC 的角度偏差");
  auto effective=s.initial;effective[1].rotation_degrees.z=15;auto solved=effective;solved[1].rotation_degrees.z=25;
  check(runtime::ik_input(s.initial,effective,solved)[1].rotation_degrees.z==10,"ERC 偏移被重复写回输入");
  const auto limited=runtime::ik_limits(s,s.initial,effective);check(limited.joints[1].channels[5].maximum==175,"ERC 通道限位没有转换到求值空间");
  check(runtime::ik_selection_allowed(1,2,2,false,true),"单选角色不能启动 IK");
  for(auto count:{size_t(0),size_t(2),size_t(3)}) check(!runtime::ik_selection_allowed(count,2,2,false,true),"未选择或多选启动了 IK");
  check(!runtime::ik_selection_allowed(1,1,2,false,true)&&!runtime::ik_selection_allowed(1,2,2,true,true)&&!runtime::ik_selection_allowed(1,2,2,false,false),"其他角色、组合键或普通对象启动 IK");
  s=fixture();ir::Mesh mesh;mesh.positions={{.8f,0,-.05f},{1.2f,0,-.05f},{1.2f,0,.05f}};mesh.triangles.push_back({{0,1,2}});s.weights={{{3,1}},{{3,1}},{{3,1}}};
  editor::PoseDrag drag;runtime::PosePointer mouse;mouse.serial=1;mouse.revision=1;mouse.held=true;mouse.x=mouse.start_x=400;mouse.y=mouse.start_y=300;CameraState camera;camera.target={.6f,0,0};camera.distance=3;camera.yaw=0;camera.pitch=0;
  drag.begin(s,mesh,mesh.positions,s.initial,s.initial,{},3,mouse,camera,800,600);check(drag.active,"预览无法开始");
  check(!drag.update(mouse)&&drag.input()==s.initial,"单击产生了姿态变化");mouse.moved=true;mouse.x-=40;mouse.y-=40;++mouse.revision;check(drag.update(mouse)&&drag.input()!=s.initial,"拖动没有更新预览姿态");
  check(mesh.positions[0]==ir::Vec3{.8f,0,-.05f}&&s.initial[1].rotation_degrees==ir::Vec3{},"代理预览修改源数据");
  angle.orientation=runtime::joint_orientation(s,s.initial,3);drag.begin(s,mesh,mesh.positions,s.initial,s.initial,{},3,mouse,camera,800,600,{angle});check(drag.active,"仅固定角度错误禁用了当前部位拖动");
}
static void actual(const std::filesystem::path &file,const std::filesystem::path &output,const std::vector<std::filesystem::path> &roots) {
  auto loaded=daz::load(file,{roots,false});auto catalog=daz::load_skeletons(loaded);check(!catalog.skins.empty(),"没有真实骨架");const auto &s=catalog.skins.front();
  nlohmann::json report={{"channels",nlohmann::json::array()},{"ik",nlohmann::json::array()},{"fk",nlohmann::json::array()}};
  size_t editable=0;
  for(size_t j=1;j<s.joints.size();++j) for(int c=0;c<10;++c) if(runtime::editable_channel(s,j,c)) {const auto &channel=s.joints[j].channels[c];++editable;auto p=s.initial;
    float v=runtime::joint_value(p[j],c)+std::max(.01f,channel.step);if(channel.clamped) v=std::clamp(v,channel.minimum,channel.maximum);if(c>=6&&v==0) v=1;
    runtime::set_joint_value(s,p,j,c,v);runtime::validate_pose(s,p);
    report["channels"].push_back({{"joint",s.joints[j].name},{"channel",c},{"label",channel.label},{"visible",channel.visible},{"locked",channel.locked},{"min",channel.minimum},{"max",channel.maximum}});
  }
  check(editable>100,"真实可编辑骨骼通道未完整发现");
  const auto &base=loaded.scene.meshes[loaded.scene.instances[s.instance].mesh].positions;const auto neutral=runtime::deform(s,s.initial,base);
  for(const auto &name:{"lMid3","lHand","lFoot","head","lEye"}) for(size_t j=0;j<s.joints.size();++j) if(s.joints[j].name==name) {
    for(int c=0;c<10;++c) if(s.joints[j].channels[c].visible&&runtime::editable_channel(s,j,c)) {auto p=s.initial;const auto &limit=s.joints[j].channels[c];const float before=runtime::joint_value(p[j],c);float step=c<3?.1f:c<6?5.f:.05f;if(limit.clamped&&before+step>limit.maximum) step=-step;
      runtime::set_joint_value(s,p,j,c,before+step);const auto changed=runtime::deform(s,p,base);size_t moved=0;for(size_t v=0;v<base.size();++v) if(distance(changed[v],neutral[v])>1e-7) ++moved;
      check(moved>0,"真实可见 FK 通道没有改变几何");check(runtime::deform(s,s.initial,base)==neutral,"FK 恢复漂移");report["fk"].push_back({{"joint",name},{"label",limit.label},{"channel",c},{"moved_vertices",moved}});
    }
  }
  for(const auto &name:{"lHand","rHand","lFoot","rFoot","lMid3","rMid3","head"}) {
    int joint=-1;for(size_t j=0;j<s.joints.size();++j) if(s.joints[j].name==name) joint=int(j);check(joint>=0,"真实测试骨骼缺失");
    auto p=s.initial;const auto chain=runtime::ik_chain(s,joint);auto target=runtime::joint_point(s,p,joint,true);target.z+=.02f;target.y+=.02f;
    runtime::IkGoal goal{joint,true,target,1};const double before=distance(target,runtime::joint_point(s,p,joint,true));const auto start=std::chrono::steady_clock::now();
    const auto result=runtime::solve_ik(s,p,chain,std::span(&goal,1),64);const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    check(result.changed&&result.error<before,"真实 IK 没有接近目标");runtime::validate_pose(s,p);
    for(size_t j=0;j<s.joints.size();++j) for(int c=3;c<6;++c) if(s.joints[j].channels[c].locked) check(runtime::joint_value(p[j],c)==runtime::joint_value(s.initial[j],c),"真实 IK 修改锁定轴");
    report["ik"].push_back({{"joint",name},{"before_m",before},{"error_m",result.error},{"ms",ms},{"iterations",result.iterations}});
    if(std::string(name)=="lHand"||std::string(name)=="rHand") {
      p=s.initial;runtime::IkGoal angle{joint,true,{},20};angle.fix_position=false;angle.fix_orientation=true;angle.orientation=runtime::joint_orientation(s,p,joint);
      const std::array<runtime::IkGoal,2> goals={goal,angle};const auto fixed=runtime::solve_ik(s,p,chain,goals,64);
      check(fixed.error<.002&&fixed.angle_error_degrees<.1,"真实手部固定角度未保持朝向或无法移动");
      report["angle_ik"].push_back({{"joint",name},{"position_error_m",fixed.error},{"angle_error_degrees",fixed.angle_error_degrees}});
    }
  }
  int hip=-1;std::vector<int> feet;for(size_t j=0;j<s.joints.size();++j) {if(s.joints[j].name=="hip") hip=int(j);if(s.joints[j].name=="lFoot"||s.joints[j].name=="rFoot") feet.push_back(int(j));}
  check(hip>=0&&feet.size()==2,"固定点验证缺少骨盆或双脚");auto pinned=s.initial;std::vector<runtime::IkGoal> goals;std::vector<int> chain;
  for(int foot:feet) {goals.push_back({foot,true,runtime::joint_point(s,pinned,foot,true),1});for(int j:runtime::ik_chain(s,foot)) if(std::find(chain.begin(),chain.end(),j)==chain.end()) chain.push_back(j);}
  pinned[hip].translation_cm.y-=1;runtime::solve_ik(s,pinned,chain,goals,64);double pin_error=0;for(const auto &goal:goals) pin_error=std::max(pin_error,distance(goal.position,runtime::joint_point(s,pinned,goal.joint,true)));
  check(pin_error<.001,"真实双脚固定未补偿骨盆移动");report["pin_error_m"]=pin_error;
  report["status"]="PASS";report["editable"]=editable;std::filesystem::create_directories(output);std::ofstream(output/"pose-edit.json")<<report.dump(2);
}
int wmain(int argc,wchar_t **argv) {try {unit();if(argc>2) {std::vector<std::filesystem::path> roots;for(int i=3;i<argc;++i) roots.emplace_back(argv[i]);actual(argv[1],argv[2],roots);}std::cout<<"FK / constrained IK / singularity / selection gates / proxy: PASS\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
