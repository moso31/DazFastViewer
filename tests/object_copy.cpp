#include "editor/document.h"
#include <iostream>

using namespace dfv;
static void check(bool v,const char *message){if(!v)throw std::runtime_error(message);}
static editor::Document fixture(){
  editor::Document d;d.generation=1;ir::Mesh mesh;mesh.id="mesh";mesh.positions={{0,0,0},{1,0,0},{0,0,1}};mesh.material_slots={"Skin"};ir::Triangle triangle;triangle.vertices={0,1,2};mesh.triangles={triangle};d.loaded.scene.meshes={mesh};d.loaded.scene.materials.resize(1);
  for(const auto &id:{"figure","prop"}){ir::Instance instance;instance.id=std::string(id)+"/mesh";instance.materials={0};d.loaded.scene.instances.push_back(instance);runtime::Target target;target.id=instance.id;target.instance=uint32_t(d.catalog.targets.size());target.label=id;d.catalog.targets.push_back(target);d.formulas.graphs.emplace_back();daz::AssetObject object;object.id=id;object.instance=target.instance;d.loaded.objects.push_back(object);daz::AssetNode node;node.id=id;d.loaded.nodes.push_back(node);}
  d.loaded.scene.meshes.push_back(mesh);d.loaded.scene.meshes[1].id="prop-mesh";d.loaded.scene.instances[1].mesh=1;
  d.catalog.targets[1].parent="#hand";d.catalog.targets[1].ancestors={"#hand","#figure"};d.loaded.objects[1].parent="#hand";d.loaded.nodes[1].parent="#hand";
  daz::AssetNode bone;bone.id="hand";bone.parent="#figure";d.loaded.nodes.push_back(bone);
  runtime::Skin skin;skin.id="figure";skin.instance=0;runtime::Joint root;root.id="root";root.scene_id="figure";runtime::Joint hand;hand.id="hand";hand.scene_id="hand";hand.parent=0;skin.joints={root,hand};skin.initial.resize(2);skin.weights.resize(3,{{0,1}});d.skeletons.skins={skin};d.formulas.graphs[0].skin=0;for(auto &g:d.formulas.graphs)g.prepare();
  return d;
}
int main(){try{
  {auto d=fixture();auto s=editor::initial_snapshot(d);s.values[0].transform.general_scale=1.5;s.poses[0][1].rotation_degrees.x=23;s.parameter_settings["figure/mesh"]["scale"]={true,0,2,.05};
    const auto copies=editor::copy_selection(d,s,{0},{},{},false);check(copies.size()==1&&d.catalog.targets.size()==3&&d.skeletons.skins.size()==2,"单体复制错误包含附件或遗漏骨架");check(s.values[copies[0]]==s.values[0]&&s.poses[1]==s.poses[0],"复制没有保留当前参数和姿势");check(d.skeletons.skins[1].joints[1].scene_id=="copy-0/hand","骨骼身份未独立");check(s.parameter_settings["copy-0/figure/mesh"]==s.parameter_settings["figure/mesh"],"范围精度未复制");
    const auto &original=d.loaded.scene.instances[0];const auto &copy=d.loaded.scene.instances[2];check(original.mesh!=copy.mesh&&original.materials!=copy.materials,"副本复用可变几何或材质");s.values[2].transform.general_scale=3;check(s.values[0].transform.general_scale==1.5,"副本修改影响原件");
    auto evaluated=d.loaded.scene;runtime::DeformationRuntime runtime(evaluated,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(s.values,s.poses);
  }
  {auto d=fixture();auto s=editor::initial_snapshot(d);const auto copies=editor::copy_selection(d,s,{0,1,0},{},{},true);check(copies.size()==2,"父子多选重复复制");check(d.catalog.targets[copies[1]].parent=="#copy-0/hand"&&d.catalog.targets[copies[1]].ancestors.back()=="#copy-0/figure","副本附件仍指向原角色");
    const auto second=editor::copy_selection(d,s,{copies[0]},{},{},true);check(second.size()==2&&d.catalog.targets[second[1]].parent=="#copy-1/copy-0/hand","重复复制没有生成唯一身份");
    auto evaluated=d.loaded.scene;runtime::DeformationRuntime runtime(evaluated,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(s.values,s.poses);
  }
  {auto d=fixture();auto s=editor::initial_snapshot(d);const auto copies=editor::copy_selection(d,s,{1},{},{},false);check(d.catalog.targets[copies[0]].parent=="#hand","单独复制附件丢失原父节点");}
  {auto d=fixture();daz::AssetNode group;group.id="group";group.group=true;d.loaded.nodes.push_back(group);d.loaded.nodes[0].parent="#group";d.loaded.objects[0].parent="#group";d.catalog.targets[0].parent="#group";auto s=editor::initial_snapshot(d);s.group_transforms["group"].general_scale=2;
    editor::copy_selection(d,s,{}, {"group"},{},false);check(d.catalog.targets.size()==2&&s.group_transforms["copy-0/group"].general_scale==2,"复制空组错误复制后代或遗漏变换");const auto copies=editor::copy_selection(d,s,{}, {"group"},{},true);check(copies.size()==2&&d.catalog.targets[copies[0]].parent=="#copy-1/group","组递归复制遗漏后代");
  }
  std::cout<<"PASS independent object copies, children, bones, materials and settings\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
