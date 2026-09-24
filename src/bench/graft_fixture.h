#pragma once
#include "render_ir/scene.h"

namespace dfv::bench {
// 弯曲宿主中央一面由带内部顶点的附件替代；两者局部坐标和 UV 不同。
inline ir::Scene graft_fixture() {
  ir::Scene scene;scene.materials.resize(2);scene.materials[0].id="body-white";scene.materials[1].id="graft-white";
  ir::Mesh body;body.id="body";body.material_slots={"body"};body.subdivision={true,2,2,0,2,0};
  for(int y=0;y<4;++y) for(int x=0;x<4;++x) body.positions.push_back({float(x),float(y),.06f*x*x+.04f*y*y});
  for(uint32_t y=0;y<3;++y) for(uint32_t x=0;x<3;++x) {ir::Polygon p;p.vertices={y*4+x,y*4+x+1,(y+1)*4+x+1,(y+1)*4+x};for(auto v:p.vertices) p.uv.push_back({body.positions[v].x/3,body.positions[v].y/3});body.polygons.push_back(p);}
  body.source_polygon_count=9;body.hidden_polygons={4};
  ir::Mesh graft;graft.id="graft";graft.material_slots={"unused","graft"};graft.subdivision={true,1,1,0,2,0};graft.graft_target_vertices=16;graft.graft_target_polygons=9;graft.graft_hidden_polygons={4};
  const auto relative=ir::Transform::translate({.25f,-.125f,.5f});const auto inverse=ir::inverse(relative);
  for(auto v:{5u,6u,10u,9u}) {graft.graft_vertex_pairs.push_back({uint32_t(graft.positions.size()),v});graft.positions.push_back(inverse.point(body.positions[v]));}
  graft.positions.push_back(inverse.point({1.5f,1.5f,.6f}));
  for(uint32_t v=0;v<4;++v) {ir::Polygon p;p.vertices={v,(v+1)%4,4};p.material_slot=1;for(auto k:p.vertices) {auto point=relative.point(graft.positions[k]);p.uv.push_back({2+point.x/3,point.y/3});}graft.polygons.push_back(p);}
  graft.source_polygon_count=4;
  for(auto *mesh:{&body,&graft}) for(uint32_t f=0;f<mesh->polygons.size();++f) {const auto &p=mesh->polygons[f];for(size_t k=1;k+1<p.vertices.size();++k) {ir::Triangle t;t.vertices={p.vertices[0],p.vertices[k],p.vertices[k+1]};t.uv={p.uv[0],p.uv[k],p.uv[k+1]};t.source_polygon=f;t.material_slot=p.material_slot;mesh->triangles.push_back(t);}}
  scene.meshes={body,graft};ir::Instance host;host.id="body";host.mesh=0;host.materials={0};host.transform=ir::Transform::translate({.3f,-.2f,.1f});host.transform.value[0]=2;host.transform.value[5]=1.5f;
  ir::Instance follower;follower.id="graft";follower.mesh=1;follower.materials={0,1};follower.transform=host.transform*relative;follower.graft_source=0;scene.instances={host,follower};return scene;
}
}
