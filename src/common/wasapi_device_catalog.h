// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_WASAPI_DEVICE_CATALOG_H_
#define BEATRICE_COMMON_WASAPI_DEVICE_CATALOG_H_

#include <string>
#include <vector>

namespace beatrice::common {

enum class WasapiDeviceDirection {
  kInput,
  kOutput,
};

struct WasapiDeviceInfo {
  std::string id;
  std::string name;
  bool is_default = false;
};

struct WasapiDeviceSnapshot {
  std::vector<WasapiDeviceInfo> inputs;
  std::vector<WasapiDeviceInfo> outputs;
  std::string error;
};

// Enumerates active Windows audio endpoints without opening or taking control
// of them.  The IDs are the stable endpoint IDs returned by MMDevice API; the
// friendly names are intended for presentation only.
auto EnumerateWasapiDevices() -> WasapiDeviceSnapshot;

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_WASAPI_DEVICE_CATALOG_H_
