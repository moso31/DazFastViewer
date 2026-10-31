#pragma once
#include <string_view>
#include <string>

namespace dfv::runtime {
// 每条适配通路单独限定类型，避免把未验证的跨代骨架用于其他服装。
inline bool male_to_female_fit(std::string_view preferred,std::string_view base,std::string_view type) {
  return (preferred=="/Genesis 8/Male"||preferred=="/Genesis 8.1/Male")&&
    (base=="/Genesis 8/Female"||base=="/Genesis 8.1/Female")&&
    (type=="Follower/Wardrobe"||type.starts_with("Follower/Wardrobe/"));
}
inline bool genesis9_footwear_fit(std::string_view preferred,std::string_view base,std::string_view type) {
  return preferred=="/Genesis 9/Base"&&(base=="/Genesis 8/Female"||base=="/Genesis 8.1/Female")&&
    (type=="Follower/Wardrobe/Footwear"||type.starts_with("Follower/Wardrobe/Footwear/"));
}
inline std::string fit_reference_shape(std::string_view preferred,std::string_view base,std::string_view type) {
  if(male_to_female_fit(preferred,base,type))return "CloneGenesis8Male";
  if(genesis9_footwear_fit(preferred,base,type))return "CloneGenesis9";
  return {};
}
inline std::string genesis9_joint_to_genesis8(std::string_view name) {
  if(name.size()>2&&(name[0]=='l'||name[0]=='r')&&name[1]=='_'){
    const auto suffix=name.substr(2);std::string mapped;
    if(suffix=="foot")mapped="Foot";else if(suffix=="toes")mapped="Toe";else if(suffix=="metatarsal")mapped="Metatarsals";
    else if(suffix=="shin")mapped="Shin";else if(suffix=="thigh")mapped="ThighBend";
    else if(suffix=="thightwist1"||suffix=="thightwist2")mapped="ThighTwist";
    if(!mapped.empty())return std::string(1,name[0])+mapped;
  }
  return std::string(name);
}
inline bool genesis9_footwear_weight(std::string_view name){
  return name=="l_foot"||name=="r_foot"||name=="l_toes"||name=="r_toes"||
    name=="l_metatarsal"||name=="r_metatarsal"||name=="l_shin"||name=="r_shin";
}
}
