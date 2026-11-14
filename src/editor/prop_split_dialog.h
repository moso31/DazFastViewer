#pragma once
#include "editor/prop_split.h"
#include <QDialog>

namespace dfv::editor {
class PropSplitDialog final:public QDialog {
public:
  explicit PropSplitDialog(const ir::Mesh &,const QString &label,QWidget *parent=nullptr);
  std::vector<PropSplitPart> parts() const;
private:
  struct State;
  std::shared_ptr<State> state_;
};
}
