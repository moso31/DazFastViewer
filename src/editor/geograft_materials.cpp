#include "editor/geograft_materials.h"
#include "daz/content_entry.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace dfv::editor {
namespace {
namespace fs=std::filesystem;using J=nlohmann::json;
const fs::path product="Runtime/Textures/3feetwolf/HDNipplesforG8F-2.0";
const fs::path resources="data/3feetwolf/HD Nipples for G8F - 2.0/Resources";
const fs::path hd_uv="data/3feetwolf/HD Nipples for G8F - 2.0/HD Nipples - 2.0/UV Sets/3feetwolf/Base/HD UVs.dsf";
std::string utf8(const fs::path &p){auto s=p.generic_u8string();return {s.begin(),s.end()};}
void require(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);}
struct Context {size_t host,instance,body;};
Context context(const Document &d,size_t selected){
  const auto host=attachment_host(d,selected);const auto instance=d.catalog.targets.at(host).instance;
  bool compatible=false;for(const auto &o:d.loaded.objects)if(o.instance==instance){const auto base=o.auto_fit_base;compatible=base=="/Genesis 8/Female"||base=="/Genesis 8.1/Female";}
  require(compatible,"请选择 Genesis 8 / 8.1 Female 或其已绑定附件");
  const auto &slots=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(instance).mesh).material_slots;
  for(const auto *name:{"Body","Torso"}){auto i=std::find(slots.begin(),slots.end(),name);if(i!=slots.end())return {host,instance,size_t(i-slots.begin())};}
  throw std::runtime_error("角色缺少 Body / Torso 皮肤表面");
}
bool owned(const Document &d,size_t instance,size_t host){
  for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].instance==instance)try{return attachment_host(d,t)==host;}catch(const std::exception &){}
  const auto &i=d.loaded.scene.instances.at(instance);if(i.shell_root>=0&&size_t(i.shell_root)!=instance)return owned(d,size_t(i.shell_root),host);
  return false;
}
bool product_instance(const Document &d,size_t instance,const std::string &part){
  for(const auto &o:d.loaded.objects)if(o.instance==instance&&daz::entry_key(o.geometry_file).find(part)!=std::string::npos)return true;
  const auto &i=d.loaded.scene.instances.at(instance);if(i.shell_root>=0&&size_t(i.shell_root)!=instance)return product_instance(d,size_t(i.shell_root),part);
  return false;
}
J copy(const Document &d,const Snapshot &s,MaterialSurface surface){return copy_material(d.loaded.scene,s.material_overrides,surface);}
void install(Document &d,Snapshot &s,J value,MaterialSurface destination){
  // UV 独立设置；跨拓扑的皮肤参数复制不尝试套用宿主的顶点 UV 索引。
  const auto &i=d.loaded.scene.instances.at(destination.instance);
  const auto baseline=daz::material_uv_baseline(d.loaded.scene.materials.at(i.materials.at(destination.slot)));
  auto definition=value.at("source_definition").get<std::string>();auto json=definition.empty()?J::object():J::parse(definition);json.erase("uv_baseline");
  if(baseline)json["uv_baseline"]={{"uri",baseline->uri},{"owner",baseline->owner},{"label",baseline->label}};
  value["source_definition"]=json.dump();
  value.erase("uv_topology");paste_material(d,s,value,{destination});
}
void standard_uv(const J &material){
  if(material.at("source_definition").get<std::string>().empty())return;
  const auto def=J::parse(material.at("source_definition").get<std::string>());
  const auto uri=daz::entry_key(fs::u8path(daz::decode_uri(def.value("uv_set",std::string{}))));
  require(uri.empty()||uri.ends_with("#base female")||uri.ends_with("#base 8.1 female"),"此角色使用自定义 UV；当前产品适配仅支持 Genesis 8 / 8.1 Female 的基础 UV");
}
std::vector<MaterialSurface> hd_surfaces(const Document &d,const Context &c,std::optional<size_t> only_instance={}){
  std::vector<MaterialSurface> result;
  for(size_t i=0;i<d.loaded.scene.instances.size();++i)if((!only_instance||i==*only_instance)&&owned(d,i,c.host)&&product_instance(d,i,"/3feetwolf/hd nipples for g8f - 2.0/")){
    const auto &instance=d.loaded.scene.instances[i];if(instance.shell_source>=0)continue;
    const auto &slots=d.loaded.scene.meshes.at(instance.mesh).material_slots;
    for(size_t n=0;n<slots.size();++n)if(slots[n]=="HD Nipples L"||slots[n]=="HD Nipples R")result.push_back({i,n});
  }
  require(!result.empty()&&result.size()%2==0,"当前角色没有已绑定的 HD Nipples - 2.0，请先加载该附件");return result;
}
const std::array<std::string,7> map_names={"D","SSS","SSS_Gray","S","B","NM","Disp"};
const std::array<std::string,7> map_kinds={"diffuse","SSS","SSS","specular","bump","normal","displacement"};
fs::path map_file(const fs::path &folder,size_t type,char side){return folder/("NipplesMap"+map_names.at(type)+"_"+side+".jpg");}
bool complete(const fs::path &folder){for(size_t t=0;t<map_names.size();++t)for(char side:{'L','R'}){std::error_code e;const auto p=map_file(folder,t,side);if(!fs::is_regular_file(p,e)||fs::file_size(p,e)==0)return false;}return true;}
bool complete_channels(const fs::path &folder,const J &identity){
  if(!complete(folder))return false;
  for(const auto &p:material_parameters())if(p.kind==MaterialParameter::texture&&!hd_primary_channel(p.id)&&!identity.at("body").at(p.id).is_null())for(char side:{'L','R'}){
    std::error_code error;const auto file=folder/("Channel_"+p.id+"_"+side+".png");if(!fs::is_regular_file(file,error)||fs::file_size(file,error)==0)return false;
  }
  return true;
}
void write_lines(const fs::path &p,const std::vector<std::string> &lines){std::ofstream out(p,std::ios::binary);require(bool(out),"无法写入烘焙任务："+utf8(p));for(const auto &line:lines){require(line.find_first_of("\r\n,")==std::string::npos||p.filename()=="8.DAT"||p.filename()=="9.DAT"||p.filename()=="11.DAT","烘焙文件路径包含不支持的字符");out<<line<<'\n';}require(bool(out),"写入烘焙任务失败");}
std::vector<std::string> lines(const fs::path &p){std::ifstream in(p);require(bool(in),"产品文件缺失："+utf8(p));std::vector<std::string> r;std::string s;while(std::getline(in,s)){if(!s.empty()&&s.back()=='\r')s.pop_back();if(!s.empty())r.push_back(s);}return r;}
void bake_coordinates(const fs::path &r,const fs::path &out){
  // 1/2 是产品原始目标/源 UV，3/4 是四边形，5a/5b 是左右面索引。
  for(int dataset=1;dataset<=2;++dataset){std::vector<std::array<double,2>> uv;for(auto line:lines(r/(std::to_string(dataset)+".DAT"))){if(line.back()==',')line.pop_back();uv.push_back(J::parse(line).get<std::array<double,2>>());}
    const auto faces=lines(r/(std::to_string(dataset+2)+".DAT"));
    for(char side:{'a','b'}){std::ofstream file(out/(std::to_string(dataset==1?6:7)+side+".DAT"));file<<std::setprecision(17);
      for(const auto &index:lines(r/(std::string("5")+side+".DAT"))){auto face=faces.at(size_t(std::stoul(index)));std::replace(face.begin(),face.end(),',',' ');std::istringstream stream(face);size_t vertex;int count=0;while(stream>>vertex){const auto p=uv.at(vertex);file<<(p[0]-std::floor(p[0]))*4096<<','<<p[1]*4096<<'\n';++count;}require(count==4,"产品 UV 数据不是四边形");}require(bool(file),"无法写入烘焙 UV 数据");}
  }
}
}
fs::path hd_product_root(const std::vector<fs::path> &roots){for(const auto &r:roots)if(fs::is_regular_file(r/hd_uv)&&fs::is_directory(r/product))return r;throw std::runtime_error("找不到完整的 HD Nipples - 2.0 产品资源，请检查内容库路径");}
bool hd_primary_channel(const std::string &id){return id=="color_texture"||id=="translucency_color_texture"||id=="specular_texture"||id=="bump_texture"||id=="normal_texture"||id=="displacement_texture";}
J hd_texture_identity(const Document &d,const Snapshot &s,size_t selected){const auto c=context(d,selected);auto m=copy(d,s,{c.instance,c.body});standard_uv(m);J result={{"body",m.at("parameters")}};return result;}
std::vector<fs::path> find_hd_textures(const Document &d,const Snapshot &s,size_t selected,const std::vector<fs::path> &roots,const fs::path &cache){
  const auto identity=hd_texture_identity(d,s,selected);std::vector<fs::path> result;std::error_code error;
  if(fs::is_directory(cache,error))for(const auto &item:fs::directory_iterator(cache))if(item.is_directory()&&fs::is_regular_file(item.path()/"dfv-hd-textures.json"))try{std::ifstream in(item.path()/"dfv-hd-textures.json");J j;in>>j;if(j.at("identity")==identity&&complete_channels(item.path(),identity))result.push_back(item.path());}catch(const std::exception &){}
  std::sort(result.rbegin(),result.rend());if(!result.empty())return result;
  const auto &texture=identity.at("body").at("color_texture");if(texture.is_null())return result;
  const auto file=fs::u8path(texture.at("file").get<std::string>()).filename();
  const auto c=context(d,selected);bool eight_one=false;for(const auto &o:d.loaded.objects)if(o.instance==c.instance)eight_one=o.auto_fit_base=="/Genesis 8.1/Female";
  for(const auto &r:roots){auto p=r/product/"Textures"/(eight_one?"Genesis 8.1 Female":"Genesis 8 Female")/file;if(complete(p))result.push_back(p);}return result;
}
size_t copy_golden_palace_materials(Document &d,Snapshot &s,size_t selected,std::optional<size_t> only_instance){
  const auto c=context(d,selected);const auto body=copy(d,s,{c.instance,c.body});standard_uv(body);std::vector<MaterialSurface> graft,shell;
  for(size_t i=0;i<d.loaded.scene.instances.size();++i)if(owned(d,i,c.host)&&product_instance(d,i,"/meipex/goldenpalace/")){
    const auto &instance=d.loaded.scene.instances[i];const auto &slots=d.loaded.scene.meshes.at(instance.mesh).material_slots;
    if(only_instance&&i!=*only_instance&&instance.shell_source!=int(*only_instance))continue;
    for(size_t n=0;n<slots.size();++n)if(slots[n].starts_with("GP_")||slots[n].find("_GP_")!=std::string::npos)(instance.shell_source>=0?shell:graft).push_back({i,n});
  }
  require(!graft.empty(),"当前角色没有已绑定的 GoldenPalace v2，请先加载该附件");
  for(auto surface:graft)install(d,s,body,surface);
  for(auto surface:shell){auto material=copy(d,s,surface);for(const auto *id:{"base_color","subsurface","subsurface_color","subsurface_radius","subsurface_anisotropy","separate_subsurface_color","ior"})if(body["parameters"].contains(id))material["parameters"][id]=body["parameters"][id];install(d,s,material,surface);}
  return graft.size()+shell.size();
}
size_t apply_hd_textures(Document &d,Snapshot &s,size_t selected,const fs::path &directory,const std::vector<fs::path> &roots,std::optional<size_t> only_instance){
  const auto c=context(d,selected);const auto surfaces=hd_surfaces(d,c,only_instance);const auto body=copy(d,s,{c.instance,c.body});standard_uv(body);require(complete(directory),"HD 纹理不完整：需要左右各 7 张贴图");
  const auto root=hd_product_root(roots);const auto file=root/hd_uv;const daz::MaterialUVSet uv{utf8(file)+"#HD%20UVs",utf8(file),"HD UVs"};
  const auto base_file=file.parent_path()/"default.dsf";const daz::MaterialUVSet base_uv{utf8(base_file)+"#default",utf8(base_file),"Default UVs"};
  // 外围使用整身皮肤与基础 UV；左右局部随后切换为 HD UV。
  std::set<size_t> instances;for(auto surface:surfaces)instances.insert(surface.instance);
  for(auto i:instances){const auto slots=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(i).mesh).material_slots;
    for(size_t n=0;n<slots.size();++n){
      if(slots[n]=="Inner"||slots[n]=="Breast"||slots[n]=="Arms")change_material_uv(d,base_uv,{{i,n}});
      if(slots[n]=="Arms"){const auto &host_slots=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(c.instance).mesh).material_slots;auto arm=std::find(host_slots.begin(),host_slots.end(),"Arms");if(arm!=host_slots.end())install(d,s,copy(d,s,{c.instance,size_t(arm-host_slots.begin())}),{i,n});}
      else if(slots[n]=="Inner"){
        // 产品原始 DUF 的 Inner 是隐藏内部网格，不能用 Body 的不透明度使其显露。
        auto inner=body;inner["parameters"]["opacity"]=0;inner["parameters"]["opacity_texture"]=nullptr;install(d,s,inner,{i,n});
      }else if(slots[n]=="Breast"||slots[n]=="HD Nipples L"||slots[n]=="HD Nipples R")install(d,s,body,{i,n});}
  }
  change_material_uv(d,uv,surfaces);
  for(auto surface:surfaces){auto m=copy(d,s,surface);auto &p=m["parameters"];const auto &slot=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(surface.instance).mesh).material_slots.at(surface.slot);const char side=slot.back();
    auto set=[&](const char *id,size_t type,int space){p[id]={{"file",utf8(map_file(directory,type,side))},{"colorspace",space},{"gamma",0},{"layers",J::array()}};};
    set("color_texture",0,int(ir::ColorSpace::srgb));set("translucency_color_texture",1,int(ir::ColorSpace::srgb));set("specular_texture",3,int(ir::ColorSpace::linear));set("bump_texture",4,int(ir::ColorSpace::linear));set("normal_texture",5,int(ir::ColorSpace::linear));set("displacement_texture",6,int(ir::ColorSpace::linear));
    // 其余贴图也必须转为局部 UV，不能沿用整张原皮或静默丢弃通道。
    for(const auto &parameter:material_parameters())if(parameter.kind==MaterialParameter::texture&&!hd_primary_channel(parameter.id)&&!p[parameter.id].is_null()){
      const auto extra=directory/("Channel_"+parameter.id+"_"+side+".png");require(fs::is_regular_file(extra),"缺少附加皮肤通道的 HD 贴图："+parameter.label);p[parameter.id]["file"]=utf8(extra);p[parameter.id]["layers"]=J::array();
    }
    install(d,s,m,surface);
  }
  return surfaces.size();
}
HDBakeJob prepare_hd_bake(const Document &d,const Snapshot &s,size_t selected,const std::vector<fs::path> &roots,const fs::path &directory,const HDBakeOptions &options,bool require_program){
  const auto c=context(d,selected);hd_surfaces(d,c);auto identity=hd_texture_identity(d,s,selected);const auto &params=identity.at("body");
  require(options.color>=1&&options.color<=3&&(options.size=="Small"||options.size=="Medium"||options.size=="Large"||options.size=="Huge")&&(options.glands=="NoGlands"||options.glands=="NotObviousGlands"||options.glands=="ObviousGlands"),"无效的 HD 纹理选项");
  require(params.at("u_scale")==1&&params.at("v_scale")==1&&params.at("u_offset")==0&&params.at("v_offset")==0,"烘焙暂不支持经过平铺或偏移的皮肤贴图");
  const auto root=hd_product_root(roots),r=root/resources;require(!require_program||fs::is_regular_file(r/"preprocessing.exe"),"产品缺少 preprocessing.exe 烘焙程序");
  require(utf8(root).find_first_of(",\r\n")==std::string::npos&&utf8(directory).find_first_of(",\r\n")==std::string::npos,"产品目录或烘焙目录不能包含逗号或换行");
  for(const auto &p:material_parameters())if(p.kind==MaterialParameter::texture&&!params.at(p.id).is_null())require(params.at(p.id).value("layers",J::array()).empty(),"烘焙暂不支持 LIE 图层，请先合成为普通皮肤贴图");
  require(!fs::exists(directory),"烘焙输出目录已存在，请使用新的目录");fs::create_directories(directory);
  HDBakeJob job{r/"preprocessing.exe",directory,{}, {},identity};const std::array<std::string,6> channels={"color_texture","translucency_color_texture","specular_texture","bump_texture","normal_texture","displacement_texture"};const std::array<std::string,6> fallback={"D","SSS","S","B","NM","Disp"};const std::array<std::string,6> kinds={"diffuse","SSS","specular","bump","normal","displacement"};std::vector<std::string> inputs,outputs,masks,templates;
  for(size_t i=0;i<channels.size();++i){auto p=root/product/"BaseColors"/("Base"+fallback[i]+".jpg");const auto &t=params.at(channels[i]);if(!t.is_null()){require(t.value("layers",J::array()).empty(),"烘焙暂不支持 LIE 图层，请先合成为普通皮肤贴图");p=fs::u8path(t.at("file").get<std::string>());}require(fs::is_regular_file(p),"烘焙源贴图不存在："+utf8(p));job.inputs.push_back(p);inputs.push_back(utf8(directory/("input-"+std::to_string(i)+".jpg"))+","+kinds[i]);}
  for(size_t t=0;t<map_names.size();++t)for(char side:{'L','R'}){outputs.push_back(utf8(map_file(directory,t,side))+","+map_kinds[t]);auto name="Default"+map_names[t];if(t==0)name+="_"+std::to_string(options.color);if(t==1)name="DefaultSSS_Color_"+std::to_string(options.color);auto p=root/product/"Templates"/options.glands/(name+"_"+side+".jpg");require(fs::is_regular_file(p),"产品缺少烘焙模板："+utf8(p));templates.push_back(utf8(p)+","+map_kinds[t]);}
  for(char side:{'L','R'}){auto p=root/product/"Masks"/(options.size+"Mask_"+side+".tif");require(fs::is_regular_file(p),"产品缺少乳晕遮罩");masks.push_back(utf8(p));}
  bake_coordinates(r,directory);write_lines(directory/"8.DAT",inputs);write_lines(directory/"9.DAT",outputs);write_lines(directory/"10.DAT",masks);write_lines(directory/"11.DAT",templates);
  for(const auto *n:{"7a","7b","6a","6b","8","9","10","11"})job.arguments.push_back(directory/(std::string(n)+".DAT"));return job;
}
void finish_hd_bake(const HDBakeJob &job){require(complete(job.directory),"烘焙未生成完整的 14 张 HD 贴图，请查看该任务的 process.log");for(const auto &p:material_parameters())if(p.kind==MaterialParameter::texture&&!hd_primary_channel(p.id)&&!job.identity.at("body").at(p.id).is_null())for(char side:{'L','R'})require(fs::is_regular_file(job.directory/("Channel_"+p.id+"_"+side+".png")),"附加皮肤通道尚未烘焙完成："+p.label);std::ofstream out(job.directory/"dfv-hd-textures.json");out<<J{{"identity",job.identity},{"version",2}}.dump(2);require(bool(out),"无法保存烘焙纹理索引");}
}
