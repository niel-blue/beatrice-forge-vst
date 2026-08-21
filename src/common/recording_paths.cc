// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "common/recording_paths.h"

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

namespace beatrice::common {

auto MusicDirectory() -> std::filesystem::path {
#ifdef _WIN32
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Music, KF_FLAG_DEFAULT, nullptr,
                                     &path))) {
    const auto result = std::filesystem::path(path);
    CoTaskMemFree(path);
    return result;
  }
  if (path != nullptr) {
    CoTaskMemFree(path);
  }
#endif
  std::error_code error;
  const auto current = std::filesystem::current_path(error);
  return error ? std::filesystem::path{} : current;
}

}  // namespace beatrice::common
