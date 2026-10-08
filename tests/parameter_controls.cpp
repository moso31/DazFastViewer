#include "editor/parameters.h"
#include "editor/selection.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QDateEdit>
#include <QTimeEdit>
#include <QComboBox>
#include <QStandardItemModel>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <QScrollBar>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QWheelEvent>
#include "daz/morphs.h"
#include <fstream>
#include "render_ir/options.h"
#include <QTest>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
#include "parameter_favorites.inl"
#include "parameter_wheel.inl"
#include "extension_panel.inl"
#include "geograft_panel.inl"
#include "matte_fog_panel.inl"
#include "parameter_settings.inl"
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  try {
    QTemporaryDir settings;require(settings.isValid(),"测试设置目录创建失败");
    QCoreApplication::setOrganizationName("DazFastViewerTests");QCoreApplication::setApplicationName("ParameterControls");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    double value=.5;dfv::editor::ParameterControl c;c.id="test";c.label="数值";c.minimum=0;c.maximum=1;c.read=[&]{return value;};c.write=[&](double v){value=v;};
    dfv::editor::ParameterPanel panel;panel.resize(380,480);panel.bind_controls({c});panel.show();app.processEvents();QTest::qWait(180);
    auto *spin=panel.findChild<QDoubleSpinBox *>("valueSpin");auto *slider=panel.findChild<QSlider *>("valueSlider");require(spin&&slider,"参数行未创建");
    require(slider->width()>=32,"无限数值范围的文本框挤占了滑轨");
    spin->setFocus();spin->selectAll();QTest::keyClicks(spin,"12345.");panel.evaluated({});QTest::qWait(200);panel.evaluated({});QTest::keyClicks(spin,"678");QTest::keyClick(spin,Qt::Key_Return);require(std::abs(value-12345.678)<1e-6,"右侧输入被范围限制或被后台刷新覆盖");
    spin->selectAll();QTest::keyClicks(spin,"-8.25");QTest::keyClick(spin,Qt::Key_Tab);require(value==-8.25,"失焦未提交负数文本");
    bool interacting=false;int released=0;panel.interaction_changed=[&](bool active){interacting=active;if(!active) ++released;};
    const auto center=slider->rect().center();QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,center);require(interacting,"拖动没有进入交互状态");
    const QPoint beyond(slider->width()*3,center.y());QMouseEvent move(QEvent::MouseMove,beyond,slider->mapToGlobal(beyond),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(slider,&move);
    QTest::mouseRelease(slider,Qt::LeftButton,Qt::NoModifier,beyond);require(value>1,"拖动越过标尺后被截断");
    require(!interacting&&released==1,"释放没有结束交互状态");
    QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,center);QEvent lost(QEvent::UngrabMouse);QApplication::sendEvent(slider,&lost);require(!interacting,"捕获丢失未结束交互状态");
    QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,center);QFocusEvent unfocus(QEvent::FocusOut);QApplication::sendEvent(slider,&unfocus);require(!interacting,"失焦未结束交互状态");
    float stored=0;c.float_backed=true;c.read=[&]{return double(stored);};c.write=[&](double v){stored=float(v);};
    panel.bind_controls({c});app.processEvents();spin=panel.findChild<QDoubleSpinBox *>("valueSpin");
    spin->setFocus();spin->selectAll();QTest::keyClicks(spin,"100.01");QTest::keyClick(spin,Qt::Key_Return);panel.evaluated({});
    require(spin->text()=="100.01"&&stored==100.01f,"Enter 后暴露 float 尾数");
    panel.bind_controls({c});app.processEvents();spin=panel.findChild<QDoubleSpinBox *>("valueSpin");require(spin->text()=="100.01","切换对象后尾数重新出现");
    spin->setFocus();spin->selectAll();QTest::keyClicks(spin,"0.123456");QTest::keyClick(spin,Qt::Key_Tab);panel.evaluated({});require(spin->text()=="0.123456","精细输入被过度舍入");
    QTreeWidget hierarchy;hierarchy.setSelectionMode(QAbstractItemView::ExtendedSelection);hierarchy.resize(300,250);
    auto item=[&](int key) {auto *i=new QTreeWidgetItem(&hierarchy,{QString::number(key)});i->setData(0,Qt::UserRole,key);i->setData(0,Qt::UserRole+1,-1);i->setData(0,Qt::UserRole+2,-1);return i;};
    auto *first=item(0),*second=item(1);hierarchy.show();app.processEvents();
    auto click=[&](QTreeWidgetItem *i,Qt::KeyboardModifiers modifiers) {QTest::mouseClick(hierarchy.viewport(),Qt::LeftButton,modifiers,hierarchy.visualItemRect(i).center());};
    click(first,Qt::NoModifier);click(second,Qt::ControlModifier);require(dfv::editor::tree_selection(&hierarchy).size()==2,"树 Ctrl 左键未增选");
    require(dfv::editor::tree_deletion_selection(&hierarchy).targets==std::vector<size_t>{0,1},"多选删除没有收集所有对象");
    click(second,Qt::ControlModifier);require(dfv::editor::tree_selection(&hierarchy).size()==1&&dfv::editor::active_selection(&hierarchy)==first,"取消当前项后主选项失效");
    dfv::editor::choose_item(&hierarchy,second,true);require(dfv::editor::tree_selection(&hierarchy).size()==2,"视口增选没有同步树");
    dfv::editor::choose_item(&hierarchy,nullptr,true);require(dfv::editor::tree_selection(&hierarchy).size()==2,"Ctrl 空白误清除多选");
    dfv::editor::choose_item(&hierarchy,first);require(dfv::editor::tree_selection(&hierarchy).size()==1,"普通单击没有替换选择");
    auto *group=item(-4);auto *child=item(-7);hierarchy.takeTopLevelItem(hierarchy.indexOfTopLevelItem(child));group->addChild(child);
    dfv::editor::choose_item(&hierarchy,group);dfv::editor::choose_item(&hierarchy,first,true);
    const auto keys=dfv::editor::tree_selection(&hierarchy);require(keys.size()==2&&keys[0][0]==-7&&keys[1][0]==0,"组和实例混选丢失聚焦目标");
    group->setData(0,Qt::UserRole+3,"outer");child->setData(0,Qt::UserRole+3,"instance");child->setSelected(true);
    const auto deletion=dfv::editor::tree_deletion_selection(&hierarchy);require(deletion.targets==std::vector<size_t>{0}&&deletion.nodes==std::vector<std::string>{"instance","outer"},"父组和实例混选删除身份错误");
    auto bone=[&](int joint) {auto *i=new QTreeWidgetItem(first,{QString::number(joint)});i->setData(0,Qt::UserRole,0);i->setData(0,Qt::UserRole+1,joint);i->setData(0,Qt::UserRole+2,-1);return i;};
    auto *left=bone(1),*right=bone(2);first->setExpanded(true);app.processEvents();
    click(left,Qt::NoModifier);click(right,Qt::ControlModifier);
    const std::vector<dfv::editor::Selection> fingers={{0,1,-1},{0,2,-1}};
    require(dfv::editor::tree_selection(&hierarchy)==fingers,"同一角色的不同骨骼被合并成整体选择");
    dfv::editor::choose_item(&hierarchy,second,true);dfv::editor::choose_item(&hierarchy,second,true);
    require(dfv::editor::tree_selection(&hierarchy)==fingers,"跨角色切换丢失同一角色的骨骼多选");
    dfv::editor::choose_item(&hierarchy,left,true);
    require(dfv::editor::tree_selection(&hierarchy)==std::vector<dfv::editor::Selection>{{0,2,-1}}&&dfv::editor::active_selection(&hierarchy)==right,"取消左手指尖没有保留右手指尖和活动项");
    dfv::ir::OptionNode options;options.id="environment";
    dfv::ir::Option mode;mode.id=mode.label="Environment Mode";mode.type="enum";mode.value={2};mode.maximum=3;mode.supported=true;mode.choices={"Dome and Scene","Dome Only","Sun-Sky Only","Scene Only"};
    auto day=mode;day.id=day.label="SS Day";day.type="float";day.value={double(QDate(2024,2,29).toJulianDay())};day.choices.clear();
    auto time=day;time.id=time.label="SS Time";time.value={13*3600+24*60+56};options.parameters={mode,day,time};
    panel.bind_options(&options,[&](size_t p,size_t k,double value){dfv::ir::set_option(options,p,k,value);});app.processEvents();QTest::qWait(50);
    auto *choice=panel.findChild<QComboBox *>("valueChoice");auto *date=panel.findChild<QDateEdit *>("valueDate");auto *clock=panel.findChild<QTimeEdit *>("valueTime");
    require(choice&&date&&clock,"太阳天空没有建立日期 / 时间控件");
    require(qobject_cast<QStandardItemModel *>(choice->model())->item(2)->isEnabled()&&!choice->itemText(2).contains(QStringLiteral("待支持")),"Sun-Sky Only 仍被禁用");
    require(date->date()==QDate(2024,2,29)&&clock->time()==QTime(13,24,56),"儒略日或当地秒数转换错误");
    date->setDate(QDate(2026,9,23));clock->setTime(QTime(23,59,59));require(options.parameters[1].value[0]==QDate(2026,9,23).toJulianDay()&&options.parameters[2].value[0]==86399,"年月日 / 时分秒没有写回原通道");
    {
      QScrollArea outer;outer.resize(520,400);outer.setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);auto *header=new QLabel("shared scroll");layout->addWidget(header);auto *parameters=new dfv::editor::ParameterPanel;parameters->shared_scroll(&outer);layout->addWidget(parameters);outer.setWidget(body);
      std::vector<dfv::editor::ParameterControl> many(2000);for(size_t i=0;i<many.size();++i){many[i].id=std::to_string(i);many[i].label="Parameter "+many[i].id;many[i].group="/Group";many[i].read=[]{return 0.;};many[i].write=[](double){};}
      parameters->bind_controls(many);outer.show();app.processEvents();QTest::qWait(180);
      auto *rows=parameters->findChild<QTreeWidget *>("parameterRows");require(rows->verticalScrollBarPolicy()==Qt::ScrollBarAlwaysOff&&outer.verticalScrollBar()->maximum()>10000,"属性组没有使用公共滚动条");
      require(parameters->findChildren<QDoubleSpinBox *>("valueSpin").size()<20,"公共滚动区一次创建了全部 Morph 控件");
      outer.verticalScrollBar()->setValue(50000);app.processEvents();QTest::qWait(180);auto spins=parameters->findChildren<QDoubleSpinBox *>("valueSpin");require(!spins.empty()&&spins.size()<20&&spins[0]->property("parameterId").toString().toInt()>500,"公共滚动后 Morph 控件未按可见范围挂载");
      parameters->query("Parameter 1999");app.processEvents();QTest::qWait(180);require(outer.verticalScrollBar()->maximum()<500,"过滤后公共页面保留了空白固定高度");
    }
    parameter_favorites(app);
    parameter_settings(app);
    parameter_wheel(app);
    matte_fog_panel(app);
    extension_panel(app);
    geograft_panel(app);
    std::cout<<"Numeric text / focus commit / unbounded slider / sun-sky date-time / DUF favorites: PASS\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}
}
