// 位于 load() 内：在宿主与插件全部建立之后再实例化派生表面。
if(!shells.empty()) {
  apply_graft_masks(out,options.defer_selection);
  for(const auto &pending:shells) {
    const auto &node=nodes.at(pending.id);const auto geometry_id=pending.instance.at("id").get<std::string>();
    std::map<std::string,Json> channels;
    for(const auto &e:array_member(node,"extra"))for(const auto &entry:array_member(e,"channels"))if(entry.contains("channel"))channels[entry["channel"].at("id").get<std::string>()]=entry["channel"];
    std::string target=decode(node.value("parent",""));
    if(auto c=channels.find("Shell Node");c!=channels.end()&&c->second.contains("node")&&c->second["node"].is_string())target=decode(c->second["node"].get<std::string>());
    std::vector<size_t> members;
    for(size_t i=0;i<out.objects.size();++i)if(target=="#"+out.objects[i].id)members.push_back(i);
    if(members.size()!=1)fail("Geometry Shell 宿主几何缺失或不唯一："+pending.id+" -> "+target);
    for(size_t p=0;p<members.size();++p)for(size_t i=0;i<out.objects.size();++i)
      if(scene.instances[out.objects[i].instance].graft_source==int(out.objects[members[p]].instance)&&std::find(members.begin(),members.end(),i)==members.end())members.push_back(i);
    float offset=0;
    for(const auto &instance:array_member(source,"modifiers"))if(decode(instance.value("parent",""))=="#"+pending.id||decode(instance.value("parent",""))=="#"+geometry_id) {
      Json modifier=instance;
      if(instance.contains("url")){const auto [path,base]=repo.asset(instance.at("url"),file,"modifier_library");modifier=merge_node(*base,instance);}
      bool push=false;float amount=number(object_member(modifier,"channel"),0);
      for(const auto &e:array_member(modifier,"extra")){
        push|=e.value("type","")=="studio/modifier/push";
        for(const auto &entry:array_member(e,"channels")){const auto &c=object_member(entry,"channel");if(c.value("id","")=="Value")amount=number(c,amount);}
      }
      if(push)offset+=amount*.01f;
    }
    if(!std::isfinite(offset))fail("Geometry Shell 偏移无效："+pending.id);
    auto visible=[&](const std::string &id){const auto c=channels.find(id);return c==channels.end()||number(c->second,1)!=0;};
    std::map<uint32_t,uint32_t> derived;
    for(size_t part=0;part<members.size();++part) {
      const auto object=out.objects[members[part]];const auto original=scene.instances.at(object.instance);auto mesh=scene.meshes.at(original.mesh);
      mesh.id=pending.id+"/"+geometry_id+"/shell/"+std::to_string(part);mesh.shell_hidden_polygons.clear();
      ir::Instance shell;shell.id=pending.id+"/"+geometry_id+"-part-"+std::to_string(part);shell.transform=render_transform(fitted_world(pending.id));shell.visible=hierarchy_visible(pending.id);shell.shell_source=int(object.instance);shell.shell_offset=offset;
      shell.shell_root=int(out.objects[members.front()].instance);
      std::vector<bool> material_visible;std::map<std::string,std::vector<size_t>> uv_slots;
      for(size_t slot=0;slot<mesh.material_slots.size();++slot) {
        const auto name=(part?object.id+"_":std::string{})+mesh.material_slots[slot];mesh.material_slots[slot]=name;
        const auto binding=bindings.find({"#"+geometry_id,name});shell.materials.push_back(binding==bindings.end()?original.materials[slot]:binding->second.material);
        material_visible.push_back(visible("material_group_"+name+"_vis"));
        if(binding!=bindings.end()&&!binding->second.uv.empty())uv_slots[binding->second.uv].push_back(slot);
      }
      // 显式 UV 引用优先；没有覆盖时继承宿主对应表面的 UV。
      for(const auto &[uri,slots]:uv_slots) {
        const auto [uv_file,u]=repo.asset(uri,uri.starts_with('#')?pending.file:file,"uv_set_library");
        if(u->at("vertex_count").get<size_t>()!=mesh.positions.size())fail("Geometry Shell UV 顶点数不匹配："+uri);
        std::vector<ir::Vec2> coords;for(const auto &p:values(u->at("uvs")))coords.push_back({p.at(0).get<float>(),p.at(1).get<float>()});
        std::map<std::pair<uint32_t,uint32_t>,uint32_t> seams;for(const auto &s:array_member(*u,"polygon_vertex_indices"))seams[{s.at(0).get<uint32_t>(),s.at(1).get<uint32_t>()}]=s.at(2).get<uint32_t>();
        auto uv=[&](uint32_t p,uint32_t v){const auto s=seams.find({p,v});return coords.at(s==seams.end()?v:s->second);};
        for(auto &t:mesh.triangles)if(std::find(slots.begin(),slots.end(),t.material_slot)!=slots.end())for(size_t c=0;c<3;++c)t.uv[c]=uv(t.source_polygon,t.vertices[c]);
        for(uint32_t p=0;p<mesh.polygons.size();++p){auto &face=mesh.polygons[p];if(std::find(slots.begin(),slots.end(),face.material_slot)!=slots.end())for(size_t c=0;c<face.vertices.size();++c)face.uv[c]=uv(p,face.vertices[c]);}
      }
      for(auto &name:mesh.polygon_groups)if(part)name="WG_"+std::to_string(part-1)+"_"+name;
      for(const auto &t:mesh.triangles)if(!material_visible.at(t.material_slot)||(t.polygon_group<mesh.polygon_groups.size()&&!visible("facet_group_"+mesh.polygon_groups[t.polygon_group]+"_vis")))mesh.shell_hidden_polygons.push_back(t.source_polygon);
      std::sort(mesh.shell_hidden_polygons.begin(),mesh.shell_hidden_polygons.end());mesh.shell_hidden_polygons.erase(std::unique(mesh.shell_hidden_polygons.begin(),mesh.shell_hidden_polygons.end()),mesh.shell_hidden_polygons.end());
      shell.mesh=uint32_t(scene.meshes.size());const auto index=uint32_t(scene.instances.size());derived[object.instance]=index;
      if(original.graft_source>=0&&derived.contains(uint32_t(original.graft_source)))shell.graft_source=int(derived.at(uint32_t(original.graft_source)));
      scene.meshes.push_back(std::move(mesh));scene.instances.push_back(std::move(shell));
      out.objects.push_back({index,pending.id,node.value("label",pending.id),target,pending.geometry->at("id").get<std::string>(),pending.file,false,geometry_id});
      geometry_reports.push_back({{"id",geometry_id},{"geometry_shell",true},{"source",original.id},{"vertices",scene.meshes.back().positions.size()},{"offset_m",offset}});
    }
  }
  apply_graft_masks(out,options.defer_selection);runtime::update_geometry_shells(scene,{},true);
}
