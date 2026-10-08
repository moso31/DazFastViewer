#include "editor/scene_extension.h"
#include "city/document.h"
#include "water/document.h"
#include "cloud/document.h"
#include "editor/group_transforms.h"
#include "render_ir/options_json.h"
#include "runtime/physics_json.h"
#include "daz/documents.h"
#include <fstream>
#include <chrono>
#include <cmath>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#endif

namespace dfv::editor {
namespace {
using J=nlohmann::json;namespace fs=std::filesystem;
std::string path_string(const fs::path &p){const auto s=p.generic_u8string();return {s.begin(),s.end()};}
J vec(ir::Vec3 v){return J::array({v.x,v.y,v.z});}
ir::Vec3 vector(const J &v){if(v.size()!=3)throw std::runtime_error("DUFEX 向量长度错误");ir::Vec3 r{v.at(0).get<float>(),v.at(1).get<float>(),v.at(2).get<float>()};if(!std::isfinite(r.x)||!std::isfinite(r.y)||!std::isfinite(r.z))throw std::runtime_error("DUFEX 数值无效");return r;}
template<class T> J transform(const T &v){return {{"translation_cm",vec(v.translation_cm)},{"rotation_degrees",vec(v.rotation_degrees)},{"scale",vec(v.scale)},{"general_scale",v.general_scale}};}
template<class T> void read_transform(T &v,const J &j){v.translation_cm=vector(j.at("translation_cm"));v.rotation_degrees=vector(j.at("rotation_degrees"));v.scale=vector(j.at("scale"));v.general_scale=j.at("general_scale").get<decltype(v.general_scale)>();}
J extension(const runtime::ObjectExtension &v){runtime::validate_extension(v);return {{"kind",int(v.kind)},{"age",v.age},{"age_step",v.age_step},{"sensitivity",v.sensitivity},{"strength",v.strength},{"density",v.density}};}
runtime::ObjectExtension read_extension(const J &j){runtime::ObjectExtension v;v.kind=runtime::ExtensionKind(j.at("kind").get<int>());v.age=j.at("age");v.age_step=j.at("age_step");v.sensitivity=j.at("sensitivity");v.strength=j.at("strength");v.density=j.at("density");runtime::validate_extension(v);return v;}
J favorites(const std::optional<runtime::FavoriteState> &v){return v?J{{"nodes",v->nodes},{"fallback",v->fallback}}:J{};}
std::optional<runtime::FavoriteState> read_favorites(const J &j){if(j.is_null())return {};runtime::FavoriteState v;v.nodes=j.at("nodes").get<decltype(v.nodes)>();v.fallback=j.at("fallback").get<decltype(v.fallback)>();return v;}
size_t target_index(const Document &d,const std::string &id){for(size_t i=0;i<d.catalog.targets.size();++i)if(d.catalog.targets[i].id==id)return i;throw std::runtime_error("DUFEX 对象不存在："+id);}
std::shared_ptr<Document> load_source(const fs::path &source,const std::vector<fs::path> &roots,const std::function<void(const std::string &)> &progress,bool defer=false){
  auto d=std::make_shared<Document>();d->source_file=source;
  if(!source.empty()){d->loaded=daz::load(source,{roots,false,defer});std::vector<fs::path> resolved=roots;for(const auto &r:d->loaded.report["content_roots"])resolved.push_back(fs::u8path(r.get<std::string>()));d->catalog=daz::discover_morphs(d->loaded,resolved,progress,true);d->skeletons=daz::load_skeletons(d->loaded);d->formulas=daz::enable_formulas(d->catalog,d->skeletons);}
  return d;
}
void replay(Document &d,const J &operations,const std::vector<fs::path> &roots,const fs::path &folder,const std::function<void(const std::string &)> &progress,int depth=0){
  if(depth>32||!operations.is_array())throw std::runtime_error("DUFEX 场景结构无效");
  auto source_path=[&](const J &j){auto p=fs::u8path(j.get<std::string>());return p.empty()||p.is_absolute()?p:folder/p;};
  for(const auto &op:operations){const auto kind=op.at("op").get<std::string>();
    auto archive=d.loaded.archive;const auto key=op.value("archive",std::string{});if(!key.empty()){if(!d.archives.contains(key))throw std::runtime_error("场景操作缺少底稿："+key);archive=d.archives.at(key);}daz::DocumentScope source_scope(archive);
    if(kind=="append"){auto s=load_source(source_path(op.at("file")),roots,progress,true);s->archives=d.archives;replay(*s,op.at("operations"),roots,folder,progress,depth+1);append_document(d,std::move(*s),op.at("prefix"));}
    else if(kind=="city") {auto config=city::config_from_json(op.at("config"));if(config.directory.is_relative())config.directory=folder/config.directory;city::install(d,city::generate(config,op.at("id"),roots,progress),false);}
    else if(kind=="city_remove")city::remove(d,op.at("id"),false);
    else if(kind=="water")water::install(d,water::from_json(op.at("water")),false);
    else if(kind=="cloud")cloud::install(d,cloud::from_json(op.at("cloud")),false);
    else if(kind=="studio")ir::add_studio(d.loaded.scene);
    else if(kind=="remove"){auto v=initial_snapshot(d);remove_target(d,v,target_index(d,op.at("target")));}
    else if(kind=="remove_light"){auto v=initial_snapshot(d);const auto id=op.at("id").get<std::string>();const auto found=std::find_if(v.lights.begin(),v.lights.end(),[&](const auto &l){return l.id==id;});if(found!=v.lights.end())remove_light(d,v,size_t(found-v.lights.begin()));}
    else if(kind=="attach")attach_import(d,target_index(d,op.at("first")),target_index(d,op.at("host")));
    else if(kind=="fit")fit_attachment(d,target_index(d,op.at("target")),op.at("host").get<std::string>().empty()?-1:int(target_index(d,op.at("host"))));
    else if(kind=="copy_outfit") {std::vector<size_t> clothing,hosts;for(const auto &id:op.at("clothing"))clothing.push_back(target_index(d,id));for(const auto &id:op.at("hosts"))hosts.push_back(target_index(d,id));auto snapshot=initial_snapshot(d);copy_outfit(d,snapshot,target_index(d,op.at("source")),clothing,hosts);}
    else if(kind=="materials")apply_materials(d,target_index(d,op.at("target")),daz::load(source_path(op.at("file")),{roots,false}));
    else if(kind=="surface_materials"||kind=="paste_material"||kind=="material_uv"){
      std::vector<MaterialSurface> surfaces;for(const auto &s:op.at("surfaces")){const auto &scene=d.loaded.scene;auto i=std::find_if(scene.instances.begin(),scene.instances.end(),[&](const auto &v){return v.id==s.at("instance").get<std::string>();});if(i==scene.instances.end())throw std::runtime_error("材质预设对象不存在");const auto &slots=scene.meshes.at(i->mesh).material_slots;auto slot=std::find(slots.begin(),slots.end(),s.at("slot").get<std::string>());if(slot==slots.end())throw std::runtime_error("材质预设表面不存在");surfaces.push_back({size_t(i-scene.instances.begin()),size_t(slot-slots.begin())});}
      auto snapshot=initial_snapshot(d);if(kind=="paste_material")paste_material(d,snapshot,op.at("material"),surfaces);else if(kind=="material_uv")change_material_uv(d,{op.at("uri"),op.at("owner"),op.at("label")},surfaces);else apply_surface_materials(d,snapshot,daz::load(source_path(op.at("file")),{roots,false}),surfaces);
    }
    else throw std::runtime_error("DUFEX 含未知场景操作："+kind);
  }
  d.operations=operations;
}
J structure(const Document &d){
  J result={{"nodes",J::object()},{"instances",J::object()},{"bindings",J::object()}};const auto &scene=d.loaded.scene;
  auto ref=[&](int i){return i<0?std::string{}:scene.instances.at(size_t(i)).id;};
  for(const auto &n:d.loaded.nodes)result["nodes"][n.id]={{"parent",n.parent},{"group",n.group},{"label",n.label},{"visible",n.visible}};
  for(const auto &i:scene.instances){const auto &mesh=scene.meshes.at(i.mesh);result["instances"][i.id]={{"prototype",ref(i.prototype)},{"graft",ref(i.graft_source)},{"shell",ref(i.shell_source)},{"shell_root",ref(i.shell_root)},{"shell_offset",i.shell_offset},{"instance_node",i.instance_node},{"instance_group",i.instance_group},{"vertices",mesh.positions.size()},{"polygons",mesh.source_polygon_count},{"curves",mesh.curves.size()},{"graft_pairs",mesh.graft_vertex_pairs},{"graft_masks",mesh.graft_hidden_polygons},{"shell_masks",mesh.shell_hidden_polygons}};}
  for(const auto &t:d.catalog.targets)result["bindings"][t.id]={{"parent",t.parent},{"fit",t.conform_target},{"collision",t.smoothing.collision_target},{"rigid",t.rigid_follow.target}};
  return result;
}
bool city_operations(const J &ops){for(const auto &op:ops){const auto kind=op.value("op",std::string{});if(kind=="city"||kind=="city_remove"||(kind=="append"&&city_operations(op.at("operations"))))return true;}return false;}
}
std::filesystem::path extension_path(std::filesystem::path p){p.replace_extension(".dufex");return p;}
J snapshot_json(const Document &d,const Snapshot &s,bool water_state){
  if(s.generation!=d.generation||s.values.size()!=d.catalog.targets.size()||s.poses.size()!=d.skeletons.skins.size())throw std::runtime_error("保存快照与场景不匹配");
  J j={{"objects",J::object()},{"poses",J::object()},{"subdivision",s.subdivision_levels},{"options",ir::options_json(s.options)},{"lights",J::array()}};
  if(s.view){for(float v:*s.view)if(!std::isfinite(v))throw std::runtime_error("观察相机数值无效");if((*s.view)[3]<=0)throw std::runtime_error("观察相机距离无效");j["view"]=*s.view;}
  for(size_t t=0;t<s.values.size();++t){const auto &v=s.values[t];const auto &target=d.catalog.targets[t];runtime::validate_transform(v.transform);J m=J::object();
    for(size_t i=0;i<target.morphs.size();++i){const auto &p=target.morphs[i];if(p.alias_morph>=0||runtime::legacy_extension_channel(p.label)||(!p.evaluable&&!p.unsupported.empty()))continue;const auto value=v.morphs.at(i);if(!std::isfinite(value))throw std::runtime_error("Morph 值无效");if(value!=p.initial)m[p.id]=value;}
    j["objects"][target.id]={{"transform",transform(v.transform)},{"visible",v.visible},{"graft_enabled",v.graft_enabled},{"morphs",m},{"unlimited",v.unlimited_morphs},{"ground_ratio",v.ground_alignment_ratio},{"ground_offset_cm",v.ground_alignment_offset_cm},{"ground_body_only",v.ground_alignment_body_only},{"extension",extension(v.extension)},{"favorites",favorites(v.favorites)},{"physics",runtime::physics_json(v.physics)}};
  }
  for(size_t i=0;i<s.poses.size();++i){const auto &skin=d.skeletons.skins[i];runtime::validate_pose(skin,s.poses[i]);J poses=J::object();for(size_t b=0;b<skin.joints.size();++b){const auto &p=s.poses[i][b];auto v=transform(p);v["center_offset_cm"]=vec(p.center_offset_cm);v["end_offset_cm"]=vec(p.end_offset_cm);v["orientation_offset_degrees"]=vec(p.orientation_offset_degrees);poses[skin.joints[b].id]=v;}j["poses"][skin.id]=poses;}
  for(const auto &l:s.lights)j["lights"].push_back({{"id",l.id},{"transform",l.transform.value},{"power",vec(l.power)},{"width",l.width},{"height",l.height},{"kind",int(l.kind)},{"angle",l.angle}});
  j["pins"]=J::array();for(const auto &p:s.pose_pins){const auto &skin=d.skeletons.skins.at(size_t(p.skin));j["pins"].push_back({{"skin",skin.id},{"joint",skin.joints.at(size_t(p.joint)).id},{"world",vec(p.world)},{"position",p.position},{"angle",p.angle},{"orientation",p.world_orientation.value}});}
  validate_material_overrides(d.loaded.scene,s.material_overrides);j["materials"]=s.material_overrides;
  validate_group_transforms(d,s.group_transforms);j["groups"]=J::object();for(const auto &[id,v]:s.group_transforms)j["groups"][id]=transform(v);
  validate_instance_ground(d.loaded.scene,s.instance_ground);j["instance_ground"]=J::object();for(const auto &[id,v]:s.instance_ground)j["instance_ground"][id]={{"offset_m",v.offset_m},{"ratio",v.ratio},{"offset_cm",v.offset_cm},{"body_only",v.body_only}};
  if(water_state){j["waters"]=J::array();for(const auto &w:s.water_overrides)j["waters"].push_back(water::json(*w));}
  if(water_state){j["clouds"]=J::array();for(const auto &v:s.cloud_overrides)j["clouds"].push_back(cloud::json(*v));}
  j["cities"]=city::json(s.city_views);j["control_favorites"]=favorites(s.control_favorites);return j;
}
void apply_snapshot_json(const Document &d,Snapshot &s,const J &j){
  cloud::Clouds clouds;for(const auto &value:j.value("clouds",J::array())){auto v=cloud::from_json(value);const auto *base=cloud::find(d.clouds,v->id);if(!base||cloud::find(clouds,v->id)||v->config.x!=base->config.x||v->config.y!=base->config.y||v->config.height!=base->config.height||v->config.thickness!=base->config.thickness)throw std::runtime_error("体积云快照引用无效");clouds.push_back(std::move(v));}
  water::Waters waters;for(const auto &value:j.value("waters",J::array())){auto w=water::from_json(value);const auto *base=water::find(d.waters,w->id);if(!base||water::find(waters,w->id)||w->config.x!=base->config.x||w->config.y!=base->config.y||w->config.level!=base->config.level)throw std::runtime_error("水体参数快照引用无效");waters.push_back(std::move(w));}
  auto next=s;next.city_views=city::views_from_json(j.value("cities",J::object()));for(const auto &[id,v]:next.city_views)if(std::none_of(d.cities.begin(),d.cities.end(),[&](const auto &c){return c->id==id;}))throw std::runtime_error("城市视图引用无效");
  next.view.reset();if(j.contains("view")){next.view=j.at("view").get<std::array<float,6>>();for(float v:*next.view)if(!std::isfinite(v))throw std::runtime_error("观察相机数值无效");if((*next.view)[3]<=0)throw std::runtime_error("观察相机距离无效");}
  next.group_transforms.clear();const auto groups=j.value("groups",J::object());for(const auto &[id,value]:groups.items())read_transform(next.group_transforms[id],value);validate_group_transforms(d,next.group_transforms);
  for(const auto &[id,o]:j.at("objects").items()){const auto t=target_index(d,id);auto &v=next.values.at(t);const auto &target=d.catalog.targets[t];read_transform(v.transform,o.at("transform"));runtime::validate_transform(v.transform);v.visible=o.at("visible");v.extension=read_extension(o.at("extension"));v.ground_alignment_ratio=o.at("ground_ratio");if(!std::isfinite(v.ground_alignment_ratio))throw std::runtime_error("地面对齐比例无效");
    v.ground_alignment_offset_cm=o.value("ground_offset_cm",0.);v.ground_alignment_body_only=o.value("ground_body_only",false);if(!std::isfinite(v.ground_alignment_offset_cm))throw std::runtime_error("地面对齐偏移无效");
    v.graft_enabled=o.value("graft_enabled",true);
    v.physics=runtime::physics_object_from_json(o.value("physics",J{}));
    v.favorites=read_favorites(o.value("favorites",J{}));v.unlimited_morphs=o.at("unlimited").get<std::set<std::string>>();
    for(const auto &[mid,value]:o.at("morphs").items()){auto m=std::find_if(target.morphs.begin(),target.morphs.end(),[&](const auto &m){return m.id==mid;});if(m==target.morphs.end())throw std::runtime_error("DUFEX 参数不存在："+mid);if(runtime::legacy_extension_channel(m->label))continue;const float x=value.get<float>();if(!std::isfinite(x))throw std::runtime_error("DUFEX Morph 数值无效");v.morphs[size_t(m-target.morphs.begin())]=x;}runtime::sync_aliases(target,v);
  }
  for(const auto &[id,poses]:j.at("poses").items()){const auto it=std::find_if(d.skeletons.skins.begin(),d.skeletons.skins.end(),[&](const auto &v){return v.id==id;});if(it==d.skeletons.skins.end())throw std::runtime_error("DUFEX 骨架不存在："+id);auto &out=next.poses.at(size_t(it-d.skeletons.skins.begin()));
    for(const auto &[bone,p]:poses.items()){auto b=std::find_if(it->joints.begin(),it->joints.end(),[&](const auto &v){return v.id==bone;});if(b==it->joints.end())throw std::runtime_error("DUFEX 骨骼不存在："+bone);auto &v=out.at(size_t(b-it->joints.begin()));read_transform(v,p);v.center_offset_cm=vector(p.at("center_offset_cm"));v.end_offset_cm=vector(p.at("end_offset_cm"));v.orientation_offset_degrees=vector(p.at("orientation_offset_degrees"));}runtime::validate_pose(*it,out);
  }
  next.options=ir::options_from_json(j.at("options"));next.subdivision_levels=j.at("subdivision").get<std::map<std::string,int>>();for(const auto &[id,l]:next.subdivision_levels)if(l<0||l>6)throw std::runtime_error("DUFEX 细分等级无效");
  next.lights.clear();std::set<std::string> ids;for(const auto &l:j.at("lights")){ir::AreaLight v;v.id=l.at("id");if(!ids.insert(v.id).second)throw std::runtime_error("DUFEX 灯光身份重复");v.transform.value=l.at("transform").get<std::array<float,12>>();for(auto x:v.transform.value)if(!std::isfinite(x))throw std::runtime_error("DUFEX 灯光变换无效");v.power=vector(l.at("power"));v.width=l.at("width");v.height=l.at("height");v.angle=l.at("angle");const int kind=l.at("kind");if(kind<0||kind>3||!std::isfinite(v.width)||v.width<=0||!std::isfinite(v.height)||v.height<=0||!std::isfinite(v.angle))throw std::runtime_error("DUFEX 灯光参数无效");v.kind=ir::LightKind(kind);next.lights.push_back(v);}
  next.pose_pins.clear();for(const auto &p:j.value("pins",J::array())){auto skin=std::find_if(d.skeletons.skins.begin(),d.skeletons.skins.end(),[&](const auto &v){return v.id==p.at("skin").get<std::string>();});if(skin==d.skeletons.skins.end())throw std::runtime_error("固定关节的角色不存在");auto joint=std::find_if(skin->joints.begin(),skin->joints.end(),[&](const auto &v){return v.id==p.at("joint").get<std::string>();});if(joint==skin->joints.end())throw std::runtime_error("固定关节不存在");runtime::PosePin pin;pin.skin=int(skin-d.skeletons.skins.begin());pin.joint=int(joint-skin->joints.begin());pin.world=vector(p.at("world"));pin.position=p.at("position");pin.angle=p.at("angle");pin.world_orientation.value=p.at("orientation").get<std::array<float,12>>();for(auto v:pin.world_orientation.value)if(!std::isfinite(v))throw std::runtime_error("固定角度无效");next.pose_pins.push_back(pin);}
  next.material_overrides=j.value("materials",J::object()).get<MaterialOverrides>();validate_material_overrides(d.loaded.scene,next.material_overrides);
  const auto instance_ground=j.value("instance_ground",J::object());next.instance_ground.clear();for(const auto &[id,v]:instance_ground.items())next.instance_ground[id]={v.at("offset_m").get<double>(),v.at("ratio").get<double>(),v.value("offset_cm",0.),v.value("body_only",false)};validate_instance_ground(d.loaded.scene,next.instance_ground);
  next.cloud_overrides=std::move(clouds);next.water_overrides=std::move(waters);next.control_favorites=read_favorites(j.value("control_favorites",J{}));s=std::move(next);
}
J scene_extension_json(const Document &d,const Snapshot &s){
  J j={{"schema","daz-fast-viewer-scene-extension"},{"version",1},{"source",path_string(d.source_file)},{"operations",d.operations},{"state",snapshot_json(d,s)},{"archives",J::object()}};
  for(const auto &[id,a]:d.archives)j["archives"][id]=a->json();
  if(d.loaded.archive){const auto id=d.loaded.archive->identity();j["archives"][id]=d.loaded.archive->json();j["source_archive"]=id;}
  if(d.loaded.report.is_object())j["content_roots"]=d.loaded.report.value("content_roots",J::array());return j;
}
bool legacy_scene_extension(const Document &d){return !d.cities.empty()||city_operations(d.operations);}
void save_scene_extension(const fs::path &file,const Document &d,const Snapshot &s){
  const auto destination=fs::absolute(file).lexically_normal();auto ext=destination.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);if(ext!=L".dufex")throw std::runtime_error("扩展文件必须使用 .dufex 后缀");
  if(!d.source_file.empty()&&daz::source_key(destination)==daz::source_key(d.source_file))throw std::runtime_error("不能覆盖源场景");
  J j=scene_extension_json(d,s);const bool legacy=legacy_scene_extension(d);
  if(!legacy){if(!d.source_file.empty()&&!d.loaded.archive)throw std::runtime_error("缺少已加载场景的完整底稿，请重新打开后保存");j["version"]=2;j["structure"]=structure(d);j["state"].erase("cities");for(auto &o:j["state"]["objects"])o.erase("physics");}
  const auto body=daz::gzip_document(j.dump());auto temporary=destination;temporary+=L".tmp-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
  try{std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);stream.write(body.data(),std::streamsize(body.size()));stream.flush();if(!stream)throw std::runtime_error("扩展文件写入失败");stream.close();if(!stream)throw std::runtime_error("扩展文件关闭失败");
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("扩展文件原子替换失败");
#else
    fs::rename(temporary,destination);
#endif
  }catch(...){std::error_code ec;fs::remove(temporary,ec);throw;}
}
RestoredScene load_scene_extension(const fs::path &file,const std::vector<fs::path> &roots,uint64_t generation,const std::function<void(const std::string &)> &progress){
  const auto j=J::parse(daz::document_bytes(file));
  return restore_scene_extension(j,fs::absolute(file).parent_path(),roots,generation,progress);
}
RestoredScene restore_scene_extension(const J &j,const fs::path &folder,const std::vector<fs::path> &roots,uint64_t generation,const std::function<void(const std::string &)> &progress){
  const auto version=j.at("version").get<int>();if(j.at("schema")!="daz-fast-viewer-scene-extension"||(version!=1&&version!=2))throw std::runtime_error("不支持的 DUFEX 格式或版本");
  if(version==2&&city_operations(j.at("operations")))throw std::runtime_error("DUFEX v2 尚未包含城市 PCG 序列化");
  std::map<std::string,std::shared_ptr<daz::SourceArchive>> archives;const auto archive_data=j.value("archives",J::object());for(const auto &[id,value]:archive_data.items())archives[id]=daz::SourceArchive::read(value);
  const auto key=j.value("source_archive",std::string{});if(!key.empty()&&!archives.contains(key))throw std::runtime_error("场景根底稿不存在");
  daz::DocumentScope source_scope(key.empty()?nullptr:archives.at(key));
  auto source=fs::u8path(j.at("source").get<std::string>());if(!source.empty()&&source.is_relative())source=folder/source;
  if(version==2&&!source.empty()&&(key.empty()||!archives.at(key)->find(source)))throw std::runtime_error("DUFEX v2 缺少完整场景底稿");
  auto resolved=roots;for(const auto &r:j.value("content_roots",J::array())){auto p=fs::u8path(r.get<std::string>());if(fs::is_directory(p)&&std::find(resolved.begin(),resolved.end(),p)==resolved.end())resolved.push_back(p);}
  auto d=load_source(source,resolved,progress);d->archives=std::move(archives);d->generation=generation;replay(*d,j.at("operations"),resolved,folder,progress);
  if(version==2&&structure(*d)!=j.at("structure"))throw std::runtime_error("场景结构或依赖资产已变化，无法完整恢复保存的关系");
  auto s=initial_snapshot(*d);apply_snapshot_json(*d,s,j.at("state"));for(const auto &w:s.water_overrides)water::install(*d,w);s.water_overrides.clear();for(const auto &v:s.cloud_overrides)cloud::install(*d,v);s.cloud_overrides.clear();d->loaded.scene.lights=s.lights;release_load_data(*d);return {std::move(d),std::move(s)};
}
}
