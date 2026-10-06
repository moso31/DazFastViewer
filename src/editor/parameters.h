#pragma once
#include "runtime/morph.h"
#include <QWidget>
#include <functional>
#include <array>
#include <map>
#include <set>
class QScrollArea;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QLabel;
namespace dfv::editor {
struct ParameterControl {
  std::string id,label,group,detail;
  std::string favorite_name,favorite_id;
  bool favorite=false;
  double minimum=-10000,maximum=10000,step=.01,initial=0;
  double slider_minimum=0,slider_maximum=1;
  int morph=-1;
  bool enabled=true,visible=true;
  bool float_backed=false;
  bool enforce_limits=false;
  enum class Format {number,date,time,color};
  Format format=Format::number;
  std::vector<std::string> choices;
  std::set<int> disabled_choices;
  std::function<double()> read;
  std::function<void(double)> write;
  std::function<std::array<double,3>()> read_color;
  std::function<void(const std::array<double,3> &)> write_color;
};
class ParameterPanel final:public QWidget {
  const runtime::Target *target_=nullptr;
  const runtime::Properties *values_=nullptr;
  QTreeWidget *groups_,*tree_;
  QScrollArea *shared_scroll_=nullptr;
  void shared_height();
  void show_item(QTreeWidgetItem *item);
  QLineEdit *search_;
  QCheckBox *hidden_;
  QLabel *count_;
  int current_=-1;
  int wheel_selected_=-1;
  void wheel_selection(int row);
  ir::OptionNode *option_node_=nullptr;
  std::string option_node_id_;
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
  const runtime::FavoriteState *saved_favorites_=nullptr;
  std::string saved_favorite_node_;
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
  void shared_scroll(QScrollArea *scroll);
  std::function<void(size_t,double)> changed;
  std::function<void(bool)> interaction_changed;
  std::function<void(const std::string &,const std::string &,bool)> favorite_changed;
  void import_favorites(const runtime::Target *target,std::optional<runtime::FavoriteState> &state) const;
  void bind_favorites(const runtime::FavoriteState *state,const std::string &node={}) {saved_favorites_=state;saved_favorite_node_=node;}
  void bind(const runtime::Target *target,const runtime::Properties *values,const std::string &node={});
  void bind_options(ir::OptionNode *node,std::function<void(size_t,size_t,double)> callback,
                    std::function<void(size_t,const std::array<double,3> &)> color_callback={});
  void set_extra(std::vector<ParameterControl> controls) {extra_=std::move(controls);}
  void bind_controls(std::vector<ParameterControl> controls) {option_node_=nullptr;target_=nullptr;values_=nullptr;favorite_scope_.clear();scene_favorites_=false;controls_=std::move(controls);morph_rows_.clear();rebuild();}
  void refresh(size_t index);
  void query(const QString &text);
  void select_parameter(size_t index);
  void set_slider(int value);
  bool edit_control(const std::string &id,double value);
  void evaluated(const std::vector<float> &values);
  void resource_states();
};
}
