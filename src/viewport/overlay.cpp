#include "viewport/overlay.h"

namespace dfv {
void HoverOverlay::release() {for(auto id:lists_) if(id) glDeleteLists(id,1);for(const auto &instance:parts_) for(const auto &[joint,part]:instance) glDeleteLists(part.first,1);lists_.clear();parts_.clear();triangle_counts_.clear();transforms_.clear();visible_.clear();}
void HoverOverlay::update(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions) {
  release();
  const auto size=scene.instances.size();lists_.resize(size);parts_.resize(size);triangle_counts_.resize(size);transforms_.resize(size);visible_.resize(size);
  for(size_t i=0;i<size;++i) {transforms_[i]=scene.instances[i].transform;visible_[i]=scene.instances[i].visible;rebuild(scene,regions,i);}
}
void HoverOverlay::rebuild(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,size_t i) {
    const auto &instance=scene.instances[i];if(lists_[i]) glDeleteLists(lists_[i],1);
    for(const auto &[joint,part]:parts_[i]) glDeleteLists(part.first,1);parts_[i].clear();
    const auto id=glGenLists(1);lists_[i]=id;glNewList(id,GL_COMPILE);glBegin(GL_TRIANGLES);
    const auto &mesh=scene.meshes[instance.mesh];
    auto triangle=[&](const ir::Triangle &face) {for(auto v:face.vertices) {const auto p=mesh.positions[v];glVertex3f(p.x,p.y,p.z);}};
    for(const auto &face:mesh.triangles) triangle(face);
    glEnd();glEndList();
    triangle_counts_[i]=mesh.triangles.size();
    std::map<int,std::vector<size_t>> by_joint;
    if(i<regions.size()) for(size_t t=0;t<regions[i].detail.size();++t) {
      const auto &region=regions[i];const int detail=region.detail[t];if(detail>=0) by_joint[detail].push_back(t);
      if(region.head>=0&&region.body[t]==region.head&&detail!=region.head) by_joint[region.head].push_back(t);
    }
    for(const auto &[joint,faces]:by_joint) {
      const auto list=glGenLists(1);parts_[i][joint]={list,faces.size()};glNewList(list,GL_COMPILE);glBegin(GL_TRIANGLES);
      for(auto t:faces) triangle(mesh.triangles.at(t));glEnd();glEndList();
    }
}
void HoverOverlay::apply(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions,const ir::Delta &delta) {
  for(const auto &e:delta.meshes) for(size_t i=0;i<scene.instances.size();++i) if(scene.instances[i].mesh==e.index) rebuild(scene,regions,i);
  for(const auto &e:delta.instances) transforms_.at(e.index)=e.transform;
  for(const auto &e:delta.visibility) visible_.at(e.index)=e.visible;
}
size_t HoverOverlay::triangle_count(int hovered,int joint) const {
  if(hovered<0||size_t(hovered)>=lists_.size()) return 0;
  if(joint<0) return triangle_counts_[size_t(hovered)];const auto found=parts_[size_t(hovered)].find(joint);return found==parts_[size_t(hovered)].end()?0:found->second.second;
}
void HoverOverlay::draw(const CameraState &camera,int width,int height,int hovered,int joint) {
  if(hovered<0||size_t(hovered)>=lists_.size()||!visible_[size_t(hovered)]) return;
  auto highlight=lists_[size_t(hovered)];
  if(joint>=0) {const auto found=parts_[size_t(hovered)].find(joint);if(found==parts_[size_t(hovered)].end()) return;highlight=found->second.first;}
  glPushAttrib(GL_ALL_ATTRIB_BITS);glUseProgram(0);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
  const double e=std::tan(.4)*.01,x=e*std::max(1.,double(width)/height),y=e*std::max(1.,double(height)/width);
  glFrustum(-x,x,-y,y,.01,10000);
  const auto m=camera.matrix();const float view[]={m[0],m[1],-m[2],0,m[4],m[5],-m[6],0,m[8],m[9],-m[10],0,
    -(m[0]*m[3]+m[4]*m[7]+m[8]*m[11]),-(m[1]*m[3]+m[5]*m[7]+m[9]*m[11]),m[2]*m[3]+m[6]*m[7]+m[10]*m[11],1};
  glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadMatrixf(view);
  glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
  auto draw_instance=[&](size_t i,GLuint list) {const auto &m=transforms_[i].value;const float matrix[]={m[0],m[4],m[8],0,m[1],m[5],m[9],0,m[2],m[6],m[10],0,m[3],m[7],m[11],1};glPushMatrix();glMultMatrixf(matrix);glCallList(list);glPopMatrix();};
  for(size_t i=0;i<lists_.size();++i) if(visible_[i]) draw_instance(i,lists_[i]);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);glDepthFunc(GL_EQUAL);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(1.f,.88f,.32f,.22f);draw_instance(size_t(hovered),highlight);
  glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
}
