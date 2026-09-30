#pragma once
#include <filesystem>
#include <cstdlib>
#include <string>

namespace dfv {
// Cycles appends "cache" to this directory. Keep writable caches out of the
// build checkout and installation folder when the application is relocated.
inline std::string cycles_user_directory() {
  const wchar_t *local = _wgetenv(L"LOCALAPPDATA");
  auto path = (local ? std::filesystem::path(local) : std::filesystem::temp_directory_path()) / L"DazFastViewer";
  const auto utf8 = path.u8string();
  return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}
}
