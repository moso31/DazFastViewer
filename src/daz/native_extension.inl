// 从实例引用读取原生扩展值；不可从共享资产目录的同名属性猜测实例归属。
using NativeChannels=std::map<std::string,std::map<std::string,std::vector<double>>>;
static std::string extension_name(const J &channel,const J &modifier=J::object()) {
  for(const auto &name:{channel.value("label",""),channel.value("name",""),modifier.value("name",""),channel.value("id","")})
    if(runtime::legacy_extension_channel(name))return name;
  return {};
}
static NativeChannels native_channels(const J &document,const fs::path &scene_file,const std::vector<fs::path> &roots,J &diagnostics) {
  NativeChannels result;std::map<std::string,const J *> definitions;
  for(const auto &m:array_member(document,"modifier_library"))definitions[m.value("id","")]=&m;
  auto collect=[&](const std::string &owner,const std::string &name,const J &channel) {
    if(owner.empty()||name.empty())return;
    const auto i=channel.find(channel.contains("current_value")?"current_value":"value");
    if(i!=channel.end()&&i->is_number())result[owner][name].push_back(i->get<double>());
  };
  for(const auto &instance:array_member(object_member(document,"scene"),"modifiers")) {
    const auto url=instance.value("url","");if(url.find('#')==std::string::npos)continue;
    const auto ref=reference(url);const auto owner=decode_uri(instance.value("parent",""));if(!owner.starts_with('#'))continue;
    const J *definition=nullptr;std::shared_ptr<const J> external;
    if(ref.file.empty()) {if(auto i=definitions.find(ref.id);i!=definitions.end())definition=i->second;}
    else {
      auto name=ref.id;const auto suffix=name.find_last_of('-');if(suffix!=std::string::npos&&suffix+1<name.size()&&std::all_of(name.begin()+suffix+1,name.end(),[](char c){return c>='0'&&c<='9';}))name.resize(suffix);
      if(!runtime::legacy_extension_channel(name)&&extension_name(object_member(instance,"channel"),instance).empty())continue;
      const auto path=resolve(ref.file,scene_file,roots);if(path.empty())continue;
      try{external=document_view(path);for(const auto &m:array_member(*external,"modifier_library"))if(m.value("id","")==ref.id){definition=&m;break;}}
      catch(const std::exception &e){diagnostics.push_back({{"type","native_extension"},{"owner",owner},{"message",e.what()}});continue;}
    }
    J channel=definition?object_member(*definition,"channel"):J::object();const auto name=extension_name(channel,definition?*definition:instance);
    const auto &override=object_member(instance,"channel");
    // 场景实例的 value 也应覆盖定义中保存的 current_value。
    if(override.contains("value")&&!override.contains("current_value"))channel.erase("current_value");
    channel.update(override);collect(owner,name.empty()?extension_name(channel,instance):name,channel);
  }
  for(const auto &node:array_member(object_member(document,"scene"),"nodes")) {
    for(const auto &extra:array_member(node,"extra"))if(extra.value("type","")=="studio_node_channels")
      for(const auto &entry:array_member(extra,"channels")){const auto &channel=object_member(entry,"channel");collect("#"+node.value("id",""),extension_name(channel),channel);}
  }
  return result;
}
static void import_native_extension(runtime::Target &target,const AssetObject &object,const NativeChannels &channels,J &diagnostics) {
  const bool character=object.figure&&object.conform_target.empty()&&(object.content_type.empty()||object.content_type=="Actor"||object.content_type.starts_with("Actor/"));
  std::map<std::string,std::vector<double>> values;
  for(const auto &owner:{"#"+object.id,"#"+object.geometry_instance_id})if(auto i=channels.find(owner);i!=channels.end())
    for(const auto &[name,v]:i->second)values[name].insert(values[name].end(),v.begin(),v.end());
  for(const auto &[name,v]:values) {
    if(v.empty()||(name=="Density")==character)continue;
    auto next=target.native_extension;const double value=v.front();
    if(!std::isfinite(value)||std::any_of(v.begin(),v.end(),[&](double x){return x!=value;})) {
      diagnostics.push_back({{"type","native_extension"},{"owner",object.id},{"channel",name},{"message","原生值无效或同名引用冲突，保留默认值"}});continue;
    }
    if(name=="Age")next.age=value;else if(name=="Age Step")next.age_step=value;else if(name=="Age Sensitivity")next.sensitivity=value;else if(name=="Body Strength")next.strength=value;else if(name=="Density")next.density=value;else continue;
    try{runtime::validate_extension(next);}catch(const std::exception &){diagnostics.push_back({{"type","native_extension"},{"owner",object.id},{"channel",name},{"value",value},{"message","原生值超出支持范围，保留默认值"}});continue;}
    next.kind=character?runtime::ExtensionKind::growth:runtime::ExtensionKind::density;target.native_extension=next;target.native_extension_channels.insert(name);
  }
}
