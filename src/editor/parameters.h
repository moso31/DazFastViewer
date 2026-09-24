#pragma once
#include "runtime/morph.h"
#include <QWidget>
#include <functional>
#include <map>
#include <set>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QLabel;
namespace dfv::editor {
struct ParameterControl {
  std::string id,label,group,detail;
  std::string favorite_name;
  bool favorite=false;
  double minimum=-10000,maximum=10000,step=.01,initial=0;
  double slider_minimum=0,slider_maximum=1;
  int morph=-1;
  bool enabled=true,visible=true;
  bool float_backed=false;
  bool enforce_limits=false;
  enum class Format {number,date,time};
  Format format=Format::number;
  std::vector<std::string> choices;
  std::set<int> disabled_choices;
  std::function<double()> read;
  std::function<void(double)> write;
};
class ParameterPanel final:public QWidget {
  const runtime::Target *target_=nullptr;
  const runtime::Properties *values_=nullptr;
  QTreeWidget *groups_,*tree_;
  QLineEdit *search_;
  QCheckBox *hidden_;
  QLabel *count_;
  int current_=-1;
  int wheel_selected_=-1;
  std::string node_;
  std::vector<float> effective_;
  std::vector<ParameterControl> controls_,extra_;
  std::vector<int> morph_rows_;
  std::vector<QTreeWidgetItem *> items_;
  std::map<int,QWidget *> mounted_;
  std::set<std::string> favorites_;
  std::map<std::string,bool> favorite_overrides_;
  std::string favorite_scope_;
  bool scene_favorites_=false;
  bool is_favorite(const ParameterControl &control) const;
  void toggle_favorite(size_t index);
  void rebuild();
  void filter();
  void mount();
  void update_rows();
protected:
  bool eventFilter(QObject *object,QEvent *event) override;
public:
  explicit ParameterPanel(QWidget *parent=nullptr);
  std::function<void(size_t,double)> changed;
  std::function<void(bool)> interaction_changed;
  void bind(const runtime::Target *target,const runtime::Properties *values,const std::string &node={});
  void bind_options(ir::OptionNode *node,std::function<void(size_t,size_t,double)> callback);
  void set_extra(std::vector<ParameterControl> controls) {extra_=std::move(controls);}
  void bind_controls(std::vector<ParameterControl> controls) {target_=nullptr;values_=nullptr;favorite_scope_.clear();scene_favorites_=false;controls_=std::move(controls);morph_rows_.clear();rebuild();}
  void refresh(size_t index);
  void query(const QString &text);
  void select_parameter(size_t index);
  void set_slider(int value);
  bool edit_control(const std::string &id,double value);
  void evaluated(const std::vector<float> &values);
  void resource_states();
};
}
