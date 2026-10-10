#include "runtime/picking.h"
#include "editor/scene_extension.h"
#include "editor/node_properties.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace dfv;
using J=nlohmann::json;
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
struct D3 {
  double x,y,z;
  D3 operator+(D3 b)const{return {x+b.x,y+b.y,z+b.z};}
  D3 operator-(D3 b)const{return {x-b.x,y-b.y,z-b.z};}
  D3 operator*(double s)const{return {x*s,y*s,z*s};}
};
static double dot(D3 a,D3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static D3 cross(D3 a,D3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static D3 point(const ir::Transform &transform,ir::Vec3 p){const auto &m=transform.value;return {double(m[0])*p.x+double(m[1])*p.y+double(m[2])*p.z+m[3],double(m[4])*p.x+double(m[5])*p.y+double(m[6])*p.z+m[7],double(m[8])*p.x+double(m[9])*p.y+double(m[10])*p.z+m[11]};}
static ir::Scene fixture(float scale,float size=.001f){
  ir::Scene scene;ir::Mesh mesh;mesh.positions={{-size,-size,0},{size,-size,0},{0,size,0}};ir::Triangle face;face.vertices={0,1,2};mesh.triangles={face};scene.meshes={mesh};scene.instances.resize(1);
  auto &m=scene.instances[0].transform.value;m[0]=m[5]=m[10]=scale;return scene;
}
static void unit(){
  for(float scale:{.001f,1.f,10000.f,1000000.f,2250000.f,100000000.f,1.e13f}){
    auto scene=fixture(scale);runtime::PickingScene picking;picking.update(scene);
    auto hit=picking.ray({0,0,2},{0,0,-1});check(hit.instance==0&&std::abs(hit.distance-2)<1e-6f,"缩放改变了正面命中");
    check(picking.ray({0,0,-2},{0,0,1}).instance==0,"缩放改变了背面命中");
    hit=picking.ray({0,0,2},{0,0,-.25f});check(hit.instance==0&&std::abs(hit.distance-8)<1e-6f,"非单位射线参数被错误转换");
    check(picking.ray({scale*.002f,0,2},{0,0,-1}).instance<0,"三角面外误命中");
    check(picking.ray({0,0,2},{1,0,0}).instance<0,"平行射线误命中");
    scene.instances.push_back(scene.instances[0]);scene.instances[1].transform.value[11]=1;picking.update(scene);
    check(picking.ray({0,0,2},{0,0,-1}).instance==1,"巨型实例最近命中顺序错误");
  }
  // 大平移与小局部坐标相消，使用世界空间的解析位置验证求逆精度。
  auto scene=fixture(100000000.f);auto &m=scene.instances[0].transform.value;
  for(auto &p:scene.meshes[0].positions){p.x+=1;p.y+=2;p.z+=3;}
  m[3]=-100000000.f;m[7]=-200000000.f;m[11]=-300000000.f;
  runtime::PickingScene picking;picking.update(scene);check(picking.ray({0,0,2},{0,0,-1}).instance==0,"巨大平移相消后丢失命中");
  // 倾斜、非均匀与负缩放仍返回世界射线参数；相机射线不重建网格。
  scene=fixture(1,1);scene.instances[0].transform.value={-2000000,400000,0,0, 0,3,0,0, 1000000,0,.0001f,0};picking.update(scene);
  auto hit=picking.ray({0,0,2},{0,0,-1});check(hit.instance==0&&std::abs(hit.distance-2)<1e-5f,"倾斜非均匀缩放丢失距离");
  const auto builds=picking.stats().mesh_builds;
  scene.instances[0].transform.value[11]=1;ir::Delta delta;delta.instances.push_back({0,scene.instances[0].transform});picking.apply(scene,delta);
  check(picking.ray({0,0,2},{0,0,-1}).distance==1&&picking.stats().mesh_builds==builds,"实例更新丢失命中或重建网格");
  picking.set_pickable(scene,{0});check(picking.ray({0,0,2},{0,0,-1}).instance<0,"不可选对象被命中");
  picking.set_pickable(scene,{1});check(picking.ray({0,0,2},{0,0,-1}).instance==0,"恢复可选状态后没有命中");
  scene.instances[0].transform.value[11]=.5f;delta={};delta.instances.push_back({0,scene.instances[0].transform});
  runtime::PickingScene prepared;prepared.prepare_delta(scene,delta,{1});picking.swap_delta(prepared,scene,delta);
  hit=picking.ray({0,0,2},{0,0,-1});check(hit.instance==0&&hit.distance==1.5f&&picking.stats().mesh_builds==builds,"异步实例更新丢失命中或重建网格");
  check(picking.ray({0,0,2},{0,0,0}).instance<0,"零方向射线误命中");
  // 两个远处平面的间距小于最终 float 距离的 ULP，仍要先在高精度下排序。
  scene=fixture(1,1);scene.instances[0].transform.value[11]=1;scene.instances.push_back(scene.instances[0]);scene.instances[1].transform.value[11]=2;picking.update(scene);
  check(picking.ray({0,0,100000000},{0,0,-1}).instance==1,"过早舍入距离导致选中较远对象");
  std::cout<<"PASS: scale / direction / translation / shear / nearest hit / pick mask"<<std::endl;
}

// 实际场景的独立参考：先把顶点用 double 转到世界空间，再求平面交点与重心坐标。
// 参考不使用 PickingScene 的局部射线、矩阵求逆或 BVH。
struct RefFace {D3 a,e1,e2,n;double aa,ab,bb,denominator;int object,triangle;};
static void saved_scene(const char *file,const char *destination){
  namespace fs=std::filesystem;const fs::path output=fs::u8path(destination);fs::create_directories(output);
  std::cout<<"Loading saved scene"<<std::endl;auto restored=editor::load_scene_extension(fs::u8path(file),{},1);auto &d=*restored.document;auto &s=restored.snapshot;auto scene=d.loaded.scene;
  runtime::DeformationRuntime deformation(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
  for(;;){auto p=deformation.prepare(s.values,s.poses);check(p.error.empty(),p.error.c_str());if(!p.pending)break;check(std::chrono::steady_clock::now()<deadline,"Morph preparation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(5));}
  deformation.evaluate(editor::scene_properties(d,s),s.poses);editor::apply_instance_node_visibility(d,s,scene);
  const auto mask=editor::scene_pick_mask(d,s,scene);runtime::PickingScene picker;picker.update(scene,mask);
  std::vector<RefFace> faces;
  for(size_t i=0;i<scene.instances.size();++i){const auto &o=scene.instances[i];if(!mask[i]||!o.visible)continue;const auto &mesh=scene.meshes[o.mesh];for(size_t t=0;t<mesh.triangles.size();++t){const auto &f=mesh.triangles[t];if(!mesh.draws(f))continue;const auto a=point(o.transform,mesh.positions[f.vertices[0]]),b=point(o.transform,mesh.positions[f.vertices[1]]),c=point(o.transform,mesh.positions[f.vertices[2]]);const auto e1=b-a,e2=c-a;const auto aa=dot(e1,e1),ab=dot(e1,e2),bb=dot(e2,e2);const auto denominator=aa*bb-ab*ab;if(!(denominator>0))continue;faces.push_back({a,e1,e2,cross(e1,e2),aa,ab,bb,denominator,int(i),int(t)});}}
  check(s.view.has_value(),"Missing saved camera");const auto &v=*s.view;CameraState camera;camera.target={v[0],v[1],v[2]};camera.distance=v[3];camera.yaw=v[4];camera.pitch=v[5];
  const auto matrix=camera.matrix();const D3 origin{matrix[3],matrix[7],matrix[11]};J records=J::array();size_t mismatches=0;double elapsed=0;
  for(int y:{32,112,192,272,352})for(int x:{32,112,192,272,352}){
    const float extent=std::tan(.4f),sx=(2*(x+.5f)/384-1)*extent,sy=(1-2*(y+.5f)/384)*extent;
    const auto f=normalized({matrix[0]*sx+matrix[1]*sy+matrix[2],matrix[4]*sx+matrix[5]*sy+matrix[6],matrix[8]*sx+matrix[9]*sy+matrix[10]});const D3 direction{f.x,f.y,f.z};
    int object=-1,triangle=-1;double nearest=std::numeric_limits<double>::infinity();
    for(const auto &face:faces){const double nd=dot(face.n,direction);if(nd==0)continue;const double t=dot(face.n,face.a-origin)/nd;if(t<=1.e-5||t>=nearest)continue;const auto p=origin+direction*t-face.a;const double ap=dot(face.e1,p),bp=dot(face.e2,p),u=(face.bb*ap-face.ab*bp)/face.denominator,w=(face.aa*bp-face.ab*ap)/face.denominator;if(u<0||w<0||u+w>1)continue;nearest=t;object=face.object;triangle=face.triangle;}
    const auto start=std::chrono::steady_clock::now();const auto hit=picker.screen(camera,x,y,384,384);const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();elapsed+=ms;
    const bool matches=hit.instance==object&&(object<0||std::abs(hit.distance-nearest)<std::max(1.e-5,nearest*1.e-5));mismatches+=!matches;
    records.push_back({{"pixel",{x,y}},{"actual_object",hit.instance},{"actual_triangle",hit.triangle},{"actual_distance",hit.distance},{"expected_object",object},{"expected_triangle",triangle},{"expected_distance",nearest},{"expected_id",object<0?"":scene.instances[object].id},{"matches",matches},{"pick_ms",ms}});
    std::cout<<records.back().dump()<<std::endl;
  }
  std::ofstream(output/"result.json")<<J{{"rays",records},{"mismatches",mismatches},{"mean_pick_ms",elapsed/records.size()},{"camera",*s.view}}.dump(2);
}
int main(int argc,char **argv){try{if(argc==3)saved_scene(argv[1],argv[2]);else unit();return 0;}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
