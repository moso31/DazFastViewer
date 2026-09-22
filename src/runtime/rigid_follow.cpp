#include "runtime/rigid_follow.h"
#include <cmath>
#include <stdexcept>

namespace dfv::runtime {
ir::Transform fit_rigid(const std::vector<ir::Vec3> &a,const std::vector<ir::Vec3> &b,bool rotate) {
  if(a.size()!=b.size()||a.empty()) throw std::runtime_error("刚性跟随参考顶点无效");
  double ac[3]{},bc[3]{};
  for(size_t i=0;i<a.size();++i) {const double x[]={a[i].x,a[i].y,a[i].z},y[]={b[i].x,b[i].y,b[i].z};for(int k=0;k<3;++k) {ac[k]+=x[k]/a.size();bc[k]+=y[k]/a.size();}}
  ir::Transform result;
  if(rotate&&a.size()>=3) {
    double h[3][3]{};
    for(size_t i=0;i<a.size();++i) {const double x[]={a[i].x-ac[0],a[i].y-ac[1],a[i].z-ac[2]},y[]={b[i].x-bc[0],b[i].y-bc[1],b[i].z-bc[2]};for(int r=0;r<3;++r) for(int c=0;c<3;++c) h[r][c]+=x[r]*y[c];}
    // Horn 四元数形式；Jacobi 对称特征分解也能处理平面上的参考组。
    const double trace=h[0][0]+h[1][1]+h[2][2];double n[4][4]{},v[4][4]{};n[0][0]=trace;
    n[0][1]=n[1][0]=h[1][2]-h[2][1];n[0][2]=n[2][0]=h[2][0]-h[0][2];n[0][3]=n[3][0]=h[0][1]-h[1][0];
    for(int r=0;r<3;++r) for(int c=0;c<3;++c) n[r+1][c+1]=h[r][c]+h[c][r]-(r==c?trace:0);
    for(int i=0;i<4;++i) v[i][i]=1;
    for(int step=0;step<64;++step) {
      int p=0,q=1;for(int r=0;r<4;++r) for(int c=r+1;c<4;++c) if(std::abs(n[r][c])>std::abs(n[p][q])) {p=r;q=c;}
      if(std::abs(n[p][q])<1e-18) break;
      const double angle=.5*std::atan2(2*n[p][q],n[q][q]-n[p][p]),cs=std::cos(angle),sn=std::sin(angle),pp=n[p][p],qq=n[q][q],pq=n[p][q];
      for(int k=0;k<4;++k) if(k!=p&&k!=q) {const double x=n[k][p],y=n[k][q];n[k][p]=n[p][k]=cs*x-sn*y;n[k][q]=n[q][k]=sn*x+cs*y;}
      n[p][p]=cs*cs*pp-2*cs*sn*pq+sn*sn*qq;n[q][q]=sn*sn*pp+2*cs*sn*pq+cs*cs*qq;n[p][q]=n[q][p]=0;
      for(int k=0;k<4;++k) {const double x=v[k][p],y=v[k][q];v[k][p]=cs*x-sn*y;v[k][q]=sn*x+cs*y;}
    }
    int best=0;for(int k=1;k<4;++k) if(n[k][k]>n[best][best]) best=k;
    const double w=v[0][best],x=v[1][best],y=v[2][best],z=v[3][best];
    result.value={float(1-2*(y*y+z*z)),float(2*(x*y-w*z)),float(2*(x*z+w*y)),0,float(2*(x*y+w*z)),float(1-2*(x*x+z*z)),float(2*(y*z-w*x)),0,float(2*(x*z-w*y)),float(2*(y*z+w*x)),float(1-2*(x*x+y*y)),0};
  }
  for(int r=0;r<3;++r) result.value[r*4+3]=float(bc[r]-result.value[r*4]*ac[0]-result.value[r*4+1]*ac[1]-result.value[r*4+2]*ac[2]);
  return result;
}
}
