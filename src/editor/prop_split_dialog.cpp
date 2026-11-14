#include "editor/prop_split_dialog.h"
#include "editor/parameter_widgets.h"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <numeric>

namespace dfv::editor {
namespace {
QColor part_color(int part){return part<0?QColor(145,145,145):QColor::fromHsv((part*137+205)%360,155,220);}
class SplitPreview final:public QWidget {
  const ir::Mesh &mesh_;
  const PropSplitSource &source_;
  const std::vector<int> &owners_;
  QPoint drag_;float yaw_=.4f,pitch_=.5f;
public:
  SplitPreview(const ir::Mesh &mesh,const PropSplitSource &source,const std::vector<int> &owners):mesh_(mesh),source_(source),owners_(owners){setObjectName("PropSplitPreview");setMinimumHeight(260);setMaximumHeight(260);setToolTip(QStringLiteral("拖动旋转预览；颜色对应拆分部分，灰色表示未分配"));}
protected:
  void mousePressEvent(QMouseEvent *e)override{drag_=e->position().toPoint();}
  void mouseMoveEvent(QMouseEvent *e)override{if(e->buttons()&Qt::LeftButton){const auto delta=e->position().toPoint()-drag_;drag_=e->position().toPoint();yaw_+=delta.x()*.01f;pitch_+=delta.y()*.01f;update();}}
  void paintEvent(QPaintEvent *)override{
    QPainter painter(this);painter.fillRect(rect(),palette().color(QPalette::Base));if(mesh_.positions.empty())return;
    ir::Bounds bounds;for(auto p:mesh_.positions)bounds.add(p);const ir::Vec3 center{(bounds.minimum.x+bounds.maximum.x)*.5f,(bounds.minimum.y+bounds.maximum.y)*.5f,(bounds.minimum.z+bounds.maximum.z)*.5f};
    const auto extent=std::max({bounds.maximum.x-bounds.minimum.x,bounds.maximum.y-bounds.minimum.y,bounds.maximum.z-bounds.minimum.z,1e-5f});const float scale=std::min(width(),height())*.65f/extent;
    std::vector<QPointF> points;std::vector<float> depths;std::vector<ir::Vec3> view;points.reserve(mesh_.positions.size());depths.reserve(mesh_.positions.size());view.reserve(mesh_.positions.size());
    for(auto p:mesh_.positions){p.x-=center.x;p.y-=center.y;p.z-=center.z;const float x=std::cos(yaw_)*p.x-std::sin(yaw_)*p.y,y=std::sin(yaw_)*p.x+std::cos(yaw_)*p.y,z=std::cos(pitch_)*p.z-std::sin(pitch_)*y,depth=std::cos(pitch_)*y+std::sin(pitch_)*p.z;points.push_back({width()*.5+x*scale,height()*.5-z*scale});depths.push_back(depth);view.push_back({x,z,depth});}
    std::vector<size_t> faces(mesh_.triangles.size());std::iota(faces.begin(),faces.end(),size_t{});
    auto depth=[&](size_t f){const auto v=mesh_.triangles[f].vertices;return depths[v[0]]+depths[v[1]]+depths[v[2]];};std::sort(faces.begin(),faces.end(),[&](auto a,auto b){return depth(a)>depth(b);});
    painter.setPen(Qt::NoPen);for(auto f:faces){const auto v=mesh_.triangles[f].vertices;const QPolygonF polygon({points[v[0]],points[v[1]],points[v[2]]});auto color=part_color(owners_[source_.faces[f]]);const auto p=view[v[0]],q=view[v[1]],r=view[v[2]];const ir::Vec3 a{q.x-p.x,q.y-p.y,q.z-p.z},b{r.x-p.x,r.y-p.y,r.z-p.z},n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};const float length=std::hypot(n.x,n.y,n.z);const float light=length>0?std::abs((-.3f*n.x+.4f*n.y-.866f*n.z)/length):0;color=color.darker(int(100/(.35f+.65f*light)));painter.setBrush(color);painter.drawPolygon(polygon);}
    for(size_t c=0;c<mesh_.curves.size();++c){painter.setPen(part_color(owners_[source_.curves[c]]));const auto &v=mesh_.curves[c].vertices;for(size_t k=1;k<v.size();++k)painter.drawLine(points[v[k-1]],points[v[k]]);}
    painter.setPen(palette().color(QPalette::Text));painter.drawText(10,22,QStringLiteral("拖动旋转 · 灰色为未分配"));
  }
};
}
struct PropSplitDialog::State {
  PropSplitSource source;
  std::vector<int> owners;
  std::vector<std::string> names;
  QTreeWidget *tree=nullptr;QComboBox *current=nullptr;QLineEdit *name=nullptr,*search=nullptr;QSpinBox *count=nullptr;QLabel *summary=nullptr;QPushButton *ok=nullptr;SplitPreview *preview=nullptr;
  bool updating=false;
  std::vector<PropSplitPart> parts() const {std::vector<PropSplitPart> out;for(const auto &n:names)out.push_back({QString::fromStdString(n).trimmed().toStdString(),{}});for(size_t r=0;r<owners.size();++r)if(owners[r]>=0)out.at(owners[r]).regions.push_back(int(r));return out;}
  bool within(int r,int ancestor) const {for(int n=r;n>=0;n=source.regions.at(n).parent)if(n==ancestor)return true;return false;}
  void resize_tree(){int rows=0;for(QTreeWidgetItemIterator it(tree);*it;++it)if(!(*it)->isHidden()){bool visible=true;for(auto *p=(*it)->parent();p;p=p->parent())visible&=p->isExpanded();rows+=visible;}const int height=tree->header()->height()+std::max(1,rows)*std::max(24,tree->sizeHintForRow(0))+4;tree->setFixedHeight(height);}
  void refresh(){
    updating=true;const int selected=current->currentIndex();auto list=parts();const auto counts=prop_split_counts(source,list);
    QSignalBlocker block(tree);std::vector<size_t> direct(source.regions.size());for(const auto *values:{&source.faces,&source.curves})for(int r:*values)++direct[r];
    for(QTreeWidgetItemIterator it(tree);*it;++it){auto *item=*it;const int r=item->data(0,Qt::UserRole).toInt();size_t total=0,checked=0;std::set<int> assigned;
      for(size_t n=0;n<owners.size();++n)if(direct[n]&&within(int(n),r)){total+=direct[n];if(owners[n]==selected)checked+=direct[n];assigned.insert(owners[n]);}
      item->setCheckState(0,checked==0?Qt::Unchecked:checked==total?Qt::Checked:Qt::PartiallyChecked);item->setText(1,QString::number(total));
      QStringList labels;for(int p:assigned)labels<<(p<0?QStringLiteral("未分配"):QString::fromStdString(names.at(p)));item->setText(2,labels.join(QStringLiteral("、")));item->setToolTip(2,item->text(2));if(assigned.size()==1)item->setForeground(2,part_color(*assigned.begin()));else item->setForeground(2,QBrush{});
    }
    QStringList descriptions;bool valid=counts.back()==0;std::set<std::string> unique;
    for(size_t p=0;p<names.size();++p){descriptions<<QStringLiteral("%1：%2").arg(QString::fromStdString(names[p])).arg(counts[p]);valid&=counts[p]>0&&!list[p].label.empty()&&unique.insert(list[p].label).second;current->setItemText(int(p),QStringLiteral("%1 · %2").arg(p+1).arg(QString::fromStdString(names[p])));}
    summary->setText(descriptions.join(QStringLiteral("　"))+QStringLiteral("\n未分配：%1 个面／曲线。每部分须有内容、名称不重复。\n拆分后保留隐藏原件，生成可独立调整的 Prop；可撤销。").arg(counts.back()));ok->setEnabled(valid);
    const auto text=selected>=0?QString::fromStdString(names.at(selected)):QString{};if(name->text()!=text)name->setText(text);preview->update();updating=false;
  }
  void set_count(int n){if(updating)return;const auto old=names.size();names.resize(size_t(n));for(size_t p=old;p<names.size();++p)names[p]="部分 "+std::to_string(p+1);for(auto &p:owners)if(p>=n)p=-1;
    QSignalBlocker block(current);const auto selected=std::clamp(current->currentIndex(),0,n-1);current->clear();for(int p=0;p<n;++p)current->addItem(QString::fromStdString(names[p]));current->setCurrentIndex(selected);refresh();}
};
PropSplitDialog::PropSplitDialog(const ir::Mesh &mesh,const QString &label,QWidget *parent):QDialog(parent),state_(std::make_shared<State>()){
  setObjectName("PropSplitDialog");setWindowTitle(QStringLiteral("拆分 Prop · ")+label);resize(820,780);auto &s=*state_;s.source=prop_split_source(mesh);s.owners.resize(s.source.regions.size(),-1);
  auto *layout=new QVBoxLayout(this);auto *scroll=new QScrollArea;scroll->setObjectName("PropSplitScroll");scroll->setWidgetResizable(true);layout->addWidget(scroll,1);auto *body=new QWidget;auto *content=new QVBoxLayout(body);scroll->setWidget(body);
  auto *help=new QLabel(s.source.bones?QStringLiteral("为每个部分选择一个或多个骨骼范围。勾选父节点包含全部后代；可将脚、脚趾、小腿等范围归到同一部分。左右按来源骨骼判断，不受当前姿势影响。"):QStringLiteral("此 Prop 没有保存骨骼区域，以下显示独立网格块。可结合颜色预览，将多个网格块归到同一部分。"));help->setWordWrap(true);content->addWidget(help);
  s.preview=new SplitPreview(mesh,s.source,s.owners);content->addWidget(s.preview);
  auto *form=new QFormLayout;content->addLayout(form);s.count=new QSpinBox;s.count->setObjectName("PropSplitCount");s.count->setRange(2,std::max(2,int(s.source.regions.size())));s.count->setValue(2);s.count->setKeyboardTracking(false);
  auto *countLabel=new QLabel(QStringLiteral("拆分数量"));auto *countRow=new QWidget;auto *countLayout=new QHBoxLayout(countRow);countLayout->setContentsMargins(0,0,0,0);countLayout->addWidget(s.count);countRow->setProperty("numericRow","prop-split/count");countRow->setProperty("numericLabel",QVariant::fromValue<QObject *>(countLabel));form->addRow(countLabel,countRow);
  s.current=new QComboBox;s.current->setObjectName("PropSplitCurrent");auto *currentLabel=new QLabel(QStringLiteral("当前部分"));auto *currentRow=new QWidget;auto *currentLayout=new QHBoxLayout(currentRow);currentLayout->setContentsMargins(0,0,0,0);currentLayout->addWidget(s.current);currentRow->setProperty("numericRow","prop-split/current");currentRow->setProperty("numericLabel",QVariant::fromValue<QObject *>(currentLabel));form->addRow(currentLabel,currentRow);
  for(auto pair:{std::pair{countLabel,countRow},std::pair{currentLabel,currentRow}}){pair.first->setProperty("numericRow",pair.second->property("numericRow"));pair.first->setProperty("numericLabel",QVariant::fromValue<QObject *>(pair.first));}
  s.name=new QLineEdit;s.name->setObjectName("PropSplitName");form->addRow(QStringLiteral("部分名称"),s.name);
  auto *buttons=new QHBoxLayout;content->addLayout(buttons);auto *sides=new QPushButton(QStringLiteral("按左右半区"));sides->setObjectName("PropSplitSides");buttons->addWidget(sides);auto *remaining=new QPushButton(QStringLiteral("其余归入当前部分"));remaining->setObjectName("PropSplitRemaining");buttons->addWidget(remaining);auto *clear=new QPushButton(QStringLiteral("清空当前部分"));buttons->addWidget(clear);
  sides->setEnabled(std::any_of(s.source.regions.begin(),s.source.regions.end(),[](const auto &r){return r.side!=0;}));
  s.search=new QLineEdit;s.search->setObjectName("PropSplitSearch");s.search->setPlaceholderText(QStringLiteral("筛选骨骼／区域"));s.search->setClearButtonEnabled(true);content->addWidget(s.search);
  s.tree=new QTreeWidget;s.tree->setObjectName("PropSplitTree");s.tree->setHeaderLabels({QStringLiteral("加入当前部分 · 骨骼／区域"),QStringLiteral("面／曲线数"),QStringLiteral("所属部分")});s.tree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);s.tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);s.tree->setUniformRowHeights(true);s.tree->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);s.tree->header()->setSectionResizeMode(0,QHeaderView::Stretch);s.tree->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);s.tree->header()->setSectionResizeMode(2,QHeaderView::Stretch);content->addWidget(s.tree);
  s.tree->setIndentation(16);s.tree->header()->setStretchLastSection(false);s.tree->header()->setSectionResizeMode(2,QHeaderView::Fixed);s.tree->setColumnWidth(2,160);
  std::set<int> used;for(const auto *values:{&s.source.faces,&s.source.curves})for(int r:*values)for(int p=r;p>=0;p=s.source.regions.at(p).parent)used.insert(p);
  std::map<int,QTreeWidgetItem *> items;for(int r:used){const auto &region=s.source.regions[r];auto *item=new QTreeWidgetItem;item->setText(0,QString::fromStdString(region.label));item->setToolTip(0,QString::fromStdString(region.id));item->setData(0,Qt::UserRole,r);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,Qt::Unchecked);items[r]=item;}
  for(const auto &[r,item]:items){const int parentRegion=s.source.regions[r].parent;if(items.contains(parentRegion))items[parentRegion]->addChild(item);else s.tree->addTopLevelItem(item);}s.tree->expandAll();s.resize_tree();
  s.summary=new QLabel;s.summary->setWordWrap(true);content->addWidget(s.summary);content->addStretch();auto *actions=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(actions);s.ok=actions->button(QDialogButtonBox::Ok);s.ok->setText(QStringLiteral("拆分"));s.ok->setObjectName("PropSplitApply");
  actions->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
  connect(actions,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
  connect(s.count,&QSpinBox::valueChanged,this,[this](int n){state_->set_count(n);});connect(s.current,&QComboBox::currentIndexChanged,this,[this]{if(!state_->updating)state_->refresh();});
  connect(s.name,&QLineEdit::textEdited,this,[this](const QString &value){auto &s=*state_;if(!s.updating&&s.current->currentIndex()>=0){s.names[s.current->currentIndex()]=value.toStdString();s.refresh();}});
  connect(sides,&QPushButton::clicked,this,[this]{auto &s=*state_;s.count->setValue(2);s.set_count(2);const auto parts=prop_split_sides(s.source);std::fill(s.owners.begin(),s.owners.end(),-1);for(size_t p=0;p<parts.size();++p){s.names[p]=parts[p].label;for(int r:parts[p].regions)s.owners[r]=int(p);}s.refresh();});
  connect(remaining,&QPushButton::clicked,this,[this]{auto &s=*state_;for(auto &p:s.owners)if(p<0)p=s.current->currentIndex();s.refresh();});connect(clear,&QPushButton::clicked,this,[this]{auto &s=*state_;for(auto &p:s.owners)if(p==s.current->currentIndex())p=-1;s.refresh();});
  connect(s.tree,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem *item,int column){auto &s=*state_;if(s.updating||column!=0)return;const int root=item->data(0,Qt::UserRole).toInt(),part=s.current->currentIndex();const bool checked=item->checkState(0)!=Qt::Unchecked;for(size_t r=0;r<s.owners.size();++r)if(s.within(int(r),root)){if(checked)s.owners[r]=part;else if(s.owners[r]==part)s.owners[r]=-1;}s.refresh();});
  connect(s.tree,&QTreeWidget::itemExpanded,this,[this]{state_->resize_tree();});connect(s.tree,&QTreeWidget::itemCollapsed,this,[this]{state_->resize_tree();});
  connect(s.search,&QLineEdit::textChanged,this,[this](const QString &query){auto &s=*state_;std::function<bool(QTreeWidgetItem *)> filter=[&](QTreeWidgetItem *item){bool matches=item->text(0).contains(query,Qt::CaseInsensitive)||item->toolTip(0).contains(query,Qt::CaseInsensitive);for(int i=0;i<item->childCount();++i)matches=filter(item->child(i))||matches;item->setHidden(!matches);return matches;};for(int i=0;i<s.tree->topLevelItemCount();++i)filter(s.tree->topLevelItem(i));if(!query.isEmpty())s.tree->expandAll();s.resize_tree();});
  auto *context=parameter_widgets::context(body);context->wheel_selection();context->bind("prop-split");s.set_count(2);if(sides->isEnabled())sides->click();
}
std::vector<PropSplitPart> PropSplitDialog::parts() const{return state_->parts();}
}
