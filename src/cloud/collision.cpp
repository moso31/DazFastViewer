#include <Jolt/Jolt.h>
#include <Jolt/Geometry/ConvexHullBuilder.h>
#include "cloud/collision.h"
#include "runtime/physics.h"
#include <cmath>
#include <set>

namespace dfv::cloud {
Hull convex_hull(std::span<const ir::Vec3> points){
  Hull out;for(auto p:points)out.bounds.add(p);if(points.size()<3||out.bounds.empty)return out;
  out.center=out.bounds.center();const float scale=out.bounds.extent();if(!(scale>0))return out;
  runtime::initialize_physics();JPH::ConvexHullBuilder::Positions input;input.reserve(points.size());
  for(auto p:points)input.emplace_back((p.x-out.center.x)/scale,(p.y-out.center.y)/scale,(p.z-out.center.z)/scale);
  JPH::ConvexHullBuilder builder(input);const char *error=nullptr;
  const auto status=builder.Initialize(hull_vertex_budget,1e-5f,error);
  if(status!=JPH::ConvexHullBuilder::EResult::Success&&status!=JPH::ConvexHullBuilder::EResult::MaxVerticesReached)return out;
  std::set<int> used;
  auto plane=[&](JPH::Vec3 normal,JPH::Vec3 point){
    if(normal.LengthSq()<1e-14f)return;normal=normal.Normalized();const ir::CloudPlane p{{normal.GetX(),normal.GetY(),normal.GetZ()},normal.Dot(point)*scale};
    if(std::none_of(out.planes.begin(),out.planes.end(),[&](const auto &q){return p.normal.x*q.normal.x+p.normal.y*q.normal.y+p.normal.z*q.normal.z>1-1e-6f&&std::abs(p.distance-q.distance)<scale*1e-5f;}))out.planes.push_back(p);
  };
  for(const auto *face:builder.GetFaces()){
    plane(face->mNormal,face->mCentroid);auto *edge=face->mFirstEdge;
    do{used.insert(edge->mStartIdx);edge=edge->mNextEdge;}while(edge!=face->mFirstEdge);
  }
  if(builder.GetFaces().size()==2){
    // A planar polygon needs edge half-spaces too; no artificial thickness.
    const auto *face=builder.GetFaces().front();auto *edge=face->mFirstEdge;
    do{const auto a=input[edge->mStartIdx],b=input[edge->mNextEdge->mStartIdx];plane((b-a).Cross(face->mNormal),a);edge=edge->mNextEdge;}while(edge!=face->mFirstEdge);
  }
  if(out.planes.size()<4||out.planes.size()>ir::max_cloud_planes){out.planes.clear();return out;}
  for(int i:used)out.vertices.push_back(points[size_t(i)]);return out;
}
Hull transformed_hull(const Hull &h,const ir::Transform &t){
  Hull out;out.center=t.point(h.center);if(h.planes.empty())return out;
  const auto &a=ir::inverse(t).value;
  for(const auto &p:h.planes){const auto n=p.normal;const ir::Vec3 normal{a[0]*n.x+a[4]*n.y+a[8]*n.z,a[1]*n.x+a[5]*n.y+a[9]*n.z,a[2]*n.x+a[6]*n.y+a[10]*n.z};const float length=std::hypot(normal.x,normal.y,normal.z);out.planes.push_back({{normal.x/length,normal.y/length,normal.z/length},p.distance/length});}
  for(auto p:h.vertices)out.vertices.push_back(t.point(p));
  if(!h.bounds.empty)for(float x:{h.bounds.minimum.x,h.bounds.maximum.x})for(float y:{h.bounds.minimum.y,h.bounds.maximum.y})for(float z:{h.bounds.minimum.z,h.bounds.maximum.z})out.bounds.add(t.point({x,y,z}));
  return out;
}
}
