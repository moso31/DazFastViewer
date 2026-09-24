#include "daz/loader.h"
#include "daz/subdivision.h"
#include "render_ir/options_json.h"
#include "render_ir/options.h"
#include "daz/documents.h"
#include "diagnostics/load_profile.h"
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
struct MissingAsset : std::runtime_error {using std::runtime_error::runtime_error;};
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
  diagnostics::Scope profile_scope("read_document");
  diagnostics::FileScope file_scope(diagnostics::active?utf8(path):std::string{});
  auto bytes=document_bytes(path);
  file_scope.bytes=fs::file_size(path);file_scope.unpacked_bytes=bytes.size();
  diagnostics::Scope parse_scope("json_parse");
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
    if(raw.starts_with("resources/")) for(const auto *installation:{L"DAZStudio4",L"DAZStudio6",L"DAZStudio4 Public Build",L"DAZStudio6 Public Build"})
      candidates.push_back(fs::path(L"C:/Program Files/DAZ 3D")/installation/L"shaders/iray"/relative);
    for(const auto &p:candidates) if(fs::is_regular_file(p)) {auto file=fs::weakly_canonical(p);dependencies.insert(utf8(file));return file;}
    throw MissingAsset("DSON: 依赖缺失: "+uri+"（来源 "+utf8(owner)+"）");
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
void add_channels(Channels &channels,const Json &material,const fs::path &owner) {
  auto merge=[&](Json &dst,const Json &src) {
    if(dst.is_null()) dst=Json::object();
    if(src.contains("image")||src.contains("image_file")) {dst.erase("image");dst.erase("image_file");dst["_dfv_owner"]=utf8(owner);}
    dst.update(src);
  };
  // DAZ Default/3Delight 把标准通道直接放在材质上，Iray 通道通常位于 extra。
  for(auto field=material.begin();field!=material.end();++field) if(field.value().is_object()&&field.value().contains("channel"))
    merge(channels[field.key()],field.value().at("channel"));
  for(const auto &extra:material.value("extra",Json::array())) for(const auto &entry:extra.value("channels",Json::array())) {
    if(!entry.contains("channel")) continue;
    const auto &c=entry["channel"];const auto id=c.at("id").get<std::string>();
    if(!channels.contains(id)) channels[id]=Json::object();merge(channels[id],c);
  }
}
}

Json read_document_file(const fs::path &file) {return read_document(file);}
std::string decode_uri(const std::string &uri) {return decode(uri);}
void sync_instance_meshes(ir::Scene &scene) {
  for(auto &instance:scene.instances) if(instance.prototype>=0) {
    const auto &source=scene.instances.at(size_t(instance.prototype));instance.mesh=source.mesh;instance.materials=source.materials;
  }
}
ir::Transform node_transform(const Json &n) {
  const auto pivot=axes(n,"center_point",{}),t=axes(n,"translation",{}),r=axes(n,"rotation",{}),s=axes(n,"scale",{1,1,1});
  const float general=number(n.value("general_scale",Json(1)),1);
  ir::Transform scale;scale.value[0]=s.x*general;scale.value[5]=s.y*general;scale.value[10]=s.z*general;
  const auto orientation=rotation(axes(n,"orientation",{}),"XYZ");
  return ir::Transform::translate({pivot.x+t.x,pivot.y+t.y,pivot.z+t.z})*orientation*rotation(r,n.value("rotation_order","XYZ"))*scale*transpose_rotation(orientation)*ir::Transform::translate({-pivot.x,-pivot.y,-pivot.z});
}
bool selection_reference(const std::string &uri) {
  const auto decoded=decode_uri(uri);
  for(const auto *prefix:{"name://@selection","id://@selection"}) if(decoded.starts_with(prefix)) {
    const auto suffix=decoded.substr(std::char_traits<char>::length(prefix));
    return suffix.empty()||suffix.starts_with(':')||suffix.starts_with('/')||suffix.starts_with('#');
  }
  return false;
}
void apply_graft_masks(LoadedScene &loaded,bool defer_selection) {
  auto &scene=loaded.scene;
  for(auto &mesh:scene.meshes) mesh.hidden_polygons.clear();
  for(auto &instance:scene.instances) instance.graft_source=-1;
  std::map<uint32_t,std::set<uint32_t>> masks;
  for(const auto &object:loaded.objects) {
    const auto &graft=scene.meshes.at(scene.instances.at(object.instance).mesh);
    if(!graft.graft_target_vertices||object.conform_target.empty()) continue;
    if(defer_selection&&selection_reference(object.conform_target)) continue;
    const AssetObject *host=nullptr;
    for(const auto &candidate:loaded.objects) if(object.conform_target=="#"+candidate.id) {
      const auto &mesh=scene.meshes.at(scene.instances.at(candidate.instance).mesh);
      if(mesh.positions.size()==graft.graft_target_vertices&&(!graft.graft_target_polygons||mesh.source_polygon_count==graft.graft_target_polygons)) {
        if(host) fail("GeoGraft 目标几何不唯一："+object.id);host=&candidate;
      }
    }
    if(!host) fail("GeoGraft 目标拓扑不匹配："+object.id);
    if(!graft.graft_vertex_pairs.empty()) scene.instances.at(object.instance).graft_source=int(host->instance);
    const auto count=scene.meshes.at(scene.instances.at(host->instance).mesh).source_polygon_count;
    for(auto polygon:graft.graft_hidden_polygons) {if(polygon>=count) fail("GeoGraft 遮盖面越界："+object.id);masks[host->instance].insert(polygon);}
  }
  // 相同资产的不同角色可以挂接不同插件，遮盖不得泄漏给另一个角色。
  std::map<std::pair<uint32_t,std::vector<uint32_t>>,uint32_t> variants;
  std::set<uint32_t> used;
  for(uint32_t i=0;i<scene.instances.size();++i) {
    auto &instance=scene.instances[i];if(instance.prototype>=0) continue;
    const auto &mask=masks[i];std::vector<uint32_t> hidden(mask.begin(),mask.end());const auto key=std::make_pair(instance.mesh,hidden);
    if(auto found=variants.find(key);found!=variants.end()) {instance.mesh=found->second;continue;}
    const auto original=instance.mesh;
    if(!used.insert(original).second) {auto mesh=scene.meshes.at(original);mesh.id+="/mask/"+instance.id;instance.mesh=uint32_t(scene.meshes.size());scene.meshes.push_back(std::move(mesh));}
    scene.meshes.at(instance.mesh).hidden_polygons=std::move(hidden);variants.emplace(key,instance.mesh);
  }
  sync_instance_meshes(scene);
}
DufContents inspect_contents(const Json &document) {
  DufContents result;const auto &scene=document.value("scene",Json::object());
  std::function<void(const Json &)> selection=[&](const Json &value) {
    if(result.requires_selection) return;
    if(value.is_object()) for(auto i=value.begin();i!=value.end();++i) {
      const auto &key=i.key();
      if(i->is_string()&&(key=="parent"||key=="conform_target"||key=="parent_in_place"||key=="node")) result.requires_selection|=selection_reference(i->get<std::string>());
      else if(key=="extra"||key=="channels"||key=="channel") selection(*i);
    } else if(value.is_array()) for(const auto &child:value) selection(child);
  };
  for(const auto *field:{"nodes","modifiers"}) if(auto i=scene.find(field);i!=scene.end()) selection(*i);
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
  auto deferred=[&](const std::string &uri) {return options.defer_selection&&selection_reference(uri);};
  Json warnings=Json::array(),material_reports=Json::array(),geometry_reports=Json::array(),subdivision_reports=Json::array();
  auto warn=[&](const std::string &code,const std::string &asset,const std::string &detail) {warnings.push_back({{"code",code},{"asset",asset},{"detail",detail}});};
  std::map<std::pair<std::string,ir::ColorSpace>,int> textures;
  auto texture=[&](const Json &channel,ir::ColorSpace colorspace,const fs::path &owner)->int {
    const auto image_uri=channel.contains("image")&&channel["image"].is_string()?channel["image"].get<std::string>():std::string{};
    if(!image_uri.empty()) {
      // 场景覆盖的局部引用属于 DUF，而不是原始材质 DSF。
      const auto image_owner=fs::u8path(channel.value("_dfv_owner",utf8(owner)));
      const auto [image_file,image]=repo.asset(image_uri,image_owner,"image_library");
      const auto key=std::make_pair(utf8(image_file)+"#"+image->at("id").get<std::string>()+":"+image->dump(),colorspace);
      if(auto found=textures.find(key);found!=textures.end()) return found->second;
      ir::Texture result;result.id=key.first;result.colorspace=colorspace;result.gamma=image->value("map_gamma",0.f);
      for(const auto &map:image->value("map",Json::array())) {
        if(!map.value("active",true)) continue;
        ir::ImageLayer layer;const auto uri=map.value("url","");
        if(!uri.empty()) try {layer.file=repo.path(uri,image_file);} catch(const MissingAsset &e) {warn("texture_missing",uri,e.what());continue;}
        layer.operation=map.value("operation","alpha_blend");
        if(layer.operation!="alpha_blend"&&layer.operation!="multiply"&&layer.operation!="add"&&layer.operation!="subtract") {
          warn("image_layer_operation",image_uri,"暂未映射图层运算："+layer.operation);layer.operation="alpha_blend";
        }
        layer.opacity=std::clamp(map.value("transparency",1.f),0.f,1.f);layer.invert=map.value("invert",false);
        layer.rotation=map.value("rotation",0.f);layer.scale={map.value("xscale",1.f),map.value("yscale",1.f)};
        layer.offset={map.value("xoffset",0.f),map.value("yoffset",0.f)};layer.mirror_x=map.value("xmirror",false);layer.mirror_y=map.value("ymirror",false);
        const auto c=map.value("color",Json::array({0,0,0}));layer.color={c.at(0),c.at(1),c.at(2)};
        if(map.contains("mask")) warn("image_layer_mask",image_uri,"图层蒙版暂未映射");
        result.layers.push_back(std::move(layer));
      }
      if(result.layers.empty()) return -1;
      result.file=result.layers.front().file;
      const int index=int(scene.textures.size());scene.textures.push_back(std::move(result));textures.emplace(key,index);return index;
    }
    const auto uri=channel.contains("image_file")&&channel["image_file"].is_string()?channel["image_file"].get<std::string>():std::string{};if(uri.empty()) return -1;
    fs::path path;
    try {path=repo.path(uri,owner);}
    catch(const MissingAsset &e) {warn("texture_missing",uri,std::string(e.what())+"；使用材质通道常量继续加载");return -1;}
    const auto key=std::make_pair(utf8(path),colorspace);
    if(auto found=textures.find(key);found!=textures.end()) return found->second;
    const int index=int(scene.textures.size());scene.textures.push_back({key.first,path,colorspace});textures.emplace(key,index);return index;
  };
  struct Binding {uint32_t material;std::string uv;};
  std::map<std::pair<std::string,std::string>,Binding> bindings;
  for(const auto &instance:source.value("materials",Json::array())) {
    Channels channels;fs::path material_file=file;
    if(instance.contains("url")) {
      try {auto [base_file,base]=repo.asset(instance.at("url"),file,"material_library");material_file=base_file;add_channels(channels,*base,base_file);}
      catch(const MissingAsset &e) {warn("material_asset_missing",instance.at("url"),std::string(e.what())+"；保留场景内的材质参数");}
    }
    add_channels(channels,instance,file);
    const bool legacy_gloss=channels.contains("glossiness")&&!channels.contains("Glossy Roughness")&&!channels.contains("Base Mixing");
    for(const auto &[legacy,modern]:std::map<std::string,std::string>{
      {"Diffuse Color","diffuse"},{"base_roughness","Glossy Roughness"},
      {"transparency","Cutout Opacity"},{"glossiness","Glossiness"},{"specular","Glossy Color"},{"specular_strength","Glossy Layered Weight"},
      {"refraction","Refraction Color"},{"refraction_strength","Refraction Weight"},{"ior","Refraction Index"},
      {"bump","Bump Strength"},{"bump_min","Bump Minimum"},{"bump_max","Bump Maximum"},{"normal","Normal Map"},
      {"displacement","Displacement Strength"},{"displacement_min","Minimum Displacement"},{"displacement_max","Maximum Displacement"},
      {"Displacement Minimum","Minimum Displacement"},{"Displacement Maximum","Maximum Displacement"},
      {"u_scale","Horizontal Tiles"},{"v_scale","Vertical Tiles"},{"u_offset","Horizontal Offset"},{"v_offset","Vertical Offset"}}) {
      if(auto old=channels.find(legacy);old!=channels.end()) {if(!channels.contains(modern)) channels[modern]=old->second;channels.erase(old);}
    }
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
    material.bump_from_texel_density=!explicit_bump_range;
    if(explicit_bump_range) {const auto range=.01f*(number(channel("Bump Maximum"),0)-number(channel("Bump Minimum"),0));material.bump_invert=range<0;material.bump_distance=std::abs(range);}
    material.color_texture=texture(diffuse,ir::ColorSpace::srgb,material_file);
    material.roughness_texture=texture(channel("Glossy Roughness"),ir::ColorSpace::linear,material_file);
    material.opacity_texture=texture(channel("Cutout Opacity"),ir::ColorSpace::linear,material_file);
    material.normal_texture=texture(channel("Normal Map"),ir::ColorSpace::linear,material_file);
    material.bump_texture=texture(channel("Bump Strength"),ir::ColorSpace::linear,material_file);
    material.displacement_texture=texture(channel("Displacement Strength"),ir::ColorSpace::linear,material_file);
    material.displacement_strength=scalar("Displacement Active",1)!=0?scalar("Displacement Strength",0):0;
    material.displacement_min=.01f*scalar("Minimum Displacement",-.1f);
    material.displacement_max=.01f*scalar("Maximum Displacement",.1f);
    material.thin_walled=scalar("Thin Walled",0)!=0;
    material.roughness_from_glossiness=legacy_gloss||int(scalar("Base Mixing",0))==1;
    if(material.roughness_from_glossiness) {
      material.roughness=unit("Glossiness",.5f);
      material.roughness_texture=texture(channel("Glossiness"),ir::ColorSpace::linear,material_file);
    }
    const auto glossy=int(scalar("Base Mixing",0))==2?"Glossy Weight":"Glossy Layered Weight";
    material.specular=unit(glossy,1)*unit("Glossy Reflectivity",.5f);
    material.weighted_glossy=int(scalar("Base Mixing",0))==2;
    if(material.weighted_glossy) {const float weight=unit("Glossy Weight");material.specular=weight/std::max(1e-6f,weight+unit("Diffuse Weight",1));}
    material.specular_color=color_value("Glossy Color",{1,1,1});
    material.specular_color_texture=texture(channel("Glossy Color"),ir::ColorSpace::srgb,material_file);
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
      const bool chromatic=int(scalar("SSS Mode",0))==1;
      const float amount=std::max(0.f,scalar("SSS Amount",0));
      auto transport=[&](float t,float s) {
        const float absorption=-std::log(std::clamp(t,.0001f,1.f))/td;
        const float scattering=chromatic?-std::log(std::clamp(s,.0001f,1.f))/sd:amount/sd;
        const float extinction=std::max(1e-6f,absorption+scattering);
        return std::pair{std::clamp(1.f/extinction,1e-6f,.02f),std::clamp(scattering/extinction,0.f,1.f)};
      };
      const auto r=transport(transmitted.x,scatter.x),g=transport(transmitted.y,scatter.y),b=transport(transmitted.z,scatter.z);
      material.subsurface_radius={r.first,g.first,b.first};material.subsurface_color={r.second,g.second,b.second};
      // Mono 使用 SSS Amount；Chromatic 使用 SSS Color 的对数衰减。不能混用隐藏通道。
      material.separate_subsurface_color=true;
      if(int(scalar("Base Color Effect",0))==1) {
        const auto tint=color_value("SSS Reflectance Tint",{1,1,1});
        material.subsurface_color.x*=tint.x;material.subsurface_color.y*=tint.y;material.subsurface_color.z*=tint.z;
      }
      if(material.subsurface>0) warn("sss_transport_approximation",material.id,"按 Mono/Chromatic 散射系数、吸收颜色和厘米测量距离构建独立透光颜色及米制半径；Cycles Random Walk 与 Iray 体积散射仍不等价");
    }
    material.coat=unit("Top Coat Weight");material.coat_roughness=unit("Top Coat Roughness",.1f);material.coat_ior=std::max(1.f,scalar("Top Coat IOR",1.5f));
    material.coat_color=color_value("Top Coat Color",{1,1,1});
    material.coat_color_texture=texture(channel("Top Coat Color"),ir::ColorSpace::srgb,material_file);
    material.coat_mode=int(scalar("Top Coat Layering Mode",2));
    material.coat_normal=material.coat_mode==0?.08f*unit("Reflectivity",.5f):unit("Top Coat Curve Normal",.04f);
    material.coat_grazing=unit("Top Coat Curve Grazing",1);material.coat_exponent=std::max(.001f,scalar("Top Coat Curve Exponent",5));
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
      warn("bump_distance_approximation",material.id,"资产未提供凹凸高度范围；后端按材质世界面积、UV 面积和纹理分辨率计算两个纹素的高度范围；无法读取尺寸时回退 1 毫米");
    const std::set<std::string> supported={"diffuse","Glossy Roughness","Metallic Weight","Cutout Opacity","Refraction Weight","Refraction Index","Normal Map","Bump Strength","Bump Minimum","Bump Maximum",
      "Displacement Strength","Displacement Active","Minimum Displacement","Maximum Displacement",
      "Thin Walled","Base Mixing","Glossiness","Glossy Layered Weight","Glossy Weight","Glossy Reflectivity","Glossy Color","Glossy Anisotropy","Glossy Anisotropy Rotations",
      "Translucency Weight","Translucency Color","SSS Direction","Transmitted Color","SSS Color","SSS Mode","SSS Amount","Base Color Effect","SSS Reflectance Tint","Transmitted Measurement Distance","Scattering Measurement Distance",
      "Top Coat Weight","Top Coat Roughness","Top Coat IOR","Top Coat Color","Dual Lobe Specular Weight","Dual Lobe Specular Ratio","Dual Lobe Specular Reflectivity","Specular Lobe 1 Roughness","Specular Lobe 2 Roughness",
      "Horizontal Tiles","Vertical Tiles","Horizontal Offset","Vertical Offset","Share Glossy Inputs","Refraction Color","Refraction Roughness","Line Start Width","Line End Width",
      "Hair Root Color","Hair Tip Color","Root Transmission Color","Tip Transmission Color","Roughness","Azimuthal Roughness","Melanin","Melanin Redness"};
    for(const char *id:{"Refraction Index","Specular Lobe 1 Roughness","Specular Lobe 2 Roughness","Hair Tip Color"})
      if(!channel(id).value("image_file","").empty()) warn("unmapped_channel_texture",material.id,std::string(id)+" 目前仅支持常量，贴图尚未映射");
    Json unmapped=Json::array();for(const auto &[id,c]:channels) if(!supported.contains(id)) unmapped.push_back(id);
    if(!unmapped.empty()) warn("material_subset",material.id,"仅映射基础 PBR 参数；未映射通道详见 materials.unmapped_channels");
    material_reports.push_back({{"id",material.id},{"groups",instance.value("groups",Json::array())},{"unmapped_channels",unmapped},{"color_texture",material.color_texture},
      {"roughness_texture",material.roughness_texture},{"normal_texture",material.normal_texture},{"bump_texture",material.bump_texture},
      {"displacement_texture",material.displacement_texture},{"displacement_strength",material.displacement_strength},{"displacement_min_m",material.displacement_min},{"displacement_max_m",material.displacement_max},
      {"hair",material.hair},{"thin_walled",material.thin_walled},{"opacity",material.opacity},{"transmission",material.transmission},{"subsurface_weight",material.subsurface},{"translucency_weight",material.translucency},
      {"subsurface_radius_m",{material.subsurface_radius.x,material.subsurface_radius.y,material.subsurface_radius.z}},{"dual_lobe_weight",material.dual_weight},
      {"line_root_radius_m",material.hair_root_radius},{"line_tip_radius_m",material.hair_tip_radius},
      {"bump_distance_m",material.bump_distance},{"bump_distance_source",explicit_bump_range?"explicit-centimeter-range":"preview-approximation"}});
    const uint32_t index=uint32_t(scene.materials.size());scene.materials.push_back(material);
    const auto geometry=decode(instance.value("geometry",""));const auto uv=instance.value("uv_set","");
    for(const auto &group:instance.at("groups")) bindings[{geometry,group.get<std::string>()}]={index,uv};
  }
  std::map<std::string,Json> nodes;
  auto node_visible=[&](const Json &n) {bool visible=true;
    for(const auto &e:n.value("extra",Json::array())) for(const auto &c:e.value("channels",Json::array())) {
      const auto &v=c.at("channel");if(v.value("id","")=="Visible"||v.value("id","")=="Renderable") visible&=number(v,1)!=0;
    }return visible;
  };
  auto hierarchy_visible=[&](std::string id) {std::set<std::string> seen;
    while(nodes.contains(id)&&seen.insert(id).second) {
      const auto &n=nodes.at(id);if(!node_visible(n)) return false;
      const auto p=decode(n.value("parent",""));if(!p.starts_with('#')) break;id=p.substr(1);
    }return true;
  };
  for(const auto &instance:source.value("nodes",Json::array())) {
    Json base=Json::object();if(instance.contains("url")) base=*repo.asset(instance.at("url"),file,"node_library").second;
    nodes.emplace(instance.at("id").get<std::string>(),merge_node(base,instance));
  }
  // 无网格的选项节点仍属于场景状态，保留参数元数据。
  for(const auto &[id,node]:nodes) {
    bool environment=false,tone=false;
    for(const auto &e:node.value("extra",Json::array())) {environment|=e.value("type","")=="studio/node/environment";tone|=e.value("type","")=="studio/node/tone_mapper";}
    if(!environment&&!tone) continue;
    auto &options=environment?scene.options.environment:scene.options.tonemapper;options.id=id;options.label=node.value("label",id);
    const std::set<std::string> supported=environment?std::set<std::string>{"Environment Mode","Environment Intensity","Environment Map","Environment Tint","Draw Dome","Dome Orientation X","Dome Orientation Y","Dome Orientation Z","Dome Rotation","SS Latitude","SS Longitude","SS Day","SS Time","SS UTC Offset","SS Sun Disk Intensity","SS Physically Scaled Sun","SS Sun Disk Scale","SS Haze","SS Multiplier","SS RGB Unit Conversion"}:
      std::set<std::string>{"Tone Mapping Enable","Exposure Value","Shutter Speed","Aperture","Film ISO","cm2 Factor","Vignetting","White Point Scale","White Point","Burn Highlights Per Component","Burn Highlights","Crush Blacks","Saturation","Gamma"};
    for(const auto &e:node.value("extra",Json::array())) for(const auto &entry:e.value("channels",Json::array())) {
      const auto &c=entry.at("channel");ir::Option p;p.id=c.at("id");p.label=c.value("label",p.id);p.group=entry.value("group","");p.type=c.value("type","float");p.visible=c.value("visible",true);p.supported=supported.contains(p.id);p.image_uri=c.value("image_file","");
      const auto value=c.value("current_value",c.value("value",Json()));
      if(value.is_array()) {for(const auto &v:value) if(v.is_number()) p.value.push_back(v.get<double>());}
      else if(value.is_number()||value.is_boolean()) p.value.push_back(number(value,0));
      if(p.value.empty()) p.supported=false;
      p.minimum=c.value("min",-10000.0);p.maximum=c.value("max",10000.0);p.step=c.value("step_size",.01);
      if(!c.value("clamped",false)) {p.minimum=std::min(p.minimum,0.0);p.maximum=std::max(p.maximum,10000.0);}
      if(c.contains("enum_values")) {p.choices=c["enum_values"].get<std::vector<std::string>>();p.minimum=0;p.maximum=double(p.choices.size()-1);}
      if(p.type=="bool") {p.minimum=0;p.maximum=1;}
      if(p.id=="Gamma"||p.id=="Aperture"||p.id=="Shutter Speed"||p.id=="White Point Scale"||p.id=="White Point") p.minimum=.001;
      if(p.id=="Environment Map"&&!p.image_uri.empty()) {
        try {scene.options.environment_file=repo.path(p.image_uri,file);}
        catch(const std::exception &) {
          // DAZ 内置 resources URI 从安装目录只读解析。
          if(p.image_uri.starts_with("/resources/")) for(const auto *installation:{L"DAZStudio4",L"DAZStudio6",L"DAZStudio4 Public Build",L"DAZStudio6 Public Build"}) {
            const auto candidate=fs::path(L"C:/Program Files/DAZ 3D")/installation/L"shaders/iray"/fs::u8path(decode(p.image_uri.substr(1)));
            if(fs::is_regular_file(candidate)) {scene.options.environment_file=candidate;repo.dependencies.insert(utf8(candidate));break;}
          }
          if(scene.options.environment_file.empty()) warn("environment_map_missing",id,"未找到环境贴图："+p.image_uri);
        }
      }
      options.parameters.push_back(std::move(p));
    }
    if(environment&&int(ir::number(options,"Environment Mode",0))==2) warn("sun_sky_approximation",id,"Sun-Sky Only 使用 Cycles 多次散射天空与日期/经纬度太阳方向；保留场景灯光/贴图忽略语义。未等价复现 Iray 测光、Sun Node、穹顶倾斜、辉光、色调、地平线和地面参数。");
  }
  for(const auto &[id,node]:nodes) {
    bool group=false;for(const auto &extra:node.value("extra",Json::array())) {
      const auto type=extra.value("type","");group|=type=="studio/node/group_node"||type=="studio/node/group_instance";
    }
    out.nodes.push_back({id,decode(node.value("parent","")),node.value("label",node.value("name",id)),group});
  }
  std::map<std::string,runtime::RigidFollow> rigid_groups;
  for(const auto &[id,node]:nodes) for(const auto &e:node.value("extra",Json::array())) if(e.value("type","")=="studio/node/rigid_follow"&&e.contains("rigidity_group")) {
    const auto &group=e.at("rigidity_group");runtime::RigidFollow follow;follow.vertex_count=e.value("vertex_count",size_t(0));
    follow.vertices=values(group.at("reference_vertices")).get<std::vector<uint32_t>>();follow.rotate=group.value("rotation_mode","full")!="none";
    bool supported=group.value("rotation_mode","full")=="full"||!follow.rotate;
    for(const auto &mode:group.value("scale_modes",Json::array())) supported&=mode=="none";
    if(!supported) {warn("rigid_follow_mode",id,"刚性跟随的轴向缩放模式尚未实现");continue;}
    for(const auto &extra:node.value("extra",Json::array())) for(const auto &entry:extra.value("channels",Json::array())) {
      const auto &c=entry.at("channel");if(c.value("id","")=="Follow Target"&&c.contains("node")&&c["node"].is_string()) follow.target=decode(c["node"].get<std::string>());
    }
    if(follow.target.empty()) {auto parent=decode(node.value("parent",""));while(parent.starts_with('#')&&nodes.contains(parent.substr(1))) {const auto &p=nodes.at(parent.substr(1));if(p.contains("geometries")) {follow.target=parent;break;}parent=decode(p.value("parent",""));}}
    if(follow.vertices.empty()||(!deferred(follow.target)&&(!follow.target.starts_with('#')||!nodes.contains(follow.target.substr(1))))) fail("刚性跟随的目标或参考顶点缺失: "+id);
    rigid_groups[id]=std::move(follow);
  }
  std::map<std::string,ir::Transform> transforms;std::set<std::string> visiting;
  auto collapsed=[&](std::string id) {
    std::set<std::string> seen;
    while(!id.empty()&&nodes.contains(id)&&seen.insert(id).second) {
      const auto &n=nodes.at(id);const auto s=axes(n,"scale",{1,1,1});
      if(number(n.value("general_scale",Json(1)),1)==0||s.x==0||s.y==0||s.z==0) return true;
      const auto p=decode(n.value("parent",""));id=p.starts_with('#')?p.substr(1):std::string{};
    }return false;
  };
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
    if(!parent.empty()&&!deferred(parent)) {
      if(!parent.starts_with('#')) fail("不支持跨文件的场景实例 parent: "+parent);
      matrix=world(decode(parent.substr(1)))*matrix;
    }
    if(!n.value("inherits_scale",true)) warn("scale_compensation",id,"未实现 DAZ 父级缩放补偿；非单位父级缩放可能产生差异");
    visiting.erase(id);transforms.emplace(id,matrix);return matrix;
  };
  std::map<std::string,uint32_t> mesh_cache;
  std::map<std::string,ir::Transform> fitted;std::set<std::string> fitting;
  std::function<ir::Transform(const std::string &)> fitted_world=[&](const std::string &id) {
    if(fitted.contains(id)) return fitted.at(id);
    const auto original=world(id);if(!fitting.insert(id).second) fail("Fit To / parent 关系存在循环: "+id);
    const auto &node=nodes.at(id);auto result=original;
    const auto target=node.contains("conform_target")&&node["conform_target"].is_string()?decode(node["conform_target"].get<std::string>()):std::string{};
    if(rigid_groups.contains(id)&&deferred(rigid_groups.at(id).target)) {
      const auto parent=decode(node.value("parent",""));result=parent.starts_with('#')?ir::inverse(world(parent.substr(1)))*original:original;
    }
    else if(deferred(target)) result=ir::Transform{};
    else if(!target.empty()) {
      if(!target.starts_with('#')||!nodes.contains(target.substr(1))) fail("Fit To 目标不存在: "+target);
      // Fit To 替换 Figure 的世界变换；保存的穿戴前位移不能再叠加一次。
      result=fitted_world(target.substr(1));
    } else if(const auto parent=decode(node.value("parent",""));!parent.empty()&&!deferred(parent)) {
      // 刚性跟随由参考表面的最终顶点求姿态，不能再继承挂接骨骼的保存姿态。
      const auto owner=rigid_groups.contains(id)?rigid_groups.at(id).target.substr(1):parent.substr(1);
      // Follow Target 使用节点原点坐标；网格矩阵已减去中心，嵌套挂接需补回。
      const auto origin=rigid_groups.contains(id)?axes(nodes.at(owner),"center_point",{}):ir::Vec3{};
      result=fitted_world(owner)*ir::Transform::translate(origin)*ir::inverse(world(parent.substr(1)))*original;
    }
    fitting.erase(id);fitted[id]=result;return result;
  };
  for(const auto &instance:source.value("nodes",Json::array())) {
    const auto id=instance.at("id").get<std::string>();const auto &node=nodes.at(id);
    if(collapsed(id)) {if(instance.contains("geometries")) warn("collapsed_geometry",id,"零缩放对象不产生可绘制表面，跳过其几何");continue;}
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
      mesh.subdivision=subdivision_settings(g,geometry_instance);
      if(g.contains("graft")&&g["graft"].contains("vertex_count")) {
        mesh.graft_target_vertices=g["graft"]["vertex_count"].get<uint32_t>();
        mesh.graft_target_polygons=g["graft"].value("poly_count",0u);
        if(g["graft"].contains("vertex_pairs")) {
          std::set<uint32_t> sources;
          for(const auto &pair:values(g["graft"]["vertex_pairs"])) {
            if(!pair.is_array()||pair.size()!=2) fail("GeoGraft 顶点对格式无效");
            const auto a=pair[0].get<uint32_t>(),b=pair[1].get<uint32_t>();
            if(a>=values(g.at("vertices")).size()||b>=mesh.graft_target_vertices||!sources.insert(a).second) fail("GeoGraft 顶点对越界或重复");
            mesh.graft_vertex_pairs.push_back({a,b});
          }
        }
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
      const auto &subd=mesh.subdivision;key+='\n'+Json({subd.enabled,subd.level,subd.render_level,subd.algorithm,subd.edge_interpolation,subd.normal_smoothing}).dump();
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
        mesh.source_polygon_count=uint32_t(polygons.size());
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
          ir::Polygon face;face.material_slot=slot;face.polygon_group=polygon[0].get<uint32_t>();
          for(size_t c=2;c<polygon.size();++c) {
            const auto v=polygon[c].get<uint32_t>();const auto seam=uv.seams.find({uint32_t(p),v});const auto ui=seam==uv.seams.end()?v:seam->second;
            if(v>=mesh.positions.size()||ui>=uv.values.size()) fail("细分多边形或 UV 索引越界");face.vertices.push_back(v);face.uv.push_back(uv.values[ui]);
          }
          mesh.polygons.push_back(std::move(face));
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
        // 同源但不同 UV／细分设置的网格需要独立身份，供编辑快照和增量渲染匹配。
        if(std::any_of(scene.meshes.begin(),scene.meshes.end(),[&](const auto &old){return old.id==mesh.id;})) mesh.id+="/variant/"+id+"/"+geometry_id;
        mesh_index=uint32_t(scene.meshes.size());mesh_cache.emplace(key,mesh_index);scene.meshes.push_back(std::move(mesh));
      }
      ir::Instance render_instance;render_instance.id=id+"/"+geometry_id;render_instance.mesh=mesh_index;render_instance.materials=std::move(material_indices);render_instance.transform=render_transform(fitted_world(id));
      render_instance.visible=hierarchy_visible(id);
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
      const auto &subdivision=scene.meshes.at(mesh_index).subdivision;
      subdivision_reports.push_back({{"id",geometry_id},{"subdivision",{{"enabled",subdivision.enabled},{"level",subdivision.level},{"render_level",subdivision.render_level},{"algorithm",subdivision.algorithm},{"edge_interpolation",subdivision.edge_interpolation},{"normal_smoothing",subdivision.normal_smoothing}}}});
      if(subdivision.enabled&&subdivision.algorithm==3) warn("subdivision_legacy",geometry_id,"Legacy Catmull-Clark 当前由 OpenSubdiv Catmark 近似，未验证 DAZ 旧版逐点等价");
      if(subdivision.enabled&&subdivision.normal_smoothing!=0) warn("subdivision_normals",geometry_id,"已保留 Preserve Cage 设置；当前没有完整的 DAZ 基础网格分裂法线，使用网格平滑法线");
    }
  }
  // 普通实例和 UltraScatter 的打包实例均引用源对象的完整子树。
  // 只追加 Object/矩阵，几何和材质继续共享源对象最终求值结果。
  Json instance_reports=Json::array();
  std::map<std::string,std::vector<uint32_t>> own_instances;
  std::map<uint32_t,std::string> source_nodes;
  for(const auto &o:out.objects) {own_instances[o.id].push_back(o.instance);source_nodes[o.instance]=o.id;}
  std::map<std::string,std::vector<std::string>> children;
  for(const auto &[id,n]:nodes) {const auto p=decode(n.value("parent",""));if(p.starts_with('#')) children[p.substr(1)].push_back(id);}
  std::set<std::string> expanded,expanding;
  std::function<std::vector<uint32_t>(const std::string &)> expand=[&](const std::string &id) {
    if(!expanding.insert(id).second) fail("实例引用或层次形成循环："+id);
    const auto &n=nodes.at(id);
    if(!expanded.contains(id)&&!collapsed(id)) {
      bool is_instance=false;const Json *items=nullptr;
      for(const auto &e:n.value("extra",Json::array())) {
        is_instance|=e.value("type","")=="studio/node/instance"||e.value("type","")=="studio/node/group_instance";
      }
      // 使用文档中的稳定引用，不能指向 value() 产生的临时副本。
      for(const auto &e:array_member(n,"extra")) if(e.contains("instance_items")) items=&e.at("instance_items");
      std::string target;
      for(const auto &e:array_member(n,"extra")) for(const auto &entry:array_member(e,"channels")) {
        const auto &c=entry.at("channel");if(c.value("id","")=="Instance Target"&&c.contains("node")&&c["node"].is_string()) target=decode(c["node"].get<std::string>());
      }
      if(is_instance&&!target.empty()) {
        if(!target.starts_with('#')||!nodes.contains(target.substr(1))) warn("instance_target_missing",id,"实例目标不存在："+target);
        else {
          const auto target_id=target.substr(1);const auto originals=expand(target_id);
          std::map<uint32_t,bool> descendants_visible;
          for(auto index:originals) {
            bool visible=true;
            if(auto found=source_nodes.find(index);found!=source_nodes.end()) {
              auto child=found->second;std::set<std::string> visited;
              while(child!=target_id&&nodes.contains(child)&&visited.insert(child).second) {
                const auto &part=nodes.at(child);visible&=node_visible(part);
                const auto parent=decode(part.value("parent",""));if(!parent.starts_with('#')) break;child=parent.substr(1);
              }
            } else visible=scene.instances.at(index).visible;
            descendants_visible[index]=visible;
          }
          const auto inverse_target=ir::inverse(render_transform(fitted_world(target_id)));
          const auto target_center=axes(nodes.at(target_id),"center_point",{});
          auto emit=[&](const Json &item,const ir::Transform &placement,const std::string &name) {
            const auto s=axes(item,"scale",{1,1,1});if(s.x==0||s.y==0||s.z==0||number(item.value("general_scale",Json(1)),1)==0) return;
            const auto center=axes(item,"center_point",{});
            const auto offset=render_transform(ir::Transform::translate({center.x-target_center.x,center.y-target_center.y,center.z-target_center.z}));
            const auto matrix=render_transform(placement)*offset*inverse_target;
            for(auto source_index:originals) {
              auto copy=scene.instances.at(source_index);copy.transform=matrix*copy.transform;
              copy.id=id+"/"+name+"/"+copy.id;copy.instance_node=id;
              copy.instance_group=items?id+"/"+name:id;copy.instance_label=items?name:n.value("label",id);
              copy.prototype=copy.prototype>=0?copy.prototype:int(source_index);
              // 隐藏原型根节点仍可提供实例；原型内部被隐藏的零件保持隐藏。
              copy.visible=hierarchy_visible(id)&&node_visible(item)&&descendants_visible.at(source_index);
              own_instances[id].push_back(uint32_t(scene.instances.size()));scene.instances.push_back(std::move(copy));
            }
          };
          if(items) {size_t index=0;for(const auto &item:*items) emit(item,fitted_world(id)*node_transform(item),item.value("name",std::to_string(index++)));}
          else emit(n,fitted_world(id),"instance");
          instance_reports.push_back({{"node",id},{"target",target_id},{"placements",items?items->size():1},{"objects",own_instances[id].size()}});
        }
      }
      expanded.insert(id);
    }
    auto result=own_instances[id];
    for(const auto &child:children[id]) {auto descendants=expand(child);result.insert(result.end(),descendants.begin(),descendants.end());}
    expanding.erase(id);return result;
  };
  for(const auto &[id,n]:nodes) if(!expanded.contains(id)) expand(id);
  for(const auto &instance:source.value("modifiers",Json::array())) {
    if(instance.contains("channel")||instance.contains("skin")) continue;
    Json modifier=instance;
    if(instance.contains("url")) {
      const auto uri=instance.at("url").get<std::string>();const auto hash=uri.find('#');if(hash==std::string::npos) fail("Modifier 引用缺少 fragment");
      const auto handle=document_view(repo.path(uri,file));const auto id=decode(uri.substr(hash+1));const Json *base=nullptr;
      for(const auto &entry:array_member(*handle,"modifier_library")) if(entry.value("id","")==id) {base=&entry;break;}
      if(!base) fail("Modifier ID 不存在："+uri);
      Json settings=Json::object();if(base->contains("extra")) settings["extra"]=base->at("extra");modifier=merge_node(std::move(settings),instance);
    }
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
  apply_graft_masks(out,options.defer_selection);
  scene.validate();const auto bounds=scene.bounds();
  out.report={{"input",utf8(file)},{"mode","static-base-mesh-preview"},{"content_roots",Json::array()},
              {"dependencies",repo.dependencies},{"parsed_documents",repo.documents.size()},{"geometries",geometry_reports},{"subdivision",subdivision_reports},{"materials",material_reports},
              {"instances",scene.instances.size()},{"textures",scene.textures.size()},{"warnings",warnings},{"fully_supported",warnings.empty()},
              {"bounds_m",{{"min",{bounds.minimum.x,bounds.minimum.y,bounds.minimum.z}},{"max",{bounds.maximum.x,bounds.maximum.y,bounds.maximum.z}}}},
              {"coordinate_conversion","DAZ centimeters Y-up to meters Z-up: (x,-z,y)/100"}};
  out.report["render_options"]=ir::options_json(scene.options);
  out.report["instancing"]=std::move(instance_reports);
  out.report["graft_masks"]=Json::array();for(const auto &o:out.objects) {
    const auto &mesh=scene.meshes.at(scene.instances.at(o.instance).mesh);
    if(!mesh.hidden_polygons.empty()) out.report["graft_masks"].push_back({{"object",o.id},{"hidden_polygons",mesh.hidden_polygons},{"hidden_triangles",std::count_if(mesh.triangles.begin(),mesh.triangles.end(),[&](const auto &t){return !mesh.draws(t);})}});
  }
  for(const auto &root:repo.roots) out.report["content_roots"].push_back(utf8(root));
  for(auto &object:out.objects) {
    object.source_file=file;object.source_node=object.id;
    const auto presentation=nodes.at(object.id).value("presentation",Json::object());
    object.content_type=presentation.value("type","");object.preferred_base=presentation.value("preferred_base","");
    object.auto_fit_base=presentation.value("auto_fit_base","");object.extended_bases=presentation.value("extended_bases",std::vector<std::string>{});
    const auto &n=nodes.at(object.id);object.translation_cm=axes(n,"translation",{});object.rotation_degrees=axes(n,"rotation",{});object.scale=axes(n,"scale",{1,1,1});object.general_scale=number(n.value("general_scale",Json(1)),1);
    const auto pivot=axes(n,"center_point",{}),t=object.translation_cm;
    const auto orientation=rotation(axes(n,"orientation",{}),"XYZ");object.rotation_order=n.value("rotation_order","XYZ");
    const auto world_rotation=rotation(object.rotation_degrees,object.rotation_order);
    ir::Transform scale;scale.value[0]=object.scale.x*object.general_scale;scale.value[5]=object.scale.y*object.general_scale;scale.value[10]=object.scale.z*object.general_scale;
    const auto local=ir::Transform::translate({pivot.x+t.x,pivot.y+t.y,pivot.z+t.z})*orientation*world_rotation*scale*transpose_rotation(orientation)*ir::Transform::translate({-pivot.x,-pivot.y,-pivot.z});
    const auto parent=fitted_world(object.id)*ir::inverse(local);
    object.translation_frame=render_transform(parent);
    object.edit_frame=render_transform(parent*ir::Transform::translate({pivot.x+t.x,pivot.y+t.y,pivot.z+t.z})*orientation);
    for(auto ancestor=object.id;!ancestor.empty();) {
      if(rigid_groups.contains(ancestor)) {object.rigid_follow=rigid_groups.at(ancestor);break;}
      if(ancestor!=object.id&&nodes.at(ancestor).contains("geometries")) break;
      const auto ref=decode(nodes.at(ancestor).value("parent",""));ancestor=ref.starts_with('#')?ref.substr(1):std::string{};
    }
    object.geometry_versions.push_back({object.geometry_file,file_version(object.geometry_file)});
    for(const auto &source:object.geometry_sources) object.geometry_versions.push_back({source.file,file_version(source.file)});
  }
  out.report["geometry_sources"]=Json::array();
  for(const auto &object:out.objects) for(const auto &source:object.geometry_sources)
    out.report["geometry_sources"].push_back({{"object",object.id},{"file",utf8(source.file)},{"geometry",source.id},{"topology_verified",true}});
  if(options.strict && !warnings.empty()) fail("严格模式拒绝未支持语义；请先用 --inspect 查看诊断");
  return out;
}
}
