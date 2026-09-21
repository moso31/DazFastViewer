#include "daz/morphs.h"
#include <algorithm>
#include <cmath>
#include <map>
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
struct Asset {fs::path file;std::string geometry;std::set<std::string> nodes;std::vector<std::string> roots;};
static Asset asset(const fs::path &file,const std::string &geometry) {
  Asset out{file,geometry,{}};const auto doc=read_document_file(file);
  for(const auto &n:doc.value("node_library",J::array())) {out.nodes.insert(n.at("id").get<std::string>());if(n.value("type","")=="figure"||!n.contains("parent")) out.roots.push_back(n.at("id").get<std::string>());}
  return out;
}
MorphCatalog discover_morphs(LoadedScene &loaded,const std::vector<fs::path> &input_roots) {
  path_keys.clear();resolved_paths.clear();
  MorphCatalog out;out.report={{"targets",J::array()},{"diagnostics",J::array()},{"files_scanned",0},{"skipped_types",J::object()},
    {"content_roots",J::array()},{"file_overrides",J::array()},{"empty_overrides",J::array()},{"root_scans",J::array()}};
  std::vector<fs::path> roots;std::set<std::string> root_keys;
  for(const auto &root:input_roots) if(root_keys.insert(key(root)).second) {
    roots.push_back(fs::weakly_canonical(root));out.report["content_roots"].push_back({{"path",path_string(root)},{"exists",fs::is_directory(root)}});
  }
  auto skipped=[&](const std::string &type) {out.report["skipped_types"][type]=out.report["skipped_types"].value(type,0)+1;};
  fs::path scene_file;J scene_document=J::object();
  if(loaded.report.contains("input")) {scene_file=fs::u8path(loaded.report.at("input").get<std::string>());scene_document=read_document_file(scene_file);}
  std::set<uint32_t> used_meshes;
  for(const auto &object:loaded.objects) {
    auto &instance=loaded.scene.instances.at(object.instance);
    if(!used_meshes.insert(instance.mesh).second) {
      auto copy=loaded.scene.meshes.at(instance.mesh);copy.id+="/"+object.id;
      instance.mesh=uint32_t(loaded.scene.meshes.size());loaded.scene.meshes.push_back(std::move(copy));
    }
    const auto &mesh=loaded.scene.meshes.at(instance.mesh);
    runtime::Target target;target.id=instance.id;target.label=object.label;target.parent=object.parent;target.instance=object.instance;
    FormulaSource formula_source;
    std::vector<Asset> allowed{asset(object.geometry_file,object.geometry_id)};
    const auto family=lower(object.geometry_file.parent_path().filename().string());
    if(family=="female 8_1" || family=="male 8_1") {
      const bool female=family=="female 8_1";
      auto legacy=object.geometry_file.parent_path().parent_path()/(female?"Female":"Male")/(female?"Genesis8Female.dsf":"Genesis8Male.dsf");
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
    for(const auto &a:allowed) formula_source.roots.insert(formula_source.roots.end(),a.roots.begin(),a.roots.end());
    // 同一代按根目录顺序 first-wins；8.1 再覆盖 8，空文件也保留屏蔽语义。
    std::map<std::string,fs::path> files;std::map<std::string,std::string> shadowed;
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
    if(!scene_file.empty() && scene_document.contains("modifier_library")) files["$scene"]=scene_file;
    std::vector<fs::path> queue;std::set<std::string> queued,empty_files,ids;
    for(const auto &[relative,path]:files) if(queued.insert(key(path)).second) queue.push_back(path);
    std::vector<std::set<std::string>> dependencies;
    for(size_t file_index=0;file_index<queue.size();++file_index) {
      const auto path=queue[file_index];
      try {
        const auto doc=read_document_file(path);out.report["files_scanned"]=out.report["files_scanned"].get<size_t>()+1;
        if(!doc.contains("modifier_library") || doc["modifier_library"].empty()) {
          skipped("empty_override");empty_files.insert(key(path));out.report["empty_overrides"].push_back(path_string(path));
        }
        for(const auto &modifier:doc.value("modifier_library",J::array())) {
          const auto &channel=modifier.value("channel",J::object());const auto type=channel.value("type","");
          if(type.empty() || modifier.contains("skin")) {skipped("non_parameter");continue;}
          const auto parent=reference(modifier.value("parent",""));const auto parent_file=resolve(parent.file,path,roots);
          bool geometry_parent=false,node_parent=false;
          for(const auto &a:allowed) if(!parent_file.empty() && key(parent_file)==key(a.file)) {
            geometry_parent|=parent.id==a.geometry;node_parent|=a.nodes.contains(parent.id);
          }
          if(!geometry_parent && !node_parent) {skipped("other_target");continue;}
          const auto source=modifier.value("morph",J::object());
          std::string geometry_reason;
          if(source.contains("vertex_count") && source.at("vertex_count").get<size_t>()!=mesh.positions.size()) geometry_reason="Morph 目标顶点数不匹配";
          else if(source.contains("deltas") && !geometry_parent) geometry_reason="Morph 绑定到节点，几何目标尚未验证";
          runtime::Morph morph;const auto id=modifier.at("id").get<std::string>();
          morph.id=key(path)+"#"+id;if(!ids.insert(morph.id).second) throw std::runtime_error("同一文件存在重复参数 ID: "+id);
          morph.source=path_string(path);morph.label=channel.value("label",modifier.value("label",id));morph.channel_id=id;morph.owner=parent.id;
          morph.channel_name=modifier.value("name",channel.value("name",id));
          morph.value_type=type;morph.locked=channel.value("locked",false);
          morph.source_vertex_count=source.value("vertex_count",size_t(0));
          if(source.contains("deltas")) morph.source_offset_count=source["deltas"].value("count",size_t(0));
          morph.group=modifier.value("group","");morph.minimum=number(channel,"min",0);morph.maximum=number(channel,"max",1);
          morph.initial=number(channel,"current_value",number(channel,"value",0));morph.step=number(channel,"step_size",.01f);
          if(!std::isfinite(morph.minimum)||!std::isfinite(morph.maximum)||!std::isfinite(morph.initial)||!std::isfinite(morph.step)||morph.minimum>morph.maximum) throw std::runtime_error("参数范围无效");
          morph.clamped=channel.value("clamped",false);morph.visible=channel.value("visible",true);morph.auto_follow=channel.value("auto_follow",false);
          if(type=="bool") {morph.minimum=0;morph.maximum=1;morph.step=1;morph.clamped=true;}
          for(const auto &override:scene_document.value("scene",J::object()).value("modifiers",J::array())) {
            const auto parent=decode_uri(override.value("parent",""));
            if(!parent.empty() && parent!="#"+object.id && parent!="#"+object.geometry_instance_id) continue;
            const auto uri=override.value("url","");if(uri.empty()) continue;const auto ref=reference(uri);const auto candidate=resolve(ref.file,scene_file,roots);
            if(ref.id==id && !candidate.empty() && key(candidate)==key(path)) {
              const auto c=override.value("channel",J::object());morph.initial=number(c,"current_value",number(c,"value",morph.initial));
            }
          }
          if(!std::isfinite(morph.initial)) throw std::runtime_error("场景参数权重无效");
          if(morph.clamped) morph.initial=std::clamp(morph.initial,morph.minimum,morph.maximum);
          std::set<std::string> refs;const auto formulas=modifier.value("formulas",J::array());morph.formula_count=formulas.size();
          for(const auto &f:formulas) {
            if(f.contains("output")) refs.insert(f["output"].get<std::string>());
            for(const auto &op:f.value("operations",J::array())) if(op.contains("url")) refs.insert(op["url"].get<std::string>());
          }
          morph.kind="control";
          if(source.contains("deltas") && geometry_reason.empty()) {
            const auto &deltas=source.at("deltas");if(deltas.at("count").get<size_t>()!=deltas.at("values").size()) throw std::runtime_error("Morph 差值 count 不一致");
            std::set<uint32_t> seen;
            for(const auto &row:deltas.at("values")) {
              if(row.size()!=4) throw std::runtime_error("Morph 差值必须为顶点索引与 XYZ");
              const auto vertex=row[0].get<uint32_t>();const float x=row[1].get<float>(),y=row[2].get<float>(),z=row[3].get<float>();
              if(vertex>=mesh.positions.size()||!seen.insert(vertex).second||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) throw std::runtime_error("Morph 差值无效");
              morph.offsets.push_back({vertex,{x*.01f,-z*.01f,y*.01f}});
            }
            if(!morph.offsets.empty()) morph.kind="sparse";
          }
          if(morph.offsets.empty()) morph.unsupported="控制器没有直接顶点差值，尚未实现公式 / 骨骼驱动";
          if(!formulas.empty()) morph.unsupported="包含 Formula / ERC，尚未支持完整驱动";
          if(source.contains("hd_url")) {morph.intrinsic_error=morph.unsupported="包含 HD 数据，尚未支持完整形态";if(morph.offsets.empty()) morph.kind="hd_only";}
          if(type=="alias") {morph.kind="alias";morph.alias_target=channel.value("target_channel","");morph.unsupported="子节点参数别名，尚未实现与目标通道的双向编辑";if(!morph.alias_target.empty()) refs.insert(morph.alias_target);}
          else if(type!="float") {morph.kind=type;morph.unsupported="尚未支持该通道类型: "+type;if(type!="bool") morph.intrinsic_error=morph.unsupported;}
          if(channel.value("locked",false)) morph.unsupported="资产将此参数标为锁定";
          if(!geometry_reason.empty()) {morph.kind="unverified_sparse";morph.intrinsic_error=morph.unsupported=geometry_reason;}
          auto formula_address=[&](const std::string &uri) {
            const auto ref=reference(uri);const auto file=resolve(ref.file,path,roots);
            if(file.empty()) return std::string("$missing/")+decode_uri(uri);
            if(shadowed.contains(key(file))) return std::string("$overridden/")+decode_uri(uri);
            for(const auto &a:allowed) if((ref.file.empty()||key(file)==key(a.file))&&a.nodes.contains(ref.id)) return "$node/"+ref.id+"?"+ref.property;
            return (ref.file.empty()?"$local/":"")+key(file)+"#"+ref.id+"?"+ref.property;
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
    std::map<std::string,size_t> by_asset;std::map<std::string,std::vector<size_t>> by_id;
    for(size_t i=0;i<target.morphs.size();++i) {by_asset[target.morphs[i].id]=i;by_id[target.morphs[i].channel_id].push_back(i);}
    J items=J::array();
    for(size_t i=0;i<target.morphs.size();++i) {
      auto &m=target.morphs[i];J unresolved=J::array();size_t nodes=0,parameters=0;
      for(const auto &uri:dependencies[i]) {
        const auto ref=reference(uri);const auto file=resolve(ref.file,fs::u8path(m.source),roots);std::string reason;
        if(file.empty()) reason="missing_file";
        else if(shadowed.contains(key(file))) reason="generation_override";
        else if(empty_files.contains(key(file))) reason="empty_override";
        else {
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
        {"channel_id",m.channel_id},{"channel_name",m.channel_name},{"alias_target",m.alias_target},{"formula_count",m.formula_count},{"offsets",m.offsets.size()},
        {"source_vertex_count",m.source_vertex_count},{"source_offset_count",m.source_offset_count},
        {"unsupported",m.unsupported},{"visible",m.visible},{"auto_follow",m.auto_follow},{"initial",m.initial},{"min",m.minimum},{"max",m.maximum},
        {"dependency_count",dependencies[i].size()},{"node_dependencies",nodes},{"parameter_dependencies",parameters},{"unresolved_dependencies",unresolved}});
    }
    out.report["targets"].push_back({{"id",target.id},{"label",target.label},{"morphs",items},{"compatible_assets",allowed.size()}});
    out.targets.push_back(std::move(target));
    formula_source.interned.clear();formula_source.interned.rehash(0);out.formulas.push_back(std::move(formula_source));
  }
  return out;
}
}
