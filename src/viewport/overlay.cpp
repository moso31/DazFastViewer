#include "viewport/overlay.h"
#include <set>

namespace dfv {
void HoverOverlay::draw_gizmo(const editor::GizmoShape &shape,int width,int height,int active,float dpi) {
  if(shape.lines.empty()) return;
  glPushAttrib(GL_ALL_ATTRIB_BITS);glUseProgram(0);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,height,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
  const float colors[][3]={{1,.3f,.28f},{.38f,.9f,.38f},{.3f,.6f,1},{.88f,.9f,.94f}};
  for(const auto &plane:shape.planes) {const auto *c=colors[plane.handle-4];if(plane.handle==active) glColor4f(1,.86f,.2f,.55f);else glColor4f(c[0],c[1],c[2],.3f);glBegin(GL_QUADS);for(auto p:plane.corners) glVertex2f(p.x,p.y);glEnd();}
  for(int pass=0;pass<2;++pass) {glLineWidth((pass?2.5f:5.5f)*dpi);glBegin(GL_LINES);for(const auto &line:shape.lines) {const auto *c=colors[line.handle>=4?line.handle-4:std::clamp(line.handle,0,3)];if(!pass) glColor4f(.03f,.04f,.06f,.9f);else if(line.handle==active) glColor4f(1,.86f,.2f,1);else glColor4f(c[0],c[1],c[2],1);glVertex2f(line.a.x,line.a.y);glVertex2f(line.b.x,line.b.y);}glEnd();}
  if(shape.rotation_center) {
    // 在旋转环之上绘制屏幕等大的轴心菱形；不占用任何手柄编号或命中区域。
    for(int pass=0;pass<2;++pass) {const float r=(pass?3.5f:5.5f)*dpi;const auto p=shape.center;
      if(pass) glColor4f(.95f,.96f,1,1);else glColor4f(.03f,.04f,.06f,1);
      glBegin(GL_QUADS);glVertex2f(p.x,p.y-r);glVertex2f(p.x+r,p.y);glVertex2f(p.x,p.y+r);glVertex2f(p.x-r,p.y);glEnd();}
  }
  glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
void HoverOverlay::draw_pose(const CameraState &camera,int width,int height,const ir::Mesh &proxy,const ir::Transform &world,
  const std::vector<std::pair<ir::Vec3,ir::Vec3>> &bones,ir::Vec3 goal,const std::vector<uint32_t> &excluded) {
  glPushAttrib(GL_ALL_ATTRIB_BITS);glUseProgram(0);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(.055f,.065f,.08f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);
  glMatrixMode(GL_PROJECTION);glPushMatrix();projection(camera,width,height,&proxy,&world);
  const auto m=camera.matrix();const float view[]={m[0],m[1],-m[2],0,m[4],m[5],-m[6],0,m[8],m[9],-m[10],0,
    -(m[0]*m[3]+m[4]*m[7]+m[8]*m[11]),-(m[1]*m[3]+m[5]*m[7]+m[9]*m[11]),m[2]*m[3]+m[6]*m[7]+m[10]*m[11],1};
  glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadMatrixf(view);
  // 背景使用已缓存的基础网格，包含其他角色和场景，GPU 负责视锥裁切。
  // 活动角色及其穿戴物由新代理替代，防止把编辑前姿势叠在新姿势上。
  glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_NORMALIZE);glDisable(GL_COLOR_MATERIAL);glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
  const GLfloat ambient[]={.35f,.35f,.35f,1},diffuse[]={.7f,.7f,.7f,1},direction[]={.3f,-.5f,.8f,0},material[]={.72f,.77f,.82f,1};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);glLightfv(GL_LIGHT0,GL_POSITION,direction);glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,material);
  for(size_t i=0;i<lists_.size();++i)if(visible_[i]&&std::find(excluded.begin(),excluded.end(),uint32_t(i))==excluded.end())draw_instance(i,lists_[i]);
  glDisable(GL_LIGHTING);glBegin(GL_TRIANGLES);
  for(const auto &t:proxy.triangles) {
    const auto a=world.point(proxy.positions[t.vertices[0]]),b=world.point(proxy.positions[t.vertices[1]]),c=world.point(proxy.positions[t.vertices[2]]);
    const auto n=normalized(cross({b.x-a.x,b.y-a.y,b.z-a.z},{c.x-a.x,c.y-a.y,c.z-a.z}));const float shade=.35f+.55f*std::abs(n.x*.3f+n.y*-.5f+n.z*.8f);glColor3f(shade*.72f,shade*.82f,shade);
    for(auto p:{a,b,c}) glVertex3f(p.x,p.y,p.z);
  }
  glEnd();glDisable(GL_DEPTH_TEST);glLineWidth(3);glColor3f(1,.75f,.18f);glBegin(GL_LINES);for(const auto &[a,b]:bones) {glVertex3f(a.x,a.y,a.z);glVertex3f(b.x,b.y,b.z);}glEnd();
  if(!proxy.positions.empty()||!bones.empty()){glPointSize(9);glColor3f(.25f,1,.45f);glBegin(GL_POINTS);glVertex3f(goal.x,goal.y,goal.z);glEnd();}
  glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
void HoverOverlay::release() {std::set<GLuint> unique;for(auto id:lists_) if(id) unique.insert(id);for(const auto &instance:parts_) for(const auto &[joint,part]:instance) unique.insert(part.first);for(auto id:unique) glDeleteLists(id,1);lists_.clear();parts_.clear();triangle_counts_.clear();transforms_.clear();visible_.clear();bounds_.clear();}
void HoverOverlay::update(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions) {
  release();
  const auto size=scene.instances.size();lists_.resize(size);parts_.resize(size);triangle_counts_.resize(size);transforms_.resize(size);visible_.resize(size);bounds_.resize(size);
  for(size_t i=0;i<size;++i) {transforms_[i]=scene.instances[i].transform;visible_[i]=scene.instances[i].visible;rebuild(scene,regions,i);}
}
void HoverOverlay::rebuild(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,size_t i) {
    const auto &instance=scene.instances[i];
    bounds_[i]={};for(auto p:scene.meshes[instance.mesh].positions)bounds_[i].add(p);
    if(instance.prototype>=0) {const auto p=size_t(instance.prototype);lists_[i]=lists_[p];parts_[i]=parts_[p];triangle_counts_[i]=triangle_counts_[p];return;}
    if(lists_[i]) glDeleteLists(lists_[i],1);
    for(const auto &[joint,part]:parts_[i]) glDeleteLists(part.first,1);parts_[i].clear();
    const auto id=glGenLists(1);lists_[i]=id;glNewList(id,GL_COMPILE);glBegin(GL_TRIANGLES);
    const auto &mesh=scene.meshes[instance.mesh];
    auto triangle=[&](const ir::Triangle &face) {const auto a=mesh.positions[face.vertices[0]],b=mesh.positions[face.vertices[1]],c=mesh.positions[face.vertices[2]];const auto n=normalized(cross({b.x-a.x,b.y-a.y,b.z-a.z},{c.x-a.x,c.y-a.y,c.z-a.z}));glNormal3f(n.x,n.y,n.z);for(auto v:face.vertices) {const auto p=mesh.positions[v];glVertex3f(p.x,p.y,p.z);}};
    size_t count=0;for(const auto &face:mesh.triangles) if(mesh.draws(face)) {triangle(face);++count;}
    glEnd();glEndList();
    triangle_counts_[i]=count;
    std::map<int,std::vector<size_t>> by_joint;
    if(i<regions.size()) for(size_t t=0;t<regions[i].detail.size();++t) {
      if(!mesh.draws(mesh.triangles[t])) continue;
      const auto &region=regions[i];const int detail=region.detail[t];if(detail>=0) by_joint[detail].push_back(t);
      if(region.head>=0&&region.body[t]==region.head&&detail!=region.head) by_joint[region.head].push_back(t);
    }
    for(const auto &[joint,faces]:by_joint) {
      const auto list=glGenLists(1);parts_[i][joint]={list,faces.size()};glNewList(list,GL_COMPILE);glBegin(GL_TRIANGLES);
      for(auto t:faces) triangle(mesh.triangles.at(t));glEnd();glEndList();
    }
    // 负键专用于材质槽，与骨骼区域共用缓存和失效机制。
    std::map<uint32_t,std::vector<size_t>> by_surface,curves;
    for(size_t t=0;t<mesh.triangles.size();++t)if(mesh.draws(mesh.triangles[t]))by_surface[mesh.triangles[t].material_slot].push_back(t);
    for(size_t c=0;c<mesh.curves.size();++c){curves[mesh.curves[c].material_slot].push_back(c);by_surface.try_emplace(mesh.curves[c].material_slot);}
    for(const auto &[slot,faces]:by_surface){const auto list=glGenLists(1);size_t count=faces.size();glNewList(list,GL_COMPILE);glBegin(GL_TRIANGLES);for(auto t:faces)triangle(mesh.triangles[t]);glEnd();glLineWidth(2);glBegin(GL_LINES);for(auto c:curves[slot]){const auto &vertices=mesh.curves[c].vertices;for(size_t v=1;v<vertices.size();++v){for(auto index:{vertices[v-1],vertices[v]}){const auto p=mesh.positions[index];glVertex3f(p.x,p.y,p.z);}++count;}}glEnd();glEndList();parts_[i][-int(slot)-2]={list,count};}
}
void HoverOverlay::apply(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,const ir::Delta &delta) {
  for(const auto &e:delta.meshes) for(size_t i=0;i<scene.instances.size();++i) if(scene.instances[i].mesh==e.index) rebuild(scene,regions,i);
  for(const auto &e:delta.instances) transforms_.at(e.index)=e.transform;
  for(const auto &e:delta.visibility) visible_.at(e.index)=e.visible;
}
void HoverOverlay::prepare_delta(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,const ir::Delta &delta) {
  const auto n=scene.instances.size();lists_.resize(n);parts_.resize(n);triangle_counts_.resize(n);transforms_.resize(n);visible_.resize(n);bounds_.resize(n);
  apply(scene,regions,delta);
}
void HoverOverlay::swap_delta(HoverOverlay &p,const ir::Scene &scene,const ir::Delta &delta) {
  for(const auto &e:delta.meshes)for(size_t i=0;i<scene.instances.size();++i)if(scene.instances[i].mesh==e.index){
    std::swap(lists_[i],p.lists_[i]);parts_[i].swap(p.parts_[i]);std::swap(triangle_counts_[i],p.triangle_counts_[i]);std::swap(bounds_[i],p.bounds_[i]);
  }
  for(const auto &e:delta.instances)transforms_[e.index]=e.transform;
}
size_t HoverOverlay::triangle_count(int hovered,int joint) const {
  if(hovered<0||size_t(hovered)>=lists_.size()) return 0;
  if(joint<0) return triangle_counts_[size_t(hovered)];const auto found=parts_[size_t(hovered)].find(joint);return found==parts_[size_t(hovered)].end()?0:found->second.second;
}
size_t HoverOverlay::surface_count(size_t instance,size_t slot) const {if(instance>=parts_.size()||!visible_[instance])return 0;auto found=parts_[instance].find(-int(slot)-2);return found==parts_[instance].end()?0:found->second.second;}
void HoverOverlay::draw(const CameraState &camera,int width,int height,int hovered,int joint,const std::vector<uint32_t> *members,const std::vector<std::pair<size_t,size_t>> *surfaces) {
  if(surfaces){if(surfaces->empty())return;hovered=int(surfaces->front().first);joint=-1;members=nullptr;}
  if(hovered<0||size_t(hovered)>=lists_.size()||(!surfaces&&!members&&!visible_[size_t(hovered)])) return;
  auto highlight=lists_[size_t(hovered)];
  if(joint>=0) {const auto found=parts_[size_t(hovered)].find(joint);if(found==parts_[size_t(hovered)].end()) return;highlight=found->second.first;}
  glPushAttrib(GL_ALL_ATTRIB_BITS);glUseProgram(0);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
  projection(camera,width,height);
  const auto m=camera.matrix();const float view[]={m[0],m[1],-m[2],0,m[4],m[5],-m[6],0,m[8],m[9],-m[10],0,
    -(m[0]*m[3]+m[4]*m[7]+m[8]*m[11]),-(m[1]*m[3]+m[5]*m[7]+m[9]*m[11]),m[2]*m[3]+m[6]*m[7]+m[10]*m[11],1};
  glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadMatrixf(view);
  glDepthMask(GL_FALSE);glStencilMask(0xff);glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);glDisable(GL_DEPTH_TEST);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
  glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,1,0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
  // 选区始终透过衣服和其他遮挡物显示；只绘制选中几何的遮罩并合成一次黄色，
  // 避免深度竞争，也避免前后表面及重叠三角形反复混色。
  if(surfaces){for(const auto &[i,slot]:*surfaces)if(i<parts_.size()&&visible_[i]){auto found=parts_[i].find(-int(slot)-2);if(found!=parts_[i].end())draw_instance(i,found->second.first);}}
  else for(size_t i=0;i<lists_.size();++i)if(visible_[i]) {
    const bool selected=members?std::find(members->begin(),members->end(),uint32_t(i))!=members->end():i==size_t(hovered);
    if(selected)draw_instance(i,joint>=0?highlight:lists_[i]);
  }
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);glDisable(GL_DEPTH_TEST);glStencilFunc(GL_EQUAL,1,0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(1.f,.88f,.32f,.22f);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();glBegin(GL_QUADS);glVertex2f(-1,-1);glVertex2f(1,-1);glVertex2f(1,1);glVertex2f(-1,1);glEnd();
  glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
void HoverOverlay::draw_instance(size_t i,GLuint list) const {const auto &m=transforms_[i].value;const float matrix[]={m[0],m[4],m[8],0,m[1],m[5],m[9],0,m[2],m[6],m[10],0,m[3],m[7],m[11],1};glPushMatrix();glMultMatrixf(matrix);glCallList(list);glPopMatrix();}
void HoverOverlay::projection(const CameraState &camera,int width,int height,const ir::Mesh *proxy,const ir::Transform *world) const {
  const auto eye=camera.eye(),forward=normalized(camera.target-eye);double closest=1e30,farthest=0;bool crossing=false;
  auto add=[&](const ir::Bounds &b,const ir::Transform &transform){if(b.empty)return;double lo=1e30,hi=-1e30;
    for(float x:{b.minimum.x,b.maximum.x})for(float y:{b.minimum.y,b.maximum.y})for(float z:{b.minimum.z,b.maximum.z}) {
      const auto p=transform.point({x,y,z});const double d=(p.x-eye.x)*forward.x+(p.y-eye.y)*forward.y+(p.z-eye.z)*forward.z;lo=std::min(lo,d);hi=std::max(hi,d);
    }
    if(hi<=0)return;farthest=std::max(farthest,hi);if(lo<=0)crossing=true;else closest=std::min(closest,lo);
  };
  for(size_t i=0;i<bounds_.size();++i)if(visible_[i])add(bounds_[i],transforms_[i]);
  if(proxy&&world){ir::Bounds b;for(auto p:proxy->positions)b.add(p);add(b,*world);}
  // 按实际几何收紧裁切范围，避免原来的极小 near / 极大 far 浪费深度精度。
  double near=closest<1e30?closest*.5:std::max(.00001,double(camera.distance)*.005);
  if(crossing)near=std::min(near,std::max(.00001,std::min(.01,double(camera.distance)*.005)));
  near=std::max(.00001,near);const double far=std::max(near*2,farthest*1.1);
  const double e=std::tan(.4)*near,x=e*std::max(1.,double(width)/height),y=e*std::max(1.,double(height)/width);glLoadIdentity();glFrustum(-x,x,-y,y,near,far);
}
}
