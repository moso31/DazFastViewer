#include "editor/edit_history.h"
#include <algorithm>
#include <stdexcept>

namespace dfv::editor {
bool same_edit(const EditState &a,const EditState &b) {
  const auto &x=a.snapshot,&y=b.snapshot;
  return a.document==b.document&&a.pending==b.pending&&a.manual==b.manual&&
    x.values==y.values&&x.poses==y.poses&&x.lights==y.lights&&x.options==y.options&&
    x.material_overrides==y.material_overrides&&x.instance_ground==y.instance_ground&&
    x.subdivision_levels==y.subdivision_levels&&x.control_favorites==y.control_favorites&&x.pose_pins==y.pose_pins;
}
class EditHistory::Command final:public QUndoCommand {
  EditHistory &owner_;
  EditState before_,after_;
  std::vector<std::string> targets_,skins_;
  bool patch_=false,first_=true;
  void apply(const EditState &saved) {
    if(owner_.rebuilding_)return;
    auto current=owner_.capture_();
    // 离开场景时记住它最近一次另存为的路径，跨场景重做也能恢复该路径。
    if(current.scene_id==before_.scene_id)before_.save_file=current.save_file;
    if(current.scene_id==after_.scene_id)after_.save_file=current.save_file;
    auto state=saved;
    if(patch_) {
      // 同一结构版本内只保存改变的对象和骨架；不重复保存网格或其他角色参数。
      for(size_t i=0;i<targets_.size();++i) {
        const auto &catalog=saved.document->catalog.targets;
        auto t=std::find_if(catalog.begin(),catalog.end(),[&](const auto &v){return v.id==targets_[i];});
        if(t==catalog.end())throw std::runtime_error("撤销对象身份失效");
        current.snapshot.values.at(size_t(t-catalog.begin()))=saved.snapshot.values.at(i);
      }
      for(size_t i=0;i<skins_.size();++i) {
        const auto &catalog=saved.document->skeletons.skins;
        auto s=std::find_if(catalog.begin(),catalog.end(),[&](const auto &v){return v.id==skins_[i];});
        if(s==catalog.end())throw std::runtime_error("撤销骨架身份失效");
        current.snapshot.poses.at(size_t(s-catalog.begin()))=saved.snapshot.poses.at(i);
      }
      state.snapshot.values=std::move(current.snapshot.values);state.snapshot.poses=std::move(current.snapshot.poses);
    }
    owner_.restore(state);
  }
public:
  Command(EditHistory &owner,QString name,EditState before,EditState after):QUndoCommand(std::move(name)),owner_(owner),before_(std::move(before)),after_(std::move(after)) {
    patch_=before_.document&&before_.document==after_.document;
    if(!patch_)return;
    auto a=std::move(before_.snapshot.values),b=std::move(after_.snapshot.values);
    before_.snapshot.values.clear();after_.snapshot.values.clear();
    for(size_t i=0;i<a.size();++i)if(a[i]!=b.at(i)){targets_.push_back(before_.document->catalog.targets.at(i).id);before_.snapshot.values.push_back(std::move(a[i]));after_.snapshot.values.push_back(std::move(b[i]));}
    auto p=std::move(before_.snapshot.poses),q=std::move(after_.snapshot.poses);
    before_.snapshot.poses.clear();after_.snapshot.poses.clear();
    for(size_t i=0;i<p.size();++i)if(p[i]!=q.at(i)){skins_.push_back(before_.document->skeletons.skins.at(i).id);before_.snapshot.poses.push_back(std::move(p[i]));after_.snapshot.poses.push_back(std::move(q[i]));}
  }
  Command(const Command &other,EditHistory &owner):QUndoCommand(other.text()),owner_(owner),before_(other.before_),after_(other.after_),targets_(other.targets_),skins_(other.skins_),patch_(other.patch_) {}
  void redo() override {if(first_){first_=false;return;}apply(after_);}
  void undo() override {apply(before_);}
};
EditHistory::EditHistory(std::function<EditState()> capture,std::function<void(const EditState &)> restore,int limit,QObject *parent)
  :QObject(parent),capture_(std::move(capture)),restore_(std::move(restore)) {stack_.setUndoLimit(std::clamp(limit,1,500));}
EditHistory::Transaction::Transaction(EditHistory *owner,QString name):owner_(owner),name_(std::move(name)),exceptions_(std::uncaught_exceptions()) {
  if(!owner_||owner_->replaying_||!owner_->enabled_){owner_=nullptr;return;}
  if(owner_->depth_==0)before_=owner_->capture_();
  ++owner_->depth_;
}
EditHistory::Transaction::~Transaction() {
  if(!owner_)return;--owner_->depth_;if(!before_)return;
  try {
    if(std::uncaught_exceptions()>exceptions_) {owner_->restore(*before_);return;}
    owner_->record(name_,std::move(*before_),owner_->capture_());
  }catch(const std::exception &e){if(owner_->failed)owner_->failed(QString::fromUtf8(e.what()));}
}
void EditHistory::restore(const EditState &state) {
  replaying_=true;
  try{restore_(state);}catch(...){replaying_=false;throw;}
  replaying_=false;
}
void EditHistory::record(QString name,EditState before,EditState after) {
  if(same_edit(before,after))return;
  if(gesture_key_) {
    if(!gesture_before_)gesture_before_=std::move(before);
    gesture_after_=std::move(after);gesture_name_=std::move(name);return;
  }
  stack_.push(new Command(*this,std::move(name),std::move(before),std::move(after)));
  if(changed)changed();
}
void EditHistory::begin_gesture(quintptr key) {if(!key||key==gesture_key_)return;finish_gesture();gesture_key_=key;}
void EditHistory::finish_gesture() {
  gesture_key_=0;
  auto before=std::move(gesture_before_),after=std::move(gesture_after_);gesture_before_.reset();gesture_after_.reset();
  if(before&&after)record(std::move(gesture_name_),std::move(*before),std::move(*after));
}
bool EditHistory::cancel_gesture() {
  gesture_key_=0;auto before=std::move(gesture_before_);gesture_before_.reset();gesture_after_.reset();
  if(!before)return false;restore(*before);if(changed)changed();return true;
}
void EditHistory::undo() {if(cancel_gesture())return;stack_.undo();if(changed)changed();}
void EditHistory::redo() {if(cancel_gesture())return;stack_.redo();if(changed)changed();}
void EditHistory::clear(){finish_gesture();stack_.clear();if(changed)changed();}
void EditHistory::set_limit(int limit) {
  limit=std::clamp(limit,1,500);finish_gesture();if(limit==stack_.undoLimit())return;
  // Qt 只允许空栈修改上限。复制仍在范围内的命令，并在不重放场景的情况下重建游标。
  const int index=stack_.index(),count=stack_.count(),clean=stack_.cleanIndex();
  const int first=std::max(0,index-limit),last=std::min(count,first+limit);
  std::vector<std::unique_ptr<Command>> commands;
  for(int i=first;i<last;++i)commands.push_back(std::make_unique<Command>(*static_cast<const Command *>(stack_.command(i)),*this));
  stack_.clear();stack_.setUndoLimit(limit);
  // 构建时禁止调用 UI 恢复；各命令第一次 redo 也不会再次应用。
  rebuilding_=true;
  for(auto &command:commands)stack_.push(command.release());
  if(clean>=first&&clean<=last){stack_.setIndex(clean-first);stack_.setClean();}else stack_.resetClean();
  stack_.setIndex(index-first);rebuilding_=false;
  if(changed)changed();
}
}
