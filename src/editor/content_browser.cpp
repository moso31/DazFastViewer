#include "editor/content_browser.h"
#include "daz/content_entry.h"
#include "editor/content_catalog.h"
#include <QAbstractListModel>
#include <QApplication>
#include <QCache>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QScreen>
#include <QShortcut>
#include <QWidgetAction>
#include <QPersistentModelIndex>
#include <algorithm>

namespace dfv::editor {
namespace {
constexpr int maximum_zoom=21;
int icon_pixels(int level) {return level==0?0:56+8*level;}
struct Thumbnail {QPixmap image;qint64 loaded=0;};
class Thumbnails final:public QObject {
  QThreadPool pool_;
  QCache<QString,Thumbnail> cache_{64*1024};
  QSet<QString> pending_;
  std::atomic<uint64_t> generation_=0;
public:
  std::function<void(const QString &,bool)> ready;
  explicit Thumbnails(QObject *parent):QObject(parent) {pool_.setMaxThreadCount(2);}
  ~Thumbnails() override {++generation_;pool_.clear();pool_.waitForDone();}
  void clear() {++generation_;pool_.clear();pending_.clear();cache_.clear();}
  QPixmap get(const QString &path,bool tip=false) {
    const auto key=path+(tip?"|tip":"|icon");
    if(auto *cached=cache_.object(key)) {if(QDateTime::currentMSecsSinceEpoch()-cached->loaded<60000) return cached->image;cache_.remove(key);}
    if(pending_.contains(key)||pending_.size()>=96) return {};
    pending_.insert(key);const auto generation=generation_.load();
    pool_.start([this,path,key,tip,generation] {
      QImage image;const int extent=tip?512:320;
      for(const auto &candidate:preview_candidates(path,tip)) {
        if(generation!=generation_) return;QImageReader reader(candidate);reader.setAutoTransform(true);const auto size=reader.size();
        if(!size.isValid()) continue;reader.setScaledSize(size.scaled(extent,extent,Qt::KeepAspectRatio));image=reader.read();if(!image.isNull()) break;
      }
      if(generation!=generation_) return;
      QMetaObject::invokeMethod(this,[this,path,key,tip,generation,image] {
        if(generation!=generation_) return;pending_.remove(key);
        const int cost=std::max(1,int(image.sizeInBytes()/1024));cache_.insert(key,new Thumbnail{QPixmap::fromImage(image),QDateTime::currentMSecsSinceEpoch()},cost);if(ready) ready(path,tip);
      },Qt::QueuedConnection);
    });return {};
  }
};
struct Item {QString path,category;bool directory=false;qint64 used=0;};
class ContentModel final:public QAbstractListModel {
  Thumbnails &thumbnails_;
  QIcon folder_,file_;
public:
  std::vector<Item> items;
  explicit ContentModel(Thumbnails &thumbnails,QObject *parent):QAbstractListModel(parent),thumbnails_(thumbnails) {
    folder_=QApplication::style()->standardIcon(QStyle::SP_DirIcon);file_=QApplication::style()->standardIcon(QStyle::SP_FileIcon);
  }
  int rowCount(const QModelIndex &parent={}) const override {return parent.isValid()?0:int(items.size());}
  QVariant data(const QModelIndex &index,int role) const override {
    if(!index.isValid()||index.row()<0||size_t(index.row())>=items.size()) return {};const auto &item=items[size_t(index.row())];
    if(role==Qt::DisplayRole) return QFileInfo(item.path).fileName();
    if(role==Qt::UserRole) return item.path;
    if(role==Qt::ToolTipRole) return item.path;
    if(role==Qt::DecorationRole) {if(item.directory) return folder_;auto image=thumbnails_.get(item.path);return image.isNull()?file_:QIcon(image);}
    return {};
  }
  void replace(std::vector<Item> next) {beginResetModel();items=std::move(next);endResetModel();}
};
QToolButton *button(const QString &text,const QString &tip,QWidget *parent) {auto *b=new QToolButton(parent);b->setText(text);b->setToolTip(tip);return b;}
}

struct ContentBrowser::Impl {
  ContentBrowser *owner;
  ContentHistory history;
  ContentIndex index;
  Thumbnails thumbnails;
  QThreadPool directories;
  std::atomic<uint64_t> directory_generation=0;
  QComboBox *libraries,*search,*scope;
  QComboBox *tabs;
  QTreeView *tree;
  QFileSystemModel *files;
  QListWidget *categories;
  QListView *view;
  ContentModel *model;
  QStackedWidget *left;
  QSplitter *splitter;
  QLabel *empty,*preview_image,*preview_text;
  QFrame *preview;
  QSlider *zoom;
  QTimer query_timer,history_timer,refresh_timer,folder_timer,repaint_timer;
  QFileSystemWatcher watcher;
  QStringList roots;
  QString directory,hovered,library_query;
  QPoint hover_position;
  int hover_row=-1;
  int wheel_remainder=0;
  bool restoring=false;

  Impl(ContentBrowser *o,const QString &settings_file,const QString &cache):owner(o),history(settings_file),index(cache.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/content-v1":cache),thumbnails(o) {
    directories.setMaxThreadCount(1);
    auto *layout=new QVBoxLayout(o);layout->setContentsMargins(1,1,1,1);layout->setSpacing(2);
    auto *bar=new QHBoxLayout;bar->setSpacing(2);
    tabs=new QComboBox;tabs->setObjectName("contentTabs");tabs->addItems({QStringLiteral("内容库"),QStringLiteral("近期使用")});tabs->setFixedWidth(96);bar->addWidget(tabs);
    libraries=new QComboBox;libraries->setObjectName("contentLibraries");libraries->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);libraries->setMinimumContentsLength(2);libraries->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);libraries->setMinimumWidth(48);libraries->setMaximumWidth(240);bar->addWidget(libraries,1);
    search=new QComboBox;search->setObjectName("contentSearch");search->setEditable(true);search->setInsertPolicy(QComboBox::NoInsert);search->setCompleter(nullptr);search->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);search->setMaxVisibleItems(10);search->addItems(history.searches());search->setCurrentIndex(-1);
    search->setMinimumWidth(80);search->lineEdit()->setPlaceholderText(QStringLiteral("搜索…"));search->lineEdit()->setClearButtonEnabled(true);search->lineEdit()->setToolTip(QStringLiteral("按文件名或路径包含匹配；Enter 保存搜索记录，Esc 清空"));search->lineEdit()->installEventFilter(o);bar->addWidget(search,3);
    auto *options=button(QStringLiteral("⋯"),QStringLiteral("搜索范围、图标大小与刷新"),o);options->setObjectName("contentOptions");options->setFixedWidth(24);options->setPopupMode(QToolButton::InstantPopup);auto *menu=new QMenu(options);options->setMenu(menu);bar->addWidget(options);layout->addLayout(bar);
    auto *up=menu->addAction(QStringLiteral("上一级目录（Alt＋↑）"));auto *up_shortcut=new QShortcut(QKeySequence(Qt::ALT|Qt::Key_Up),o);up_shortcut->setContext(Qt::WidgetWithChildrenShortcut);QObject::connect(up_shortcut,&QShortcut::activated,up,&QAction::trigger);
    auto *refresh=menu->addAction(QStringLiteral("刷新内容库"));refresh->setObjectName("contentRefresh");
    menu->addSection(QStringLiteral("搜索范围"));scope=new QComboBox;scope->setObjectName("contentScope");scope->addItems({QStringLiteral("全部内容库"),QStringLiteral("当前内容库"),QStringLiteral("当前目录及子目录")});auto *scope_action=new QWidgetAction(menu);scope_action->setDefaultWidget(scope);menu->addAction(scope_action);
    menu->addSection(QStringLiteral("图标大小（Ctrl＋滚轮）"));zoom=new QSlider(Qt::Horizontal);zoom->setObjectName("contentZoom");zoom->setRange(0,maximum_zoom);zoom->setMinimumWidth(180);zoom->setToolTip(QStringLiteral("列表，或 64–224 像素图标，每档 8 像素"));auto *zoom_action=new QWidgetAction(menu);zoom_action->setDefaultWidget(zoom);menu->addAction(zoom_action);
    splitter=new QSplitter;splitter->setObjectName("contentSplitter");left=new QStackedWidget;
    files=new QFileSystemModel(o);files->setReadOnly(true);files->setOption(QFileSystemModel::DontUseCustomDirectoryIcons);files->setNameFilters({"*.duf","HD Nipples for G8F - 2.0.dse"});files->setNameFilterDisables(false);
    tree=new QTreeView;tree->setObjectName("contentTree");tree->setModel(files);tree->setHeaderHidden(true);tree->setUniformRowHeights(true);tree->setMinimumWidth(85);for(int i=1;i<4;++i) tree->hideColumn(i);tree->viewport()->installEventFilter(o);left->addWidget(tree);
    categories=new QListWidget;categories->setObjectName("contentCategories");categories->setMinimumWidth(105);for(const auto &c:content_categories()) {auto *item=new QListWidgetItem(category_label(c),categories);item->setData(Qt::UserRole,c);}categories->setCurrentRow(0);categories->viewport()->installEventFilter(o);left->addWidget(categories);splitter->addWidget(left);
    view=new QListView;view->setObjectName("contentItems");model=new ContentModel(thumbnails,o);view->setModel(model);view->setUniformItemSizes(true);view->setLayoutMode(QListView::Batched);view->setBatchSize(100);view->setResizeMode(QListView::Adjust);view->setSelectionMode(QAbstractItemView::SingleSelection);view->setEditTriggers(QAbstractItemView::NoEditTriggers);view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);view->setMouseTracking(true);view->viewport()->installEventFilter(o);view->setContextMenuPolicy(Qt::CustomContextMenu);splitter->addWidget(view);splitter->setStretchFactor(1,1);layout->addWidget(splitter,1);
    empty=new QLabel(view->viewport());empty->setObjectName("contentEmpty");empty->setAlignment(Qt::AlignCenter);empty->setWordWrap(true);empty->setAttribute(Qt::WA_TransparentForMouseEvents);empty->hide();
    preview=new QFrame(o,Qt::ToolTip);preview->setObjectName("contentPreview");preview->setFrameShape(QFrame::StyledPanel);auto *preview_layout=new QVBoxLayout(preview);preview_image=new QLabel;preview_image->setObjectName("contentPreviewImage");preview_image->setAlignment(Qt::AlignCenter);preview_text=new QLabel;preview_text->setWordWrap(true);preview_text->setMaximumWidth(420);preview_text->setTextFormat(Qt::PlainText);preview_layout->addWidget(preview_image);preview_layout->addWidget(preview_text);
    query_timer.setSingleShot(true);query_timer.setInterval(140);history_timer.setSingleShot(true);history_timer.setInterval(900);folder_timer.setSingleShot(true);folder_timer.setInterval(180);repaint_timer.setSingleShot(true);repaint_timer.setInterval(25);
    QObject::connect(&repaint_timer,&QTimer::timeout,o,[this]{view->viewport()->update();});
    thumbnails.ready=[this](const QString &path,bool tip) {if(tip&&hovered==path) show_preview(path);if(!repaint_timer.isActive()) repaint_timer.start();};
    index.changed=[this]{owner->setProperty("contentIndexSize",qulonglong(index.size()));if(!search->currentText().trimmed().isEmpty()&&!recent()) query_timer.start();};
    QObject::connect(&query_timer,&QTimer::timeout,o,[this]{show_items();});
    QObject::connect(&history_timer,&QTimer::timeout,o,[this]{remember_search();});
    QObject::connect(&folder_timer,&QTimer::timeout,o,[this]{if(!recent()&&search->currentText().trimmed().isEmpty()) show_items();});
    QObject::connect(&watcher,&QFileSystemWatcher::directoryChanged,o,[this]{thumbnails.clear();folder_timer.start();});
    QObject::connect(search,&QComboBox::editTextChanged,o,[this]{if(restoring) return;index.cancel_search();++directory_generation;query_timer.start();history_timer.start();});
    QObject::connect(search->lineEdit(),&QLineEdit::returnPressed,o,[this]{query_timer.stop();remember_search();show_items();});
    QObject::connect(search,&QComboBox::activated,o,[this]{remember_search();show_items();});
    QObject::connect(scope,&QComboBox::currentIndexChanged,o,[this]{show_items();});
    QObject::connect(libraries,&QComboBox::currentIndexChanged,o,[this](int row){if(restoring||row<0) return;navigate(roots.value(row));});
    QObject::connect(tree->selectionModel(),&QItemSelectionModel::currentChanged,o,[this](const QModelIndex &item){if(restoring) return;const auto path=files->filePath(item);if(files->isDir(item)) navigate(path,false);});
    QObject::connect(tree,&QTreeView::doubleClicked,o,[this](const QModelIndex &item){if(!files->isDir(item)) activate(files->filePath(item));});
    QObject::connect(view,&QListView::doubleClicked,o,[this](const QModelIndex &item){activate_item(item);});
    view->installEventFilter(o);
    QObject::connect(view->verticalScrollBar(),&QScrollBar::valueChanged,o,[this]{hide_preview();});
    QObject::connect(view,&QWidget::customContextMenuRequested,o,[this](const QPoint &point){const auto i=view->indexAt(point);if(!i.isValid()) return;const auto item=model->items[size_t(i.row())];QMenu menu(owner);auto *open=menu.addAction(item.directory?QStringLiteral("打开目录"):QStringLiteral("添加 / 应用 DUF"));auto *locate=menu.addAction(QStringLiteral("在内容库中定位"));auto *system=menu.addAction(QStringLiteral("在资源管理器中打开所在目录"));const auto *selected=menu.exec(view->viewport()->mapToGlobal(point));if(selected==open) activate_item(i);else if(selected==locate) {tabs->setCurrentIndex(0);search->setEditText({});navigate(item.directory?item.path:QFileInfo(item.path).path());}else if(selected==system) QDesktopServices::openUrl(QUrl::fromLocalFile(item.directory?item.path:QFileInfo(item.path).path()));});
    QObject::connect(up,&QAction::triggered,o,[this]{if(recent()) return;const auto root=libraries->currentText();if(directory.compare(root,Qt::CaseInsensitive)!=0) navigate(QFileInfo(directory).path());});
    QObject::connect(refresh,&QAction::triggered,o,[this]{thumbnails.clear();show_items();index.refresh(roots);});
    QObject::connect(tabs,&QComboBox::currentIndexChanged,o,[this]{
      query_timer.stop();history_timer.stop();
      {const QSignalBlocker block(search);if(recent()) {library_query=search->currentText();search->setEditText({});}else search->setEditText(library_query);}
      if(recent()) categories->setCurrentRow(0);show_items();
    });
    QObject::connect(categories,&QListWidget::currentRowChanged,o,[this]{if(recent()) show_items();});
    QObject::connect(zoom,&QSlider::valueChanged,o,[this]{const bool entering_icons=view->isHidden()&&zoom->value()>0;apply_zoom();if(entering_icons) show_items();history.settings().setValue("content/iconSize",icon_pixels(zoom->value()));history.settings().sync();});
    refresh_timer.setInterval(5*60*1000);QObject::connect(&refresh_timer,&QTimer::timeout,o,[this]{if(!index.scanning()) index.refresh(roots);});refresh_timer.start();
    const int old_sizes[]={0,64,112,160,224};const int previous=old_sizes[std::clamp(history.settings().value("content/zoom",2).toInt(),0,4)];
    const int pixels=history.settings().value("content/iconSize",previous).toInt();zoom->setValue(pixels==0?0:1+(std::clamp(pixels,64,224)-64+4)/8);apply_zoom();
    scope->setCurrentIndex(std::clamp(history.settings().value("content/scope",0).toInt(),0,2));
    splitter->setSizes({150,360});const auto saved=history.settings().value("content/splitter").toByteArray();if(!saved.isEmpty()) splitter->restoreState(saved);
  }
  ~Impl() {++directory_generation;directories.clear();directories.waitForDone();index.changed={};index.progress={};thumbnails.ready={};}
  bool recent() const {return tabs->currentIndex()==1;}
  void hide_preview() {hovered.clear();hover_row=-1;preview->hide();}
  void show_preview(const QString &path) {
    if(hover_row<0||size_t(hover_row)>=model->items.size()||model->items[size_t(hover_row)].path!=path) {hide_preview();return;}
    const auto image=thumbnails.get(path,true);preview_image->setVisible(!image.isNull());if(!image.isNull()) preview_image->setPixmap(image.scaled(384,384,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    const auto &item=model->items[size_t(hover_row)];auto description=path;if(item.used) description+="\n"+category_label(item.category)+" · "+QDateTime::fromMSecsSinceEpoch(item.used).toString("yyyy-MM-dd HH:mm:ss");
    preview_text->setText(description);preview->adjustSize();auto point=hover_position+QPoint(18,20);const auto area=owner->screen()->availableGeometry();point.setX(std::clamp(point.x(),area.left(),std::max(area.left(),area.right()-preview->width())));point.setY(std::clamp(point.y(),area.top(),std::max(area.top(),area.bottom()-preview->height())));preview->move(point);preview->show();
  }
  void remember_search() {const auto q=search->currentText();history.searched(q);const QSignalBlocker block(search);search->clear();search->addItems(history.searches());search->setEditText(q);}
  void activate(const QString &path) {hide_preview();if(owner->open_asset) owner->open_asset(path);}
  void activate_item(const QModelIndex &i) {if(!i.isValid()) return;const auto item=model->items[size_t(i.row())];if(item.directory) navigate(item.path);else activate(item.path);}
  void navigate(const QString &path,bool select_tree=true) {
    if(path.isEmpty()) return;directory=content_path(path);restoring=true;
    for(int i=0;i<roots.size();++i) if(directory.compare(roots[i],Qt::CaseInsensitive)==0||directory.startsWith(roots[i]+"/",Qt::CaseInsensitive)) {libraries->setCurrentIndex(i);break;}
    const auto root=files->setRootPath(libraries->currentText());tree->setRootIndex(root);
    if(select_tree) {const auto i=files->index(directory);tree->setCurrentIndex(i);tree->scrollTo(i);}
    restoring=false;const auto watching=watcher.directories();if(!watching.isEmpty()) watcher.removePaths(watching);watcher.addPath(directory);show_items();
  }
  void apply_zoom() {
    hide_preview();const auto anchor=QPersistentModelIndex(view->currentIndex().isValid()?view->currentIndex():view->indexAt(view->viewport()->rect().center()));
    const int level=zoom->value();files->setFilter(level==0?QDir::AllDirs|QDir::Files|QDir::NoDotAndDotDot:QDir::AllDirs|QDir::NoDotAndDotDot);
    const int size=level?icon_pixels(level):20;
    view->setViewMode(level?QListView::IconMode:QListView::ListMode);view->setFlow(level?QListView::LeftToRight:QListView::TopToBottom);view->setWrapping(level!=0);view->setMovement(QListView::Static);view->setWordWrap(level!=0);view->setIconSize(QSize(size,size));view->setGridSize(level?QSize(size+8,size+2*view->fontMetrics().height()+6):QSize());view->setSpacing(0);view->setTextElideMode(Qt::ElideRight);
    update_visibility();
    if(anchor.isValid()) QTimer::singleShot(0,owner,[this,anchor]{if(anchor.isValid()&&view->isVisible()) view->scrollTo(anchor,QAbstractItemView::PositionAtCenter);});
  }
  void update_visibility() {const bool is_recent=recent(),searching=!search->currentText().trimmed().isEmpty();left->setCurrentWidget(is_recent?static_cast<QWidget *>(categories):tree);view->setVisible(is_recent||searching||zoom->value()>0);libraries->setEnabled(!is_recent);libraries->setToolTip(directory);scope->setEnabled(!is_recent);if(view->isHidden()) empty->hide();}
  void display(std::vector<Item> items) {
    hide_preview();model->replace(std::move(items));empty->setText(recent()?QStringLiteral("暂无近期使用记录"):roots.empty()?QStringLiteral("请在项目设置中添加内容库"):QStringLiteral("没有匹配的资源"));empty->setGeometry(view->viewport()->rect().adjusted(8,8,-8,-8));empty->setVisible(model->items.empty()&&view->isVisible());
  }
  void show_items() {
    if(restoring) return;index.cancel_search();const auto generation=++directory_generation;directories.clear();update_visibility();
    const auto query=search->currentText().trimmed();
    if(recent()) {std::vector<Item> items;const auto category=categories->currentItem()->data(Qt::UserRole).toString();for(const auto &r:history.recent(category)) if(r.path.contains(query,Qt::CaseInsensitive)) items.push_back({r.path,r.category,false,r.used});display(std::move(items));return;}
    if(!query.isEmpty()) {
      const auto scope_path=scope->currentIndex()==0?QString():scope->currentIndex()==1?libraries->currentText():directory;
      index.search(query,scope_path,[this](ContentSearch result) {
        std::vector<Item> items;items.reserve(result.paths.size());for(auto &path:result.paths) items.push_back({std::move(path)});
        display(std::move(items));
      });return;
    }
    if(directory.isEmpty()) {display({});return;}
    if(zoom->value()==0) {empty->hide();return;}
    const auto folder=directory;directories.start([this,folder,generation] {
      const QDir dir(folder);const auto entries=dir.entryInfoList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot,QDir::DirsFirst|QDir::Name|QDir::IgnoreCase);std::vector<Item> items;
      for(const auto &entry:entries) {if(generation!=directory_generation) return;if(entry.isDir()||daz::supported_content_entry(std::filesystem::path(entry.absoluteFilePath().toStdWString()))) items.push_back({entry.absoluteFilePath(),{},entry.isDir()});}
      QMetaObject::invokeMethod(owner,[this,generation,items=std::move(items)]() mutable {if(generation!=directory_generation) return;display(std::move(items));},Qt::QueuedConnection);
    });
  }
};

ContentBrowser::ContentBrowser(QWidget *parent,const QString &settings,const QString &cache):QWidget(parent) {impl_=std::make_unique<Impl>(this,settings,cache);setMinimumWidth(270);}
ContentBrowser::~ContentBrowser() {save();}
void ContentBrowser::set_roots(const QStringList &roots) {
  auto &p=*impl_;p.restoring=true;p.roots.clear();p.libraries->clear();for(const auto &root:roots) {const auto path=content_path(root);if(!p.roots.contains(path,Qt::CaseInsensitive)) p.roots.append(path);}p.libraries->addItems(p.roots);p.restoring=false;
  auto directory=p.directory.isEmpty()?p.history.settings().value("content/directory").toString():p.directory;
  bool valid=false;for(const auto &root:p.roots) if(directory.compare(root,Qt::CaseInsensitive)==0||directory.startsWith(root+"/",Qt::CaseInsensitive)) valid=true;
  if(!valid||!QFileInfo(directory).isDir()) directory=p.roots.value(0);p.directory=directory;if(!directory.isEmpty()) p.navigate(directory);else {p.tree->setRootIndex(p.files->setRootPath({}));p.tree->setEnabled(false);p.show_items();}p.tree->setEnabled(!p.roots.isEmpty());p.index.refresh(p.roots,true);
}
void ContentBrowser::record_use(const QString &path,const QString &category) {impl_->history.used(path,category);if(impl_->recent()) impl_->show_items();}
void ContentBrowser::show_recent() {impl_->tabs->setCurrentIndex(1);impl_->categories->setCurrentRow(0);impl_->show_items();}
void ContentBrowser::save() {if(!impl_) return;auto &p=*impl_;auto &settings=p.history.settings();settings.setValue("content/iconSize",icon_pixels(p.zoom->value()));settings.setValue("content/splitter",p.splitter->saveState());settings.setValue("content/directory",p.directory);settings.setValue("content/scope",p.scope->currentIndex());settings.sync();}
bool ContentBrowser::eventFilter(QObject *object,QEvent *event) {
  if(!impl_) return QWidget::eventFilter(object,event);auto &p=*impl_;
  if(event->type()==QEvent::Wheel) {auto *wheel=static_cast<QWheelEvent *>(event);if(wheel->modifiers()&Qt::ControlModifier) {p.wheel_remainder+=wheel->angleDelta().y();const int steps=p.wheel_remainder/120;p.wheel_remainder%=120;if(steps) p.zoom->setValue(std::clamp(p.zoom->value()+steps,0,maximum_zoom));wheel->accept();return true;}}
  if(event->type()==QEvent::KeyPress) {auto *key=static_cast<QKeyEvent *>(event);if(object==p.search->lineEdit()&&key->key()==Qt::Key_Escape) {p.search->setEditText({});return true;}if(object==p.view&&(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter)) {p.activate_item(p.view->currentIndex());return true;}}
  if(object==p.view->viewport()) {
    if(event->type()==QEvent::Resize) p.empty->setGeometry(p.view->viewport()->rect().adjusted(8,8,-8,-8));
    if(event->type()==QEvent::Leave||event->type()==QEvent::Wheel||event->type()==QEvent::MouseButtonPress) p.hide_preview();
    if(event->type()==QEvent::MouseMove&&!p.hovered.isEmpty()) {const auto i=p.view->indexAt(static_cast<QMouseEvent *>(event)->position().toPoint());if(!i.isValid()||p.model->items[size_t(i.row())].path!=p.hovered) p.hide_preview();}
    if(event->type()==QEvent::ToolTip) {const auto *help=static_cast<QHelpEvent *>(event);const auto i=p.view->indexAt(help->pos());if(i.isValid()) {p.hover_row=i.row();p.hover_position=help->globalPos();p.hovered=p.model->items[size_t(i.row())].path;p.show_preview(p.hovered);}return true;}
  }
  return QWidget::eventFilter(object,event);
}
}
