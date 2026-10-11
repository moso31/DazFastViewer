#include "editor/garment_controls.h"
#include <QFontDatabase>
static void garment_panel(QApplication &app) {
  using namespace dfv;
  using namespace dfv::editor;
  ir::Mesh mesh;mesh.positions.resize(3);mesh.polygon_groups={"default","lFoot","lToe","rFoot","rToe"};
  for(uint32_t g=1;g<5;++g){ir::Triangle t;t.vertices={0,1,2};t.polygon_group=g;mesh.triangles.push_back(t);}
  runtime::Properties values;int edits=0;
  QScrollArea outer;outer.setWidgetResizable(true);outer.resize(570,340);auto *page=new QWidget;auto *layout=new QVBoxLayout(page);auto *panel=new GarmentPanel;layout->addWidget(panel);layout->addStretch();outer.setWidget(page);
  const auto font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");const auto families=QFontDatabase::applicationFontFamilies(font);if(!families.empty())outer.setFont(QFont(families.front(),9));
  panel->changed=[&](const std::string &id,bool enabled){if(enabled)values.disabled_deformation_regions.erase(id);else values.disabled_deformation_regions.insert(id);++edits;};
  auto bind=[&]{panel->bind(runtime::deformation_regions(mesh),values.disabled_deformation_regions);app.processEvents();QTest::qWait(80);};
  bind();outer.show();app.processEvents();QTest::qWait(80);
  auto checks=[&]{return panel->findChildren<QCheckBox *>("GarmentRegion");};
  require(checks().size()==2&&checks()[0]->text()=="Foot"&&checks()[1]->text()=="Toes","鞋子没有恰好两个部位开关");
  require(checks()[0]->y()==checks()[1]->y(),"宽页面未将勾选框排在同一行");
  auto *toes=checks()[1];require(toes->isChecked(),"Toes 默认未开启");QTest::mouseClick(toes,Qt::LeftButton,Qt::NoModifier,QPoint(8,toes->height()/2));require(values.disabled_deformation_regions==std::set<std::string>{"Toes"}&&edits==1&&checks()[0]->isChecked(),"点击 Toes 改变了其他部位");
  bind();require(!checks()[1]->isChecked()&&edits==1,"重新挂载丢失开关或重复写值");
  auto *header=panel->findChild<QToolButton *>("GarmentCollapse");QTest::mouseClick(header,Qt::LeftButton);app.processEvents();require(!checks()[0]->isVisible(),"形变组无法独立收起");bind();require(!checks()[0]->isVisible(),"刷新重置了折叠状态");QTest::mouseClick(header,Qt::LeftButton);app.processEvents();
  outer.grab().save("garment-panel.png");
  for(uint32_t g=5;g<65;++g){mesh.polygon_groups.push_back("Region"+std::to_string(g));ir::Triangle f;f.vertices={0,1,2};f.polygon_group=g;mesh.triangles.push_back(f);}
  bind();outer.resize(240,250);app.processEvents();QTest::qWait(80);require(outer.verticalScrollBar()->maximum()>200&&panel->findChildren<QScrollArea *>().empty(),"长部位列表没有使用外层滚动");
  require(checks()[0]->y()<checks().back()->y(),"窄页面没有自动换行");
  auto *first=checks()[0];const QPointF pos(first->rect().center());QWheelEvent wheel(pos,first->mapToGlobal(pos.toPoint()),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(first,&wheel);app.processEvents();require(edits==1&&outer.verticalScrollBar()->value()>0,"部位勾选框吞掉滚轮或误改开关");
  outer.verticalScrollBar()->setValue(outer.verticalScrollBar()->maximum());app.processEvents();QTest::qWait(80);auto *last=checks().back();outer.grab().save("garment-panel-long.png");require(outer.viewport()->rect().contains(last->mapTo(outer.viewport(),last->rect().center())),"最后一个形变部位无法通过外层滚动访问");
  QTest::mouseClick(last,Qt::LeftButton,Qt::NoModifier,QPoint(8,last->height()/2));require(values.disabled_deformation_regions.contains("Region64")&&edits==2,"最后一个部位不能编辑");
  panel->bind({},{});require(panel->isHidden(),"切换到无形变对象后仍显示旧选项");
}
