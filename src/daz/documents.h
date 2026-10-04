#pragma once
#include <filesystem>
#include <memory>
#include <exception>
#include <span>
#include <vector>
#include <nlohmann/json.hpp>
#include "daz/source_archive.h"
namespace dfv::daz {
enum class DocumentView {metadata,skeleton,payload};
// 返回不可变文档；元数据视图不构造几何、UV 和 Morph 差值的数值 DOM。
std::shared_ptr<const nlohmann::json> document_view(const std::filesystem::path &file,DocumentView view=DocumentView::metadata);
// 已解析的几何文档在同一次导入中继续供参数和骨架阶段只读使用。
void remember_document(const std::filesystem::path &file,const std::string &version,const std::shared_ptr<const nlohmann::json> &document);
struct DocumentResult {
  std::shared_ptr<const nlohmann::json> value;
  std::exception_ptr error;
  std::shared_ptr<const nlohmann::json> get() const {if(error) std::rethrow_exception(error);return value;}
};
// 调用方分批消费；句柄一直保留到消费完毕，避免预读结果被 LRU 提前淘汰。
std::vector<DocumentResult> prefetch_documents(std::span<const std::filesystem::path> files,DocumentView view=DocumentView::metadata);
std::string document_bytes(const std::filesystem::path &file);
std::string file_version(const std::filesystem::path &file);
const nlohmann::json &array_member(const nlohmann::json &value,const char *key);
const nlohmann::json &object_member(const nlohmann::json &value,const char *key);
}
