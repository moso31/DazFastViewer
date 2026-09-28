#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include "runtime/physics.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <numeric>
#include <queue>
#include <set>
#include <tuple>

namespace dfv::runtime {
namespace {
using namespace JPH;
using Clock=std::chrono::steady_clock;
Vec3 jv(ir::Vec3 p){return {p.x,p.y,p.z};}
ir::Vec3 iv(Vec3 p){return {p.GetX(),p.GetY(),p.GetZ()};}
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
void cancelled(const std::function<bool()> &f){if(f&&f())throw std::runtime_error("物理任务已取消");}
struct Broad final:BroadPhaseLayerInterface {uint GetNumBroadPhaseLayers()const override{return 2;}BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l)const override{return BroadPhaseLayer(uint8(l));}};
struct Pairs final:ObjectLayerPairFilter {bool ShouldCollide(ObjectLayer a,ObjectLayer b)const override{return a||b;}};
struct Filter final:ObjectVsBroadPhaseLayerFilter {bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b)const override{return a||b==BroadPhaseLayer(1);}};
struct World {
  Broad broad;Pairs pairs;Filter filter;TempAllocatorImplWithMallocFallback temp{64*1024*1024};JobSystemSingleThreaded jobs{cMaxPhysicsJobs};PhysicsSystem system;
  World(){system.Init(8192,0,16384,16384,broad,filter,pairs);}
};
struct Union {
  std::vector<uint32_t> parent;
  explicit Union(size_t n):parent(n){std::iota(parent.begin(),parent.end(),0u);}
  uint32_t root(uint32_t v){while(parent[v]!=v){parent[v]=parent[parent[v]];v=parent[v];}return v;}
  void link(uint32_t a,uint32_t b){a=root(a);b=root(b);if(a!=b)parent[b]=a;}
};
struct Binding {uint32_t a=0,b=0;float blend=0,weight=1;bool operator==(const Binding &)const=default;};
struct Soft {
  PhysicsTarget target;Vec3 center;Ref<SoftBodySharedSettings> mesh;Body *body=nullptr;
  std::vector<Binding> mapping;std::vector<Vec3> rest;std::vector<float> activity;
  ir::Mesh source;std::vector<uint32_t> original;
};
std::vector<Vec3> world_vertices(const ir::Mesh &m,const ir::Transform &matrix,const std::function<bool()> &cancel){
  std::vector<Vec3> out;out.reserve(m.positions.size());for(size_t v=0;v<m.positions.size();++v){if((v&8191)==0)cancelled(cancel);out.push_back(jv(matrix.point(m.positions[v])));}return out;
}
// 保留连通片边界的体素聚合，不将相邻衣片或发束焊接。
void cloth_proxy(Soft &s,const ir::Mesh &mesh,const std::vector<Vec3> &points,size_t budget,const std::function<bool()> &cancel){
  Union sets(points.size());for(const auto &f:mesh.triangles){sets.link(f.vertices[0],f.vertices[1]);sets.link(f.vertices[1],f.vertices[2]);}
  Vec3 lo=points[0],hi=lo;for(auto p:points){lo=Vec3::sMin(lo,p);hi=Vec3::sMax(hi,p);}const Vec3 extent=hi-lo;
  float cell=std::max(.001f,extent.ReduceMax()/std::sqrt(float(budget)));
  std::vector<uint32_t> remap(points.size()),counts;std::vector<Vec3> sums;
  for(int attempt=0;attempt<30;++attempt){
    cancelled(cancel);std::map<std::tuple<uint32_t,int,int,int>,uint32_t> bins;sums.clear();counts.clear();
    for(size_t v=0;v<points.size();++v){if((v&8191)==0)cancelled(cancel);const auto p=(points[v]-lo)/cell;
      auto [it,added]=bins.try_emplace({sets.root(uint32_t(v)),int(std::floor(p.GetX())),int(std::floor(p.GetY())),int(std::floor(p.GetZ()))},uint32_t(sums.size()));
      if(added){sums.push_back(Vec3::sZero());counts.push_back(0);}remap[v]=it->second;sums[it->second]+=points[v];++counts[it->second];}
    if(sums.size()<=budget)break;cell*=1.3f;
  }
  if(sums.size()>budget)throw std::runtime_error("衣物独立组件过多，请选择发束模式或拆分对象");
  s.rest.resize(sums.size());for(size_t i=0;i<sums.size();++i)s.rest[i]=sums[i]/float(counts[i])-s.center;
  std::set<std::array<uint32_t,3>> faces;
  for(const auto &f:mesh.triangles){const uint32_t a=remap[f.vertices[0]],b=remap[f.vertices[1]],c=remap[f.vertices[2]];if(a==b||a==c||b==c)continue;
    if((s.rest[b]-s.rest[a]).Cross(s.rest[c]-s.rest[a]).LengthSq()<1e-14f)continue;auto key=std::array{a,b,c};std::sort(key.begin(),key.end());if(faces.insert(key).second)s.mesh->AddFace({a,b,c});}
  // 在导入形状上确定固定点，旋转或弯腰后也不会改挂另一端。
  const auto &reference=s.target.attachment_rest?s.target.attachment_rest->positions:mesh.positions;
  Vec3 root_lo=jv(reference[0]),root_hi=root_lo;for(auto p:reference){root_lo=Vec3::sMin(root_lo,jv(p));root_hi=Vec3::sMax(root_hi,jv(p));}const auto root_extent=root_hi-root_lo;
  int axis=root_extent.GetZ()>root_extent.ReduceMax()*.1f?2:(root_extent.GetX()>root_extent.GetY()?0:1);
  const float span=std::max(root_extent[axis],.001f),fixed=s.target.settings.fixed_fraction;
  s.activity.assign(s.rest.size(),1);for(size_t v=0;v<reference.size();++v){const float depth=(root_hi[axis]-jv(reference[v])[axis])/span;s.activity[remap[v]]=std::min(s.activity[remap[v]],std::clamp((depth-fixed)/std::max(.01f,1-fixed),0.f,1.f));}
  s.mapping.resize(points.size());for(size_t v=0;v<points.size();++v)s.mapping[v]={remap[v],remap[v],0,1};
}
// 每个源发束保留独立挂接点。用少量引导曲线传递位移，不改源拓扑。
void hair_proxy(Soft &s,const ir::Mesh &mesh,const std::vector<Vec3> &points,size_t budget,const std::function<bool()> &cancel){
  struct Strand {std::vector<uint32_t> vertices;std::vector<float> t;std::array<Vec3,12> guide;};
  std::vector<Strand> strands;
  if(!mesh.curves.empty()){
    for(const auto &c:mesh.curves){if(c.vertices.size()<2)continue;Strand v;v.vertices=c.vertices;v.t.resize(v.vertices.size());
      for(size_t i=1;i<v.t.size();++i)v.t[i]=v.t[i-1]+(points[v.vertices[i]]-points[v.vertices[i-1]]).Length();const float length=std::max(v.t.back(),1e-6f);for(auto &t:v.t)t/=length;strands.push_back(std::move(v));}
  }else{
    Union sets(points.size());std::vector<std::vector<uint32_t>> adjacent(points.size());
    for(const auto &f:mesh.triangles)for(int e=0;e<3;++e){auto a=f.vertices[e],b=f.vertices[(e+1)%3];sets.link(a,b);adjacent[a].push_back(b);adjacent[b].push_back(a);}
    std::map<uint32_t,std::vector<uint32_t>> groups;for(uint32_t v=0;v<points.size();++v)groups[sets.root(v)].push_back(v);
    std::vector<float> distance(points.size(),FLT_MAX);
    const auto &reference=s.target.attachment_rest?s.target.attachment_rest->positions:mesh.positions;
    for(auto &[_,vertices]:groups){cancelled(cancel);if(vertices.size()<2)continue;const auto root=*std::max_element(vertices.begin(),vertices.end(),[&](auto a,auto b){return reference[a].z<reference[b].z;});
      std::priority_queue<std::pair<float,uint32_t>,std::vector<std::pair<float,uint32_t>>,std::greater<>> queue;distance[root]=0;queue.push({0,root});
      while(!queue.empty()){auto [d,v]=queue.top();queue.pop();if(d!=distance[v])continue;for(auto w:adjacent[v]){const auto next=d+(points[w]-points[v]).Length();if(next<distance[w]){distance[w]=next;queue.push({next,w});}}}
      Strand strand;strand.vertices=std::move(vertices);float length=0;for(auto v:strand.vertices)length=std::max(length,distance[v]);for(auto v:strand.vertices)strand.t.push_back(distance[v]/std::max(length,1e-6f));strands.push_back(std::move(strand));
    }
  }
  if(strands.empty())throw std::runtime_error("发型缺少可模拟发束");
  for(auto &strand:strands){
    std::array<Vec3,12> sums;std::array<int,12> counts{};sums.fill(Vec3::sZero());
    for(size_t i=0;i<strand.vertices.size();++i){int k=std::clamp(int(std::round(strand.t[i]*11)),0,11);sums[k]+=points[strand.vertices[i]];++counts[k];}
    for(int k=0;k<12;++k)if(counts[k]){
      // 宽发片的质心可能落进头骨；引导点取真实表面顶点，避免从身体内部向外弹出。
      const auto mean=sums[k]/float(counts[k]);float best=FLT_MAX;
      for(size_t i=0;i<strand.vertices.size();++i)if(std::clamp(int(std::round(strand.t[i]*11)),0,11)==k){const auto p=points[strand.vertices[i]];const float d=(p-mean).LengthSq();if(d<best){best=d;strand.guide[k]=p;}}
    }
    for(int k=0;k<12;++k)if(!counts[k]){int a=k,b=k;while(a>0&&!counts[a])--a;while(b<11&&!counts[b])++b;strand.guide[k]=counts[a]&&counts[b]?strand.guide[a]+(strand.guide[b]-strand.guide[a])*(float(k-a)/std::max(1,b-a)):counts[a]?strand.guide[a]:strand.guide[b];}
  }
  const size_t count=std::min(strands.size(),std::max(size_t(1),budget/12));std::vector<size_t> guides;
  for(size_t i=0;i<count;++i){auto index=i*strands.size()/count;guides.push_back(index);for(int k=0;k<12;++k){s.rest.push_back(strands[index].guide[k]-s.center);s.activity.push_back(std::clamp((k/11.f-s.target.settings.fixed_fraction)/(1-s.target.settings.fixed_fraction),0.f,1.f));}}
  s.mapping.resize(points.size());for(auto &b:s.mapping)b.weight=0;
  for(size_t si=0;si<strands.size();++si){if((si&127)==0)cancelled(cancel);const auto &strand=strands[si];size_t nearest=0;float best=FLT_MAX;
    for(size_t g=0;g<guides.size();++g){const auto &other=strands[guides[g]];float d=(strand.guide[0]-other.guide[0]).LengthSq()+.2f*(strand.guide[11]-other.guide[11]).LengthSq();if(d<best){best=d;nearest=g;}}
    for(size_t i=0;i<strand.vertices.size();++i){float t=std::clamp(strand.t[i]*11,0.f,11.f);uint32_t a=uint32_t(t),b=std::min(a+1,11u);s.mapping[strand.vertices[i]]={uint32_t(nearest*12)+a,uint32_t(nearest*12)+b,t-a,1};}
  }
  const float compliance=(1-s.target.settings.stiffness)*1e-5f+1e-8f;
  for(uint32_t g=0;g<count;++g)for(uint32_t k=1;k<12;++k){s.mesh->mEdgeConstraints.push_back({g*12+k-1,g*12+k,compliance});if(k>1)s.mesh->mEdgeConstraints.push_back({g*12+k-2,g*12+k,compliance*20});}
}
// 仅收集选中材质的顶点；与未选中表面共享的边界顶点保持跟随。
struct Selection {ir::Mesh mesh,reference;std::vector<uint32_t> original;std::set<uint32_t> boundary;};
Selection select_surfaces(const ir::Mesh &mesh,const PhysicsTarget &target){
  Selection out;const auto &reference=target.attachment_rest?*target.attachment_rest:mesh;
  auto selected=[&](uint32_t slot){const auto name=slot<mesh.material_slots.size()?mesh.material_slots[slot]:std::string("#")+std::to_string(slot);return !target.settings.excluded_surfaces.contains(name);};
  std::vector<uint8_t> use(mesh.positions.size());
  for(const auto &f:mesh.triangles)for(auto v:f.vertices)use[v]|=selected(f.material_slot)?1:2;
  for(const auto &c:mesh.curves)for(auto v:c.vertices)use[v]|=selected(c.material_slot)?1:2;
  // 无面的刚体测试／点云也使用整个点集。
  if(mesh.triangles.empty()&&mesh.curves.empty()&&target.settings.excluded_surfaces.empty())std::fill(use.begin(),use.end(),1);
  std::vector<uint32_t> remap(use.size(),UINT32_MAX);
  for(uint32_t v=0;v<use.size();++v)if(use[v]&1){remap[v]=uint32_t(out.original.size());out.original.push_back(v);out.mesh.positions.push_back(mesh.positions[v]);out.reference.positions.push_back(reference.positions[v]);if(use[v]==3)out.boundary.insert(remap[v]);}
  for(auto f:mesh.triangles)if(selected(f.material_slot)&&mesh.draws(f)){for(auto &v:f.vertices)v=remap[v];out.mesh.triangles.push_back(f);}
  for(auto c:mesh.curves)if(selected(c.material_slot)){for(auto &v:c.vertices)v=remap[v];out.mesh.curves.push_back(std::move(c));}
  return out;
}
// 跨拓扑连通片建立距离约束。材质分组本身不是物理分割边界。
void join_components(Soft &s){
  if(!s.target.settings.joined||s.rest.size()<2)return;
  Union sets(s.rest.size());for(const auto &e:s.mesh->mEdgeConstraints)sets.link(e.mVertex[0],e.mVertex[1]);
  std::map<uint32_t,std::vector<uint32_t>> parts;for(uint32_t v=0;v<s.rest.size();++v)parts[sets.root(v)].push_back(v);
  if(parts.size()<2)return;
  // 每片连接最近的异片点，多点约束同时限制平移和转动。
  std::set<std::pair<uint32_t,uint32_t>> links;
  for(const auto &[root,vertices]:parts)for(auto a:vertices){
    float distance=FLT_MAX;uint32_t best=UINT32_MAX;
    for(uint32_t b=0;b<s.rest.size();++b)if(sets.root(b)!=root){const float d=(s.rest[a]-s.rest[b]).LengthSq();if(d<distance){distance=d;best=b;}}
    if(best!=UINT32_MAX&&links.insert(std::minmax(a,best)).second)s.mesh->mEdgeConstraints.push_back({a,best,1e-9f});
  }
  s.mesh->CalculateEdgeLengths();
}
struct Rigid {PhysicsTarget target;BodyID id;Vec3 center;std::vector<uint32_t> original;bool instance_transform=false;};
}
struct PhysicsSimulation::Impl {
  std::shared_ptr<const ir::Scene> scene;
  std::unique_ptr<World> world;
  PhysicsOptions options;
  PhysicsStats stats;
  std::vector<Rigid> rigids;
  std::vector<Soft> soft;
  std::vector<uint32_t> colliders;
  std::vector<PhysicsTarget> targets;
  std::map<uint32_t,int> remaining;std::map<uint32_t,uint64_t> advanced;
  bool running(const PhysicsTarget &t) const {return !t.settings.paused&&remaining.at(t.instance)>0;}
  void build(const std::function<bool()> &cancel);
};
void PhysicsSimulation::Impl::build(const std::function<bool()> &cancel){
  validate_physics(options);const auto begin=Clock::now();world=std::make_unique<World>();
  auto &system=world->system;auto &bodies=system.GetBodyInterface();system.SetGravity({0,0,-options.gravity});
  auto ps=system.GetPhysicsSettings();ps.mPenetrationSlop=.001f;ps.mSpeculativeContactDistance=.002f;system.SetPhysicsSettings(ps);
  if(options.ground)bodies.CreateAndAddBody(BodyCreationSettings(new PlaneShape(Plane(Vec3::sAxisZ(),-options.ground_height)),RVec3::sZero(),Quat::sIdentity(),EMotionType::Static,0),EActivation::DontActivate);
  std::set<uint32_t> simulated;for(const auto &t:targets)simulated.insert(t.instance);
  for(auto i:colliders){cancelled(cancel);const auto &instance=scene->instances.at(i);if(simulated.contains(i)||!instance.visible)continue;
    const auto &mesh=scene->meshes[instance.mesh];TriangleList triangles;triangles.reserve(mesh.triangles.size());
    for(const auto &f:mesh.triangles)if(mesh.draws(f)){auto a=jv(instance.transform.point(mesh.positions[f.vertices[0]])),b=jv(instance.transform.point(mesh.positions[f.vertices[1]])),c=jv(instance.transform.point(mesh.positions[f.vertices[2]]));if((b-a).Cross(c-a).LengthSq()>1e-14f)triangles.emplace_back(a,b,c);}
    if(triangles.empty())continue;auto shape=MeshShapeSettings(triangles).Create();if(shape.HasError())continue;bodies.CreateAndAddBody(BodyCreationSettings(shape.Get(),RVec3::sZero(),Quat::sIdentity(),EMotionType::Static,0),EActivation::DontActivate);
  }
  // 预算按对象固定；增加穿戴物不会改动现有对象的代理粒子身份。
  const size_t each_budget=options.quality==0?384:options.quality==1?768:1536;
  for(const auto &t:targets){cancelled(cancel);validate_physics(t.settings);const auto &instance=scene->instances.at(t.instance);const auto &full=scene->meshes.at(instance.mesh);
    auto selected=select_surfaces(full,t);auto &mesh=selected.mesh;if(mesh.positions.empty())continue;
    auto points=world_vertices(mesh,instance.transform,cancel);Vec3 lo=points[0],hi=lo;for(auto p:points){lo=Vec3::sMin(lo,p);hi=Vec3::sMax(hi,p);}const Vec3 center=(lo+hi)*.5f;
    if(t.kind==PhysicsKind::rigid){
      // 普通物体的选中表面可以组合为一个刚体，也可以按材质分成独立刚体。
      std::vector<std::vector<uint32_t>> pieces;
      if(t.settings.joined){pieces.push_back(selected.original);}else{
        std::map<uint32_t,std::set<uint32_t>> slots;for(const auto &f:mesh.triangles)for(auto v:f.vertices)slots[f.material_slot].insert(selected.original[v]);
        for(const auto &c:mesh.curves)for(auto v:c.vertices)slots[c.material_slot].insert(selected.original[v]);
        for(const auto &[_,vertices]:slots)pieces.emplace_back(vertices.begin(),vertices.end());if(pieces.empty())pieces.push_back(selected.original);
      }
      std::set<uint32_t> boundary;for(auto v:selected.boundary)boundary.insert(selected.original[v]);
      for(auto &piece:pieces){std::erase_if(piece,[&](auto v){return boundary.contains(v);});if(piece.empty())continue;
        Vec3 low=jv(instance.transform.point(full.positions[piece[0]])),high=low;for(auto v:piece){auto p=jv(instance.transform.point(full.positions[v]));low=Vec3::sMin(low,p);high=Vec3::sMax(high,p);}const Vec3 pivot=(low+high)*.5f;
        RefConst<Shape> collision=new BoxShape(Vec3::sMax((high-low)*.5f,Vec3::sReplicate(.002f)),.001f);
        Array<Vec3> hull_points;for(auto v:piece)hull_points.push_back(jv(instance.transform.point(full.positions[v]))-pivot);auto hull=ConvexHullShapeSettings(hull_points,.001f).Create();if(!hull.HasError())collision=hull.Get();
        // 角色上的刚性配件保持挂接，不作为自由落体掉落。
        BodyCreationSettings settings(collision,RVec3(pivot),Quat::sIdentity(),t.host>=0?EMotionType::Kinematic:EMotionType::Dynamic,1);
        settings.mFriction=t.settings.friction;settings.mRestitution=t.settings.restitution;settings.mLinearDamping=std::min(t.settings.damping,5.f);settings.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;settings.mMassPropertiesOverride.mMass=t.settings.mass;
        const auto id=bodies.CreateAndAddBody(settings,EActivation::Activate);if(id.IsInvalid())throw std::runtime_error("物理对象数量超出预算");
        const bool whole_instance=pieces.size()==1&&piece.size()==full.positions.size();
        rigids.push_back({t,id,pivot,std::move(piece),whole_instance});
      }
      continue;
    }
    Soft s;s.target=t;s.center=center;s.mesh=new SoftBodySharedSettings;s.source=std::move(mesh);s.original=std::move(selected.original);s.target.attachment_rest=&selected.reference;
    if(t.kind==PhysicsKind::cloth)cloth_proxy(s,s.source,points,each_budget,cancel);else hair_proxy(s,s.source,points,each_budget,cancel);
    s.target.attachment_rest=t.attachment_rest;
    for(auto i:selected.boundary){const auto b=s.mapping[i];s.activity[b.a]=s.activity[b.b]=0;}
    const float travel=(t.kind==PhysicsKind::hair_cards||t.kind==PhysicsKind::hair_curves)?std::min(t.settings.max_distance,(hi-lo).ReduceMax()*.2f):t.settings.max_distance;
    for(size_t i=0;i<s.rest.size();++i){SoftBodySharedSettings::Vertex v;s.rest[i].StoreFloat3(&v.mPosition);v.mInvMass=s.activity[i]==0?0:float(s.rest.size())/t.settings.mass;s.mesh->mVertices.push_back(v);
      SoftBodySharedSettings::Skinned skinned(uint32_t(i),s.activity[i]*travel,FLT_MAX,0);skinned.mWeights[0]=SoftBodySharedSettings::SkinWeight(0,1);s.mesh->mSkinnedConstraints.push_back(skinned);}
    if(t.kind==PhysicsKind::cloth){SoftBodySharedSettings::VertexAttributes attributes((1-t.settings.stiffness)*1e-6f+1e-9f,(1-t.settings.stiffness)*1e-6f+1e-9f,(1-t.settings.stiffness)*1e-3f+1e-7f,SoftBodySharedSettings::ELRAType::GeodesicDistance,1.03f);s.mesh->CreateConstraints(&attributes,1,SoftBodySharedSettings::EBendType::Distance);}else s.mesh->CalculateEdgeLengths();
    join_components(s);s.mesh->mInvBindMatrices.push_back({0,Mat44::sIdentity()});s.mesh->CalculateSkinnedConstraintNormals();s.mesh->Optimize();
    SoftBodyCreationSettings settings(s.mesh,RVec3(center),Quat::sIdentity(),1);settings.mUpdatePosition=false;settings.mAllowSleeping=false;settings.mNumIterations=options.quality==0?2:options.quality==1?4:6;settings.mLinearDamping=t.settings.damping;settings.mVertexRadius=std::min(t.settings.thickness,.01f);settings.mFriction=t.settings.friction;settings.mMaxLinearVelocity=1;
    s.body=bodies.CreateSoftBody(settings);if(!s.body)throw std::runtime_error("物理对象数量超出预算");bodies.AddBody(s.body->GetID(),EActivation::Activate);auto *mp=static_cast<SoftBodyMotionProperties *>(s.body->GetMotionProperties());auto identity=Mat44::sIdentity();mp->SkinVertices(s.body->GetCenterOfMassTransform(),&identity,1,true,world->temp);
    stats.particles+=s.rest.size();soft.push_back(std::move(s));
  }
  stats.objects=soft.size()+rigids.size();stats.prepare_ms=elapsed(begin);
}
PhysicsSimulation::PhysicsSimulation(){static std::once_flag once;std::call_once(once,[]{RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();});}
PhysicsSimulation::~PhysicsSimulation()=default;
void PhysicsSimulation::update(std::shared_ptr<const ir::Scene> scene,const std::vector<PhysicsTarget> &targets,const PhysicsOptions &options,const std::vector<uint32_t> &colliders,const std::function<bool()> &cancel){
  validate_physics(options);
  bool compatible=impl_&&impl_->scene==scene&&impl_->targets.size()==targets.size()&&impl_->colliders==colliders&&same_physics_environment(impl_->options,options);
  if(compatible)for(size_t i=0;i<targets.size();++i){const auto &a=impl_->targets[i],&b=targets[i];compatible&=a.instance==b.instance&&a.kind==b.kind&&a.host==b.host&&same_physics_parameters(a.settings,b.settings)&&a.settings.reset_sequence==b.settings.reset_sequence;}
  if(compatible){
    auto &bodies=impl_->world->system.GetBodyInterface();
    for(size_t i=0;i<targets.size();++i)if(targets[i].settings.run_sequence!=impl_->targets[i].settings.run_sequence)impl_->remaining[targets[i].instance]=targets[i].settings.rounds;
    for(auto &s:impl_->soft)s.target=*std::find_if(targets.begin(),targets.end(),[&](const auto &t){return t.instance==s.target.instance;});
    for(auto &r:impl_->rigids){r.target=*std::find_if(targets.begin(),targets.end(),[&](const auto &t){return t.instance==r.target.instance;});if(impl_->running(r.target))bodies.ActivateBody(r.id);}
    impl_->targets=targets;impl_->options=options;return;
  }
  auto next=std::make_unique<Impl>();next->scene=std::move(scene);next->targets=targets;next->options=options;next->colliders=colliders;next->build(cancel);
  auto preserve=[&](const PhysicsTarget &t){if(!impl_||!same_physics_environment(impl_->options,options))return false;auto old=std::find_if(impl_->targets.begin(),impl_->targets.end(),[&](const auto &p){return p.instance==t.instance;});return old!=impl_->targets.end()&&old->kind==t.kind&&old->host==t.host&&same_physics_parameters(old->settings,t.settings)&&old->settings.reset_sequence==t.settings.reset_sequence;};
  for(const auto &t:targets){
    next->remaining[t.instance]=!impl_&&t.settings.run_sequence>t.settings.reset_sequence? t.settings.rounds:0;next->advanced[t.instance]=0;
    if(preserve(t)){next->remaining[t.instance]=impl_->remaining.at(t.instance);next->advanced[t.instance]=impl_->advanced.at(t.instance);}
    if(impl_){auto old=std::find_if(impl_->targets.begin(),impl_->targets.end(),[&](const auto &p){return p.instance==t.instance;});if(old!=impl_->targets.end()){
      if(old->settings.reset_sequence!=t.settings.reset_sequence)next->remaining[t.instance]=0;
      if(old->settings.run_sequence!=t.settings.run_sequence&&t.settings.run_sequence>t.settings.reset_sequence)next->remaining[t.instance]=t.settings.rounds;
    }else if(t.settings.run_sequence>t.settings.reset_sequence)next->remaining[t.instance]=t.settings.rounds;}
    if(std::none_of(next->soft.begin(),next->soft.end(),[&](const auto &s){return s.target.instance==t.instance;})&&std::none_of(next->rigids.begin(),next->rigids.end(),[&](const auto &r){return r.target.instance==t.instance;}))next->remaining[t.instance]=0;
  }
  if(impl_){
    next->stats.steps=impl_->stats.steps;
    for(auto &s:next->soft){if(!preserve(s.target))continue;auto old=std::find_if(impl_->soft.begin(),impl_->soft.end(),[&](const auto &p){return p.target.instance==s.target.instance;});if(old==impl_->soft.end())continue;
      const auto &before=static_cast<SoftBodyMotionProperties *>(old->body->GetMotionProperties())->GetVertices();auto &after=static_cast<SoftBodyMotionProperties *>(s.body->GetMotionProperties())->GetVertices();
      if(s.mapping==old->mapping&&s.original==old->original&&s.rest.size()==old->rest.size()){
        for(size_t i=0;i<after.size();++i){after[i].mPosition=s.rest[i]+before[i].mPosition-old->rest[i];after[i].mPreviousPosition=after[i].mPosition;after[i].mVelocity=before[i].mVelocity;}continue;
      }
      std::map<uint32_t,size_t> source;for(size_t i=0;i<old->original.size();++i)source[old->original[i]]=i;
      std::vector<Vec3> offsets(s.rest.size(),Vec3::sZero()),velocities(s.rest.size(),Vec3::sZero());std::vector<float> weights(s.rest.size());
      for(size_t i=0;i<s.original.size();++i){auto it=source.find(s.original[i]);if(it==source.end())continue;const auto a=old->mapping[it->second],b=s.mapping[i];
        const Vec3 offset=(before[a.a].mPosition-old->rest[a.a])*(1-a.blend)+(before[a.b].mPosition-old->rest[a.b])*a.blend;
        const Vec3 velocity=before[a.a].mVelocity*(1-a.blend)+before[a.b].mVelocity*a.blend;
        for(auto [v,w]:{std::pair{b.a,1-b.blend},std::pair{b.b,b.blend}}){offsets[v]+=offset*w;velocities[v]+=velocity*w;weights[v]+=w;}
      }
      for(size_t i=0;i<after.size();++i)if(weights[i]>0&&s.activity[i]>0){after[i].mPosition=s.rest[i]+offsets[i]/weights[i];after[i].mPreviousPosition=after[i].mPosition;after[i].mVelocity=velocities[i]/weights[i];}
    }
    auto &before=impl_->world->system.GetBodyInterface();auto &after=next->world->system.GetBodyInterface();
    for(const auto &r:next->rigids){if(!preserve(r.target))continue;auto old=std::find_if(impl_->rigids.begin(),impl_->rigids.end(),[&](const auto &p){return p.target.instance==r.target.instance&&p.original==r.original;});if(old==impl_->rigids.end()||r.target.host>=0)continue;
      after.SetPositionAndRotation(r.id,before.GetPosition(old->id)+RVec3(r.center-old->center),before.GetRotation(old->id),EActivation::Activate);after.SetLinearAndAngularVelocity(r.id,before.GetLinearVelocity(old->id),before.GetAngularVelocity(old->id));
    }
  }
  cancelled(cancel);impl_=std::move(next);
}
void PhysicsSimulation::options(const PhysicsOptions &options){
  validate_physics(options);if(!impl_)return;
  if(!same_physics_environment(impl_->options,options)){update(impl_->scene,impl_->targets,options,impl_->colliders);return;}
  impl_->options=options;impl_->world->system.SetGravity({0,0,-options.gravity});
}
void PhysicsSimulation::step(const std::function<bool()> &cancel){
  if(!impl_||impl_->options.paused||std::none_of(impl_->targets.begin(),impl_->targets.end(),[&](const auto &t){return impl_->running(t);}))return;cancelled(cancel);auto &p=*impl_;const auto begin=Clock::now();auto &bodies=p.world->system.GetBodyInterface();
  constexpr float dt=1.f/60;
  std::vector<std::vector<SoftBodyMotionProperties::Vertex>> previous;previous.reserve(p.soft.size());
  for(auto &s:p.soft){auto &v=static_cast<SoftBodyMotionProperties *>(s.body->GetMotionProperties())->GetVertices();previous.emplace_back(v.begin(),v.end());if(!p.running(s.target))for(auto &x:v)x.mInvMass=0;}
  struct RigidState {RVec3 position;Quat rotation;Vec3 velocity,angular;};std::vector<RigidState> frozen;
  for(const auto &r:p.rigids)if(!p.running(r.target)){frozen.push_back({bodies.GetPosition(r.id),bodies.GetRotation(r.id),bodies.GetLinearVelocity(r.id),bodies.GetAngularVelocity(r.id)});bodies.DeactivateBody(r.id);}
  const auto error=p.world->system.Update(dt,1,&p.world->temp,&p.world->jobs);if(error!=EPhysicsUpdateError::None)throw std::runtime_error("物理碰撞预算不足，请减少参与的表面");
  for(size_t k=0;k<p.soft.size();++k){auto &s=p.soft[k];auto &vertices=static_cast<SoftBodyMotionProperties *>(s.body->GetMotionProperties())->GetVertices();const auto &before=previous[k];
    for(size_t i=0;i<vertices.size();++i){auto &v=vertices[i];if(!p.running(s.target)){v=before[i];continue;}
      if(s.activity[i]==0){v.mPosition=s.rest[i];v.mVelocity=Vec3::sZero();continue;}
      if(!std::isfinite(v.mPosition.LengthSq())){v=before[i];v.mVelocity=Vec3::sZero();continue;}
      // 碰撞修正也受单步预算约束，仅限制速度无法防止深穿透投影弹飞。
      Vec3 change=v.mPosition-before[i].mPosition;const float maximum=.008f;
      if(change.LengthSq()>maximum*maximum){change=change.Normalized()*maximum;v.mPosition=before[i].mPosition+change;v.mVelocity=change/dt;}
      Vec3 offset=v.mPosition-s.rest[i];const float limit=s.activity[i]*s.target.settings.max_distance;
      if(offset.LengthSq()>limit*limit){v.mPosition=s.rest[i]+offset.Normalized()*limit;v.mVelocity=Vec3::sZero();}
      v.mPreviousPosition=v.mPosition;v.mVelocity=v.mVelocity.NormalizedOr(Vec3::sZero())*std::min(v.mVelocity.Length(),1.f);
    }
  }
  size_t frozen_index=0;for(const auto &r:p.rigids)if(!p.running(r.target)){const auto &v=frozen[frozen_index++];bodies.SetPositionAndRotation(r.id,v.position,v.rotation,EActivation::DontActivate);bodies.SetLinearAndAngularVelocity(r.id,v.velocity,v.angular);}
  for(const auto &t:p.targets)if(p.running(t)){--p.remaining[t.instance];++p.advanced[t.instance];}
  ++p.stats.steps;p.stats.solve_ms=elapsed(begin);
}
PhysicsStats PhysicsSimulation::stats() const{auto result=impl_?impl_->stats:PhysicsStats{};if(impl_&&!impl_->options.paused)for(const auto &t:impl_->targets)if(impl_->running(t))++result.active_objects;return result;}
PhysicsOutput PhysicsSimulation::output(const std::function<bool()> &cancel) const{
  PhysicsOutput output;if(!impl_)return output;const auto &p=*impl_;output.stats=stats();const auto &scene=*p.scene;auto &bodies=p.world->system.GetBodyInterface();
  std::map<uint32_t,std::vector<ir::Vec3>> changed;
  for(const auto &r:p.rigids){const auto &instance=scene.instances[r.target.instance];const auto &original=instance.transform;auto matrix=Mat44::sRotationTranslation(bodies.GetRotation(r.id),Vec3(bodies.GetPosition(r.id)));ir::Transform moved;
    const auto origin=iv(matrix*(jv(original.point({}))-r.center));auto x=iv(matrix.Multiply3x3(jv(original.point({1,0,0}))-jv(original.point({}))));auto y=iv(matrix.Multiply3x3(jv(original.point({0,1,0}))-jv(original.point({}))));auto z=iv(matrix.Multiply3x3(jv(original.point({0,0,1}))-jv(original.point({}))));moved.value={x.x,y.x,z.x,origin.x,x.y,y.y,z.y,origin.y,x.z,y.z,z.z,origin.z};
    if(p.advanced.at(r.target.instance)==0||r.target.host>=0)moved=original;
    if(r.instance_transform){output.delta.instances.push_back({r.target.instance,moved});continue;}
    auto [it,_]=changed.try_emplace(instance.mesh,scene.meshes[instance.mesh].positions);const auto inverse=ir::inverse(original);
    if(p.advanced.at(r.target.instance)>0&&r.target.host<0)for(auto v:r.original)it->second[v]=inverse.point(moved.point(scene.meshes[instance.mesh].positions[v]));
  }
  for(const auto &s:p.soft){cancelled(cancel);const auto &instance=scene.instances[s.target.instance];auto [it,_]=changed.try_emplace(instance.mesh,scene.meshes[instance.mesh].positions);auto &positions=it->second;
    if(p.advanced.at(s.target.instance)==0)continue;
    const auto inverse=ir::inverse(instance.transform);const auto &vertices=static_cast<const SoftBodyMotionProperties *>(s.body->GetMotionProperties())->GetVertices();
    std::vector<Vec3> offsets(s.source.positions.size());
    for(size_t i=0;i<offsets.size();++i){const auto &b=s.mapping[i];offsets[i]=((vertices[b.a].mPosition-s.rest[b.a])*(1-b.blend)+(vertices[b.b].mPosition-s.rest[b.b])*b.blend)*b.weight;}
    if(s.target.kind==PhysicsKind::cloth){std::vector<Vec3> sums(offsets.size(),Vec3::sZero());std::vector<uint32_t> counts(offsets.size());
      for(const auto &f:s.source.triangles)for(int k=0;k<3;++k){const auto a=f.vertices[k],b=f.vertices[(k+1)%3];sums[a]+=offsets[b];sums[b]+=offsets[a];++counts[a];++counts[b];}
      for(size_t i=0;i<offsets.size();++i)if(counts[i]&&s.activity[s.mapping[i].a]>0)offsets[i]=offsets[i]*.8f+sums[i]*(.2f/float(counts[i]));
    }
    for(size_t i=0;i<offsets.size();++i){if((i&8191)==0)cancelled(cancel);const auto d=iv(offsets[i]),v=s.source.positions[i];const auto &m=inverse.value;positions[s.original[i]]={v.x+m[0]*d.x+m[1]*d.y+m[2]*d.z,v.y+m[4]*d.x+m[5]*d.y+m[6]*d.z,v.z+m[8]*d.x+m[9]*d.y+m[10]*d.z};}
  }
  for(auto &[mesh,positions]:changed)output.delta.meshes.push_back({mesh,std::move(positions)});
  return output;
}
PhysicsOutput settle_physics(const ir::Scene &scene,const std::vector<PhysicsTarget> &targets,const PhysicsOptions &options,const std::function<bool()> &cancel){
  if(options.paused||targets.empty())return {};PhysicsSimulation simulation;std::vector<uint32_t> colliders;for(uint32_t i=0;i<scene.instances.size();++i)colliders.push_back(i);
  auto batch=targets;for(auto &t:batch){t.settings.rounds=180;t.settings.run_sequence=1;}simulation.update(std::make_shared<ir::Scene>(scene),batch,options,colliders,cancel);for(int i=0;i<180;++i)simulation.step(cancel);return simulation.output(cancel);
}
}
