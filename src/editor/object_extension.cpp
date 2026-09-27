#include "editor/object_extension.h"
#include "runtime/decimal_float.h"
#include <cmath>

namespace dfv::editor {
namespace {
int skin_index(const Document &d,size_t target){const auto i=d.catalog.targets.at(target).instance;for(size_t s=0;s<d.skeletons.skins.size();++s)if(d.skeletons.skins[s].instance==i)return int(s);return -1;}
double root_scale(const Document &d,const Snapshot &snapshot,size_t t,const runtime::Properties &p){
  const auto &g=d.formulas.graphs.at(t);if(g.skin<0)return 1;const auto &skin=d.skeletons.skins.at(g.skin);const auto &poses=snapshot.poses.at(g.skin);
  runtime::FormulaRuntime formula(g);
  auto component=[](ir::Vec3 v,uint32_t a){return a==0?v.x:a==1?v.y:v.z;};
  for(size_t c=0;c<g.channels.size();++c){const auto &b=g.channels[c].binding;double value=0;
    if(b.property==runtime::Property::morph){if(g.morph_channels.at(b.index)!=int(c))continue;value=p.morphs.at(b.index);formula.set_unlimited(uint32_t(c),p.unlimited_morphs.contains(d.catalog.targets[t].morphs.at(b.index).id));}
    else{const auto &v=poses.at(b.index);const auto &j=skin.joints.at(b.index);switch(b.property){
      case runtime::Property::translation:value=component(v.translation_cm,b.axis);break;
      case runtime::Property::rotation:value=component(v.rotation_degrees,b.axis);break;
      case runtime::Property::scale:value=component(v.scale,b.axis);break;
      case runtime::Property::general_scale:value=v.general_scale*(b.index==0?skin.root_general_scale:1);break;
      case runtime::Property::center:value=component(j.center_cm,b.axis)+component(v.center_offset_cm,b.axis);break;
      case runtime::Property::end:value=component(j.end_cm,b.axis)+component(v.end_offset_cm,b.axis);break;
      case runtime::Property::orientation:value=component(j.orientation_degrees,b.axis)+component(v.orientation_offset_degrees,b.axis);break;
      default:break;
    }}formula.set(uint32_t(c),value);
  }
  formula.evaluate();for(size_t c=0;c<g.channels.size();++c)if(g.channels[c].binding.property==runtime::Property::general_scale&&g.channels[c].binding.index==0)return runtime::decimal_derived_float(formula.values()[c]);
  return runtime::decimal_derived_float(skin.root_general_scale*poses.at(0).general_scale);
}
class MeasurementContext {
  const Document *document_=nullptr;size_t target_=0;
  ir::Scene scene_;std::vector<runtime::Target> targets_;std::vector<runtime::Skin> skins_;std::vector<runtime::FormulaGraph> graphs_;
  std::unique_ptr<runtime::DeformationRuntime> runtime_;std::unique_ptr<runtime::WeightTopology> topology_;
  runtime::Properties previous_;std::vector<std::vector<runtime::JointPose>> previous_poses_;
  runtime::WeightResult cached_;bool cached_valid_=false;
  std::optional<ir::Transform> previous_world_;
  std::shared_ptr<const std::vector<ir::Vec3>> previous_positions_;
public:
  runtime::WeightResult measure(const Document &d,const Snapshot &snapshot,size_t target,std::optional<ir::Transform> world={},std::shared_ptr<const std::vector<ir::Vec3>> positions={}){
    const bool character=snapshot.values.at(target).extension.kind==runtime::ExtensionKind::growth;
    if(document_!=&d||target_!=target){
      runtime_.reset();topology_.reset();cached_valid_=false;scene_={};targets_={d.catalog.targets.at(target)};auto &t=targets_[0];
      const auto &instance=d.loaded.scene.instances.at(t.instance);scene_.meshes={d.loaded.scene.meshes.at(instance.mesh)};scene_.meshes[0].subdivision={};scene_.meshes[0].graft_vertex_pairs.clear();scene_.meshes[0].graft_hidden_polygons.clear();scene_.meshes[0].graft_target_vertices=0;
      scene_.instances={instance};scene_.instances[0].mesh=0;scene_.instances[0].graft_source=-1;scene_.instances[0].prototype=-1;
      t.instance=0;t.parent.clear();t.ancestors.clear();t.conform_target.clear();t.rigid_follow={};t.smoothing.enabled=false;t.attachment_bind_rest=false;
      skins_.clear();const auto s=skin_index(d,target);if(s>=0){skins_={d.skeletons.skins[size_t(s)]};skins_[0].instance=0;}
      graphs_={d.formulas.graphs.at(target)};graphs_[0].skin=s>=0?0:-1;
      topology_=std::make_unique<runtime::WeightTopology>(scene_.meshes[0]);runtime_=std::make_unique<runtime::DeformationRuntime>(scene_,targets_,skins_,graphs_);document_=&d;target_=target;
    }
    auto value=snapshot.values.at(target);runtime::validate_extension(value.extension);
    // 普通物体直接使用渲染工作线程发布的基础网格副本，包含服装跟随、骨骼与刚性附件结果。
    if(!character&&positions&&world){
      if(!cached_valid_||positions!=previous_positions_||world!=previous_world_)cached_=topology_->measure(*positions,*world,false,value.extension.density);
      previous_positions_=std::move(positions);previous_world_=world;cached_valid_=true;auto result=cached_;result.coefficient=value.extension.density/1000;result.kg=result.liters*result.coefficient;return result;
    }
    std::vector<std::vector<runtime::JointPose>> poses;const auto s=skin_index(d,target);if(s>=0)poses={snapshot.poses.at(s)};
    const auto &a=value.transform,&b=previous_.transform;
    const bool same=cached_valid_&&value.morphs==previous_.morphs&&value.unlimited_morphs==previous_.unlimited_morphs&&poses==previous_poses_&&a.translation_cm==b.translation_cm&&a.rotation_degrees==b.rotation_degrees&&a.scale==b.scale&&a.general_scale==b.general_scale&&value.extension.kind==previous_.extension.kind&&value.extension.age==previous_.extension.age&&value.extension.strength==previous_.extension.strength;
    if(same&&world==previous_world_){auto result=cached_;if(!character){result.coefficient=value.extension.density/1000;result.kg=result.liters*result.coefficient;}return result;}
    previous_=value;previous_poses_=poses;previous_world_=world;cached_valid_=false;
    const auto &loaded_world=d.loaded.scene.instances.at(d.catalog.targets.at(target).instance).transform;
    const auto unparented=runtime::parameter_transform(value.transform,targets_[0],loaded_world)*loaded_world;
    std::vector<std::string> missing;
    if(character){
      if(s<0)throw std::runtime_error("当前角色缺少可测量骨架");
      const auto actual=runtime_->resolve_poses({value},poses).at(0).at(0);
      poses[0].assign(skins_[0].joints.size(),{});poses[0][0]=snapshot.poses.at(s).at(0);poses[0][0].translation_cm={};poses[0][0].rotation_degrees={};
      missing=runtime::apply_growth(targets_[0],value);
      const auto measured=runtime_->resolve_poses({value},poses).at(0).at(0);
      if(actual.general_scale<=0||measured.general_scale<=0||actual.scale.x<=0||actual.scale.y<=0||actual.scale.z<=0||measured.scale.x<=0||measured.scale.y<=0||measured.scale.z<=0)throw std::runtime_error("测量需要正数根节点缩放");
      value.transform.general_scale*=actual.general_scale/measured.general_scale;
      value.transform.scale.x*=actual.scale.x/measured.scale.x;value.transform.scale.y*=actual.scale.y/measured.scale.y;value.transform.scale.z*=actual.scale.z/measured.scale.z;
    }
    runtime_->evaluate({value},poses);const auto measurement_world=world?*world*ir::inverse(unparented)*scene_.instances[0].transform:scene_.instances[0].transform;
    auto result=topology_->measure(scene_.meshes[0].positions,measurement_world,character,value.extension.density);
    result.missing_morphs=std::move(missing);
    cached_=result;cached_valid_=true;return result;
  }
};
}
bool growth_character(const Document &d,size_t t){
  const auto instance=d.catalog.targets.at(t).instance;const int index=skin_index(d,t);
  if(index<0||!d.catalog.targets[t].conform_target.empty())return false;
  for(const auto &o:d.loaded.objects)if(o.instance==instance){
    if(!o.figure||!o.conform_target.empty())return false;
    if(o.content_type=="Actor"||o.content_type.starts_with("Actor/"))return true;
    if(!o.content_type.empty())return false;
  }
  // 缺少元数据的旧资产只接受完整的人形骨架，不能用 figure / skin 判定道具。
  const auto &skin=d.skeletons.skins[size_t(index)];
  auto has=[&](std::initializer_list<const char *> names){for(const auto &j:skin.joints)for(auto name:names)if(j.id==name||j.name==name)return true;return false;};
  return has({"hip","pelvis"})&&has({"head"})&&has({"lHand"})&&has({"rHand"})&&has({"lFoot"})&&has({"rFoot"});
}
std::vector<std::string> edit_growth(const Document &d,Snapshot &snapshot,size_t t,runtime::ObjectExtension next,bool apply_shape,double scale_step){
  runtime::validate_extension(next);if(!growth_character(d,t))throw std::runtime_error("请选择角色本体或其骨骼");
  const auto &old=snapshot.values.at(t);auto value=old;value.extension=next;std::vector<std::string> missing;
  if(apply_shape)missing=runtime::apply_growth(d.catalog.targets.at(t),value);
  const double base_before=root_scale(d,snapshot,t,old),base_after=root_scale(d,snapshot,t,value),before=base_before*old.transform.general_scale;
  const double delta=next.sensitivity*(next.age-old.extension.age+scale_step)*.01,after=before+delta;
  if(before<=0||after<=0||base_after<=0||!std::isfinite(after))throw std::runtime_error("本次生长会使缩放小于或等于零，已保留原值");
  value.transform.general_scale=after/base_after;
  double origin_y=0;for(const auto &o:d.loaded.objects)if(o.instance==d.catalog.targets[t].instance){origin_y=o.translation_cm.y;break;}
  value.transform.translation_cm.y=float((origin_y+old.transform.translation_cm.y)*after/before-origin_y);
  runtime::validate_transform(value.transform);snapshot.values[t]=std::move(value);return missing;
}
runtime::WeightResult measure_object(const Document &d,const Snapshot &s,size_t t,std::optional<ir::Transform> world){MeasurementContext c;return c.measure(d,s,t,world);}
MeasurementService::MeasurementService():worker_([this](std::stop_token stop){
  MeasurementContext context;std::shared_ptr<const Document> active;
  while(!stop.stop_requested()){Request request;{std::unique_lock lock(mutex_);if(!ready_.wait(lock,stop,[&]{return pending_.serial!=0;}))return;request=std::move(pending_);pending_={};}
    active=request.document;if(!active){context=MeasurementContext{};continue;}
    Result r;r.serial=request.serial;try{r.weight=context.measure(*active,request.snapshot,request.target,request.world,request.positions);}catch(const std::exception &e){r.error=e.what();}
    {std::lock_guard lock(mutex_);if(!pending_.serial)result_=std::move(r);}
  }
}){}
MeasurementService::~MeasurementService(){worker_.request_stop();ready_.notify_all();}
void MeasurementService::request(uint64_t serial,std::shared_ptr<const Document> d,Snapshot s,size_t target,std::optional<ir::Transform> world,std::shared_ptr<const std::vector<ir::Vec3>> positions){std::lock_guard lock(mutex_);pending_={serial,std::move(d),std::move(s),target,world,std::move(positions)};ready_.notify_all();}
MeasurementService::Result MeasurementService::result(){std::lock_guard lock(mutex_);return result_;}
}
