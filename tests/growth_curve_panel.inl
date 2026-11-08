#include "editor/object_extension.h"
static void growth_curve_panel(QApplication &app){
  using namespace dfv;
  editor::Document document;document.generation=1;runtime::Target target;target.id="curve-ui";target.instance=0;document.catalog.targets={target};
  runtime::Skin skin;skin.id="curve-ui";skin.instance=0;for(const auto *name:{"hip","head","lHand","rHand","lFoot","rFoot"}){runtime::Joint joint;joint.id=name;skin.joints.push_back(joint);}skin.initial.resize(6);document.skeletons.skins={skin};
  runtime::FormulaGraph graph;graph.skin=0;runtime::Channel root;root.binding={runtime::Property::general_scale,0,0};root.initial=1;graph.channels.push_back(root);graph.prepare();document.formulas.graphs={graph};
  editor::Snapshot snapshot;snapshot.values.resize(1);snapshot.poses={skin.initial};auto &value=snapshot.values[0];value.extension.kind=runtime::ExtensionKind::growth;
  QWidget host;QVBoxLayout host_layout(&host);editor::ExtensionPanel panel;host_layout.addWidget(&panel);host.resize(390,580);panel.bind(value.extension,true);host.show();app.processEvents();
  int applications=0;panel.changed=[&](auto next,bool shape,double step){editor::edit_growth(document,snapshot,0,next,shape,step);++applications;panel.bind(value.extension);};
  panel.curve_edited=[&](auto next,const editor::GrowthCurveSettings &settings){panel.changed(next,false,0);auto *binding=editor::parameter_widgets::context(&panel);for(const auto &[key,s]:settings)binding->commit(key,s,[]{});};
  panel.prepare_curve_preview=[]{return editor::GrowthCurvePreview([](const auto &v,bool){runtime::WeightResult r;r.height_cm=2*runtime::growth_curve_scale(v.curve,v.age);return r;});};
  auto *enabled=panel.findChild<QCheckBox *>("GrowthCurveEnabled");auto *age=panel.findChild<QDoubleSpinBox *>("GrowthAge");auto *edit=panel.findChild<QPushButton *>("GrowthCurveEdit");
  enabled->click();require(value.extension.curve.enabled&&!panel.findChild<QDoubleSpinBox *>("GrowthSensitivity")->isEnabled()&&!panel.findChild<QPushButton *>("GrowthScaleUp")->isEnabled(),"启用曲线未禁用旧增长入口");
  auto correct_scale=[&]{return std::abs(value.transform.general_scale*100-runtime::growth_curve_scale(value.extension.curve,value.extension.age))<1e-9;};require(correct_scale(),"启用曲线未立即应用当前年龄");
  const int start=applications;age->selectAll();QTest::keyClicks(age,"13.375");QTest::keyClick(age,Qt::Key_Return);require(value.extension.age==13.375&&correct_scale()&&applications>start,"数字输入未同步 Scale");
  QStyleOptionSpinBox option;option.initFrom(age);option.rect=age->rect();option.frame=true;option.buttonSymbols=age->buttonSymbols();option.stepEnabled=QAbstractSpinBox::StepUpEnabled|QAbstractSpinBox::StepDownEnabled;
  const auto up=age->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxUp,age);QTest::mouseClick(age,Qt::LeftButton,Qt::NoModifier,up.center());require(value.extension.age==14.375&&correct_scale(),"鼠标箭头未同步 Scale");
  auto wheel=[&](QDoubleSpinBox *s,int delta){QWheelEvent e(s->rect().center(),s->mapToGlobal(s->rect().center()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(s,&e);};
  QTest::mouseClick(age,Qt::LeftButton);wheel(age,-120);require(value.extension.age==13.375&&correct_scale(),"选中滚轮未同步 Scale");age->setValue(50);require(value.extension.age==50&&correct_scale(),"50 年龄未保留或曲线越界");
  const auto original=value;const auto original_settings=editor::parameter_widgets::context(&panel)->local;
  auto open=[&](const std::function<void(QDialog *)> &action,bool accept){
    std::string failure;QTimer::singleShot(0,[&,action,accept]{auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());try{
      require(dialog&&dialog->objectName()=="GrowthCurveDialog","曲线对话框未打开");dialog->resize(1080,860);app.processEvents();action(dialog);
      dialog->findChild<QDialogButtonBox *>()->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();
    }catch(const std::exception &e){failure=e.what();if(dialog)dialog->reject();}});
    edit->click();if(!failure.empty())throw std::runtime_error(failure);
  };
  auto control=[](QDialog *dialog,const char *name){auto *s=dialog->findChild<QDoubleSpinBox *>(name);require(s!=nullptr,"数值控件缺失");return s;};
  auto mouse=[&](QWidget *plot,double age,double plan){const QPoint pos(qRound(58+(plot->width()-80)*age/plan),qRound(plot->height()*.45));QMouseEvent e(QEvent::MouseMove,pos,plot->mapToGlobal(pos),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(plot,&e);app.processEvents();return pos;};
  auto ready=[&](QWidget *plot){auto *info=plot->findChild<QLabel *>("GrowthCurveHoverInfo");for(int i=0;i<80&&!info->text().contains(" cm");++i)QTest::qWait(30);require(info->isVisible()&&info->text().contains(" cm"),"悬停身高没有完成换算");return info;};
  auto capture=[&](QDialog *dialog,const char *suffix){const auto prefix=qEnvironmentVariable("DFV_EXTENSION_CAPTURE");if(!prefix.isEmpty())require(dialog->grab().save(prefix+suffix),"曲线截图失败");};
  open([&](QDialog *dialog){
    auto *shape=dialog->findChild<QWidget *>("GrowthCurveShapeParameters");require(shape&&dialog->findChild<QWidget *>("GrowthCurveMainParameters"),"参数分组缺失");
    require(!dialog->findChild<QDoubleSpinBox *>("GrowthCurvePeak")&&!dialog->findChild<QDoubleSpinBox *>("GrowthCurvePreviewAge")&&!dialog->findChild<QLabel *>("GrowthCurveScaleSummary"),"已取消的峰值快捷项或预览控件仍存在");
    require(shape->findChildren<QToolButton *>("rangeButton").empty()&&shape->findChildren<QToolButton *>("precisionButton").empty(),"曲线形状仍有范围精度按钮");
    require(control(dialog,"GrowthCurvePlannedAge")->singleStep()==1&&control(dialog,"GrowthCurveBirth")->singleStep()==5&&control(dialog,"GrowthCurveAdult")->singleStep()==5,"主要参数默认步进错误");
    for(int i=0;i<8;++i){require(control(dialog,("GrowthCurveStageAge"+std::to_string(i)).c_str())->singleStep()==.5,"阶段默认步进错误");const auto name="GrowthCurveStageRate"+QString::number(i);auto *label=dialog->findChild<QLabel *>(name);require(label&&!dialog->findChild<QDoubleSpinBox *>(name)&&(label->alignment()&Qt::AlignRight),"每年增长没有使用右对齐只读标签");const auto parts=label->text().split('.');require(parts.size()==1||parts.back().size()<=1,"每年增长显示超过一位小数");}
    require(control(dialog,"GrowthCurveStageAge5")->value()==12.5&&control(dialog,"GrowthCurveStageAge6")->value()==15,"峰值和减速默认年龄错误");
    auto *reference=dialog->findChild<QComboBox *>("GrowthCurveTemplate");std::array<double,8> original_ages{};for(int i=0;i<8;++i)original_ages[i]=control(dialog,("GrowthCurveStageAge"+std::to_string(i)).c_str())->value();
    control(dialog,"GrowthCurveStageAge5")->setValue(13.5);for(int i=0;i<8;++i)if(i!=5)require(control(dialog,("GrowthCurveStageAge"+std::to_string(i)).c_str())->value()==original_ages[i],"峰值编辑影响其他年龄");
    require(reference->currentText()==QStringLiteral("自定义"),"阶段编辑没有标记自定义");
    control(dialog,"GrowthCurvePlannedAge")->setValue(25);for(int i=0;i<8;++i)require(control(dialog,("GrowthCurveStageAge"+std::to_string(i)).c_str())->value()==(i==5?13.5:original_ages[i]),"规划年龄拉伸了阶段");
    control(dialog,"GrowthCurveStageAge7")->setValue(22);require(control(dialog,"GrowthCurvePlannedAge")->value()==25&&control(dialog,"GrowthCurveStageAge6")->value()==15,"成年阶段影响其他参数");
    control(dialog,"GrowthCurveStageAge5")->setValue(16);require(control(dialog,"GrowthCurveStageAge5")->value()==13.5&&!dialog->findChild<QLabel *>("GrowthCurveError")->text().isEmpty(),"相邻阶段冲突未拒绝");
    reference->setCurrentIndex(0);require(control(dialog,"GrowthCurveStageAge5")->value()==12.5&&control(dialog,"GrowthCurveStageAge6")->value()==15&&control(dialog,"GrowthCurvePlannedAge")->value()==18,"模板没有恢复默认年龄");
    auto *b=control(dialog,"GrowthCurveMinimum");auto *fit=dialog->findChild<QPushButton *>("GrowthCurveFit");b->setValue(4);fit->click();require(!dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled(),"无解拟合仍可提交");
    dialog->findChild<QComboBox *>("GrowthCurveMinimumMode")->setCurrentIndex(1);fit->click();
    auto *hp=dialog->findChild<QWidget *>("GrowthCurveHeightPlot"),*gp=dialog->findChild<QWidget *>("GrowthCurveGrowthPlot");
    QTest::mouseClick(hp,Qt::LeftButton,Qt::NoModifier,QPoint(90,40));require(!hp->property("hoverAge").isValid(),"左键点击仍选择虚线年龄");
    const auto pos=mouse(hp,10.6,18);auto *info=ready(hp);require(hp->property("hoverAge").toDouble()==11&&gp->property("hoverAge").toDouble()==11&&info->text().contains(QStringLiteral("年龄：11")),"悬停未选取最近整数年龄或同步两图");
    require(hp->rect().contains(info->geometry())&&std::abs(info->x()-pos.x())<200&&std::abs(info->y()-pos.y())<150,"悬停提示没有放在鼠标附近或越界");
    QTest::mouseClick(hp,Qt::LeftButton,Qt::NoModifier,QPoint(60,45));require(hp->property("hoverAge").toDouble()==11,"左键点击改变了悬停虚线");
    mouse(hp,18,18);info=ready(hp);require(info->text().contains("172")&&info->text().contains("344 cm"),"悬停成年结果换算错误");
    auto expected=value.extension.curve;expected.minimum_growth=4;expected.minimum_mode=runtime::GrowthMinimumMode::additive;runtime::fit_growth_curve(expected);
    const auto shown=dialog->findChild<QLabel *>("GrowthCurveStageRate5")->text().toDouble();require(std::abs(shown-runtime::growth_curve_rate(expected,12.5))<=.051,"每年增长仍显示基础值而非实际增长");
    capture(dialog,"-hover.png");QEvent leave(QEvent::Leave);QApplication::sendEvent(hp,&leave);require(!hp->property("hoverAge").isValid()&&!gp->property("hoverAge").isValid()&&!info->isVisible(),"离开图表未清除虚线和浮层");
    require(value==original,"悬停或草稿修改了场景");auto *binding=dynamic_cast<editor::parameter_widgets::Context *>(dialog->findChild<QObject *>("ParameterWidgetContext"));binding->commit("extension/curve/GrowthCurveBirth",{false,0,100,2.5},[]{});
  },false);
  require(value==original&&editor::parameter_widgets::context(&panel)->local==original_settings,"Cancel 没有丢弃草稿");
  open([&](QDialog *dialog){
    auto *b=control(dialog,"GrowthCurveMinimum");b->setValue(1);dialog->findChild<QPushButton *>("GrowthCurveFit")->click();auto *peak=control(dialog,"GrowthCurveStageAge5");peak->setValue(13.375);
    auto expected=value.extension.curve;expected.minimum_growth=1;runtime::fit_growth_curve(expected);runtime::move_growth_peak(expected,13.375);
    auto *plot=dialog->findChild<QWidget *>("GrowthCurveGrowthPlot");app.processEvents();const double max=plot->property("yTicks").toList().back().toDouble(),rate=runtime::growth_curve_rate(expected,13.375);const QPoint point(qRound(58+(plot->width()-80)*13.375/18),qRound(plot->height()-44-(plot->height()-68)*rate/max));
    QTest::mousePress(plot,Qt::LeftButton,Qt::NoModifier,point);auto drag=[&](QPoint p){QMouseEvent e(QEvent::MouseMove,p,plot->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(plot,&e);};
    drag(point+QPoint(0,-15));require(std::abs(peak->value()-13.375)<1e-8,"竖向拖动改变年龄");drag(point+QPoint(8,-15));QTest::mouseRelease(plot,Qt::LeftButton,Qt::NoModifier,point+QPoint(8,-15));require(peak->value()>13.375&&control(dialog,"GrowthCurveStageAge6")->value()==15&&control(dialog,"GrowthCurveStageAge3")->value()==5,"拖动峰值没有独立编辑年龄");peak->setValue(13.375);
    dialog->resize(800,440);app.processEvents();auto scrolls=dialog->findChildren<QScrollArea *>();require(scrolls.size()==1,"存在嵌套滚动");auto *outer=scrolls.front();outer->verticalScrollBar()->setValue(0);auto *child=control(dialog,"GrowthCurveStageAge3");wheel(child,-120);require(child->value()==5&&outer->verticalScrollBar()->value()>0,"无按钮的阶段行未选中滚轮处理错误");QTest::mouseClick(child,Qt::LeftButton);wheel(child,120);require(child->value()==5.5&&peak->value()==13.375,"阶段滚轮步进或独立性错误");
    auto *binding=dynamic_cast<editor::parameter_widgets::Context *>(dialog->findChild<QObject *>("ParameterWidgetContext"));binding->commit("extension/curve/GrowthCurveBirth",{false,0,100,2.5},[]{});
  },true);
  require(value.extension.curve.shape.points[5].age==13.375&&correct_scale(),"OK 未提交曲线");
  const auto before_custom=value;
  open([&](QDialog *dialog){
    require(control(dialog,"GrowthCurveBirth")->singleStep()==2.5,"主要参数自定义步进丢失");auto *planned=control(dialog,"GrowthCurvePlannedAge");planned->setValue(8);require(control(dialog,"GrowthCurveStageAge7")->value()==18&&control(dialog,"GrowthCurveStageAge5")->value()==13.375,"短规划年龄改写了形状");planned->setValue(25);control(dialog,"GrowthCurvePubertyWeight")->setValue(2.5);dialog->findChild<QPushButton *>("GrowthCurveFit")->click();
    auto *hu=dialog->findChild<QComboBox *>("GrowthCurveHeightUnit"),*gu=dialog->findChild<QComboBox *>("GrowthCurveGrowthUnit");hu->setCurrentIndex(1);require(gu->currentIndex()==0,"单位选择不独立");gu->setCurrentIndex(1);
    auto *hp=dialog->findChild<QWidget *>("GrowthCurveHeightPlot"),*gp=dialog->findChild<QWidget *>("GrowthCurveGrowthPlot");mouse(gp,12.4,25);auto *info=ready(gp);require(gp->property("hoverAge").toDouble()==12&&info->text().contains(QStringLiteral("Scale：")),"cm 图悬停选择错误");
    // 读数在 bind 时更新，绘制诊断在 paintEvent 更新；离屏验证先完成绘制。
    for(auto *p:{hp,gp}){p->grab();require(p->property("cmReady").toBool(),"cm 换算未完成");for(const auto &tick:p->property("yTicks").toList())require(std::fmod(tick.toDouble(),5)==0,"Y 轴不是 5 的整数倍");}
    capture(dialog,"-cm.png");planned->setValue(.5);mouse(hp,.45,.5);require(hp->property("hoverAge").toDouble()==0,"不足 1 的规划范围悬停越界");planned->setValue(25);require(value==before_custom,"悬停修改了实际场景");
  },true);
  require(value.extension.curve.planned_age==25&&value.extension.curve.height_in_cm&&value.extension.curve.growth_in_cm&&correct_scale(),"参数没有保存");
  value.transform.general_scale=1.234;
  open([&](QDialog *dialog){dialog->findChild<QComboBox *>("GrowthCurveHeightUnit")->setCurrentIndex(0);dialog->findChild<QComboBox *>("GrowthCurveGrowthUnit")->setCurrentIndex(0);},true);
  require(value.transform.general_scale==1.234,"单位选择修改手工 Scale");const auto scale=value.transform.general_scale;enabled->click();require(!value.extension.curve.enabled&&value.transform.general_scale==scale,"关闭曲线改变 Scale");
  std::string sample_error;QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());try{require(dialog&&dialog->windowTitle().contains(QStringLiteral("示例身高")),"独立调试入口未打开示例编辑器");dialog->resize(1080,760);app.processEvents();auto *plot=dialog->findChild<QWidget *>("GrowthCurveHeightPlot");mouse(plot,18,18);auto *info=ready(plot);require(info->text().contains("180 cm"),"独立调试示例身高错误");capture(dialog,"-standalone.png");}catch(const std::exception &e){sample_error=e.what();}if(dialog)dialog->reject();});
  require(editor::growth_curve_preview()==0,"独立调试入口失败");if(!sample_error.empty())throw std::runtime_error(sample_error);
  std::cout<<"PASS independent stages, planning, actual annual rates, integer hover, tooltip, no click selection, wheel and standalone preview\n";
}
