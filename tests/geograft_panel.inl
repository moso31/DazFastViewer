#include "editor/geograft_panel.h"
static void geograft_panel(QApplication &app) {
  using namespace dfv::editor;
  QScrollArea outer;outer.setWidgetResizable(true);outer.resize(390,280);
  auto *page=new QWidget;auto *layout=new QVBoxLayout(page);auto *panel=new GeograftPanel;layout->addWidget(panel);layout->addStretch();outer.setWidget(page);
  std::vector<GeograftItem> items;for(size_t i=0;i<60;++i)items.push_back({i,QStringLiteral("Geograft %1").arg(i),true});
  int edits=0;panel->changed=[&](size_t target,bool enabled){items.at(target).enabled=enabled;++edits;};panel->bind(items);outer.show();app.processEvents();QTest::qWait(50);
  auto *first=panel->findChild<QCheckBox *>("GeograftEnabled/0");auto *last=panel->findChild<QCheckBox *>("GeograftEnabled/59");
  auto *update=panel->findChild<QPushButton *>("GeograftUpdate/0");size_t updated=999;int updates=0;panel->update_materials=[&](size_t target){updated=target;++updates;};
  require(update&&update->parentWidget()==first->parentWidget()&&update->x()>first->x()&&std::abs(update->geometry().center().y()-first->geometry().center().y())<=1,"更新按钮不在 checkbox 右侧同一行");
  QTest::mouseClick(update,Qt::LeftButton);require(updated==0&&updates==1&&edits==0&&first->isChecked(),"更新按钮没有隔离材质更新与启用开关");
  require(first&&last&&panel->findChildren<QScrollArea *>().empty()&&outer.verticalScrollBar()->maximum()>500,"Geograft 长列表没有在外层页面展开");
  QTest::mouseClick(first,Qt::LeftButton,Qt::NoModifier,QPoint(8,first->height()/2));require(!items[0].enabled&&edits==1,"点击 Geograft 开关没有停用对应对象");panel->bind(items);require(edits==1&&!first->isChecked(),"刷新 Geograft 开关重复提交编辑");
  QTest::mouseClick(update,Qt::LeftButton);require(updates==2&&!first->isChecked()&&edits==1,"停用附件不能单独更新材质，或更新改变了启用状态");
  const QPointF p(first->rect().center());QWheelEvent wheel(p,first->mapToGlobal(p.toPoint()),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(first,&wheel);app.processEvents();require(edits==1&&!first->isChecked()&&outer.verticalScrollBar()->value()>0,"Geograft 开关吞掉滚轮或滚轮修改了开关");
  outer.verticalScrollBar()->setValue(0);const QPointF u(update->rect().center());QWheelEvent update_wheel(u,update->mapToGlobal(u.toPoint()),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(update,&update_wheel);app.processEvents();require(updates==2&&outer.verticalScrollBar()->value()>0,"更新按钮吞掉滚轮或误触发更新");
  outer.verticalScrollBar()->setValue(outer.verticalScrollBar()->maximum());app.processEvents();require(outer.viewport()->rect().contains(last->mapTo(outer.viewport(),last->rect().center())),"外层滚动无法访问最后一个 Geograft");
  QTest::mouseClick(last,Qt::LeftButton,Qt::NoModifier,QPoint(8,last->height()/2));require(!items[59].enabled&&edits==2,"长列表最后一项开关不可用");
  outer.verticalScrollBar()->setValue(0);app.processEvents();
  auto *header=panel->findChild<QToolButton *>("GeograftCollapse");QTest::mouseClick(header,Qt::LeftButton);require(!header->isChecked(),"点击没有折叠 Geograft 分组");QTest::qWait(50);require(outer.verticalScrollBar()->maximum()==0,"折叠 Geograft 后遗留空白滚动范围");
  panel->bind({});require(panel->isHidden(),"没有插件时仍显示 Geograft 分组");
  panel->bind({{71,QStringLiteral("另一角色的附件"),true}});auto *rebound=panel->findChild<QPushButton *>("GeograftUpdate/71");require(rebound&&!panel->findChild<QPushButton *>("GeograftUpdate/0"),"切换角色残留旧更新入口");QTest::mouseClick(rebound,Qt::LeftButton);require(updated==71&&updates==3,"切换角色后更新指向旧附件");
  if(qEnvironmentVariableIsSet("DFV_GEOGRAFT_CAPTURE")){panel->bind({{0,QStringLiteral("Golden Palace Gens"),true},{1,QStringLiteral("HD Nipples for G8F - 2.0"),true}});header->setChecked(true);outer.resize(430,210);app.processEvents();outer.grab().save(qEnvironmentVariable("DFV_GEOGRAFT_CAPTURE"));}
}
