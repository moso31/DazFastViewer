#include "editor/outfit_dialog.h"
#include "editor/geograft_visibility.h"
#include "editor/scene_extension.h"
#include <fstream>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTest>
#include <iostream>
using namespace dfv;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
static editor::Document fixture() {
  editor::Document d;d.generation=1;
  for(uint32_t i=0;i<62;++i) {
    const auto id="node"+std::to_string(i);ir::Mesh mesh;mesh.id=id;mesh.positions={{0,0,0}};d.loaded.scene.meshes.push_back(mesh);
    ir::Instance instance;instance.id=id;instance.mesh=i;d.loaded.scene.instances.push_back(instance);
    daz::AssetObject o;o.instance=i;o.id=id;o.label=i<2?"角色 "+std::to_string(i):"服装 "+std::to_string(i);o.figure=true;
    if(i<2)o.auto_fit_base=i==0?"/Genesis 8/Female":"/Genesis 8.1/Female";else{o.content_type="Follower/Wardrobe";o.conform_target="#node0";o.parent="#node0";}
    d.loaded.objects.push_back(o);d.loaded.nodes.push_back({id,o.parent,o.label});runtime::Target t;t.instance=i;t.id=id+"/mesh";t.label=o.label;t.character=i<2;t.parent=o.parent;t.conform_target=o.conform_target;d.catalog.targets.push_back(t);
  }
  d.formulas.graphs.resize(d.catalog.targets.size());return d;
}
static void wheel(QWidget *widget,int delta){const QPointF p(widget->rect().center());QWheelEvent event(p,widget->mapToGlobal(p.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(widget,&event);QApplication::processEvents();}
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  try {
    if(app.arguments().size()>=4&&app.arguments()[1]=="--scene") {
      const auto file=std::filesystem::path(app.arguments()[2].toStdWString());
      auto loaded=editor::load_scene_extension(file,{"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library"},1);
      auto &d=*loaded.document;auto &s=loaded.snapshot;size_t source=SIZE_MAX,host=SIZE_MAX;
      for(size_t t=0;t<d.catalog.targets.size();++t){if(d.catalog.targets[t].label=="big_01")source=t;if(d.catalog.targets[t].label=="lit")host=t;}
      check(source!=SIZE_MAX&&host!=SIZE_MAX,"真实场景缺少 big_01 / lit");
      editor::OutfitDialog dialog(d,s,source);dialog.show();QTest::qWait(50);
      const auto actual_visibility=runtime::effective_visibility(d.catalog.targets,runtime::visibility_children(d.loaded.scene,d.catalog.targets),s.values);
      auto report=nlohmann::json::array();bool socks=false,loafers=false,lashes=false,hair=false;size_t fe_root=SIZE_MAX;
      std::vector<size_t> fe_parts;
      for(auto t:editor::outfit_clothing(d,source))if(d.catalog.targets[t].label.starts_with("FE Low Ponytail Base Hair"))fe_parts.push_back(t);
      for(auto t:editor::outfit_roots(d,source)) {
        const auto &target=d.catalog.targets[t];auto *box=dialog.findChild<QCheckBox *>("OutfitClothing/"+QString::number(qulonglong(t)));check(box,"实际穿戴没有出现在对话框");
        check(box->isChecked()==actual_visibility[t],"3.dufex 列表初始勾选与场景实际显隐不一致");
        const auto &o=*std::find_if(d.loaded.objects.begin(),d.loaded.objects.end(),[&](const auto &o){return o.instance==target.instance;});
        report.push_back({{"label",target.label},{"id",target.id},{"checked",box->isChecked()},{"visible",s.values[t].visible},{"type",o.content_type},{"figure",o.figure},{"base",o.preferred_base}});
        socks|=target.label=="Socks (2)";loafers|=target.label=="Loafers";lashes|=target.label.find("Eyelashes")!=std::string::npos;hair|=target.label.find("Hair")!=std::string::npos;
        if(target.label=="FE Low Ponytail Base Hair")fe_root=t;
      }
      check(fe_root!=SIZE_MAX&&fe_parts.size()==4,"真实场景 FE Hair 根节点或子部件缺失");
      for(auto t:fe_parts)check(bool(dialog.findChild<QCheckBox *>("OutfitClothing/"+QString::number(qulonglong(t))))==(t==fe_root),"FE Hair 子部件仍被列为独立选项");
      for(auto t:editor::outfit_clothing(d,source))if(d.catalog.targets[t].label=="SU Fashion Long Jeans Button")check(!dialog.findChild<QCheckBox *>("OutfitClothing/"+QString::number(qulonglong(t))),"裤子纽扣仍被列为独立选项");
      auto *fe_box=dialog.findChild<QCheckBox *>("OutfitClothing/"+QString::number(qulonglong(fe_root)));fe_box->setChecked(false);
      for(auto t:dialog.clothing())check(std::find(fe_parts.begin(),fe_parts.end(),t)==fe_parts.end(),"取消发型根条目后仍选中了子部件");fe_box->setChecked(true);
      dialog.grab().save(QString::fromStdWString(std::filesystem::path(app.arguments()[3].toStdWString()).replace_extension("png").wstring()));
      std::ofstream(std::filesystem::path(app.arguments()[3].toStdWString()))<<report.dump(2);check(socks&&loafers&&lashes&&hair,"真实场景遗漏袜子、鞋、睫毛或发型");
      const auto clothes=dialog.clothing();dialog.reject();
      const auto before=s.values;const auto first=d.catalog.targets.size();
      auto result=editor::copy_outfit(d,s,source,clothes,{host});
      std::cout<<"实际场景复制：copied="<<result.copied<<" skipped="<<result.skipped<<std::endl;
      auto again=editor::copy_outfit(d,s,source,clothes,{host});check(!again.copied&&again.skipped==clothes.size(),"真实场景重复复制没有全部跳过");
      for(size_t t=0;t<before.size();++t)check(before[t]==s.values[t],"复制改变了原对象状态");
      for(size_t t=first;t<d.catalog.targets.size();++t)check(editor::attachment_host(d,t)==host,"新穿戴仍依赖来源角色");
      const auto target_clothes=editor::outfit_clothing(d,host);
      for(auto part:fe_parts) {
        const auto &label=d.catalog.targets[part].label;
        check(std::count_if(target_clothes.begin(),target_clothes.end(),[&](size_t t){return d.catalog.targets[t].label==label;})==1,"选择 FE Hair 根条目后没有完整复制四个部件，或重复创建了部件");
      }
      auto scene=d.loaded.scene;runtime::DeformationRuntime runtime(scene,d.catalog.targets,d.skeletons.skins,d.formulas.graphs);runtime.evaluate(s.values,s.poses);
      for(size_t t=first;t<d.catalog.targets.size();++t)for(const auto &p:scene.meshes[scene.instances[d.catalog.targets[t].instance].mesh].positions)check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"实际复制后的穿戴形变无效");
      const auto archive=std::filesystem::path(app.arguments()[3].toStdWString()).parent_path()/"outfit-feedback-copy.dufex";
      editor::save_scene_extension(archive,d,s);std::cout<<"实际场景形变及保存：PASS，正在重新打开验证"<<std::endl;
      auto reopened=editor::load_scene_extension(archive,{"H:/G1","H:/G3","C:/Users/Public/Documents/My DAZ 3D Library"},2);
      check(editor::snapshot_json(d,s)==editor::snapshot_json(*reopened.document,reopened.snapshot),"实际复制保存重开丢失状态");
      std::cout<<"3.dufex / big_01 -> lit: PASS\n";return 0;
    }
    auto d=fixture();auto state=editor::initial_snapshot(d);
    for(size_t t=2;t<6;++t)d.loaded.objects[t].content_type=std::vector<std::string>{"Follower","Follower/Hair","Follower/Attachment/Head/Face/Eyelashes",""}[t-2];
    {auto hidden=state;hidden.values[2].visible=false;hidden.values[4].visible=false;auto nested=d;
      nested.catalog.targets[3].parent="#wear-bone";nested.catalog.targets[3].ancestors={"#wear-bone","#node2","#node0"};
      nested.loaded.objects[3].parent="#wear-bone";nested.loaded.nodes[3].parent="#wear-bone";nested.loaded.nodes.push_back({"wear-bone","#node2","Wear Bone"});
      // 与真实发型一致：父物体反向 Fit To 子物体，分组仍以场景层级为准。
      nested.catalog.targets[2].conform_target="#node3";nested.loaded.objects[2].conform_target="#node3";
      editor::OutfitDialog visibility(nested,hidden,0);
      check(visibility.findChildren<QCheckBox *>().size()==61,"分类不同或隐藏的根穿戴被漏掉，或子部件未折叠");
      check(!visibility.findChild<QCheckBox *>("OutfitClothing/3"),"经骨骼挂接的子部件仍有独立选项");
      check(!visibility.findChild<QCheckBox *>("OutfitClothing/2")->isChecked()&&!visibility.findChild<QCheckBox *>("OutfitClothing/4")->isChecked(),"列表勾选没有跟随实际显隐");
      check(visibility.findChild<QCheckBox *>("OutfitClothing/5")->isChecked(),"无分类附件没有默认勾选");
      auto *sources=visibility.findChild<QComboBox *>("OutfitSource");sources->setCurrentIndex(1);sources->setCurrentIndex(0);check(!visibility.findChild<QCheckBox *>("OutfitClothing/2")->isChecked(),"切换角色后没有重新同步显隐");
    }
    editor::OutfitDialog dialog(d,state,0);dialog.resize(480,350);dialog.show();QTest::qWait(30);
    auto *scroll=dialog.findChild<QScrollArea *>("OutfitScroll");auto *source=dialog.findChild<QComboBox *>("OutfitSource");auto *buttons=dialog.findChild<QDialogButtonBox *>();
    check(dialog.findChildren<QScrollArea *>().size()==1&&scroll->verticalScrollBar()->maximum()>500,"服装长列表没有在唯一外层页面展开");
    check(dialog.clothing().size()==60&&dialog.hosts().empty()&&!buttons->button(QDialogButtonBox::Ok)->isEnabled(),"复制穿搭默认选择不正确");
    wheel(source,-120);check(dialog.source()==0&&scroll->verticalScrollBar()->value()>0,"未点击选中的来源下拉框吞掉滚轮或改变角色");
    scroll->verticalScrollBar()->setValue(0);QTest::mouseClick(dialog.findChild<QLabel *>("OutfitSourceLabel"),Qt::LeftButton);wheel(source,-120);check(dialog.source()==1&&dialog.clothing().empty(),"选中来源参数行后滚轮不能切换角色或服装列表未刷新");
    source->setCurrentIndex(0);QTest::qWait(20);auto *first=dialog.findChild<QCheckBox *>("OutfitClothing/2");QTest::mouseClick(first,Qt::LeftButton,{},QPoint(8,first->height()/2));check(dialog.clothing().size()==59,"单件服装 checkbox 未生效");
    const auto count=dialog.clothing().size();wheel(first,-120);check(dialog.clothing().size()==count&&scroll->verticalScrollBar()->value()>0,"服装 checkbox 滚轮修改了选择或未滚动外层");
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QTest::qWait(20);auto *host=dialog.findChild<QCheckBox *>("OutfitHost/1");check(scroll->viewport()->rect().contains(host->mapTo(scroll->viewport(),host->rect().center())),"最后一个复制目标无法通过外层滚动访问");QTest::mouseClick(host,Qt::LeftButton,{},QPoint(8,host->height()/2));check(dialog.hosts()==std::vector<size_t>{1}&&buttons->button(QDialogButtonBox::Ok)->isEnabled(),"目标选择或确定按钮不可用");
    check(!dialog.findChild<QCheckBox *>("OutfitHost/0")->isEnabled(),"来源角色可以复制给自己");dialog.reject();
    d.loaded.scene.meshes[2].graft_vertex_pairs={{0,0}};d.loaded.scene.instances[2].graft_source=0;auto snapshot=editor::initial_snapshot(d);
    QTreeWidget tree;tree.setItemDelegate(new editor::GeograftVisibilityDelegate(&tree));auto *item=new QTreeWidgetItem(&tree,{QStringLiteral("Geograft")});item->setData(0,Qt::UserRole,2);item->setData(0,Qt::UserRole+1,-1);editor::sync_geograft_visibility(&tree,d,snapshot);tree.show();QTest::qWait(20);
    check(item->flags().testFlag(Qt::ItemIsUserCheckable),"启用的 Geograft checkbox 无法操作");snapshot.values[2].graft_enabled=false;editor::sync_geograft_visibility(&tree,d,snapshot);
    check(!item->flags().testFlag(Qt::ItemIsUserCheckable)&&!item->data(0,editor::graft_visibility_role).toBool(),"停用后 checkbox 没有置灰禁用");
    QTest::mouseClick(tree.viewport(),Qt::LeftButton,{},tree.visualItemRect(item).topLeft()+QPoint(8,tree.visualItemRect(item).height()/2));QTest::keyClick(&tree,Qt::Key_Space);check(item->checkState(0)==Qt::Checked,"停用的 Geograft checkbox 仍被鼠标或键盘更改");
    snapshot.values[2].graft_enabled=true;editor::sync_geograft_visibility(&tree,d,snapshot);check(item->flags().testFlag(Qt::ItemIsUserCheckable),"重新启用后 checkbox 未恢复");
    std::cout<<"复制穿搭对话框：来源切换、单件和目标勾选、滚轮门槛、唯一外层滚动、Geograft 禁用：PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
