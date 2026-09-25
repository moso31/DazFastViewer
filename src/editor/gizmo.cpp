#include "editor/gizmo.h"
#include <numbers>

namespace dfv::editor {
namespace {
ir::Vec3 add(ir::Vec3 a,ir::Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
ir::Vec3 mul(ir::Vec3 a,float f) {return {a.x*f,a.y*f,a.z*f};}
float dot(ir::Vec3 a,ir::Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
ir::Vec3 cross(ir::Vec3 a,ir::Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
ir::Vec3 unit(ir::Vec3 a) {return mul(a,1/std::max(1e-12f,std::sqrt(dot(a,a))));}
ir::Vec3 direction(const ir::Transform &m,ir::Vec3 a) {return sub(m.point(a),m.point({}));}
float &component(ir::Vec3 &v,int a) {return a==0?v.x:a==1?v.y:v.z;}
bool project(const CameraState &camera,int w,int h,ir::Vec3 p,ir::Vec2 &out) {
  const auto m=camera.matrix();const auto d=sub(p,{m[3],m[7],m[11]});const float z=d.x*m[2]+d.y*m[6]+d.z*m[10];
  if(z<=std::max(.00001f,camera.distance*1e-5f)) return false;
  const float f=std::min(w,h)*.5f/std::tan(.4f)/z;
  out={w*.5f+(d.x*m[0]+d.y*m[4]+d.z*m[8])*f,h*.5f-(d.x*m[1]+d.y*m[5]+d.z*m[9])*f};return true;
}
float distance(ir::Vec2 p,ir::Vec2 a,ir::Vec2 b) {
  const float x=b.x-a.x,y=b.y-a.y,t=std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/std::max(1e-8f,x*x+y*y),0.f,1.f);
  return std::hypot(p.x-a.x-x*t,p.y-a.y-y*t);
}
bool plane_point(const CameraState &camera,int w,int h,ir::Vec3 pivot,ir::Vec3 normal,float x,float y,ir::Vec3 &point) {
  const auto m=camera.matrix();const ir::Vec3 eye{m[3],m[7],m[11]};
  const float f=2*std::tan(.4f)/std::min(w,h),sx=(x-w*.5f)*f,sy=(h*.5f-y)*f;
  const ir::Vec3 ray{m[2]+sx*m[0]+sy*m[1],m[6]+sx*m[4]+sy*m[5],m[10]+sx*m[8]+sy*m[9]};
  const float den=dot(ray,normal);if(std::abs(den)<1e-5f) return false;
  const float t=dot(sub(pivot,eye),normal)/den;if(t<=0||!std::isfinite(t)) return false;
  point=add(eye,mul(ray,t));return true;
}
float ring_angle(const CameraState &camera,int width,int height,const GizmoShape &shape,int handle,float x,float y) {
  const auto u=shape.axes[(handle+1)%3],v=shape.axes[(handle+2)%3];float best=1e30f,result=0;
  for(int k=0;k<720;++k) {const float t=k*2*std::numbers::pi_v<float>/720;ir::Vec2 p;
    if(project(camera,width,height,add(shape.pivot,mul(add(mul(u,std::cos(t)),mul(v,std::sin(t))),shape.radius)),p)) {
      const float d=std::hypot(x-p.x,y-p.y);if(d<best) {best=d;result=t;}
    }
  }return result;
}
ir::Vec3 euler_angles(const ir::Transform &rotation,const std::string &order,ir::Vec3 reference) {
  // 把任意非重复轴顺序置换成 Rz * Ry * Rx；奇排列同时反转角度符号。
  // IR → DAZ：X 不变，Y = IR.Z，Z = -IR.Y。
  const int indices[]={0,2,1},signs[]={1,1,-1};double m[3][3]{};
  const int i=order[0]-'X',j=order[1]-'X',k=order[2]-'X';const int axes[]={i,j,k};
  for(int r=0;r<3;++r) for(int c=0;c<3;++c) m[r][c]=signs[axes[r]]*signs[axes[c]]*rotation.value[indices[axes[r]]*4+indices[axes[c]]];
  const double parity=(i+1)%3==j?1.:-1.,cosine=std::hypot(m[0][0],m[1][0]);
  double angles[]={0,std::atan2(-m[2][0],cosine),0};
  if(cosine>1e-6) {angles[0]=std::atan2(m[2][1],m[2][2]);angles[2]=std::atan2(m[1][0],m[0][0]);}
  else {angles[2]=component(reference,k)*std::numbers::pi/180*parity;angles[0]=std::atan2(-m[1][2],m[1][1])+(angles[1]>=0?1:-1)*angles[2];}
  ir::Vec3 best;double cost=1e30;
  for(int branch=0;branch<2;++branch) {
    const double alternate[]={angles[0]+std::numbers::pi,std::numbers::pi-angles[1],angles[2]+std::numbers::pi};ir::Vec3 candidate;double distance=0;
    for(int c=0;c<3;++c) {const double value=(branch?alternate[c]:angles[c])*parity*180/std::numbers::pi;const double delta=std::remainder(value-component(reference,axes[c]),360.);component(candidate,axes[c])=component(reference,axes[c])+float(delta);distance+=delta*delta;}
    if(distance<cost) {best=candidate;cost=distance;}
  }return best;
}
}
int GizmoShape::hit(float x,float y,float tolerance) const {
  for(const auto &plane:planes) {
    bool positive=false,negative=false;
    for(int i=0;i<4;++i) {const auto a=plane.corners[i],b=plane.corners[(i+1)%4];const float c=(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);positive|=c>0;negative|=c<0;}
    if(!(positive&&negative)) return plane.handle;
  }
  int result=-1;float best=tolerance;
  for(const auto &line:lines) {const float d=distance({x,y},line.a,line.b);if(d<best) {best=d;result=line.handle;}}return result;
}
ir::Transform gizmo_rotation(ir::Vec3 axis,float radians) {
  axis=unit(axis);const float a[]={axis.x,axis.y,axis.z},c=std::cos(radians),s=std::sin(radians);ir::Transform r;
  for(int i=0;i<3;++i) for(int j=0;j<3;++j) r.value[i*4+j]=(i==j?c:0)+(1-c)*a[i]*a[j];
  r.value[1]-=s*axis.z;r.value[2]+=s*axis.y;r.value[4]+=s*axis.z;r.value[6]-=s*axis.x;r.value[8]-=s*axis.y;r.value[9]+=s*axis.x;return r;
}
runtime::TransformValues ground_aligned_transform(const runtime::Target &target,const runtime::TransformValues &value,
  const ir::Transform &loaded,const ir::Transform &current,const ir::Bounds &bounds,double ratio) {
  if(bounds.empty||!std::isfinite(ratio)) throw std::runtime_error("无法计算角色的地面对齐范围");
  // IR 为 Z 向上、单位米；DAZ 世界 Y 对应这里的 Z。比例始终乘当前世界高度。
  const double height=double(bounds.maximum.z)-bounds.minimum.z,shift=ratio*height-bounds.minimum.z;
  if(!std::isfinite(shift)||height<0) throw std::runtime_error("角色的地面对齐范围无效");
  if(std::abs(shift)<=std::max(1e-7,height*1e-7)) return value;
  const auto prefix=current*ir::inverse(runtime::parameter_transform(value,target,loaded)*loaded);
  const auto delta=direction(ir::inverse(prefix*target.translation_frame),{0,0,float(shift)});
  auto result=value;result.translation_cm=add(value.translation_cm,{delta.x*100,delta.z*100,-delta.y*100});
  runtime::validate_transform(result);return result;
}
GizmoShape gizmo_shape(const CameraState &camera,int w,int h,ir::Vec3 pivot,const ir::Transform &orientation,GizmoSettings settings,float dpi,std::array<bool,3> enabled) {
  GizmoShape out;out.pivot=pivot;out.pixels=90*dpi;
  if(settings.tool==GizmoTool::select||w<=0||h<=0||!project(camera,w,h,pivot,out.center)) return out;
  const auto m=camera.matrix();const auto d=sub(pivot,{m[3],m[7],m[11]});const float depth=d.x*m[2]+d.y*m[6]+d.z*m[10];
  out.radius=out.pixels*2*std::tan(.4f)*depth/std::min(w,h);
  const ir::Vec3 axes[]={{1,0,0},{0,0,1},{0,-1,0}};
  const bool local=settings.space==GizmoSpace::local||settings.tool==GizmoTool::scale;
  for(int i=0;i<3;++i) out.axes[i]=local?unit(direction(orientation,axes[i])):axes[i];
  auto segment=[&](ir::Vec3 a,ir::Vec3 b,int handle) {ir::Vec2 p,q;if(project(camera,w,h,a,p)&&project(camera,w,h,b,q)) out.lines.push_back({p,q,handle});};
  for(int i=0;i<3;++i) if(enabled[i]) {
    if(settings.tool==GizmoTool::rotate) {
      const auto u=out.axes[(i+1)%3],v=out.axes[(i+2)%3];
      for(int k=0;k<96;++k) {auto at=[&](int n){const float t=float(n)*2*std::numbers::pi_v<float>/96;return add(pivot,mul(add(mul(u,std::cos(t)),mul(v,std::sin(t))),out.radius));};segment(at(k),at(k+1),i);}
    } else {
      const auto end=add(pivot,mul(out.axes[i],out.radius));
      ir::Vec2 p;if(project(camera,w,h,end,p)) {
        const float dx=p.x-out.center.x,dy=p.y-out.center.y,length=std::hypot(dx,dy);if(length<12*dpi) continue;
        segment(add(pivot,mul(out.axes[i],out.radius*.16f)),end,i);
        if(settings.tool==GizmoTool::translate) {const float x=dx/length,y=dy/length;out.lines.push_back({{p.x-(x*.9f+y*.45f)*12*dpi,p.y-(y*.9f-x*.45f)*12*dpi},p,i});out.lines.push_back({{p.x-(x*.9f-y*.45f)*12*dpi,p.y-(y*.9f+x*.45f)*12*dpi},p,i});}
        else {const float r=4*dpi;const ir::Vec2 a[]={{p.x-r,p.y-r},{p.x+r,p.y-r},{p.x+r,p.y+r},{p.x-r,p.y+r}};for(int k=0;k<4;++k) out.lines.push_back({a[k],a[(k+1)%4],i});}
      }
    }
  }
  if(settings.tool==GizmoTool::translate) for(int n=0;n<3;++n) {
    const int a=(n+1)%3,b=(n+2)%3;if(!enabled[a]||!enabled[b]) continue;
    const auto normal=unit(cross(out.axes[a],out.axes[b]));if(std::abs(dot(unit(d),normal))<.12f) continue;
    GizmoPlane plane;plane.handle=4+n;bool visible=true;const float corners[][2]={{.25f,.25f},{.48f,.25f},{.48f,.48f},{.25f,.48f}};
    for(int k=0;k<4;++k) visible&=project(camera,w,h,add(pivot,mul(add(mul(out.axes[a],corners[k][0]),mul(out.axes[b],corners[k][1])),out.radius)),plane.corners[k]);
    float area=0;for(int k=0;k<4;++k) {const auto p=plane.corners[k],q=plane.corners[(k+1)%4];area+=p.x*q.y-p.y*q.x;}
    if(!visible||std::abs(area)<72*dpi*dpi) continue;
    out.planes.push_back(plane);for(int k=0;k<4;++k) out.lines.push_back({plane.corners[k],plane.corners[(k+1)%4],plane.handle});
  }
  // 中央方框仅用于整体等比缩放。
  if(settings.tool==GizmoTool::scale&&enabled[0]&&enabled[1]&&enabled[2]) {const auto p=out.center;const float r=6*dpi;const ir::Vec2 a[]={{p.x-r,p.y-r},{p.x+r,p.y-r},{p.x+r,p.y+r},{p.x-r,p.y+r}};for(int k=0;k<4;++k) out.lines.push_back({a[k],a[(k+1)%4],3});}
  out.rotation_center=settings.tool==GizmoTool::rotate&&!out.lines.empty();
  return out;
}
void GizmoDrag::object(const runtime::Target &target,const runtime::TransformValues &value,const ir::Transform &loaded,const ir::Transform &current) {
  joint=skin=-1;frame_={};frame_.has_edit_frame=target.has_edit_frame;frame_.edit_frame=target.edit_frame;frame_.translation_frame=target.translation_frame;frame_.base_rotation_degrees=target.base_rotation_degrees;frame_.rotation_order=target.rotation_order;
  initial_=transform=value;loaded_=loaded;base_world_=world=current;prefix_=current*ir::inverse(runtime::parameter_transform(value,frame_,loaded)*loaded);enabled={true,true,true};
}
void GizmoDrag::bone(const runtime::Skin &skeleton,int bone,const std::vector<runtime::JointPose> &input,const std::vector<runtime::JointPose> &evaluated,const ir::Transform &current) {
  skin_=skeleton;skin_.weights.clear();proxy_skin_=skeleton;joint=bone;input_=poses=input;effective_=solved_=evaluated;base_world_=world=current;
}
std::vector<runtime::JointPose> GizmoDrag::effective(const std::vector<runtime::JointPose> &input) const {
  auto result=effective_;
  for(int c=0;c<9;++c) {const float delta=runtime::joint_value(input[joint],c)-runtime::joint_value(input_[joint],c);auto &p=result[joint];component(c<3?p.translation_cm:c<6?p.rotation_degrees:p.scale,c%3)+=delta;}
  return result;
}
ir::Vec3 GizmoDrag::pivot() const {
  if(joint>=0) return world.point(runtime::joint_point(skin_,solved_,joint));
  const auto frame=frame_.has_edit_frame?frame_.edit_frame:ir::Transform::translate(loaded_.point({}));
  return (world*ir::inverse(loaded_)).point(frame.point({}));
}
ir::Transform GizmoDrag::orientation() const {
  if(joint>=0) {const auto &j=skin_.joints[joint];const auto &p=solved_[joint];runtime::TransformValues rest;rest.rotation_degrees=add(j.orientation_degrees,p.orientation_offset_degrees);
    return runtime::rotation_frame(world)*runtime::joint_orientation(skin_,solved_,joint)*runtime::make_transform(rest);}
  runtime::TransformValues v;v.rotation_degrees=add(transform.rotation_degrees,frame_.base_rotation_degrees);
  return runtime::rotation_frame(prefix_*frame_.edit_frame)*runtime::make_transform(v,frame_.rotation_order);
}
void GizmoDrag::layout(CameraState view,int w,int h,GizmoSettings mode,float dpi) {
  camera=view;width=w;height=h;settings=mode;
  if(mode.tool==GizmoTool::select) {shape={};return;}
  if(joint>=0) {const int offset=mode.tool==GizmoTool::translate?0:mode.tool==GizmoTool::rotate?3:6;for(int c=0;c<3;++c) enabled[c]=runtime::editable_channel(skin_,joint,offset+c);
    // 世界旋转可能需要同时写入多个本地通道，求解器始终保留锁定通道。
    if(mode.tool!=GizmoTool::scale) enabled.fill(enabled[0]||enabled[1]||enabled[2]);}
  shape=gizmo_shape(camera,w,h,pivot(),orientation(),mode,dpi,enabled);
}
bool GizmoDrag::begin(runtime::PosePointer pointer,CameraState view,int w,int h,GizmoSettings mode,float dpi,const ir::Mesh &mesh,const std::vector<ir::Vec3> *skin_source) {
  active=moved=false;event_=0;press=pointer;angle_=previous_angle_=0;layout(view,w,h,mode,dpi);
  initial_pivot_=pivot();initial_orientation_=orientation();
  handle=shape.hit(float(pointer.start_x),float(pointer.start_y),8*dpi);if(handle<0||pointer.cancelled||pointer.modified) return false;
  if(mode.tool==GizmoTool::translate&&handle>=4&&!plane_point(camera,w,h,initial_pivot_,shape.axes[handle-4],float(pointer.start_x),float(pointer.start_y),plane_press_)) return false;
  if(mode.tool==GizmoTool::rotate) start_angle_=previous_angle_=ring_angle(camera,w,h,shape,handle,float(pointer.start_x),float(pointer.start_y));
  proxy={};proxy.positions=mesh.positions;
  const size_t step=std::max(size_t(1),(mesh.triangles.size()+39999)/40000);for(size_t i=0;i<mesh.triangles.size();i+=step) if(mesh.draws(mesh.triangles[i])) proxy.triangles.push_back(mesh.triangles[i]);
  if(joint>=0&&skin_source) {runtime::Skin reduced;build_pose_proxy(proxy_skin_,mesh,*skin_source,reduced,proxy,source_);proxy_skin_=std::move(reduced);proxy.positions=runtime::deform(proxy_skin_,solved_,source_);}
  active=true;return true;
}
// 三个通道的小型最小二乘求解；直接使用现有变换求值，支持六种欧拉顺序、父节点和自身轴心。
void GizmoDrag::solve(const ir::Vec3 &position,const ir::Transform &orientation) {
  const bool rotate=settings.tool==GizmoTool::rotate;const int offset=rotate?3:0;
  auto value=[&](int c){return joint>=0?runtime::joint_value(poses[joint],offset+c):component(rotate?transform.rotation_degrees:transform.translation_cm,c);};
  auto set=[&](int c,float v){if(joint>=0) {
    if(runtime::editable_channel(skin_,joint,offset+c)) {
      const auto &limit=skin_.joints[joint].channels[offset+c];const float initial=runtime::joint_value(input_[joint],offset+c);
      // 导入时已越限的通道允许逐步回到范围；微分探针和还原不能强行截断原姿势。
      if(limit.clamped) v=std::clamp(v,std::min(limit.minimum,initial),std::max(limit.maximum,initial));
      component(rotate?poses[joint].rotation_degrees:poses[joint].translation_cm,c)=v;
    }solved_=effective(poses);
  }else {component(rotate?transform.rotation_degrees:transform.translation_cm,c)=v;world=prefix_*runtime::parameter_transform(transform,frame_,loaded_)*loaded_;}};
  auto residual=[&]{std::array<double,9> r{};if(rotate) {const auto current=this->orientation();for(int i=0;i<3;++i) for(int j=0;j<3;++j) r[i*3+j]=orientation.value[i*4+j]-current.value[i*4+j];}else {const auto d=sub(position,pivot());r[0]=d.x;r[1]=d.y;r[2]=d.z;}return r;};
  auto score=[](const auto &r){double sum=0;for(auto v:r) sum+=v*v;return sum;};
  if(rotate) {
    // 先求精确的欧拉表示，避免 90° 附近雅可比降秩；受锁定／限位影响时再数值求最近结果。
    const auto order=joint>=0?skin_.joints[joint].rotation_order:frame_.rotation_order;
    runtime::TransformValues current;current.rotation_degrees=joint>=0?solved_[joint].rotation_degrees:add(transform.rotation_degrees,frame_.base_rotation_degrees);
    const auto prefix=this->orientation()*ir::inverse(runtime::make_transform(current,order));
    const auto angles=euler_angles(ir::inverse(prefix)*orientation,order,current.rotation_degrees);
    const double before=score(residual());const float saved[]={value(0),value(1),value(2)};
    for(int c=0;c<3;++c) {auto desired=angles;const float v=component(desired,c)-(joint>=0?component(effective_[joint].rotation_degrees,c)-component(input_[joint].rotation_degrees,c):component(frame_.base_rotation_degrees,c));set(c,v);}
    if(score(residual())>before) for(int c=0;c<3;++c) set(c,saved[c]);
  }
  for(int it=0;it<48;++it) {
    const auto r=residual();const double error=score(r);if(error<1e-12) break;double jac[3][9]{};float before[3]{};
    for(int c=0;c<3;++c) {before[c]=value(c);if(joint>=0&&!runtime::editable_channel(skin_,joint,offset+c)) continue;
      set(c,before[c]+.1f);float step=value(c)-before[c];if(std::abs(step)<1e-6f) {set(c,before[c]-.1f);step=value(c)-before[c];}
      if(std::abs(step)>1e-6f) {const auto trial=residual();for(int k=0;k<9;++k) jac[c][k]=(r[k]-trial[k])/step;}set(c,before[c]);}
    double a[3][4]{};for(int c=0;c<3;++c) {for(int d=0;d<3;++d) for(int k=0;k<9;++k) a[c][d]+=jac[c][k]*jac[d][k];a[c][c]+=1e-10;for(int k=0;k<9;++k) a[c][3]+=jac[c][k]*r[k];}
    for(int c=0;c<3;++c) {int p=c;for(int d=c+1;d<3;++d) if(std::abs(a[d][c])>std::abs(a[p][c])) p=d;for(int k=0;k<4;++k) std::swap(a[p][k],a[c][k]);const double div=a[c][c];for(int k=c;k<4;++k) a[c][k]/=div;for(int d=0;d<3;++d) if(d!=c) {const double f=a[d][c];for(int k=c;k<4;++k) a[d][k]-=f*a[c][k];}}
    double max_step=0;for(int c=0;c<3;++c) max_step=std::max(max_step,std::abs(a[c][3]));const double cap=rotate?12.:100.;bool improved=false;
    for(double f:{1.,.5,.25,.125}) {for(int c=0;c<3;++c) set(c,before[c]+float(a[c][3]*std::min(1.,cap/std::max(1e-12,max_step))*f));if(score(residual())<error) {improved=true;break;}}
    if(!improved) {for(int c=0;c<3;++c) set(c,before[c]);break;}
  }
}
bool GizmoDrag::update(runtime::PosePointer pointer) {
  if(!active||pointer.cancelled||!pointer.moved||pointer.revision==event_) return false;event_=pointer.revision;
  const float dx=float(pointer.x-press.start_x),dy=float(pointer.y-press.start_y);
  if(dx==0&&dy==0) {
    // 回到按下位置须精确还原，不能把数值求解的残差当作一次新编辑。
    transform=initial_;world=base_world_;poses=input_;solved_=effective_;angle_=0;previous_angle_=start_angle_;
  } else if(settings.tool==GizmoTool::rotate) {
    // 沿实际旋转圆的切线累计角度，避免背向相机时反向以及跨越 ±180 度跳变。
    const auto axis=shape.axes[handle];const float next=ring_angle(camera,width,height,shape,handle,float(pointer.x),float(pointer.y));angle_+=std::remainder(next-previous_angle_,2*std::numbers::pi_v<float>);previous_angle_=next;
    // DAZ 三轴在 IR 中仍为右手基；上面的圆参数 u × v = axis。
    solve(initial_pivot_,gizmo_rotation(axis,angle_)*initial_orientation_);
  } else if(settings.tool==GizmoTool::translate) {
    if(handle>=4) {
      ir::Vec3 point;if(!plane_point(camera,width,height,initial_pivot_,shape.axes[handle-4],float(pointer.x),float(pointer.y),point)) return false;
      solve(add(initial_pivot_,sub(point,plane_press_)),initial_orientation_);
    } else {
      ir::Vec2 end;if(!project(camera,width,height,add(initial_pivot_,mul(shape.axes[handle],shape.radius)),end)) return false;
      const float x=end.x-shape.center.x,y=end.y-shape.center.y,den=x*x+y*y;if(den<16) return false;
      solve(add(initial_pivot_,mul(shape.axes[handle],shape.radius*(dx*x+dy*y)/den)),initial_orientation_);
    }
  } else {
    float amount=(dx-dy)/shape.pixels;if(handle<3) {ir::Vec2 end;if(!project(camera,width,height,add(initial_pivot_,mul(shape.axes[handle],shape.radius)),end)) return false;const float x=end.x-shape.center.x,y=end.y-shape.center.y;amount=(dx*x+dy*y)/std::max(16.f,x*x+y*y);}
    const float factor=std::exp(std::clamp(amount,-6.f,6.f));
    if(joint>=0) {poses=input_;for(int c=0;c<3;++c) if(handle==3||handle==c) runtime::set_joint_value(skin_,poses,joint,6+c,runtime::joint_value(input_[joint],6+c)*factor);solved_=effective(poses);}
    else {transform=initial_;for(int c=0;c<3;++c) if(handle==3||handle==c) component(transform.scale,c)*=factor;world=prefix_*runtime::parameter_transform(transform,frame_,loaded_)*loaded_;}
  }
  if(joint>=0) proxy.positions=runtime::deform(proxy_skin_,solved_,source_);
  moved=true;return true;
}
bool GizmoDrag::changed() const {return joint>=0?poses!=input_:transform.translation_cm!=initial_.translation_cm||transform.rotation_degrees!=initial_.rotation_degrees||transform.scale!=initial_.scale;}
}
