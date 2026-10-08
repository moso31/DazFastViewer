static void joint_region_selection() {
  runtime::Skin skin;
  auto bone=[&](const char *id,int parent) {runtime::Joint j;j.id=id;j.parent=parent;skin.joints.push_back(j);};
  bone("figure",-1);bone("lFoot",0);bone("lToe",1);bone("lBigToe",2);bone("lBigToe2",3);
  bone("lSmallToe1",2);bone("rFoot",0);bone("rToe",6);bone("rBigToe",7);bone("minorToeControl",2);
  skin.joints[2].aliases={"LeftToesGroup"};
  ir::Mesh mesh;mesh.polygon_groups={"LeftToesGroup","rToe","lFoot","lBigToe2","unmapped"};
  auto face=[&](uint32_t group,int joint,float x) {
    const auto v=uint32_t(mesh.positions.size());mesh.positions.insert(mesh.positions.end(),{{x,10,2},{x+1,10,2},{x,11,3}});
    ir::Triangle t;t.vertices={v,v+1,v+2};t.polygon_group=group;t.source_polygon=uint32_t(mesh.triangles.size());mesh.triangles.push_back(t);
    for(int i=0;i<3;++i) skin.weights.push_back({{uint32_t(joint),1}});
  };
  face(0,2,2);face(0,3,4);face(0,4,6);face(0,5,8);face(1,8,-10);
  face(2,4,12); // 明确的脚部组不能因权重落入脚趾组。
  face(3,4,14);face(4,4,16);
  for(size_t v=0;v<3;++v) skin.weights[v]={{2,.9},{9,.1}};
  const auto parts=runtime::joint_regions(mesh,skin);
  require(parts.body==std::vector<int>({2,2,2,2,7,1,4,4})&&parts.detail==std::vector<int>({2,3,4,5,8,4,4,4}),"共享多边形组没有按边界及子骨骼权重细分");
  std::vector<runtime::JointRegions> regions={parts,parts};
  auto click=[&](runtime::PickHit hit,runtime::HoverRegion active) {return runtime::selection_region(hit,active,{},regions,false);};
  auto selected=click({0,2},{});require(selected.instance==0&&selected.joint==-1,"首次脚趾点击没有选中角色");
  selected=click({0,2},selected);require(selected.joint==2,"第二次点击没有先选 Left Toes");
  selected=click({0,2},selected);require(selected.joint==4,"第三次点击不能进入脚趾末节");
  require(click({0,3},selected).joint==5,"已进入细选后不能切换同组脚趾");
  require(click({0,4},selected).joint==7&&click({1,2},selected).joint==-1,"脚趾细选上下文泄漏到另一只脚或另一角色");
  std::vector<runtime::HoverRegion> multi={{0,8},{0,4},{1,-1}};
  require(runtime::selection_region({0,3},{1,-1},multi,regions,true).joint==5,"Ctrl 多选没有保留命中脚趾组的细选层级");
  std::reverse(multi.begin(),multi.end());
  require(runtime::selection_region({0,3},{1,-1},multi,regions,true).joint==5,"脚趾多选受集合顺序影响");
  multi={{0,8},{1,4}};require(runtime::selection_region({0,2},{1,4},multi,regions,true).joint==2,"Ctrl 借用了另一只脚或另一角色的三级选择");
  const auto world=ir::Transform::translate({100,-20,5});
  auto bounds=runtime::joint_region_bounds(mesh,world,parts,3,&skin);
  require(!bounds.empty&&bounds.minimum==ir::Vec3{104,-10,7}&&bounds.maximum==ir::Vec3{117,-9,8},"父脚趾聚焦没有包含后代或丢失世界变换");
  bounds=runtime::joint_region_bounds(mesh,world,parts,9,&skin);
  require(!bounds.empty&&bounds.minimum==ir::Vec3{102,-10,7}&&bounds.maximum==ir::Vec3{103,-9,8},"非最大权重子骨骼聚焦仍退回骨骼原点");
  mesh.hidden_polygons={2,5,6,7};bounds=runtime::joint_region_bounds(mesh,world,parts,4,&skin);
  require(bounds.empty,"隐藏面仍参与子部位聚焦");
  mesh.hidden_polygons.clear();for(auto &p:mesh.positions) p.z+=4;
  bounds=runtime::joint_region_bounds(mesh,world,parts,4,&skin);require(bounds.minimum.z==11&&bounds.maximum.z==12,"聚焦沿用了未变形的绑定网格");
  runtime::JointRegions cycle;cycle.parents={1,0};require(!cycle.within(0,2),"异常骨架祖先查询没有终止");
  runtime::Skin head;runtime::Joint neck,skull,eye;neck.id="neck";skull.id="head";skull.parent=0;eye.id="eye";eye.parent=1;head.joints={neck,skull,eye};head.weights.assign(3,{{0,.01},{1,.09},{2,.9}});
  ir::Mesh neck_mesh;neck_mesh.positions.assign(mesh.positions.begin(),mesh.positions.begin()+3);neck_mesh.polygon_groups={"neck"};neck_mesh.triangles={mesh.triangles[0]};
  const auto head_parts=runtime::joint_regions(neck_mesh,head);require(head_parts.body[0]==0&&head_parts.detail[0]==0,"头部权重越过明确的头颈组边界");
  require(runtime::joint_region_bounds(neck_mesh,world,head_parts,2,&head).empty,"无面部几何时用颈部权重作为眼睛聚焦范围");
}

static void actual_joint_regions(const std::filesystem::path &file,const std::filesystem::path &output) {
  const std::vector<std::filesystem::path> roots={L"H:/G1",L"H:/G3",L"C:/Users/Public/Documents/My DAZ 3D Library"};
  const auto loaded=daz::load(file,{roots,false});const auto skeletons=daz::load_skeletons(loaded);
  nlohmann::json report={{"status","PASS"},{"figures",nlohmann::json::array()}};size_t toes_checked=0;
  for(const auto &skin:skeletons.skins) {
    const auto &instance=loaded.scene.instances.at(skin.instance);const auto &mesh=loaded.scene.meshes.at(instance.mesh);
    const auto parts=runtime::joint_regions(mesh,skin);const std::vector<runtime::JointRegions> regions{parts};
    nlohmann::json figure={{"instance",skin.instance},{"parts",nlohmann::json::array()}};
    for(size_t j=0;j<skin.joints.size();++j) {
      int toe=-1;for(size_t root=0;root<skin.joints.size();++root) if((skin.joints[root].id=="lToe"||skin.joints[root].id=="rToe")&&parts.within(int(j),int(root))) toe=int(root);
      if(toe<0) continue;
      const auto bounds=runtime::joint_region_bounds(mesh,instance.transform,parts,int(j),&skin);require(!bounds.empty,"真实脚趾节点没有网格面聚焦范围");
      size_t count=0;for(size_t t=0;t<parts.detail.size();++t) if(parts.detail[t]==int(j)) {
        require(parts.body[t]==toe,"真实脚趾细分跨越多边形组边界");
        require(runtime::hover_region({0,int(t)},0,-1,regions).joint==toe,"真实脚趾第二次点击跳过 Toes");
        require(runtime::hover_region({0,int(t)},0,toe,regions).joint==int(j),"真实脚趾第三次点击未细分");++count;
      }
      require(count>0,"真实脚趾子节点未分配可点击面");++toes_checked;
      figure["parts"].push_back({{"id",skin.joints[j].id},{"triangles",count},{"min",{bounds.minimum.x,bounds.minimum.y,bounds.minimum.z}},{"max",{bounds.maximum.x,bounds.maximum.y,bounds.maximum.z}}});
    }
    if(!figure["parts"].empty()) report["figures"].push_back(figure);
  }
  require(toes_checked>=22,"真实资产缺少左右脚趾完整层级");
  if(!output.parent_path().empty()) std::filesystem::create_directories(output.parent_path());std::ofstream(output)<<report.dump(2);
  std::cout<<"Real joint regions / mesh focus: PASS ("<<toes_checked<<" nodes)\n";
}
