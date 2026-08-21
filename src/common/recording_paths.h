// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_RECORDING_PATHS_H_
#define BEATRICE_COMMON_RECORDING_PATHS_H_

#include <filesystem>

namespace beatrice::common {

// Returns the user's platform music directory.  Both the standalone client
// and the VST recording panel use this as the initial recording location.
[[nodiscard]] auto MusicDirectory() -> std::filesystem::path;

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_RECORDING_PATHS_H_
