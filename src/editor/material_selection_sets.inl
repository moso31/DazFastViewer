std::vector<MaterialPanel::Surface> MaterialPanel::item_surfaces(QTreeWidgetItem *item) const {
  std::set<std::pair<size_t,size_t>> selected;
  std::function<void(QTreeWidgetItem *)> visit=[&](auto *node){
    if(node->data(0,Qt::UserRole+3).isValid()){for(const auto &slot:node->data(0,Qt::UserRole+3).toList())selected.emplace(size_t(node->data(0,Qt::UserRole).toULongLong()),size_t(slot.toULongLong()));return;}
    if(node->data(0,Qt::UserRole+1).isValid())selected.emplace(size_t(node->data(0,Qt::UserRole).toULongLong()),size_t(node->data(0,Qt::UserRole+1).toULongLong()));
    for(int i=0;i<node->childCount();++i)visit(node->child(i));
  };visit(item);std::vector<Surface> result;for(auto [i,s]:selected)result.push_back({i,s});return result;
}
void MaterialPanel::choose_selection_set(QTreeWidgetItem *item,bool additive) {
  const auto members=item_surfaces(item);std::set<std::pair<size_t,size_t>> selected;
  if(additive)for(auto s:surfaces())selected.emplace(s.instance,s.slot);
  const bool remove=additive&&std::all_of(members.begin(),members.end(),[&](auto s){return selected.contains({s.instance,s.slot});});
  for(auto s:members)if(remove)selected.erase({s.instance,s.slot});else selected.emplace(s.instance,s.slot);
  {QSignalBlocker block(tree_);tree_->clearSelection();tree_->setCurrentItem(item,0,QItemSelectionModel::NoUpdate);
    for(QTreeWidgetItemIterator it(tree_);*it;++it)if((*it)->data(0,Qt::UserRole+1).isValid())(*it)->setSelected(selected.contains({size_t((*it)->data(0,Qt::UserRole).toULongLong()),size_t((*it)->data(0,Qt::UserRole+1).toULongLong())}));
  }wheel_selected_.clear();rebuild_properties();
}
void MaterialPanel::add_selection_sets(QTreeWidgetItem *object,size_t instance) {
  const auto &scene=document_->loaded.scene;auto source=instance;if(scene.instances[instance].prototype>=0)source=size_t(scene.instances[instance].prototype);
  const auto found=std::find_if(document_->loaded.objects.begin(),document_->loaded.objects.end(),[&](const auto &o){return o.instance==source;});
  if(found==document_->loaded.objects.end()||found->material_selection_sets.empty())return;
  const auto &slots=scene.meshes.at(scene.instances.at(instance).mesh).material_slots;
  auto *root=new QTreeWidgetItem(object,{QStringLiteral("选择集")});root->setData(0,Qt::UserRole,qulonglong(instance));root->setData(0,Qt::UserRole+2,"sets/"+text(scene.instances[instance].id));root->setExpanded(true);
  std::map<std::string,const daz::MaterialSelectionSet *> definitions;for(const auto &set:found->material_selection_sets)definitions.try_emplace(set.name,&set);
  std::map<std::string,QTreeWidgetItem *> items;std::set<std::string> building;
  std::function<QTreeWidgetItem *(const std::string &)> build=[&](const auto &name)->QTreeWidgetItem *{
    if(items.contains(name))return items.at(name);if(!definitions.contains(name)||!building.insert(name).second)return nullptr;
    const auto &set=*definitions.at(name);auto *parent=set.parent.empty()?root:build(set.parent);if(!parent)parent=root;
    auto *item=new QTreeWidgetItem(parent,{text(set.name)});item->setData(0,Qt::UserRole,qulonglong(instance));item->setData(0,Qt::UserRole+2,"set/"+text(scene.instances[instance].id)+"/"+text(name));
    QVariantList members;for(const auto &member:set.materials){auto slot=std::find(slots.begin(),slots.end(),member);if(slot!=slots.end())members.push_back(QVariant::fromValue(qulonglong(slot-slots.begin())));}
    item->setData(0,Qt::UserRole+3,members);items[name]=item;building.erase(name);return item;
  };
  for(const auto &set:found->material_selection_sets)build(set.name);
  std::function<void(QTreeWidgetItem *)> combine=[&](auto *item){std::set<qulonglong> members;for(const auto &m:item->data(0,Qt::UserRole+3).toList())members.insert(m.toULongLong());
    for(int c=0;c<item->childCount();++c){combine(item->child(c));for(const auto &m:item->child(c)->data(0,Qt::UserRole+3).toList())members.insert(m.toULongLong());}
    QVariantList list;QStringList names;for(auto m:members){list.push_back(QVariant::fromValue(m));names.push_back(text(slots.at(size_t(m))));}item->setData(0,Qt::UserRole+3,list);item->setToolTip(0,names.join(", "));item->setDisabled(members.empty());
  };combine(root);
}
