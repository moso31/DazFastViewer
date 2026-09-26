#include "editor/chrome.h"
#include "editor/ui_scale.h"
#include <QActionGroup>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPainter>
#include <QMenu>
#include <QTimer>
#include <QWindow>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionToolBar>
#include <QApplication>
#include <QSignalBlocker>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

namespace dfv::editor {
namespace {
class ModuleToolBar final:public QToolBar {
public:
  using QToolBar::QToolBar;
  bool handle_at(QPoint p) const {
    QStyleOptionToolBar option;initStyleOption(&option);
    return isMovable()&&style()->subElementRect(QStyle::SE_ToolBarHandle,&option,this).contains(p);
  }
};
QIcon tool_icon(int kind) {
  QPixmap pix(48,48);pix.fill(Qt::transparent);QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);p.scale(2,2);
  const auto ink=QGuiApplication::palette().color(QPalette::ButtonText);p.setPen(QPen(ink,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
  if(kind==0) {p.setBrush(ink);p.drawPolygon(QPolygonF{{5,3},{6,19},{10,14},{15,20},{18,18},{13,12},{20,11}});}
  if(kind==1) {p.drawLine(3,12,21,12);p.drawLine(12,3,12,21);for(int i=0;i<4;++i) {p.save();p.translate(12,12);p.rotate(i*90);p.drawPolyline(QPolygonF{{-3,-6},{0,-9},{3,-6}});p.restore();}}
  if(kind==2) {p.drawArc(QRectF(4,4,16,16),35*16,285*16);p.drawPolyline(QPolygonF{{17,2},{20,7},{14,7}});}
  if(kind==3) {p.drawRect(QRectF(4,13,7,7));p.drawLine(11,13,20,4);p.drawPolyline(QPolygonF{{13,4},{20,4},{20,11}});}
  if(kind==4) {p.setPen(QPen(QColor("#54baff"),2));p.drawPolygon(QPolygonF{{12,2},{21,7},{21,17},{12,22},{3,17},{3,7}});p.drawPolyline(QPolygonF{{3,7},{12,12},{21,7}});p.drawLine(12,12,12,22);}
  if(kind==5) {p.drawEllipse(QRectF(6,2,4,4));p.drawLine(8,7,8,13);p.drawLine(4,9,12,9);p.drawLine(8,13,4,18);p.drawLine(8,13,12,18);p.drawLine(18,4,18,17);p.drawPolyline(QPolygonF{{15,14},{18,17},{21,14}});p.drawLine(2,21,22,21);}
  if(kind==6) {p.drawEllipse(QRectF(9,2,6,6));p.drawPolygon(QPolygonF{{6,8},{18,8},{21,21},{3,21}});p.setFont(QFont(QStringLiteral("Segoe UI"),6,QFont::Bold));p.drawText(QRectF(6,10,12,9),Qt::AlignCenter,QStringLiteral("kg"));}
  return QIcon(pix);
}
}
EditorChrome::EditorChrome(QMainWindow *owner):QWidget(owner),owner_(owner) {
  setObjectName("EditorChrome");auto *row=new QHBoxLayout(this);row->setContentsMargins(5,0,0,0);row->setSpacing(4);
  auto *logo=new QToolButton(this);logo->setObjectName("ApplicationIcon");logo->setIcon(tool_icon(4));logo->setIconSize({24,24});logo->setFixedSize(32,34);logo->setToolTip("DazFastViewer");logo->setPopupMode(QToolButton::InstantPopup);auto *app_menu=new QMenu(logo);logo->setMenu(app_menu);row->addWidget(logo,0,Qt::AlignTop);owner->setWindowIcon(tool_icon(4));
  modules_=new QMainWindow(this);modules_->setWindowFlags(Qt::Widget);modules_->setObjectName("ToolbarModules");modules_->setContentsMargins(0,0,0,0);modules_->setContextMenuPolicy(Qt::PreventContextMenu);modules_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);modules_->installEventFilter(this);row->addWidget(modules_,1);
  menus_=new ModuleToolBar(QStringLiteral("菜单模块"),modules_);menus_->setObjectName("MenuModule");menus_->setAllowedAreas(Qt::TopToolBarArea|Qt::BottomToolBarArea);menus_->setMinimumHeight(34);modules_->addToolBar(menus_);
  menu_=new QMenuBar(menus_);menu_->setNativeMenuBar(false);menu_->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);menus_->addWidget(menu_);
  tools_=new ModuleToolBar(QStringLiteral("3D 操作模块"),modules_);tools_->setObjectName("TransformModule");tools_->setAllowedAreas(Qt::TopToolBarArea|Qt::BottomToolBarArea);tools_->setMinimumHeight(34);tools_->setIconSize({20,20});modules_->addToolBar(tools_);
  auto *group=new QActionGroup(this);const QString names[]={QStringLiteral("选择 / IK"),QStringLiteral("平移"),QStringLiteral("旋转"),QStringLiteral("缩放")};
  for(int i=0;i<4;++i) {auto *a=tools_->addAction(tool_icon(i),names[i]);a->setObjectName("GizmoTool"+QString::number(i));a->setCheckable(true);group->addAction(a);a->setChecked(i==0);a->setToolTip(names[i]+(i==3?QStringLiteral(" · 本地轴；中心方框等比缩放"):i==1?QStringLiteral(" · 拖动轴或平面方块；Esc 取消"):i==2?QStringLiteral(" · 拖动旋转环；Esc 取消"):QStringLiteral(" · 选择与左键 IK"))+(i>0?QStringLiteral("；未命中手柄时可拖动 IK"):QString{}));connect(a,&QAction::triggered,this,[this,i]{settings_.tool=GizmoTool(i);settings_.space=i==1?translation_space_:i==2?rotation_space_:GizmoSpace::local;emit_settings();});}
  tools_->addSeparator();auto *spaces=new QActionGroup(this);local_=tools_->addAction(QStringLiteral("Local"));world_=tools_->addAction(QStringLiteral("World"));
  for(auto *a:{local_,world_}) {a->setCheckable(true);spaces->addAction(a);}local_->setChecked(true);local_->setToolTip(QStringLiteral("沿节点当前自身坐标轴平移／旋转"));world_->setToolTip(QStringLiteral("沿世界固定坐标轴平移／旋转；缩放使用本地轴"));
  auto set_space=[this](GizmoSpace space){settings_.space=space;if(settings_.tool==GizmoTool::translate) translation_space_=space;if(settings_.tool==GizmoTool::rotate) rotation_space_=space;emit_settings();};
  connect(local_,&QAction::triggered,this,[set_space]{set_space(GizmoSpace::local);});connect(world_,&QAction::triggered,this,[set_space]{set_space(GizmoSpace::world);});
  functions_=new ModuleToolBar(QStringLiteral("角色功能模块"),modules_);functions_->setObjectName("FunctionModule");functions_->setAllowedAreas(Qt::TopToolBarArea|Qt::BottomToolBarArea);functions_->setMinimumHeight(34);functions_->setIconSize({20,20});modules_->addToolBar(functions_);
  ground_=functions_->addAction(tool_icon(5),QStringLiteral("对齐到地面"));ground_->setObjectName("AlignToGround");ground_->setShortcut(QKeySequence(Qt::CTRL|Qt::Key_D));ground_->setAutoRepeat(false);owner->addAction(ground_);
  ground_->setToolTip(QStringLiteral("对齐到地面（Ctrl+D）：沿世界 Y 移动选中对象，使底部高度等于当前世界包围盒高度 × 地面对齐比例"));
  ground_ratio_=new QDoubleSpinBox(functions_);ground_ratio_->setObjectName("GroundAlignmentRatio");ground_ratio_->setAccessibleName(QStringLiteral("地面对齐比例"));ground_ratio_->setPrefix(QStringLiteral("比例 "));ground_ratio_->setDecimals(5);ground_ratio_->setRange(-1000,1000);ground_ratio_->setSingleStep(.01);ground_ratio_->setKeyboardTracking(false);ground_ratio_->setMaximumWidth(146);
  ground_ratio_->setToolTip(QStringLiteral("地面对齐比例（每个对象独立）：0 贴地；0.01 使底部高出地面当前对象高度的 1%；负值下沉。修改后点击对齐到地面生效。"));functions_->addWidget(ground_ratio_);
  connect(ground_ratio_,&QDoubleSpinBox::valueChanged,this,[this](double ratio){if(ground_ratio_changed) ground_ratio_changed(ratio);});bind_ground(false,0);emit_settings();
  connect(ground_,&QAction::triggered,this,[this]{ground_ratio_->interpretText();});
  weight_=functions_->addAction(tool_icon(6),QStringLiteral("添加生长或密度参数"));weight_->setObjectName("ObjectWeight");weight_->setEnabled(false);weight_->setToolTip(QStringLiteral("为选中角色添加生长与体重参数，为普通网格添加密度与重量参数"));
  grip_=new QWidget(this);grip_->setObjectName("WindowDragArea");grip_->setMinimumWidth(36);grip_->setFixedHeight(34);grip_->setToolTip("DazFastViewer");row->addWidget(grip_,0,Qt::AlignTop);
  const QStyle::StandardPixmap icons[]={QStyle::SP_TitleBarMinButton,QStyle::SP_TitleBarMaxButton,QStyle::SP_TitleBarCloseButton};
  const QString labels[]={QStringLiteral("最小化"),QStringLiteral("最大化 / 还原"),QStringLiteral("关闭")};
  for(int i=0;i<3;++i) {auto *button=new QToolButton(this);button->setObjectName("WindowControl"+QString::number(i));button->setFixedSize(42,34);button->setIcon(style()->standardIcon(icons[i]));button->setToolTip(labels[i]);button->setAccessibleName(labels[i]);row->addWidget(button,0,Qt::AlignTop);connect(button,&QToolButton::clicked,owner,[owner,i]{if(i==0) owner->showMinimized();else if(i==1) owner->isMaximized()?owner->showNormal():owner->showMaximized();else owner->close();});}
  for(auto *bar:{menus_,tools_,functions_}) {bar->installEventFilter(this);connect(bar,&QToolBar::topLevelChanged,this,[this]{QTimer::singleShot(0,this,[this]{fit_height();});});connect(bar,&QToolBar::visibilityChanged,this,[this]{QTimer::singleShot(0,this,[this]{fit_height();});});}
  menu_->installEventFilter(this);grip_->installEventFilter(this);installEventFilter(this);
  defaults_=modules_->saveState(1);add_layout_actions(app_menu);app_menu->addSeparator();connect(app_menu->addAction(QStringLiteral("关闭")),&QAction::triggered,owner,&QWidget::close);
  setContextMenuPolicy(Qt::CustomContextMenu);connect(this,&QWidget::customContextMenuRequested,this,[this](QPoint p){QMenu menu(this);add_layout_actions(&menu);menu.exec(mapToGlobal(p));});
  QTimer::singleShot(0,this,[this]{fit_height();});
}
void EditorChrome::emit_settings() {const bool enabled=settings_.tool==GizmoTool::translate||settings_.tool==GizmoTool::rotate;world_->setEnabled(enabled);local_->setEnabled(enabled);local_->setChecked(settings_.space==GizmoSpace::local);world_->setChecked(settings_.space==GizmoSpace::world);if(changed) changed(settings_);}
void EditorChrome::bind_ground(bool enabled,double ratio) {ground_->setEnabled(enabled);ground_ratio_->setEnabled(enabled);QSignalBlocker block(ground_ratio_);ground_ratio_->setValue(ratio);}
void EditorChrome::fit_height() {
  const int height=std::clamp(modules_->minimumSizeHint().height(),ui_pixels(34),ui_pixels(160));
  if(modules_->minimumHeight()!=height||modules_->maximumHeight()!=height) modules_->setFixedHeight(height);
  if(minimumHeight()!=height||maximumHeight()!=height) {setFixedHeight(height);updateGeometry();owner_->layout()->invalidate();}
}
void EditorChrome::schedule_fit() {if(fit_pending_)return;fit_pending_=true;QTimer::singleShot(0,this,[this]{fit_pending_=false;fit_height();});}
bool EditorChrome::eventFilter(QObject *object,QEvent *e) {
  if(e->type()==QEvent::LayoutRequest||e->type()==QEvent::Resize||e->type()==QEvent::Show||e->type()==QEvent::Hide||e->type()==QEvent::FontChange||e->type()==QEvent::StyleChange) schedule_fit();
  if(e->type()==QEvent::MouseButtonPress||e->type()==QEvent::MouseButtonDblClick) {
    auto *widget=qobject_cast<QWidget *>(object);auto *mouse=static_cast<QMouseEvent *>(e);
    // 原生子窗口可能先收到客户区鼠标消息，因此 Qt 路径也使用同一套空白区判定。
    if(widget&&widget->window()==owner_&&mouse->button()==Qt::LeftButton&&caption_at(widget->mapTo(this,mouse->position().toPoint()))) {
      if(e->type()==QEvent::MouseButtonDblClick) {owner_->isMaximized()?owner_->showNormal():owner_->showMaximized();return true;}
      if(auto *window=owner_->windowHandle()) return window->startSystemMove();
    }
  }
  return false;
}
bool EditorChrome::caption_at(QPoint position) const {
  if(!rect().contains(position)) return false;
  auto *child=childAt(position);if(!child||child==grip_||child==modules_) return true;
  if(child==menu_) return !menu_->actionAt(menu_->mapFrom(this,position));
  for(auto *bar:{menus_,tools_,functions_}) if(!bar->isFloating()&&(child==bar||bar->isAncestorOf(child))) {
    const auto point=bar->mapFrom(this,position);
    if(static_cast<ModuleToolBar *>(bar)->handle_at(point)) return false;
    if(auto *action=bar->actionAt(point)) return action->isSeparator();
    // 保留按钮、数值输入及溢出菜单；工具栏本身的空白用于移动整个窗口。
    return child==bar;
  }
  return false;
}
QByteArray EditorChrome::save_modules() const {return modules_->saveState(1);}
bool EditorChrome::restore_modules(const QByteArray &state) {const bool ok=modules_->restoreState(state,1);QTimer::singleShot(0,this,[this]{fit_height();});return ok;}
void EditorChrome::reset_modules() {restore_modules(defaults_);menus_->show();tools_->show();functions_->show();}
void EditorChrome::add_layout_actions(QMenu *menu) {menu->addAction(menus_->toggleViewAction());menu->addAction(tools_->toggleViewAction());menu->addAction(functions_->toggleViewAction());connect(menu->addAction(QStringLiteral("恢复顶部模块布局")),&QAction::triggered,this,[this]{reset_modules();});}
void EditorWindow::install_chrome() {
  chrome=new EditorChrome(this);setMenuWidget(chrome);
  if(QGuiApplication::platformName()!=QStringLiteral("windows")) return;
  const auto hwnd=reinterpret_cast<HWND>(winId());MARGINS margins{1,1,1,1};DwmExtendFrameIntoClientArea(hwnd,&margins);
  SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
}
bool EditorWindow::nativeEvent(const QByteArray &type,void *message,qintptr *result) {
  const auto *m=static_cast<MSG *>(message);if(!chrome) return QMainWindow::nativeEvent(type,message,result);
  if(m->message==WM_NCCALCSIZE&&m->wParam) {auto &rect=reinterpret_cast<NCCALCSIZE_PARAMS *>(m->lParam)->rgrc[0];if(IsZoomed(m->hwnd)) {MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromWindow(m->hwnd,MONITOR_DEFAULTTONEAREST),&info);rect=info.rcWork;}*result=0;return true;}
  if(m->message==WM_NCHITTEST) {
    RECT r;GetWindowRect(m->hwnd,&r);const int x=GET_X_LPARAM(m->lParam),y=GET_Y_LPARAM(m->lParam);const int border=std::max(4,GetSystemMetricsForDpi(SM_CXSIZEFRAME,GetDpiForWindow(m->hwnd))+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,GetDpiForWindow(m->hwnd)));
    if(!IsZoomed(m->hwnd)) {const bool l=x<r.left+border,rr=x>=r.right-border,t=y<r.top+border,b=y>=r.bottom-border;const int hit=t?(l?HTTOPLEFT:rr?HTTOPRIGHT:HTTOP):b?(l?HTBOTTOMLEFT:rr?HTBOTTOMRIGHT:HTBOTTOM):l?HTLEFT:rr?HTRIGHT:HTCLIENT;if(hit!=HTCLIENT) {*result=hit;return true;}}
    // Qt 的屏幕坐标原点不会随 DPI 缩放，用物理客户区坐标转换避免副屏偏移。
    POINT client{x,y};ScreenToClient(m->hwnd,&client);const auto local=chrome->mapFrom(this,QPoint(qRound(client.x/devicePixelRatioF()),qRound(client.y/devicePixelRatioF())));
    if(chrome->rect().contains(local)&&chrome->caption_at(local)) {*result=HTCAPTION;return true;}
    *result=HTCLIENT;return true;
  }
  return QMainWindow::nativeEvent(type,message,result);
}
}
