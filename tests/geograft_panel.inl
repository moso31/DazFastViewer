#include "editor/geograft_panel.h"
static void geograft_panel(QApplication &app) {
  using namespace dfv::editor;
  QScrollArea outer;outer.setWidgetResizable(true);outer.resize(390,280);
  auto *page=new QWidget;auto *layout=new QVBoxLayout(page);auto *panel=new GeograftPanel;layout->addWidget(panel);layout->addStretch();outer.setWidget(page);
  std::vector<GeograftItem> items;for(size_t i=0;i<60;++i)items.push_back({i,QStringLiteral("Geograft %1").arg(i),true});
  int edits=0;panel->changed=[&](size_t target,bool enabled){items.at(target).enabled=enabled;++edits;};panel->bind(items);outer.show();app.processEvents();QTest::qWait(50);
  auto *first=panel->findChild<QCheckBox *>("GeograftEnabled/0");auto *last=panel->findChild<QCheckBox *>("GeograftEnabled/59");
  require(first&&last&&panel->findChildren<QScrollArea *>().empty()&&outer.verticalScrollBar()->maximum()>500,"Geograft 长列表没有在外层页面展开");
  QTest::mouseClick(first,Qt::LeftButton,Qt::NoModifier,QPoint(8,first->height()/2));require(!items[0].enabled&&edits==1,"点击 Geograft 开关没有停用对应对象");panel->bind(items);require(edits==1&&!first->isChecked(),"刷新 Geograft 开关重复提交编辑");
  const QPointF p(first->rect().center());QWheelEvent wheel(p,first->mapToGlobal(p.toPoint()),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(first,&wheel);app.processEvents();require(edits==1&&!first->isChecked()&&outer.verticalScrollBar()->value()>0,"Geograft 开关吞掉滚轮或滚轮修改了开关");
  outer.verticalScrollBar()->setValue(outer.verticalScrollBar()->maximum());app.processEvents();require(outer.viewport()->rect().contains(last->mapTo(outer.viewport(),last->rect().center())),"外层滚动无法访问最后一个 Geograft");
  QTest::mouseClick(last,Qt::LeftButton,Qt::NoModifier,QPoint(8,last->height()/2));require(!items[59].enabled&&edits==2,"长列表最后一项开关不可用");
  outer.verticalScrollBar()->setValue(0);app.processEvents();
  auto *header=panel->findChild<QToolButton *>("GeograftCollapse");QTest::mouseClick(header,Qt::LeftButton);require(!header->isChecked(),"点击没有折叠 Geograft 分组");QTest::qWait(50);require(outer.verticalScrollBar()->maximum()==0,"折叠 Geograft 后遗留空白滚动范围");
  panel->bind({});require(panel->isHidden(),"没有插件时仍显示 Geograft 分组");
}
