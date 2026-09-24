static void parameter_wheel(QApplication &app) {
  using namespace dfv::editor;
  std::vector<float> values(40,.1f);std::vector<ParameterControl> controls;
  for(size_t i=0;i<values.size();++i) {
    ParameterControl c;c.id="wheel/"+std::to_string(i);c.label="参数 "+std::to_string(i);c.float_backed=true;c.step=double(.01f);
    c.read=[&,i]{return double(values[i]);};c.write=[&,i](double value){values[i]=float(value);};controls.push_back(std::move(c));
  }
  ParameterPanel panel;panel.resize(540,430);panel.bind_controls(controls);panel.show();app.processEvents();
  auto *rows=panel.findChild<QTreeWidget *>("parameterRows");auto *bar=rows->verticalScrollBar();
  auto widget=[&](int index){auto *w=rows->itemWidget(rows->topLevelItem(index),0);require(w,"滚轮测试参数行未挂载");return w;};
  auto slider=[&](int index){return widget(index)->findChild<QSlider *>("valueSlider");};
  auto spin=[&](int index){return widget(index)->findChild<QDoubleSpinBox *>("valueSpin");};
  auto label=[&](int index){return widget(index)->findChild<QLabel *>("valueLabel");};
  auto top=[&]{bar->setValue(0);app.processEvents();};
  auto wheel=[&](QWidget *w,int delta,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
    require(w,"缺少滚轮测试控件");const QPointF pos(w->rect().center());
    QWheelEvent event(pos,w->mapToGlobal(pos.toPoint()),{},QPoint(0,delta),Qt::NoButton,modifiers,Qt::NoScrollPhase,false);
    QApplication::sendEvent(w,&event);app.processEvents();
  };
  auto left=[&](QWidget *w){QTest::mouseClick(w,Qt::LeftButton);app.processEvents();};
  require(bar->maximum()>0,"滚轮测试列表没有超出页面");
  wheel(slider(0),-120);require(values[0]==.1f&&bar->value()>0,"未左键选中的 Slider 抢占滚轮或没有翻页");
  top();wheel(spin(0),-120);require(values[0]==.1f&&bar->value()>0,"未选中的数字框抢占滚轮");
  top();wheel(spin(0)->findChild<QLineEdit *>(),-120);require(values[0]==.1f&&bar->value()>0,"数字框内部文本区绕过滚轮选择限制");
  top();spin(0)->setFocus();rows->setCurrentItem(rows->topLevelItem(0));wheel(slider(0),-120);
  require(values[0]==.1f&&bar->value()>0,"自动焦点或程序选择开启了滚轮调参");
  top();left(slider(0));require(rows->currentItem()==rows->topLevelItem(0)&&rows->topLevelItem(0)->isSelected(),"点击 Slider 没有同步参数行选中状态");
  const int scroll=bar->value();wheel(slider(0),120);require(values[0]==.11f&&bar->value()==scroll,"已选中 Slider 无法调参或同时滚动页面");
  wheel(spin(0),120);require(values[0]==.12f&&bar->value()==scroll,"选中行的数字框不能复用同一个选中状态");
  wheel(slider(1),-120);require(values[0]==.12f&&values[1]==.1f&&bar->value()>scroll,"悬停其他行误改参数或远程调整旧选中项");
  top();left(label(1));wheel(slider(1),120);require(values[1]==.11f&&rows->currentItem()==rows->topLevelItem(1),"点击参数名称不能启用该行滚轮");
  const auto saved=values;wheel(spin(0),-120);require(values==saved&&bar->value()>0,"新选中行没有取消旧行的滚轮权限");
  top();QTest::mouseClick(label(2),Qt::RightButton);wheel(slider(2),-120);require(values==saved&&bar->value()>0,"右键意外启用了滚轮调参");
  top();left(spin(0)->findChild<QLineEdit *>());wheel(slider(0),120);require(values[0]==.13f,"左键数字文本没有选中整行");
  // 虚拟行离开视口后会销毁控件；重新挂载仍须沿用该参数行的显式选择。
  bar->setValue(bar->maximum());app.processEvents();top();wheel(slider(0),120);require(values[0]==.14f,"滚动卸载再挂载丢失参数的选中状态");
  panel.query("参数 1");app.processEvents();panel.query("");top();const auto hidden_values=values;wheel(slider(0),-120);
  require(values==hidden_values&&bar->value()>0,"筛选隐藏后恢复的行没有重新要求左键选中");
  panel.bind_controls(controls);app.processEvents();top();wheel(slider(0),-120);require(values==hidden_values&&bar->value()>0,"切换绑定继承了旧的滚轮调参权限");
  // float 步长、连续正反向、多个刻度及高分辨率滚轮都必须使用数字框的十进制步进。
  values[0]=100.01f;top();panel.evaluated({});left(slider(0));
  for(int i=0;i<50;++i) {wheel(slider(0),120);panel.evaluated({});}
  require(values[0]==100.51f&&spin(0)->text()=="100.51","Slider 连续滚轮累积了 float 尾数");
  for(int i=0;i<50;++i) wheel(slider(0),-120);
  require(values[0]==100.01f&&spin(0)->text()=="100.01","Slider 正反滚轮未精确恢复十进制初值");
  wheel(slider(0),360);require(values[0]==100.04f,"Slider 丢失同一事件的多个滚轮刻度");
  wheel(slider(0),60);require(values[0]==100.04f,"半个滚轮刻度被转换为非标准步长");
  wheel(slider(0),60);require(values[0]==100.05f,"高分辨率滚轮未正确累计一个刻度");
  wheel(slider(0),-480);require(values[0]==100.01f,"多刻度反向恢复错误");
  // 小数细调不被统一舍入为两位，百分比换算也不把 float 尾数重新带入下一步。
  float percent=1.0001f;double precise=.123456;
  auto p=controls[0];p.id="percentage";p.read=[&]{return double(percent)*100;};p.write=[&](double v){percent=float(v/100);};
  auto fine=controls[0];fine.id="precise";fine.float_backed=false;fine.step=.000001;fine.read=[&]{return precise;};fine.write=[&](double v){precise=v;};
  panel.bind_controls({p,fine});app.processEvents();left(slider(0));for(int i=0;i<50;++i) wheel(slider(0),120);
  require(percent==1.0051f&&spin(0)->text()=="100.51","百分比换算后的 Slider 滚轮暴露 float 尾数");
  left(slider(1));wheel(slider(1),120);require(std::abs(precise-.123457)<1e-12&&spin(1)->text()=="0.123457","滚轮修复损失六位小数的细调精度");
  // 枚举、日期和时间也属于参数行，悬停时必须可以滚动长列表。
  controls[0].choices={"A","B","C"};values[0]=0;
  controls[1].format=ParameterControl::Format::date;values[1]=float(QDate(2026,9,25).toJulianDay());
  controls[2].format=ParameterControl::Format::time;values[2]=3600;
  controls[3].enabled=false;
  panel.bind_controls(controls);app.processEvents();const auto before_choices=values;
  for(const auto &[index,name]:std::vector<std::pair<int,const char *>>{{0,"valueChoice"},{1,"valueDate"},{2,"valueTime"},{3,"valueSlider"}}) {
    top();wheel(widget(index)->findChild<QWidget *>(name),-120);require(values==before_choices&&bar->value()>0,"枚举、日期、时间或禁用参数阻断了翻页");
  }
  top();left(label(0));const int before_scroll=bar->value();wheel(widget(0)->findChild<QComboBox *>("valueChoice"),-120);
  require(values[0]==1&&bar->value()==before_scroll,"选中的枚举参数无法使用滚轮");
  std::cout<<"Explicit row selection / wheel scrolling / decimal slider stepping: PASS\n";
}
