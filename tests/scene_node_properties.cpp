#include "editor/node_properties.h"
#include "editor/scene_extension.h"
#include "daz/documents.h"
#include <fstream>
#include <iostream>

using namespace dfv;using namespace dfv::editor;using J=nlohmann::json;
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static Document fixture(){
  Document d;d.generation=1;auto &scene=d.loaded.scene;ir::Material material;material.id="material";scene.materials={material};
  ir::Mesh mesh;mesh.id="mesh";mesh.material_slots={"Surface"};mesh.positions={{-1,0,-1},{1,0,-1},{0,0,1}};ir::Triangle triangle;triangle.vertices={0,1,2};mesh.triangles={triangle};scene.meshes={mesh};
  d.loaded.nodes={{"outer","","Outer",true},{"inner","#outer","Inner",true}};
  for(const auto *id:{"parent","child","other"}){const auto index=uint32_t(scene.instances.size());ir::Instance i;i.id=std::string(id)+"/mesh";i.materials={0};i.transform=ir::Transform::translate({0,float(index)*2,0});scene.instances.push_back(i);runtime::Target t;t.id=i.id;t.label=id;t.instance=index;t.parent=index==0?"#inner":index==1?"#parent":"";d.catalog.targets.push_back(t);daz::AssetObject o;o.id=id;o.parent=t.parent;o.instance=index;d.loaded.objects.push_back(o);d.loaded.nodes.push_back({id,t.parent,id,false});}
  for(size_t i=1;i<scene.instances.size();++i){auto copy=mesh;copy.id+="/"+std::to_string(i);scene.instances[i].mesh=uint32_t(scene.meshes.size());scene.meshes.push_back(std::move(copy));}
  d.formulas.graphs.resize(3);scene.validate();return d;
}
static void unit(){
  auto d=fixture();auto s=initial_snapshot(d);s.values[1].visible=false;s.values[1].selectable=false;s.node_properties["outer"]={false,false};
  auto scene=d.loaded.scene;runtime::DeformationRuntime deform(scene,d.catalog.targets,{},d.formulas.graphs);deform.evaluate(scene_properties(d,s),{});
  check(!scene.instances[0].visible&&!scene.instances[1].visible&&scene.instances[2].visible,"父组显隐没有正确继承");
  runtime::PickingScene picking;picking.update(scene,scene_pick_mask(d,s,scene));check(picking.ray({0,-2,0},{0,1,0}).instance==2,"不可选中的对象仍参与射线命中");
  s.node_properties["outer"]={true,true};deform.evaluate(scene_properties(d,s),{});picking.apply(scene,{});picking.set_pickable(scene,scene_pick_mask(d,s,scene));check(picking.ray({0,-2,0},{0,1,0}).instance==0,"重新开启父组没有恢复选中");
  check(!scene.instances[1].visible&&!s.values[1].selectable,"父组开关覆盖了子对象独立状态");
  s.values[0].selectable=false;picking.set_pickable(scene,scene_pick_mask(d,s,scene));check(picking.ray({0,-2,0},{0,1,0}).instance==2,"关闭 Selectable 后未穿透到可选对象");
  auto encoded=snapshot_json(d,s);auto restored=initial_snapshot(d);apply_snapshot_json(d,restored,encoded);check(restored.node_properties==s.node_properties&&restored.values==s.values,"保存恢复丢失组属性或 Selectable");
  encoded.erase("node_properties");for(auto &[id,v]:encoded["objects"].items())v.erase("selectable");apply_snapshot_json(d,restored,encoded);check(restored.node_properties.empty()&&restored.values[0].selectable,"旧快照兼容失败");
  check(remove_selection(d,s,{0,1},{"outer","inner"})==2&&d.catalog.targets.size()==1&&d.catalog.targets[0].label=="other","父子嵌套多选删除遗漏或误删");check(d.loaded.nodes.size()==1&&s.node_properties.empty(),"整组删除未清理空组和属性");
  d=fixture();s=initial_snapshot(d);check(remove_selection(d,s,{0,2},{})==3&&d.loaded.scene.instances.empty(),"独立根节点多选删除遗漏");
  d=fixture();s=initial_snapshot(d);auto clone=d.loaded.scene.instances[0];clone.id="clone";clone.prototype=0;clone.instance_node=clone.instance_group="clone-node";d.loaded.scene.instances.push_back(clone);d.loaded.nodes.push_back({"clone-node","#outer","Clone",false});
  check(remove_selection(d,s,{}, {"clone-node"})==1&&d.catalog.targets.size()==3,"删除 Instance 误删原型");
  d=fixture();s=initial_snapshot(d);s.lights.resize(2);s.lights[0].id="a";s.lights[1].id="b";remove_selection(d,s,{2},{},{0,1});check(s.lights.empty()&&d.catalog.targets.size()==2,"模型与灯光混合多选删除失败");
}
static void actual_preset(const std::filesystem::path &file,const std::filesystem::path &preset_file){
  const std::vector<std::filesystem::path> roots={"H:/G1","H:/G3"};Document d;d.generation=1;d.source_file=file;std::cout<<"加载模型"<<std::endl;d.loaded=daz::load(file,{roots,false,true});
  d.catalog=daz::discover_morphs(d.loaded,roots,{},true);d.skeletons=daz::load_skeletons(d.loaded);d.formulas=daz::enable_formulas(d.catalog,d.skeletons);
  auto s=initial_snapshot(d);std::cout<<"加载材质"<<std::endl;const auto preset=daz::load(preset_file,{roots,false});
  std::cout<<preset.report.value("warnings",J::array()).dump(2)<<std::endl;std::cout<<"应用材质"<<std::endl;check(apply_materials(d,0,preset,&s)==3,"Loafers 材质未匹配全部三个表面");
  const auto &i=d.loaded.scene.instances[d.catalog.targets[0].instance];for(auto binding:i.materials){const auto &m=d.loaded.scene.materials[binding];check(m.color_texture>=0&&m.normal_texture>=0,"Brown 贴图丢失");check(d.loaded.scene.textures[m.color_texture].file.filename()=="LoafersBrown_Base_color.png","Brown 颜色贴图不正确");check(daz::material_uv_set(m).label=="Default UVs","Brown UV Set 丢失");}
  std::cout<<"验证选中穿戴角色时赋值"<<std::endl;ir::Mesh body;body.id="character-mesh";body.material_slots={"Skin"};body.positions={{0,0,0},{1,0,0},{0,0,1}};ir::Triangle face;face.vertices={0,1,2};body.triangles={face};
  ir::Instance character;character.id="character/body";character.mesh=uint32_t(d.loaded.scene.meshes.size());character.materials={i.materials[0]};d.loaded.scene.meshes.push_back(body);const auto ci=uint32_t(d.loaded.scene.instances.size());d.loaded.scene.instances.push_back(character);
  runtime::Target target;target.id=character.id;target.label="Character";target.instance=ci;target.character=true;const auto host=d.catalog.targets.size();d.catalog.targets.push_back(target);d.formulas.graphs.push_back({});daz::AssetObject object;object.id="character";object.instance=ci;object.figure=true;object.auto_fit_base="Genesis8Female";d.loaded.objects.push_back(object);d.loaded.nodes.push_back({"character","","Character",false});
  d.catalog.targets[0].parent=d.catalog.targets[0].conform_target="#character";for(auto &o:d.loaded.objects)if(o.instance==d.catalog.targets[0].instance)o.parent=o.conform_target="#character";
  s=initial_snapshot(d);const auto original_character=character.materials;check(apply_materials(d,host,preset,&s)==3,"选中角色后未给其 Loafers 赋值");check(d.loaded.scene.instances[ci].materials==original_character,"Loafers 预设修改了角色本体");
  auto restored=initial_snapshot(d);apply_snapshot_json(d,restored,snapshot_json(d,s));check(restored.values==s.values,"材质赋值后的参数快照无法恢复");
}
int wmain(int argc,wchar_t **argv){try{if(argc==4&&std::wstring(argv[1])==L"--preset")actual_preset(argv[2],argv[3]);else unit();std::cout<<"Scene node properties: PASS"<<std::endl;return 0;}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
