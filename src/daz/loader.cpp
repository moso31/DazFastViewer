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
    if(it.value().is_array() && (it.key()=="rotation" || it.key()=="translation" || it.key()=="scale" || it.key()=="center_point" || it.key()=="orientation")) {
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
    const auto diffuse=channel("diffuse");const auto color=diffuse.value("current_value",diffuse.value("value",Json::array({1,1,1})));
    if(!color.is_array() || color.size()!=3) fail("材质 diffuse 必须为三通道颜色");
    material.base_color={color[0].get<float>(),color[1].get<float>(),color[2].get<float>()};
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
    if(material.bump_texture>=0 && !explicit_bump_range)
      warn("bump_distance_approximation",material.id,"资产未提供凹凸高度范围；暂用 1 毫米。参考 Importer 根据几何和纹理得到不同距离，尚未对齐");
    const std::set<std::string> supported={"diffuse","Glossy Roughness","Metallic Weight","Cutout Opacity","Refraction Weight","Refraction Index","Normal Map","Bump Strength","Bump Minimum","Bump Maximum"};
    for(const char *id:{"Metallic Weight","Refraction Weight","Refraction Index"})
      if(!channel(id).value("image_file","").empty()) warn("unmapped_channel_texture",material.id,std::string(id)+" 目前仅支持常量，贴图尚未映射");
    Json unmapped=Json::array();for(const auto &[id,c]:channels) if(!supported.contains(id)) unmapped.push_back(id);
    if(!unmapped.empty()) warn("material_subset",material.id,"仅映射基础 PBR 参数；未映射通道详见 materials.unmapped_channels");
    material_reports.push_back({{"id",material.id},{"unmapped_channels",unmapped},{"color_texture",material.color_texture},
      {"roughness_texture",material.roughness_texture},{"normal_texture",material.normal_texture},{"bump_texture",material.bump_texture},
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
  for(const auto &instance:source.value("nodes",Json::array())) {
    const auto id=instance.at("id").get<std::string>();const auto &node=nodes.at(id);
    if(node.value("type","")=="camera" || node.value("type","")=="light")
      warn("scene_view_settings",id,"尚未导入 DAZ 相机或灯光，使用自动取景与预览灯光");
    if(node.value("type","")=="bone") {
      const auto r=axes(node,"rotation",{}),t=axes(node,"translation",{});
      if(std::abs(r.x)+std::abs(r.y)+std::abs(r.z)+std::abs(t.x)+std::abs(t.y)+std::abs(t.z)>1e-6f) warn("bone_pose",id,"本阶段显示静态基础网格，未应用骨骼变形");
    }
    for(const auto &geometry_instance:instance.value("geometries",Json::array())) {
      const auto uri=geometry_instance.at("url").get<std::string>();const auto [geometry_file,gptr]=repo.asset(uri,file,"geometry_library");const auto &g=*gptr;
      ir::Mesh mesh;mesh.id=uri;
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
        for(size_t p=0;p<polygons.size();++p) {
          const auto &polygon=polygons[p];if(polygon.size()<5 || polygon.size()>6) fail("仅支持 DSON 三角形和四边形");
          const auto slot=polygon[1].get<uint32_t>();if(slot>=mesh.material_slots.size()) fail("多边形材质组索引越界");
          const auto &uv=uv_cache.at(uv_refs.at(slot));
          for(size_t k=3;k+1<polygon.size();++k) {
            ir::Triangle triangle;triangle.material_slot=slot;
            triangle.vertices={polygon[2].get<uint32_t>(),polygon[k].get<uint32_t>(),polygon[k+1].get<uint32_t>()};
            for(size_t c=0;c<3;++c) {
              const auto v=triangle.vertices[c];auto seam=uv.seams.find({uint32_t(p),v});const auto ui=seam==uv.seams.end()?v:seam->second;
              if(ui>=uv.values.size()) fail("UV 索引越界");triangle.uv[c]=uv.values[ui];
            }
            mesh.triangles.push_back(triangle);
          }
        }
        geometry_reports.push_back({{"id",geometry_id},{"vertices",mesh.positions.size()},{"polygons",polygons.size()},{"triangles",mesh.triangles.size()},{"material_groups",mesh.material_slots}});
        mesh_index=uint32_t(scene.meshes.size());mesh_cache.emplace(key,mesh_index);scene.meshes.push_back(std::move(mesh));
      }
      ir::Instance render_instance;render_instance.id=id+"/"+geometry_id;render_instance.mesh=mesh_index;render_instance.materials=std::move(material_indices);render_instance.transform=render_transform(world(id));
      out.objects.push_back({uint32_t(scene.instances.size()),id,node.value("label",node.value("name",id)),node.value("parent",""),g.at("id").get<std::string>(),geometry_file,node.value("type","")=="figure",geometry_id});
      scene.instances.push_back(std::move(render_instance));
      if(geometry_instance.value("type",g.value("type",""))=="subdivision_surface") warn("subdivision",geometry_id,"当前显示基础笼形网格；未应用 SubD/HD 细分");
    }
  }
  for(const auto &modifier:source.value("modifiers",Json::array())) {
    const auto [modifier_file,m]=repo.asset(modifier.at("url"),file,"modifier_library");
    if(m->contains("skin")) warn("skinning",modifier.value("id",""),"保留静态基础形态，尚未实现蒙皮/姿态求值");
    else warn("modifier",modifier.value("id",""),"未应用该 Modifier/HD Morph，基础网格预览与 DAZ 最终求值结果可能不同");
  }
  if(scene.instances.empty()) fail("场景没有可加载的几何实例");
  scene.validate();const auto bounds=scene.bounds();
  out.report={{"input",utf8(file)},{"mode","static-base-mesh-preview"},{"content_roots",Json::array()},
              {"dependencies",repo.dependencies},{"parsed_documents",repo.documents.size()},{"geometries",geometry_reports},{"materials",material_reports},
              {"instances",scene.instances.size()},{"textures",scene.textures.size()},{"warnings",warnings},{"fully_supported",warnings.empty()},
              {"bounds_m",{{"min",{bounds.minimum.x,bounds.minimum.y,bounds.minimum.z}},{"max",{bounds.maximum.x,bounds.maximum.y,bounds.maximum.z}}}},
              {"coordinate_conversion","DAZ centimeters Y-up to meters Z-up: (x,-z,y)/100"}};
  for(const auto &root:repo.roots) out.report["content_roots"].push_back(utf8(root));
  if(options.strict && !warnings.empty()) fail("严格模式拒绝未支持语义；请先用 --inspect 查看诊断");
  return out;
}
}
