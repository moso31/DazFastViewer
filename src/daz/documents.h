#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
namespace dfv::daz {
enum class DocumentView {metadata,skeleton};
// 返回不可变文档；元数据视图不构造几何、UV 和 Morph 差值的数值 DOM。
std::shared_ptr<const nlohmann::json> document_view(const std::filesystem::path &file,DocumentView view=DocumentView::metadata);
void prefetch_documents(const std::vector<std::filesystem::path> &files);
std::string document_bytes(const std::filesystem::path &file);
std::string file_version(const std::filesystem::path &file);
const nlohmann::json &array_member(const nlohmann::json &value,const char *key);
const nlohmann::json &object_member(const nlohmann::json &value,const char *key);
}
