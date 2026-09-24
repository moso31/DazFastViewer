#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace dfv::daz {
inline std::string entry_key(const std::filesystem::path &path) {
  const auto u8=path.generic_u8string();std::string s(u8.begin(),u8.end());for(auto &c:s) if(c>='A'&&c<='Z') c+=32;return s;
}
// 此适配只加载产品已提供的基础 DUF；不读取或执行加密脚本。
inline bool hd_nipples_entry(const std::filesystem::path &path) {
  return entry_key(path).ends_with("/people/genesis 8 female/anatomy/3feetwolf/hd nipples - 2.0/hd nipples for g8f - 2.0.dse");
}
inline bool supported_content_entry(const std::filesystem::path &path) {return entry_key(path.extension())==".duf"||hd_nipples_entry(path);}
inline std::filesystem::path content_asset(const std::filesystem::path &entry,const std::vector<std::filesystem::path> &roots) {
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
