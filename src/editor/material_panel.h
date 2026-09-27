#pragma once
#include "editor/document.h"
#include <QWidget>
#include <memory>
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
  QTreeWidget *tree_;
  QLineEdit *search_;
  QCheckBox *modified_;
  QComboBox *scope_;
  QScrollArea *properties_;
  QLabel *heading_,*status_;
  using Surface=MaterialSurface;
  std::vector<Surface> surfaces() const;
  nlohmann::json value(Surface surface,const MaterialParameter &parameter) const;
  void rebuild_tree();
  void rebuild_properties();
  void filter();
  void commit(const MaterialParameter &parameter,const nlohmann::json &value,int component=-1);
  void reset(const std::string &parameter);
  void schedule_properties();
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
  std::function<void(const QString &)> locate_file;
  std::vector<MaterialSurface> selected_surfaces() const{return surfaces();}
  std::vector<std::pair<std::string,std::string>> selection_ids() const;
  void restore_selection(const std::vector<std::pair<std::string,std::string>> &selection);
  void bind(std::shared_ptr<const Document> document,Snapshot *snapshot,int target);
};
}
