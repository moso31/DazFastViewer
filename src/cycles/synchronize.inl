// 在 adapter.cpp 的 dfv 命名空间内包含，复用材质和坐标转换辅助函数。
static bool same_topology(const ir::Mesh &a,const ir::Mesh &b) {
  return runtime::same_mesh_topology(a,b);
}
void CyclesAdapter::load(const ir::Scene &source) {
  if(loaded_) throw std::runtime_error("同一 CyclesAdapter 只允许一次完整加载，请使用同步接口");
  synchronize(source);
}
bool CyclesAdapter::synchronize(const ir::Scene &source) {
  using namespace ccl;
  diagnostics::Scope scope("adapter_sync");
  source.validate();
  std::set<std::string> identities;
  for(const auto &mesh:source.meshes) if(!identities.insert(mesh.id).second) throw std::runtime_error("网格身份重复，无法安全增量同步："+mesh.id);
  // 所有模板及快照先准备完毕。非法等级、拓扑和主要 CPU 分配失败不会破坏现有节点。
  ir::Scene saved=source;
  std::vector<runtime::GraftSurface> grafts;std::vector<GraftBinding> graft_bindings(source.instances.size());
  std::vector<bool> graft_topology_changed,graft_positions_changed;
  {
    diagnostics::Scope scope("graft_subdivision_build");
    for(auto members:runtime::graft_groups(source)) {
      auto old=std::find_if(grafts_.begin(),grafts_.end(),[&](const auto &g){return g.compatible(source,source_,members);});
      const bool rebuild=old==grafts_.end();bool changed=true;
      if(rebuild) grafts.emplace_back(source,members,final_render_);
      else {grafts.push_back(*old);grafts.back().rebind(members);changed=grafts.back().evaluate(source);}
      for(size_t part=0;part<members.size();++part) graft_bindings[members[part]]={int(grafts.size()-1),part};
      graft_topology_changed.push_back(rebuild);graft_positions_changed.push_back(changed);
    }
    for(size_t i=0;i<source.instances.size();++i) if(source.instances[i].prototype>=0) graft_bindings[i]=graft_bindings[size_t(source.instances[i].prototype)];
  }
  std::vector<bool> ordinary_mesh(source.meshes.size());
  for(size_t i=0;i<source.instances.size();++i) if(graft_bindings[i].group<0) ordinary_mesh[source.instances[i].mesh]=true;
  std::map<std::string,size_t> old_meshes;
  for(size_t i=0;i<source_.meshes.size();++i) old_meshes.emplace(source_.meshes[i].id,i);
  std::vector<runtime::Subdivision> subdivisions;std::vector<int> previous_mesh(source.meshes.size(),-1);
  std::vector<bool> topology_changed(source.meshes.size(),true);
  {
    diagnostics::Scope scope("subdivision_build");
    for(size_t i=0;i<source.meshes.size();++i) {
      if(auto old=old_meshes.find(source.meshes[i].id);old!=old_meshes.end()) {
        previous_mesh[i]=int(old->second);topology_changed[i]=!same_topology(source.meshes[i],source_.meshes[old->second]);
      }
      if(!ordinary_mesh[i]) subdivisions.emplace_back();
      else if(!topology_changed[i]&&subdivisions_.at(size_t(previous_mesh[i])).active()==(source.meshes[i].subdivision.enabled&&source.meshes[i].subdivision.level>0)) subdivisions.push_back(subdivisions_.at(size_t(previous_mesh[i])));
      else subdivisions.emplace_back(source.meshes[i],final_render_);
    }
  }
  std::vector<std::vector<ir::Vec3>> refined(source.meshes.size());
  for(size_t i=0;i<source.meshes.size();++i) if(subdivisions[i].active()&&(topology_changed[i]||source.meshes[i].positions!=source_.meshes[size_t(previous_mesh[i])].positions)) refined[i]=subdivisions[i].evaluate(source.meshes[i].positions);
  bool changed=!loaded_;
  std::vector<int> texture_map;
  for(auto texture:source.textures) {
    texture.id.clear();auto found=std::find(textures_.begin(),textures_.end(),texture);
    if(found==textures_.end()) {texture_map.push_back(int(textures_.size()));textures_.push_back(std::move(texture));}else texture_map.push_back(int(found-textures_.begin()));
  }
  std::vector<Shader *> shaders;std::vector<ir::Material> canonical;std::vector<float> bumps;
  std::set<Shader *> used_shaders,modified_shaders;
  for(auto value:source.materials) {
    for(auto *index:ir::texture_indices(value)) if(*index>=0) *index=texture_map.at(size_t(*index));
    int previous=-1;
    for(size_t j=0;j<canonical_materials_.size();++j) if(!used_shaders.contains(shaders_[j])&&canonical_materials_[j].id==value.id) {previous=int(j);break;}
    Shader *shader=previous>=0?shaders_[size_t(previous)]:nullptr;
    float bump=previous>=0?bump_distances_[size_t(previous)]:0;
    const bool modified=previous<0||canonical_materials_[size_t(previous)]!=value;
    if(modified&&value.bump_from_texel_density&&value.bump_texture>=0&&value.bump_strength>0) {
      double world=0,uv=0;const auto material_index=uint32_t(canonical.size());
      for(const auto &instance:source.instances) if(instance.prototype<0) for(const auto &t:source.meshes[instance.mesh].triangles) if(instance.materials[t.material_slot]==material_index) {
        const auto &p=source.meshes[instance.mesh].positions;const auto a=instance.transform.point(p[t.vertices[0]]),b=instance.transform.point(p[t.vertices[1]]),c=instance.transform.point(p[t.vertices[2]]);
        const double x1=b.x-a.x,y1=b.y-a.y,z1=b.z-a.z,x2=c.x-a.x,y2=c.y-a.y,z2=c.z-a.z;
        world+=std::hypot(y1*z2-z1*y2,z1*x2-x1*z2,x1*y2-y1*x2);
        uv+=std::abs(((t.uv[1].x-t.uv[0].x)*(t.uv[2].y-t.uv[0].y)-(t.uv[1].y-t.uv[0].y)*(t.uv[2].x-t.uv[0].x))*value.uv_scale.x*value.uv_scale.y);
      }
      const auto p=textures_[size_t(value.bump_texture)].file.u8string();auto image=OIIO::ImageInput::open(std::string(p.begin(),p.end()));
      if(image&&world>0&&uv>0) bump=float(2*std::sqrt(world/(uv*double(image->spec().width)*image->spec().height)));
    }
    if(!shader) {if(retired_shaders_.empty()) shader=scene_.create_node<Shader>();else {shader=retired_shaders_.back();retired_shaders_.pop_back();}}
    if(modified) {material(*shader,value,bump);modified_shaders.insert(shader);if(loaded_) ++stats_.material_updates;changed=true;}
    shaders.push_back(shader);canonical.push_back(std::move(value));bumps.push_back(bump);used_shaders.insert(shader);
  }
  using Key=std::pair<std::string,std::vector<Shader *>>;
  struct Existing {Mesh *mesh=nullptr;Hair *hair=nullptr;};
  std::map<Key,Existing> old_geometry,new_geometry;
  std::map<std::string,std::vector<Object *>> old_objects;
  std::set<Object *> retired_objects;std::set<Geometry *> retired_geometry;
  for(size_t i=0;i<source_.instances.size();++i) {
    const auto &instance=source_.instances[i];Key key;key.first=source_.meshes[instance.mesh].id;if(graft_bindings_[i].group>=0) key.first+="/graft-curves";for(auto m:instance.materials) key.second.push_back(shaders_[m]);
    old_objects[instance.id]=objects_[i];
    for(auto *object:objects_[i]) {
      auto *g=object->get_geometry();retired_objects.insert(object);retired_geometry.insert(g);
      if(g->is_mesh()) old_geometry[key].mesh=static_cast<Mesh *>(g);else if(g->is_hair()) old_geometry[key].hair=static_cast<Hair *>(g);
    }
  }
  std::vector<std::vector<Mesh *>> meshes(source.meshes.size());std::vector<std::vector<HairBinding>> hairs(source.meshes.size());
  std::vector<std::vector<Object *>> objects;std::vector<size_t> counts;
  for(const auto &m:source.meshes) counts.push_back(m.positions.size());
  stats_.meshes=stats_.curves=stats_.unique_triangles=stats_.triangles=0;
  for(const auto &instance:source.instances) {
    const auto index=instance.mesh;const auto &data=source.meshes[index];const auto &subdivision=subdivisions[index];
    Key key;key.first=data.id;if(graft_bindings[size_t(&instance-source.instances.data())].group>=0) key.first+="/graft-curves";for(auto m:instance.materials) key.second.push_back(shaders[m]);
    auto [it,inserted]=new_geometry.try_emplace(key);auto &geometry=it->second;
    if(inserted) {
      const auto old=old_geometry.find(key);if(old!=old_geometry.end()) geometry=old->second;
      const bool shader_changed=std::any_of(key.second.begin(),key.second.end(),[&](auto *s){return modified_shaders.contains(s);});
      const bool positions_changed=previous_mesh[index]<0||data.positions!=source_.meshes[size_t(previous_mesh[index])].positions;
      array<Node *> used(key.second.size());for(size_t m=0;m<used.size();++m) used[m]=key.second[m];
      if(!data.triangles.empty()&&graft_bindings[size_t(&instance-source.instances.data())].group<0) {
        const bool rebuild=topology_changed[index]||!geometry.mesh;
        if(!geometry.mesh) geometry.mesh=scene_.create_node<Mesh>();auto *mesh=geometry.mesh;retired_geometry.erase(mesh);
        if(rebuild||positions_changed||shader_changed) {
          const auto &triangles=subdivision.active()?subdivision.triangles():data.triangles;
          if(subdivision.active()&&refined[index].empty()) refined[index]=subdivision.evaluate(data.positions);
          const auto &points=subdivision.active()?refined[index]:data.positions;
          if(rebuild) {
            mesh->clear(true);const auto visible=std::count_if(triangles.begin(),triangles.end(),[&](const auto &t){return data.draws(t);});mesh->resize_mesh(int(points.size()),int(visible));
            auto *uv=mesh->attributes.add(ATTR_STD_UV,ustring("UVMap"))->data_for_write<float2>();size_t face=0;
            for(const auto &t:triangles) if(data.draws(t)) {for(size_t c=0;c<3;++c) {mesh->get_triangles()[face*3+c]=int(t.vertices[c]);uv[face*3+c]=make_float2(t.uv[c].x,t.uv[c].y);}mesh->get_shader()[face]=t.material_slot;mesh->get_smooth()[face]=data.smooth;++face;}
            ++stats_.topology_updates;
          }
          auto *positions=mesh->get_position_for_write();if(size_t(mesh->num_verts())!=points.size()) throw std::runtime_error("细分顶点数量与设备网格不一致");
          for(size_t v=0;v<points.size();++v) positions[v]=vector(points[v]);
          for(auto attribute:{ATTR_STD_VERTEX_NORMAL,ATTR_STD_POSITION_UNDISPLACED,ATTR_STD_NORMAL_UNDISPLACED,ATTR_STD_UV_TANGENT_UNDISPLACED,ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED}) mesh->attributes.remove(attribute);
          mesh->set_used_shaders(used);mesh->tag_position_modified();mesh->compute_bounds();mesh->tag_update(&scene_,rebuild);if(loaded_) ++stats_.geometry_updates;changed=true;
        }
        meshes[index].push_back(mesh);++stats_.meshes;stats_.unique_triangles+=mesh->num_triangles();
      } else geometry.mesh=nullptr;
      if(!data.curves.empty()) {
        const bool rebuild=topology_changed[index]||!geometry.hair;
        if(!geometry.hair) geometry.hair=scene_.create_node<Hair>();auto *hair=geometry.hair;retired_geometry.erase(hair);
        HairBinding binding;binding.hair=hair;for(const auto &curve:data.curves) binding.vertices.insert(binding.vertices.end(),curve.vertices.begin(),curve.vertices.end());
        if(rebuild||positions_changed||shader_changed) {
          hair->clear(true);hair->resize_curves(int(data.curves.size()),int(binding.vertices.size()));hair->curve_shape=CURVE_THICK;
          auto *positions=hair->get_position_for_write();auto *radii=hair->get_radius_for_write();auto *uv=hair->attributes.add(ATTR_STD_UV,ustring("UVMap"))->data_for_write<float2>();auto *intercept=hair->attributes.add(ATTR_STD_CURVE_INTERCEPT)->data_for_write<float>();size_t first=0;
          for(size_t c=0;c<data.curves.size();++c) {const auto &curve=data.curves[c];const auto &m=source.materials[instance.materials[curve.material_slot]];
            hair->get_curve_first_key()[c]=int(first);hair->get_curve_shader()[c]=int(curve.material_slot);uv[c]=make_float2(curve.uv.x,curve.uv.y);
            for(size_t k=0;k<curve.vertices.size();++k) {const float t=float(k)/float(curve.vertices.size()-1);positions[first]=vector(data.positions[curve.vertices[k]]);radii[first]=std::max(1e-8f,m.hair_root_radius*(1-t)+m.hair_tip_radius*t);intercept[first]=t;++first;}
          }
          hair->set_used_shaders(used);hair->tag_position_modified();hair->compute_bounds();hair->tag_update(&scene_,true);if(loaded_) ++stats_.geometry_updates;changed=true;
        }
        hairs[index].push_back(std::move(binding));stats_.curves+=data.curves.size();
      } else geometry.hair=nullptr;
    }
    objects.emplace_back();
    for(Geometry *g:std::array<Geometry *,2>{geometry.mesh,geometry.hair}) if(g) {
      Object *object=nullptr;if(auto old=old_objects.find(instance.id);old!=old_objects.end()) for(auto *o:old->second) if(o->get_geometry()==g&&retired_objects.contains(o)) {object=o;break;}
      if(!object) {object=scene_.create_node<Object>();object->name=ustring(instance.id);object->set_geometry(g);changed=true;}
      const auto matrix=transform(instance.transform);const auto visibility=instance.visible?PATH_RAY_VISIBILITY_ALL:0;
      if(object->get_tfm()!=matrix||object->get_visibility()!=visibility) {object->set_tfm(matrix);object->set_visibility(visibility);object->tag_update(&scene_);if(loaded_) ++stats_.instance_updates;changed=true;}
      objects.back().push_back(object);retired_objects.erase(object);
    }
    if(geometry.mesh) stats_.triangles+=geometry.mesh->num_triangles();
  }
  // 一个宿主组合使用一个 Cycles Object，使 SSS 可以跨越 GeoGraft 交界。
  std::map<std::string,GraftRender> assemblies;
  for(size_t i=0;i<source.instances.size();++i) if(graft_bindings[i].group>=0) {
    const auto &binding=graft_bindings[i];const auto &surface=grafts[size_t(binding.group)];const auto &object=source.instances[i];
    const auto placement=object.prototype<0?std::string("original"):object.instance_group.empty()?object.id:object.instance_group;
    const auto id=source.instances[surface.members()[0]].id+"/graft/"+placement;auto [it,inserted]=assemblies.try_emplace(id);auto &render=it->second;
    if(inserted) {render.id=id;render.group=size_t(binding.group);render.parts.assign(surface.members().size(),-1);}render.parts[binding.part]=int(i);
  }
  std::map<Key,Mesh *> old_graft_geometry,new_graft_geometry;
  for(const auto &render:graft_renders_) {old_graft_geometry[{render.geometry_key,render.shaders}]=render.mesh;retired_objects.insert(render.object);retired_geometry.insert(render.mesh);}
  std::vector<GraftRender> graft_renders;
  for(auto &[id,render]:assemblies) {
    const auto &surface=grafts[render.group];render.visible=graft_visibility(source,render.parts);render.geometry_key=source.instances[surface.members()[0]].id+"/graft";
    for(size_t p=0;p<render.parts.size();++p) {render.geometry_key+=render.visible[p]?"/1":"/0";const auto i=render.parts[p]<0?surface.members()[p]:uint32_t(render.parts[p]);for(auto m:source.instances[i].materials) render.shaders.push_back(shaders[m]);}
    Key key{render.geometry_key,render.shaders};auto [geometry,inserted]=new_graft_geometry.try_emplace(key,nullptr);
    if(inserted) {
      if(auto old=old_graft_geometry.find(key);old!=old_graft_geometry.end()) geometry->second=old->second;
      const bool rebuild=graft_topology_changed[render.group]||!geometry->second;
      if(!geometry->second) geometry->second=scene_.create_node<Mesh>();
      const bool shader_changed=std::any_of(render.shaders.begin(),render.shaders.end(),[&](auto *s){return modified_shaders.contains(s);});
      if(rebuild||graft_positions_changed[render.group]||shader_changed) {graft_mesh(scene_,*geometry->second,surface,source,render.parts,render.shaders,rebuild);if(rebuild) ++stats_.topology_updates;if(loaded_) ++stats_.geometry_updates;changed=true;}
      ++stats_.meshes;stats_.unique_triangles+=geometry->second->num_triangles();
    }
    render.mesh=geometry->second;retired_geometry.erase(render.mesh);
    auto old=std::find_if(graft_renders_.begin(),graft_renders_.end(),[&](const auto &r){return r.id==id;});
    render.object=old==graft_renders_.end()?scene_.create_node<Object>():old->object;
    if(old==graft_renders_.end()||render.object->get_geometry()!=render.mesh) {render.object->name=ustring(id);render.object->set_geometry(render.mesh);changed=true;}
    const auto matrix=transform(graft_transform(surface,source,render.parts));const auto visible=std::any_of(render.visible.begin(),render.visible.end(),[](bool v){return v;})?PATH_RAY_VISIBILITY_ALL:0;
    if(render.object->get_tfm()!=matrix||render.object->get_visibility()!=visible) {render.object->set_tfm(matrix);render.object->set_visibility(visible);render.object->tag_update(&scene_);if(loaded_) ++stats_.instance_updates;changed=true;}
    retired_objects.erase(render.object);stats_.triangles+=render.mesh->num_triangles();graft_renders.push_back(std::move(render));
  }
  for(auto *o:retired_objects) scene_.delete_node(o);
  for(auto *g:retired_geometry) scene_.delete_node(g);
  changed|=!retired_objects.empty()||!retired_geometry.empty();
  for(auto *shader:shaders_) if(!used_shaders.contains(shader)) {shader->set_graph(make_unique<ShaderGraph>());shader->tag_update(&scene_);scene_.delete_node(shader);retired_shaders_.push_back(shader);changed=true;}
  if(!loaded_) {auto graph=make_unique<ShaderGraph>();auto *emission=graph->create_node<EmissionNode>();emission->set_color(one_float3());emission->set_strength(1);graph->connect(emission->output("Emission"),graph->output()->input("Surface"));scene_.default_light->set_graph(std::move(graph));scene_.default_light->tag_update(&scene_);}
  if(source.lights!=source_.lights||!loaded_) {
    for(auto *o:light_objects_) scene_.delete_node(o);for(auto *l:lights_) scene_.delete_node(l);lights_.clear();light_objects_.clear();light_power_.clear();
    for(const auto &data:source.lights) {Light *light=nullptr;
      if(data.kind==ir::LightKind::point) light=scene_.create_node<PointLight>();
      else if(data.kind==ir::LightKind::spot) {auto *spot=scene_.create_node<SpotLight>();spot->set_angle(data.angle);light=spot;}
      else if(data.kind==ir::LightKind::distant) light=scene_.create_node<SunLight>();
      else {auto *area=scene_.create_node<AreaLight>();area->set_sizeu(data.width);area->set_sizev(data.height);light=area;}
      light_power_.push_back(data.power);light->set_strength(ir::scene_lights(source.options)?vector(data.power):zero_float3());light->set_use_mis(true);lights_.push_back(light);
      auto *object=scene_.create_node<Object>();object->set_geometry(light);object->set_tfm(transform(data.transform));light_objects_.push_back(object);
    }changed=true;
  }
  if(!loaded_||environment_!=source.environment||options_!=source.options) {environment_=source.environment;environment(source.options);changed=true;}
  const bool camera_changed=!loaded_||source_.camera.transform!=source.camera.transform||source_.camera.width!=source.camera.width||source_.camera.height!=source.camera.height||source_.camera.fov!=source.camera.fov;
  texture_map_=std::move(texture_map);shaders_=std::move(shaders);canonical_materials_=std::move(canonical);bump_distances_=std::move(bumps);subdivisions_=std::move(subdivisions);meshes_=std::move(meshes);hairs_=std::move(hairs);objects_=std::move(objects);vertex_counts_=std::move(counts);source_=std::move(saved);loaded_=true;
  grafts_=std::move(grafts);graft_bindings_=std::move(graft_bindings);graft_renders_=std::move(graft_renders);
  stats_.instances=source.instances.size();stats_.materials=source.materials.size();stats_.textures=source.textures.size();++stats_.scene_updates;
  if(camera_changed) {ir::Delta delta;delta.camera=source.camera;apply(delta);changed=true;}
  return changed;
}
