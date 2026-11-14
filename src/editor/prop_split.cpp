#include "editor/prop_split.h"
#include "editor/group_transforms.h"
#include "editor/node_properties.h"
#include "runtime/picking.h"
#include <algorithm>
#include <cctype>
#include <numeric>

namespace dfv::editor {
namespace {
int side_name(const std::string &name){
  if(name.size()>1&&(name[0]=='l'||name[0]=='r')&&(std::isupper(static_cast<unsigned char>(name[1]))||name[1]=='_'))return name[0]=='l'?-1:1;
  auto s=name;for(auto &c:s)c=char(std::tolower(static_cast<unsigned char>(c)));
  if(s.starts_with("left")||s.ends_with("_l")||s.ends_with(".l"))return -1;
  if(s.starts_with("right")||s.ends_with("_r")||s.ends_with(".r"))return 1;return 0;
}
std::vector<int> assignments(const PropSplitSource &source,const std::vector<PropSplitPart> &parts){
  std::vector<int> owners(source.regions.size(),-1);
  for(size_t p=0;p<parts.size();++p)for(int r:parts[p].regions){
    if(r<0||size_t(r)>=owners.size())throw std::runtime_error("拆分区域不存在");
    if(owners[r]>=0&&owners[r]!=int(p))throw std::runtime_error("同一区域不能同时属于多个部分");owners[r]=int(p);
  }return owners;
}
size_t find_target(const Document &d,const std::string &id){for(size_t t=0;t<d.catalog.targets.size();++t)if(d.catalog.targets[t].id==id)return t;throw std::runtime_error("拆分原件不存在："+id);}
ir::Mesh subset(const ir::Mesh &mesh,const PropSplitSource &source,const std::vector<int> &owners,int part){
  ir::Mesh out;out.smooth=mesh.smooth;out.material_slots=mesh.material_slots;out.polygon_groups=mesh.polygon_groups;out.prop_regions=source.regions;
  std::vector<int> vertices(mesh.positions.size(),-1);
  auto vertex=[&](uint32_t v){auto &mapped=vertices.at(v);if(mapped<0){mapped=int(out.positions.size());out.positions.push_back(mesh.positions.at(v));}return uint32_t(mapped);};
  for(size_t f=0;f<mesh.triangles.size();++f)if(owners.at(source.faces.at(f))==part){auto t=mesh.triangles[f];for(auto &v:t.vertices)v=vertex(v);out.triangles.push_back(t);out.prop_face_regions.push_back(source.faces[f]);}
  for(size_t c=0;c<mesh.curves.size();++c)if(owners.at(source.curves.at(c))==part){auto curve=mesh.curves[c];for(auto &v:curve.vertices)v=vertex(v);out.curves.push_back(std::move(curve));out.prop_curve_regions.push_back(source.curves[c]);}
  return out;
}
}
void capture_prop_regions(ir::Mesh &frozen,const ir::Mesh &source,const runtime::Skin *skin){
  frozen.polygon_groups=source.polygon_groups;
  if(!skin&&source.prop_regions.empty()&&(source.polygon_groups.empty()||std::all_of(source.polygon_groups.begin(),source.polygon_groups.end(),[](const auto &name){return name=="Default"||name=="default";})))return;
  PropSplitSource labels;
  if(!source.prop_regions.empty())labels=prop_split_source(source);
  else {
    if(skin)for(const auto &j:skin->joints)labels.regions.push_back({"bone/"+j.id,j.label.empty()?j.name:j.label,j.parent,side_name(j.name)?side_name(j.name):side_name(j.id)});
    for(size_t r=0;r<labels.regions.size();++r)if(!labels.regions[r].side){int p=labels.regions[r].parent;for(size_t depth=0;p>=0&&depth<labels.regions.size();++depth){const auto &parent=labels.regions.at(p);if(parent.side){labels.regions[r].side=parent.side;break;}p=parent.parent;}}
    std::map<uint32_t,int> groups;int unknown=-1;
    auto group=[&](uint32_t g){if(g<source.polygon_groups.size()){auto [it,added]=groups.try_emplace(g,int(labels.regions.size()));if(added){const auto &name=source.polygon_groups[g];labels.regions.push_back({"group/"+std::to_string(g),name,-1,side_name(name)});}return it->second;}
      if(unknown<0){unknown=int(labels.regions.size());labels.regions.push_back({"unknown","未识别区域",-1,0});}return unknown;};
    for(size_t f=0;f<source.triangles.size();++f){int r=skin?runtime::hit_joint(source,int(f),*skin):-1;if(r<0)r=group(source.triangles[f].polygon_group);labels.faces.push_back(r);}
    for(const auto &curve:source.curves){std::map<uint32_t,double> weights;if(skin)for(auto v:curve.vertices)if(v<skin->weights.size())for(const auto &w:skin->weights[v])weights[w.joint]+=w.weight;
      int r=-1;double maximum=0;for(auto [j,w]:weights)if(w>maximum){r=int(j);maximum=w;}labels.curves.push_back(r>=0?r:group(uint32_t(-1)));}
  }
  if(labels.regions.empty())return;
  frozen.prop_regions=labels.regions;frozen.prop_curve_regions=labels.curves;
  std::map<uint32_t,int> polygons;for(size_t f=0;f<source.triangles.size();++f)polygons.try_emplace(source.triangles[f].source_polygon,labels.faces[f]);
  frozen.prop_face_regions.clear();
  if(source.polygons.empty()){
    for(size_t f=0;f<source.triangles.size();++f)if(source.draws(source.triangles[f]))frozen.prop_face_regions.push_back(labels.faces[f]);
  }else for(const auto &t:frozen.triangles)frozen.prop_face_regions.push_back(polygons.at(t.source_polygon));
  if(frozen.prop_face_regions.size()!=frozen.triangles.size()||frozen.prop_curve_regions.size()!=frozen.curves.size())throw std::runtime_error("固定 Prop 时来源区域与最终几何不匹配");
}
PropSplitSource prop_split_source(const ir::Mesh &mesh){
  PropSplitSource out;
  if(!mesh.prop_regions.empty()){
    out.regions=mesh.prop_regions;out.faces=mesh.prop_face_regions;out.curves=mesh.prop_curve_regions;
    if(out.faces.size()!=mesh.triangles.size()||out.curves.size()!=mesh.curves.size())throw std::runtime_error("Prop 区域数量与几何不匹配");
    for(size_t i=0;i<out.regions.size();++i){const auto &r=out.regions[i];out.bones|=r.id.starts_with("bone/");int p=int(i);size_t depth=0;
      while(p>=0){if(size_t(p)>=out.regions.size()||++depth>out.regions.size())throw std::runtime_error("Prop 区域树包含无效父节点或循环");p=out.regions[p].parent;}}
    for(const auto *values:{&out.faces,&out.curves})for(int r:*values)if(r<0||size_t(r)>=out.regions.size())throw std::runtime_error("Prop 面引用了无效区域");return out;
  }
  // 旧 Prop 无来源标签时，仅依赖拓扑，不以当前 X 坐标冒充身体左右。
  std::vector<uint32_t> parent(mesh.positions.size());std::iota(parent.begin(),parent.end(),0u);
  auto root=[&](uint32_t v){while(parent.at(v)!=v){parent[v]=parent.at(parent[v]);v=parent[v];}return v;};
  auto link=[&](uint32_t a,uint32_t b){parent.at(root(a))=root(b);};
  for(const auto &f:mesh.triangles){link(f.vertices[0],f.vertices[1]);link(f.vertices[1],f.vertices[2]);}
  for(const auto &c:mesh.curves)for(size_t i=1;i<c.vertices.size();++i)link(c.vertices[0],c.vertices[i]);
  std::map<uint32_t,int> components;
  auto region=[&](uint32_t v){auto [it,added]=components.try_emplace(root(v),int(out.regions.size()));if(added)out.regions.push_back({"island/"+std::to_string(it->second),"独立网格块 "+std::to_string(it->second+1),-1,0});return it->second;};
  for(const auto &f:mesh.triangles)out.faces.push_back(region(f.vertices[0]));
  for(const auto &c:mesh.curves){if(c.vertices.empty())throw std::runtime_error("Prop 包含空曲线");out.curves.push_back(region(c.vertices[0]));}return out;
}
std::vector<PropSplitPart> prop_split_sides(const PropSplitSource &source){
  std::vector<PropSplitPart> result={{"左侧",{}},{"右侧",{}}};for(size_t r=0;r<source.regions.size();++r)if(int side=source.regions[r].side)result[side<0?0:1].regions.push_back(int(r));return result;
}
std::vector<size_t> prop_split_counts(const PropSplitSource &source,const std::vector<PropSplitPart> &parts){
  const auto owners=assignments(source,parts);std::vector<size_t> counts(parts.size()+1);
  for(const auto *values:{&source.faces,&source.curves})for(int r:*values){const auto p=owners.at(r);++counts[p<0?parts.size():size_t(p)];}return counts;
}
void restore_split_prop(Document &d,Snapshot &s,const nlohmann::json &op){
  const auto selected=find_target(d,op.at("source"));
  if(!d.catalog.targets[selected].static_prop)throw std::runtime_error("请先固定副本为 Prop");
  const auto operation_count=d.operations.size();
  for(const auto &part:op.at("parts")){
    const auto copies=copy_selection(d,s,{selected},{},{},false,part.at("prefix"));if(copies.size()!=1)throw std::runtime_error("请选择单个 Prop 网格进行拆分");
    const auto t=copies.front();auto &target=d.catalog.targets[t];auto &instance=d.loaded.scene.instances.at(target.instance);
    auto mesh=read_prop_mesh(part.at("mesh"));const auto old_mesh=d.loaded.scene.meshes.at(instance.mesh).id;s.subdivision_levels.erase(old_mesh);
    d.loaded.scene.meshes.at(instance.mesh)=std::move(mesh);instance.transform.value=part.at("transform").get<std::array<float,12>>();
    instance.prototype=instance.graft_source=instance.shell_source=instance.shell_root=-1;instance.shell_offset=0;
    target.label=part.at("label");target.parent.clear();target.ancestors.clear();target.has_edit_frame=true;
    target.edit_frame.value=part.at("frame").get<std::array<float,12>>();target.translation_frame={};target.base_rotation_degrees={};
    instance.instance_label=target.label;instance.instance_group.clear();s.instance_ground.erase(instance.id);
    s.values[t].transform={};s.values[t].visible=part.at("visible");instance.visible=s.values[t].visible;target.initial_visible=instance.visible;
    s.values[t].ground_alignment_offset_cm=0;s.values[t].ground_alignment_ratio=0;
    for(auto &o:d.loaded.objects)if(o.instance==target.instance){o.parent.clear();o.label=target.label;o.edit_frame=target.edit_frame;o.translation_frame={};o.rotation_degrees={};o.translation_cm={};o.scale={1,1,1};o.general_scale=1;}
    const auto node=target.id.substr(0,target.id.rfind('/'));for(auto &n:d.loaded.nodes)if(n.id==node){n.parent.clear();n.label=target.label;n.world=instance.transform;n.edit_frame=target.edit_frame;n.translation_frame={};n.rotation_degrees={};n.translation_cm={};n.scale={1,1,1};n.general_scale=1;}
  }
  // 保留隐藏原件，避免拆分时丢失原件下的附件或无法再调整分组。
  s.values[selected].visible=false;d.catalog.targets[selected].initial_visible=false;d.loaded.scene.instances[d.catalog.targets[selected].instance].visible=false;
  d.operations.erase(d.operations.begin()+operation_count,d.operations.end());d.operations.push_back(op);collect_resources(d);release_load_data(d);++d.asset_revision;++s.revision;d.loaded.scene.validate();
}
std::vector<size_t> split_static_prop(Document &d,Snapshot &s,size_t selected,const std::vector<PropSplitPart> &parts){
  if(s.generation!=d.generation)throw std::runtime_error("Prop 拆分的场景状态已变化");
  const auto target=d.catalog.targets.at(selected);if(!target.static_prop)throw std::runtime_error("请先固定副本为 Prop");
  const auto &reference=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(target.instance).mesh);const auto regions=prop_split_source(reference);const auto counts=prop_split_counts(regions,parts);
  if(parts.size()<2)throw std::runtime_error("至少需要两个拆分部分");if(counts.back())throw std::runtime_error("还有未分配区域，请先将所有区域归入拆分部分");
  std::set<std::string> names;for(size_t p=0;p<parts.size();++p){if(!counts[p])throw std::runtime_error("拆分部分不能为空");if(parts[p].label.empty()||!names.insert(parts[p].label).second)throw std::runtime_error("各部分需要不重复的名称");}
  auto source=transformed_groups(std::make_shared<Document>(d),s.group_transforms);auto scene=source->loaded.scene;
  runtime::DeformationRuntime deformation(scene,source->catalog.targets,source->skeletons.skins,source->formulas.graphs);deformation.evaluate(scene_properties(*source,s),s.poses);
  apply_instance_node_visibility(*source,s,scene);const ObjectHierarchy hierarchy(d);const GroupFrames frames(d,s.group_transforms);
  const auto bases=group_instance_bases(d,scene,hierarchy,deformation.effective_poses(),&frames);apply_instance_ground(scene,d.loaded.scene,s.instance_ground,nullptr,&bases);
  const auto &instance=scene.instances.at(target.instance);const auto &mesh=scene.meshes.at(instance.mesh);const auto owners=assignments(regions,parts);
  std::string prefix;size_t serial=0;bool occupied;do{prefix="split-"+std::to_string(serial++)+"/";occupied=false;for(const auto &n:d.loaded.nodes)occupied|=n.id.starts_with(prefix);for(const auto &i:d.loaded.scene.instances)occupied|=i.id.starts_with(prefix);}while(occupied);
  nlohmann::json op={{"op","split_prop"},{"source",target.id},{"parts",nlohmann::json::array()}};
  for(size_t p=0;p<parts.size();++p){auto piece=subset(mesh,regions,owners,int(p));const auto identity=prefix+std::to_string(p)+"/";piece.id=identity+target.id+"/mesh";
    ir::Bounds bounds;for(auto v:piece.positions)bounds.add(instance.transform.point(v));const auto center=ir::Vec3{(bounds.minimum.x+bounds.maximum.x)*.5f,(bounds.minimum.y+bounds.maximum.y)*.5f,(bounds.minimum.z+bounds.maximum.z)*.5f};
    op["parts"].push_back({{"prefix",identity},{"label",target.label+" · "+parts[p].label},{"mesh",prop_mesh_json(piece)},{"transform",instance.transform.value},{"frame",ir::Transform::translate(center).value},{"visible",instance.visible}});
  }
  const auto first=d.catalog.targets.size();restore_split_prop(d,s,op);std::vector<size_t> result;for(size_t t=first;t<d.catalog.targets.size();++t)result.push_back(t);return result;
}
}
