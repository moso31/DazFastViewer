// Shared with the DSON loader's repository and URI resolution rules.
namespace {
std::string encode_uv_uri(const std::string &value) {
  constexpr char digits[]="0123456789ABCDEF";std::string result;
  for(unsigned char c:value)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c==':'||c=='.'||c=='-'||c=='_'||c=='~')result+=char(c);
    else {result+='%';result+=digits[c>>4];result+=digits[c&15];}
  return result;
}
MaterialUVSet uv_reference(const fs::path &file,const Json &asset) {
  const auto id=asset.at("id").get<std::string>();auto label=asset.value("label",asset.value("name",id));if(label.empty())label=id;
  auto path=utf8(file);
#ifdef _WIN32
  // DAZ assets vary "Daz 3D"/"DAZ 3D" spelling. Paths are case insensitive;
  // the fragment remains case sensitive and the authored label stays intact.
  for(auto &c:path)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
#endif
  return {encode_uv_uri(path)+"#"+encode_uv_uri(id),path,label};
}
struct UVCoordinates {
  std::vector<ir::Vec2> values;
  std::map<std::pair<uint32_t,uint32_t>,uint32_t> seams;
  ir::Vec2 at(uint32_t polygon,uint32_t vertex) const {const auto s=seams.find({polygon,vertex});return values.at(s==seams.end()?vertex:s->second);}
};
UVCoordinates read_uv_coordinates(const Json &asset,const ir::Mesh &mesh) {
  if(asset.at("vertex_count").get<size_t>()!=mesh.positions.size())fail("UV Set 与目标顶点数不匹配");
  UVCoordinates uv;
  for(const auto &v:values(asset.at("uvs"))){if(v.size()!=2)fail("无效 UV 坐标");ir::Vec2 p{v.at(0).get<float>(),v.at(1).get<float>()};if(!std::isfinite(p.x)||!std::isfinite(p.y))fail("UV 坐标不是有限数值");uv.values.push_back(p);}
  if(uv.values.size()<mesh.positions.size())fail("UV Set 缺少顶点坐标");
  for(const auto &v:asset.value("polygon_vertex_indices",Json::array())) {
    if(v.size()!=3)fail("无效 UV 接缝记录");const auto p=v.at(0).get<uint32_t>(),vertex=v.at(1).get<uint32_t>(),index=v.at(2).get<uint32_t>();
    if(p>=mesh.source_polygon_count||vertex>=mesh.positions.size()||index>=uv.values.size())fail("UV 接缝索引越界");
    if(!mesh.polygons.empty()){const auto &face=mesh.polygons.at(p);if(std::find(face.vertices.begin(),face.vertices.end(),vertex)==face.vertices.end())fail("UV 接缝与目标多边形不匹配");}
    if(!uv.seams.emplace(std::make_pair(p,vertex),index).second)fail("UV 接缝记录重复");
  }
  return uv;
}
bool geometry_matches_uv_mesh(const Json &g,const ir::Mesh &mesh) {
  if(!g.contains("vertices")||!g.contains("polylist")||values(g.at("vertices")).size()!=mesh.positions.size())return false;
  const auto &polygons=values(g.at("polylist"));if(polygons.size()!=mesh.polygons.size())return false;
  for(size_t p=0;p<polygons.size();++p){const auto &a=polygons[p];const auto &b=mesh.polygons[p].vertices;if(a.size()!=b.size()+2)return false;for(size_t v=0;v<b.size();++v)if(a[v+2]!=b[v])return false;}
  return true;
}
std::vector<MaterialSelectionSet> read_material_selection_sets(const Json &geometry) {
  std::vector<MaterialSelectionSet> result;
  for(const auto &extra:array_member(geometry,"extra"))if(extra.value("type","")=="material_selection_sets")
    for(const auto &s:array_member(extra,"material_selection_sets"))if(!s.value("name","").empty())result.push_back({s.at("name"),s.value("parent",""),s.value("materials",std::vector<std::string>{})});
  return result;
}
void bind_uv_reference(ir::Scene &scene,std::vector<uint32_t> &materials,size_t slot,const MaterialUVSet &uv) {
  auto m=scene.materials.at(materials.at(slot));if(material_uv_set(m)==uv)return;
  set_material_uv_set(m,uv);materials[slot]=uint32_t(scene.materials.size());scene.materials.push_back(std::move(m));
}
}
MaterialUVSet material_uv_set(const ir::Material &material) {
  if(material.source_definition.empty())return {};const auto d=Json::parse(material.source_definition);const auto uri=d.value("uv_set","");
  if(uri.empty())return {};const auto hash=uri.find('#');
  return {uri,d.value("uv_owner",d.value("owner","")),d.value("uv_label",decode(uri.substr(hash==std::string::npos?0:hash+1)))};
}
void set_material_uv_set(ir::Material &material,const MaterialUVSet &uv) {
  auto d=material.source_definition.empty()?Json{{"channels",Json::object()}}:Json::parse(material.source_definition);
  if(uv.uri.empty()){d.erase("uv_set");d.erase("uv_owner");d.erase("uv_label");}
  else {d["uv_set"]=uv.uri;d["uv_owner"]=uv.owner;d["uv_label"]=uv.label;}
  material.source_definition=d.dump();
}
std::optional<MaterialUVSet> material_uv_baseline(const ir::Material &material) {
  if(material.source_definition.empty())return {};const auto d=Json::parse(material.source_definition);
  if(!d.contains("uv_baseline"))return {};const auto &b=d.at("uv_baseline");return MaterialUVSet{b.at("uri"),b.at("owner"),b.at("label")};
}
std::string material_uv_topology(const ir::Mesh &mesh) {
  uint64_t hash=14695981039346656037ull;auto add=[&](uint64_t n){for(int b=0;b<8;++b){hash^=(n>>(8*b))&255;hash*=1099511628211ull;}};
  add(mesh.positions.size());add(mesh.polygons.size());add(mesh.triangles.size());add(mesh.curves.size());
  for(const auto &p:mesh.polygons){add(p.vertices.size());for(auto v:p.vertices)add(v);}
  for(const auto &t:mesh.triangles){add(t.source_polygon);for(auto v:t.vertices)add(v);}
  for(const auto &c:mesh.curves){add(c.vertices.size());for(auto v:c.vertices)add(v);}
  return std::to_string(hash);
}
MaterialUVCatalog discover_material_uv_sets(const LoadedScene &loaded,size_t instance,const LoadOptions &options) {
  MaterialUVCatalog result;Repository repo;repo.roots=options.content_roots;
  const auto &i=loaded.scene.instances.at(instance);const auto &mesh=loaded.scene.meshes.at(i.mesh);
  std::set<fs::path> files,directories;std::set<std::string> ids;
  auto add=[&](const fs::path &file,const Json &asset){read_uv_coordinates(asset,mesh);auto ref=uv_reference(file,asset);if(ids.insert(ref.uri).second)result.sets.push_back(std::move(ref));};
  // Saved per-surface references may live outside the model's UV Sets directory.
  for(auto m:i.materials){const auto ref=material_uv_set(loaded.scene.materials.at(m));if(ref.uri.empty())continue;
    try{const auto [file,asset]=repo.asset(ref.uri,fs::u8path(ref.owner),"uv_set_library");add(file,*asset);}
    catch(const std::exception &e){result.warnings.push_back(e.what());}
  }
  auto geometry=[&](const fs::path &file,const std::string &id){
    if(file.empty())return;
    const auto &doc=repo.document(file);if(!doc.contains("geometry_library"))return;
    for(const auto &g:doc.at("geometry_library"))if(g.value("id","")==id&&geometry_matches_uv_mesh(g,mesh)){
      files.insert(file);const auto directory=file.parent_path()/"UV Sets";directories.insert(directory);
      // Each configured library may contribute additional sets for the same asset.
      for(const auto &root:repo.roots){auto relative=directory.lexically_relative(root);if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")continue;for(const auto &other:repo.roots)directories.insert(other/relative);}
    }
  };
  size_t source_instance=instance;if(i.shell_source>=0)source_instance=size_t(i.shell_source);if(i.prototype>=0)source_instance=size_t(i.prototype);
  for(const auto &object:loaded.objects)if(object.instance==source_instance){
    try{geometry(object.geometry_file,object.geometry_id);for(const auto &source:object.geometry_sources)geometry(source.file,source.id);
      // Genesis 8.1 explicitly declares Genesis 8 as an extended base. Verify
      // polygon connectivity before discovering that base's installed UV sets.
      const auto filename=object.geometry_file.filename().string();
      for(const auto &base:object.extended_bases)for(const auto *sex:{"Female","Male"})if(base==std::string("/Genesis 8/")+sex&&filename==std::string("Genesis8_1")+sex+".dsf"){
        const auto relative="/data/DAZ%203D/Genesis%208/"+std::string(sex)+"/Genesis8"+sex+".dsf";
        const auto file=repo.path(relative,object.geometry_file);const auto &doc=repo.document(file);
        for(const auto &g:doc.at("geometry_library"))geometry(file,g.at("id"));
      }
    }catch(const std::exception &e){result.warnings.push_back(e.what());}
  }
  std::set<std::string> library_files;
  for(const auto &directory:directories){std::error_code ec;if(!fs::is_directory(directory,ec))continue;
    for(fs::recursive_directory_iterator it(directory,fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
      if(ec){result.warnings.push_back(ec.message());ec.clear();continue;}if(!it->is_regular_file(ec)||it->path().extension()!=".dsf")continue;
      auto path=it->path();std::string key=utf8(path);
      for(const auto &root:repo.roots){auto relative=path.lexically_relative(root);if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")continue;key=utf8(relative);path=repo.path("/"+encode_uv_uri(key),path);break;}
      if(library_files.insert(key).second)files.insert(path);
    }
  }
  for(const auto &file:files)try{for(const auto &asset:array_member(repo.document(file),"uv_set_library"))try{add(file,asset);}catch(const std::exception &e){result.warnings.push_back(utf8(file)+": "+e.what());}}
    catch(const std::exception &e){result.warnings.push_back(e.what());}
  std::sort(result.sets.begin(),result.sets.end(),[](const auto &a,const auto &b){return std::tie(a.label,a.uri)<std::tie(b.label,b.uri);});return result;
}
bool apply_material_uv(ir::Scene &scene,size_t instance,size_t slot,const ir::Material &preset,const LoadOptions &options){
  const auto requested=material_uv_set(preset);if(requested.uri.empty())return false;
  Repository repo;repo.roots=options.content_roots;const auto [file,source]=repo.asset(requested.uri,fs::u8path(requested.owner),"uv_set_library");auto &i=scene.instances.at(instance);auto mesh=scene.meshes.at(i.mesh);
  const auto uv=read_uv_coordinates(*source,mesh);bool changed=false;
  for(auto &face:mesh.triangles)if(face.material_slot==slot)for(size_t c=0;c<3;++c){const auto value=uv.at(face.source_polygon,face.vertices[c]);changed|=face.uv[c]!=value;face.uv[c]=value;}
  for(size_t p=0;p<mesh.polygons.size();++p){auto &face=mesh.polygons[p];if(face.material_slot==slot)for(size_t c=0;c<face.vertices.size();++c){const auto value=uv.at(uint32_t(p),face.vertices[c]);changed|=face.uv[c]!=value;face.uv[c]=value;}}
  for(auto &curve:mesh.curves)if(curve.material_slot==slot){const auto value=uv.values.at(curve.vertices.front());changed|=curve.uv!=value;curve.uv=value;}
  if(i.prototype>=0)fail("DAZ Instance 的 UV 与原型共享，请在原型对象上切换 UV Set");
  const auto ref=uv_reference(file,*source);const bool renamed=material_uv_set(scene.materials.at(i.materials.at(slot)))!=ref;
  if(changed){if(std::count_if(scene.instances.begin(),scene.instances.end(),[&](const auto &other){return other.mesh==i.mesh;})>1){mesh.id+="/material-uv/"+i.id;i.mesh=uint32_t(scene.meshes.size());scene.meshes.push_back(std::move(mesh));}else scene.meshes[i.mesh]=std::move(mesh);}
  bind_uv_reference(scene,i.materials,slot,ref);return changed||renamed;
}
