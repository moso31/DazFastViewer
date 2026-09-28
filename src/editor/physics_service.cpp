#include "editor/physics_service.h"
#include "editor/physics_runner.h"
#include "editor/object_extension.h"
#include "runtime/geometry_shell.h"
#include <cctype>
#include <bit>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace dfv::editor {
bool physics_eligible(const Document &d,size_t t) {
  if(t>=d.catalog.targets.size()||growth_character(d,t))return false;
  const auto &target=d.catalog.targets[t];const auto &i=d.loaded.scene.instances.at(target.instance);
  for(const auto &o:d.loaded.objects)if(o.instance==target.instance&&(o.content_type=="Actor"||o.content_type.starts_with("Actor/")))return false;
  const auto &m=d.loaded.scene.meshes.at(i.mesh);
  // GeoGraft 和 Shell 属于宿主依赖曲面，不作为独立衣物模拟。
  return !m.graft_target_vertices&&i.graft_source<0&&i.shell_source<0&&i.prototype<0&&!m.positions.empty()&&(!m.triangles.empty()||!m.curves.empty());
}
runtime::PhysicsKind physics_kind(const Document &d,size_t t) {
  const auto &target=d.catalog.targets.at(t);const auto &m=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(target.instance).mesh);
  if(!m.curves.empty())return runtime::PhysicsKind::hair_curves;
  std::string type;bool follower=!target.conform_target.empty();
  for(const auto &o:d.loaded.objects)if(o.instance==target.instance){type=o.content_type;follower|=type.starts_with("Follower/");break;}
  auto label=type+" "+target.label;for(auto &c:label)c=char(std::tolower(static_cast<unsigned char>(c)));
  if(label.find("hair")!=std::string::npos||label.find("bangs")!=std::string::npos)return runtime::PhysicsKind::hair_cards;
  if(label.find("shoe")!=std::string::npos||label.find("boot")!=std::string::npos||label.find("footwear")!=std::string::npos)return runtime::PhysicsKind::rigid;
  return follower?runtime::PhysicsKind::cloth:runtime::PhysicsKind::rigid;
}
bool same_physics_geometry(const Snapshot &a,const Snapshot &b) {
  if(a.generation!=b.generation||a.poses!=b.poses||a.instance_ground!=b.instance_ground||a.values.size()!=b.values.size())return false;
  for(size_t i=0;i<a.values.size();++i){const auto &x=a.values[i],&y=b.values[i];if(x.transform!=y.transform||x.visible!=y.visible||x.morphs!=y.morphs||x.unlimited_morphs!=y.unlimited_morphs||x.extension!=y.extension||x.ground_alignment_ratio!=y.ground_alignment_ratio)return false;}
  return true;
}
bool same_physics_input(const Snapshot &a,const Snapshot &b) {
  if(!same_physics_geometry(a,b))return false;
  for(size_t i=0;i<a.values.size();++i)if(a.values[i].physics!=b.values[i].physics)return false;
  return true;
}
namespace {
int physics_host(const Document &d,size_t t){
  const auto &target=d.catalog.targets[t];
  for(size_t h=0;h<d.catalog.targets.size();++h)if(growth_character(d,h)){
    const auto &host=d.catalog.targets[h];const auto id="#"+host.id.substr(0,host.id.rfind('/'));
    if(target.conform_target==id||target.parent==id||std::find(target.ancestors.begin(),target.ancestors.end(),id)!=target.ancestors.end())return int(host.instance);
  }
  return -1;
}
bool same_targets(const std::vector<runtime::PhysicsTarget> &a,const std::vector<runtime::PhysicsTarget> &b){
  if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].instance!=b[i].instance||a[i].host!=b[i].host||a[i].kind!=b[i].kind||a[i].settings!=b[i].settings)return false;return true;
}
uint64_t positions_hash(const std::vector<ir::Vec3> &positions){uint64_t h=14695981039346656037ull;for(auto p:positions)for(float x:{p.x,p.y,p.z}){h^=std::bit_cast<uint32_t>(x);h*=1099511628211ull;}return h;}
bool changed_geometry(const ir::Scene &a,const ir::Scene &b,uint32_t i){const auto &x=a.instances[i],&y=b.instances[i];return x.transform!=y.transform||x.visible!=y.visible||a.meshes[x.mesh].positions!=b.meshes[y.mesh].positions;}
}
PhysicsService::PhysicsService():worker_([this](std::stop_token stop){
#ifdef _WIN32
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
#endif
  using Clock=std::chrono::steady_clock;
  struct Group {std::unique_ptr<PhysicsRunner> simulation;size_t observed_steps=0;std::vector<runtime::PhysicsTarget> targets;std::vector<uint32_t> colliders;std::shared_ptr<const ir::Scene> base;std::string error;};
  std::map<int,Group> groups;std::shared_ptr<const Document> cached_document;
  std::unique_ptr<ir::Scene> deformation_scene;std::shared_ptr<const ir::Scene> base;
  std::unique_ptr<runtime::DeformationRuntime> deformation;Request active;
  std::map<uint32_t,uint64_t> applied_meshes,base_hashes;std::map<uint32_t,ir::Transform> applied_instances;
  bool frame_dirty=false;std::set<uint32_t> previous_meshes,previous_instances;auto next_tick=Clock::now(),next_frame=next_tick;
  while(!stop.stop_requested()){
    std::optional<Request> incoming;std::shared_ptr<PhysicsResult> acknowledged;std::vector<std::shared_ptr<PhysicsResult>> retired;
    {std::unique_lock lock(mutex_);ready_.wait_until(lock,stop,std::min(groups.empty()?Clock::time_point::max():next_tick,frame_requested_&&active.document?next_frame:Clock::time_point::max()),[&]{return pending_.has_value()||acknowledged_||reset_||!retired_.empty()||(frame_requested_&&active.document&&Clock::now()>=next_frame);});
      if(stop.stop_requested())break;retired.swap(retired_);acknowledged=std::move(acknowledged_);if(pending_){incoming=std::move(pending_);pending_.reset();}}
    retired.clear();
    if(acknowledged&&active.document&&acknowledged->generation==active.document->generation){frame_dirty=true;frame_requested_=true;for(auto [i,h]:acknowledged->mesh_hashes)applied_meshes[i]=h;for(const auto &[i,t]:acknowledged->transforms)applied_instances[i]=t;}
    acknowledged.reset();
    if(reset_.exchange(false)){groups.clear();active={};base.reset();applied_meshes.clear();applied_instances.clear();base_hashes.clear();previous_meshes.clear();previous_instances.clear();busy_=false;}
    if(incoming){const auto &q=*incoming;auto cancelled=[&]{return stop.stop_requested()||serial_.load()!=q.serial;};
      try{
        if(groups.empty()&&previous_meshes.empty()&&previous_instances.empty()&&std::none_of(q.snapshot.values.begin(),q.snapshot.values.end(),[](const auto &v){return v.physics.enabled;})){
          active=q;frame_requested_=false;frame_dirty=false;auto result=std::make_shared<PhysicsResult>();result->serial=q.serial;result->generation=q.document->generation;std::lock_guard lock(mutex_);result_.swap(result);continue;
        }
        const bool new_document=cached_document!=q.document;
        if(new_document){applied_meshes.clear();applied_instances.clear();base_hashes.clear();groups.clear();previous_meshes.clear();previous_instances.clear();deformation.reset();deformation_scene=std::make_unique<ir::Scene>(q.document->loaded.scene);cached_document=q.document;deformation=std::make_unique<runtime::DeformationRuntime>(*deformation_scene,q.document->catalog.targets,q.document->skeletons.skins,q.document->formulas.graphs);base.reset();}
        if(!base||!same_physics_geometry(active.snapshot,q.snapshot)){
          applied_meshes.clear();applied_instances.clear();base_hashes.clear();
          while(!cancelled()){const auto prepared=deformation->prepare(q.snapshot.values,q.snapshot.poses);if(!prepared.error.empty())throw std::runtime_error(prepared.error);if(!prepared.pending)break;std::unique_lock lock(mutex_);ready_.wait_for(lock,std::chrono::milliseconds(5),[&]{return cancelled();});}
          if(cancelled())continue;deformation->evaluate(q.snapshot.values,q.snapshot.poses);
          auto next=std::make_shared<ir::Scene>(*deformation_scene);apply_instance_ground(*next,q.document->loaded.scene,q.snapshot.instance_ground);base=std::move(next);
        }
        std::map<int,std::vector<runtime::PhysicsTarget>> wanted;std::vector<uint32_t> environment;std::set<uint32_t> all_simulated;
        for(size_t t=0;t<q.snapshot.values.size();++t){const int host=physics_host(*q.document,t);const auto &target=q.document->catalog.targets[t];
          if(q.snapshot.values[t].physics.enabled&&q.snapshot.values[t].visible&&physics_eligible(*q.document,t)){
            runtime::PhysicsTarget v;v.instance=target.instance;v.host=host;v.settings=q.snapshot.values[t].physics;v.kind=v.settings.kind==runtime::PhysicsKind::automatic?physics_kind(*q.document,t):v.settings.kind;
            // 命令只消费一次。关闭再启用不会重放之前的运行命令。
            const runtime::PhysicsObjectSettings *previous=nullptr;if(active.document==q.document&&t<active.snapshot.values.size())previous=&active.snapshot.values[t].physics;
            const runtime::PhysicsTarget *existing=nullptr;if(auto g=groups.find(host);g!=groups.end())for(const auto &old:g->second.targets)if(old.instance==v.instance)existing=&old;
            const bool run=v.settings.run_sequence>(previous?previous->run_sequence:0),reset=v.settings.reset_sequence>(previous?previous->reset_sequence:0);
            v.settings.run_sequence=run?v.settings.run_sequence:(existing?existing->settings.run_sequence:0);v.settings.reset_sequence=reset?v.settings.reset_sequence:(existing?existing->settings.reset_sequence:0);
            v.attachment_rest=&q.document->loaded.scene.meshes.at(q.document->loaded.scene.instances.at(v.instance).mesh);wanted[host].push_back(v);all_simulated.insert(v.instance);
          }else if(host<0&&!growth_character(*q.document,t)&&physics_kind(*q.document,t)==runtime::PhysicsKind::rigid&&q.snapshot.values[t].visible)environment.push_back(target.instance);
        }
        for(auto &[id,targets]:wanted){std::vector<uint32_t> colliders=environment;if(id>=0)colliders.push_back(uint32_t(id));else for(size_t t=0;t<q.document->catalog.targets.size();++t)if(growth_character(*q.document,t))colliders.push_back(q.document->catalog.targets[t].instance);
          std::erase_if(colliders,[&](auto i){return all_simulated.contains(i);});
          auto &group=groups[id];bool changed=!group.simulation||!same_targets(group.targets,targets)||group.colliders!=colliders;
          if(!changed&&group.base!=base){for(const auto &t:targets)changed|=changed_geometry(*base,*group.base,t.instance);for(auto i:colliders)changed|=changed_geometry(*base,*group.base,i);}
          try{
            for(const auto &t:targets)if(t.kind!=runtime::PhysicsKind::rigid||!t.settings.joined||!t.settings.excluded_surfaces.empty()){
              const auto mesh=base->instances[t.instance].mesh;for(size_t i=0;i<base->instances.size();++i)if(i!=t.instance&&base->instances[i].mesh==mesh)throw std::runtime_error("共享网格不能独立形变，请先创建独立对象");
            }
            if(changed){if(!group.simulation)group.simulation=std::make_unique<PhysicsRunner>();group.simulation->update(base,targets,q.options,colliders,cancelled);group.targets=targets;group.colliders=colliders;group.base=base;}
            else group.simulation->options(q.options,cancelled);
            group.error.clear();
          }catch(const std::exception &e){if(cancelled())throw;group.error=e.what();}
        }
        if(cancelled())continue;
        std::erase_if(groups,[&](const auto &entry){return !wanted.contains(entry.first);});active=q;frame_requested_=true;frame_dirty=true;next_tick=Clock::now();next_frame=next_tick;
      }catch(const std::exception &e){if(!cancelled()){auto result=std::make_shared<PhysicsResult>();result->serial=q.serial;result->generation=q.document->generation;result->error=e.what();active=q;std::lock_guard lock(mutex_);result_.swap(result);}}
    }
    if(!active.document)continue;
    const auto tick=Clock::now();
    if(tick>=next_tick){
      for(auto &[id,group]:groups){if(!group.simulation||!group.error.empty())continue;const auto steps=group.simulation->stats().steps;frame_dirty|=group.observed_steps!=steps;group.observed_steps=steps;}
      next_tick=Clock::now()+std::chrono::milliseconds(16);
    }
    busy_=!active.options.paused&&std::any_of(groups.begin(),groups.end(),[](const auto &entry){return entry.second.error.empty()&&(entry.second.simulation&&entry.second.simulation->stats().active_objects>0);});
    if(!frame_requested_||Clock::now()<next_frame||active.serial!=serial_)continue;
    frame_requested_=false;if(!frame_dirty)continue;frame_dirty=false;const auto frame_now=Clock::now();const double interval=1.0/active.options.refresh_hz;next_frame=interval>=std::chrono::duration<double>(Clock::time_point::max()-frame_now).count()?Clock::time_point::max():frame_now+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(interval));
    auto result=std::make_shared<PhysicsResult>();result->serial=active.serial;result->generation=active.document->generation;
    try{
      std::set<uint32_t> meshes,instances;
      for(auto &[id,group]:groups){if(!group.error.empty()){if(!result->stats.warning.empty())result->stats.warning+="；";result->stats.warning+="部分物理暂未应用："+group.error;continue;}auto solved=group.simulation->output([&]{return stop.stop_requested()||reset_.load();});result->group_steps[id]=solved.stats.steps;result->stats.steps=std::max(result->stats.steps,solved.stats.steps);result->stats.active_objects+=solved.stats.active_objects;result->stats.objects+=solved.stats.objects;result->stats.particles+=solved.stats.particles;result->stats.prepare_ms+=solved.stats.prepare_ms;result->stats.solve_ms+=solved.stats.solve_ms;
        for(auto &e:solved.delta.meshes){if(!meshes.insert(e.index).second)throw std::runtime_error("共享网格不能重复独立模拟");result->delta.meshes.push_back(std::move(e));}
        for(auto &e:solved.delta.instances){instances.insert(e.index);result->delta.instances.push_back(e);}
      }
      if(!result->stats.objects&&!result->stats.warning.empty())result->error=result->stats.warning;
      // 关闭或取消选择只恢复相应对象，其他角色继续模拟。
      for(auto i:previous_meshes)if(!meshes.contains(i))result->delta.meshes.push_back({i,base->meshes[i].positions});
      for(auto i:previous_instances)if(!instances.contains(i))result->delta.instances.push_back({i,base->instances[i].transform});
      // 仅与已经提交的画面比较；丢弃或被覆盖的结果不会吞掉待显示的变化。
      std::erase_if(result->delta.meshes,[&](const auto &e){const auto h=positions_hash(e.positions);auto known=applied_meshes.find(e.index);if(!base_hashes.contains(e.index))base_hashes[e.index]=positions_hash(base->meshes[e.index].positions);const auto before=known==applied_meshes.end()?base_hashes[e.index]:known->second;if(before==h)return true;result->mesh_hashes[e.index]=h;return false;});
      std::erase_if(result->delta.instances,[&](const auto &e){const auto known=applied_instances.find(e.index);const auto &before=known==applied_instances.end()?base->instances[e.index].transform:known->second;if(before==e.transform)return true;result->transforms[e.index]=e.transform;return false;});
      if(!result->delta.meshes.empty()||!result->delta.instances.empty()){
        result->scene=std::make_shared<ir::Scene>(*base);
        for(const auto &e:result->delta.meshes){result->restore.meshes.push_back({e.index,base->meshes[e.index].positions});result->scene->meshes[e.index].positions=e.positions;}
        for(const auto &e:result->delta.instances){result->restore.instances.push_back({e.index,base->instances[e.index].transform});result->scene->instances[e.index].transform=e.transform;}
        result->delta=runtime::update_geometry_shells(*result->scene,std::move(result->delta));
        for(const auto &e:result->delta.meshes)if(!result->mesh_hashes.contains(e.index))result->mesh_hashes[e.index]=positions_hash(e.positions);
        for(const auto &e:result->delta.meshes)if(std::none_of(result->restore.meshes.begin(),result->restore.meshes.end(),[&](const auto &v){return v.index==e.index;}))result->restore.meshes.push_back({e.index,base->meshes[e.index].positions});
      }
      // 保留恢复数据，避免最新帧邮箱丢弃一次性的关闭恢复。
      previous_meshes.insert(meshes.begin(),meshes.end());previous_instances.insert(instances.begin(),instances.end());
    }catch(const std::exception &e){result->error=e.what();result->delta={};}
    {std::lock_guard lock(mutex_);if(active.serial==serial_&&!reset_)result_.swap(result);}
  }
  // 在文档和挂接参考释放前停止各角色线程。
  groups.clear();
}){}
PhysicsService::~PhysicsService(){worker_.request_stop();ready_.notify_all();}
uint64_t PhysicsService::request(std::shared_ptr<const Document> d,Snapshot s,runtime::PhysicsOptions o){runtime::validate_physics(o);const auto id=++serial_;{std::lock_guard lock(mutex_);pending_=Request{id,std::move(d),std::move(s),o};if(result_)retired_.push_back(std::move(result_));}ready_.notify_all();return id;}
void PhysicsService::cancel(){++serial_;reset_=true;{std::lock_guard lock(mutex_);pending_.reset();if(result_)retired_.push_back(std::move(result_));}ready_.notify_all();}
void PhysicsService::acknowledge(std::shared_ptr<PhysicsResult> r){{std::lock_guard lock(mutex_);if(acknowledged_)retired_.push_back(std::move(acknowledged_));acknowledged_=std::move(r);}ready_.notify_all();}
void PhysicsService::retire(std::shared_ptr<PhysicsResult> r){if(!r)return;{std::lock_guard lock(mutex_);retired_.push_back(std::move(r));}ready_.notify_all();}
std::shared_ptr<PhysicsResult> PhysicsService::take(){std::lock_guard lock(mutex_);return std::exchange(result_,{});}
}
