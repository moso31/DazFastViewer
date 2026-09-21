#include "viewport/overlay.h"

namespace dfv {
void HoverOverlay::release() {for(auto id:lists_) glDeleteLists(id,1);for(const auto &instance:parts_) for(const auto &[joint,part]:instance) glDeleteLists(part.first,1);lists_.clear();parts_.clear();triangle_counts_.clear();}
void HoverOverlay::update(const ir::Scene &scene,const std::vector<runtime::JointRegions> &regions) {
  release();
  for(size_t i=0;i<scene.instances.size();++i) {const auto &instance=scene.instances[i];
    const auto id=glGenLists(1);lists_.push_back(id);glNewList(id,GL_COMPILE);glBegin(GL_TRIANGLES);
    const auto &mesh=scene.meshes[instance.mesh];
    auto triangle=[&](const ir::Triangle &face) {for(auto v:face.vertices) {const auto p=instance.transform.point(mesh.positions[v]);glVertex3f(p.x,p.y,p.z);}};
    if(instance.visible) for(const auto &face:mesh.triangles) triangle(face);
    glEnd();glEndList();
    triangle_counts_.push_back(mesh.triangles.size());parts_.emplace_back();
    std::map<int,std::vector<size_t>> by_joint;
    if(instance.visible&&i<regions.size()) for(size_t t=0;t<regions[i].detail.size();++t) {
      const auto &region=regions[i];const int detail=region.detail[t];if(detail>=0) by_joint[detail].push_back(t);
      if(region.head>=0&&region.body[t]==region.head&&detail!=region.head) by_joint[region.head].push_back(t);
    }
    for(const auto &[joint,faces]:by_joint) {
      const auto list=glGenLists(1);parts_.back()[joint]={list,faces.size()};glNewList(list,GL_COMPILE);glBegin(GL_TRIANGLES);
      for(auto t:faces) triangle(mesh.triangles.at(t));glEnd();glEndList();
    }
  }
}
size_t HoverOverlay::triangle_count(int hovered,int joint) const {
  if(hovered<0||size_t(hovered)>=lists_.size()) return 0;
  if(joint<0) return triangle_counts_[size_t(hovered)];const auto found=parts_[size_t(hovered)].find(joint);return found==parts_[size_t(hovered)].end()?0:found->second.second;
}
void HoverOverlay::draw(const CameraState &camera,int width,int height,int hovered,int joint) {
  if(hovered<0||size_t(hovered)>=lists_.size()) return;
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
  for(auto list:lists_) glCallList(list);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);glDepthFunc(GL_EQUAL);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(1.f,.88f,.32f,.22f);glCallList(highlight);
  glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
}
