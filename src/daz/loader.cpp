#include "daz/loader.h"
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>

namespace dfv::daz {
using Json=nlohmann::json;
namespace fs=std::filesystem;
namespace {
std::string utf8(const fs::path &path) {const auto s=path.generic_u8string();return {s.begin(),s.end()};}
[[noreturn]] void fail(const std::string &message) {throw std::runtime_error("DSON: "+message);}
std::string decode(const std::string &input) {
  std::string out;
  auto digit=[](char c)->int {if(c>='0'&&c<='9') return c-'0';if(c>='a'&&c<='f') return c-'a'+10;if(c>='A'&&c<='F') return c-'A'+10;return -1;};
  for(size_t i=0;i<input.size();++i) {
    if(input[i]=='%') {
      if(i+2>=input.size() || digit(input[i+1])<0 || digit(input[i+2])<0) fail("URI 百分号编码无效: "+input);
      const char c=char(digit(input[i+1])*16+digit(input[i+2]));if(!c) fail("URI 包含 NUL");out+=c;i+=2;
    } else out+=input[i];
  }
  return out;
}
Json read_document(const fs::path &path) {
  std::ifstream input(path,std::ios::binary);if(!input) fail("无法读取 "+utf8(path));
  std::string bytes((std::istreambuf_iterator<char>(input)),{});
  constexpr size_t limit=512*1024*1024;
  if(bytes.size()>limit) fail("文档超过 512 MiB 限制");
  if(bytes.size()>=2 && static_cast<unsigned char>(bytes[0])==0x1f && static_cast<unsigned char>(bytes[1])==0x8b) {
    z_stream stream{};stream.next_in=reinterpret_cast<Bytef *>(bytes.data());stream.avail_in=uInt(bytes.size());
    if(inflateInit2(&stream,15+32)!=Z_OK) fail("gzip 初始化失败");
    std::string unpacked;std::array<char,65536> chunk{};int code=Z_OK;
    while(code==Z_OK) {
      stream.next_out=reinterpret_cast<Bytef *>(chunk.data());stream.avail_out=uInt(chunk.size());
      code=inflate(&stream,Z_NO_FLUSH);unpacked.append(chunk.data(),chunk.size()-stream.avail_out);
      if(unpacked.size()>limit) {inflateEnd(&stream);fail("gzip 解压结果超过限制");}
      // RFC 1952 允许连续 gzip member；部分 DAZ 资源附带第二个空 member。
      if(code==Z_STREAM_END && stream.avail_in) {
        if(stream.avail_in<2 || stream.next_in[0]!=0x1f || stream.next_in[1]!=0x8b) break;
        auto *next=stream.next_in;const auto remaining=stream.avail_in;
        code=inflateReset2(&stream,15+32);stream.next_in=next;stream.avail_in=remaining;
      }
    }
    const auto trailing=stream.avail_in;inflateEnd(&stream);
    if(code!=Z_STREAM_END || trailing) fail("损坏或不支持的 gzip 文档: "+utf8(path));
    bytes=std::move(unpacked);
  }
  try {return Json::parse(bytes);} catch(const Json::exception &e) {fail(utf8(path)+": "+e.what());}
}
const Json &values(const Json &object) {
  if(object.is_array()) return object;
  if(!object.is_object() || !object.contains("values") || !object["values"].is_array()) fail("数组格式错误");
  if(object.contains("count") && object.at("count").get<size_t>()!=object["values"].size()) fail("数组 count 与内容不一致");
  return object["values"];
}
struct Repository {
  std::vector<fs::path> roots;
  std::map<std::string,Json> documents;
  std::set<std::string> dependencies;
  fs::path path(const std::string &uri,const fs::path &owner) {
    std::string raw=uri.substr(0,uri.find('#'));
    if(raw.empty()) return owner;
    if(raw.find('?')!=std::string::npos || raw.find(':')!=std::string::npos) fail("不支持的资产 URI: "+uri);
    raw=decode(raw);
    const bool absolute=raw.starts_with('/');
    if(absolute) raw.erase(0,1);
    const auto relative=fs::u8path(raw).lexically_normal();
    if(relative.is_absolute() || relative.has_root_name() || (!relative.empty() && *relative.begin()=="..")) fail("资产路径越过内容根目录: "+uri);
    std::vector<fs::path> candidates;
    if(!absolute) candidates.push_back(owner.parent_path()/relative);
    for(const auto &root:roots) candidates.push_back(root/relative);
    for(const auto &p:candidates) if(fs::is_regular_file(p)) {auto file=fs::weakly_canonical(p);dependencies.insert(utf8(file));return file;}
    fail("依赖缺失: "+uri+"（来源 "+utf8(owner)+"）");
  }
  const Json &document(const fs::path &path) {
    const auto key=utf8(fs::weakly_canonical(path));dependencies.insert(key);
    auto found=documents.find(key);if(found==documents.end()) found=documents.emplace(key,read_document(path)).first;
    return found->second;
  }
  std::pair<fs::path,const Json *> asset(const std::string &uri,const fs::path &owner,const char *library) {
    const auto hash=uri.find('#');if(hash==std::string::npos) fail("资产引用缺少 fragment: "+uri);
    const auto id=decode(uri.substr(hash+1));const auto file=path(uri,owner);const auto &doc=document(file);
    if(doc.contains(library)) for(const auto &entry:doc.at(library)) if(entry.value("id","")==id) return {file,&entry};
    fail("资产 ID 不存在: "+uri+" in "+library);
  }
};
float number(const Json &channel,float fallback) {
  if(channel.is_number()) return channel.get<float>();
  if(channel.is_boolean()) return channel.get<bool>()?1.0f:0.0f;
  if(!channel.is_object()) return fallback;
  return number(channel.value("current_value",channel.value("value",Json(fallback))),fallback);
}
ir::Vec3 axes(const Json &node,const char *key,ir::Vec3 fallback) {
  if(!node.contains(key)) return fallback;
  const auto &channels=node.at(key);if(!channels.is_array()) fail(std::string("无效的变换通道: ")+key);
  if(channels.size()==3 && channels[0].is_number()) return {channels[0].get<float>(),channels[1].get<float>(),channels[2].get<float>()};
  for(const auto &channel:channels) {
    const auto id=channel.value("id","");
    if(id=="x") fallback.x=number(channel,fallback.x);else if(id=="y") fallback.y=number(channel,fallback.y);else if(id=="z") fallback.z=number(channel,fallback.z);
  }
  return fallback;
}
Json merge_node(Json base,const Json &instance) {
  for(auto it=instance.begin();it!=instance.end();++it) {
    if(it.key()=="extra"&&it.value().is_array()) {
      auto extras=base.value("extra",Json::array());
      for(const auto &entry:it.value()) {
        auto old=std::find_if(extras.begin(),extras.end(),[&](const Json &e) {return e.value("type","")==entry.value("type","");});
        if(old==extras.end()) {extras.push_back(entry);continue;}
        auto channels=old->value("channels",Json::array());old->update(entry);
        for(const auto &c:entry.value("channels",Json::array())) {
          auto found=std::find_if(channels.begin(),channels.end(),[&](const Json &a) {return a.at("channel").value("id","")==c.at("channel").value("id","");});
          if(found==channels.end()) channels.push_back(c);else (*found)["channel"].update(c.at("channel"));
        }
        if(!channels.empty()) (*old)["channels"]=std::move(channels);
      }
      base["extra"]=std::move(extras);
    } else if(it.value().is_array() && (it.key()=="rotation" || it.key()=="translation" || it.key()=="scale" || it.key()=="center_point" || it.key()=="orientation")) {
      auto data=base.value(it.key(),Json::array());
      for(const auto &channel:it.value()) {
        bool merged=false;
        if(channel.is_object()) for(auto &old:data) if(old.value("id","")==channel.value("id","")) {old.update(channel);merged=true;break;}
        if(!merged) data.push_back(channel);
      }
      base[it.key()]=data;
    } else base[it.key()]=it.value();
  }
  return base;
}
ir::Transform rotation(ir::Vec3 degrees,const std::string &order) {
  if(order.size()!=3 || std::set<char>(order.begin(),order.end())!=std::set<char>{'X','Y','Z'}) fail("无效 rotation_order");
  ir::Transform result;
  for(char axis:order) {
    const int a=axis=='X'?0:axis=='Y'?1:2;const float angle=(a==0?degrees.x:a==1?degrees.y:degrees.z)*std::numbers::pi_v<float>/180;
    ir::Transform r;const int j=(a+1)%3,k=(a+2)%3;
    r.value[j*4+j]=r.value[k*4+k]=std::cos(angle);r.value[j*4+k]=-std::sin(angle);r.value[k*4+j]=std::sin(angle);result=r*result;
  }
  return result;
}
ir::Transform transpose_rotation(const ir::Transform &r) {
  ir::Transform t;for(int i=0;i<3;++i) for(int j=0;j<3;++j) t.value[i*4+j]=r.value[j*4+i];return t;
}
ir::Vec3 to_render(ir::Vec3 p) {return {p.x*.01f,-p.z*.01f,p.y*.01f};}
ir::Transform render_transform(const ir::Transform &daz) {
  ir::Transform c;c.value={.01f,0,0,0, 0,0,-.01f,0, 0,.01f,0,0};
  ir::Transform inverse;inverse.value={100,0,0,0, 0,0,100,0, 0,-100,0,0};
  return c*daz*inverse;
}
using Channels=std::map<std::string,Json>;
void add_channels(Channels &channels,const Json &material) {
  if(material.contains("diffuse")) channels["diffuse"].update(material.at("diffuse").at("channel"));
  for(const auto &extra:material.value("extra",Json::array())) for(const auto &entry:extra.value("channels",Json::array())) {
    if(!entry.contains("channel")) continue;
    const auto &c=entry["channel"];const auto id=c.at("id").get<std::string>();
    if(!channels.contains(id)) channels[id]=Json::object();channels[id].update(c);
  }
}
}

Json read_document_file(const fs::path &file) {return read_document(file);}
std::string decode_uri(const std::string &uri) {return decode(uri);}
DufContents inspect_contents(const Json &document) {
  DufContents result;const auto &scene=document.value("scene",Json::object());
  const auto type=document.value("asset_info",Json::object()).value("type","");
  result.materials=!scene.value("materials",Json::array()).empty();
  result.properties=!scene.value("animations",Json::array()).empty();
  for(const auto &node:scene.value("nodes",Json::array())) {
    const auto kind=node.value("type","");
    result.instantiate|=!node.value("geometries",Json::array()).empty()||kind=="light"||kind=="camera";
  }
  // 类型只补充无法从实例字段推断的用途；复合文件优先按实际内容处理。
  result.instantiate|=type=="scene"||type=="preset_scene"||type=="preset_light"||type=="preset_lights";
  return result;
}
LoadedScene load(const fs::path &input,const LoadOptions &options) {
  const auto file=fs::weakly_canonical(input);
  if(!fs::is_regular_file(file)) fail("输入文件不存在: "+utf8(file));
  Repository repo;for(const auto &root:options.content_roots) repo.roots.push_back(fs::weakly_canonical(root));
  for(auto p=file.parent_path();!p.empty() && p!=p.parent_path();p=p.parent_path()) {
    if(fs::is_directory(p/"data")) {if(std::find(repo.roots.begin(),repo.roots.end(),p)==repo.roots.end()) repo.roots.push_back(p);break;}
  }
  if(repo.roots.empty()) fail("不能推断内容库；请使用 --content-root");
  const auto &document=repo.document(file);if(!document.contains("scene")) fail("文件中没有 scene");
  const auto &source=document.at("scene");LoadedScene out;auto &scene=out.scene;
  Json warnings=Json::array(),material_reports=Json::array(),geometry_reports=Json::array();
  auto warn=[&](const std::string &code,const std::string &asset,const std::string &detail) {warnings.push_back({{"code",code},{"asset",asset},{"detail",detail}});};
  std::map<std::pair<std::string,ir::ColorSpace>,int> textures;
  auto texture=[&](const Json &channel,ir::ColorSpace colorspace,const fs::path &owner)->int {
    const auto uri=channel.value("image_file","");if(uri.empty()) return -1;
    const auto path=repo.path(uri,owner);const auto key=std::make_pair(utf8(path),colorspace);
    if(auto found=textures.find(key);found!=textures.end()) return found->second;
    const int index=int(scene.textures.size());scene.textures.push_back({key.first,path,colorspace});textures.emplace(key,index);return index;
  };
  struct Binding {uint32_t material;std::string uv;};
  std::map<std::pair<std::string,std::string>,Binding> bindings;
  for(const auto &instance:source.value("materials",Json::array())) {
    Channels channels;fs::path material_file=file;
    if(instance.contains("url")) {auto [base_file,base]=repo.asset(instance.at("url"),file,"material_library");material_file=base_file;add_channels(channels,*base);}
    add_channels(channels,instance);
    ir::Material material;material.id=instance.at("id").get<std::string>();
    auto channel=[&](const std::string &name)->Json {auto i=channels.find(name);return i==channels.end()?Json::object():i->second;};
    auto scalar=[&](const std::string &name,float fallback) {return number(channel(name),fallback);};
    auto unit=[&](const std::string &name,float fallback=0) {return std::clamp(scalar(name,fallback),0.f,1.f);};
    auto linear=[](float v) {return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
    auto color_value=[&](const std::string &name,ir::Vec3 fallback) {
      const auto c=channel(name);const auto value=c.value("current_value",c.value("value",Json()));
      if(!value.is_array()||value.size()!=3) return fallback;
      return ir::Vec3{linear(value[0].get<float>()),linear(value[1].get<float>()),linear(value[2].get<float>())};
    };
    const auto diffuse=channel("diffuse");const auto color=diffuse.value("current_value",diffuse.value("value",Json::array({1,1,1})));
    if(!color.is_array() || color.size()!=3) fail("材质 diffuse 必须为三通道颜色");
    material.base_color=color_value("diffuse",{1,1,1});
    material.roughness=std::clamp(number(channel("Glossy Roughness"),.5f),0.0f,1.0f);
    material.metallic=std::clamp(number(channel("Metallic Weight"),0),0.0f,1.0f);
    material.opacity=std::clamp(number(channel("Cutout Opacity"),1),0.0f,1.0f);
    material.transmission=std::clamp(number(channel("Refraction Weight"),0),0.0f,1.0f);
    // 非透射 Iray Uber 的 Refraction Index 不控制表面高光；参考的 Principled 保持 1.5。
    material.ior=material.transmission>0?std::max(1.0f,number(channel("Refraction Index"),1.5f)):1.5f;
    material.normal_strength=number(channel("Normal Map"),1);
    material.bump_strength=std::max(0.0f,number(channel("Bump Strength"),1));
    const bool explicit_bump_range=channels.contains("Bump Minimum") && channels.contains("Bump Maximum");
    if(explicit_bump_range) material.bump_distance=.01f*(number(channel("Bump Maximum"),0)-number(channel("Bump Minimum"),0));
    material.color_texture=texture(diffuse,ir::ColorSpace::srgb,material_file);
    material.roughness_texture=texture(channel("Glossy Roughness"),ir::ColorSpace::linear,material_file);
    material.opacity_texture=texture(channel("Cutout Opacity"),ir::ColorSpace::linear,material_file);
    material.normal_texture=texture(channel("Normal Map"),ir::ColorSpace::linear,material_file);
    material.bump_texture=texture(channel("Bump Strength"),ir::ColorSpace::linear,material_file);
    material.thin_walled=scalar("Thin Walled",0)!=0;
    material.roughness_from_glossiness=int(scalar("Base Mixing",0))==1;
    if(material.roughness_from_glossiness) {
      material.roughness=unit("Glossiness",.5f);
      material.roughness_texture=texture(channel("Glossiness"),ir::ColorSpace::linear,material_file);
    }
    const auto glossy=int(scalar("Base Mixing",0))==2?"Glossy Weight":"Glossy Layered Weight";
    material.specular=unit(glossy,1)*unit("Glossy Reflectivity",.5f);
    material.weighted_glossy=int(scalar("Base Mixing",0))==2;
    if(material.weighted_glossy) {const float weight=unit("Glossy Weight");material.specular=weight/std::max(1e-6f,weight+unit("Diffuse Weight",1));}
    material.specular_color=color_value("Glossy Color",{1,1,1});
    material.specular_texture=texture(channel(glossy),ir::ColorSpace::linear,material_file);
    material.anisotropy=unit("Glossy Anisotropy");material.anisotropy_rotation=scalar("Glossy Anisotropy Rotations",0);
    material.translucency_color=color_value("Translucency Color",{1,1,1});
    material.translucency_texture=texture(channel("Translucency Weight"),ir::ColorSpace::linear,material_file);
    material.translucency_color_texture=texture(channel("Translucency Color"),ir::ColorSpace::srgb,material_file);
    if(material.thin_walled) material.translucency=unit("Translucency Weight");
    else {
      material.subsurface=unit("Translucency Weight");
      material.subsurface_anisotropy=std::clamp(scalar("SSS Direction",0),-.9f,.9f);
      const auto transmitted=color_value("Transmitted Color",{1,1,1});
      const auto scatter=color_value("SSS Color",{.5f,.5f,.5f});
      const float td=std::max(1e-6f,.01f*scalar("Transmitted Measurement Distance",.1f));
      const float sd=std::max(1e-6f,.01f*scalar("Scattering Measurement Distance",.1f));
      auto radius=[&](float t,float s) {return std::clamp(1.f/std::max(1.f,-std::log(std::clamp(t,.0001f,.9999f))/td-std::log(std::clamp(s,.0001f,.9999f))/sd),1e-6f,.02f);};
      material.subsurface_radius={radius(transmitted.x,scatter.x),radius(transmitted.y,scatter.y),radius(transmitted.z,scatter.z)};
      if(material.subsurface>0) warn("sss_transport_approximation",material.id,"按厘米测量距离与颜色构建米制散射半径；Cycles Random Walk 与 Iray 体积散射不保证等价");
    }
    material.coat=unit("Top Coat Weight");material.coat_roughness=unit("Top Coat Roughness",.1f);material.coat_ior=std::max(1.f,scalar("Top Coat IOR",1.5f));
    material.coat_color=color_value("Top Coat Color",{1,1,1});
    material.coat_texture=texture(channel("Top Coat Weight"),ir::ColorSpace::linear,material_file);
    material.coat_roughness_texture=texture(channel("Top Coat Roughness"),ir::ColorSpace::linear,material_file);
    material.dual_weight=unit("Dual Lobe Specular Weight");material.dual_ratio=unit("Dual Lobe Specular Ratio",.5f);
    material.dual_roughness1=unit("Specular Lobe 1 Roughness",.3f);material.dual_roughness2=unit("Specular Lobe 2 Roughness",.6f);
    material.dual_specular=unit("Dual Lobe Specular Reflectivity",.5f);
    material.dual_texture=texture(channel("Dual Lobe Specular Weight"),ir::ColorSpace::linear,material_file);
    material.metallic_texture=texture(channel("Metallic Weight"),ir::ColorSpace::linear,material_file);
    material.transmission_texture=texture(channel("Refraction Weight"),ir::ColorSpace::linear,material_file);
    material.uv_scale={scalar("Horizontal Tiles",1),scalar("Vertical Tiles",1)};
    material.uv_offset={scalar("Horizontal Offset",0),scalar("Vertical Offset",0)};
    if(material.transmission>0) {
      // 透射层使用折射颜色，不能把视口漫反射颜色乘进角膜和泪膜。
      const bool share=scalar("Share Glossy Inputs",0)!=0;
      material.base_color=color_value(share?"Glossy Color":"Refraction Color",{1,1,1});
      material.color_texture=texture(channel(share?"Glossy Color":"Refraction Color"),ir::ColorSpace::srgb,material_file);
      if(!share) {material.roughness=unit("Refraction Roughness");material.roughness_texture=texture(channel("Refraction Roughness"),ir::ColorSpace::linear,material_file);material.roughness_from_glossiness=false;}
    }
    material.hair=channels.contains("Hair Root Color")||channels.contains("Root Transmission Color");
    material.hair_root_radius=.0005f*std::max(0.f,scalar("Line Start Width",.1f));
    material.hair_tip_radius=.0005f*std::max(0.f,scalar("Line End Width",.05f));
    if(material.hair) {
      material.base_color=color_value(channels.contains("Hair Root Color")?"Hair Root Color":"Root Transmission Color",{.1f,.1f,.1f});
      material.hair_tip_color=color_value(channels.contains("Hair Tip Color")?"Hair Tip Color":"Tip Transmission Color",material.base_color);
      material.roughness=unit("Roughness",.3f);material.hair_radial_roughness=unit("Azimuthal Roughness",.3f);
      material.hair_melanin=unit("Melanin");material.hair_redness=unit("Melanin Redness",.5f);
      material.color_texture=texture(channel("Hair Root Color"),ir::ColorSpace::srgb,material_file);
      material.roughness_from_glossiness=false;
    }
    if(material.bump_texture>=0 && !explicit_bump_range)
      warn("bump_distance_approximation",material.id,"资产未提供凹凸高度范围；暂用 1 毫米。参考 Importer 根据几何和纹理得到不同距离，尚未对齐");
    const std::set<std::string> supported={"diffuse","Glossy Roughness","Metallic Weight","Cutout Opacity","Refraction Weight","Refraction Index","Normal Map","Bump Strength","Bump Minimum","Bump Maximum",
      "Thin Walled","Base Mixing","Glossiness","Glossy Layered Weight","Glossy Weight","Glossy Reflectivity","Glossy Color","Glossy Anisotropy","Glossy Anisotropy Rotations",
      "Translucency Weight","Translucency Color","SSS Direction","Transmitted Color","SSS Color","Transmitted Measurement Distance","Scattering Measurement Distance",
      "Top Coat Weight","Top Coat Roughness","Top Coat IOR","Top Coat Color","Dual Lobe Specular Weight","Dual Lobe Specular Ratio","Dual Lobe Specular Reflectivity","Specular Lobe 1 Roughness","Specular Lobe 2 Roughness",
      "Horizontal Tiles","Vertical Tiles","Horizontal Offset","Vertical Offset","Share Glossy Inputs","Refraction Color","Refraction Roughness","Line Start Width","Line End Width",
      "Hair Root Color","Hair Tip Color","Root Transmission Color","Tip Transmission Color","Roughness","Azimuthal Roughness","Melanin","Melanin Redness"};
    for(const char *id:{"Refraction Index","Glossy Color","Top Coat Color","Specular Lobe 1 Roughness","Specular Lobe 2 Roughness","Hair Tip Color"})
      if(!channel(id).value("image_file","").empty()) warn("unmapped_channel_texture",material.id,std::string(id)+" 目前仅支持常量，贴图尚未映射");
    Json unmapped=Json::array();for(const auto &[id,c]:channels) if(!supported.contains(id)) unmapped.push_back(id);
    if(!unmapped.empty()) warn("material_subset",material.id,"仅映射基础 PBR 参数；未映射通道详见 materials.unmapped_channels");
    material_reports.push_back({{"id",material.id},{"groups",instance.value("groups",Json::array())},{"unmapped_channels",unmapped},{"color_texture",material.color_texture},
      {"roughness_texture",material.roughness_texture},{"normal_texture",material.normal_texture},{"bump_texture",material.bump_texture},
      {"hair",material.hair},{"thin_walled",material.thin_walled},{"subsurface_weight",material.subsurface},{"translucency_weight",material.translucency},
      {"subsurface_radius_m",{material.subsurface_radius.x,material.subsurface_radius.y,material.subsurface_radius.z}},{"dual_lobe_weight",material.dual_weight},
      {"line_root_radius_m",material.hair_root_radius},{"line_tip_radius_m",material.hair_tip_radius},
      {"bump_distance_m",material.bump_distance},{"bump_distance_source",explicit_bump_range?"explicit-centimeter-range":"preview-approximation"}});
    const uint32_t index=uint32_t(scene.materials.size());scene.materials.push_back(material);
    const auto geometry=decode(instance.value("geometry",""));const auto uv=instance.value("uv_set","");
    for(const auto &group:instance.at("groups")) bindings[{geometry,group.get<std::string>()}]={index,uv};
  }
  std::map<std::string,Json> nodes;
  for(const auto &instance:source.value("nodes",Json::array())) {
    Json base=Json::object();if(instance.contains("url")) base=*repo.asset(instance.at("url"),file,"node_library").second;
    nodes.emplace(instance.at("id").get<std::string>(),merge_node(base,instance));
  }
  for(const auto &[id,node]:nodes) out.nodes.push_back({id,decode(node.value("parent",""))});
  std::map<std::string,ir::Transform> transforms;std::set<std::string> visiting;
  std::function<ir::Transform(const std::string &)> world=[&](const std::string &id) {
    if(auto old=transforms.find(id);old!=transforms.end()) return old->second;
    if(!visiting.insert(id).second) fail("节点层次存在循环: "+id);
    auto found=nodes.find(id);if(found==nodes.end()) fail("父节点不存在: "+id);const auto &n=found->second;
    const auto pivot=axes(n,"center_point",{}),t=axes(n,"translation",{}),r=axes(n,"rotation",{}),s=axes(n,"scale",{1,1,1});
    const float general=number(n.value("general_scale",Json(1)),1);
    ir::Transform scale;scale.value[0]=s.x*general;scale.value[5]=s.y*general;scale.value[10]=s.z*general;
    const auto orientation=rotation(axes(n,"orientation",{}),"XYZ");
    auto matrix=ir::Transform::translate({pivot.x+t.x,pivot.y+t.y,pivot.z+t.z})*orientation*rotation(r,n.value("rotation_order","XYZ"))*scale*transpose_rotation(orientation)*ir::Transform::translate({-pivot.x,-pivot.y,-pivot.z});
    const auto parent=n.value("parent","");
    if(!parent.empty()) {
      if(!parent.starts_with('#')) fail("不支持跨文件的场景实例 parent: "+parent);
      matrix=world(decode(parent.substr(1)))*matrix;
    }
    if(!n.value("inherits_scale",true)) warn("scale_compensation",id,"未实现 DAZ 父级缩放补偿；非单位父级缩放可能产生差异");
    visiting.erase(id);transforms.emplace(id,matrix);return matrix;
  };
  std::map<std::string,uint32_t> mesh_cache;
  auto fitted_world=[&](const std::string &id) {
    const auto original=world(id); // 同时先验证 parent 链，避免损坏文件的循环遍历。
    auto terminal=id;std::set<std::string> seen;
    while(nodes.at(terminal).contains("conform_target")&&nodes.at(terminal)["conform_target"].is_string()) {
      if(!seen.insert(terminal).second) fail("Fit To 关系存在循环: "+id);
      const auto ref=decode(nodes.at(terminal)["conform_target"].get<std::string>());
      if(ref.empty()) break;
      if(!ref.starts_with('#')||!nodes.contains(ref.substr(1))) fail("Fit To 目标不存在: "+ref);
      terminal=ref.substr(1);
    }
    auto root=id;bool already_inherited=root==terminal;
    while(!nodes.at(root).value("parent","").empty()) {root=decode(nodes.at(root).at("parent").get<std::string>()).substr(1);already_inherited|=root==terminal;}
    // 同一 Figure 下的服装已经含父变换。根层级穿戴物需继承目标的场景变换。
    return terminal!=id&&!already_inherited?world(terminal)*original:original;
  };
  for(const auto &instance:source.value("nodes",Json::array())) {
    const auto id=instance.at("id").get<std::string>();const auto &node=nodes.at(id);
    if(node.value("type","")=="camera") warn("scene_camera",id,"保留编辑器观察相机，未切换到保存的 DAZ 相机");
    if(node.value("type","")=="light") {
      const auto color=node.value("color",Json::array({1,1,1}));ir::Vec3 rgb{1,1,1};
      if(color.is_array()&&color.size()==3) rgb={color[0].get<float>(),color[1].get<float>(),color[2].get<float>()};
      const char *kind=node.contains("spot")?"spot":node.contains("directional")?"directional":node.contains("point")?"point":"";
      if(!*kind) scene.environment=rgb;
      else {
        const auto &settings=node.at(kind);ir::AreaLight light;light.id=id;light.transform=render_transform(world(id));
        light.kind=std::string(kind)=="spot"?ir::LightKind::spot:std::string(kind)=="point"?ir::LightKind::point:ir::LightKind::distant;
        const auto intensity=node.value("on",true)?number(settings.value("intensity",Json(1)),1):0;
        light.power={rgb.x*intensity,rgb.y*intensity,rgb.z*intensity};light.angle=number(settings.value("falloff_angle",Json(45)),45)*std::numbers::pi_v<float>/180;
        scene.lights.push_back(light);
        if(node.contains("extra")) warn("light_extensions",id,"已导入 DSON 基础灯光；Iray 光度与专用扩展尚未等价转换");
      }
    }
    if(node.value("type","")=="bone") {
      const auto r=axes(node,"rotation",{}),t=axes(node,"translation",{});
      if(std::abs(r.x)+std::abs(r.y)+std::abs(r.z)+std::abs(t.x)+std::abs(t.y)+std::abs(t.z)>1e-6f) warn("bone_pose",id,"本阶段显示静态基础网格，未应用骨骼变形");
    }
    for(const auto &geometry_instance:instance.value("geometries",Json::array())) {
      const auto uri=geometry_instance.at("url").get<std::string>();const auto [geometry_file,gptr]=repo.asset(uri,file,"geometry_library");const auto &g=*gptr;
      ir::Mesh mesh;mesh.id=uri;
      if(g.contains("graft")&&g["graft"].contains("vertex_count")) {
        mesh.graft_target_vertices=g["graft"]["vertex_count"].get<uint32_t>();
        for(const auto &p:values(g["graft"].at("hidden_polys"))) mesh.graft_hidden_polygons.push_back(p.get<uint32_t>());
      }
      if(g.contains("polygon_groups")) for(const auto &name:values(g.at("polygon_groups"))) mesh.polygon_groups.push_back(name.get<std::string>());
      for(const auto &name:values(g.at("polygon_material_groups"))) mesh.material_slots.push_back(name.get<std::string>());
      std::vector<uint32_t> material_indices;std::vector<std::string> uv_refs;
      const auto geometry_id=geometry_instance.at("id").get<std::string>();
      for(const auto &group:mesh.material_slots) {
        auto binding=bindings.find({"#"+geometry_id,group});
        if(binding==bindings.end()) fail("缺少材质绑定: "+geometry_id+" / "+group);
        material_indices.push_back(binding->second.material);uv_refs.push_back(binding->second.uv.empty()?g.value("default_uv_set",""):binding->second.uv);
      }
      std::string key=uri;for(const auto &uv:uv_refs) key+='\n'+uv;
      uint32_t mesh_index;
      if(auto cached=mesh_cache.find(key);cached!=mesh_cache.end()) mesh_index=cached->second;
      else {
        const auto &positions=values(g.at("vertices"));
        for(const auto &p:positions) {if(!p.is_array()||p.size()!=3) fail("无效顶点");mesh.positions.push_back(to_render({p[0].get<float>(),p[1].get<float>(),p[2].get<float>()}));}
        struct UV {std::vector<ir::Vec2> values;std::map<std::pair<uint32_t,uint32_t>,uint32_t> seams;};
        std::map<std::string,UV> uv_cache;
        for(const auto &ref:uv_refs) if(!uv_cache.contains(ref)) {
          if(ref.empty()) fail("缺少 UV Set: "+uri);
          const auto [uv_file,uptr]=repo.asset(ref,ref.starts_with('#')?geometry_file:file,"uv_set_library");const auto &u=*uptr;
          if(u.at("vertex_count").get<size_t>()!=mesh.positions.size()) fail("UV Set 顶点数与网格不符: "+ref);
          UV uv;
          for(const auto &p:values(u.at("uvs"))) {if(!p.is_array()||p.size()!=2) fail("无效 UV 坐标");uv.values.push_back({p[0].get<float>(),p[1].get<float>()});}
          for(const auto &s:u.value("polygon_vertex_indices",Json::array())) {
            if(s.size()!=3) fail("无效 UV 接缝记录");uv.seams[{s[0].get<uint32_t>(),s[1].get<uint32_t>()}]=s[2].get<uint32_t>();
          }
          uv_cache.emplace(ref,std::move(uv));
        }
        const auto &polygons=values(g.at("polylist"));
        if(g.contains("polyline_list")) for(const auto &line:values(g.at("polyline_list"))) {
          if(line.size()<4) fail("发丝至少需要两个控制点");
          ir::Curve curve;curve.material_slot=line[1].get<uint32_t>();
          if(curve.material_slot>=mesh.material_slots.size()) fail("发丝材质槽越界");
          for(size_t k=2;k<line.size();++k) {const auto v=line[k].get<uint32_t>();if(v>=mesh.positions.size()) fail("发丝顶点越界");curve.vertices.push_back(v);}
          const auto &uv=uv_cache.at(uv_refs.at(curve.material_slot));
          if(curve.vertices.front()<uv.values.size()) curve.uv=uv.values[curve.vertices.front()];
          mesh.curves.push_back(std::move(curve));
        }
        if(!mesh.curves.empty()) for(const auto &modifier:repo.document(geometry_file).value("modifier_library",Json::array()))
          for(const auto &extra:modifier.value("extra",Json::array())) if(extra.value("type","")=="studio/modifier/dynamic_generate_hair"&&extra.value("generates_for_render",false))
            warn("strand_render_generator",geometry_id,"已导入资产保存的发丝曲线与蒙皮；额外渲染发丝生成及 dForce 模拟尚未复现");
        for(size_t p=0;p<polygons.size();++p) {
          const auto &polygon=polygons[p];if(polygon.size()<5 || polygon.size()>6) fail("仅支持 DSON 三角形和四边形");
          const auto slot=polygon[1].get<uint32_t>();if(slot>=mesh.material_slots.size()) fail("多边形材质组索引越界");
          const auto &uv=uv_cache.at(uv_refs.at(slot));
          for(size_t k=3;k+1<polygon.size();++k) {
            ir::Triangle triangle;triangle.material_slot=slot;
            triangle.polygon_group=polygon[0].get<uint32_t>();
            triangle.source_polygon=uint32_t(p);
            triangle.vertices={polygon[2].get<uint32_t>(),polygon[k].get<uint32_t>(),polygon[k+1].get<uint32_t>()};
            for(size_t c=0;c<3;++c) {
              const auto v=triangle.vertices[c];auto seam=uv.seams.find({uint32_t(p),v});const auto ui=seam==uv.seams.end()?v:seam->second;
              if(ui>=uv.values.size()) fail("UV 索引越界");triangle.uv[c]=uv.values[ui];
            }
            mesh.triangles.push_back(triangle);
          }
        }
        geometry_reports.push_back({{"id",geometry_id},{"vertices",mesh.positions.size()},{"polygons",polygons.size()},{"triangles",mesh.triangles.size()},{"curves",mesh.curves.size()},{"material_groups",mesh.material_slots}});
        mesh_index=uint32_t(scene.meshes.size());mesh_cache.emplace(key,mesh_index);scene.meshes.push_back(std::move(mesh));
      }
      ir::Instance render_instance;render_instance.id=id+"/"+geometry_id;render_instance.mesh=mesh_index;render_instance.materials=std::move(material_indices);render_instance.transform=render_transform(fitted_world(id));
      for(const auto &extra:node.value("extra",Json::array())) for(const auto &entry:extra.value("channels",Json::array())) {
        const auto &c=entry.at("channel");if(c.value("id","")=="Visible") render_instance.visible=number(c,1)!=0;
      }
      out.objects.push_back({uint32_t(scene.instances.size()),id,node.value("label",node.value("name",id)),decode(node.value("parent","")),g.at("id").get<std::string>(),geometry_file,node.value("type","")=="figure",geometry_id,node.contains("conform_target")&&node["conform_target"].is_string()?decode_uri(node["conform_target"].get<std::string>()):""});
      // DUF 可以内嵌派生几何；保持其顶点、UV 与材质，同时解析可继承的源资产身份。
      auto owner=geometry_file;const Json *derived=&g;std::set<std::pair<fs::path,std::string>> ancestors{{owner,g.at("id").get<std::string>()}};
      while(!derived->value("source","").empty()) {
        const auto [source_file,base]=repo.asset(derived->at("source"),owner,"geometry_library");const auto source_id=base->at("id").get<std::string>();
        if(!ancestors.emplace(source_file,source_id).second) fail("派生几何 source 形成循环: "+uri);
        bool compatible=values(derived->at("vertices")).size()==values(base->at("vertices")).size();
        const auto &a=values(derived->at("polylist")),&b=values(base->at("polylist"));compatible&=a.size()==b.size();
        for(size_t p=0;compatible&&p<a.size();++p) {
          compatible=a[p].size()==b[p].size();
          for(size_t v=2;compatible&&v<a[p].size();++v) compatible=a[p][v]==b[p][v];
        }
        if(!compatible) {warn("derived_geometry_topology",geometry_id,"派生几何拓扑不同，未继承源资产的 Morph / 蒙皮索引");break;}
        out.objects.back().geometry_sources.push_back({source_file,source_id});owner=source_file;derived=base;
      }
      scene.instances.push_back(std::move(render_instance));
      if(geometry_instance.value("type",g.value("type",""))=="subdivision_surface") warn("subdivision",geometry_id,"当前显示基础笼形网格；未应用 SubD/HD 细分");
    }
  }
  for(const auto &instance:source.value("modifiers",Json::array())) {
    if(instance.contains("channel")||instance.contains("skin")) continue;
    Json modifier=instance;
    if(instance.contains("url")) modifier=merge_node(*repo.asset(instance.at("url"),file,"modifier_library").second,instance);
    bool smoothing=false;std::map<std::string,Json> channels;
    for(const auto &extra:modifier.value("extra",Json::array())) {
      smoothing|=extra.value("type","")=="studio/modifier/smoothing";
      for(const auto &entry:extra.value("channels",Json::array())) if(entry.contains("channel")) {const auto &c=entry["channel"];channels[c.at("id").get<std::string>()]=c;}
    }
    if(!smoothing) continue;
    auto value=[&](const char *id,float fallback) {auto c=channels.find(id);return c==channels.end()?fallback:number(c->second,fallback);};
    ir::MeshSmoothing settings;settings.enabled=value("Enable Smoothing",1)!=0;
    settings.smoothing_iterations=int(std::clamp(value("Smoothing Iterations",2),0.f,200.f));
    settings.collision_iterations=int(std::clamp(value("Collision Iterations",3),0.f,100.f));settings.weight=std::clamp(value("Weight",.5f),0.f,1.f);
    if(auto c=channels.find("Collision Item");c!=channels.end()&&c->second.contains("node")&&c->second["node"].is_string()) settings.collision_target=decode(c->second["node"].get<std::string>());
    const auto parent=decode(instance.value("parent",""));
    for(auto &object:out.objects) if(parent=="#"+object.id||parent=="#"+object.geometry_instance_id) object.smoothing=settings;
    if(settings.enabled) warn("mesh_collision_approximation",parent,"蒙皮后执行表面碰撞与修正位移平滑；不是 DAZ Pyramid Coordinates 或 dForce 的等价实现");
  }
  if(source.contains("modifiers")&&!source["modifiers"].empty())
    warn("runtime_modifiers",std::to_string(source["modifiers"].size()),"几何阶段不求值 Modifier；由后续 Morph / Skeleton / Formula 阶段应用并报告支持情况");
  if(source.contains("animations")&&!source["animations"].empty())
    warn("scene_animation_timeline",std::to_string(source["animations"].size()),"载入保存的节点和 Modifier 当前值；场景动画时间线尚未求值。选中对象的单帧预设由预设入口应用");
  if(scene.instances.empty()&&scene.lights.empty()&&scene.materials.empty()&&source.value("nodes",Json::array()).empty()) fail("文件没有可加载的场景内容");
  scene.validate();const auto bounds=scene.bounds();
  out.report={{"input",utf8(file)},{"mode","static-base-mesh-preview"},{"content_roots",Json::array()},
              {"dependencies",repo.dependencies},{"parsed_documents",repo.documents.size()},{"geometries",geometry_reports},{"materials",material_reports},
              {"instances",scene.instances.size()},{"textures",scene.textures.size()},{"warnings",warnings},{"fully_supported",warnings.empty()},
              {"bounds_m",{{"min",{bounds.minimum.x,bounds.minimum.y,bounds.minimum.z}},{"max",{bounds.maximum.x,bounds.maximum.y,bounds.maximum.z}}}},
              {"coordinate_conversion","DAZ centimeters Y-up to meters Z-up: (x,-z,y)/100"}};
  for(const auto &root:repo.roots) out.report["content_roots"].push_back(utf8(root));
  out.report["geometry_sources"]=Json::array();
  for(const auto &object:out.objects) for(const auto &source:object.geometry_sources)
    out.report["geometry_sources"].push_back({{"object",object.id},{"file",utf8(source.file)},{"geometry",source.id},{"topology_verified",true}});
  if(options.strict && !warnings.empty()) fail("严格模式拒绝未支持语义；请先用 --inspect 查看诊断");
  return out;
}
}
