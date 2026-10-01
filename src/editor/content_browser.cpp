#include "editor/content_browser.h"
#include "editor/ui_scale.h"
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
#include <QHeaderView>
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
#include <optional>

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
  std::vector<std::filesystem::path> roots;
  std::function<void(const QString &,bool)> ready;
  explicit Thumbnails(QObject *parent):QObject(parent) {pool_.setMaxThreadCount(2);}
  ~Thumbnails() override {++generation_;pool_.clear();pool_.waitForDone();}
  void clear() {++generation_;pool_.clear();pending_.clear();cache_.clear();}
  QPixmap get(const QString &path,bool tip=false) {
    const auto key=path+(tip?"|tip":"|icon");
    if(auto *cached=cache_.object(key)) {if(QDateTime::currentMSecsSinceEpoch()-cached->loaded<60000) return cached->image;cache_.remove(key);}
    if(pending_.contains(key)||pending_.size()>=96) return {};
    pending_.insert(key);const auto generation=generation_.load();
    pool_.start([this,path,key,tip,generation,libraries=roots] {
      QImage image;const int extent=tip?512:320;
      auto candidates=preview_candidates(path,tip);
      if(path.endsWith(".djl",Qt::CaseInsensitive))try{candidates.append(preview_candidates(QString::fromStdWString(daz::content_asset(std::filesystem::path(path.toStdWString()),libraries).wstring()),tip));}catch(const std::exception &){}
      for(const auto &candidate:candidates) {
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
    if(role==Qt::DecorationRole) {auto image=thumbnails_.get(item.path);return image.isNull()?(item.directory?folder_:file_):QIcon(image);}
    return {};
  }
  void replace(std::vector<Item> next) {beginResetModel();items=std::move(next);endResetModel();}
};
QToolButton *button(const QString &text,const QString &tip,QWidget *parent) {auto *b=new QToolButton(parent);b->setText(text);b->setToolTip(tip);return b;}
class ContentFiles final:public QFileSystemModel {
  Thumbnails &thumbnails_;
public:
  ContentFiles(Thumbnails &thumbnails,QObject *parent):QFileSystemModel(parent),thumbnails_(thumbnails){}
  QVariant data(const QModelIndex &index,int role=Qt::DisplayRole)const override {
    if(role==Qt::DecorationRole&&index.column()==0){const auto image=thumbnails_.get(filePath(index));if(!image.isNull())return QIcon(image);}
    return QFileSystemModel::data(index,role);
  }
};
class Breadcrumbs final:public QWidget {
  QHBoxLayout *row_;QToolButton *overflow_;std::vector<QToolButton *> parts_;
  void fit(){
    int available=width(),used=0;size_t first=parts_.size();
    while(first>0){const int next=std::min(parts_[first-1]->sizeHint().width(),std::max(24,available-28));if(used+next+(first>1?28:0)>available&&first<parts_.size())break;used+=next;--first;}
    overflow_->setVisible(first>0);auto *menu=overflow_->menu();menu->clear();
    for(size_t i=0;i<parts_.size();++i){auto *part=parts_[i];part->setVisible(i>=first);part->setMaximumWidth(std::max(24,available-(first?28:0)));if(i<first){auto *action=menu->addAction(part->toolTip());connect(action,&QAction::triggered,part,&QToolButton::click);}}
  }
  void resizeEvent(QResizeEvent *event)override{QWidget::resizeEvent(event);fit();}
public:
  std::function<void(const QString &)> navigate;
  Breadcrumbs(){setObjectName("contentBreadcrumbs");setMinimumWidth(40);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);row_=new QHBoxLayout(this);row_->setContentsMargins(0,0,0,0);row_->setSpacing(0);overflow_=button(QStringLiteral("»"),QStringLiteral("上级目录"),this);overflow_->setObjectName("contentBreadcrumbOverflow");overflow_->setFixedWidth(28);overflow_->setMenu(new QMenu(overflow_));overflow_->setPopupMode(QToolButton::InstantPopup);row_->addWidget(overflow_);row_->addStretch();}
  void path(const QString &directory,const QString &root){
    for(auto *part:parts_)delete part;parts_.clear();setToolTip(directory);if(directory.isEmpty()){fit();return;}
    QString base=root;if(base.isEmpty()||!(directory.compare(base,Qt::CaseInsensitive)==0||directory.startsWith(base+"/",Qt::CaseInsensitive)))base=QDir(directory).rootPath();
    QString current=QDir::cleanPath(base);QStringList paths{current};auto relative=QDir(base).relativeFilePath(directory);if(relative!=".")for(const auto &name:relative.split('/',Qt::SkipEmptyParts)){current=QDir::cleanPath(current+"/"+name);paths.append(current);}
    for(const auto &path:paths){auto name=QFileInfo(path).fileName();if(name.isEmpty())name=path;auto *part=button(name+QStringLiteral(" ›"),path,this);part->setProperty("contentBreadcrumbPath",path);part->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);connect(part,&QToolButton::clicked,this,[this,path]{if(navigate)navigate(path);});row_->insertWidget(row_->count()-1,part);parts_.push_back(part);}fit();
  }
};
}

struct ContentBrowser::Impl {
  ContentBrowser *owner;
  ContentHistory history;
  ContentIndex index;
  Thumbnails thumbnails;
  QThreadPool directories;
  QThreadPool locator;
  std::atomic<uint64_t> directory_generation=0;
  std::atomic<uint64_t> locate_generation=0;
  std::optional<ContentOrigin> locating;
  bool locate_running=false,location_view=false;
  QComboBox *libraries,*search,*scope;
  Breadcrumbs *breadcrumbs;
  QComboBox *tabs;
  QTreeView *tree;
  QFileSystemModel *files;
  QListWidget *categories;
  QListView *view;
  ContentModel *model;
  QStackedWidget *left;
  QSplitter *splitter;
  QLabel *empty,*preview_image,*preview_text,*notice;
  QFrame *preview;
  QSlider *zoom;
  QTimer query_timer,history_timer,refresh_timer,folder_timer,repaint_timer;
  QFileSystemWatcher watcher;
  QStringList roots;
  QString directory,hovered;
  QPoint hover_position;
  int hover_row=-1;
  int wheel_remainder=0;
  bool restoring=false;
  QString pending_tree,pending_item;
  void reveal_tree(){
    if(pending_tree.isEmpty())return;const auto i=files->index(pending_tree);if(!i.isValid())return;
    const bool was_restoring=restoring;restoring=true;QList<QModelIndex> parents;for(auto p=i.parent();p.isValid()&&p!=tree->rootIndex();p=p.parent())parents.prepend(p);
    for(const auto &p:parents){tree->expand(p);if(files->canFetchMore(p))files->fetchMore(p);}tree->setCurrentIndex(i);tree->doItemsLayout();tree->resizeColumnToContents(0);tree->scrollTo(i,QAbstractItemView::PositionAtCenter);
    // 深层目录的缩进可能占满左栏；横向滚到目录名称开头，而不是只露出箭头和图标。
    tree->horizontalScrollBar()->setValue(std::max(0,tree->horizontalScrollBar()->value()+tree->visualRect(i).left()-ui_pixels(8)));restoring=was_restoring;
  }

  Impl(ContentBrowser *o,const QString &settings_file,const QString &cache):owner(o),history(settings_file),index(cache.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/content-v1":cache),thumbnails(o) {
    directories.setMaxThreadCount(1);locator.setMaxThreadCount(1);
    auto *layout=new QVBoxLayout(o);layout->setContentsMargins(1,1,1,1);layout->setSpacing(2);
    auto *bar=new QHBoxLayout;bar->setSpacing(2);
    tabs=new QComboBox;tabs->setObjectName("contentTabs");tabs->addItems({QStringLiteral("内容库"),QStringLiteral("近期使用")});tabs->setFixedWidth(96);bar->addWidget(tabs);
    libraries=new QComboBox;libraries->setObjectName("contentLibraries");libraries->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);libraries->setMinimumContentsLength(2);libraries->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);libraries->setMinimumWidth(48);libraries->setMaximumWidth(240);bar->addWidget(libraries,1);
    breadcrumbs=new Breadcrumbs;bar->addWidget(breadcrumbs,3);breadcrumbs->navigate=[this](const QString &path){owner->locate(path);};
    search=new QComboBox;search->setObjectName("contentSearch");search->setEditable(true);search->setInsertPolicy(QComboBox::NoInsert);search->setCompleter(nullptr);search->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);search->setMaxVisibleItems(10);search->addItems(history.searches());search->setCurrentIndex(-1);
    search->setMinimumWidth(80);search->lineEdit()->setPlaceholderText(QStringLiteral("搜索…"));search->lineEdit()->setClearButtonEnabled(true);search->lineEdit()->setToolTip(QStringLiteral("按文件名或路径包含匹配；Enter 保存搜索记录，Esc 清空"));search->lineEdit()->installEventFilter(o);bar->addWidget(search,3);
    auto *options=button(QStringLiteral("⋯"),QStringLiteral("搜索范围、图标大小与刷新"),o);options->setObjectName("contentOptions");options->setFixedWidth(24);options->setPopupMode(QToolButton::InstantPopup);auto *menu=new QMenu(options);options->setMenu(menu);bar->addWidget(options);layout->addLayout(bar);
    notice=new QLabel;notice->setObjectName("contentNotice");notice->setWordWrap(true);notice->hide();layout->addWidget(notice);
    auto *up=menu->addAction(QStringLiteral("上一级目录（Alt＋↑）"));auto *up_shortcut=new QShortcut(QKeySequence(Qt::ALT|Qt::Key_Up),o);up_shortcut->setContext(Qt::WidgetWithChildrenShortcut);QObject::connect(up_shortcut,&QShortcut::activated,up,&QAction::trigger);
    auto *refresh=menu->addAction(QStringLiteral("刷新内容库"));refresh->setObjectName("contentRefresh");
    menu->addSection(QStringLiteral("搜索范围"));scope=new QComboBox;scope->setObjectName("contentScope");scope->addItems({QStringLiteral("全部内容库"),QStringLiteral("当前内容库"),QStringLiteral("当前目录及子目录")});auto *scope_action=new QWidgetAction(menu);scope_action->setDefaultWidget(scope);menu->addAction(scope_action);
    menu->addSection(QStringLiteral("图标大小（Ctrl＋滚轮）"));zoom=new QSlider(Qt::Horizontal);zoom->setObjectName("contentZoom");zoom->setRange(0,maximum_zoom);zoom->setMinimumWidth(180);zoom->setToolTip(QStringLiteral("列表，或 64–224 像素图标，每档 8 像素"));auto *zoom_action=new QWidgetAction(menu);zoom_action->setDefaultWidget(zoom);menu->addAction(zoom_action);
    splitter=new QSplitter;splitter->setObjectName("contentSplitter");left=new QStackedWidget;
    files=new ContentFiles(thumbnails,o);files->setReadOnly(true);files->setOption(QFileSystemModel::DontUseCustomDirectoryIcons);files->setNameFilters({"*.duf","*.djl","HD Nipples for G8F - 2.0.dse"});files->setNameFilterDisables(false);
    tree=new QTreeView;tree->setObjectName("contentTree");tree->setModel(files);tree->setHeaderHidden(true);tree->header()->setStretchLastSection(false);tree->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);tree->setUniformRowHeights(true);tree->setMinimumWidth(85);for(int i=1;i<4;++i) tree->hideColumn(i);tree->viewport()->installEventFilter(o);left->addWidget(tree);
    categories=new QListWidget;categories->setObjectName("contentCategories");categories->setMinimumWidth(105);for(const auto &c:content_categories()) {auto *item=new QListWidgetItem(category_label(c),categories);item->setData(Qt::UserRole,c);}categories->setCurrentRow(0);categories->viewport()->installEventFilter(o);left->addWidget(categories);splitter->addWidget(left);
    view=new QListView;view->setObjectName("contentItems");model=new ContentModel(thumbnails,o);view->setModel(model);view->setUniformItemSizes(true);view->setLayoutMode(QListView::Batched);view->setBatchSize(100);view->setResizeMode(QListView::Adjust);view->setSelectionMode(QAbstractItemView::SingleSelection);view->setEditTriggers(QAbstractItemView::NoEditTriggers);view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);view->setMouseTracking(true);view->viewport()->installEventFilter(o);view->setContextMenuPolicy(Qt::CustomContextMenu);splitter->addWidget(view);splitter->setStretchFactor(1,1);layout->addWidget(splitter,1);
    empty=new QLabel(view->viewport());empty->setObjectName("contentEmpty");empty->setAlignment(Qt::AlignCenter);empty->setWordWrap(true);empty->setAttribute(Qt::WA_TransparentForMouseEvents);empty->hide();
    preview=new QFrame(o,Qt::ToolTip);preview->setObjectName("contentPreview");preview->setFrameShape(QFrame::StyledPanel);auto *preview_layout=new QVBoxLayout(preview);preview_image=new QLabel;preview_image->setObjectName("contentPreviewImage");preview_image->setAlignment(Qt::AlignCenter);preview_text=new QLabel;preview_text->setWordWrap(true);preview_text->setMaximumWidth(420);preview_text->setTextFormat(Qt::PlainText);preview_layout->addWidget(preview_image);preview_layout->addWidget(preview_text);
    query_timer.setSingleShot(true);query_timer.setInterval(140);history_timer.setSingleShot(true);history_timer.setInterval(900);folder_timer.setSingleShot(true);folder_timer.setInterval(180);repaint_timer.setSingleShot(true);repaint_timer.setInterval(25);
    QObject::connect(&repaint_timer,&QTimer::timeout,o,[this]{view->viewport()->update();tree->viewport()->update();});
    thumbnails.ready=[this](const QString &path,bool tip) {if(tip&&hovered==path) show_preview(path);if(!repaint_timer.isActive()) repaint_timer.start();};
    index.changed=[this]{owner->setProperty("contentIndexSize",qulonglong(index.size()));if(!active_query().isEmpty())query_timer.start();try_locate();};
    QObject::connect(&query_timer,&QTimer::timeout,o,[this]{show_items();});
    QObject::connect(&history_timer,&QTimer::timeout,o,[this]{remember_search();});
    QObject::connect(&folder_timer,&QTimer::timeout,o,[this]{if(!recent()&&active_query().isEmpty()) show_items();});
    QObject::connect(&watcher,&QFileSystemWatcher::directoryChanged,o,[this]{thumbnails.clear();folder_timer.start();});
    QObject::connect(search,&QComboBox::editTextChanged,o,[this]{if(restoring||recent())return;resume_search();index.cancel_search();++directory_generation;query_timer.start();history_timer.start();});
    QObject::connect(search->lineEdit(),&QLineEdit::returnPressed,o,[this]{if(recent())return;resume_search();query_timer.stop();remember_search();show_items();});
    QObject::connect(search,&QComboBox::activated,o,[this]{if(recent())return;resume_search();remember_search();show_items();});
    QObject::connect(scope,&QComboBox::currentIndexChanged,o,[this]{resume_search();show_items();});
    QObject::connect(libraries,&QComboBox::currentIndexChanged,o,[this](int row){if(restoring||row<0)return;resume_search();navigate(roots.value(row));});
    QObject::connect(tree->selectionModel(),&QItemSelectionModel::currentChanged,o,[this](const QModelIndex &item){if(restoring)return;const auto path=files->filePath(item);if(files->isDir(item)){resume_search();navigate(path,false);}});
    QObject::connect(files,&QFileSystemModel::directoryLoaded,o,[this](const QString &folder){const auto path=content_path(folder);if(!pending_tree.isEmpty()&&(pending_tree.compare(path,Qt::CaseInsensitive)==0||pending_tree.startsWith(path+"/",Qt::CaseInsensitive)))QTimer::singleShot(0,owner,[this]{reveal_tree();});});
    QObject::connect(tree,&QTreeView::clicked,o,[this](const QModelIndex &item){if(!owner->apply_pose||files->isDir(item))return;const auto path=files->filePath(item);if(pose_file(path)){hide_preview();owner->apply_pose(path,QApplication::keyboardModifiers().testFlag(Qt::ControlModifier));}});
    QObject::connect(tree,&QTreeView::doubleClicked,o,[this](const QModelIndex &item){if(!files->isDir(item)&&!(owner->apply_pose&&pose_file(files->filePath(item)))) activate(files->filePath(item));});
    QObject::connect(view,&QListView::clicked,o,[this](const QModelIndex &index){if(!owner->apply_pose||!pose_item(index))return;const auto path=model->items[size_t(index.row())].path;hide_preview();owner->apply_pose(path,QApplication::keyboardModifiers().testFlag(Qt::ControlModifier));});
    QObject::connect(view,&QListView::doubleClicked,o,[this](const QModelIndex &item){if(owner->apply_pose&&pose_item(item))return;activate_item(item);});
    view->installEventFilter(o);
    QObject::connect(view->verticalScrollBar(),&QScrollBar::valueChanged,o,[this]{hide_preview();});
    QObject::connect(view,&QWidget::customContextMenuRequested,o,[this](const QPoint &point){
      const auto i=view->indexAt(point);if(!i.isValid())return;const auto item=model->items[size_t(i.row())];hide_preview();
      QMenu menu(owner);auto *open=menu.addAction(item.directory?QStringLiteral("打开目录"):QStringLiteral("添加 / 应用 DUF"));
      auto *locate=menu.addAction(QStringLiteral("在内容库中定位"));auto *system=menu.addAction(QStringLiteral("在资源管理器中打开所在目录"));
      QAction *remove=nullptr;if(recent()){menu.addSeparator();remove=menu.addAction(QStringLiteral("从近期使用中移除"));remove->setObjectName("RemoveRecentContent");}
      const auto *selected=menu.exec(view->viewport()->mapToGlobal(point));
      if(selected==open)activate_item(i);
      else if(selected==locate)owner->locate(item.path);
      else if(selected==system)QDesktopServices::openUrl(QUrl::fromLocalFile(item.directory?item.path:QFileInfo(item.path).path()));
      else if(remove&&selected==remove){history.remove(item.path);show_items();}
    });
    QObject::connect(up,&QAction::triggered,o,[this]{if(recent()) return;const auto root=libraries->currentText();if(directory.compare(root,Qt::CaseInsensitive)!=0){resume_search();navigate(QFileInfo(directory).path());}});
    QObject::connect(refresh,&QAction::triggered,o,[this]{thumbnails.clear();show_items();index.refresh(roots);});
    QObject::connect(tabs,&QComboBox::currentIndexChanged,o,[this]{
      query_timer.stop();history_timer.stop();
      resume_search();
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
  ~Impl() {++locate_generation;locator.clear();locator.waitForDone();++directory_generation;directories.clear();directories.waitForDone();index.changed={};index.progress={};thumbnails.ready={};}
  bool recent() const {return tabs->currentIndex()==1;}
  QString active_query() const {return recent()||location_view?QString{}:search->currentText().trimmed();}
  void resume_search(){location_view=false;pending_item.clear();pending_tree.clear();notice->hide();++locate_generation;locator.clear();locating.reset();locate_running=false;}
  void try_locate(){
    if(!locating||locate_running||(index.size()==0&&index.scanning()))return;
    locate_running=true;const auto generation=locate_generation.load();const auto entries=index.entries();const auto origin=*locating;const auto libraries=roots;
    locator.start([this,generation,entries,origin,libraries]{
      QStringList paths;paths.reserve(qsizetype(entries->size()));for(const auto &entry:*entries){if(generation!=locate_generation)return;paths.append(entry.path);}
      const auto path=resolve_content_entry(origin,libraries,paths,[this,generation]{return generation!=locate_generation;});
      QMetaObject::invokeMethod(owner,[this,generation,path,entries]{
        if(generation!=locate_generation)return;locate_running=false;
        if(path.isEmpty()&&index.entries()!=entries){try_locate();return;}
        if(path.isEmpty()&&index.scanning())return;
        locating.reset();owner->setProperty("contentLocateState",path.isEmpty()?"missing":"found");owner->setProperty("contentLocatedPath",path);
        if(!path.isEmpty())owner->locate(path);else{notice->setText(QStringLiteral("未找到与此模型资产引用匹配的内容库入口。"));notice->show();}
      },Qt::QueuedConnection);
    });
  }
  void hide_preview() {hovered.clear();hover_row=-1;preview->hide();}
  void show_preview(const QString &path) {
    if(hover_row<0||size_t(hover_row)>=model->items.size()||model->items[size_t(hover_row)].path!=path) {hide_preview();return;}
    const auto image=thumbnails.get(path,true);preview_image->setVisible(!image.isNull());if(!image.isNull()) preview_image->setPixmap(image.scaled(384,384,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    const auto &item=model->items[size_t(hover_row)];auto description=path;if(item.used) description+="\n"+category_label(item.category)+" · "+QDateTime::fromMSecsSinceEpoch(item.used).toString("yyyy-MM-dd HH:mm:ss");
    preview_text->setText(description);preview->adjustSize();auto point=hover_position+QPoint(18,20);const auto area=owner->screen()->availableGeometry();point.setX(std::clamp(point.x(),area.left(),std::max(area.left(),area.right()-preview->width())));point.setY(std::clamp(point.y(),area.top(),std::max(area.top(),area.bottom()-preview->height())));preview->move(point);preview->show();
  }
  void remember_search() {if(recent())return;const auto q=search->currentText();history.searched(q);const QSignalBlocker block(search);search->clear();search->addItems(history.searches());search->setEditText(q);}
  bool pose_item(const QModelIndex &index) {
    if(!index.isValid())return false;auto &item=model->items[size_t(index.row())];if(item.directory)return false;
    if(item.category.isEmpty())return pose_file(item.path);
    return item.category=="pose";
  }
  bool pose_file(const QString &path) const {
    try{std::vector<std::filesystem::path> libraries;for(const auto &root:roots)libraries.emplace_back(root.toStdWString());const auto file=daz::content_asset(std::filesystem::path(path.toStdWString()),libraries);return content_category(*daz::document_view(file))=="pose";}catch(const std::exception &){return false;}
  }
  void activate(const QString &path) {hide_preview();if(owner->open_asset) owner->open_asset(path);}
  void activate_item(const QModelIndex &i) {if(!i.isValid()) return;const auto item=model->items[size_t(i.row())];if(item.directory){resume_search();navigate(item.path);}else activate(item.path);}
  void navigate(const QString &path,bool select_tree=true) {
    if(path.isEmpty()) return;pending_tree.clear();directory=content_path(path);restoring=true;bool in_library=false;
    for(int i=0;i<roots.size();++i) if(directory.compare(roots[i],Qt::CaseInsensitive)==0||directory.startsWith(roots[i]+"/",Qt::CaseInsensitive)) {libraries->setCurrentIndex(i);in_library=true;break;}
    const auto root=files->setRootPath(in_library?libraries->currentText():QFileInfo(directory).path());tree->setRootIndex(root);tree->setEnabled(true);
    if(select_tree) {pending_tree=directory;reveal_tree();QTimer::singleShot(0,owner,[this]{reveal_tree();});QTimer::singleShot(100,owner,[this]{reveal_tree();});}
    breadcrumbs->path(directory,in_library?libraries->currentText():QString{});restoring=false;const auto watching=watcher.directories();if(!watching.isEmpty()) watcher.removePaths(watching);watcher.addPath(directory);show_items();
  }
  void apply_zoom() {
    hide_preview();const auto anchor=QPersistentModelIndex(view->currentIndex().isValid()?view->currentIndex():view->indexAt(view->viewport()->rect().center()));
    const int level=zoom->value();files->setFilter(level==0?QDir::AllDirs|QDir::Files|QDir::NoDotAndDotDot:QDir::AllDirs|QDir::NoDotAndDotDot);
    const int size=ui_pixels(level?icon_pixels(level):20);
    view->setViewMode(level?QListView::IconMode:QListView::ListMode);view->setFlow(level?QListView::LeftToRight:QListView::TopToBottom);view->setWrapping(level!=0);view->setMovement(QListView::Static);view->setWordWrap(level!=0);view->setIconSize(QSize(size,size));view->setGridSize(level?QSize(size+ui_pixels(8),size+2*view->fontMetrics().height()+ui_pixels(6)):QSize());view->setSpacing(0);view->setTextElideMode(Qt::ElideRight);
    update_visibility();
    if(anchor.isValid()) QTimer::singleShot(0,owner,[this,anchor]{if(anchor.isValid()&&view->isVisible()) view->scrollTo(anchor,QAbstractItemView::PositionAtCenter);});
  }
  void update_visibility() {const bool is_recent=recent(),searching=!active_query().isEmpty();left->setCurrentWidget(is_recent?static_cast<QWidget *>(categories):tree);view->setVisible(is_recent||searching||location_view||zoom->value()>0);breadcrumbs->setEnabled(!is_recent);libraries->setEnabled(!is_recent);libraries->setToolTip(directory);scope->setEnabled(!is_recent);if(view->isHidden()) empty->hide();}
  void display(std::vector<Item> items) {
    hide_preview();model->replace(std::move(items));empty->setText(recent()?QStringLiteral("暂无近期使用记录"):roots.empty()?QStringLiteral("请在项目设置中添加内容库"):QStringLiteral("没有匹配的资源"));empty->setGeometry(view->viewport()->rect().adjusted(8,8,-8,-8));empty->setVisible(model->items.empty()&&view->isVisible());
    if(!pending_item.isEmpty()){for(size_t i=0;i<model->items.size();++i)if(content_path(model->items[i].path).compare(pending_item,Qt::CaseInsensitive)==0){const QPersistentModelIndex index=model->index(int(i),0);view->setCurrentIndex(index);view->doItemsLayout();view->scrollTo(index,QAbstractItemView::PositionAtCenter);for(int delay:{0,100})QTimer::singleShot(delay,owner,[this,index]{if(index.isValid()&&view->currentIndex()==index)view->scrollTo(index,QAbstractItemView::PositionAtCenter);});pending_item.clear();break;}}
  }
  void show_items() {
    if(restoring) return;index.cancel_search();const auto generation=++directory_generation;directories.clear();update_visibility();
    const auto query=active_query();
    if(recent()) {std::vector<Item> items;const auto category=categories->currentItem()->data(Qt::UserRole).toString();for(const auto &r:history.recent(category))items.push_back({r.path,r.category,false,r.used});display(std::move(items));return;}
    if(!query.isEmpty()) {
      const auto scope_path=scope->currentIndex()==0?QString():scope->currentIndex()==1?libraries->currentText():directory;
      index.search(query,scope_path,[this](ContentSearch result) {
        std::vector<Item> items;items.reserve(result.paths.size());for(auto &path:result.paths) items.push_back({std::move(path)});
        display(std::move(items));
      });return;
    }
    if(directory.isEmpty()) {display({});return;}
    if(zoom->value()==0&&!location_view) {empty->hide();return;}
    const auto folder=directory;const auto located=pending_item;directories.start([this,folder,generation,located] {
      const QDir dir(folder);const auto entries=dir.entryInfoList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot,QDir::DirsFirst|QDir::Name|QDir::IgnoreCase);std::vector<Item> items;
      for(const auto &entry:entries) {if(generation!=directory_generation) return;if(entry.isDir()||entry.absoluteFilePath().compare(located,Qt::CaseInsensitive)==0||daz::supported_content_entry(std::filesystem::path(entry.absoluteFilePath().toStdWString()))||(!entry.fileName().contains(".tip.",Qt::CaseInsensitive)&&QImageReader::supportedImageFormats().contains(entry.suffix().toLower().toLatin1())&&!QFileInfo(entry.absoluteFilePath().left(entry.absoluteFilePath().lastIndexOf('.'))).exists()&&!QFileInfo(entry.path()+"/"+entry.completeBaseName()+".duf").exists())) items.push_back({entry.absoluteFilePath(),{},entry.isDir()});}
      QMetaObject::invokeMethod(owner,[this,generation,items=std::move(items)]() mutable {if(generation!=directory_generation) return;display(std::move(items));},Qt::QueuedConnection);
    });
  }
};

ContentBrowser::ContentBrowser(QWidget *parent,const QString &settings,const QString &cache):QWidget(parent) {impl_=std::make_unique<Impl>(this,settings,cache);setMinimumWidth(270);}
ContentBrowser::~ContentBrowser() {save();}
void ContentBrowser::set_roots(const QStringList &roots) {
  auto &p=*impl_;p.resume_search();p.restoring=true;p.roots.clear();p.libraries->clear();for(const auto &root:roots) {const auto path=content_path(root);if(!p.roots.contains(path,Qt::CaseInsensitive)) p.roots.append(path);}p.libraries->addItems(p.roots);p.thumbnails.roots.clear();for(const auto &root:p.roots)p.thumbnails.roots.emplace_back(root.toStdWString());p.thumbnails.clear();p.restoring=false;
  auto directory=p.directory.isEmpty()?p.history.settings().value("content/directory").toString():p.directory;
  bool valid=false;for(const auto &root:p.roots) if(directory.compare(root,Qt::CaseInsensitive)==0||directory.startsWith(root+"/",Qt::CaseInsensitive)) valid=true;
  if(!valid||!QFileInfo(directory).isDir()) directory=p.roots.value(0);p.directory=directory;if(!directory.isEmpty()) p.navigate(directory);else {p.breadcrumbs->path({},{});p.tree->setRootIndex(p.files->setRootPath({}));p.tree->setEnabled(false);p.show_items();}p.tree->setEnabled(!p.roots.isEmpty());p.index.refresh(p.roots,true);
}
void ContentBrowser::record_use(const QString &path,const QString &category) {impl_->history.used(path,category);if(impl_->recent()) impl_->show_items();}
void ContentBrowser::show_recent() {impl_->tabs->setCurrentIndex(1);impl_->categories->setCurrentRow(0);impl_->show_items();}
bool ContentBrowser::locate(const QString &path){const QFileInfo info(path);if(!info.exists())return false;auto &p=*impl_;p.resume_search();p.location_view=true;{const QSignalBlocker block(p.tabs);p.tabs->setCurrentIndex(0);}p.query_timer.stop();p.history_timer.stop();p.pending_item=info.isDir()?QString{}:content_path(info.absoluteFilePath());p.navigate(info.isDir()?info.absoluteFilePath():info.absolutePath());if(!p.search->currentText().trimmed().isEmpty()){p.notice->setText(QStringLiteral("已定位到资源所在目录；搜索文本已保留，修改文本或按 Enter 恢复搜索。"));p.notice->show();}return true;}
void ContentBrowser::locate_asset(const ContentOrigin &origin){auto &p=*impl_;p.resume_search();p.locating=origin;setProperty("contentLocateState","resolving");setProperty("contentLocatedPath",QString{});p.notice->setText(QStringLiteral("正在内容库中查找模型入口…"));p.notice->show();p.try_locate();}
void ContentBrowser::save() {if(!impl_) return;auto &p=*impl_;auto &settings=p.history.settings();settings.setValue("content/iconSize",icon_pixels(p.zoom->value()));settings.setValue("content/splitter",p.splitter->saveState());settings.setValue("content/directory",p.directory);settings.setValue("content/scope",p.scope->currentIndex());settings.sync();}
bool ContentBrowser::eventFilter(QObject *object,QEvent *event) {
  if(!impl_) return QWidget::eventFilter(object,event);auto &p=*impl_;
  if(object==p.tree->viewport()&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::Wheel))p.pending_tree.clear();
  if(object==p.view&&(event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange))p.apply_zoom();
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
