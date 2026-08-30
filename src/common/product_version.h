// Copyright (c) 2026 Niel

#ifndef BEATRICE_COMMON_PRODUCT_VERSION_H_
#define BEATRICE_COMMON_PRODUCT_VERSION_H_

#include <string_view>

// Product targets supply these values from BeatriceForgeVersion.cmake.  The
// fallback keeps source-only tools usable without inventing a product version.
#ifndef BEATRICE_FORGE_VERSION_STR
#define BEATRICE_FORGE_VERSION_STR "unknown"
#endif
#ifndef BEATRICE_FORGE_FULL_VERSION_STR
#define BEATRICE_FORGE_FULL_VERSION_STR "unknown"
#endif
#ifndef BEATRICE_BASE_VERSION_STR
#define BEATRICE_BASE_VERSION_STR "unknown"
#endif

namespace beatrice::common {

inline constexpr std::string_view kForgeVersion =
    BEATRICE_FORGE_VERSION_STR;
inline constexpr std::string_view kForgeFullVersion =
    BEATRICE_FORGE_FULL_VERSION_STR;
inline constexpr std::string_view kBeatriceBaseVersion =
    BEATRICE_BASE_VERSION_STR;

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_PRODUCT_VERSION_H_
