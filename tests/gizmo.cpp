#include "editor/gizmo.h"
#include <iostream>
#include <stdexcept>
using namespace dfv;
using namespace dfv::editor;
static void check(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
static double distance(ir::Vec3 a,ir::Vec3 b) {return std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));}
static ir::Vec2 point(const GizmoShape &shape,int handle,int index=0) {
  for(const auto &s:shape.lines) if(s.handle==handle) {if(index--==0) return {(s.a.x+s.b.x)*.5f,(s.a.y+s.b.y)*.5f};}throw std::runtime_error("找不到手柄");
}
static runtime::PosePointer pointer(ir::Vec2 p) {runtime::PosePointer v;v.serial=v.revision=1;v.held=true;v.x=v.start_x=int(std::round(p.x));v.y=v.start_y=int(std::round(p.y));return v;}
static CameraState view(ir::Vec3 p={}) {CameraState c;c.target={p.x,p.y,p.z};c.distance=5;c.yaw=.6f;c.pitch=.4f;return c;}
static ir::Mesh mesh() {ir::Mesh m;m.positions={{0,0,0},{1,0,0},{0,0,1}};m.triangles.push_back({{0,1,2}});return m;}
static void rotate(GizmoDrag &drag,GizmoSpace space,int axis) {
  const auto camera=view(drag.pivot());drag.layout(camera,1000,800,{GizmoTool::rotate,space},1);const auto shape=drag.shape;
  auto p=pointer(point(shape,axis,9));check(drag.begin(p,camera,1000,800,{GizmoTool::rotate,space},1,mesh()),"旋转手柄未命中");check(drag.handle==axis,"旋转命中其他轴");
  // 沿投影圆移动 45 度，逐步跨越每个点，而不是直接写入欧拉角。
  for(int i=10;i<=21;++i) {const auto next=point(shape,axis,i);p.x=int(std::round(next.x));p.y=int(std::round(next.y));p.moved=true;++p.revision;drag.update(p);}
}
static void planes_and_ground() {
  runtime::Target target;target.has_edit_frame=true;target.edit_frame=runtime::make_transform({{35,50,-25},{12,17,25}});target.translation_frame=runtime::make_transform({{},{25,40,15},{1.2f,.8f,1.1f}});
  const auto loaded=target.edit_frame,parent=runtime::make_transform({{20,-30,40},{20,35,-10},{1.1f,.9f,1.3f}});
  runtime::TransformValues input;input.translation_cm={15,20,-10};input.rotation_degrees={15,25,35};input.scale={1.3f,.7f,1.1f};input.general_scale=1.4f;
  const auto current=parent*runtime::parameter_transform(input,target,loaded)*loaded;
  for(auto space:{GizmoSpace::local,GizmoSpace::world}) for(float dpi:{1.f,1.5f}) for(int normal=0;normal<3;++normal) {
    GizmoDrag drag;drag.object(target,input,loaded,current);const auto pivot=drag.pivot();auto camera=view(pivot);const int w=int(1000*dpi),h=int(800*dpi);GizmoPlane plane;bool found=false;
    for(float yaw:{.6f,1.4f,2.3f}) {camera.yaw=yaw;drag.layout(camera,w,h,{GizmoTool::translate,space},dpi);for(const auto &p:drag.shape.planes) if(p.handle==4+normal) {plane=p;found=true;break;}if(found) break;}
    check(found,"平移平面缺失");ir::Vec2 center{};for(auto p:plane.corners) {center.x+=p.x/4;center.y+=p.y/4;}
    auto p=pointer(center);check(drag.begin(p,camera,w,h,{GizmoTool::translate,space},dpi,mesh())&&drag.handle==4+normal,"平面内部没有命中对应手柄");
    p.x+=int(32*dpi);p.y-=int(19*dpi);p.moved=true;++p.revision;check(drag.update(p)&&drag.changed(),"平面拖动没有生效");
    const auto now=drag.pivot(),n=drag.shape.axes[normal];const double offset=(now.x-pivot.x)*n.x+(now.y-pivot.y)*n.y+(now.z-pivot.z)*n.z;
    check(std::abs(offset)<2e-5&&distance(now,pivot)>.02,"平面平移越出约束平面");check(drag.transform.rotation_degrees==input.rotation_degrees&&drag.transform.scale==input.scale,"平面移动污染旋转或缩放");
    p.x=p.start_x;p.y=p.start_y;++p.revision;drag.update(p);check(!drag.changed()&&drag.world==current,"平面拖动回到起点未精确恢复");
  }
  auto front=view();front.yaw=front.pitch=0;const auto edge=gizmo_shape(front,1000,800,{},{},{GizmoTool::translate,GizmoSpace::world});check(edge.planes.size()==1,"沿视线的平面没有隐藏");
  const std::vector<ir::Vec3> points={{-.2f,-.1f,-.3f},{.4f,.2f,1.7f},{.1f,-.2f,.8f}};
  auto bounds=[&](const ir::Transform &world){ir::Bounds b;for(auto p:points) b.add(world.point(p));return b;};const auto before=bounds(current);
  for(double ratio:{0.,.01,-.05,.5}) {
    const auto result=ground_aligned_transform(target,input,loaded,current,before,ratio);const auto world=parent*runtime::parameter_transform(result,target,loaded)*loaded;const auto after=bounds(world);
    check(std::abs(after.minimum.z-ratio*(before.maximum.z-before.minimum.z))<2e-6,"地面对齐没有使用当前世界高度比例");
    check(std::abs(after.minimum.x-before.minimum.x)<2e-6&&std::abs(after.minimum.y-before.minimum.y)<2e-6,"地面对齐被父级坐标轴影响");
    check(result.rotation_degrees==input.rotation_degrees&&result.scale==input.scale&&result.general_scale==input.general_scale,"地面对齐改变了旋转或缩放");
    const auto again=ground_aligned_transform(target,result,loaded,world,after,ratio);check(distance(again.translation_cm,result.translation_cm)<.0001,"重复地面对齐产生累积位移");
  }
  bool rejected=false;try {ground_aligned_transform(target,input,loaded,current,{},0);} catch(const std::exception &) {rejected=true;}check(rejected,"空包围盒没有拒绝");
}
int main() {
  try {
    planes_and_ground();
    const auto m=mesh();
    for(const std::string order:{"XYZ","XZY","YXZ","YZX","ZXY","ZYX"}) for(auto space:{GizmoSpace::local,GizmoSpace::world}) for(int axis=0;axis<3;++axis) {
      runtime::Target target;target.rotation_order=order;target.has_edit_frame=true;
      target.edit_frame=runtime::make_transform({{25,40,70},{15,-20,35}});target.translation_frame=runtime::make_transform({{},{10,20,30}});target.base_rotation_degrees={20,-15,25};
      const auto loaded=target.edit_frame*runtime::make_transform({{10,15,20},{30,20,-10},{1.2f,.8f,1.1f}});
      runtime::TransformValues input;input.translation_cm={12,-7,4};input.rotation_degrees={13,17,-11};input.scale={1.3f,.7f,1.5f};
      const auto parent=runtime::make_transform({{10,20,-5},{25,30,10},{1.1f,.9f,1.2f}});
      GizmoDrag drag;drag.object(target,input,loaded,parent*runtime::parameter_transform(input,target,loaded)*loaded);const auto pivot=drag.pivot();const auto before=drag.orientation();
      drag.layout(view(pivot),1000,800,{GizmoTool::rotate,space},1);const auto direction=drag.shape.axes[axis];
      rotate(drag,space,axis);check(drag.changed(),"旋转没有变化");check(distance(pivot,drag.pivot())<2e-5,"旋转改变了节点自身轴心");
      const auto expected=gizmo_rotation(direction,3.14159265f/4)*before;
      const auto error=runtime::orientation_error_degrees(expected,drag.orientation());if(error>=2) {std::cerr<<order<<" space="<<int(space)<<" axis="<<axis<<" error="<<error<<'\n';}check(error<2,"本地／世界旋转方向或欧拉顺序错误");
      check(drag.transform.translation_cm==input.translation_cm&&drag.transform.scale==input.scale,"旋转污染平移或缩放");
      auto origin=drag.press;origin.moved=true;origin.revision=999;drag.update(origin);check(!drag.changed()&&drag.world==parent*runtime::parameter_transform(input,target,loaded)*loaded,"旋转回到起点存在残差");
    }
    for(const std::string order:{"XYZ","XZY","YXZ","YZX","ZXY","ZYX"}) for(float sign:{-1.f,1.f}) for(int axis=0;axis<3;++axis) {
      GizmoDrag singular;runtime::TransformValues initial;initial.rotation_degrees={15,25,-35};const int middle=order[1]-'X';if(middle==0) initial.rotation_degrees.x=sign*90;else if(middle==1) initial.rotation_degrees.y=sign*90;else initial.rotation_degrees.z=sign*90;const auto before=runtime::make_transform(initial,order);
      runtime::Target target;target.rotation_order=order;singular.object(target,initial,{},before);singular.layout(view(),1000,800,{GizmoTool::rotate,GizmoSpace::world},1);const auto direction=singular.shape.axes[axis];rotate(singular,GizmoSpace::world,axis);
      check(runtime::orientation_error_degrees(gizmo_rotation(direction,3.14159265f/4)*before,runtime::rotation_frame(singular.world))<2,"欧拉角奇点阻止世界旋转");
    }
    GizmoDrag drag;runtime::Target target;target.has_edit_frame=true;target.edit_frame=ir::Transform::translate({.7f,.2f,.9f});target.translation_frame=runtime::make_transform({{},{20,40,15}});
    runtime::TransformValues initial;initial.rotation_degrees={35,25,15};const auto loaded=target.edit_frame;const auto current=runtime::parameter_transform(initial,target,loaded)*loaded;
    drag.object(target,initial,loaded,current);const auto pivot=drag.pivot();const auto camera=view(pivot);
    drag.layout(camera,1000,800,{GizmoTool::translate,GizmoSpace::world},1);auto p=pointer(point(drag.shape,0));check(drag.begin(p,camera,1000,800,{GizmoTool::translate,GizmoSpace::world},1,m),"平移未命中");p.x+=45;p.moved=true;++p.revision;drag.update(p);check(drag.pivot().x>pivot.x&&std::abs(drag.pivot().z-pivot.z)<1e-5&&std::abs(drag.pivot().y-pivot.y)<1e-5,"世界平移没有消除父坐标轴影响");
    drag.object(target,initial,loaded,current);drag.layout(camera,1000,800,{GizmoTool::scale,GizmoSpace::world},1);p=pointer(point(drag.shape,3));check(drag.begin(p,camera,1000,800,{GizmoTool::scale,GizmoSpace::world},1,m),"等比缩放未命中");p.x+=30;p.moved=true;++p.revision;drag.update(p);check(drag.transform.scale.x>1&&drag.transform.scale.x==drag.transform.scale.y&&drag.transform.scale.x==drag.transform.scale.z,"等比缩放错误");check(distance(pivot,drag.pivot())<1e-5,"缩放改变了自身轴心");
    p.cancelled=true;p.x+=30;++p.revision;const auto before=drag.transform.scale;check(!drag.update(p)&&drag.transform.scale==before,"取消后仍更新变换");
    runtime::Skin skin;skin.joints.resize(2);skin.initial.resize(2);skin.joints[1].parent=0;skin.joints[1].center_cm={20,10,5};skin.joints[1].orientation_degrees={10,20,30};skin.weights={{{1,1}},{{1,1}},{{1,1}}};
    for(auto &c:skin.joints[1].channels) {c.present=true;c.clamped=false;}skin.joints[1].channels[3].locked=true;skin.joints[1].channels[5].clamped=true;skin.joints[1].channels[5].minimum=-10;skin.joints[1].channels[5].maximum=10;
    auto input=skin.initial;input[0].rotation_degrees={15,25,35};auto effective=input;effective[1].rotation_degrees.y=12;
    drag.bone(skin,1,input,effective,{});drag.layout(view(drag.pivot()),1000,800,{GizmoTool::rotate,GizmoSpace::world},1);const auto shape=drag.shape;p=pointer(point(shape,2,9));check(drag.begin(p,view(drag.pivot()),1000,800,{GizmoTool::rotate,GizmoSpace::world},1,m,&m.positions),"骨骼未命中");for(int k=10;k<=25;++k) {auto next=point(shape,2,k);p.x=int(next.x);p.y=int(next.y);p.moved=true;++p.revision;drag.update(p);}check(drag.poses[1].rotation_degrees.x==0&&std::abs(drag.poses[1].rotation_degrees.z)<=10&&drag.poses[0]==input[0],"骨骼修改锁定轴、越限或污染父骨");check(skin.initial[1].rotation_degrees==ir::Vec3{},"代理修改源骨架");
    for(auto tool:{GizmoTool::translate,GizmoTool::scale}) {
      drag.bone(skin,1,input,effective,{});const auto start=drag.pivot();const auto camera=view(start);drag.layout(camera,1000,800,{tool,GizmoSpace::world},1);p=pointer(point(drag.shape,0));check(drag.begin(p,camera,1000,800,{tool,GizmoSpace::world},1,m,&m.positions),"骨骼平移／缩放未命中");p.x+=40;p.y-=10;p.moved=true;++p.revision;drag.update(p);
      check(drag.changed()&&drag.poses[0]==input[0],"骨骼变换没有生效或污染父骨");
      if(tool==GizmoTool::translate) check(std::abs(drag.pivot().y-start.y)<1e-5&&std::abs(drag.pivot().z-start.z)<1e-5,"骨骼世界平移方向错误");
      else check(distance(start,drag.pivot())<1e-5&&drag.poses[1].scale.y==1&&drag.poses[1].scale.z==1,"骨骼单轴缩放移动轴心或其他轴");
      p.x=p.start_x;p.y=p.start_y;++p.revision;drag.update(p);check(!drag.changed(),"骨骼回到起点仍有输入残差");
    }
    for(auto &c:skin.joints[1].channels) c.locked=true;drag.bone(skin,1,input,effective,{});drag.layout(view(drag.pivot()),1000,800,{GizmoTool::rotate,GizmoSpace::world},1);check(drag.shape.lines.empty(),"全锁定骨骼仍可操作");
    const auto normal=gizmo_shape(camera,1000,800,pivot,{},{GizmoTool::translate,GizmoSpace::world},1),large=gizmo_shape(camera,1500,1200,pivot,{},{GizmoTool::translate,GizmoSpace::world},1.5f);
    check(normal.lines.size()==large.lines.size()&&std::abs(normal.lines[0].b.x*1.5f-large.lines[0].b.x)<.001f,"DPI 缩放改变手柄的逻辑位置");
    auto front=view();front.yaw=front.pitch=0;const auto front_scale=gizmo_shape(front,1000,800,{},{},{GizmoTool::scale,GizmoSpace::local});check(front_scale.hit(front_scale.center.x,front_scale.center.y)==3,"沿视线的退化轴遮挡等比缩放中心");
    const auto hidden=gizmo_shape(camera,1000,800,{camera.eye().x*2,camera.eye().y*2,camera.eye().z*2},{},{GizmoTool::translate,GizmoSpace::world});check(hidden.lines.empty(),"相机背后手柄仍可命中");
    std::cout<<"gizmo: 72 rotation / singularity cases, pivot, translation, scale, cancellation, bone limits and DPI passed\n";return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
