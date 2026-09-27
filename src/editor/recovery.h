#pragma once
#include "editor/edit_history.h"
#include <QStringList>
#include <QLockFile>
#include <condition_variable>
#include <thread>

namespace dfv::editor {
nlohmann::json recovery_json(const EditState &state,const std::vector<std::filesystem::path> &roots);
EditState restore_recovery(const nlohmann::json &data,uint64_t generation,const std::function<void(const std::string &)> &progress={});

// 每个进程持有独立锁与原子恢复文件；不覆盖其他仍在运行的编辑器。
class RecoverySession {
  struct Request {EditState state;std::vector<std::filesystem::path> roots;uint64_t serial=0;bool clean=false;};
  QString file_;
  std::unique_ptr<QLockFile> lock_;
  std::mutex mutex_;
  std::condition_variable ready_,written_;
  std::optional<Request> pending_;
  uint64_t serial_=0,completed_=0;
  QString error_;
  bool stopping_=false;
  std::thread worker_;
  void run();
public:
  explicit RecoverySession(const QString &directory);
  ~RecoverySession();
  RecoverySession(const RecoverySession &)=delete;
  static QStringList candidates(const QString &directory);
  static nlohmann::json read(const QString &file);
  static void dismiss(const QString &file);
  void checkpoint(EditState state,std::vector<std::filesystem::path> roots,bool clean=false);
  bool flush();
  QString error();
  const QString &file() const{return file_;}
};
}
