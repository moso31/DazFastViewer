#include "daz/pose.h"
#include "runtime/deformation.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace dfv::daz {
PosePreset parse_pose(const nlohmann::json &document,const std::string &source) {
  if(document.value("asset_info",nlohmann::json::object()).value("type","")!="preset_pose") throw std::runtime_error("所选文件不是姿势 DUF（preset_pose）");
  PosePreset preset;preset.source=source;
  const auto &animations=document.at("scene").at("animations");if(!animations.is_array()||animations.empty()) throw std::runtime_error("姿势预设没有通道");
  std::set<std::string> seen;
  for(const auto &animation:animations) {
    const auto &keys=animation.at("keys");if(keys.size()!=1||keys.at(0).size()<2||keys.at(0).at(0).get<double>()!=0) throw std::runtime_error("006 只支持时间 0 的单帧姿势，尚不支持多帧动画");
    const auto url=decode_uri(animation.at("url").get<std::string>());const auto query=url.find('?');const auto protocol=url.find("://");
    if(protocol==std::string::npos||query==std::string::npos||!seen.insert(url).second) throw std::runtime_error("姿势通道地址无效或重复："+url);
    PoseChannel c;c.scheme=url.substr(0,protocol);c.property=url.substr(query+1);c.value=keys.at(0).at(1).get<float>();
    if(!std::isfinite(c.value)) throw std::runtime_error("姿势数值必须为有限数");
    auto address=url.substr(protocol+3,query-protocol-3);if(!address.empty()&&address.back()==':') address.pop_back();
    if((c.scheme!="name"&&c.scheme!="id")||(address!="@selection"&&!address.starts_with("@selection/")&&!address.starts_with("@selection#"))) throw std::runtime_error("当前只支持作用于选中角色的姿势通道："+url);
    address.erase(0,10);const auto hash=address.find('#');if(hash!=std::string::npos) {c.modifier=address.substr(hash+1);address.resize(hash);}
    if(address.starts_with('/')) address.erase(0,1);c.node=address;preset.channels.push_back(std::move(c));
  }
  return preset;
}
PosePreset read_pose(const std::filesystem::path &file) {const auto u=file.generic_u8string();return parse_pose(read_document_file(file),std::string(u.begin(),u.end()));}
AppliedPose apply_pose(const PosePreset &preset,const runtime::Skin &skin,const std::vector<runtime::JointPose> &current,const runtime::Target &target,const runtime::Properties &properties) {
  runtime::validate_pose(skin,current);AppliedPose result;result.joints=current;result.properties=properties;
  if(properties.morphs.size()!=target.morphs.size()||skin.instance!=target.instance) throw std::runtime_error("姿势目标与角色属性不匹配");
  result.report={{"source",preset.source},{"target",skin.id},{"applied_bone_channels",0},{"applied_morph_channels",0},{"ignored_zero_controls",0},{"unapplied",nlohmann::json::array()}};
  std::set<size_t> bones;
  auto skip=[&](const PoseChannel &c,const std::string &reason) {result.report["unapplied"].push_back({{"node",c.node},{"modifier",c.modifier},{"property",c.property},{"value",c.value},{"reason",reason}});};
  for(const auto &c:preset.channels) {
    if(!c.modifier.empty()) {
      size_t found=target.morphs.size(),matches=0;
      for(size_t i=0;i<target.morphs.size();++i) {
        const auto &m=target.morphs[i];const auto &name=c.scheme=="name"&&!m.channel_name.empty()?m.channel_name:m.channel_id;
        bool owner=false;
        if(c.node.empty()) {owner=m.kind!="alias";for(const auto &joint:skin.joints) if(joint.parent>=0&&joint.id==m.owner) owner=false;}
        else for(const auto &joint:skin.joints) if(m.owner==joint.id) owner=c.scheme=="id"?joint.id==c.node:joint.name==c.node||std::find(joint.aliases.begin(),joint.aliases.end(),c.node)!=joint.aliases.end();
        if(name==c.modifier&&owner) {found=i;++matches;}
      }
      if(matches>1) {skip(c,"同一节点下有多个同名参数，无法唯一确定目标");continue;}
      if(c.property=="value/value"&&found<target.morphs.size()&&target.morphs[found].unsupported.empty()) {runtime::set_parameter(target,result.properties,found,c.value);result.report["applied_morph_channels"]=result.report["applied_morph_channels"].get<int>()+1;}
      else if(c.value==0) result.report["ignored_zero_controls"]=result.report["ignored_zero_controls"].get<int>()+1;
      else skip(c,"控制器或 Morph 尚不可求值；ERC / JCM 归属 007");
      continue;
    }
    // name:// 使用 DSON name，不可直接拿来匹配 skin.joints 内的 DSF id。
    size_t index=skin.joints.size();
    for(size_t i=0;i<skin.joints.size();++i) {const auto &j=skin.joints[i];bool match=c.node.empty()?j.parent<0:c.scheme=="id"?j.id==c.node:j.name==c.node||std::find(j.aliases.begin(),j.aliases.end(),c.node)!=j.aliases.end();
      if(match) {if(index!=skin.joints.size()) throw std::runtime_error("姿势骨骼地址不唯一："+c.node);index=i;}}
    if(index==skin.joints.size()) {skip(c,"未找到目标骨骼");continue;}
    auto &p=result.joints[index];float *value=nullptr;
    const auto slash=c.property.find('/');const auto property=c.property.substr(0,slash);
    if(c.property=="general_scale/value") value=&p.general_scale;
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
