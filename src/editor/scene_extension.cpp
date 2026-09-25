#include "editor/scene_extension.h"
#include "render_ir/options_json.h"
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
    if(kind=="append"){auto s=load_source(source_path(op.at("file")),roots,progress,true);replay(*s,op.at("operations"),roots,folder,progress,depth+1);append_document(d,std::move(*s),op.at("prefix"));}
    else if(kind=="studio")ir::add_studio(d.loaded.scene);
    else if(kind=="remove"){auto v=initial_snapshot(d);remove_target(d,v,target_index(d,op.at("target")));}
    else if(kind=="remove_light"){auto v=initial_snapshot(d);const auto id=op.at("id").get<std::string>();const auto found=std::find_if(v.lights.begin(),v.lights.end(),[&](const auto &l){return l.id==id;});if(found!=v.lights.end())remove_light(d,v,size_t(found-v.lights.begin()));}
    else if(kind=="attach")attach_import(d,target_index(d,op.at("first")),target_index(d,op.at("host")));
    else if(kind=="fit")fit_attachment(d,target_index(d,op.at("target")),op.at("host").get<std::string>().empty()?-1:int(target_index(d,op.at("host"))));
    else if(kind=="materials")apply_materials(d,target_index(d,op.at("target")),daz::load(source_path(op.at("file")),{roots,false}));
    else throw std::runtime_error("DUFEX 含未知场景操作："+kind);
  }
  d.operations=operations;
}
}
std::filesystem::path extension_path(std::filesystem::path p){p.replace_extension(".dufex");return p;}
J snapshot_json(const Document &d,const Snapshot &s){
  if(s.generation!=d.generation||s.values.size()!=d.catalog.targets.size()||s.poses.size()!=d.skeletons.skins.size())throw std::runtime_error("保存快照与场景不匹配");
  J j={{"objects",J::object()},{"poses",J::object()},{"subdivision",s.subdivision_levels},{"options",ir::options_json(s.options)},{"lights",J::array()}};
  for(size_t t=0;t<s.values.size();++t){const auto &v=s.values[t];const auto &target=d.catalog.targets[t];runtime::validate_transform(v.transform);J m=J::object();
    for(size_t i=0;i<target.morphs.size();++i){const auto &p=target.morphs[i];if(p.alias_morph>=0||runtime::legacy_extension_channel(p.label)||(!p.evaluable&&!p.unsupported.empty()))continue;const auto value=v.morphs.at(i);if(!std::isfinite(value))throw std::runtime_error("Morph 值无效");if(value!=p.initial)m[p.id]=value;}
    j["objects"][target.id]={{"transform",transform(v.transform)},{"visible",v.visible},{"morphs",m},{"unlimited",v.unlimited_morphs},{"ground_ratio",v.ground_alignment_ratio},{"extension",extension(v.extension)},{"favorites",favorites(v.favorites)}};
  }
  for(size_t i=0;i<s.poses.size();++i){const auto &skin=d.skeletons.skins[i];runtime::validate_pose(skin,s.poses[i]);J poses=J::object();for(size_t b=0;b<skin.joints.size();++b){const auto &p=s.poses[i][b];auto v=transform(p);v["center_offset_cm"]=vec(p.center_offset_cm);v["end_offset_cm"]=vec(p.end_offset_cm);v["orientation_offset_degrees"]=vec(p.orientation_offset_degrees);poses[skin.joints[b].id]=v;}j["poses"][skin.id]=poses;}
  for(const auto &l:s.lights)j["lights"].push_back({{"id",l.id},{"transform",l.transform.value},{"power",vec(l.power)},{"width",l.width},{"height",l.height},{"kind",int(l.kind)},{"angle",l.angle}});
  j["pins"]=J::array();for(const auto &p:s.pose_pins){const auto &skin=d.skeletons.skins.at(size_t(p.skin));j["pins"].push_back({{"skin",skin.id},{"joint",skin.joints.at(size_t(p.joint)).id},{"world",vec(p.world)},{"position",p.position},{"angle",p.angle},{"orientation",p.world_orientation.value}});}
  j["control_favorites"]=favorites(s.control_favorites);return j;
}
void apply_snapshot_json(const Document &d,Snapshot &s,const J &j){
  auto next=s;
  for(const auto &[id,o]:j.at("objects").items()){const auto t=target_index(d,id);auto &v=next.values.at(t);const auto &target=d.catalog.targets[t];read_transform(v.transform,o.at("transform"));runtime::validate_transform(v.transform);v.visible=o.at("visible");v.extension=read_extension(o.at("extension"));v.ground_alignment_ratio=o.at("ground_ratio");if(!std::isfinite(v.ground_alignment_ratio))throw std::runtime_error("地面对齐比例无效");
    v.favorites=read_favorites(o.value("favorites",J{}));v.unlimited_morphs=o.at("unlimited").get<std::set<std::string>>();
    for(const auto &[mid,value]:o.at("morphs").items()){auto m=std::find_if(target.morphs.begin(),target.morphs.end(),[&](const auto &m){return m.id==mid;});if(m==target.morphs.end())throw std::runtime_error("DUFEX 参数不存在："+mid);if(runtime::legacy_extension_channel(m->label))continue;const float x=value.get<float>();if(!std::isfinite(x))throw std::runtime_error("DUFEX Morph 数值无效");v.morphs[size_t(m-target.morphs.begin())]=x;}runtime::sync_aliases(target,v);
  }
  for(const auto &[id,poses]:j.at("poses").items()){const auto it=std::find_if(d.skeletons.skins.begin(),d.skeletons.skins.end(),[&](const auto &v){return v.id==id;});if(it==d.skeletons.skins.end())throw std::runtime_error("DUFEX 骨架不存在："+id);auto &out=next.poses.at(size_t(it-d.skeletons.skins.begin()));
    for(const auto &[bone,p]:poses.items()){auto b=std::find_if(it->joints.begin(),it->joints.end(),[&](const auto &v){return v.id==bone;});if(b==it->joints.end())throw std::runtime_error("DUFEX 骨骼不存在："+bone);auto &v=out.at(size_t(b-it->joints.begin()));read_transform(v,p);v.center_offset_cm=vector(p.at("center_offset_cm"));v.end_offset_cm=vector(p.at("end_offset_cm"));v.orientation_offset_degrees=vector(p.at("orientation_offset_degrees"));}runtime::validate_pose(*it,out);
  }
  next.options=ir::options_from_json(j.at("options"));next.subdivision_levels=j.at("subdivision").get<std::map<std::string,int>>();for(const auto &[id,l]:next.subdivision_levels)if(l<0||l>6)throw std::runtime_error("DUFEX 细分等级无效");
  next.lights.clear();std::set<std::string> ids;for(const auto &l:j.at("lights")){ir::AreaLight v;v.id=l.at("id");if(!ids.insert(v.id).second)throw std::runtime_error("DUFEX 灯光身份重复");v.transform.value=l.at("transform").get<std::array<float,12>>();for(auto x:v.transform.value)if(!std::isfinite(x))throw std::runtime_error("DUFEX 灯光变换无效");v.power=vector(l.at("power"));v.width=l.at("width");v.height=l.at("height");v.angle=l.at("angle");const int kind=l.at("kind");if(kind<0||kind>3||!std::isfinite(v.width)||v.width<=0||!std::isfinite(v.height)||v.height<=0||!std::isfinite(v.angle))throw std::runtime_error("DUFEX 灯光参数无效");v.kind=ir::LightKind(kind);next.lights.push_back(v);}
  next.pose_pins.clear();for(const auto &p:j.value("pins",J::array())){auto skin=std::find_if(d.skeletons.skins.begin(),d.skeletons.skins.end(),[&](const auto &v){return v.id==p.at("skin").get<std::string>();});if(skin==d.skeletons.skins.end())throw std::runtime_error("固定关节的角色不存在");auto joint=std::find_if(skin->joints.begin(),skin->joints.end(),[&](const auto &v){return v.id==p.at("joint").get<std::string>();});if(joint==skin->joints.end())throw std::runtime_error("固定关节不存在");runtime::PosePin pin;pin.skin=int(skin-d.skeletons.skins.begin());pin.joint=int(joint-skin->joints.begin());pin.world=vector(p.at("world"));pin.position=p.at("position");pin.angle=p.at("angle");pin.world_orientation.value=p.at("orientation").get<std::array<float,12>>();for(auto v:pin.world_orientation.value)if(!std::isfinite(v))throw std::runtime_error("固定角度无效");next.pose_pins.push_back(pin);}
  next.control_favorites=read_favorites(j.value("control_favorites",J{}));s=std::move(next);
}
void save_scene_extension(const fs::path &file,const Document &d,const Snapshot &s){
  const auto destination=fs::absolute(file).lexically_normal();if(destination.extension()!=L".dufex")throw std::runtime_error("扩展文件必须使用 .dufex 后缀");
  if(!d.source_file.empty()&&fs::equivalent(destination.parent_path(),d.source_file.parent_path())&&destination.filename()==d.source_file.filename())throw std::runtime_error("不能覆盖源场景");
  auto source=d.source_file;if(!source.empty()){std::error_code ec;auto relative=fs::relative(source,destination.parent_path(),ec);if(!ec&&!relative.empty())source=relative;}
  J j={{"schema","daz-fast-viewer-scene-extension"},{"version",1},{"source",path_string(source)},{"operations",d.operations},{"state",snapshot_json(d,s)}};
  const auto body=j.dump(2);auto temporary=destination;temporary+=L".tmp-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
  try{std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);stream.write(body.data(),std::streamsize(body.size()));stream.flush();if(!stream)throw std::runtime_error("扩展文件写入失败");stream.close();if(!stream)throw std::runtime_error("扩展文件关闭失败");
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("扩展文件原子替换失败");
#else
    fs::rename(temporary,destination);
#endif
  }catch(...){std::error_code ec;fs::remove(temporary,ec);throw;}
}
RestoredScene load_scene_extension(const fs::path &file,const std::vector<fs::path> &roots,uint64_t generation,const std::function<void(const std::string &)> &progress){
  J j;std::ifstream stream(file,std::ios::binary);if(!stream)throw std::runtime_error("无法读取 DUFEX");stream>>j;
  if(j.at("schema")!="daz-fast-viewer-scene-extension"||j.at("version")!=1)throw std::runtime_error("不支持的 DUFEX 格式或版本");
  auto source=fs::u8path(j.at("source").get<std::string>());if(!source.empty()&&source.is_relative())source=fs::absolute(file).parent_path()/source;
  auto d=load_source(source,roots,progress);d->generation=generation;replay(*d,j.at("operations"),roots,fs::absolute(file).parent_path(),progress);
  auto s=initial_snapshot(*d);apply_snapshot_json(*d,s,j.at("state"));d->loaded.scene.lights=s.lights;release_load_data(*d);return {std::move(d),std::move(s)};
}
}
