#pragma once
#include "editor/document.h"
#include <QWidget>
#include <memory>
#include <set>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QScrollArea;
class QLabel;
namespace dfv::editor {
class MaterialPanel final:public QWidget {
  std::shared_ptr<const Document> document_;
  Snapshot *snapshot_=nullptr;
  int target_=-1;
  std::string group_;
  QTreeWidget *tree_;
  QLineEdit *search_;
  QCheckBox *modified_;
  QComboBox *scope_;
  QScrollArea *properties_;
  QLabel *heading_,*status_;
  using Surface=MaterialSurface;
  mutable std::optional<std::vector<Surface>> surface_cache_;
  std::vector<Surface> surfaces() const;
  std::vector<Surface> item_surfaces(QTreeWidgetItem *item) const;
  void add_selection_sets(QTreeWidgetItem *object,size_t instance);
  void choose_selection_set(QTreeWidgetItem *item,bool additive);
  void populate_uv_combo(QComboBox *combo);
  std::string uv_catalog_key(size_t instance) const;
  std::map<std::string,daz::MaterialUVCatalog> uv_catalogs_;
  std::set<std::string> uv_pending_;
  nlohmann::json value(Surface surface,const MaterialParameter &parameter) const;
  void rebuild_tree();
  void rebuild_properties();
  void filter();
  void commit(const MaterialParameter &parameter,const nlohmann::json &value,int component=-1);
  void reset(const std::string &parameter);
  void schedule_properties();
  nlohmann::json clipboard_;
  int scroll_position_=0;
  uint64_t properties_generation_=0;
  bool restoring_scroll_=false;
  bool refresh_pending_=false;
  std::map<std::string,bool> collapsed_;
  QString wheel_selected_,hovered_,hover_suppressed_;
  bool eventFilter(QObject *object,QEvent *event) override;
  void clear_hover();
public:
  explicit MaterialPanel(QWidget *parent=nullptr);
  std::function<void()> changed;
  std::function<void(const QString &,const std::function<void()> &)> edit_requested;
  std::function<void(bool)> interaction_changed;
  std::function<void(const std::vector<MaterialSurface> &)> hovered;
  std::function<void(const std::filesystem::path &,const std::vector<MaterialSurface> &)> preset_requested;
  std::function<void(const nlohmann::json &,const std::vector<MaterialSurface> &)> paste_requested;
  std::function<void(const daz::MaterialUVSet &,const std::vector<MaterialSurface> &)> uv_requested;
  std::function<void(const std::vector<MaterialSurface> &)> uv_reset_requested;
  std::function<void(const QString &)> locate_file;
  std::vector<MaterialSurface> selected_surfaces() const{return surfaces();}
  std::vector<std::pair<std::string,std::string>> selection_ids() const;
  void restore_selection(const std::vector<std::pair<std::string,std::string>> &selection);
  void bind(std::shared_ptr<const Document> document,Snapshot *snapshot,int target,const std::string &group={});
};
}
