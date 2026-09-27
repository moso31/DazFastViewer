#pragma once
#include "editor/document.h"
#include <QObject>
#include <QUndoStack>
#include <QString>
#include <exception>
#include <functional>

namespace dfv::editor {
struct PendingParameter {
  float value=0;
  bool unlimited=false;
  bool operator==(const PendingParameter &) const = default;
};
using PendingParameters=std::map<std::pair<std::string,std::string>,PendingParameter>;
// UI 下标只在当前文档中有效；历史上下文始终保存资产身份。
struct EditSelection {
  std::string object,joint,light;
  int special=-1;
  bool operator==(const EditSelection &) const = default;
};
struct EditContext {
  std::vector<EditSelection> selection;
  EditSelection active;
  std::vector<std::pair<std::string,std::string>> surfaces;
};
struct EditState {
  uint64_t scene_id=1;
  std::shared_ptr<const Document> document;
  Snapshot snapshot;
  PendingParameters pending;
  EditContext context;
  std::filesystem::path save_file;
  bool manual=false;
};
bool same_edit(const EditState &a,const EditState &b);

// 新功能使用 transaction() 或 execute()；成功时发布命令，异常时恢复原状态。
// 拖动期间只预览，finish_gesture() 才入栈，取消与无效编辑不破坏 redo 分支。
class EditHistory final:public QObject {
  class Command;
  QUndoStack stack_;
  std::function<EditState()> capture_;
  std::function<void(const EditState &)> restore_;
  std::optional<EditState> gesture_before_,gesture_after_;
  QString gesture_name_;
  quintptr gesture_key_=0;
  int depth_=0;
  bool replaying_=false,enabled_=true,rebuilding_=false;
  void record(QString name,EditState before,EditState after);
  void restore(const EditState &state);
public:
  class Transaction {
    EditHistory *owner_=nullptr;
    QString name_;
    std::optional<EditState> before_;
    int exceptions_=0;
  public:
    Transaction(EditHistory *owner,QString name);
    Transaction(const Transaction &)=delete;
    Transaction &operator=(const Transaction &)=delete;
    ~Transaction();
  };
  explicit EditHistory(std::function<EditState()> capture,std::function<void(const EditState &)> restore,int limit=50,QObject *parent=nullptr);
  Transaction transaction(QString name) {return Transaction(this,std::move(name));}
  void execute(QString name,const std::function<void()> &change) {auto edit=transaction(std::move(name));change();}
  QUndoStack &stack() {return stack_;}
  const QUndoStack &stack() const {return stack_;}
  void begin_gesture(quintptr key);
  void finish_gesture();
  bool cancel_gesture();
  bool gesturing() const {return gesture_key_!=0;}
  void undo();
  void redo();
  void set_limit(int limit);
  void clear();
  void set_enabled(bool value) {finish_gesture();enabled_=value;}
  void mark_saved() {finish_gesture();stack_.setClean();}
  std::function<void()> changed;
  std::function<void(const QString &)> failed;
};
}
