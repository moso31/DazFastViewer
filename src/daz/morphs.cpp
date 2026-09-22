#include "daz/morphs.h"
#include "daz/documents.h"
#include <mutex>
#include "diagnostics/load_profile.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <set>

namespace dfv::daz {
namespace fs=std::filesystem;
using J=nlohmann::json;
static std::string path_string(const fs::path &p) {auto s=p.generic_u8string();return {s.begin(),s.end()};}
static std::string lower(std::string s) {for(char &c:s) if(c>='A'&&c<='Z') c+=32;return s;}
static thread_local std::map<fs::path,std::string> path_keys;
static thread_local std::map<std::pair<fs::path,std::string>,fs::path> resolved_paths;
static std::string key(const fs::path &p) {
  auto [it,inserted]=path_keys.try_emplace(p);if(inserted) it->second=lower(path_string(fs::weakly_canonical(p)));return it->second;
}
static float number(const J &j,const char *name,float value) {const auto i=j.find(name);if(i==j.end()) return value;return i->is_boolean()?(i->get<bool>()?1.f:0.f):i->is_number()?i->get<float>():value;}
struct Reference {std::string file,id,property;};
static Reference reference(std::string uri) {
  uri=decode_uri(uri);const auto hash=uri.find('#');
  if(hash==std::string::npos) throw std::runtime_error("参数引用缺少 fragment: "+uri);
  std::string file=uri.substr(0,hash);const auto colon=file.find(':');
  if(colon!=std::string::npos) file=file.substr(colon+1);
  const auto query=uri.find('?',hash);return {file,uri.substr(hash+1,query==std::string::npos?query:query-hash-1),query==std::string::npos?"":uri.substr(query+1)};
}
static fs::path resolve_uncached(const std::string &raw,const fs::path &owner,const std::vector<fs::path> &roots) {
  if(raw.empty()) return owner;
  auto relative=fs::u8path(raw.starts_with('/')?raw.substr(1):raw).lexically_normal();
  if(relative.is_absolute() || relative.has_root_name() || (!relative.empty() && *relative.begin()=="..")) return {};
  if(!raw.starts_with('/') && fs::is_regular_file(owner.parent_path()/relative)) return fs::weakly_canonical(owner.parent_path()/relative);
  for(const auto &root:roots) if(fs::is_regular_file(root/relative)) return fs::weakly_canonical(root/relative);
  return {};
}
static fs::path resolve(const std::string &raw,const fs::path &owner,const std::vector<fs::path> &roots) {
  if(raw.empty()) return owner;
  const auto cache_key=std::make_pair(raw.starts_with('/')?fs::path{}:owner.parent_path(),raw);
  auto [it,inserted]=resolved_paths.try_emplace(cache_key);if(inserted) it->second=resolve_uncached(raw,owner,roots);return it->second;
}
static fs::path relative_to_roots(const fs::path &path,const std::vector<fs::path> &roots) {
  for(const auto &root:roots) {const auto rel=path.lexically_relative(root);if(!rel.empty() && *rel.begin()!="..") return rel;}
  return {};
}
// 逻辑资产地址只在已验证属于当前角色的目录中建立索引，不作为磁盘绝对路径使用。
static std::string asset_uri(std::string uri) {
  uri=decode_uri(uri);
  if(!uri.starts_with('/')||uri.find_first_of(":#?\\")!=std::string::npos) return {};
  const auto path=fs::u8path(uri.substr(1)).lexically_normal();
  if(path.empty()||path.is_absolute()||path.has_root_name()||*path.begin()=="..") return {};
  return "/"+lower(path_string(path));
}
struct Asset {fs::path file;std::string geometry;std::set<std::string> nodes;std::vector<std::string> roots;std::set<std::string> geometry_nodes;};
static Asset asset(const fs::path &file,const std::string &geometry) {
  Asset out{file,geometry,{}};const auto handle=document_view(file);const auto &doc=*handle;
  for(const auto &n:array_member(doc,"node_library")) {out.nodes.insert(n.at("id").get<std::string>());if(n.value("type","")=="figure"||!n.contains("parent")) out.roots.push_back(n.at("id").get<std::string>());}
  // SkinBinding 同时给出角色和几何身份；只接受所属资产内明确关联的节点。
  std::map<std::string,std::set<std::string>> node_geometries;
  for(const auto &m:array_member(doc,"modifier_library")) if(m.contains("skin")) {
    const auto g=reference(m.at("skin").value("geometry","")),n=reference(m.at("skin").value("node",""));
    if(n.file.empty()&&out.nodes.contains(n.id)) node_geometries[n.id].insert(g.file+"#"+g.id);
  }
  for(const auto &[node,geometries]:node_geometries) if(geometries.size()==1&&geometries.contains("#"+geometry)) out.geometry_nodes.insert(node);
  return out;
}
static runtime::OffsetBuffer offsets(const J &source,size_t vertices) {
  const auto &deltas=source.at("deltas");const auto &rows=deltas.at("values");
  if(deltas.at("count").get<size_t>()!=rows.size()) throw std::runtime_error("Morph 差值 count 不一致");
  runtime::OffsetBuffer result;result.reserve(rows.size());std::vector<uint8_t> seen(vertices);
  for(const auto &row:rows) {
    if(row.size()!=4||!row[0].is_number_integer()||row[0].get<uint64_t>()>=vertices) throw std::runtime_error("Morph 差值索引无效");
    const auto vertex=row[0].get<uint32_t>();const float x=row[1].get<float>(),y=row[2].get<float>(),z=row[3].get<float>();
    if(seen[vertex]||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) throw std::runtime_error("Morph 差值重复或非有限");seen[vertex]=1;result.push_back({vertex,{x*.01f,-z*.01f,y*.01f}});
  }return result;
}
static std::shared_ptr<runtime::MorphPayload> payload(const fs::path &file,const std::string &id,size_t count,size_t vertices) {
  static std::mutex mutex;static std::map<std::string,std::weak_ptr<runtime::MorphPayload>> shared;
  const auto version=file_version(file),identity=key(file)+"#"+id+"|"+version+"|"+std::to_string(vertices);
  std::lock_guard lock(mutex);if(auto p=shared[identity].lock()) return p;
  auto p=std::make_shared<runtime::MorphPayload>(identity,count,vertices,[file,id,version,vertices] {
    if(file_version(file)!=version) throw std::runtime_error("资源已变化，请刷新参数目录："+path_string(file));
    const auto doc=read_document_file(file);runtime::OffsetBuffer result;bool found=false;
    for(const auto &m:array_member(doc,"modifier_library")) if(m.value("id","")==id) {if(found) throw std::runtime_error("重复的 Morph ID");result=offsets(m.at("morph"),vertices);found=true;}
    if(!found||file_version(file)!=version) throw std::runtime_error("Morph 缺失或读取期间资源已变化："+id);return result;
  });shared[identity]=p;
  if(shared.size()>16384) std::erase_if(shared,[](const auto &entry){return entry.second.expired();});return p;
}
MorphCatalog discover_morphs(LoadedScene &loaded,const std::vector<fs::path> &input_roots,const std::function<void(const std::string &)> &progress,bool lazy) {
  struct CacheScope {~CacheScope() {path_keys.clear();resolved_paths.clear();}} cache_scope;
  path_keys.clear();resolved_paths.clear();
  MorphCatalog out;out.report={{"targets",J::array()},{"diagnostics",J::array()},{"files_scanned",0},{"skipped_types",J::object()},
    {"content_roots",J::array()},{"file_overrides",J::array()},{"empty_overrides",J::array()},{"root_scans",J::array()}};
  std::vector<fs::path> roots;std::set<std::string> root_keys;
  for(const auto &root:input_roots) if(root_keys.insert(key(root)).second) {
    roots.push_back(fs::weakly_canonical(root));out.report["content_roots"].push_back({{"path",path_string(root)},{"exists",fs::is_directory(root)}});
  }
  auto skipped=[&](const std::string &type) {out.report["skipped_types"][type]=out.report["skipped_types"].value(type,0)+1;};
  fs::path scene_file;J scene_document=J::object();
  if(loaded.report.contains("input")) {scene_file=fs::u8path(loaded.report.at("input").get<std::string>());scene_document=*document_view(scene_file);}
  // 场景覆盖只解析一次；大场景不能为每个 Morph 复制和遍历整个 scene。
  std::map<std::pair<std::string,std::string>,J> overrides;
  if(scene_document.contains("scene")&&scene_document["scene"].contains("modifiers"))
    for(const auto &item:scene_document["scene"]["modifiers"]) {
      const auto uri=item.value("url","");if(uri.empty()) continue;
      const auto ref=reference(uri);const auto path=resolve(ref.file,scene_file,roots);if(path.empty()) continue;
      overrides[{decode_uri(item.value("parent","")),key(path)+"#"+ref.id}]=item.value("channel",J::object());
    }
  std::set<uint32_t> used_meshes;
  struct CachedTarget {runtime::Target target;FormulaSource formulas;J report;};
  std::map<std::string,CachedTarget> cached_targets;
  for(const auto &object:loaded.objects) {
    diagnostics::Scope object_scope(diagnostics::active?object.id:std::string{});
    if(progress) progress("正在发现参数："+object.label);
    auto &instance=loaded.scene.instances.at(object.instance);
    if(!used_meshes.insert(instance.mesh).second) {
      auto copy=loaded.scene.meshes.at(instance.mesh);copy.id+="/"+object.id;
      instance.mesh=uint32_t(loaded.scene.meshes.size());loaded.scene.meshes.push_back(std::move(copy));
    }
    const auto &mesh=loaded.scene.meshes.at(instance.mesh);
    runtime::Target target;target.id=instance.id;target.label=object.label;target.parent=object.parent;target.instance=object.instance;target.conform_target=object.conform_target;target.smoothing=object.smoothing;
    target.edit_frame=object.edit_frame;target.translation_frame=object.translation_frame;target.base_rotation_degrees=object.rotation_degrees;target.rotation_order=object.rotation_order;target.has_edit_frame=true;
    target.rigid_follow=object.rigid_follow;
    for(auto parent=object.parent;!parent.empty();) {
      if(std::find(target.ancestors.begin(),target.ancestors.end(),parent)!=target.ancestors.end()) throw std::runtime_error("附件父节点链形成循环");
      target.ancestors.push_back(parent);auto found=std::find_if(loaded.nodes.begin(),loaded.nodes.end(),[&](const auto &node){return "#"+node.id==parent;});
      parent=found==loaded.nodes.end()?std::string{}:found->parent;
    }
    const auto target_key=key(object.geometry_file)+"#"+object.geometry_id;
    auto apply_override=[&](runtime::Morph &m) {
      for(const auto &owner:{std::string{},"#"+object.id,"#"+object.geometry_instance_id}) if(auto it=overrides.find({owner,m.id});it!=overrides.end()) {
        const auto &channel=it->second;
        m.initial=number(channel,"current_value",number(channel,"value",m.initial));
        m.minimum=number(channel,"min",m.minimum);m.maximum=number(channel,"max",m.maximum);
        m.clamped=channel.value("clamped",m.clamped);m.step=number(channel,"step_size",m.step);
      }
      if(!std::isfinite(m.initial)||!std::isfinite(m.minimum)||!std::isfinite(m.maximum)||!std::isfinite(m.step)||m.minimum>m.maximum)
        throw std::runtime_error("场景参数范围或权重无效");
    };
    auto instance_channels=[&](J &report) {
      for(size_t i=0;i<target.morphs.size();++i) {auto &m=target.morphs[i];apply_override(m);auto &item=report["morphs"][i];
        item["initial"]=m.initial;item["min"]=m.minimum;item["max"]=m.maximum;item["clamped"]=m.clamped;item["step_size"]=m.step;}
    };
    if(auto cached=cached_targets.find(target_key);cached!=cached_targets.end()) {
      diagnostics::Scope reuse_scope("reuse_catalog");
      target.morphs=cached->second.target.morphs;
      auto report=cached->second.report;report["id"]=target.id;report["label"]=target.label;report["reused_asset_catalog"]=true;
      instance_channels(report);
      out.targets.push_back(std::move(target));out.formulas.push_back(cached->second.formulas);out.report["targets"].push_back(std::move(report));continue;
    }
    FormulaSource formula_source;
    std::map<std::string,std::pair<std::string,fs::path>> missing_addresses;
    std::vector<Asset> allowed;
    {
    diagnostics::Scope assets_scope("asset_metadata");
    allowed.push_back(asset(object.geometry_file,object.geometry_id));
    for(const auto &source:object.geometry_sources) allowed.insert(allowed.begin(),asset(source.file,source.id));
    const auto family_file=object.geometry_sources.empty()?object.geometry_file:object.geometry_sources.back().file;
    const auto family=lower(family_file.parent_path().filename().string());
    if(family=="female 8_1" || family=="male 8_1") {
      const bool female=family=="female 8_1";
      auto legacy=family_file.parent_path().parent_path()/(female?"Female":"Male")/(female?"Genesis8Female.dsf":"Genesis8Male.dsf");
      const auto relative=relative_to_roots(legacy,roots);
      if(!relative.empty()) {const auto resolved=resolve("/"+path_string(relative),legacy,roots);if(!resolved.empty()) legacy=resolved;}
      if(fs::is_regular_file(legacy)) {
        const auto doc=read_document_file(legacy);
        for(const auto &geometry:doc.value("geometry_library",J::array())) {
          if(geometry.at("vertices").at("count").get<size_t>()!=mesh.positions.size()) continue;
          size_t index=0;bool equal=true;
          for(const auto &poly:geometry.at("polylist").at("values")) for(size_t c=3;c+1<poly.size();++c) {
            const std::array<uint32_t,3> triangle{poly[2].get<uint32_t>(),poly[c].get<uint32_t>(),poly[c+1].get<uint32_t>()};
            if(index>=mesh.triangles.size() || mesh.triangles[index].vertices!=triangle) equal=false;
            ++index;
          }
          if(equal && index==mesh.triangles.size()) allowed.insert(allowed.begin(),asset(legacy,geometry.at("id").get<std::string>()));
        }
      }
    }
    }
    for(const auto &a:allowed) formula_source.roots.insert(formula_source.roots.end(),a.roots.begin(),a.roots.end());
    // 同一代按根目录顺序 first-wins；8.1 再覆盖 8，空文件也保留屏蔽语义。
    std::map<std::string,fs::path> files;std::map<std::string,std::string> shadowed;
    {
    diagnostics::Scope scan_scope("directory_scan");
    for(const auto &a:allowed) {
      std::map<std::string,fs::path> family_files;std::vector<fs::path> folders;
      const auto relative=relative_to_roots(a.file.parent_path(),roots);
      if(!relative.empty()) for(const auto &root:roots) folders.push_back(root/relative/"Morphs");
      else folders.push_back(a.file.parent_path()/"Morphs");
      for(const auto &folder:folders) {
        size_t count=0;
        if(fs::is_directory(folder)) for(const auto &entry:fs::recursive_directory_iterator(folder)) {
          if(!entry.is_regular_file() || lower(entry.path().extension().string())!=".dsf") continue;
          ++count;const auto relative_key=lower(path_string(entry.path().lexically_relative(folder)));
          auto [it,inserted]=family_files.emplace(relative_key,entry.path());
          if(!inserted) out.report["file_overrides"].push_back({{"selected",path_string(it->second)},{"ignored",path_string(entry.path())},{"reason","content_root_priority"}});
        }
        out.report["root_scans"].push_back({{"folder",path_string(folder)},{"files",count},{"exists",fs::is_directory(folder)}});
      }
      for(const auto &[relative_key,path]:family_files) {
        if(auto old=files.find(relative_key);old!=files.end()) {
          shadowed[key(old->second)]=key(path);
          out.report["file_overrides"].push_back({{"selected",path_string(path)},{"ignored",path_string(old->second)},{"reason","generation_override"}});
        }
        files[relative_key]=path;
      }
    }
    }
    if(!scene_file.empty() && scene_document.contains("modifier_library")) files["$scene"]=scene_file;
    std::vector<fs::path> queue;std::set<std::string> queued,empty_files,ids;
    for(const auto &[relative,path]:files) if(queued.insert(key(path)).second) queue.push_back(path);
    if(lazy) prefetch_documents(queue);
    std::vector<std::set<std::string>> dependencies;
    std::map<std::string,std::string> declared_assets;
    for(size_t file_index=0;file_index<queue.size();++file_index) {
      diagnostics::Scope resource_scope("parameter_resources");
      if(progress&&file_index%(mesh.curves.empty()?250:10)==0) progress(object.label+" · 参数资源 "+std::to_string(file_index)+" / "+std::to_string(queue.size()));
      const auto path=queue[file_index];
      try {
        const auto handle=lazy?document_view(path):std::make_shared<const J>(read_document_file(path));const auto &doc=*handle;out.report["files_scanned"]=out.report["files_scanned"].get<size_t>()+1;
        declared_assets[key(path)]=asset_uri(doc.value("asset_info",J::object()).value("id",""));
        if(!doc.contains("modifier_library") || doc["modifier_library"].empty()) {
          skipped("empty_override");empty_files.insert(key(path));out.report["empty_overrides"].push_back(path_string(path));
        }
        std::unordered_map<std::string,std::string> addresses;
        for(const auto &modifier:array_member(doc,"modifier_library")) {
          const auto &channel=object_member(modifier,"channel");const auto type=channel.value("type","");
          if(type.empty() || modifier.contains("skin")) {skipped("non_parameter");continue;}
          const auto parent=reference(modifier.value("parent",""));const auto parent_file=resolve(parent.file,path,roots);
          bool geometry_parent=false,node_parent=false,verified_node=false;
          for(const auto &a:allowed) if(!parent_file.empty() && key(parent_file)==key(a.file)) {
            geometry_parent|=parent.id==a.geometry;node_parent|=a.nodes.contains(parent.id);verified_node|=a.geometry_nodes.contains(parent.id);
          }
          if(!geometry_parent && !node_parent) {skipped("other_target");continue;}
          const auto &source=object_member(modifier,"morph");
          std::string geometry_reason;
          int64_t vertex_count=0;
          if(source.contains("vertex_count")) {
            const auto &count=source.at("vertex_count");
            if(!count.is_number_integer()||(count.is_number_unsigned()&&count.get<uint64_t>()>uint64_t(std::numeric_limits<int64_t>::max()))) throw std::runtime_error("Morph 顶点总数必须为有效整数");
            vertex_count=count.get<int64_t>();
            if(vertex_count < -1) geometry_reason="Morph 顶点总数为不支持的负值";
            else if(vertex_count>=0&&uint64_t(vertex_count)!=mesh.positions.size()) geometry_reason="Morph 目标顶点数不匹配";
          }
          if(geometry_reason.empty()&&source.contains("deltas")&&!geometry_parent&&!verified_node) geometry_reason="Morph 绑定到节点，几何目标尚未验证";
          runtime::Morph morph;const auto id=modifier.at("id").get<std::string>();
          morph.id=key(path)+"#"+id;if(!ids.insert(morph.id).second) throw std::runtime_error("同一文件存在重复参数 ID: "+id);
          morph.source=path_string(path);morph.label=channel.value("label",modifier.value("label",id));morph.channel_id=id;morph.owner=parent.id;
          morph.channel_name=modifier.value("name",channel.value("name",id));
          morph.value_type=type;morph.locked=channel.value("locked",false);
          morph.source_vertex_count=vertex_count;
          morph.geometry_validation=geometry_parent?"geometry_uri":verified_node?"skin_binding_node":"unverified_node";
          if(source.contains("deltas")) morph.source_offset_count=source["deltas"].value("count",size_t(0));
          if(lazy&&source.contains("deltas")) {
            const auto &deltas=source["deltas"];
            if(deltas.value("_dfv_rows",size_t(0))!=morph.source_offset_count) throw std::runtime_error("Morph 差值 count 不一致");
          }
          morph.group=modifier.value("group","");morph.minimum=number(channel,"min",0);morph.maximum=number(channel,"max",1);
          morph.initial=number(channel,"current_value",number(channel,"value",0));morph.step=number(channel,"step_size",.01f);
          if(!std::isfinite(morph.minimum)||!std::isfinite(morph.maximum)||!std::isfinite(morph.initial)||!std::isfinite(morph.step)||morph.minimum>morph.maximum) throw std::runtime_error("参数范围无效");
          morph.clamped=channel.value("clamped",false);morph.visible=channel.value("visible",true);morph.auto_follow=channel.value("auto_follow",false);
          if(type=="bool") {morph.minimum=0;morph.maximum=1;morph.step=1;morph.clamped=true;}
          std::set<std::string> refs;const auto &formulas=array_member(modifier,"formulas");morph.formula_count=formulas.size();
          for(const auto &f:formulas) {
            if(f.contains("output")) refs.insert(f["output"].get<std::string>());
            for(const auto &op:array_member(f,"operations")) if(op.contains("url")) refs.insert(op["url"].get<std::string>());
          }
          morph.kind="control";
          if(source.contains("deltas") && geometry_reason.empty()) {
            if(lazy&&morph.source_offset_count) morph.payload=payload(path,id,morph.source_offset_count,mesh.positions.size());
            else if(!lazy) morph.offsets=offsets(source,mesh.positions.size());
            if(morph.has_offsets()) morph.kind="sparse";
          }
          if(!morph.has_offsets()) morph.unsupported="控制器没有直接顶点差值，尚未实现公式 / 骨骼驱动";
          if(!formulas.empty()) morph.unsupported="包含 Formula / ERC，尚未支持完整驱动";
          if(source.contains("hd_url")) {morph.limitation="仅应用基础网格形态；HD 细节尚未支持";if(!morph.has_offsets()) {morph.kind="hd_only";morph.intrinsic_error=morph.unsupported="仅包含 HD 数据，尚未支持";}}
          if(type=="alias") {morph.kind="alias";morph.alias_target=channel.value("target_channel","");morph.unsupported="子节点参数别名，尚未实现与目标通道的双向编辑";if(!morph.alias_target.empty()) refs.insert(morph.alias_target);}
          else if(type!="float") {morph.kind=type;morph.unsupported="尚未支持该通道类型: "+type;if(type!="bool"&&type!="int") morph.intrinsic_error=morph.unsupported;}
          if(channel.value("locked",false)) morph.unsupported="资产将此参数标为锁定";
          if(!geometry_reason.empty()) {morph.kind="unverified_sparse";morph.intrinsic_error=morph.unsupported=geometry_reason;}
          auto formula_address=[&](const std::string &uri) {
            if(const auto cached=addresses.find(uri);cached!=addresses.end()) return cached->second;
            auto compute=[&] {
            const auto ref=reference(uri);const auto file=resolve(ref.file,path,roots);
            if(file.empty()) {const auto marker="$missing/"+key(path)+"|"+decode_uri(uri);missing_addresses[marker]={uri,path};return marker;}
            if(shadowed.contains(key(file))) return std::string("$overridden/")+decode_uri(uri);
            for(const auto &a:allowed) if((ref.file.empty()||key(file)==key(a.file))&&a.nodes.contains(ref.id)) return "$node/"+ref.id+"?"+ref.property;
            return (ref.file.empty()?"$local/":"")+key(file)+"#"+ref.id+"?"+ref.property;
            };auto address=compute();addresses.emplace(uri,address);return address;
          };
          // 加入目录外的参数依赖，但不能重新引入被 8.1 覆盖的旧参数。
          for(const auto &uri:refs) {
            const auto ref=reference(uri);if(ref.file.empty()) continue;
            const auto dependency=resolve(ref.file,path,roots);if(dependency.empty() || shadowed.contains(key(dependency))) continue;
            bool is_asset=false;for(const auto &a:allowed) is_asset|=key(dependency)==key(a.file);
            if(!is_asset && lower(dependency.extension().string())==".dsf" && queued.insert(key(dependency)).second) queue.push_back(dependency);
          }
          formula_source.alias_symbols.push_back(morph.alias_target.empty()?"":formula_address(morph.alias_target));
          try {append_formulas(formula_source,uint32_t(target.morphs.size()),formulas,formula_address);}
          catch(const std::exception &e) {morph.intrinsic_error=morph.unsupported=std::string("公式无法编译：")+e.what();}
          target.morphs.push_back(std::move(morph));dependencies.push_back(std::move(refs));
        }
      } catch(const std::exception &e) {out.report["diagnostics"].push_back({{"file",path_string(path)},{"reason",e.what()}});}
    }
    diagnostics::Scope finalize_scope("references_reports_cache");
    std::map<std::string,size_t> by_asset;std::map<std::string,std::vector<size_t>> by_id;
    std::map<std::pair<std::string,std::string>,std::vector<size_t>> by_directory;
    std::map<std::pair<std::string,std::string>,std::vector<size_t>> by_declared_asset;
    for(size_t i=0;i<target.morphs.size();++i) {
      const auto &m=target.morphs[i];by_asset[m.id]=i;by_id[m.channel_id].push_back(i);
      by_directory[{key(fs::u8path(m.source).parent_path()),m.channel_id}].push_back(i);
      const auto &declared=declared_assets.at(key(fs::u8path(m.source)));
      if(!declared.empty()) by_declared_asset[{declared,m.channel_id}].push_back(i);
    }
    struct Recovered {fs::path file;std::string reason="missing_file";bool repaired=false;std::string method;};
    std::map<std::pair<fs::path,std::string>,Recovered> recovered;
    auto recover=[&](const std::string &uri,const fs::path &owner) {
      const auto ref=reference(uri);const auto cache_key=std::make_pair(ref.file.starts_with('/')?fs::path{}:owner,uri);
      if(const auto found=recovered.find(cache_key);found!=recovered.end()) return found->second;
      Recovered result;result.file=resolve(ref.file,owner,roots);
      if(!result.file.empty()) {result.reason.clear();if(shadowed.contains(key(result.file))) result.reason="generation_override";else if(empty_files.contains(key(result.file))) result.reason="empty_override";}
      else if(!ref.file.empty()) {
        const auto declared=by_declared_asset.find({asset_uri(ref.file),ref.id});
        if(declared!=by_declared_asset.end()) {
          if(declared->second.size()!=1) result.reason="ambiguous_asset_uri";
          else {result.file=fs::u8path(target.morphs[declared->second[0]].source);result.reason.clear();result.repaired=true;result.method="declared_asset_uri";}
          recovered[cache_key]=result;return result;
        }
        const auto relative=fs::u8path(ref.file.starts_with('/')?ref.file.substr(1):ref.file).lexically_normal();
        if(!relative.is_absolute()&&!relative.has_root_name()&&!relative.empty()&&*relative.begin()!="..") {
          std::vector<fs::path> directories;if(!ref.file.starts_with('/')) directories.push_back((owner.parent_path()/relative).parent_path());
          for(const auto &root:roots) directories.push_back((root/relative).parent_path());
          for(const auto &directory:directories) {
            const auto found=by_directory.find({key(directory),ref.id});if(found==by_directory.end()) continue;
            if(found->second.size()!=1) {result.reason="ambiguous_directory_id";break;}
            result.file=fs::u8path(target.morphs[found->second[0]].source);result.reason.clear();result.repaired=true;result.method="same_directory_unique_id";break;
          }
        }
      }
      recovered[cache_key]=result;return result;
    };
    // 等完整目录建立后才恢复引用，避免目录枚举顺序决定结果；保留空覆盖和同目录歧义。
    auto repair_symbol=[&](std::string &symbol) {
      const auto found=missing_addresses.find(symbol);if(found==missing_addresses.end()) return;
      const auto &[uri,owner]=found->second;const auto ref=reference(uri);const auto match=recover(uri,owner);
      if(match.repaired) symbol=key(match.file)+"#"+ref.id+"?"+ref.property;
      else symbol="$"+match.reason+"/"+decode_uri(uri);
    };
    for(auto &symbol:formula_source.symbols) repair_symbol(symbol);
    for(auto &symbol:formula_source.alias_symbols) repair_symbol(symbol);
    J repairs=J::array();
    J items=J::array();
    for(size_t i=0;i<target.morphs.size();++i) {
      auto &m=target.morphs[i];J unresolved=J::array();size_t nodes=0,parameters=0;
      for(const auto &uri:dependencies[i]) {
        const auto ref=reference(uri);const auto match=recover(uri,fs::u8path(m.source));const auto &file=match.file;std::string reason=match.reason;
        if(match.repaired) {++m.repaired_references;repairs.push_back({{"owner",m.id},{"uri",uri},{"resolved",path_string(file)},{"channel",ref.id},{"method",match.method}});}
        if(reason.empty()) {
          bool node=false;for(const auto &a:allowed) if((ref.file.empty() || key(file)==key(a.file)) && a.nodes.contains(ref.id)) node=true;
          if(node) {++nodes;continue;}
          if(by_asset.contains(key(file)+"#"+ref.id)) {++parameters;continue;}
          if(ref.file.empty() && by_id.contains(ref.id) && by_id[ref.id].size()==1) {++parameters;continue;}
          reason="missing_or_ambiguous_channel";
        }
        unresolved.push_back({{"uri",uri},{"reason",reason}});
      }
      m.missing_dependencies=unresolved.size();
      if(!unresolved.empty() && m.unsupported.empty()) m.unsupported="存在未解析的参数依赖";
      items.push_back({{"id",m.id},{"label",m.label},{"group",m.group},{"source",m.source},{"owner",m.owner},{"kind",m.kind},
        {"channel_id",m.channel_id},{"channel_name",m.channel_name},{"asset_uri",declared_assets.at(key(fs::u8path(m.source)))},{"alias_target",m.alias_target},{"formula_count",m.formula_count},{"offsets",m.offset_count()},
        {"source_vertex_count",m.source_vertex_count},{"source_offset_count",m.source_offset_count},{"geometry_validation",m.geometry_validation},{"repaired_references",m.repaired_references},
        {"unsupported",m.unsupported},{"visible",m.visible},{"auto_follow",m.auto_follow},{"initial",m.initial},{"min",m.minimum},{"max",m.maximum},
        {"dependency_count",dependencies[i].size()},{"node_dependencies",nodes},{"parameter_dependencies",parameters},{"unresolved_dependencies",unresolved}});
    }
    out.report["targets"].push_back({{"id",target.id},{"label",target.label},{"morphs",items},{"compatible_assets",allowed.size()},{"reference_repairs",repairs}});
    // 缓存资产原始通道，实例的数值、限幅与步长都在复制之后应用。
    cached_targets[target_key]={target,formula_source,out.report["targets"].back()};
    instance_channels(out.report["targets"].back());
    out.targets.push_back(std::move(target));
    formula_source.interned.clear();formula_source.interned.rehash(0);out.formulas.push_back(std::move(formula_source));
  }
  return out;
}
}
