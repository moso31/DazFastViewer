#include "powerpose_fixture.h"
#include "editor/powerpose_drag.h"
#include "editor/pose_drag.h"
#include "daz/skeleton.h"
#include <iostream>
#include <fstream>
#include <cmath>
using namespace dfv;
static void check(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
static bool near(float a,float b) {return std::abs(a-b)<.0001f;}
static nlohmann::json directions(const runtime::Skin &skin) {
  nlohmann::json report=nlohmann::json::array();size_t edits=0,navigations=0;
  for(const auto &page:runtime::powerpose_templates()) for(const auto &point:page.points) {
    if(point.kind==runtime::PosePointKind::navigation) {++navigations;continue;}++edits;const auto bound=runtime::bind_powerpose(skin,point);
    for(bool right:{false,true}) for(const auto &[dx,dy]:std::array<std::pair<int,int>,4>{{{0,-30},{0,30},{-30,0},{30,0}}}) {
      const auto result=runtime::apply_powerpose(skin,bound,skin.initial,right,dx,dy);runtime::validate_pose(skin,result);
      const int slot=(right?2:0)+(dx==0?1:0);std::set<std::pair<int,int>> affected;for(const auto &b:bound.slots[slot]) affected.emplace(b.joint,b.channel);
      size_t changed=0;for(size_t j=0;j<result.size();++j) for(int c=0;c<10;++c) {
        const auto v=runtime::joint_value(result[j],c),before=runtime::joint_value(skin.initial[j],c);const auto &limit=skin.joints[j].channels[c];
        if(v!=before) {++changed;check(affected.contains({int(j),c}),"方向写入未绑定参数");check(!limit.locked,"写入锁定轴");check(!limit.clamped||(v>=limit.minimum&&v<=limit.maximum),"越过角色实例限位");}
      }
      check(result==runtime::apply_powerpose(skin,bound,skin.initial,right,dx,dy),"重复相同位移发生漂移");
      check(runtime::apply_powerpose(skin,bound,skin.initial,right,0,0)==skin.initial,"回到按下位置未恢复");
      report.push_back({{"point",point.id},{"right",right},{"delta",{dx,dy}},{"changed",changed},{"bindings",bound.slots[slot].size()},{"unavailable",bound.unavailable}});
    }
  }check(edits==85&&navigations==6,"模板范围不符");return report;
}
static void unit() {
  auto skin=powerpose_fixture();auto bind=[&](const char *id){return runtime::bind_powerpose(skin,powerpose_point(id));};
  check(powerpose_point("B07").kind==runtime::PosePointKind::group,"双肩组缺少虚线类型");
  for(const auto *id:{"B08","B09"}) {const auto b=bind(id);check(b.slots[1].size()==1&&b.slots[1][0].channel==5,"上臂左键纵向不是 Bend");check(b.slots[3].size()==1&&skin.joints[b.slots[3][0].joint].id.find("ShldrTwist")!=std::string::npos&&b.slots[3][0].channel==3,"上臂右键纵向未转交 Twist");}
  auto shoulder=bind("B09");const int twist=shoulder.slots[3][0].joint;skin.joints[twist].channels[3].locked=true;check(bind("B09").slots[3].empty(),"锁定 Twist 后仍可拖动");skin.joints[twist].channels[3].locked=false;
  auto left=bind("B27");const int thigh=runtime::powerpose_joint(skin,"lThigh"),shin=runtime::powerpose_joint(skin,"lShin");
  auto p=runtime::apply_powerpose(skin,left,skin.initial,false,0,30);check(near(p[thigh].rotation_degrees.x,15)&&near(p[shin].rotation_degrees.x,16.6f),"腿组没有按大小腿不同量程变化");
  p=runtime::apply_powerpose(skin,left,skin.initial,false,0,-10000);check(p[thigh].rotation_degrees.x==-115&&p[shin].rotation_degrees.x==-11,"腿组大位移没有限位");
  skin.joints[shin].channels[3].minimum=-2;skin.joints[shin].channels[3].maximum=80;left=bind("B27");p=runtime::apply_powerpose(skin,left,skin.initial,false,0,30);check(near(p[shin].rotation_degrees.x,8.2f),"没有使用实例覆盖范围");
  p=runtime::apply_powerpose(skin,left,skin.initial,false,0,30,.1);check(near(p[thigh].rotation_degrees.x,1.5f),"Shift 精调倍率错误");
  auto eyes=bind("N02");p=runtime::apply_powerpose(skin,eyes,skin.initial,true,10,30);check(p[runtime::powerpose_joint(skin,"lEye")].rotation_degrees.y*p[runtime::powerpose_joint(skin,"rEye")].rotation_degrees.y<0,"双眼右键水平没有反向");check(eyes.slots[3].empty(),"双眼右键纵向被额外绑定");
  check(bind("N03").slots[2].empty()&&bind("N04").slots[3].empty(),"单眼被增加右键动作");
  check(bind("B20").slots[3][0].rate==-bind("H29").slots[3][0].rate,"两个左手腕入口的符号差异丢失");
  auto translated=bind("B21");p=runtime::apply_powerpose(skin,translated,skin.initial,true,10,10);const int hip=runtime::powerpose_joint(skin,"hip");check(p[hip].translation_cm.x>0&&p[hip].translation_cm.y<0&&p[hip].translation_cm.z==0,"髋部平移坐标映射错误");
  auto start=skin.initial;start[hip].rotation_degrees.y=9;start[shin].rotation_degrees.x=25;p=runtime::reset_powerpose(skin,translated,start);check(p==start,"本点恢复修改了无关通道");
  skin.initial[shin].rotation_degrees.x=90;p=runtime::reset_powerpose(skin,left,start);check(p[shin].rotation_degrees.x==90,"恢复加载姿势擅自裁剪了原始限位外数值");skin.initial[shin].rotation_degrees.x=0;
  directions(skin);skin.joints.front().id="Genesis8_1Male";check(directions(skin).size()==680,"男性未共用 85 点机制");
  // 稀疏三角形重排必须同步压缩权重；共享代理不能保留原始完整权重表。
  ir::Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{0,0,1},{2,0,0}};mesh.triangles.push_back({{3,1,2}});skin.weights={{{0,1}},{{uint32_t(hip),1}},{{uint32_t(hip),1}},{{uint32_t(hip),1}}};
  runtime::Skin proxy_skin;ir::Mesh proxy;std::vector<ir::Vec3> source;editor::build_pose_proxy(skin,mesh,mesh.positions,proxy_skin,proxy,source);check(source.size()==3&&proxy_skin.weights.size()==3&&source[0]==mesh.positions[3],"代理索引或权重未同步压缩");
  editor::PowerPoseDrag drag;runtime::PowerPoseInput mouse;mouse.serial=1;mouse.event=1;mouse.held=true;drag.begin(skin,mesh,mesh.positions,skin.initial,skin.initial,{}, {},translated,mouse,{},800,600,{});
  mouse.moved=true;mouse.dx=12;++mouse.event;check(drag.update(skin,mouse)&&drag.proxy.positions!=source,"PowerPose 灰模未更新");
  const auto committed=drag.commit(skin,[](const auto &p){return p;});check(committed[hip].translation_cm.x>0&&skin.initial[hip].translation_cm.x==0&&mesh.positions.size()==4,"预览修改了原始输入");
  mouse.dx=0;++mouse.event;check(drag.update(skin,mouse)&&!drag.changed(),"回到起点仍产生提交");
  auto eye=bind("N03");mouse.right=true;drag.begin(skin,mesh,mesh.positions,skin.initial,skin.initial,{}, {},eye,mouse,{},800,600,{});check(!drag.active,"单眼无绑定右键仍进入灰模事务");mouse.right=false;mouse.dx=12;
  auto fig=bind("B35");drag.begin(skin,mesh,mesh.positions,skin.initial,skin.initial,{}, {},fig,mouse,{},800,600,{});++mouse.event;check(drag.update(skin,mouse)&&drag.world.value[3]>0,"角色整体未更新实例矩阵");check(drag.commit(skin,[](const auto &p){return p;})==skin.initial,"角色整体改写了骨骼姿势");
  runtime::Target frame;frame.has_edit_frame=true;frame.edit_frame=ir::Transform::translate({2,0,0});frame.base_rotation_degrees={10,20,30};frame.rotation_order="YZX";
  runtime::TransformValues saved;saved.rotation_degrees=frame.base_rotation_degrees;const auto loaded=frame.edit_frame*runtime::make_transform(saved,frame.rotation_order);
  drag.begin(skin,mesh,mesh.positions,skin.initial,skin.initial,loaded,{},bind("B36"),mouse,{},800,600,{},&frame,loaded);++mouse.event;drag.update(skin,mouse);
  const auto pivot=drag.world.point({});check(near(pivot.x,2)&&near(pivot.y,0)&&near(pivot.z,0),"整体旋转灰模绕了世界原点，而非角色自己的轴心");
  bool failed=false;try {runtime::apply_powerpose(skin,left,skin.initial,false,NAN,0);}catch(...) {failed=true;}check(failed,"无效位移未拒绝");
}
static nlohmann::json pinned(const runtime::Skin &skin,const ir::Scene &scene) {
  editor::PowerPoseDrag drag;std::vector<runtime::IkGoal> goals;
  for(const auto *name:{"lFoot","rFoot"}) {const int j=runtime::powerpose_joint(skin,name);runtime::IkGoal g{j,true,runtime::joint_point(skin,skin.initial,j,true),20};g.fix_orientation=true;g.orientation=runtime::joint_orientation(skin,skin.initial,j);goals.push_back(g);}
  const auto &mesh=scene.meshes[scene.instances[skin.instance].mesh];runtime::PowerPoseInput pointer;pointer.precision=.1;pointer.right=true;pointer.moved=true;pointer.dy=6;pointer.event=1;
  drag.begin(skin,mesh,mesh.positions,skin.initial,skin.initial,{}, {},runtime::bind_powerpose(skin,powerpose_point("B21")),pointer,{},800,600,goals);
  drag.update(skin,pointer);const auto final=drag.commit(skin,[](const auto &p){return p;});nlohmann::json report=nlohmann::json::array();
  for(const auto &g:goals) {auto a=runtime::joint_point(skin,final,g.joint,true),b=g.position;const double distance=std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));const double angle=runtime::orientation_error_degrees(g.orientation,runtime::joint_orientation(skin,final,g.joint));report.push_back({{"joint",skin.joints[g.joint].name},{"distance",distance},{"angle",angle}});check(distance<.005&&angle<.5,"真实双脚位置及角度固定未保持");}
  return report;
}
int wmain(int argc,wchar_t **argv) {try {
  unit();if(argc>2) {std::vector<std::filesystem::path> roots;for(int i=3;i<argc;++i) roots.emplace_back(argv[i]);auto scene=daz::load(argv[1],{roots,false});const auto catalog=daz::load_skeletons(scene);check(!catalog.skins.empty(),"真实文件没有骨架");nlohmann::json report;report["status"]="PASS";report["directions"]=directions(catalog.skins.front());report["pins"]=pinned(catalog.skins.front(),scene.scene);report["figure"]=catalog.skins.front().joints.front().id;std::ofstream(std::filesystem::path(argv[2]))<<report.dump(2);std::cout<<report["pins"].dump()<<'\n';}
  std::cout<<"PowerPose bindings / directions / native limits / proxies: PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
