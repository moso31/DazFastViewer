#include "water/model.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>

namespace dfv::water {
namespace {
struct V {
  double x=0,y=0,z=0;
  V operator+(V b)const{return {x+b.x,y+b.y,z+b.z};}
  V operator-(V b)const{return {x-b.x,y-b.y,z-b.z};}
  V operator*(double s)const{return {x*s,y*s,z*s};}
};
double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct Box {
  V lo{1e100,1e100,1e100},hi{-1e100,-1e100,-1e100};
  void add(V p){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
  bool overlaps(const Box &b)const{return lo.x<=b.hi.x&&hi.x>=b.lo.x&&lo.y<=b.hi.y&&hi.y>=b.lo.y&&lo.z<=b.hi.z&&hi.z>=b.lo.z;}
  double distance2(V p)const{double x=std::max({lo.x-p.x,0.,p.x-hi.x}),y=std::max({lo.y-p.y,0.,p.y-hi.y}),z=std::max({lo.z-p.z,0.,p.z-hi.z});return x*x+y*y+z*z;}
};
struct Face {V a,b,c;Box box;};
double distance2(V p,const Face &f){
  const auto ab=f.b-f.a,ac=f.c-f.a,ap=p-f.a;double d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return dot(ap,ap);
  const auto bp=p-f.b;double d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return dot(bp,bp);
  double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0){auto v=ap-ab*(d1/(d1-d3));return dot(v,v);}
  auto cp=p-f.c;double d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return dot(cp,cp);
  double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0){auto v=ap-ac*(d2/(d2-d6));return dot(v,v);}
  double va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){auto v=bp-(f.c-f.b)*((d4-d3)/((d4-d3)+(d5-d6)));return dot(v,v);}
  const auto n=cross(ab,ac);const double nn=dot(n,n);return nn>1e-24?dot(ap,n)*dot(ap,n)/nn:std::min({dot(ap,ap),dot(bp,bp),dot(cp,cp)});
}
// Vertical intersections are merged at shared triangle edges before parity is used.
bool vertical(const Face &f,V p,double &z){
  const double ax=f.b.x-f.a.x,ay=f.b.y-f.a.y,bx=f.c.x-f.a.x,by=f.c.y-f.a.y,det=ax*by-ay*bx;
  if(std::abs(det)<1e-15)return false;const double px=p.x-f.a.x,py=p.y-f.a.y,u=(px*by-py*bx)/det,v=(ax*py-ay*px)/det;
  if(u< -1e-9||v< -1e-9||u+v>1+1e-9)return false;z=f.a.z+u*(f.b.z-f.a.z)+v*(f.c.z-f.a.z);return true;
}
class Collider {
  struct Branch {Box box;int begin=0,end=0,left=-1,right=-1;};
  std::vector<Face> faces;std::vector<Branch> branches;
  int build(int begin,int end){Branch b;b.begin=begin;b.end=end;for(int i=begin;i<end;++i){b.box.add(faces[i].box.lo);b.box.add(faces[i].box.hi);}const int index=int(branches.size());branches.push_back(b);
    if(end-begin>12){const auto e=b.box.hi-b.box.lo;int axis=e.x>e.y?(e.x>e.z?0:2):(e.y>e.z?1:2);auto coord=[axis](const Face &f){return axis==0?f.box.lo.x+f.box.hi.x:axis==1?f.box.lo.y+f.box.hi.y:f.box.lo.z+f.box.hi.z;};int middle=(begin+end)/2;std::nth_element(faces.begin()+begin,faces.begin()+middle,faces.begin()+end,[&](const auto &a,const auto &c){return coord(a)<coord(c);});int left=build(begin,middle),right=build(middle,end);branches[index].left=left;branches[index].right=right;}return index;}
public:
  bool volume=false;
  explicit Collider(const Obstacle &o):volume(o.volume){
    faces.reserve(o.mesh.triangles.size());branches.reserve(o.mesh.triangles.size()/3+1);
    std::map<std::tuple<int64_t,int64_t,int64_t>,uint32_t> welded;std::map<std::pair<uint32_t,uint32_t>,int> edges;
    for(const auto &t:o.mesh.triangles){if(!o.mesh.draws(t))continue;Face f;V *points[]={&f.a,&f.b,&f.c};uint32_t ids[3];
      for(int k=0;k<3;++k){auto p=o.transform.point(o.mesh.positions.at(t.vertices[k]));*points[k]={p.x,p.y,p.z};f.box.add(*points[k]);if(volume)ids[k]=welded.try_emplace({std::llround(p.x*100000.),std::llround(p.y*100000.),std::llround(p.z*100000.)},uint32_t(welded.size())).first->second;}
      const auto normal=cross(f.b-f.a,f.c-f.a);if(dot(normal,normal)<1e-20)continue;
      if(volume)for(int k=0;k<3;++k)++edges[std::minmax(ids[k],ids[(k+1)%3])];faces.push_back(f);
    }
    if(faces.empty())throw std::runtime_error("海岸线对象没有有效三角形："+o.id);
    if(volume&&std::any_of(edges.begin(),edges.end(),[](const auto &e){return e.second!=2;}))throw std::runtime_error("排水体积必须使用封闭网格，请改为表面交界或指定封闭代理："+o.id);
    build(0,int(faces.size()));
  }
  bool near(const Box &box)const{if(!box.overlaps(branches.front().box))return false;std::vector<int> stack{0};while(!stack.empty()){int i=stack.back();stack.pop_back();auto &b=branches[i];if(!box.overlaps(b.box))continue;if(b.left>=0){stack.push_back(b.left);stack.push_back(b.right);}else for(int j=b.begin;j<b.end;++j)if(box.overlaps(faces[j].box))return true;}return false;}
  double clearance(V p,double cap)const{
    const auto &root=branches.front().box;const bool ray=p.x>=root.lo.x&&p.x<=root.hi.x&&p.y>=root.lo.y&&p.y<=root.hi.y;
    if(!ray&&(!volume||root.distance2(p)>cap*cap))return cap;
    if(!volume){double highest=-1e100;std::vector<int> stack{0};while(!stack.empty()){int i=stack.back();stack.pop_back();const auto &b=branches[i];if(p.x<b.box.lo.x||p.x>b.box.hi.x||p.y<b.box.lo.y||p.y>b.box.hi.y)continue;if(b.left>=0){stack.push_back(b.left);stack.push_back(b.right);}else for(int j=b.begin;j<b.end;++j){double z;if(vertical(faces[j],p,z))highest=std::max(highest,z);}}return highest== -1e100?cap:std::clamp(p.z-highest,-cap,cap);}
    double nearest=cap*cap;std::vector<double> hits;std::vector<int> stack{0};
    while(!stack.empty()){int i=stack.back();stack.pop_back();const auto &b=branches[i];const bool ray=p.x>=b.box.lo.x&&p.x<=b.box.hi.x&&p.y>=b.box.lo.y&&p.y<=b.box.hi.y&&p.z<=b.box.hi.z;
      if(!ray&&b.box.distance2(p)>nearest)continue;if(b.left>=0){stack.push_back(b.left);stack.push_back(b.right);}else for(int j=b.begin;j<b.end;++j){const auto &f=faces[j];if(f.box.distance2(p)<=nearest)nearest=std::min(nearest,distance2(p,f));double z;if(ray&&vertical(f,p,z)&&z>p.z+1e-8)hits.push_back(z);}}
    std::sort(hits.begin(),hits.end());hits.erase(std::unique(hits.begin(),hits.end(),[](double a,double b){return std::abs(a-b)<1e-6;}),hits.end());return (hits.size()%2?-1:1)*std::sqrt(std::max(0.,nearest));
  }
};
using Key=std::tuple<int,uint32_t,uint32_t>;
Key key(const Cell &c){return {c.level,c.x,c.y};}
struct Rect {double x,y,w,h;};
Rect rect(const Config &c,const Cell &v){const double n=std::ldexp(1.,v.level),w=c.width/n,h=c.length/n;return {-c.width*.5+v.x*w,-c.length*.5+v.y*h,w,h};}
std::array<std::array<double,2>,5> points(Rect r){return {{{r.x,r.y},{r.x+r.w,r.y},{r.x+r.w,r.y+r.h},{r.x,r.y+r.h},{r.x+r.w*.5,r.y+r.h*.5}}};}
}
std::shared_ptr<const Cache> calculate(const Water &w,const std::vector<Obstacle> &objects,uint64_t stamp,const Progress &progress){
  validate(w.config);auto cache=std::make_shared<Cache>();cache->stamp=stamp;cache->objects=objects.size();if(!w.config.coast||objects.empty())return cache;
  std::vector<Collider> colliders;for(const auto &o:objects){if(progress)progress("准备交界对象："+o.id);colliders.emplace_back(o);}
  auto c=w.config;const double band=c.wave_height+c.foam_width;size_t visited=0;struct BudgetExceeded {};
  double cap=std::max(c.foam_width*2,c.precision*2);
  std::function<void(Cell)> visit=[&](Cell cell){
    if((++visited%1024)==0&&progress)progress("计算水体交界："+std::to_string(cache->cells.size())+" 个局部网格");
    if(visited>270000||cache->cells.size()>=50000)throw BudgetExceeded{};
    const auto r=rect(c,cell);auto samples=points(r);bool positive=true,negative=true;
    for(int k=0;k<5;++k){const auto p=wave(c,samples[k][0],samples[k][1]);double value=cap;for(const auto &collider:colliders)value=std::min(value,collider.clearance({p.x,p.y,p.z},cap));cell.clearance[k]=value;positive&=value>=cap*.999;negative&=value<=-cap*.999;}
    Box box;box.add({r.x-band,r.y-band,-band});box.add({r.x+r.w+band,r.y+r.h+band,band});const bool near=std::any_of(colliders.begin(),colliders.end(),[&](const auto &v){return v.near(box);});
    if(positive&&!near)return;
    if(negative&&!near){cache->cells.push_back(cell);return;}
    if(std::max(r.w,r.h)<=c.precision||cell.level>=26){cache->cells.push_back(cell);return;}
    for(uint32_t y=0;y<2;++y)for(uint32_t x=0;x<2;++x)visit({cell.x*2+x,cell.y*2+y,cell.level+1,{}});
  };
  for(;;){try{visit({});break;}catch(const BudgetExceeded &){c.precision*=2;cap=std::max(c.foam_width*2,c.precision*2);visited=0;cache->cells.clear();if(progress)progress("交界细节超出视口预算，调整采样间距为 "+std::to_string(c.precision)+" 米");}}
  cache->sampling_spacing=c.precision;if(c.precision!=w.config.precision)cache->warnings.push_back("交界采样间距已按视口预算调整为 "+std::to_string(c.precision)+" 米；可缩小参与范围以保留更细交界。");
  if(progress)progress("交界计算完成");return cache;
}
struct MeshBudget:std::runtime_error {MeshBudget():std::runtime_error("水面网格超出预算，请降低顶点密度或减少交界区域"){};};
static ir::Mesh mesh_at_density(const Water &water,ir::Vec3 eye,const Progress &progress,double density){
  const auto &c=water.config;validate(c);std::map<Key,const Cell *> locked;std::set<Key> ancestors;
  if(c.coast&&water.cache)for(const auto &v:water.cache->cells){locked.emplace(key(v),&v);auto a=v;while(a.level>0){a.x/=2;a.y/=2;--a.level;ancestors.insert(key(a));}}
  // Reject overlapping serialized cells; otherwise an ancestor could hide a hole.
  for(const auto &[k,v]:locked)if(ancestors.contains(k))throw std::runtime_error("海岸线缓存存在重叠网格");
  std::vector<Cell> leaves;std::function<void(Cell)> visit=[&](Cell v){
    if(leaves.size()>160000)throw MeshBudget{};
    if(auto found=locked.find(key(v));found!=locked.end()){leaves.push_back(*found->second);return;}
    auto r=rect(c,v);double dx=std::max({r.x-double(eye.x),0.,double(eye.x)-r.x-r.w}),dy=std::max({r.y-double(eye.y),0.,double(eye.y)-r.y-r.h});
    const double distance=std::sqrt(dx*dx+dy*dy+double(eye.z)*eye.z),spacing=std::max({c.precision,c.wavelength/8.,distance*.10})/std::sqrt(density);
    if(v.level<26&&(ancestors.contains(key(v))||std::max(r.w,r.h)>spacing)){for(uint32_t y=0;y<2;++y)for(uint32_t x=0;x<2;++x)visit({v.x*2+x,v.y*2+y,v.level+1,{}});}
    else{v.clearance.fill(c.foam_width*2);leaves.push_back(v);}
  };visit({});if(progress)progress("连接水面分块边界");
  constexpr int64_t scale=int64_t(1)<<26;std::map<int64_t,std::set<int64_t>> horizontal,verticals;
  for(const auto &v:leaves){const auto s=scale>>v.level,x=int64_t(v.x)*s,y=int64_t(v.y)*s;for(auto yy:{y,y+s}){horizontal[yy].insert(x);horizontal[yy].insert(x+s);}for(auto xx:{x,x+s}){verticals[xx].insert(y);verticals[xx].insert(y+s);}}
  ir::Mesh out;out.id=water.id+"/mesh";out.material_slots={"Water"};out.smooth=true;
  struct Vertex {V p;double clearance,foam;};
  auto vertex=[&](double gx,double gy,double clearance){const auto p=wave(c,(gx/scale-.5)*c.width,(gy/scale-.5)*c.length);const double contact=std::clamp(1-std::max(0.,clearance)/c.foam_width,0.,1.);return Vertex{{p.x,p.y,p.z},clearance,std::max(p.foam,contact)};};
  auto triangle=[&](std::array<Vertex,3> input){
    std::vector<Vertex> clipped;for(int k=0;k<3;++k){auto a=input[k],b=input[(k+1)%3];if(a.clearance>=0)clipped.push_back(a);if((a.clearance<0)!=(b.clearance<0)){const double t=a.clearance/(a.clearance-b.clearance);clipped.push_back({a.p+(b.p-a.p)*t,0,a.foam+(b.foam-a.foam)*t});}}
    for(size_t k=1;k+1<clipped.size();++k){const Vertex verts[]={clipped[0],clipped[k],clipped[k+1]};auto n=cross(verts[1].p-verts[0].p,verts[2].p-verts[0].p);if(dot(n,n)<1e-18)continue;ir::Triangle t;t.source_polygon=uint32_t(out.triangles.size());for(int j=0;j<3;++j){t.vertices[j]=uint32_t(out.positions.size());const auto &v=verts[j];out.positions.push_back({float(v.p.x),float(v.p.y),float(v.p.z)});out.water_foam.push_back(float(v.foam));t.uv[j]={float(v.p.x),float(v.p.y)};}out.triangles.push_back(t);}
  };
  size_t processed=0;for(const auto &v:leaves){if((++processed%4096)==0&&progress)progress("生成水面："+std::to_string(processed)+" / "+std::to_string(leaves.size()));if(std::all_of(v.clearance.begin(),v.clearance.end(),[](double f){return f<0;}))continue;
    const auto s=scale>>v.level,x=int64_t(v.x)*s,y=int64_t(v.y)*s;std::vector<Vertex> boundary;
    auto edge=[&](const std::set<int64_t> &set,int64_t from,int64_t to,bool reverse,bool horiz,int64_t constant,int a,int b){std::vector<int64_t> values;for(auto it=set.lower_bound(from);it!=set.end()&&*it<=to;++it)values.push_back(*it);if(reverse)std::reverse(values.begin(),values.end());for(size_t i=0;i+1<values.size();++i){double q=double(values[i]-from)/double(to-from);if(reverse)q=1-q;boundary.push_back(vertex(horiz?values[i]:constant,horiz?constant:values[i],v.clearance[a]*(1-q)+v.clearance[b]*q));}};
    edge(horizontal[y],x,x+s,false,true,y,0,1);edge(verticals[x+s],y,y+s,false,false,x+s,1,2);edge(horizontal[y+s],x,x+s,true,true,y+s,2,3);edge(verticals[x],y,y+s,true,false,x,3,0);
    const auto center=vertex(x+s*.5,y+s*.5,v.clearance[4]);for(size_t k=0;k<boundary.size();++k)triangle({center,boundary[k],boundary[(k+1)%boundary.size()]});
  }
  // Weld identical positions so Cycles' smooth normals remain continuous across tiles.
  std::map<std::tuple<float,float,float>,uint32_t> unique;std::vector<ir::Vec3> positions;std::vector<float> foam;
  for(auto &t:out.triangles)for(auto &i:t.vertices){auto p=out.positions[i];auto [it,inserted]=unique.try_emplace({p.x,p.y,p.z},uint32_t(positions.size()));if(inserted){positions.push_back(p);foam.push_back(out.water_foam[i]);}else foam[it->second]=std::max(foam[it->second],out.water_foam[i]);i=it->second;}
  out.positions=std::move(positions);out.water_foam=std::move(foam);if(out.positions.empty()){out.positions.push_back({});out.water_foam.push_back(0);}out.source_polygon_count=uint32_t(out.triangles.size());return out;
}
ir::Mesh mesh(const Water &water,ir::Vec3 eye,const Progress &progress){
  const auto density=water.config.density;
  if(density<=1)return mesh_at_density(water,eye,progress,density);
  auto baseline=mesh_at_density(water,eye,progress,1);
  const auto limit=size_t(std::floor(baseline.positions.size()*density));
  // Quadtree refinement is discrete. Enforce the requested vertex-count ceiling
  // after stitching/clipping as well, with a bounded retry and a safe baseline.
  double actual=density;
  for(int attempt=0;attempt<5&&actual>1.001;++attempt){
    try {auto result=mesh_at_density(water,eye,progress,actual);if(result.positions.size()<=limit)return result;
      actual*=std::min(.9,.95*double(limit)/result.positions.size());
    }catch(const MeshBudget &){actual*=.5;}
  }
  return baseline;
}
}
