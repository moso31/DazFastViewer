std::string MaterialPanel::uv_catalog_key(size_t instance) const {
  const auto &d=*document_;const auto &i=d.loaded.scene.instances.at(instance);const auto &mesh=d.loaded.scene.meshes.at(i.mesh);
  auto path=d.source_file.generic_u8string();std::string key(path.begin(),path.end());key+='\n'+std::to_string(d.asset_revision)+'\n'+i.id+'\n'+mesh.id+'\n'+std::to_string(mesh.positions.size());
  if(auto a=daz::instance_archive(d.loaded,instance))key+=a->identity();for(const auto &[surface,a]:d.material_archives)if(surface.first==i.id&&a)key+=a->identity();
  if(d.loaded.report.is_object())key+='\n'+d.loaded.report.value("content_roots",J::array()).dump();
  std::set<std::string> refs;for(auto m:i.materials)refs.insert(daz::material_uv_set(d.loaded.scene.materials.at(m)).uri);for(const auto &ref:refs)key+='\n'+ref;return key;
}
void MaterialPanel::populate_uv_combo(QComboBox *combo) {
  QSignalBlocker block(combo);combo->clear();if(!document_)return;const auto selected=surfaces();if(selected.empty())return;
  const auto &scene=document_->loaded.scene;auto current=daz::material_uv_set(scene.materials.at(scene.instances.at(selected.front().instance).materials.at(selected.front().slot)));
  bool mixed=false,waiting=false,prototype=false;std::set<size_t> instances;std::vector<daz::MaterialUVSet> common;bool first=true;QStringList warnings;
  for(auto s:selected){const auto &i=scene.instances.at(s.instance);mixed|=daz::material_uv_set(scene.materials.at(i.materials.at(s.slot))).uri!=current.uri;prototype|=i.prototype>=0;instances.insert(s.instance);}
  for(auto instance:instances){const auto key=uv_catalog_key(instance);auto found=uv_catalogs_.find(key);
    if(found==uv_catalogs_.end()){
      waiting=true;if(uv_pending_.insert(key).second){QPointer<MaterialPanel> guard(this);auto document=document_;
        QThreadPool::globalInstance()->start([guard,document,key,instance]{daz::MaterialUVCatalog catalog;
          try{daz::LoadOptions options;if(document->loaded.report.is_object())for(const auto &root:document->loaded.report.value("content_roots",J::array()))options.content_roots.push_back(std::filesystem::u8path(root.get<std::string>()));std::vector<std::shared_ptr<daz::SourceArchive>> sources;const auto &i=document->loaded.scene.instances.at(instance);for(size_t slot=0;slot<i.materials.size();++slot)sources.push_back(material_archive(*document,{instance,slot},daz::material_uv_set(document->loaded.scene.materials.at(i.materials[slot])).owner));catalog=daz::discover_material_uv_sets(document->loaded,instance,options,sources);}
          catch(const std::exception &e){catalog.warnings.push_back(e.what());}
          QMetaObject::invokeMethod(qApp,[guard,key,catalog=std::move(catalog)]()mutable{if(!guard)return;guard->uv_pending_.erase(key);guard->uv_catalogs_[key]=std::move(catalog);if(auto *control=guard->findChild<QComboBox *>("material/uv_set"))guard->populate_uv_combo(control);},Qt::QueuedConnection);
        });
      }continue;
    }
    for(const auto &warning:found->second.warnings)warnings.push_back(text(warning));
    if(first){common=found->second.sets;first=false;}else std::erase_if(common,[&](const auto &set){return std::none_of(found->second.sets.begin(),found->second.sets.end(),[&](const auto &other){return set.uri==other.uri;});});
  }
  if(mixed)combo->addItem(QStringLiteral("多值"));
  else if(current.uri.empty())combo->addItem(QStringLiteral("未记录 UV Set"));
  else if(waiting||std::none_of(common.begin(),common.end(),[&](const auto &set){return set.uri==current.uri;}))combo->addItem(text(current.label)+(waiting?QStringLiteral("（正在读取选项…）"):QStringLiteral("（不可用）")));
  if(!waiting)for(const auto &set:common){combo->addItem(text(set.label),text(J{{"uri",set.uri},{"owner",set.owner},{"label",set.label}}.dump()));combo->setItemData(combo->count()-1,text(daz::decode_uri(set.uri)),Qt::ToolTipRole);if(!mixed&&set.uri==current.uri)combo->setCurrentIndex(combo->count()-1);}
  combo->setEnabled(!waiting&&!prototype&&!common.empty()&&bool(uv_requested));
  QString tooltip=QStringLiteral("切换所选表面的 UV Set，保留贴图与材质参数。列表仅包含当前模型兼容的已安装资源。");
  if(prototype)tooltip=QStringLiteral("DAZ Instance 共用原型的 UV Set；请在原型对象上切换。");
  if(!mixed&&!current.uri.empty())tooltip+="\n"+text(daz::decode_uri(current.uri));
  if(!warnings.empty())tooltip+=QStringLiteral("\n有 %1 个资源未能列为可用选项：\n").arg(warnings.size())+warnings.mid(0,5).join("\n");
  if(!waiting&&common.empty())tooltip+=QStringLiteral("\n所选对象没有共同可用的 UV Set。");combo->setToolTip(tooltip);
}
