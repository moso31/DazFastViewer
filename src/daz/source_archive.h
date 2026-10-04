#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace dfv::daz {
// Frozen user-facing documents. Installed DSF/texture assets stay external.
// Each import owns its scope: two revisions of the same original path can coexist.
struct FrozenDocument {
  std::string digest;
  size_t size=0;
  std::vector<unsigned char> compressed;
  std::string bytes() const;
};
class SourceArchive {
  mutable std::mutex mutex_;
  std::map<std::string,std::shared_ptr<const FrozenDocument>> documents_;
public:
  std::shared_ptr<const FrozenDocument> find(const std::filesystem::path &) const;
  std::shared_ptr<const FrozenDocument> capture(const std::filesystem::path &);
  nlohmann::json json() const;
  std::string identity() const;
  static std::shared_ptr<SourceArchive> read(const nlohmann::json &);
};
class DocumentScope {
  std::shared_ptr<SourceArchive> previous_;
public:
  explicit DocumentScope(std::shared_ptr<SourceArchive>);
  ~DocumentScope();
  DocumentScope(const DocumentScope &)=delete;
  DocumentScope &operator=(const DocumentScope &)=delete;
};
std::shared_ptr<SourceArchive> current_archive();
std::shared_ptr<const FrozenDocument> frozen_document(const std::filesystem::path &,bool capture=false);
bool document_exists(const std::filesystem::path &);
std::string source_key(const std::filesystem::path &);
std::string source_digest(const std::string &);
std::string gzip_document(const std::string &);
std::string unpack_document(const std::string &);
}
