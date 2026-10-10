#include "water/document.h"
#include "editor/document.h"
#include "editor/group_transforms.h"
#include "editor/scene_extension.h"
#include "runtime/geometry_key.h"
#include "runtime/subdivision.h"
#include <thread>
#include <cctype>

namespace dfv::water {
std::vector<Candidate> candidates(const editor::Document &d,const std::vector<ir::Bounds> &instance_bounds){
  const editor::ObjectHierarchy hierarchy(d);std::map<std::string,ir::Bounds> bounds;
  const bool ready=instance_bounds.size()==d.loaded.scene.instances.size();
  auto add=[](ir::Bounds &to,const ir::Bounds &from){if(!from.empty){to.add(from.minimum);to.add(from.maximum);}};
  for(size_t i=0;i<d.loaded.scene.instances.size();++i){if(find(d.waters,d.loaded.scene.instances[i].id)||cloud::find(d.clouds,d.loaded.scene.instances[i].id))continue;
    auto &box=bounds[hierarchy.instances[i]];if(!ready)continue;add(box,instance_bounds[i]);
    for(const auto &group:hierarchy.groups)if(hierarchy.contains(group,hierarchy.instances[i]))add(bounds[group],instance_bounds[i]);
  }
  for(const auto &group:hierarchy.groups)bounds.try_emplace(group);
  std::vector<Candidate> result;for(const auto &[id,b]:bounds){double volume=0;
    if(!ready)volume=-1;else if(!b.empty)volume=(double(b.maximum.x)-b.minimum.x)*(double(b.maximum.y)-b.minimum.y)*(double(b.maximum.z)-b.minimum.z);
    const auto label=hierarchy.labels.find(id);result.push_back({id,(hierarchy.groups.contains(id)?"[组] ":"")+(label==hierarchy.labels.end()?id:label->second),volume});
  }return result;
}
Waters effective(const editor::Document &d,const editor::Snapshot &s){
  auto result=d.waters;for(auto &w:result)for(const auto &override:s.water_overrides)if(w->id==override->id){w=override;break;}return result;
}
bool materials(const Waters &waters,ir::Scene &scene,ir::Delta *delta){
  bool changed=false;for(const auto &w:waters){auto i=std::find_if(scene.instances.begin(),scene.instances.end(),[&](const auto &v){return v.id==w->id+"/surface";});if(i==scene.instances.end())continue;const auto index=i->materials.at(0);auto mat=material(*w);if(scene.materials.at(index)==mat)continue;scene.materials[index]=std::move(mat);if(delta){std::erase_if(delta->materials,[&](const auto &e){return e.index==index;});delta->materials.push_back({index,scene.materials[index]});}changed=true;}return changed;
}
void install(editor::Document &d,std::shared_ptr<const Water> w,bool record){
  validate(w->config);auto generated=mesh(*w);auto mat=material(*w);auto &s=d.loaded.scene;
  auto old=std::find_if(d.waters.begin(),d.waters.end(),[&](const auto &v){return v->id==w->id;});
  auto target=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.id==w->id+"/surface";});
  if(old!=d.waters.end()){
    if(target==d.catalog.targets.end())throw std::runtime_error("水体对象缺少场景绑定");auto &i=s.instances.at(target->instance);s.meshes.at(i.mesh)=std::move(generated);s.materials.at(i.materials.at(0))=std::move(mat);i.transform=ir::Transform::translate({float(w->config.x),float(w->config.y),float(w->config.level)});*old=w;
  }else{
    if(target!=d.catalog.targets.end()||std::any_of(s.instances.begin(),s.instances.end(),[&](const auto &i){return i.id==w->id+"/surface";}))throw std::runtime_error("水体身份冲突");
    ir::Instance i;i.id=w->id+"/surface";i.instance_node=w->id;i.instance_label="水体";i.mesh=uint32_t(s.meshes.size());i.materials={uint32_t(s.materials.size())};i.transform=ir::Transform::translate({float(w->config.x),float(w->config.y),float(w->config.level)});
    runtime::Target t;t.id=i.id;t.label="水体";t.instance=uint32_t(s.instances.size());t.initial_selectable=false;d.catalog.targets.push_back(t);d.formulas.graphs.emplace_back();
    daz::AssetNode n;n.id=w->id;n.label="水体";n.selectable=false;d.loaded.nodes.push_back(n);
    s.instances.push_back(i);s.meshes.push_back(std::move(generated));s.materials.push_back(std::move(mat));d.waters.push_back(w);
  }
  ++d.asset_revision;s.validate();if(record){nlohmann::json op={{"op","water"},{"water",json(*w)}};if(!d.operations.empty()&&d.operations.back().value("op","")=="water"&&d.operations.back().at("water").at("id")==w->id)d.operations.back()=std::move(op);else d.operations.push_back(std::move(op));}
}
uint64_t input_stamp(const editor::Document &d,const editor::Snapshot &snapshot){
  auto j=editor::snapshot_json(d,snapshot,false);nlohmann::json inputs={{"poses",j.at("poses")},{"groups",j.at("groups")},{"subdivision",j.at("subdivision")},{"instance_ground",j.at("instance_ground")},{"objects",nlohmann::json::object()},{"resources",nlohmann::json::array()}};
  for(auto &[id,o]:j["objects"].items())if(!cloud::find(d.clouds,id))inputs["objects"][id]={{"transform",o.at("transform")},{"visible",o.at("visible")},{"morphs",o.at("morphs")},{"extension",o.at("extension")},{"unlimited",o.at("unlimited")}};
  std::map<uint32_t,uint64_t> geometry;
  for(const auto &i:d.loaded.scene.instances){if(find(d.waters,i.id)||cloud::find(d.clouds,i.id))continue;const auto &m=d.loaded.scene.meshes[i.mesh];
    if(!geometry.contains(i.mesh)){runtime::GeometryKey shape;shape.points(m.positions);shape.topology(m);for(const auto &t:m.triangles)shape.add(uint64_t(t.material_slot));for(auto p:m.hidden_polygons)shape.add(uint64_t(p));geometry[i.mesh]=shape.value;}
    nlohmann::json opacity=nlohmann::json::array();for(auto mat:i.materials)opacity.push_back(d.loaded.scene.materials.at(mat).opacity);
    inputs["resources"].push_back({i.id,m.id,geometry.at(i.mesh),i.transform.value,opacity});
    if(auto object=snapshot.material_overrides.find(i.id);object!=snapshot.material_overrides.end())for(const auto &[slot,patch]:object->second)if(auto alpha=patch.find("opacity");alpha!=patch.end())inputs["opacity"][i.id][slot]=alpha->second;
  }
  runtime::GeometryKey h;h.add(inputs.dump());return h.value;
}
std::shared_ptr<const Cache> recalculate(const editor::Document &d,const editor::Snapshot &snapshot,const Water &water,const Progress &progress){
  if(progress)progress("读取当前姿态与对象变换");auto source=std::shared_ptr<const editor::Document>(&d,[](const auto *){});auto grouped=editor::transformed_groups(source,snapshot.group_transforms);ir::Scene scene=grouped->loaded.scene;
  runtime::DeformationRuntime runtime(scene,grouped->catalog.targets,grouped->skeletons.skins,grouped->formulas.graphs);
  for(;;){auto state=runtime.prepare(snapshot.values,snapshot.poses);if(!state.error.empty())throw std::runtime_error(state.error);if(!state.pending)break;if(progress)progress("等待参与对象的 Morph 数据");std::this_thread::sleep_for(std::chrono::milliseconds(5));}
  runtime.evaluate(snapshot.values,snapshot.poses);editor::apply_subdivision_levels(scene,snapshot.subdivision_levels);
  editor::apply_material_overrides(scene,d.loaded.scene,snapshot.material_overrides);
  const editor::GroupFrames frames(d,snapshot.group_transforms);const auto bases=editor::group_instance_bases(d,scene,frames.hierarchy,runtime.effective_poses(),&frames);editor::apply_instance_ground(scene,grouped->loaded.scene,snapshot.instance_ground,nullptr,&bases);
  return calculate(water,inputs(d,scene,water,progress),input_stamp(d,snapshot),progress);
}
std::shared_ptr<const Cache> calculate(const Water &water,const Inputs &input,uint64_t stamp,const Progress &progress){
  auto cache=std::make_shared<Cache>(*calculate(water,input.objects,stamp,progress));cache->warnings.insert(cache->warnings.end(),input.warnings.begin(),input.warnings.end());return cache;
}
Inputs inputs(const editor::Document &d,const ir::Scene &scene,const Water &water,const Progress &progress){
  auto own=std::find_if(scene.instances.begin(),scene.instances.end(),[&](const auto &i){return i.id==water.id+"/surface";});if(own==scene.instances.end())throw std::runtime_error("水体实例不存在");const auto inverse=ir::inverse(own->transform);const editor::ObjectHierarchy hierarchy(d);
  Inputs result;auto &obstacles=result.objects;auto &warnings=result.warnings;size_t triangles=0;
  for(const auto &s:water.config.sources)if(!hierarchy.parents.contains(s.id)&&std::none_of(scene.instances.begin(),scene.instances.end(),[&](const auto &i){return i.id==s.id;}))throw std::runtime_error("海岸线对象已移除："+s.id);
  for(size_t index=0;index<scene.instances.size();++index){const auto &i=scene.instances[index];if(find(d.waters,i.id)||cloud::find(d.clouds,i.id))continue;bool selected=water.config.scan_scene&&i.visible,volume=false,explicit_source=false;
    for(const auto &s:water.config.sources)if(s.id==i.id||hierarchy.contains(s.id,hierarchy.instances[index])){selected=true;volume=s.volume;explicit_source=true;break;}
    const auto &base=scene.meshes[i.mesh];if(!selected||base.triangles.empty())continue;
    const auto transform=inverse*i.transform;ir::Bounds bounds;for(auto p:base.positions)bounds.add(transform.point(p));const auto &c=water.config;
    if(bounds.empty||bounds.minimum.x>c.width*.5||bounds.maximum.x< -c.width*.5||bounds.minimum.y>c.length*.5||bounds.maximum.y< -c.length*.5||bounds.maximum.z< -c.wave_height-c.foam_width||bounds.minimum.z>c.wave_height+c.foam_width)continue;
    // Legacy water shaders are sometimes opaque/misconfigured. Also honor explicit
    // water surface slot names; WaterGround remains ordinary terrain. Manual sources
    // override this heuristic, so authored glass/other surfaces can still participate.
    std::vector<bool> named_water;for(const auto &slot:base.material_slots){auto name=slot;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char ch){return char(std::tolower(ch));});named_water.push_back(name=="water"||name=="matwater"||name=="water surface"||name=="watersurface"||name=="ocean"||name=="sea");}
    if(!explicit_source)for(size_t slot=0;slot<named_water.size();++slot)if(named_water[slot]&&scene.materials.at(i.materials.at(slot)).opacity>0)warnings.insert(warnings.begin(),"可见旧水面 "+i.id+" / "+base.material_slots[slot]+" 已排除出交界计算；请隐藏该材质，避免遮挡新水面与泡沫。");
    auto draws=[&](const ir::Triangle &t){const auto &m=scene.materials.at(i.materials.at(t.material_slot));return base.draws(t)&&(volume||m.opacity>0)&&(explicit_source||(!named_water.at(t.material_slot)&&!m.water&&m.transmission<.5f));};
    if(std::none_of(base.triangles.begin(),base.triangles.end(),draws))continue;
    if(progress)progress("准备参与对象："+i.id);Obstacle o;o.id=i.id;o.transform=transform;o.volume=volume;
    // Collision sampling has metre-scale spacing; rendering subdivisions on centimetre
    // sized character faces only consume memory without adding sampled shore detail.
    double edge=0;for(const auto &t:base.triangles)if(draws(t)){ir::Vec3 p[3];for(int k=0;k<3;++k)p[k]=transform.point(base.positions[t.vertices[k]]);if(std::min({p[0].z,p[1].z,p[2].z})>c.wave_height+c.foam_width||std::max({p[0].z,p[1].z,p[2].z})< -c.wave_height-c.foam_width)continue;for(int k=0;k<3;++k)edge=std::max(edge,std::hypot(double(p[k].x-p[(k+1)%3].x),double(p[k].y-p[(k+1)%3].y)));}
    int level=0;size_t estimate=base.triangles.size();const int authored=base.subdivision.enabled?base.subdivision.level:0;
    while(level<authored&&edge>c.precision*.5&&estimate<=(3000000-triangles)/4){++level;estimate*=4;edge*=.5;}
    runtime::Subdivision sub;if(level>0){auto cage=base;cage.subdivision.level=level;sub=runtime::Subdivision(cage);}
    o.mesh.positions=sub.active()?sub.evaluate(base.positions):base.positions;
    const auto &faces=sub.active()?sub.triangles():base.triangles;
    // Only the wave/foam height band can meet the surface. In particular, merged
    // island-tree assets must not bring every leaf above the sea into the BVH.
    // Volumes keep ALL faces because inside/outside parity requires a closed shell.
    auto contact=[&](const ir::Triangle &t){if(!draws(t))return false;ir::Vec3 p[3];ir::Bounds b;for(int k=0;k<3;++k){p[k]=transform.point(o.mesh.positions[t.vertices[k]]);b.add(p[k]);}
      const double ax=double(p[1].x)-p[0].x,ay=double(p[1].y)-p[0].y,az=double(p[1].z)-p[0].z,bx=double(p[2].x)-p[0].x,by=double(p[2].y)-p[0].y,bz=double(p[2].z)-p[0].z;
      const double nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx;if(nx*nx+ny*ny+nz*nz<1e-20)return false;
      if(volume)return true;const double band=c.wave_height+c.foam_width;return b.minimum.z<=band&&b.maximum.z>=-band&&b.minimum.x<=c.width*.5+band&&b.maximum.x>=-c.width*.5-band&&b.minimum.y<=c.length*.5+band&&b.maximum.y>=-c.length*.5-band;};
    const auto count=size_t(std::count_if(faces.begin(),faces.end(),contact));
    if(count==0)continue;
    if(level<authored)warnings.push_back(i.id+"：交界使用 "+std::to_string(level)+" 级细分（渲染为 "+std::to_string(authored)+" 级），按交界采样间距和视口预算限制。");
    if(count>3000000-triangles)throw std::runtime_error("水面交界带超过 300 万三角形预算："+i.id+"。请指定简化交界代理或缩小参与范围。");
    o.mesh.triangles.reserve(count);for(const auto &t:faces)if(contact(t))o.mesh.triangles.push_back(t);
    for(auto material:i.materials)if(scene.materials.at(material).displacement_texture>=0&&scene.materials.at(material).displacement_strength!=0&&base.displacement_rest.empty()){warnings.push_back(i.id+"：材质置换未参与交界，请使用已置换的几何代理。");break;}
    if(o.mesh.triangles.empty())continue;triangles+=o.mesh.triangles.size();if(triangles>3000000)throw std::runtime_error("参与对象超过 300 万三角形，请指定简化交界代理或缩小对象列表");obstacles.push_back(std::move(o));
  }
  return result;
}
bool adapt(const Waters &waters,ir::Scene &scene,ir::Vec3 world_eye,const Progress &progress){
  Runtime runtime;return runtime.adapt(waters,scene,world_eye,progress);
}
bool Runtime::adapt(const Waters &waters,ir::Scene &scene,ir::Vec3 world_eye,const Progress &progress){
  std::erase_if(meshes_,[&](const auto &v){return !find(waters,v.first);});
  bool changed=false;for(const auto &w:waters){auto i=std::find_if(scene.instances.begin(),scene.instances.end(),[&](const auto &v){return v.id==w->id+"/surface";});if(i==scene.instances.end()||!i->visible)continue;const auto generated=cached_mesh(*w,ir::inverse(i->transform).point(world_eye),meshes_[w->id],progress);auto &old=scene.meshes.at(i->mesh);if(old.positions!=generated->positions||old.triangles!=generated->triangles||old.water_foam!=generated->water_foam){old=*generated;changed=true;}}
  return changed;
}
}
