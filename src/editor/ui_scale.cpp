#include "editor/ui_scale.h"
#include <QAbstractButton>
#include <QAction>
#include <QEvent>
#include <QFont>
#include <QFormLayout>
#include <QHash>
#include <QLayout>
#include <QMenu>
#include <QMainWindow>
#include <QPointer>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionDockWidget>
#include <QStyleOptionMenuItem>
#include <QStyleFactory>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolBar>
#include <QWidgetAction>
#include <algorithm>
#include <vector>

namespace dfv::editor {
namespace {
class ScaledStyle final:public QProxyStyle {
  QStyle *text_style_=QStyleFactory::create("Fusion");
  static bool toolbar_menu(const QWidget *widget){return widget&&widget->objectName()=="ToolbarMenu";}
public:
  explicit ScaledStyle(const QString &name):QProxyStyle(name) {text_style_->setParent(this);}
  QSize sizeFromContents(ContentsType type,const QStyleOption *option,const QSize &contents,const QWidget *widget=nullptr) const override {
    if(type==CT_MenuBarItem&&toolbar_menu(widget))return contents+QSize(ui_pixels(12),ui_pixels(4));
    if(ui_scale()!=1&&type==CT_MenuItem) {
      const auto size=text_style_->sizeFromContents(type,option,contents,widget);
      return contents+QSize(ui_pixels(size.width()-contents.width()),ui_pixels(size.height()-contents.height()));
    }
    return QProxyStyle::sizeFromContents(type,option,contents,widget);
  }
  void drawPrimitive(PrimitiveElement element,const QStyleOption *option,QPainter *painter,const QWidget *widget=nullptr) const override {
    if(element==PE_PanelMenuBar&&toolbar_menu(widget))return;
    QProxyStyle::drawPrimitive(element,option,painter,widget);
  }
  void drawControl(ControlElement element,const QStyleOption *option,QPainter *painter,const QWidget *widget=nullptr) const override {
    // 嵌入工具栏的菜单使用同一套测量和文字绘制，不画原生菜单栏的底边线。
    if(toolbar_menu(widget)) {
      if(element==CE_MenuBarEmptyArea)return;
      if(element==CE_MenuBarItem)if(const auto *menu=qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
        auto item=*menu;painter->save();painter->setFont(widget->font());
        const bool selected=item.state.testFlag(State_Selected)&&item.state.testFlag(State_Enabled);
        painter->fillRect(item.rect,item.palette.brush(selected?QPalette::Highlight:QPalette::Window));
        if(selected)item.palette.setBrush(QPalette::ButtonText,item.palette.brush(QPalette::HighlightedText));
        // Fusion 在每个菜单项底部画分隔线，还向上偏移文字；工具栏只需居中的菜单标签。
        QCommonStyle::drawControl(element,&item,painter,widget);painter->restore();return;
      }
    }
    if(element==CE_DockWidgetTitle&&widget)if(const auto *dock=qstyleoption_cast<const QStyleOptionDockWidget *>(option)) {
      // QDockWidget 绘制标题时可能切回创建时缓存的字体；文字与裁切统一使用当前控件字体。
      auto title=*dock;title.fontMetrics=widget->fontMetrics();painter->save();painter->setFont(widget->font());
      text_style_->drawControl(element,&title,painter,widget);painter->restore();return;
    }
    // Windows 主题文字强制使用系统字体；缩放时让菜单遵循控件字体。
    if(ui_scale()!=1&&(element==CE_MenuBarItem||element==CE_MenuItem))text_style_->drawControl(element,option,painter,widget);
    else QProxyStyle::drawControl(element,option,painter,widget);
  }
  int pixelMetric(PixelMetric metric,const QStyleOption *option=nullptr,const QWidget *widget=nullptr) const override {
    if(toolbar_menu(widget)&&(metric==PM_MenuBarPanelWidth||metric==PM_MenuBarHMargin||metric==PM_MenuBarVMargin))return 0;
    const int value=QProxyStyle::pixelMetric(metric,option,widget);
    // 只缩放几何尺寸，保留比例、数量及系统拖动阈值的原义。
    switch(metric) {
      case PM_DefaultFrameWidth:case PM_ButtonMargin:case PM_MenuHMargin:case PM_MenuVMargin:
      case PM_MenuPanelWidth:case PM_MenuBarHMargin:case PM_MenuBarVMargin:case PM_MenuBarPanelWidth:
      case PM_ScrollBarExtent:case PM_ScrollBarSliderMin:case PM_SliderThickness:case PM_SliderLength:
      case PM_IndicatorWidth:case PM_IndicatorHeight:case PM_ExclusiveIndicatorWidth:case PM_ExclusiveIndicatorHeight:
      case PM_SmallIconSize:case PM_LargeIconSize:case PM_ToolBarIconSize:case PM_ButtonIconSize:
      case PM_ToolBarHandleExtent:case PM_ToolBarItemSpacing:case PM_ToolBarItemMargin:case PM_ToolBarFrameWidth:
      case PM_DockWidgetSeparatorExtent:case PM_DockWidgetHandleExtent:case PM_DockWidgetTitleMargin:
      case PM_LayoutLeftMargin:case PM_LayoutTopMargin:case PM_LayoutRightMargin:case PM_LayoutBottomMargin:
      case PM_LayoutHorizontalSpacing:case PM_LayoutVerticalSpacing:case PM_TabBarTabHSpace:case PM_TabBarTabVSpace:
        return value>0?std::max(1,ui_pixels(value)):value;
      default:return value;
    }
  }
};
QFont scaled_font(QFont font,double scale) {if(font.pixelSize()>0)font.setPixelSize(std::max(1,qRound(font.pixelSize()*scale)));else font.setPointSizeF(font.pointSizeF()*scale);return font;}
}
struct UiScale::Impl {
  struct Widget {QSize minimum,maximum,icon;QFont font;};
  struct Layout {QMargins margins;int spacing;};
  QHash<QWidget *,Widget> widgets;
  QHash<QLayout *,Layout> layouts;
  QFont base_font=QApplication::font();
  std::unique_ptr<QSettings> settings;
  QPointer<QMenu> menu;
  QPointer<QSpinBox> percent;
  bool applying=false;
  int value=100;
  uint64_t layout_revision=0;
  QHash<QMainWindow *,QByteArray> layouts_at_100;
  void capture(QWidget *w,UiScale *owner,bool existing=false) {
    if(!widgets.contains(w)) {
      Widget m{w->minimumSize(),w->maximumSize(),{},w->font()};
      if(!existing)m.font=scaled_font(m.font,1./ui_scale());
      if(auto *bar=qobject_cast<QToolBar *>(w))m.icon=bar->iconSize();
      else if(auto *button=qobject_cast<QAbstractButton *>(w);button&&!button->icon().isNull())m.icon=button->iconSize();
      widgets.insert(w,m);QObject::connect(w,&QObject::destroyed,owner,[this,w]{widgets.remove(w);});
    }
    const auto all=w->findChildren<QLayout *>();for(auto *layout:all)if(!layouts.contains(layout)) {
      // Qt 的主窗口和工具栏内部布局使用已缩放的 style metrics；再乘一次
      // 会使恢复布局时新建的内部布局与启动时捕获的布局产生不同边距。
      if(qobject_cast<QToolBar *>(layout->parentWidget())||qobject_cast<QMainWindow *>(layout->parentWidget()))continue;
      layouts.insert(layout,{layout->contentsMargins(),layout->spacing()});QObject::connect(layout,&QObject::destroyed,owner,[this,layout]{layouts.remove(layout);});
    }
  }
  void apply(QWidget *w) {
    const auto i=widgets.constFind(w);if(i==widgets.cend())return;const auto m=*i;
    auto limit=[](int x){return x==QWIDGETSIZE_MAX?x:std::clamp(ui_pixels(x),0,int(QWIDGETSIZE_MAX));};
    const bool dynamic_height=w->objectName()=="EditorChrome"||w->objectName()=="ToolbarModules";
    w->setMinimumSize(limit(m.minimum.width()),dynamic_height?w->minimumHeight():limit(m.minimum.height()));
    w->setMaximumSize(limit(m.maximum.width()),dynamic_height?w->maximumHeight():limit(m.maximum.height()));
    w->setFont(scaled_font(m.font,ui_scale()));
    if(!m.icon.isEmpty()) {const QSize icon(ui_pixels(m.icon.width()),ui_pixels(m.icon.height()));if(auto *bar=qobject_cast<QToolBar *>(w))bar->setIconSize(icon);else if(auto *button=qobject_cast<QAbstractButton *>(w))button->setIconSize(icon);}
    for(auto *layout:w->findChildren<QLayout *>())if(const auto l=layouts.constFind(layout);l!=layouts.cend()) {
      const auto b=l->margins;layout->setContentsMargins(ui_pixels(b.left()),ui_pixels(b.top()),ui_pixels(b.right()),ui_pixels(b.bottom()));if(l->spacing>=0)layout->setSpacing(ui_pixels(l->spacing));layout->invalidate();
    }
    QEvent style(QEvent::StyleChange);QApplication::sendEvent(w,&style);w->updateGeometry();w->update();
  }
};
UiScale::UiScale(bool persistent,const QString &file,QObject *parent):QObject(parent?parent:qApp),impl_(std::make_unique<Impl>()) {
  if(persistent)impl_->settings=file.isEmpty()?std::make_unique<QSettings>():std::make_unique<QSettings>(file,QSettings::IniFormat);
  qApp->setProperty("dfvUiScale",1.);for(auto *w:QApplication::allWidgets())impl_->capture(w,this,true);
  QApplication::setStyle(new ScaledStyle(QApplication::style()->name()));qApp->installEventFilter(this);
  set_percent(impl_->settings?impl_->settings->value("ui/scalePercent",100).toInt():100);
}
UiScale::~UiScale()=default;
int UiScale::percent() const {return impl_->value;}
void UiScale::set_percent(int value) {
  value=std::clamp(value,50,200);if(impl_->applying)return;impl_->applying=true;
  std::vector<std::pair<QPointer<QMainWindow>,QByteArray>> layouts;
  for(auto *w:QApplication::topLevelWidgets())if(auto *window=qobject_cast<QMainWindow *>(w);window&&window->isVisible()) {
    const auto state=window->saveState(1);
    if(impl_->value==100&&value!=100) {
      if(!impl_->layouts_at_100.contains(window))connect(window,&QObject::destroyed,this,[this,window]{impl_->layouts_at_100.remove(window);});
      impl_->layouts_at_100[window]=state;
    }
    layouts.emplace_back(window,value==100?impl_->layouts_at_100.value(window,state):state);
  }
  for(auto *w:QApplication::allWidgets())impl_->capture(w,this);
  impl_->value=value;qApp->setProperty("dfvUiScale",value/100.);QApplication::setFont(scaled_font(impl_->base_font,value/100.));
  // 父控件的 FontChange / StyleChange 会更新内部编辑器；先应用父级，
  // 再应用子级，避免 allWidgets 的无序遍历将某些输入框恢复成旧字号。
  auto widgets=QApplication::allWidgets();
  auto depth=[](QWidget *w){int n=0;for(;w;w=w->parentWidget())++n;return n;};
  std::stable_sort(widgets.begin(),widgets.end(),[&](auto *a,auto *b){return depth(a)<depth(b);});
  for(auto *w:widgets)impl_->apply(w);
  if(impl_->menu)impl_->menu->setTitle(QStringLiteral("界面缩放（%1%）").arg(value));
  if(impl_->percent){QSignalBlocker block(impl_->percent);impl_->percent->setValue(value);}
  if(impl_->settings){impl_->settings->setValue("ui/scalePercent",value);impl_->settings->sync();}impl_->applying=false;
  const auto revision=++impl_->layout_revision;
  QTimer::singleShot(0,this,[this,revision,layouts=std::move(layouts)]{if(impl_->layout_revision!=revision)return;for(const auto &[window,state]:layouts)if(window)window->restoreState(state,1);});
}
void UiScale::add_menu(QMenu *parent) {
  auto *menu=parent->addMenu(QStringLiteral("界面缩放（%1%）").arg(percent()));impl_->menu=menu;
  menu->setObjectName("UiScaleMenu");
  auto *panel=new QWidget(menu);auto *layout=new QFormLayout(panel);
  auto *percent=new QSpinBox(panel);impl_->percent=percent;percent->setObjectName("UiScalePercent");
  percent->setRange(50,200);percent->setSingleStep(10);percent->setSuffix("%");percent->setValue(this->percent());percent->setKeyboardTracking(false);
  percent->setToolTip(QStringLiteral("调整整个界面的文字、图标与控件大小，与视口渲染倍率独立。"));
  layout->addRow(QStringLiteral("界面缩放"),percent);
  auto *action=new QWidgetAction(menu);action->setDefaultWidget(panel);menu->addAction(action);
  connect(percent,&QSpinBox::valueChanged,this,[this](int value){set_percent(value);});
  auto *reset=menu->addAction(QStringLiteral("恢复 100%"));reset->setObjectName("UiZoomReset");
  connect(reset,&QAction::triggered,this,[this]{set_percent(100);});
}
bool UiScale::eventFilter(QObject *object,QEvent *event) {
  if(!impl_->applying&&event->type()==QEvent::Polish)if(auto *w=qobject_cast<QWidget *>(object)) {
    impl_->capture(w,this);impl_->applying=true;impl_->apply(w);impl_->applying=false;
  }
  return false;
}
}
