// 在 adapter.cpp 的 dfv 命名空间内包含。GeoGraft 的 SSS 必须使用同一个 Cycles Object。
static std::vector<bool> graft_visibility(const ir::Scene &scene,const std::vector<int> &parts) {
  std::vector<bool> result;for(int i:parts) result.push_back(i>=0&&scene.instances[size_t(i)].visible);return result;
}
static ir::Transform graft_transform(const runtime::GraftSurface &surface,const ir::Scene &scene,const std::vector<int> &parts) {
  for(int i:parts) if(i>=0) return surface.transform(scene,uint32_t(i));
  throw std::runtime_error("GeoGraft 渲染组没有对象");
}
static void graft_mesh(ccl::Scene &scene,ccl::Mesh &mesh,const runtime::GraftSurface &surface,const ir::Scene &source,
                       const std::vector<int> &parts,const std::vector<ccl::Shader *> &shaders,bool topology) {
  using namespace ccl;
  const auto visible=graft_visibility(source,parts);std::vector<size_t> offsets;size_t slots=0,faces=0;
  for(size_t p=0;p<parts.size();++p) {offsets.push_back(slots);slots+=source.instances[parts[p]<0?surface.members()[p]:uint32_t(parts[p])].materials.size();if(visible[p]) faces+=surface.triangles(p).size();}
  if(topology) {
    mesh.clear(true);mesh.resize_mesh(int(surface.positions().size()),int(faces));
    auto *uv=mesh.attributes.add(ATTR_STD_UV,ustring("UVMap"))->data_for_write<float2>();size_t face=0;
    for(size_t p=0;p<parts.size();++p) if(visible[p]) {
      const auto &vertices=surface.vertices(p);const bool smooth=source.meshes[source.instances[size_t(parts[p])].mesh].smooth;
      for(const auto &t:surface.triangles(p)) {for(size_t c=0;c<3;++c) {mesh.get_triangles()[face*3+c]=int(vertices[t.vertices[c]]);uv[face*3+c]=make_float2(t.uv[c].x,t.uv[c].y);}mesh.get_shader()[face]=int(offsets[p]+t.material_slot);mesh.get_smooth()[face]=smooth;++face;}
    }
  }
  if(mesh.num_verts()!=surface.positions().size()) throw std::runtime_error("GeoGraft 细分顶点数量与设备网格不一致");
  for(auto attribute:{ATTR_STD_VERTEX_NORMAL,ATTR_STD_POSITION_UNDISPLACED,ATTR_STD_NORMAL_UNDISPLACED,ATTR_STD_UV_TANGENT_UNDISPLACED,ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED}) mesh.attributes.remove(attribute);
  auto *positions=mesh.get_position_for_write();auto *normals=mesh.attributes.add(ATTR_STD_VERTEX_NORMAL)->data_for_write<packed_normal>();
  for(size_t v=0;v<surface.positions().size();++v) {positions[v]=vector(surface.positions()[v]);normals[v]=packed_normal(vector(surface.normals()[v]));}
  array<Node *> used(shaders.size());for(size_t i=0;i<shaders.size();++i) used[i]=shaders[i];mesh.set_used_shaders(used);
  mesh.tag_position_modified();mesh.compute_bounds();mesh.tag_update(&scene,topology);
}
