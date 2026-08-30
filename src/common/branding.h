// Copyright (c) 2026 Niel

#ifndef BEATRICE_COMMON_BRANDING_H_
#define BEATRICE_COMMON_BRANDING_H_

#include <string_view>

namespace beatrice::common {

inline constexpr std::string_view kProductName = "Beatrice Forge";
inline constexpr std::string_view kAuthorName = "Niel";

// The prefix is shared by the VST and standalone recorder so a recording can
// be identified without depending on which front end created it.
inline constexpr std::string_view kRecordingFilePrefix = "Beatrice-Forge-Rec";
inline constexpr std::string_view kPreviousRecordingFilePrefix =
    "Beatrice-Rec";
inline constexpr std::string_view kDefaultRecordingBaseName =
    "Beatrice-Forge-Rec.wav";

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_BRANDING_H_
