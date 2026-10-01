#include "daz/resource_paths.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <string>

namespace dfv::daz {
namespace fs=std::filesystem;
namespace {
bool same_path(const fs::path &a,const fs::path &b) {
  return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;
}
std::wstring environment(const wchar_t *name) {
  const auto size=GetEnvironmentVariableW(name,nullptr,0);if(!size)return {};
  std::wstring value(size,L'\0');const auto written=GetEnvironmentVariableW(name,value.data(),size);
  if(!written||written>=size)return {};value.resize(written);return value;
}
fs::path registry_installation(HKEY hive,const wchar_t *key,const wchar_t *name,DWORD view) {
  const auto flags=RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ|view;
  DWORD bytes=0;if(RegGetValueW(hive,key,name,flags,nullptr,nullptr,&bytes)!=ERROR_SUCCESS)return {};
  std::vector<wchar_t> value(bytes/sizeof(wchar_t)+1,L'\0');bytes=DWORD(value.size()*sizeof(wchar_t));
  if(RegGetValueW(hive,key,name,flags,nullptr,value.data(),&bytes)!=ERROR_SUCCESS)return {};
  return fs::path(value.data());
}
}
std::vector<fs::path> studio_iray_roots() {
  std::vector<fs::path> roots;
  auto add=[&](const fs::path &installation) {
    if(installation.empty())return;
    std::error_code ec;auto root=fs::weakly_canonical(installation/"shaders/iray",ec);
    if(ec||!fs::is_directory(root/"resources",ec))return;
    if(std::none_of(roots.begin(),roots.end(),[&](const auto &old){return same_path(old,root);}))roots.push_back(std::move(root));
  };
  // Optional override for portable/unregistered installations; no system settings are changed.
  add(environment(L"DFV_DAZ_STUDIO_PATH"));
  for(const auto hive:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE})
    for(const auto *key:{L"Software\\DAZ\\Studio4",L"Software\\DAZ\\Studio6",L"Software\\DAZ\\Studio4 Public Build",L"Software\\DAZ\\Studio6 Public Build"})
      for(const DWORD view:{RRF_SUBKEY_WOW6464KEY,RRF_SUBKEY_WOW6432KEY})
        for(const auto *name:{L"InstallPath-64",L"InstallPath"})add(registry_installation(hive,key,name,view));
  // Retain the usual installation layout when no registration exists.
  for(const auto *variable:{L"ProgramW6432",L"ProgramFiles"}) {
    const auto programs=environment(variable);if(programs.empty())continue;
    for(const auto *name:{L"DAZStudio4",L"DAZStudio6",L"DAZStudio4 Public Build",L"DAZStudio6 Public Build"})add(fs::path(programs)/L"DAZ 3D"/name);
  }
  return roots;
}
std::vector<fs::path> relocated_library_paths(const fs::path &file,const std::vector<fs::path> &roots) {
  std::vector<fs::path> candidates;
  if(!file.is_absolute()||!file.has_root_name())return candidates;
  const auto source=file.lexically_normal().relative_path();
  for(const auto &root:roots) {
    auto library=root.lexically_normal().relative_path();
    while(!library.empty()&&library.filename().empty())library=library.parent_path();
    if(library.empty())continue;
    auto part=source.begin(),prefix=library.begin();
    for(;part!=source.end()&&prefix!=library.end()&&same_path(*part,*prefix);++part,++prefix){}
    if(prefix!=library.end()||part==source.end())continue;
    fs::path relative;for(;part!=source.end();++part)relative/=*part;
    candidates.push_back(root/relative);
  }
  return candidates;
}
}
