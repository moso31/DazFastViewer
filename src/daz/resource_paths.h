#pragma once
#include <filesystem>
#include <vector>

namespace dfv::daz {
// Iray search roots contain resources/, and are separate from content libraries.
std::vector<std::filesystem::path> studio_iray_roots();
// Only replace the volume when the configured library has the same root-relative path.
std::vector<std::filesystem::path> relocated_library_paths(
    const std::filesystem::path &file,const std::vector<std::filesystem::path> &roots);
}
