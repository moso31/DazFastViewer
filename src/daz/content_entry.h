#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>
#include <set>
#include "daz/documents.h"
#include "daz/loader.h"

namespace dfv::daz {
inline std::string entry_key(const std::filesystem::path &path) {
  const auto u8=path.generic_u8string();std::string s(u8.begin(),u8.end());for(auto &c:s) if(c>='A'&&c<='Z') c+=32;return s;
}
// 此适配只加载产品已提供的基础 DUF；不读取或执行加密脚本。
inline bool hd_nipples_entry(const std::filesystem::path &path) {
  return entry_key(path).ends_with("/people/genesis 8 female/anatomy/3feetwolf/hd nipples - 2.0/hd nipples for g8f - 2.0.dse");
}
inline bool supported_content_entry(const std::filesystem::path &path) {const auto ext=entry_key(path.extension());return ext==".duf"||ext==".djl"||hd_nipples_entry(path);}
inline std::filesystem::path content_asset(const std::filesystem::path &entry,const std::vector<std::filesystem::path> &roots) {
  if(entry_key(entry.extension())==".djl") {
    auto current=entry;std::set<std::string> visited;
    for(int depth=0;depth<32;++depth) {
      if(!visited.insert(entry_key(std::filesystem::absolute(current).lexically_normal())).second)throw std::runtime_error("内容链接存在循环引用");
      const auto doc=document_view(current);const auto link=doc->find("path");
      if(link==doc->end()||!link->is_string()||link->get<std::string>().empty())throw std::runtime_error("内容链接缺少有效的 path");
      const auto target=std::filesystem::u8path(decode_uri(link->get<std::string>()));
      std::vector<std::filesystem::path> candidates;
      if(target.has_root_name())candidates.push_back(target);
      // 优先链接所属库，再跨库查找；DAZ 的前导斜线表示库内路径。
      for(const auto &root:roots)if(entry_key(current).starts_with(entry_key(root)+"/"))candidates.push_back(root/target.relative_path());
      for(const auto &root:roots)candidates.push_back(root/target.relative_path());
      candidates.push_back(current.parent_path()/target.relative_path());
      bool found=false;for(const auto &candidate:candidates){std::error_code error;if(std::filesystem::is_regular_file(candidate,error)){current=candidate.lexically_normal();found=true;break;}}
      if(!found)throw std::runtime_error("内容链接的目标不存在："+link->get<std::string>());
      if(entry_key(current.extension())!=".djl")return content_asset(current,roots);
    }
    throw std::runtime_error("内容链接层级过深");
  }
  if(entry_key(entry.extension())==".duf") return entry;
  if(!hd_nipples_entry(entry)) throw std::runtime_error("此 DAZ 脚本尚未支持，请选择对应的 DUF 资产或保存后的场景");
  auto libraries=roots;
  for(auto p=entry.parent_path();!p.empty()&&p!=p.parent_path();p=p.parent_path()) if(std::filesystem::is_directory(p/"data")) {libraries.insert(libraries.begin(),p);break;}
  for(const auto &root:libraries) {
    const auto file=root/"data/3feetwolf/HD Nipples for G8F - 2.0/HD Nipples - 2.0/HD Nipples for G8F - 2.0.duf";
    if(std::filesystem::is_regular_file(file)) return file;
  }
  throw std::runtime_error("没有找到 HD Nipples for G8F - 2.0 的基础 DUF，请检查产品是否完整安装");
}
}
