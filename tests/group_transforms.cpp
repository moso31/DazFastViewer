#include "editor/group_transforms.h"
#include "editor/gizmo.h"
#include "editor/scene_extension.h"
#include "editor/ground_selection.h"
#include "runtime/deformation.h"
#include <iostream>
#include <fstream>
using namespace dfv;using namespace dfv::editor;
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static void close(const ir::Transform &a,const ir::Transform &b,const char *message){for(size_t i=0;i<a.value.size();++i)check(std::abs(a.value[i]-b.value[i])<2e-5,message);}
static std::shared_ptr<Document> fixture(){
  auto d=std::make_shared<Document>();d->generation=7;ir::Material material;material.id="material";d->loaded.scene.materials.push_back(material);
  daz::AssetNode outer{"outer","","Outer",true},inner{"inner","#parent","Inner",true};
  outer.world=outer.edit_frame=runtime::make_transform({{20,50,30},{15,25,35}});outer.translation_frame=runtime::make_transform({{},{20,10,5}});
  inner.world=inner.edit_frame=runtime::make_transform({{30,70,50},{10,-15,20}});inner.translation_frame=outer.world;d->loaded.nodes={outer,inner};
  for(const auto *name:{"parent","child","sibling","outside"}){
    const auto i=uint32_t(d->catalog.targets.size());ir::Mesh mesh;mesh.id=name;mesh.material_slots={"surface"};mesh.positions={{0,0,0},{.4f,0,0},{0,0,.4f}};mesh.triangles={{{0,1,2}}};d->loaded.scene.meshes.push_back(mesh);
    ir::Instance instance;instance.id=name;instance.mesh=i;instance.materials={0};instance.transform=runtime::make_transform({{float(20*i),60,40},{5,10,15}});d->loaded.scene.instances.push_back(instance);
    runtime::Target target;target.id=std::string(name)+"/shape";target.instance=i;target.has_edit_frame=true;target.edit_frame=instance.transform;target.translation_frame=outer.world;
    target.ancestors=i==0?std::vector<std::string>{"#outer"}:i==1?std::vector<std::string>{"#inner","#parent","#outer"}:i==2?std::vector<std::string>{"#outer"}:std::vector<std::string>{};
    if(!target.ancestors.empty())target.parent=target.ancestors.front();d->catalog.targets.push_back(target);
    daz::AssetObject object;object.id=name;object.parent=target.parent;object.instance=i;object.edit_frame=target.edit_frame;object.translation_frame=target.translation_frame;d->loaded.objects.push_back(object);
    d->loaded.nodes.push_back({name,target.parent,name,false});
  }
  d->loaded.nodes.push_back({"light","#inner","Light",false});ir::AreaLight light;light.id="light";d->loaded.scene.lights.push_back(light);
  d->formulas.graphs.resize(4);return d;
}
static ir::Scene evaluate(const Document &d,const Snapshot &s){auto scene=d.loaded.scene;runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(s.values,s.poses);return scene;}
static void nested(){
  const auto d=fixture();auto s=initial_snapshot(*d);
  s.group_transforms["outer"]={{15,-10,25},{15,20,35},{1.1f,.9f,1.2f}};s.group_transforms["inner"]={{-5,12,3},{-10,25,15},{.8f,1.1f,1.3f}};
  s.values[0].transform={{5,7,3},{15,20,-5},{1.2f,1.3f,.8f}};s.values[1].transform={{-2,6,5},{5,-15,20}};
  const auto original=evaluate(*d,s);const auto effective=transformed_groups(d,s.group_transforms);const auto scene=evaluate(*effective,s);
  const auto &outer=*group_node(*d,"outer"),&inner=*group_node(*d,"inner");
  const auto a=runtime::parameter_transform(s.group_transforms.at("outer"),group_frame(outer),outer.world),b=runtime::parameter_transform(s.group_transforms.at("inner"),group_frame(inner),inner.world);
  const auto parent=runtime::parameter_transform(s.values[0].transform,d->catalog.targets[0],d->loaded.scene.instances[0].transform);
  const auto child=runtime::parameter_transform(s.values[1].transform,d->catalog.targets[1],d->loaded.scene.instances[1].transform);
  close(scene.instances[0].transform,a*original.instances[0].transform,"outer group lost parent transform");
  close(scene.instances[1].transform,a*parent*b*child*d->loaded.scene.instances[1].transform,"nested group / prop order incorrect");
  close(scene.instances[2].transform,a*original.instances[2].transform,"outer sibling did not follow");
  close(scene.instances[3].transform,original.instances[3].transform,"unrelated object moved");
  close(group_world(*effective,scene,"inner"),a*parent*b*inner.world,"group pivot lost edited parent");
  const auto lights=group_lights(*effective,scene,ObjectHierarchy(*effective,false),effective->loaded.scene.lights,{});
  close(lights[0].transform,a*parent*b*d->loaded.scene.lights[0].transform,"group light lost its group or edited prop parent");
  check(group_instances(*d,"inner")==std::vector<uint32_t>{1}&&group_instances(*d,"outer")==std::vector<uint32_t>({0,1,2}),"wrong group members");
  for(auto space:{GizmoSpace::local,GizmoSpace::world})for(auto tool:{GizmoTool::translate,GizmoTool::rotate,GizmoTool::scale}){
    const auto before=group_world(*effective,scene,"inner");GizmoDrag drag;drag.object(group_frame(inner),s.group_transforms.at("inner"),inner.world,before);
    const auto pivot=drag.pivot();CameraState camera;camera.target={pivot.x,pivot.y,pivot.z};camera.distance=5;camera.yaw=.6f;camera.pitch=.4f;
    const GizmoSettings settings{tool,space};drag.layout(camera,1000,800,settings,1);const int handle=tool==GizmoTool::scale?3:0;
    std::vector<ir::Vec2> points;for(const auto &line:drag.shape.lines)if(line.handle==handle)points.push_back({(line.a.x+line.b.x)*.5f,(line.a.y+line.b.y)*.5f});
    check(!points.empty(),"group gizmo missing");const auto at=points[tool==GizmoTool::rotate?9:0];runtime::PosePointer pointer;pointer.serial=pointer.revision=1;pointer.held=true;pointer.x=pointer.start_x=int(std::round(at.x));pointer.y=pointer.start_y=int(std::round(at.y));
    const auto proxy=group_proxy(scene,{1},before);check(proxy.triangles.size()==1,"group preview geometry missing");
    check(drag.begin(pointer,camera,1000,800,settings,1,proxy),"group gizmo hit failed");
    if(tool==GizmoTool::rotate){for(size_t i=10;i<20;++i){pointer.x=int(std::round(points[i].x));pointer.y=int(std::round(points[i].y));pointer.moved=true;++pointer.revision;drag.update(pointer);}}
    else{pointer.x+=45;pointer.y-=10;pointer.moved=true;++pointer.revision;drag.update(pointer);}
    check(drag.changed(),"group drag did not change");auto after=s;after.group_transforms["inner"]=drag.transform;const auto updated=transformed_groups(d,after.group_transforms);const auto result=evaluate(*updated,after);
    close(result.instances[1].transform,drag.world*ir::inverse(before)*scene.instances[1].transform,"group release jumped from preview");
    close(result.instances[0].transform,scene.instances[0].transform,"inner group changed parent");close(result.instances[2].transform,scene.instances[2].transform,"inner group changed sibling");
    pointer.x=pointer.start_x;pointer.y=pointer.start_y;++pointer.revision;drag.update(pointer);check(!drag.changed(),"group drag origin did not restore");
  }
  auto saved=snapshot_json(*d,s);auto restored=initial_snapshot(*d);apply_snapshot_json(*d,restored,saved);check(restored.group_transforms==s.group_transforms&&restored.values==s.values,"group save roundtrip failed");
  saved.erase("groups");apply_snapshot_json(*d,restored,saved);check(restored.group_transforms.empty(),"legacy file retained stale group values");
  check(transformed_groups(d,{})==d,"reset did not restore original scene");close(evaluate(*d,s).instances[1].transform,original.instances[1].transform,"group edited source scene");
  auto invalid=s.group_transforms;invalid["missing"]={};bool rejected=false;try{transformed_groups(d,invalid);}catch(...){rejected=true;}check(rejected,"missing group accepted");
  remove_target(*d,s,0);check(!s.group_transforms.contains("inner")&&s.group_transforms.contains("outer"),"delete did not prune group edits");
}
static void bone_parent(){
  auto d=fixture();d->loaded.nodes[1].parent="#joint";d->loaded.nodes.push_back({"joint","#parent","Joint",false});d->catalog.targets[1].ancestors={"#inner","#joint","#parent","#outer"};
  runtime::Skin skin;skin.instance=0;skin.joints.resize(1);skin.joints[0].scene_id="joint";skin.joints[0].center_cm={20,30,10};skin.initial.resize(1);skin.weights={{{0,1}},{{0,1}},{{0,1}}};d->skeletons.skins={skin};
  auto s=initial_snapshot(*d);s.group_transforms["outer"].rotation_degrees.y=25;s.group_transforms["inner"].translation_cm.x=10;s.poses[0][0].rotation_degrees.z=35;
  const auto effective=transformed_groups(d,s.group_transforms);const auto scene=evaluate(*effective,s);
  const auto world=group_world(*effective,scene,"inner",s.poses);const auto child=world*ir::inverse(group_node(*effective,"inner")->world)*effective->loaded.scene.instances[1].transform;
  close(scene.instances[1].transform,child,"bone-parented group pivot does not follow children");
}
static void instance_parent(){
  auto d=fixture();auto copy=d->loaded.scene.instances[3];copy.id="copy";copy.instance_node=copy.instance_group="copy-node";copy.prototype=3;d->loaded.scene.instances.push_back(copy);d->loaded.nodes.push_back({"copy-node","#inner","Copy",false});
  auto s=initial_snapshot(*d);s.group_transforms["outer"].rotation_degrees.y=35;s.group_transforms["inner"].translation_cm.y=70;s.values[0].transform.rotation_degrees.z=25;s.instance_ground["copy"].offset_m=.15;
  const auto effective=transformed_groups(d,s.group_transforms);auto scene=evaluate(*effective,s);
  const auto bases=group_instance_bases(*effective,scene,ObjectHierarchy(*effective,false),s.poses);apply_instance_ground(scene,effective->loaded.scene,s.instance_ground,nullptr,&bases);
  auto expected=group_world(*effective,scene,"inner")*ir::inverse(group_node(*d,"inner")->world)*copy.transform;expected.value[11]+=.15f;
  close(scene.instances[4].transform,expected,"group instance lost edited parent or world ground offset");close(scene.instances[3].transform,copy.transform,"group instance moved its prototype");
  check(group_instances(*d,"inner")==std::vector<uint32_t>({1,4}),"group omitted its instance child");
  ir::Delta repeat;apply_instance_ground(scene,effective->loaded.scene,s.instance_ground,&repeat,&bases);check(repeat.instances.empty(),"group instance ground offset accumulated");
}
static void unrelated_skeleton(){
  auto d=fixture();runtime::Skin skin;skin.instance=3;skin.joints.resize(1);skin.initial.resize(1);skin.weights={{{0,1}},{{0,1}},{{0,1}}};d->skeletons.skins={skin};
  auto s=initial_snapshot(*d);s.poses[0][0].general_scale=1.f/61;s.poses[0][0].translation_cm={30,40,50};
  s.group_transforms["outer"].translation_cm={20,-30,40};
  for(const auto &parent:std::vector<std::string>{"","#","#container"}){
    d->loaded.nodes[0].parent=parent;if(parent=="#container")d->loaded.nodes.push_back({"container","","Container",true});
    const auto effective=transformed_groups(d,s.group_transforms);auto scene=evaluate(*effective,s);const ObjectHierarchy hierarchy(*effective,false);
    close(node_parent_delta(*effective,scene,hierarchy,"outer",s.poses),{},"root group matched an unrelated joint with empty scene ID");
    close(group_world(*effective,scene,"outer",s.poses),group_node(*effective,"outer")->world,"root group inherited an unrelated figure's scale or pose");
    const auto lights=group_lights(*effective,scene,hierarchy,effective->loaded.scene.lights,s.poses);
    close(lights[0].transform,effective->loaded.scene.lights[0].transform,"group light inherited unrelated figure pose");
  }
}
static void incremental(){
  auto d=fixture();d->loaded.nodes[1].parent="#joint";d->loaded.nodes.push_back({"joint","#parent","Joint",false});d->catalog.targets[1].ancestors={"#inner","#joint","#parent","#outer"};
  runtime::Skin skin;skin.instance=0;skin.joints.resize(1);skin.joints[0].scene_id="joint";skin.joints[0].center_cm={20,30,10};skin.initial.resize(1);skin.weights={{{0,1}},{{0,1}},{{0,1}}};d->skeletons.skins={skin};
  auto s=initial_snapshot(*d);s.poses[0][0].rotation_degrees.z=35;s.values[0].transform.rotation_degrees.x=12;s.values[1].transform.translation_cm.z=7;
  auto scene=d->loaded.scene;runtime::DeformationRuntime runtime(scene,d->catalog.targets,d->skeletons.skins,d->formulas.graphs);runtime.evaluate(s.values,s.poses);
  const auto original=scene;const auto skin_count=runtime.skin_stats().evaluations,morph_count=runtime.morph_stats().morph_evaluations;
  for(int step=0;step<5;++step){
    s.group_transforms["outer"]={{float(step*10),float(step*-7),12},{float(step*7),20,35},{1.1f,.9f,1.2f}};
    s.group_transforms["inner"]={{-5,12,3},{-10,float(step*13),15},{.8f,1.1f,1.3f}};if(step==4)s.group_transforms.clear();
    const GroupFrames frames(*d,s.group_transforms);std::vector<ir::Transform> deltas;for(const auto &id:frames.hierarchy.targets)deltas.push_back(frames.delta(id));
    check(runtime.reframe(deltas),"ordinary group should reuse runtime");const auto delta=runtime.evaluate(s.values,s.poses);
    const auto effective=transformed_groups(d,s.group_transforms);const auto expected=evaluate(*effective,s);
    for(const auto &id:frames.hierarchy.groups)close(group_world(*d,scene,id,s.poses,&frames),group_world(*effective,expected,id,s.poses),"lightweight group frame differs from document frame");
    const auto light=group_lights(*d,scene,frames.hierarchy,effective->loaded.scene.lights,s.poses,&frames),reference_light=group_lights(*effective,expected,frames.hierarchy,effective->loaded.scene.lights,s.poses);
    for(size_t i=0;i<light.size();++i)close(light[i].transform,reference_light[i].transform,"lightweight group light lost parent frame");
    for(size_t i=0;i<scene.instances.size();++i)close(scene.instances[i].transform,expected.instances[i].transform,"incremental group differs from full evaluation");
    check(delta.meshes.empty()&&runtime.skin_stats().evaluations==skin_count&&runtime.morph_stats().morph_evaluations==morph_count,"group move reevaluated geometry");
  }
  // A later bone edit must still evaluate in the current group reference frame.
  s.group_transforms["outer"].rotation_degrees.y=30;GroupFrames frames(*d,s.group_transforms);std::vector<ir::Transform> deltas;for(const auto &id:frames.hierarchy.targets)deltas.push_back(frames.delta(id));check(runtime.reframe(deltas),"reframe failed");
  s.poses[0][0].rotation_degrees.z=48;runtime.evaluate(s.values,s.poses);const auto expected=evaluate(*transformed_groups(d,s.group_transforms),s);
  for(size_t i=0;i<scene.instances.size();++i)close(scene.instances[i].transform,expected.instances[i].transform,"pose after group move lost attachment frame");
}
static void collective_ground(){
  auto d=fixture();auto s=initial_snapshot(*d);s.group_transforms["outer"].rotation_degrees.z=23;s.values[0].transform.rotation_degrees.x=15;
  auto copy=d->loaded.scene.instances[3];copy.id="copy";copy.instance_node=copy.instance_group="copy-node";copy.prototype=3;d->loaded.scene.instances.push_back(copy);d->loaded.nodes.push_back({"copy-node","#inner","Copy",false});
  s.instance_ground["copy"].offset_m=.15;
  for(auto &v:s.values){v.ground_alignment_ratio=.8;v.ground_alignment_offset_cm=100;v.ground_alignment_body_only=true;}
  auto evaluate_all=[&]{auto effective=transformed_groups(d,s.group_transforms);auto scene=evaluate(*effective,s);const auto bases=group_instance_bases(*effective,scene,ObjectHierarchy(*effective,false),s.poses);apply_instance_ground(scene,effective->loaded.scene,s.instance_ground,nullptr,&bases);return scene;};
  for(const auto &roots:std::vector<std::vector<std::string>>{{"inner","child","copy-node"},{"parent","child","outside"},{"outer","inner","outside"},{"copy-node","outside"}}){
    const auto selection=ground_selection(*d,roots);auto before=evaluate_all();std::vector<ir::Bounds> bounds;std::vector<ir::Transform> worlds;std::map<std::string,ir::Transform> group_worlds;
    for(const auto &i:before.instances){ir::Bounds b;if(i.visible)for(auto p:before.meshes[i.mesh].positions)b.add(i.transform.point(p));bounds.push_back(b);}
    for(const auto &t:d->catalog.targets)worlds.push_back(before.instances[t.instance].transform);
    const auto effective=transformed_groups(d,s.group_transforms);for(const auto &n:effective->loaded.nodes)if(n.group)group_worlds[n.id]=group_world(*effective,before,n.id);
    const auto input=s;const auto shift=ground_vertical_shift(ground_selection_bounds(selection,bounds),0);align_ground_selection(*d,s,selection,bounds,worlds,group_worlds);const auto after=evaluate_all();
    for(size_t i=0;i<before.instances.size();++i){auto expected=before.instances[i].transform;if(std::find(selection.instances.begin(),selection.instances.end(),i)!=selection.instances.end())expected.value[11]+=float(shift);close(after.instances[i].transform,expected,"collective ground changed spacing or moved a child twice");}
    for(size_t t=0;t<s.values.size();++t)check(s.values[t].ground_alignment_ratio==input.values[t].ground_alignment_ratio&&s.values[t].ground_alignment_offset_cm==input.values[t].ground_alignment_offset_cm&&s.values[t].ground_alignment_body_only,"collective ground overwrote character settings");
    s=input;
  }
}
static void scene_motion(const std::filesystem::path &file,const std::string &label,const std::filesystem::path &report,const std::vector<std::filesystem::path> &roots){
  auto d=std::make_shared<Document>();d->generation=1;d->source_file=file;
  d->loaded=daz::load(file,{roots});d->catalog=daz::discover_morphs(d->loaded,roots,{},true);d->skeletons=daz::load_skeletons(d->loaded);d->formulas=daz::enable_formulas(d->catalog,d->skeletons);
  auto s=initial_snapshot(*d);const daz::AssetNode *node=nullptr;for(const auto &n:d->loaded.nodes)if(n.group&&(n.label==label||n.id==label)){node=&n;break;}check(node,"scene group not found");
  auto base=d->loaded.scene;runtime::DeformationRuntime base_runtime(base,d->catalog.targets,d->skeletons.skins,d->formulas.graphs);base_runtime.evaluate(s.values,s.poses);
  const auto members=group_instances(*d,node->id);nlohmann::json log={{"group",node->id},{"world",node->world.value},{"edit_frame",node->edit_frame.value},{"translation_frame",node->translation_frame.value},{"cases",nlohmann::json::array()}};double maximum=0;
  auto length=[](ir::Vec3 p){return std::sqrt(double(p.x)*p.x+double(p.y)*p.y+double(p.z)*p.z);};
  for(auto space:{GizmoSpace::local,GizmoSpace::world})for(int axis=0;axis<3;++axis){
    GizmoDrag drag;const auto world=group_world(*d,base,node->id,base_runtime.effective_poses());drag.object(group_frame(*node),{},node->world,world);const auto pivot=drag.pivot();CameraState camera;camera.target={pivot.x,pivot.y,pivot.z};camera.distance=5;camera.yaw=.6f;camera.pitch=.4f;const GizmoSettings settings{GizmoTool::translate,space};drag.layout(camera,1000,800,settings,1);
    const auto shape=drag.shape;ir::Vec2 at{};bool hit=false;for(const auto &line:shape.lines)if(line.handle==axis){const ir::Vec2 p{(line.a.x+line.b.x)*.5f,(line.a.y+line.b.y)*.5f};if(shape.hit(p.x,p.y)==axis){at=p;hit=true;break;}}check(hit,"scene group handle cannot be hit");
    runtime::PosePointer pointer;pointer.serial=pointer.revision=1;pointer.held=true;pointer.x=pointer.start_x=int(std::round(at.x));pointer.y=pointer.start_y=int(std::round(at.y));check(drag.begin(pointer,camera,1000,800,settings,1,group_proxy(base,members,world)),"scene group drag failed");
    for(int step=1;step<=10;++step){pointer.x=pointer.start_x+step*6;pointer.y=pointer.start_y-step*2;pointer.moved=true;++pointer.revision;drag.update(pointer);}
    auto next=s;next.group_transforms[node->id]=drag.transform;const auto effective=transformed_groups(d,next.group_transforms);auto result=evaluate(*effective,next);const auto delta=drag.world*ir::inverse(world);nlohmann::json row={{"space",int(space)},{"axis",axis},{"input_translation",{drag.transform.translation_cm.x,drag.transform.translation_cm.y,drag.transform.translation_cm.z}},{"preview_delta",delta.value},{"children",nlohmann::json::array()}};
    for(auto i:members){const auto &old=base.instances.at(i),&current=result.instances.at(i);const auto expected=delta*old.transform;double error=0;const auto &before=base.meshes.at(old.mesh).positions,&after=result.meshes.at(current.mesh).positions;check(before.size()==after.size(),"group translation changed topology");for(size_t v=0;v<before.size();++v){const auto a=expected.point(before[v]),b=current.transform.point(after[v]);error=std::max(error,length({a.x-b.x,a.y-b.y,a.z-b.z}));}maximum=std::max(maximum,error);row["children"].push_back({{"id",old.id},{"before",old.transform.value},{"expected",expected.value},{"actual",current.transform.value},{"vertex_error_m",error}});}
    log["cases"].push_back(row);
  }
  log["max_vertex_error_m"]=maximum;std::ofstream(report)<<log.dump(2);std::cout<<"scene: "<<node->label<<" members="<<members.size()<<" max vertex error="<<maximum<<" m\n";check(maximum<1e-4,"scene group preview/commit world vertices mismatch");
}
int main(int argc,char **argv){try{if(argc>=4){std::vector<std::filesystem::path> roots;for(int i=4;i<argc;++i)roots.push_back(std::filesystem::u8path(argv[i]));scene_motion(std::filesystem::u8path(argv[1]),argv[2],std::filesystem::u8path(argv[3]),roots);return 0;}nested();bone_parent();instance_parent();unrelated_skeleton();incremental();collective_ground();std::cout<<"group transforms: nested props, pivots, six drag cases, proxy/commit, bone parenting, unrelated skeleton, instances/ground, save/reset/delete passed\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
