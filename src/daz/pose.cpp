#include "daz/pose.h"
#include "daz/documents.h"
#include "daz/material_animation.h"
#include "runtime/deformation.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace dfv::daz {
namespace {
PoseChannel selection_address(std::string url) {
  PoseChannel c;const auto protocol=url.find("://");
  if(protocol==std::string::npos)throw std::runtime_error("无效的选中角色地址："+url);
  c.scheme=url.substr(0,protocol);auto address=url.substr(protocol+3);if(address.ends_with(':'))address.pop_back();
  if((c.scheme!="name"&&c.scheme!="id")||(address!="@selection"&&!address.starts_with("@selection/")&&!address.starts_with("@selection#")))throw std::runtime_error("当前只支持作用于选中角色的姿势通道："+url);
  address.erase(0,10);const auto hash=address.find('#');if(hash!=std::string::npos){c.modifier=address.substr(hash+1);address.resize(hash);}
  if(address.starts_with('/'))address.erase(0,1);c.node=address;return c;
}
bool matches_node(const runtime::Joint &joint,const std::string &scheme,const std::string &node) {
  return node.empty()?joint.parent<0:scheme=="id"?joint.id==node:joint.name==node||std::find(joint.aliases.begin(),joint.aliases.end(),node)!=joint.aliases.end();
}
}
PosePreset parse_pose(const nlohmann::json &document,const std::string &source) {
  PosePreset preset;preset.source=source;
  const auto &animations=document.at("scene").at("animations");if(!animations.is_array()||animations.empty()) throw std::runtime_error("姿势预设没有通道");
  const auto &nodes=array_member(document.at("scene"),"nodes");std::map<std::string,int> indices;std::set<std::string> visiting;
  std::function<int(const std::string &)> add_node=[&](const std::string &id) {
    if(indices.contains(id))return indices.at(id);
    if(!visiting.insert(id).second)throw std::runtime_error("分层姿势的节点父链存在循环："+id);
    const nlohmann::json *found=nullptr;for(const auto &node:nodes)if(decode_uri(node.value("id",""))==id){if(found)throw std::runtime_error("分层姿势的节点 ID 重复："+id);found=&node;}
    if(!found)throw std::runtime_error("分层姿势缺少引用节点："+id);
    const auto address=selection_address(decode_uri(found->at("url")));if(!address.modifier.empty())throw std::runtime_error("分层姿势节点不能引用 Morph："+id);
    PoseNode node{id,address.scheme,address.node};const auto parent=decode_uri(found->value("parent",""));
    if(!parent.empty()){if(!parent.starts_with('#'))throw std::runtime_error("分层姿势的父节点不是本文件引用："+parent);node.parent=add_node(parent.substr(1));}
    const auto index=int(preset.nodes.size());preset.nodes.push_back(std::move(node));indices[id]=index;visiting.erase(id);return index;
  };
  std::set<std::string> seen;
  for(const auto &animation:animations) {
    if(material_animation(animation))continue;
    const auto &keys=animation.at("keys");if(keys.size()!=1||keys.at(0).size()<2||keys.at(0).at(0).get<double>()!=0) throw std::runtime_error("006 只支持时间 0 的单帧姿势，尚不支持多帧动画");
    const auto url=decode_uri(animation.at("url").get<std::string>());const auto query=url.find('?');const auto protocol=url.find("://");
    if(query==std::string::npos||!seen.insert(url).second) throw std::runtime_error("姿势通道地址无效或重复："+url);
    PoseChannel c;
    if(protocol!=std::string::npos&&protocol<query)c=selection_address(url.substr(0,query));
    else {
      const auto colon=url.find(':'),hash=url.find('#');
      if(colon==std::string::npos||colon==0||hash==std::string::npos||hash<=colon||hash+1>=query)throw std::runtime_error("分层姿势通道地址无效："+url);
      c.scene_node=add_node(url.substr(0,colon));const auto &node=preset.nodes[size_t(c.scene_node)];c.scheme=node.scheme;c.node=node.node;
      // 实例名确定所属节点；DSF fragment 在变换通道中指骨骼，在 value 通道中指 Morph。
      // 不能只取 fragment，否则服装的 hip / pelvis 会覆盖人物的同名骨骼。
      if(url.substr(query+1)=="value/value"){c.modifier=url.substr(hash+1,query-hash-1);c.scheme="id";}
    }
    c.property=url.substr(query+1);c.value=keys.at(0).at(1).get<float>();
    if(!std::isfinite(c.value)) throw std::runtime_error("姿势数值必须为有限数");
    preset.channels.push_back(std::move(c));
  }
  for(const auto &extra:document.at("scene").value("extra",nlohmann::json::array()))if(extra.value("type","")=="dazfastviewer/parameters"){
    if(extra.at("version")!=1)throw std::runtime_error("不支持的参数预设扩展版本");
    std::erase_if(preset.channels,[](const PoseChannel &c){return !c.modifier.empty();});std::set<std::pair<std::string,std::string>> identities;
    for(const auto &value:extra.at("values")){PoseChannel c;c.scheme="name";c.node=value.at("node");c.modifier=value.at("modifier");c.asset=value.at("asset");c.property="value/value";c.value=value.at("value");
      if(!std::isfinite(c.value)||c.asset.empty()||!identities.emplace(c.node,c.asset).second)throw std::runtime_error("参数预设含无效或重复的资产身份");preset.channels.push_back(std::move(c));}
  }
  return preset;
}
PosePreset read_pose(const std::filesystem::path &file) {const auto u=file.generic_u8string();return parse_pose(read_document_file(file),std::string(u.begin(),u.end()));}
AppliedPose apply_pose(const PosePreset &preset,const runtime::Skin &skin,const std::vector<runtime::JointPose> &current,const runtime::Target &target,const runtime::Properties &properties,int subtree_root) {
  runtime::validate_pose(skin,current);AppliedPose result;result.joints=current;result.properties=properties;
  if(properties.morphs.size()!=target.morphs.size()||skin.instance!=target.instance) throw std::runtime_error("姿势目标与角色属性不匹配");
  result.report={{"source",preset.source},{"target",skin.id},{"applied_bone_channels",0},{"applied_morph_channels",0},{"ignored_zero_controls",0},{"unapplied",nlohmann::json::array()}};
  std::set<size_t> bones;
  if(subtree_root < -1 || subtree_root>=int(skin.joints.size())) throw std::runtime_error("局部姿势的起始骨骼无效");
  std::vector<bool> included(skin.joints.size(),subtree_root<0);
  for(size_t i=0;i<skin.joints.size();++i) if(subtree_root>=0)
    included[i]=int(i)==subtree_root||(skin.joints[i].parent>=0&&included[size_t(skin.joints[i].parent)]);
  result.report["subtree_root"]=subtree_root;result.report["excluded_channels"]=0;
  // 按预设父链在当前 Skin 中匹配，根节点也接受 DSON name_aliases（如 G8 -> G8.1）。
  // 未匹配的服装分支整体保留为未应用，绝不回退到人物中的同名骨骼。
  std::vector<int> node_joints(preset.nodes.size(),-1);
  for(size_t n=0;n<preset.nodes.size();++n){const auto &node=preset.nodes[n];if(node.parent < -1||node.parent>=int(n))throw std::runtime_error("分层姿势的节点顺序或父链无效");
    if(node.parent>=0&&node_joints[size_t(node.parent)]<0)continue;
    const int parent=node.parent<0?-1:node_joints[size_t(node.parent)];
    for(size_t j=0;j<skin.joints.size();++j)if(skin.joints[j].parent==parent&&matches_node(skin.joints[j],node.scheme,node.node)){
      if(node_joints[n]>=0)throw std::runtime_error("分层姿势骨骼地址不唯一："+node.id);node_joints[n]=int(j);}
  }
  auto skip=[&](const PoseChannel &c,const std::string &reason) {result.report["unapplied"].push_back({{"node",c.node},{"modifier",c.modifier},{"property",c.property},{"value",c.value},{"reason",reason}});};
  for(const auto &c:preset.channels) {
    int hierarchical_joint=-1;
    if(c.scene_node>=0){hierarchical_joint=node_joints.at(size_t(c.scene_node));if(hierarchical_joint<0){skip(c,"分层预设节点不属于选中角色的骨架");continue;}}
    auto matches=[&](size_t i){return c.scene_node>=0?int(i)==hierarchical_joint:matches_node(skin.joints[i],c.scheme,c.node);};
    if(subtree_root>=0) {
      bool in_scope=false;
      for(size_t i=0;i<skin.joints.size();++i)if(matches(i)&&included[i])in_scope=true;
      // Figure-wide controls can drive unrelated limbs; only channels owned by this subtree are applied.
      if(!in_scope) {result.report["excluded_channels"]=result.report["excluded_channels"].get<int>()+1;continue;}
    }
    if(!c.modifier.empty()) {
      size_t found=target.morphs.size(),matches=0;
      for(size_t i=0;i<target.morphs.size();++i) {
        const auto &m=target.morphs[i];const auto &name=c.scheme=="name"&&!m.channel_name.empty()?m.channel_name:m.channel_id;
        bool owner=false;
        if(c.scene_node>=0)owner=m.owner==skin.joints[size_t(hierarchical_joint)].id;
        else if(c.node.empty()) {owner=m.kind!="alias";for(const auto &joint:skin.joints) if(joint.parent>=0&&joint.id==m.owner) owner=false;}
        else for(const auto &joint:skin.joints) if(m.owner==joint.id) owner=c.scheme=="id"?joint.id==c.node:joint.name==c.node||std::find(joint.aliases.begin(),joint.aliases.end(),c.node)!=joint.aliases.end();
        if((c.asset.empty()?name==c.modifier:m.id==c.asset)&&owner) {found=i;++matches;}
      }
      if(matches>1) {skip(c,"同一节点下有多个同名参数，无法唯一确定目标");continue;}
      if(c.property=="value/value"&&found<target.morphs.size()&&target.morphs[found].unsupported.empty()) {
        runtime::set_parameter(target,result.properties,found,c.value);
        const auto &m=target.morphs[found];const auto &canonical=target.morphs[m.alias_morph>=0?size_t(m.alias_morph):found];
        // Presets often explicitly zero driven JCMs. A normal authored value must
        // retain ERC limits; the manual editor's unlimited mode would allow
        // negative corrective weights and leave them active on later poses.
        if(c.value>=canonical.minimum&&c.value<=canonical.maximum)result.properties.unlimited_morphs.erase(canonical.id);
        result.report["applied_morph_channels"]=result.report["applied_morph_channels"].get<int>()+1;
      }
      else if(c.value==0) result.report["ignored_zero_controls"]=result.report["ignored_zero_controls"].get<int>()+1;
      else skip(c,found<target.morphs.size()?target.morphs[found].unsupported:"目标角色未找到该控制器或 Morph");
      continue;
    }
    // name:// 使用 DSON name，不可直接拿来匹配 skin.joints 内的 DSF id。
    size_t index=skin.joints.size();
    for(size_t i=0;i<skin.joints.size();++i)if(matches(i)){if(index!=skin.joints.size())throw std::runtime_error("姿势骨骼地址不唯一："+c.node);index=i;}
    if(index==skin.joints.size()) {skip(c,"未找到目标骨骼");continue;}
    auto &p=result.joints[index];float *value=nullptr;
    const auto slash=c.property.find('/');const auto property=c.property.substr(0,slash);
    if(c.property=="general_scale/value"||c.property=="scale/general/value") value=&p.general_scale;
    else if(slash!=std::string::npos&&c.property.size()==slash+8&&c.property.substr(slash+2)=="/value") {
      const char axis=c.property[slash+1];ir::Vec3 *vector=property=="rotation"?&p.rotation_degrees:property=="translation"?&p.translation_cm:property=="scale"?&p.scale:nullptr;
      if(vector) value=axis=='x'?&vector->x:axis=='y'?&vector->y:axis=='z'?&vector->z:nullptr;
    }
    if(!value) {skip(c,"尚不支持该骨骼通道");continue;}
    *value=c.value;bones.insert(index);result.report["applied_bone_channels"]=result.report["applied_bone_channels"].get<int>()+1;
  }
  runtime::validate_pose(skin,result.joints);result.report["bones"]=bones.size();result.report["fully_applied"]=result.report["unapplied"].empty();return result;
}
}
