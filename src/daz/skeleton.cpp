#include "daz/skeleton.h"
#include <functional>
#include <cmath>
#include <map>
#include <set>

namespace dfv::daz {
namespace {
using Json=nlohmann::json;
std::string path_string(const std::filesystem::path &p) {const auto u=p.generic_u8string();return {u.begin(),u.end()};}
std::string fragment(const std::string &uri) {const auto text=decode_uri(uri);const auto hash=text.find('#');return hash==std::string::npos?text:text.substr(hash+1);}
float number(const Json &channel,float fallback) {return channel.value("current_value",channel.value("value",fallback));}
ir::Vec3 vector(const Json &node,const char *key,ir::Vec3 fallback={}) {
  if(!node.contains(key)) return fallback;
  for(const auto &c:node.at(key)) {const auto id=c.value("id","");if(id=="x") fallback.x=number(c,fallback.x);else if(id=="y") fallback.y=number(c,fallback.y);else if(id=="z") fallback.z=number(c,fallback.z);}
  return fallback;
}
void pose_channels(runtime::JointPose &p,const Json &node) {
  p.translation_cm=vector(node,"translation",p.translation_cm);p.rotation_degrees=vector(node,"rotation",p.rotation_degrees);p.scale=vector(node,"scale",p.scale);
  if(node.contains("general_scale")) p.general_scale=number(node.at("general_scale"),p.general_scale);
}
}
SkinCatalog load_skeletons(const LoadedScene &loaded) {
  SkinCatalog catalog;catalog.report={{"skins",Json::array()}};
  Json saved=Json::object();if(loaded.report.contains("input")) saved=read_document_file(std::filesystem::u8path(loaded.report.at("input").get<std::string>())).value("scene",Json::object());
  for(const auto &object:loaded.objects) {
    if(!object.figure) continue;
    const auto document=read_document_file(object.geometry_file);const Json *binding=nullptr,*modifier=nullptr;
    if(document.contains("modifier_library")) for(const auto &m:document.at("modifier_library")) if(m.contains("skin")&&fragment(m.at("skin").value("geometry",""))==object.geometry_id) {if(binding) throw std::runtime_error("同一几何包含多个 SkinBinding");binding=&m.at("skin");modifier=&m;}
    if(!binding) {catalog.report["skins"].push_back({{"object",object.id},{"status","NO_SKIN_BINDING"}});continue;}
    runtime::Skin skin;skin.id=object.id;skin.instance=object.instance;
    std::string mode="DualQuat";
    for(const auto &extra:modifier->value("extra",Json::array())) if(extra.value("type","")=="skin_settings") {
      if(extra.value("binding_mode","General")!="General") throw std::runtime_error("当前只支持 General 权重蒙皮："+object.id);
      mode=extra.value("general_map_mode","DualQuat");
    }
    if(mode=="DualQuat") skin.method=runtime::SkinMethod::dual_quaternion;
    else if(mode=="Linear") skin.method=runtime::SkinMethod::linear;
    else throw std::runtime_error("尚未支持的蒙皮方式："+mode);
    std::map<std::string,Json> nodes;for(const auto &n:document.at("node_library")) {const auto id=n.at("id").get<std::string>();if(!nodes.emplace(id,n).second) throw std::runtime_error("骨架节点 ID 重复");}
    const auto root=fragment(binding->at("node").get<std::string>());if(!nodes.contains(root)) throw std::runtime_error("缺少骨架根节点");
    std::map<std::string,size_t> indices;std::set<std::string> visiting;
    std::function<size_t(const std::string &)> add=[&](const std::string &id)->size_t {
      if(indices.contains(id)) return indices.at(id);
      if(!nodes.contains(id)||!visiting.insert(id).second) throw std::runtime_error("骨架父节点缺失或形成循环："+id);
      const auto &n=nodes.at(id);int parent=-1;
      if(id!=root) {const auto pid=fragment(n.value("parent",""));if(pid.empty()) throw std::runtime_error("骨骼不属于目标 Figure："+id);parent=int(add(pid));}
      runtime::Joint j;j.id=id;j.name=n.value("name",id);j.label=n.value("label",j.name);j.parent=parent;j.rotation_order=n.value("rotation_order","XYZ");
      j.center_cm=vector(n,"center_point");j.end_cm=vector(n,"end_point");j.orientation_degrees=vector(n,"orientation");
      j.inherits_scale=n.value("inherits_scale",parent<0||skin.joints[parent].parent<0);
      j.aliases=n.value("name_aliases",std::vector<std::string>{});runtime::JointPose pose;
      if(id!=root) pose_channels(pose,n); // Figure 的初始世界变换已由静态 Loader 写入实例。
      const size_t index=skin.joints.size();skin.joints.push_back(j);skin.initial.push_back(pose);indices[id]=index;visiting.erase(id);return index;
    };
    add(root);
    const auto vertex_count=loaded.scene.meshes.at(loaded.scene.instances.at(object.instance).mesh).positions.size();
    if(binding->at("vertex_count").get<size_t>()!=vertex_count) throw std::runtime_error("SkinBinding 顶点数量不匹配");skin.weights.resize(vertex_count);
    size_t weight_count=0;std::set<size_t> weighted_joints;
    for(const auto &w:binding->at("joints")) {
      const auto index=add(fragment(w.at("node").get<std::string>()));if(!weighted_joints.insert(index).second) throw std::runtime_error("重复的关节权重记录");
      if(w.contains("local_weights")||w.contains("scale_weights")) throw std::runtime_error("当前尚不支持独立局部或缩放权重图");
      if(!w.contains("node_weights")) continue;
      const auto &weights=w.at("node_weights");const auto &rows=weights.at("values");if(weights.at("count").get<size_t>()!=rows.size()) throw std::runtime_error("权重记录数量不匹配");
      std::set<uint32_t> seen;
      for(const auto &row:rows) {if(row.size()!=2) throw std::runtime_error("无效权重记录");const auto vertex=row.at(0).get<uint32_t>();const double value=row.at(1).get<double>();
        if(vertex>=vertex_count||!seen.insert(vertex).second||!std::isfinite(value)||value<0) throw std::runtime_error("权重索引或数值无效");
        if(value>0) {skin.weights[vertex].push_back({uint32_t(index),value});++weight_count;}}
    }
    // 保存场景中的骨骼实例覆盖值只可作用于所属 Figure，不能按名称跨角色覆盖。
    std::map<std::string,Json> instances;for(const auto &n:saved.value("nodes",Json::array())) instances[n.value("id","")]=n;
    for(const auto &[id,n]:instances) {
      if(id==object.id) continue;auto parent=fragment(n.value("parent",""));std::set<std::string> seen;
      while(!parent.empty()&&parent!=object.id&&instances.contains(parent)&&seen.insert(parent).second) parent=fragment(instances.at(parent).value("parent",""));
      if(parent!=object.id) continue;const auto bone=fragment(n.value("url",""));if(indices.contains(bone)) pose_channels(skin.initial[indices.at(bone)],n);
    }
    runtime::validate_pose(skin,skin.initial);size_t unweighted=0;double error=0;
    for(const auto &weights:skin.weights) {double sum=0;for(const auto &w:weights) sum+=w.weight;if(weights.empty()) ++unweighted;else error=std::max(error,std::abs(sum-1));}
    catalog.report["skins"].push_back({{"object",object.id},{"source",path_string(object.geometry_file)},{"method",mode},{"joints",skin.joints.size()},
      {"vertices",vertex_count},{"weights",weight_count},{"unweighted_vertices",unweighted},{"max_source_weight_sum_error",error},{"normalization","per_vertex"},{"status","READY"}});
    catalog.skins.push_back(std::move(skin));
  }
  return catalog;
}
}
