#include "editor/document.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>

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
void rebuild_ancestors(Document &d) {
  std::map<std::string,std::string> parents;
  for(const auto &n:d.loaded.nodes) parents["#"+n.id]=n.parent;
  for(auto &t:d.catalog.targets) {
    t.ancestors.clear();std::set<std::string> seen;
    for(auto p=t.parent;!p.empty();) {
      if(!seen.insert(p).second) throw std::runtime_error("附件父子关系存在循环");
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
    if(f.preferred_base!=h.auto_fit_base&&std::find(h.extended_bases.begin(),h.extended_bases.end(),f.preferred_base)==h.extended_bases.end())
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
    std::string owner=t.conform_target;
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
}
void fit_attachment(Document &d,size_t follower,int host) {
  if(host>=0&&size_t(host)==follower) throw std::runtime_error("不能将附件绑定到自身");
  if(host>=0) host=int(attachment_host(d,size_t(host)));
  const auto id=node_id(d.catalog.targets.at(follower));
  for(auto &b:d.attachments) if(std::any_of(b.items.begin(),b.items.end(),[&](const auto &i){return i.node==id;})) {apply(d,b,host);return;}
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
  apply(d,b,host);d.attachments.push_back(std::move(b));
}
}
