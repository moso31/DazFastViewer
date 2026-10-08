#include "editor/document.h"
#include "editor/object_hierarchy.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <cctype>

namespace dfv::editor {
namespace {
std::string node_id(const runtime::Target &t) {return t.id.substr(0,t.id.rfind('/'));}
size_t target_index(const Document &d,const std::string &node) {
  for(size_t t=0;t<d.catalog.targets.size();++t) if(node_id(d.catalog.targets[t])==node) return t;
  throw std::runtime_error("附件引用的对象不存在："+node);
}
daz::AssetObject &object(Document &d,const std::string &id) {
  for(auto &o:d.loaded.objects) if(o.id==id) return o;
  throw std::runtime_error("附件缺少对象元数据："+id);
}
const daz::AssetObject &object(const Document &d,size_t target) {
  for(const auto &o:d.loaded.objects) if(o.instance==d.catalog.targets.at(target).instance) return o;
  throw std::runtime_error("目标缺少对象元数据");
}
bool refers(const std::string &uri,const std::set<std::string> &nodes) {return uri.starts_with('#')&&nodes.contains(uri.substr(1));}
std::string genesis8_base(std::string base) {
  const auto pos=base.find("Genesis 8.1/");if(pos!=std::string::npos)base.replace(pos,12,"Genesis 8/");return base;
}
bool genesis8(const daz::AssetObject &o) {
  return o.figure&&o.conform_target.empty()&&genesis8_base(o.auto_fit_base).find("Genesis 8/")!=std::string::npos;
}
bool graft(const Document &d,const daz::AssetObject &o) {
  return !d.loaded.scene.meshes.at(d.loaded.scene.instances.at(o.instance).mesh).graft_vertex_pairs.empty();
}
std::string clothing_identity(const daz::AssetObject &o) {
  const auto path=o.geometry_file.lexically_normal().generic_u8string();std::string result(path.begin(),path.end());
  std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return char(std::tolower(c));});
  return result+"#"+o.geometry_id;
}
void rebuild_ancestors(Document &d) {
  std::map<std::string,std::string> parents;
  std::set<std::string> hidden;
  for(const auto &n:d.loaded.nodes) parents["#"+n.id]=n.parent;
  for(const auto &n:d.loaded.nodes)if(!n.group&&!n.visible&&std::none_of(d.loaded.objects.begin(),d.loaded.objects.end(),[&](const auto &o){return o.id==n.id;}))hidden.insert("#"+n.id);
  for(auto &t:d.catalog.targets) {
    t.ancestors.clear();t.ancestors_visible=true;std::set<std::string> seen;
    for(auto p=t.parent;!p.empty();) {
      if(!seen.insert(p).second) throw std::runtime_error("附件父子关系存在循环");
      t.ancestors_visible&=!hidden.contains(p);
      t.ancestors.push_back(p);auto found=parents.find(p);p=found==parents.end()?std::string{}:found->second;
    }
  }
}
std::string resolve_selection(Document &d,size_t host,const std::string &uri) {
  if(!daz::selection_reference(uri)) return uri;
  auto address=daz::decode_uri(uri);const bool by_id=address.starts_with("id://");
  address=address.substr(address.find("@selection")+10);
  if(address.empty()||address==":") return "#"+node_id(d.catalog.targets.at(host));
  if(!address.starts_with('/')||!address.ends_with(':')) throw std::runtime_error("尚不支持此附件目标地址："+uri);
  address=address.substr(1,address.size()-2);
  if(address.empty()||address.find_first_of("/#?:")!=std::string::npos) throw std::runtime_error("附件目标需要唯一骨骼名称："+uri);
  const auto &target=d.catalog.targets.at(host);const auto host_node=node_id(target);
  for(auto &skin:d.skeletons.skins) if(skin.instance==target.instance) {
    runtime::Joint *joint=nullptr;
    for(auto &j:skin.joints) if((by_id?j.id:j.name)==address) {
      if(joint) throw std::runtime_error("附件目标骨骼有歧义："+address);joint=&j;
    }
    if(!joint) throw std::runtime_error("目标角色缺少附件骨骼："+address);
    if(joint->scene_id.empty()) {
      joint->scene_id=host_node+"/attachment-bone/"+joint->id;
      if(std::any_of(d.loaded.nodes.begin(),d.loaded.nodes.end(),[&](const auto &n){return n.id==joint->scene_id;})) throw std::runtime_error("附件骨骼节点 ID 冲突");
      d.loaded.nodes.push_back({joint->scene_id,"#"+host_node,joint->label});
    }
    return "#"+joint->scene_id;
  }
  throw std::runtime_error("挂接骨骼需要带骨架的目标角色");
}
void compatible(const Document &d,const daz::AssetObject &f,size_t host,bool conform) {
  const auto &h=object(d,host);
  if(!f.preferred_base.empty()) {
    if(f.preferred_base!=h.auto_fit_base&&std::find(h.extended_bases.begin(),h.extended_bases.end(),f.preferred_base)==h.extended_bases.end()&&
       !(genesis8(h)&&genesis8_base(f.preferred_base)==genesis8_base(h.auto_fit_base)))
      throw std::runtime_error("附件需要 "+f.preferred_base+"，当前角色为 "+h.auto_fit_base+"；跨代 / 跨性别 AutoFit 尚未支持");
  } else if(conform) throw std::runtime_error("附件未声明 preferred_base，无法确认 Fit To 兼容性："+f.label);
  const auto &mesh=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(f.instance).mesh);
  const auto &body=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(h.instance).mesh);
  if(mesh.graft_target_vertices&&(mesh.graft_target_vertices!=body.positions.size()||(mesh.graft_target_polygons&&mesh.graft_target_polygons!=body.source_polygon_count)))
    throw std::runtime_error("GeoGraft 与目标角色的基础拓扑不匹配："+f.label);
}
void apply(Document &d,AttachmentBinding &binding,int host) {
  std::set<std::string> members;for(const auto &i:binding.items) members.insert(i.node);
  if(host>=0) {
    if(members.contains(node_id(d.catalog.targets.at(size_t(host))))) throw std::runtime_error("不能将附件绑定到自身或子对象");
    for(const auto &i:binding.items) if(daz::selection_reference(i.conform)||daz::selection_reference(i.parent)||daz::selection_reference(i.rigid))
      compatible(d,object(d,i.node),size_t(host),daz::selection_reference(i.conform));
    for(const auto &i:binding.items) if(daz::selection_reference(i.conform)) {
      const auto &o=object(d,i.node);const auto &mesh=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(o.instance).mesh);
      if(!mesh.graft_target_vertices) continue;
      for(const auto &other:d.loaded.objects) if(!members.contains(other.id)&&other.conform_target=="#"+node_id(d.catalog.targets.at(size_t(host)))) {
        const auto &g=d.loaded.scene.meshes.at(d.loaded.scene.instances.at(other.instance).mesh);
        for(auto polygon:mesh.graft_hidden_polygons) if(std::find(g.graft_hidden_polygons.begin(),g.graft_hidden_polygons.end(),polygon)!=g.graft_hidden_polygons.end())
          throw std::runtime_error("目标角色的同一区域已挂接 GeoGraft："+other.label+"；请先解除或移除原附件");
      }
    }
  }
  auto resolve=[&](const std::string &uri) {return host>=0?resolve_selection(d,size_t(host),uri):daz::selection_reference(uri)?std::string{}:uri;};
  const auto frame=host>=0?d.loaded.scene.instances.at(d.catalog.targets.at(size_t(host)).instance).transform:ir::Transform{};
  for(const auto &saved:binding.nodes) {
    const auto parent=resolve(saved.parent);
    for(auto &n:d.loaded.nodes) if(n.id==saved.id) {n.parent=parent;break;}
  }
  for(const auto &i:binding.items) {
    auto &o=object(d,i.node);auto &t=d.catalog.targets.at(target_index(d,i.node));
    o.parent=resolve(i.parent);o.conform_target=resolve(i.conform);o.smoothing.collision_target=resolve(i.collision);o.rigid_follow.target=resolve(i.rigid);
    if(host<0&&daz::selection_reference(i.conform)) {
      // 头皮可能在层级上位于发丝下面，而发丝 Fit To 头皮。
      // 解除宿主后将原来直接适配宿主的 Figure 作为独立根，避免继承形成反向循环。
      o.parent.clear();for(auto &n:d.loaded.nodes) if(n.id==i.node) n.parent.clear();
    }
    o.edit_frame=frame*i.edit_frame;o.translation_frame=frame*i.translation_frame;
    o.attachment_bind_rest=false;
    if(host>=0) {
      std::set<std::string> seen;
      for(auto parent=i.parent;!parent.empty()&&seen.insert(parent).second;) {
        if(daz::selection_reference(parent)) {o.attachment_bind_rest=parent.find("@selection/")!=std::string::npos;break;}
        auto n=std::find_if(binding.nodes.begin(),binding.nodes.end(),[&](const auto &n){return parent=="#"+n.id;});
        parent=n==binding.nodes.end()?std::string{}:n->parent;
      }
    }
    d.loaded.scene.instances.at(o.instance).transform=frame*i.transform;
    t.parent=o.parent;t.conform_target=o.conform_target;t.smoothing=o.smoothing;t.rigid_follow=o.rigid_follow;
    t.edit_frame=o.edit_frame;t.translation_frame=o.translation_frame;t.attachment_bind_rest=o.attachment_bind_rest;
  }
  binding.host=host>=0?node_id(d.catalog.targets.at(size_t(host))):std::string{};
  rebuild_ancestors(d);daz::apply_graft_masks(d.loaded);
  // 在发布文档前校验依赖、接缝及刚性挂接；不把求值网格写回基础资产。
  runtime::DeformationRuntime validate(d.loaded.scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);
  d.loaded.scene.validate();++d.asset_revision;
}
AttachmentBinding capture(Document &d,const std::set<std::string> &members,const std::set<std::string> &nodes) {
  AttachmentBinding b;
  for(const auto &n:d.loaded.nodes) if(nodes.contains(n.id)) b.nodes.push_back(n);
  for(const auto &o:d.loaded.objects) if(members.contains(o.id)) b.items.push_back({o.id,o.parent,o.conform_target,o.smoothing.collision_target,o.rigid_follow.target,d.loaded.scene.instances.at(o.instance).transform,o.edit_frame,o.translation_frame});
  return b;
}
}
size_t attachment_host(const Document &d,size_t selected) {
  std::set<size_t> seen;
  for(;;) {
    if(!seen.insert(selected).second) throw std::runtime_error("角色附件目标形成循环");
    const auto &t=d.catalog.targets.at(selected);const auto &o=object(d,selected);
    if(o.figure&&!o.auto_fit_base.empty()&&o.conform_target.empty()) return selected;
    std::string owner=t.conform_target.empty()?t.rigid_follow.target:t.conform_target;
    if(owner.empty()) {
      const auto &instance=d.loaded.scene.instances.at(t.instance);
      for(auto source:{instance.shell_source,instance.graft_source})if(source>=0) {
        const auto found=std::find_if(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &candidate){return candidate.instance==uint32_t(source);});
        if(found!=d.catalog.targets.end()){owner="#"+node_id(*found);break;}
      }
    }
    if(owner.empty()&&t.parent.starts_with('#'))for(const auto &candidate:d.catalog.targets)if(t.parent=="#"+node_id(candidate)){owner=t.parent;break;}
    if(owner.empty()) for(const auto &p:t.ancestors) {
      for(const auto &candidate:d.catalog.targets) if(p=="#"+node_id(candidate)) {owner=p;break;}
      if(!owner.empty()) break;
    }
    if(owner.empty()) throw std::runtime_error("请先选中兼容角色或其骨骼 / 已绑定附件");
    selected=target_index(d,owner.substr(1));
  }
}
void attach_import(Document &d,size_t first_target,size_t host) {
  host=attachment_host(d,host);
  std::set<std::string> imported,connected;
  for(size_t t=first_target;t<d.catalog.targets.size();++t) imported.insert(node_id(d.catalog.targets[t]));
  for(auto &o:d.loaded.objects) if(imported.contains(o.id)) {
    // 无选择引用的单件 Follower 也可手动保存成 wearable；已有内部绑定保持原样。
    if(o.parent.empty()&&o.conform_target.empty()&&o.content_type.starts_with("Follower/")) {
      o.parent="name://@selection:";if(o.figure) o.conform_target="name://@selection:";
      for(auto &n:d.loaded.nodes) if(n.id==o.id) n.parent=o.parent;
    }
    if(daz::selection_reference(o.parent)||daz::selection_reference(o.conform_target)||daz::selection_reference(o.rigid_follow.target)) connected.insert(o.id);
  }
  for(const auto &n:d.loaded.nodes) if(daz::selection_reference(n.parent)) connected.insert(n.id);
  bool changed=true;while(changed) {
    const auto count=connected.size();
    for(const auto &n:d.loaded.nodes) if(refers(n.parent,connected)) connected.insert(n.id);
    for(const auto &o:d.loaded.objects) if(imported.contains(o.id)&&(refers(o.parent,connected)||refers(o.conform_target,connected)||refers(o.rigid_follow.target,connected))) connected.insert(o.id);
    changed=count!=connected.size();
  }
  std::set<std::string> members;for(const auto &id:imported) if(connected.contains(id)) members.insert(id);
  if(members.empty()) throw std::runtime_error("穿戴预设没有可绑定到选中角色的附件");
  auto binding=capture(d,members,connected);apply(d,binding,int(host));d.attachments.push_back(std::move(binding));
  d.operations.push_back({{"op","attach"},{"first",d.catalog.targets.at(first_target).id},{"host",d.catalog.targets.at(host).id}});
}
void fit_attachment(Document &d,size_t follower,int host) {
  if(host>=0&&size_t(host)==follower) throw std::runtime_error("不能将附件绑定到自身");
  if(host>=0) host=int(attachment_host(d,size_t(host)));
  const auto id=node_id(d.catalog.targets.at(follower));
  auto record=[&]{d.operations.push_back({{"op","fit"},{"target",d.catalog.targets.at(follower).id},{"host",host>=0?d.catalog.targets.at(size_t(host)).id:std::string{}}});};
  for(auto &b:d.attachments) if(std::any_of(b.items.begin(),b.items.end(),[&](const auto &i){return i.node==id;})) {apply(d,b,host);record();return;}
  auto &root=object(d,id);
  if(!root.auto_fit_base.empty()||(!root.content_type.starts_with("Follower/")&&root.conform_target.empty())) throw std::runtime_error("请选择服装、头发或角色附件");
  std::set<std::string> connected{id};bool changed=true;
  while(changed) {
    const auto count=connected.size();for(const auto &n:d.loaded.nodes) if(refers(n.parent,connected)) connected.insert(n.id);
    for(const auto &o:d.loaded.objects) if(refers(o.conform_target,connected)) connected.insert(o.id);
    changed=count!=connected.size();
  }
  auto b=capture(d,connected,connected);
  std::string old;int old_host=-1;
  if(!root.conform_target.empty()||!root.parent.empty()) {old_host=int(attachment_host(d,follower));old="#"+node_id(d.catalog.targets.at(size_t(old_host)));}
  ir::Transform inverse_old;
  std::map<std::string,std::string> external;
  if(!old.empty()) {
    inverse_old=ir::inverse(d.loaded.scene.instances.at(d.catalog.targets.at(size_t(old_host)).instance).transform);external[old]="name://@selection:";
    for(const auto &skin:d.skeletons.skins) if(skin.instance==d.catalog.targets.at(size_t(old_host)).instance) {
      const auto bind=runtime::joint_transforms(skin,skin.initial);
      size_t nearest=d.catalog.targets[follower].ancestors.size(),bone=0;
      for(size_t j=0;j<skin.joints.size();++j) if(!skin.joints[j].scene_id.empty()) {
        const auto ref="#"+skin.joints[j].scene_id;external[ref]="name://@selection/"+skin.joints[j].name+":";
        const auto &ancestors=d.catalog.targets[follower].ancestors;const auto found=std::find(ancestors.begin(),ancestors.end(),ref);
        if(found!=ancestors.end()&&size_t(found-ancestors.begin())<nearest) {nearest=size_t(found-ancestors.begin());bone=j;}
      }
      if(root.conform_target.empty()&&!root.attachment_bind_rest&&nearest<d.catalog.targets[follower].ancestors.size()) inverse_old=ir::inverse(bind[bone])*inverse_old;
    }
    if(!root.parent.empty()&&!external.contains(root.parent)) for(const auto &p:d.catalog.targets[follower].ancestors) if(external.contains(p)) {external[root.parent]=external.at(p);break;}
  }
  for(auto &i:b.items) {
    i.transform=inverse_old*i.transform;i.edit_frame=inverse_old*i.edit_frame;i.translation_frame=inverse_old*i.translation_frame;
    for(auto *ref:{&i.parent,&i.conform,&i.collision,&i.rigid}) if(external.contains(*ref)) *ref=external.at(*ref);
    if(i.node==id&&root.parent.empty()&&root.conform_target.empty()) {i.parent="name://@selection:";if(root.figure) i.conform="name://@selection:";}
  }
  for(auto &n:b.nodes) {if(external.contains(n.parent)) n.parent=external.at(n.parent);else if(n.id==id&&n.parent.empty()) n.parent="name://@selection:";}
  apply(d,b,host);d.attachments.push_back(std::move(b));record();
}
std::vector<size_t> outfit_characters(const Document &d) {
  std::vector<size_t> result;for(size_t t=0;t<d.catalog.targets.size();++t)if(genesis8(object(d,t)))result.push_back(t);return result;
}
std::vector<size_t> outfit_clothing(const Document &d,size_t host) {
  std::vector<size_t> result;
  for(size_t t=0;t<d.catalog.targets.size();++t) {
    const auto &o=object(d,t);
    // DAZ 资源的分类和 Figure 标记并不可靠（例如袜子、鞋、睫毛和发型）。
    // 按实际挂接关系确定归属，Geograft 是唯一排除的穿戴种类。
    if(t==host||graft(d,o))continue;
    try {if(attachment_host(d,t)==host)result.push_back(t);}catch(const std::exception &){}
  }
  return result;
}
std::vector<size_t> outfit_roots(const Document &d,size_t host) {
  const auto clothing=outfit_clothing(d,host);
  // 使用场景父子层级，而非 Fit To：发丝可能 Fit To 自己下面的头皮。
  // 骨骼和无网格节点仍参与遍历，同一场景节点拆分的网格只显示一次。
  const ObjectHierarchy hierarchy(d,false);
  std::map<std::string,size_t> wearable;
  for(auto t:clothing)wearable.try_emplace(hierarchy.targets[t],t);
  std::vector<size_t> roots;
  for(auto t:clothing) {
    const auto &node=hierarchy.targets[t];if(wearable.at(node)!=t)continue;
    std::set<std::string> seen{node};auto parent=hierarchy.parents.find(node);bool nested=false;
    while(parent!=hierarchy.parents.end()&&!parent->second.empty()&&seen.insert(parent->second).second) {
      if(wearable.contains(parent->second)){nested=true;break;}
      parent=hierarchy.parents.find(parent->second);
    }
    if(!nested)roots.push_back(t);
  }
  return roots;
}
OutfitCopyResult copy_outfit(Document &d,Snapshot &snapshot,size_t source,const std::vector<size_t> &clothing,const std::vector<size_t> &hosts) {
  if(snapshot.generation!=d.generation)throw std::runtime_error("复制穿搭的场景状态已变化");
  const auto characters=outfit_characters(d),available=outfit_clothing(d,source);
  if(std::find(characters.begin(),characters.end(),source)==characters.end())throw std::runtime_error("请先选中 Genesis 8 / 8.1 角色");
  for(auto t:clothing)if(std::find(available.begin(),available.end(),t)==available.end())throw std::runtime_error("所选服装不属于当前角色");
  for(auto h:hosts)if(h==source||std::find(characters.begin(),characters.end(),h)==characters.end())throw std::runtime_error("复制目标必须是场景中的其他 Genesis 8 / 8.1 角色");
  const Document original=d;const Snapshot state=snapshot;OutfitCopyResult result;
  const auto source_node=node_id(original.catalog.targets.at(source));
  const auto inverse_host=ir::inverse(original.loaded.scene.instances.at(original.catalog.targets[source].instance).transform);
  std::map<std::string,std::string> external{{"#"+source_node,"name://@selection:"}};
  for(const auto &skin:original.skeletons.skins)if(skin.instance==original.catalog.targets[source].instance)
    for(const auto &joint:skin.joints)if(!joint.scene_id.empty())external["#"+joint.scene_id]="name://@selection/"+joint.name+":";
  std::set<size_t> distinct_hosts(hosts.begin(),hosts.end()),distinct_clothes(clothing.begin(),clothing.end());
  std::map<size_t,std::set<size_t>> copied_selection;
  for(auto host:distinct_hosts)for(auto root:distinct_clothes) {
    bool duplicate=false;
    std::map<std::string,std::string> existing;
    std::map<uint32_t,uint32_t> existing_instances{{original.catalog.targets[source].instance,d.catalog.targets[host].instance}};
    const auto worn=outfit_clothing(d,host);
    for(auto item:available)for(auto other:worn)if(clothing_identity(object(original,item))==clothing_identity(object(d,other))) {
      existing[object(original,item).id]=object(d,other).id;existing_instances[object(original,item).instance]=object(d,other).instance;
      if(item==root)duplicate=true;break;
    }
    if(duplicate){if(!copied_selection[host].contains(root))++result.skipped;continue;}
    std::set<std::string> nodes{node_id(original.catalog.targets[root])},members;
    std::set<std::string> eligible;for(auto t:available)eligible.insert(object(original,t).id);
    bool changed=true;while(changed) {
      const auto count=nodes.size();
      for(const auto &n:original.loaded.nodes)if(refers(n.parent,nodes)) {
        const auto o=std::find_if(original.loaded.objects.begin(),original.loaded.objects.end(),[&](const auto &o){return o.id==n.id;});
        if(o==original.loaded.objects.end()||eligible.contains(n.id))nodes.insert(n.id);
      }
      for(const auto &o:original.loaded.objects)if(eligible.contains(o.id)) {
        if(refers(o.parent,nodes)||refers(o.conform_target,nodes)||refers(o.rigid_follow.target,nodes))nodes.insert(o.id);
        // 发丝可 Fit To 头皮，挂件也可能依赖另一件附件；复制时不能仍引用来源角色。
        if(nodes.contains(o.id)) {
          for(const auto &ref:{o.conform_target,o.rigid_follow.target})if(ref.starts_with('#')&&eligible.contains(ref.substr(1))&&!existing.contains(ref.substr(1)))nodes.insert(ref.substr(1));
          const auto &instance=original.loaded.scene.instances[o.instance];
          if(instance.shell_source>=0)for(const auto &dependency:original.loaded.objects)if(dependency.instance==uint32_t(instance.shell_source)&&eligible.contains(dependency.id)&&!existing.contains(dependency.id))nodes.insert(dependency.id);
        }
      }
      changed=count!=nodes.size();
    }
    auto references=external;
    for(const auto &[from,to]:existing) {
      references["#"+from]="#"+to;nodes.erase(from);
      const auto source_instance=object(original,target_index(original,from)).instance;
      for(const auto &skin:original.skeletons.skins)if(skin.instance==source_instance)
        for(const auto &other:d.skeletons.skins)if(other.instance==existing_instances.at(source_instance))
          for(const auto &joint:skin.joints)if(!joint.scene_id.empty())for(const auto &match:other.joints)if(joint.name==match.name&&!match.scene_id.empty()) {
            references["#"+joint.scene_id]="#"+match.scene_id;nodes.erase(joint.scene_id);break;
          }
    }
    for(const auto &o:original.loaded.objects)if(nodes.contains(o.id)) {
      if(graft(original,o))nodes.erase(o.id);else members.insert(o.id);
    }
    // 服装可以位于宿主的空 Group 下；不保留指向来源角色 Group 的外部父链。
    for(const auto &t:original.catalog.targets)if(members.contains(node_id(t))&&!t.parent.empty()&&!refers(t.parent,nodes)&&!references.contains(t.parent))
      for(const auto &ancestor:t.ancestors)if(references.contains(ancestor)){references[t.parent]=references.at(ancestor);break;}
    // 同一次穿戴预设的其他单件不加入本次复制；每个勾选项单独查重、绑定。
    auto binding=capture(d,members,nodes);
    size_t serial=0;std::string prefix;
    do {prefix="outfit-"+std::to_string(serial++)+"/";}while(std::any_of(d.loaded.nodes.begin(),d.loaded.nodes.end(),[&](const auto &n){return n.id.starts_with(prefix);})||std::any_of(d.catalog.targets.begin(),d.catalog.targets.end(),[&](const auto &t){return t.id.starts_with(prefix);}));
    auto rename=[&](std::string &uri) {if(refers(uri,nodes))uri="#"+prefix+uri.substr(1);else if(references.contains(uri))uri=references.at(uri);};
    std::map<uint32_t,uint32_t> instances,materials;std::map<int,int> skins;
    for(const auto &o:original.loaded.objects)if(members.contains(o.id)) {
      const auto &old=original.loaded.scene.instances.at(o.instance);auto instance=old;
      auto mesh=original.loaded.scene.meshes.at(old.mesh);const auto old_mesh_id=mesh.id;mesh.id=prefix+mesh.id;
      instance.id=prefix+instance.id;instance.mesh=uint32_t(d.loaded.scene.meshes.size());instance.prototype=-1;instance.graft_source=-1;
      if(!instance.instance_node.empty())instance.instance_node=prefix+instance.instance_node;
      if(!instance.instance_group.empty())instance.instance_group=prefix+instance.instance_group;
      for(auto &m:instance.materials) {
        if(!materials.contains(m)){auto material=original.loaded.scene.materials.at(m);material.id=prefix+material.id;materials[m]=uint32_t(d.loaded.scene.materials.size());d.loaded.scene.materials.push_back(std::move(material));}
        m=materials.at(m);
      }
      instances[o.instance]=uint32_t(d.loaded.scene.instances.size());d.loaded.scene.meshes.push_back(std::move(mesh));d.loaded.scene.instances.push_back(std::move(instance));
      if(state.subdivision_levels.contains(old_mesh_id))snapshot.subdivision_levels[prefix+old_mesh_id]=state.subdivision_levels.at(old_mesh_id);
      if(state.material_overrides.contains(old.id))snapshot.material_overrides[prefix+old.id]=state.material_overrides.at(old.id);
      for(const auto &[key,archive]:original.material_archives)if(key.first==old.id)d.material_archives[{prefix+key.first,key.second}]=archive;
      auto copy=o;copy.id=prefix+copy.id;copy.instance=instances.at(o.instance);rename(copy.parent);rename(copy.conform_target);rename(copy.smoothing.collision_target);rename(copy.rigid_follow.target);
      if(copy.preferred_base.empty()&&daz::selection_reference(copy.conform_target))copy.preferred_base=object(original,source).auto_fit_base;
      d.loaded.objects.push_back(std::move(copy));
    }
    for(const auto &[old,index]:instances) {
      auto &instance=d.loaded.scene.instances.at(index);
      auto mapped=[&](int source) {if(source<0)return source;const auto id=uint32_t(source);if(instances.contains(id))return int(instances.at(id));if(existing_instances.contains(id))return int(existing_instances.at(id));throw std::runtime_error("穿戴的 Geometry Shell 依赖不完整");};
      instance.shell_source=mapped(instance.shell_source);instance.shell_root=mapped(instance.shell_root);
    }
    for(size_t s=0;s<original.skeletons.skins.size();++s)if(instances.contains(original.skeletons.skins[s].instance)) {
      auto skin=original.skeletons.skins[s];skin.id=prefix+skin.id;skin.instance=instances.at(skin.instance);for(auto &j:skin.joints)if(!j.scene_id.empty())j.scene_id=prefix+j.scene_id;
      skins[int(s)]=int(d.skeletons.skins.size());d.skeletons.skins.push_back(std::move(skin));snapshot.poses.push_back(state.poses.at(s));
    }
    for(size_t t=0;t<original.catalog.targets.size();++t)if(instances.contains(original.catalog.targets[t].instance)) {
      auto target=original.catalog.targets[t];target.id=prefix+target.id;target.instance=instances.at(target.instance);rename(target.parent);rename(target.conform_target);rename(target.smoothing.collision_target);rename(target.rigid_follow.target);
      auto graph=original.formulas.graphs.at(t);if(graph.skin>=0)graph.skin=skins.at(graph.skin);
      d.catalog.targets.push_back(std::move(target));d.formulas.graphs.push_back(std::move(graph));snapshot.values.push_back(state.values.at(t));
    }
    for(const auto &n:original.loaded.nodes)if(nodes.contains(n.id)){auto copy=n;copy.id=prefix+copy.id;rename(copy.parent);d.loaded.nodes.push_back(std::move(copy));}
    for(auto &i:binding.items) {
      auto inverse_frame=inverse_host;
      const auto target=target_index(original,i.node);const auto &o=object(original,target);
      if(o.conform_target.empty()&&!o.attachment_bind_rest)for(const auto &skin:original.skeletons.skins)if(skin.instance==original.catalog.targets[source].instance) {
        const auto bind=runtime::joint_transforms(skin,skin.initial);const auto &ancestors=original.catalog.targets[target].ancestors;
        size_t nearest=ancestors.size(),bone=0;
        for(size_t j=0;j<skin.joints.size();++j)if(!skin.joints[j].scene_id.empty()) {
          const auto found=std::find(ancestors.begin(),ancestors.end(),"#"+skin.joints[j].scene_id);
          if(found!=ancestors.end()&&size_t(found-ancestors.begin())<nearest){nearest=size_t(found-ancestors.begin());bone=j;}
        }
        if(nearest<ancestors.size())inverse_frame=ir::inverse(bind[bone])*inverse_host;
      }
      i.node=prefix+i.node;i.transform=inverse_frame*i.transform;i.edit_frame=inverse_frame*i.edit_frame;i.translation_frame=inverse_frame*i.translation_frame;
      for(auto *uri:{&i.parent,&i.conform,&i.collision,&i.rigid})rename(*uri);
    }
    for(auto &n:binding.nodes){n.id=prefix+n.id;rename(n.parent);}
    apply(d,binding,int(host));d.attachments.push_back(std::move(binding));
    for(auto selected:distinct_clothes)if(instances.contains(original.catalog.targets[selected].instance)&&copied_selection[host].insert(selected).second)++result.copied;
  }
  if(result.copied) {
    nlohmann::json clothes=nlohmann::json::array(),targets=nlohmann::json::array();for(auto t:clothing)clothes.push_back(original.catalog.targets.at(t).id);for(auto h:hosts)targets.push_back(original.catalog.targets.at(h).id);
    d.operations.push_back({{"op","copy_outfit"},{"source",original.catalog.targets.at(source).id},{"clothing",clothes},{"hosts",targets}});
    release_load_data(d);
  }
  return result;
}
}
