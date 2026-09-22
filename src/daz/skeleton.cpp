#include "daz/skeleton.h"
#include "daz/documents.h"
#include "diagnostics/load_profile.h"
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
void joint_channels(runtime::Joint &joint,const Json &node) {
  // 场景可以重新定义附件的铰链与局部轴；preview 仅为缓存，不能当输入。
  joint.center_cm=vector(node,"center_point",joint.center_cm);
  joint.end_cm=vector(node,"end_point",joint.end_cm);
  joint.orientation_degrees=vector(node,"orientation",joint.orientation_degrees);
  joint.rotation_order=node.value("rotation_order",joint.rotation_order);
  joint.inherits_scale=node.value("inherits_scale",joint.inherits_scale);
}
}
SkinCatalog load_skeletons(const LoadedScene &loaded) {
  SkinCatalog catalog;catalog.report={{"skins",Json::array()}};
  Json saved=Json::object();if(loaded.report.contains("input")) saved=document_view(std::filesystem::u8path(loaded.report.at("input").get<std::string>()))->value("scene",Json::object());
  std::map<std::string,Json> instances;std::set<std::string> figures;
  for(const auto &n:saved.value("nodes",Json::array())) {const auto id=n.value("id","");instances[id]=n;
    if(n.value("type","")=="figure"||n.value("preview",Json::object()).value("type","")=="figure"||n.contains("geometries")) figures.insert(id);}
  for(const auto &o:loaded.objects) if(o.figure) figures.insert(o.id);
  for(const auto &object:loaded.objects) {
    if(!object.figure) continue;
    diagnostics::Scope object_scope(diagnostics::active?object.id:std::string{});
    auto binding_file=object.geometry_file;auto handle=document_view(binding_file,DocumentView::skeleton);const Json *document=handle.get();const Json *binding=nullptr,*modifier=nullptr;
    auto find_binding=[&](const std::string &geometry) {
      if(document->contains("modifier_library")) for(const auto &m:document->at("modifier_library")) if(m.contains("skin")&&fragment(m.at("skin").value("geometry",""))==geometry) {if(binding) throw std::runtime_error("同一几何包含多个 SkinBinding");binding=&m.at("skin");modifier=&m;}
    };
    find_binding(object.geometry_id);
    for(const auto &source:object.geometry_sources) {
      if(binding) break;binding_file=source.file;handle=document_view(binding_file,DocumentView::skeleton);document=handle.get();find_binding(source.id);
    }
    if(!binding) {catalog.report["skins"].push_back({{"object",object.id},{"status","NO_SKIN_BINDING"}});continue;}
    runtime::Skin skin;skin.id=object.id;skin.instance=object.instance;
    std::string mode="DualQuat",binding_mode="General";
    for(const auto &extra:modifier->value("extra",Json::array())) if(extra.value("type","")=="skin_settings") {
      binding_mode=extra.value("binding_mode","General");
      if(binding_mode!="General"&&binding_mode!="Local") throw std::runtime_error("尚未支持的蒙皮绑定："+binding_mode+" / "+object.id);
      mode=extra.value("general_map_mode","DualQuat");
    }
    if(binding_mode=="Local") mode="Linear";
    if(mode=="DualQuat") skin.method=runtime::SkinMethod::dual_quaternion;
    else if(mode=="Linear") skin.method=runtime::SkinMethod::linear;
    else throw std::runtime_error("尚未支持的蒙皮方式："+mode);
    std::map<std::string,Json> nodes;for(const auto &n:document->at("node_library")) {const auto id=n.at("id").get<std::string>();if(!nodes.emplace(id,n).second) throw std::runtime_error("骨架节点 ID 重复");}
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
    skin.root_general_scale=number(nodes.at(root).value("general_scale",Json::object()),1);
    if(instances.contains(object.id)&&instances.at(object.id).contains("general_scale")) skin.root_general_scale=number(instances.at(object.id).at("general_scale"),skin.root_general_scale);
    if(!std::isfinite(skin.root_general_scale)||skin.root_general_scale<=0) throw std::runtime_error("Figure 保存缩放必须大于零");
    const auto vertex_count=loaded.scene.meshes.at(loaded.scene.instances.at(object.instance).mesh).positions.size();
    if(binding->at("vertex_count").get<size_t>()!=vertex_count) throw std::runtime_error("SkinBinding 顶点数量不匹配");skin.weights.resize(vertex_count);
    size_t weight_count=0;std::set<size_t> weighted_joints;
    auto weight_map=[&](const Json &weights) {
      const auto &rows=weights.at("values");if(weights.at("count").get<size_t>()!=rows.size()) throw std::runtime_error("权重记录数量不匹配");
      std::map<uint32_t,double> result;
      for(const auto &row:rows) {
        if(row.size()!=2) throw std::runtime_error("无效权重记录");const auto vertex=row.at(0).get<uint32_t>();const double value=row.at(1).get<double>();
        if(vertex>=vertex_count||result.contains(vertex)||!std::isfinite(value)||value<0) throw std::runtime_error("权重索引或数值无效");
        result.emplace(vertex,value);
      }
      std::erase_if(result,[](const auto &entry) {return entry.second==0;});return result;
    };
    for(const auto &w:binding->at("joints")) {
      const auto index=add(fragment(w.at("node").get<std::string>()));if(!weighted_joints.insert(index).second) throw std::runtime_error("重复的关节权重记录");
      std::map<uint32_t,double> weights;
      if(binding_mode=="Local") {
        const auto &local=w.at("local_weights");weights=weight_map(local.at("x"));
        if(weight_map(local.at("y"))!=weights||weight_map(local.at("z"))!=weights||!w.contains("scale_weights"))
          throw std::runtime_error("局部轴权重不同，尚需 TriAx 蒙皮："+object.id);
      } else if(w.contains("node_weights")) weights=weight_map(w.at("node_weights"));
      if(binding_mode=="General"&&w.contains("local_weights")) for(const auto &axis:w.at("local_weights"))
        if(weight_map(axis)!=weights) throw std::runtime_error("独立局部权重与绑定权重不同："+object.id);
      if(w.contains("scale_weights")&&weight_map(w.at("scale_weights"))!=weights) throw std::runtime_error("独立缩放权重与绑定权重不同："+object.id);
      for(const auto &[vertex,value]:weights) {skin.weights[vertex].push_back({uint32_t(index),value});++weight_count;}
    }
    // 各轴与缩放图一致且每个顶点只属于一个关节时，Local 与刚性 Linear 变换等价。
    if(binding_mode=="Local") for(const auto &weights:skin.weights) if(!weights.empty()&&(weights.size()!=1||weights.front().weight!=1))
      throw std::runtime_error("非刚性局部权重尚需 TriAx 蒙皮："+object.id);
    // 保存场景中的骨骼实例覆盖值只可作用于所属 Figure，不能按名称跨角色覆盖。
    for(const auto &[id,n]:instances) {
      if(id==object.id) {joint_channels(skin.joints[indices.at(root)],n);continue;}auto parent=fragment(n.value("parent",""));std::set<std::string> seen;
      while(!parent.empty()&&parent!=object.id&&!figures.contains(parent)&&instances.contains(parent)&&seen.insert(parent).second) parent=fragment(instances.at(parent).value("parent",""));
      if(parent!=object.id) continue;const auto bone=fragment(n.value("url",""));if(indices.contains(bone)) {
        const auto index=indices.at(bone);joint_channels(skin.joints[index],n);pose_channels(skin.initial[index],n);skin.joints[index].scene_id=id;
      }
    }
    runtime::validate_pose(skin,skin.initial);size_t unweighted=0;double error=0;
    for(const auto &weights:skin.weights) {double sum=0;for(const auto &w:weights) sum+=w.weight;if(weights.empty()) ++unweighted;else error=std::max(error,std::abs(sum-1));}
    catalog.report["skins"].push_back({{"object",object.id},{"source",path_string(binding_file)},{"inherited_geometry",binding_file!=object.geometry_file},{"method",mode},{"binding_mode",binding_mode},{"weight_conversion",binding_mode=="Local"?"rigid-identical-axis-maps":"general-maps"},{"joints",skin.joints.size()},
      {"vertices",vertex_count},{"weights",weight_count},{"unweighted_vertices",unweighted},{"max_source_weight_sum_error",error},{"normalization","per_vertex"},{"status","READY"}});
    Json formulas=Json::array();
    for(const auto &joint:skin.joints) for(const auto &formula:nodes.at(joint.id).value("formulas",Json::array())) formulas.push_back(formula);
    catalog.report["skins"].back()["node_formulas"]=formulas.size();
    catalog.node_formulas.push_back(std::move(formulas));catalog.skins.push_back(std::move(skin));
  }
  return catalog;
}
}
