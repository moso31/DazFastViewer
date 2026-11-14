#include "editor/prop_split_dialog.h"
#include "editor/edit_history.h"
#include "editor/scene_extension.h"
#include "editor/ground_panel.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QTest>
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <fstream>
#include <iostream>
using namespace dfv;
using namespace dfv::editor;
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
static void wheel(QWidget *w,int delta=-120){const QPointF p=w->rect().center();QWheelEvent e(p,w->mapToGlobal(p.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(w,&e);QApplication::processEvents();}
static QImage arrows(QAbstractSpinBox *spin){QStyleOptionSpinBox option;option.initFrom(spin);option.rect={0,0,200,30};option.state=QStyle::State_Enabled|QStyle::State_Active;option.frame=true;option.buttonSymbols=spin->buttonSymbols();option.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;QImage image(200,30,QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter painter(&image);spin->style()->drawComplexControl(QStyle::CC_SpinBox,&option,&painter,spin);painter.end();return image.copy(170,0,30,30);}
static ir::Mesh fixture(){ir::Mesh m;m.id="shoe";m.material_slots={"Leather"};m.prop_regions={{"bone/hip","hip",-1,0},{"bone/lThigh","左腿",0,-1},{"bone/rThigh","右腿",0,1}};
  for(int i=0;i<80;++i){const auto r=int(m.prop_regions.size());const auto v=uint32_t(m.positions.size());const int side=i<40?-1:1;const auto x=side*.2f,y=(i%40)*.01f;m.prop_regions.push_back({"bone/"+std::to_string(i),"区域 "+std::to_string(i),side<0?1:2,side});m.positions.push_back({x,y,0});m.positions.push_back({x+.1f,y,0});m.positions.push_back({x,y,.1f});ir::Triangle t;t.vertices={v,v+1,v+2};m.triangles.push_back(t);m.prop_face_regions.push_back(r);}return m;}
static void history(const ir::Mesh &mesh){auto doc=std::make_shared<Document>();doc->generation=1;doc->loaded.scene.meshes={mesh};doc->loaded.scene.materials.resize(1);ir::Instance instance;instance.id="shoe/mesh";instance.instance_node="shoe";instance.materials={0};doc->loaded.scene.instances={instance};runtime::Target target;target.id=instance.id;target.label="shoe";target.static_prop=true;doc->catalog.targets={target};doc->formulas.graphs.resize(1);daz::AssetNode node;node.id="shoe";doc->loaded.nodes={node};daz::AssetObject object;object.id="shoe";doc->loaded.objects={object};EditState state;state.document=doc;state.snapshot=initial_snapshot(*doc);const auto before=state;
  EditHistory edits([&]{return state;},[&](const auto &s){state=s;});edits.execute(QStringLiteral("拆分 Prop"),[&]{auto next=std::make_shared<Document>(*state.document);auto snapshot=state.snapshot;split_static_prop(*next,snapshot,0,prop_split_sides(prop_split_source(mesh)));state.document=next;state.snapshot=snapshot;});const auto after=state;check(edits.stack().count()==1&&state.document->catalog.targets.size()==3,"拆分没有形成单条撤销记录");edits.undo();check(state.document==before.document&&snapshot_json(*state.document,state.snapshot)==snapshot_json(*before.document,before.snapshot),"拆分撤销未恢复原件");edits.redo();check(state.document==after.document&&snapshot_json(*state.document,state.snapshot)==snapshot_json(*after.document,after.snapshot),"拆分重做没有恢复结果");}
int main(int argc,char **argv){QApplication app(argc,argv);app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"),9));try{
  auto mesh=fixture();if(argc>1&&std::string(argv[1])=="--preview"){std::ifstream file(argv[2]);nlohmann::json saved;file>>saved;mesh=read_prop_mesh(saved);PropSplitDialog dialog(mesh,QStringLiteral("7.dufex · 鞋子"));dialog.show();QApplication::processEvents();QTest::qWait(200);check(dialog.grab().save(QString::fromLocal8Bit(argv[3])),"无法保存真实模型对话框截图");std::cout<<"PASS real scene dialog preview\n";return 0;}
  history(mesh);PropSplitDialog dialog(mesh,QStringLiteral("多区域长靴"));dialog.show();QApplication::processEvents();auto *count=dialog.findChild<QSpinBox *>("PropSplitCount");auto *scroll=dialog.findChild<QScrollArea *>("PropSplitScroll");auto *tree=dialog.findChild<QTreeWidget *>("PropSplitTree");auto *apply=dialog.findChild<QPushButton *>("PropSplitApply");auto *sides=dialog.findChild<QPushButton *>("PropSplitSides");auto *current=dialog.findChild<QComboBox *>("PropSplitCurrent");auto *search=dialog.findChild<QLineEdit *>("PropSplitSearch");auto *name=dialog.findChild<QLineEdit *>("PropSplitName");
  check(apply->isEnabled()&&prop_split_counts(prop_split_source(mesh),dialog.parts())==std::vector<size_t>({40,40,0}),"左右快捷分组没有包含所有子骨骼");
  GroundPanel ground;ground.bind(true,0,0,false);auto *reference=ground.findChild<QDoubleSpinBox *>("GroundAlignmentOffset");check(arrows(count)==arrows(reference),"拆分数量的原生箭头与地面对齐不一致");
  count->setFocus();wheel(count);check(count->value()==2&&scroll->verticalScrollBar()->value()>0,"未点击时滚轮修改数量或没有滚动外层");scroll->ensureWidgetVisible(count);QApplication::processEvents();QTest::mouseClick(count->findChild<QLineEdit *>(),Qt::LeftButton);wheel(count,120);check(count->value()==3&&!apply->isEnabled(),"点击后的滚轮没有修改数量，或允许空部分");check(arrows(count)==arrows(reference),"选中态破坏原生箭头");
  wheel(current);check(current->currentIndex()==0,"滚轮修改了其他未选中行");QTest::mouseClick(sides,Qt::LeftButton);wheel(count,120);check(count->value()==2,"点击其他模块后旧数值行仍接受滚轮");
  count->setValue(3);current->setCurrentIndex(2);QTreeWidgetItem *left=nullptr,*right=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it){if((*it)->data(0,Qt::UserRole).toInt()==3)left=*it;if((*it)->data(0,Qt::UserRole).toInt()==43)right=*it;}
  for(auto *item:{left,right}){check(item,"测试区域不存在");tree->setCurrentItem(item);QTest::keyClick(tree,Qt::Key_Space);}const auto multi=prop_split_counts(prop_split_source(mesh),dialog.parts());for(auto n:multi)std::cout<<n<<' ';std::cout<<std::endl;check(apply->isEnabled()&&multi==std::vector<size_t>({39,39,2,0}),"同一部分不能选择多个不相连范围");
  current->setCurrentIndex(2);name->setFocus();name->selectAll();QTest::keyClicks(name,"mixed");check(dialog.parts()[2].label=="mixed","部分名称未保存");
  search->setText("79");QApplication::processEvents();check(search->isClearButtonEnabled(),"缺少原生搜索清空按钮");auto *clear=search->findChild<QToolButton *>();check(clear&&clear->isVisible(),"搜索清空按钮不可见");QTest::mouseClick(clear,Qt::LeftButton);QApplication::processEvents();check(search->text().isEmpty(),"清空按钮未清空筛选");for(QTreeWidgetItemIterator it(tree);*it;++it)check(!(*it)->isHidden(),"清空筛选未恢复区域");
  check(tree->verticalScrollBar()->maximum()==0&&tree->horizontalScrollBar()->maximum()==0,"骨骼树有内层滚动范围");auto *last=tree->topLevelItem(0)->child(1)->child(39);check(tree->visualItemRect(last).bottom()<=tree->viewport()->height(),"最后一个骨骼被截断");scroll->verticalScrollBar()->setValue(0);wheel(tree->viewport());check(scroll->verticalScrollBar()->value()>0,"骨骼树吞掉外层滚轮");scroll->ensureVisible(0,tree->y()+tree->height());QApplication::processEvents();check(scroll->verticalScrollBar()->value()>1000,"外层无法到达最后一个骨骼");
  scroll->ensureWidgetVisible(count);QTest::mouseClick(count,Qt::LeftButton);dialog.hide();dialog.show();wheel(count,120);check(count->value()==3,"隐藏对话框后没有清除参数选择");
  const auto draft=dialog.parts();dialog.reject();check(dialog.result()==QDialog::Rejected&&draft.size()==3,"取消状态错误");std::cout<<"PASS Qt multi-range selection, presets, names, search clear, native arrows, wheel selection, outer scroll, cancel, undo/redo\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}}
