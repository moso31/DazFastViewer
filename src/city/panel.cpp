#include "editor/parameter_widgets.h"
#include "city/panel.h"
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QSignalBlocker>
namespace dfv::city {
namespace {QString q(const std::string &s){return QString::fromUtf8(s);}std::string u(const QString &s){return s.toUtf8().toStdString();}}
Panel::Panel(const std::filesystem::path &directory,QWidget *parent):QWidget(parent){
  setObjectName("CityPCGPanel");auto *layout=new QVBoxLayout(this);auto *form=new QFormLayout;layout->addLayout(form);
  cities_=new QComboBox;cities_->addItem(QStringLiteral("新城市"),QString{});form->addRow(QStringLiteral("城市"),cities_);
  directory_=new QLineEdit(QString::fromStdWString(directory.wstring()));directory_->setObjectName("CityDirectory");form->addRow(QStringLiteral("建筑 DUF 文件夹"),directory_);
  auto *browse=new QPushButton(QStringLiteral("选择文件夹并扫描"));form->addRow(browse);connect(browse,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getExistingDirectory(this,QStringLiteral("选择建筑文件夹"),directory_->text());if(!path.isEmpty()){directory_->setText(path);scan();}});
  auto *scan_button=new QPushButton(QStringLiteral("扫描建筑原型"));form->addRow(scan_button);connect(scan_button,&QPushButton::clicked,this,[this]{scan();});
  assets_=new QListWidget;assets_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);assets_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);form->addRow(QStringLiteral("参与生成的原型"),assets_);
  auto integer=[&](const QString &label,int minimum,int maximum,int value){auto *w=new QSpinBox;w->setRange(minimum,maximum);w->setValue(value);form->addRow(label,w);return w;};
  auto real=[&](const QString &label,double minimum,double maximum,double value,int decimals=1){auto *w=editor::parameter_widgets::number({});w->setRange(minimum,maximum);w->setDecimals(decimals);w->setValue(value);form->addRow(label,w);return w;};
  seed_=integer(QStringLiteral("随机种子"),0,2147483647,1337);seed_->setObjectName("CitySeed");x_=integer(QStringLiteral("东西街区数"),1,32,4);y_=integer(QStringLiteral("南北街区数"),1,32,4);lots_=integer(QStringLiteral("每边地块数"),1,6,4);
  size_=real(QStringLiteral("街区边长（米）"),40,1000,112);road_=real(QStringLiteral("道路宽度（米）"),2,100,14);density_=real(QStringLiteral("建筑填充率"),.01,1,.85,2);
  const char *axes[]={"X","Y","Z"};for(int i=0;i<3;++i)origin_[i]=real(QStringLiteral("城市原点 %1（米）").arg(axes[i]),-100000,100000,0);
  for(int i=0;i<3;++i)distances_[i]=real(QStringLiteral("LOD%1 → %2 距离（米）").arg(i+1).arg(i+2),20,1000000,i==0?300:i==1?3000:30000,0);
  generate_=new QPushButton(QStringLiteral("生成 / 更新城市"));generate_->setObjectName("CityGenerate");form->addRow(generate_);
  connect(generate_,&QPushButton::clicked,this,[this]{try{Config c;c.directory=std::filesystem::path(directory_->text().toStdWString());for(int i=0;i<assets_->count();++i)if(assets_->item(i)->checkState()==Qt::Checked)c.assets.push_back(u(assets_->item(i)->text()));if(c.assets.empty())throw std::runtime_error("请扫描并勾选至少一个建筑原型");c.seed=uint32_t(seed_->value());c.blocks_x=x_->value();c.blocks_y=y_->value();c.lots=lots_->value();c.block_size=size_->value();c.road_width=road_->value();c.density=density_->value();c.origin_x=origin_[0]->value();c.origin_y=origin_[1]->value();c.origin_z=origin_[2]->value();for(int i=0;i<3;++i)c.distances[i]=distances_[i]->value();validate(c);if(generate)generate(c,u(cities_->currentData().toString()));}catch(const std::exception &e){message(q(e.what()));}});
  cancel_=new QPushButton(QStringLiteral("取消生成"));cancel_->setEnabled(false);form->addRow(cancel_);connect(cancel_,&QPushButton::clicked,this,[this]{if(cancel)cancel();});
  lod_=new QComboBox;lod_->addItem(QStringLiteral("自动 LOD / HLOD"),-1);lod_->addItem(QStringLiteral("LOD0 = LOD1"),0);for(int i=1;i<=4;++i)lod_->addItem(QStringLiteral("强制 LOD%1").arg(i),i);form->addRow(QStringLiteral("显示档位"),lod_);
  enabled_=new QCheckBox(QStringLiteral("显示城市"));enabled_->setChecked(true);lock_=new QCheckBox(QStringLiteral("锁定当前自动 LOD"));form->addRow(enabled_);form->addRow(lock_);
  connect(lod_,&QComboBox::currentIndexChanged,this,[this]{change_view();});connect(enabled_,&QCheckBox::toggled,this,[this]{change_view();});connect(lock_,&QCheckBox::toggled,this,[this]{change_view();});
  auto *focus_button=new QPushButton(QStringLiteral("聚焦城市"));form->addRow(focus_button);connect(focus_button,&QPushButton::clicked,this,[this]{if(focus)focus(u(cities_->currentData().toString()));});
  remove_=new QPushButton(QStringLiteral("移除城市"));form->addRow(remove_);connect(remove_,&QPushButton::clicked,this,[this]{if(remove)remove(u(cities_->currentData().toString()));});
  size_->setObjectName("CityBlockSize");road_->setObjectName("CityRoadWidth");density_->setObjectName("CityDensity");for(int i=0;i<3;++i){origin_[i]->setObjectName(QString("CityOrigin%1").arg(i));distances_[i]->setObjectName(QString("CityDistance%1").arg(i));distances_[i]->setDecimals(3);}
  editor::parameter_widgets::decorate(this,"city/");
  stats_=new QLabel;stats_->setWordWrap(true);layout->addWidget(stats_);message_=new QLabel(QStringLiteral("先扫描原型，再生成。首版建议 4 × 4 街区；共享材质可在材质面板编辑。"));message_->setWordWrap(true);layout->addWidget(message_);layout->addStretch();
  connect(cities_,&QComboBox::currentIndexChanged,this,[this]{if(!updating_)read_selection();});
}
void Panel::scan(const std::vector<std::string> &selected){try{assets_->clear();for(const auto &p:discover(std::filesystem::path(directory_->text().toStdWString()))){const auto str=p.filename().u8string();const std::string name(str.begin(),str.end());auto *item=new QListWidgetItem(q(name),assets_);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);const bool recommended=name=="Building5.duf"||name=="Building Tall2.duf"||name=="Building Tall1.duf";item->setCheckState((selected.empty()?recommended:std::find(selected.begin(),selected.end(),name)!=selected.end())?Qt::Checked:Qt::Unchecked);}bool checked=false;for(int i=0;i<assets_->count();++i)checked|=assets_->item(i)->checkState()==Qt::Checked;if(!checked&&assets_->count())assets_->item(0)->setCheckState(Qt::Checked);message(QStringLiteral("找到 %1 个建筑场景；材质预设不会作为建筑导入。").arg(assets_->count()));}catch(const std::exception &e){message(q(e.what()));}}
void Panel::read_selection(){const auto id=u(cities_->currentData().toString());editor::parameter_widgets::context(this)->bind(id.empty()?"@city-new":id);auto it=std::find_if(source_.begin(),source_.end(),[&](const auto &c){return c->id==id;});updating_=true;
  if(it!=source_.end()){const auto &c=(*it)->config;directory_->setText(QString::fromStdWString(c.directory.wstring()));scan(c.assets);seed_->setValue(int(c.seed));x_->setValue(c.blocks_x);y_->setValue(c.blocks_y);lots_->setValue(c.lots);size_->setValue(c.block_size);road_->setValue(c.road_width);density_->setValue(c.density);origin_[0]->setValue(c.origin_x);origin_[1]->setValue(c.origin_y);origin_[2]->setValue(c.origin_z);for(int i=0;i<3;++i)distances_[i]->setValue(c.distances[i]);}
  const auto view=views_.contains(id)?views_.at(id):View{};lod_->setCurrentIndex(lod_->findData(view.forced));lock_->setChecked(view.locked);enabled_->setChecked(view.enabled);updating_=false;
}
void Panel::bind(const Cities &cities,const Views &views){const bool changed=source_!=cities;source_=cities;views_=views;if(changed){const auto selected=cities_->currentData();updating_=true;cities_->clear();cities_->addItem(QStringLiteral("新城市"),QString{});for(const auto &c:cities)cities_->addItem(QStringLiteral("城市 %1 · %2 栋").arg(c->config.seed).arg(qulonglong(c->buildings.size())),q(c->id));int index=cities_->findData(selected);if(index<=0&&!cities.empty())index=cities_->count()-1;cities_->setCurrentIndex(std::max(0,index));updating_=false;read_selection();}else{const auto id=u(cities_->currentData().toString());const auto v=views.contains(id)?views.at(id):View{};updating_=true;lod_->setCurrentIndex(lod_->findData(v.forced));lock_->setChecked(v.locked);enabled_->setChecked(v.enabled);updating_=false;}}
void Panel::change_view(){if(updating_||busy_)return;const auto id=u(cities_->currentData().toString());if(!id.empty()&&view_changed)view_changed(id,{lod_->currentData().toInt(),enabled_->isChecked(),lock_->isChecked()});}
void Panel::busy(bool value){busy_=value;generate_->setEnabled(!value);remove_->setEnabled(!value);cancel_->setEnabled(value);cities_->setEnabled(!value);}
void Panel::message(const QString &value){message_->setText(value);}
void Panel::status(const Stats &s){stats_->setText(QStringLiteral("全场城市：%1 栋，%2 个可见实例\n三角形：实例展开 %3 / 共享网格 %4\n街区 LOD1 / 2 / 3 / 4：%5 / %6 / %7 / %8").arg(qulonglong(s.buildings)).arg(qulonglong(s.visible_instances)).arg(qulonglong(s.triangles)).arg(qulonglong(s.unique_triangles)).arg(qulonglong(s.cells[0])).arg(qulonglong(s.cells[1])).arg(qulonglong(s.cells[2])).arg(qulonglong(s.cells[3])));}
}
