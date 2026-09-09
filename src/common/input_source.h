#ifndef BEATRICE_COMMON_INPUT_SOURCE_H_
#define BEATRICE_COMMON_INPUT_SOURCE_H_

#include <cstdint>

namespace beatrice::common {

// Source choices shared by the VST input selectors.  The standalone client
// keeps its legacy endpoint selection state separately, but uses the same
// meaning when it hosts the shared editor.
enum class InputSource : std::uint32_t {
  kOff = 0,
  kDawInput = 1,
  kApplicationInput = 2,
  // Legacy serialized value.  Audio Files is intentionally not exposed by
  // the current VST or standalone UI, but retaining the numeric value keeps
  // old project data readable for migration.
  kAudioFile = 3,
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_INPUT_SOURCE_H_
