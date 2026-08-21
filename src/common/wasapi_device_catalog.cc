// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "common/wasapi_device_catalog.h"

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mmdeviceapi.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <string>
#include <utility>

namespace beatrice::common {

namespace {

using Microsoft::WRL::ComPtr;

auto WideToUtf8(const wchar_t* const text) -> std::string {
  if (text == nullptr || text[0] == L'\0') {
    return {};
  }
  const auto size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0,
                                        nullptr, nullptr);
  if (size <= 1) {
    return {};
  }
  auto result = std::string(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr,
                      nullptr);
  result.resize(static_cast<std::size_t>(size - 1));
  return result;
}

auto DeviceId(IMMDevice* const device) -> std::string {
  LPWSTR raw_id = nullptr;
  if (device == nullptr || FAILED(device->GetId(&raw_id))) {
    return {};
  }
  auto result = WideToUtf8(raw_id);
  CoTaskMemFree(raw_id);
  return result;
}

auto DeviceName(IMMDevice* const device) -> std::string {
  ComPtr<IPropertyStore> properties;
  if (device == nullptr ||
      FAILED(device->OpenPropertyStore(STGM_READ, &properties))) {
    return "Unnamed audio device";
  }
  PROPVARIANT value;
  PropVariantInit(&value);
  auto result = std::string("Unnamed audio device");
  if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
      value.vt == VT_LPWSTR) {
    const auto name = WideToUtf8(value.pwszVal);
    if (!name.empty()) {
      result = name;
    }
  }
  PropVariantClear(&value);
  return result;
}

auto EnumerateDirection(IMMDeviceEnumerator* const enumerator,
                        const EDataFlow direction)
    -> std::vector<WasapiDeviceInfo> {
  auto default_id = std::string{};
  ComPtr<IMMDevice> default_device;
  if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(
          direction, eConsole, &default_device))) {
    default_id = DeviceId(default_device.Get());
  }

  ComPtr<IMMDeviceCollection> collection;
  if (FAILED(enumerator->EnumAudioEndpoints(
          direction, DEVICE_STATE_ACTIVE, &collection))) {
    return {};
  }
  UINT count = 0;
  if (FAILED(collection->GetCount(&count))) {
    return {};
  }
  auto result = std::vector<WasapiDeviceInfo>{};
  result.reserve(count);
  for (UINT index = 0; index < count; ++index) {
    ComPtr<IMMDevice> device;
    if (FAILED(collection->Item(index, &device))) {
      continue;
    }
    const auto id = DeviceId(device.Get());
    if (id.empty()) {
      continue;
    }
    result.push_back({.id = id,
                      .name = DeviceName(device.Get()),
                      .is_default = id == default_id});
  }
  return result;
}

}  // namespace

auto EnumerateWasapiDevices() -> WasapiDeviceSnapshot {
  const auto com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const auto uninitialize = SUCCEEDED(com_result);
  if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
    return {.error = "Windows audio initialization failed."};
  }

  ComPtr<IMMDeviceEnumerator> enumerator;
  const auto create_result = CoCreateInstance(
      __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
      IID_PPV_ARGS(&enumerator));
  if (FAILED(create_result)) {
    if (uninitialize) {
      CoUninitialize();
    }
    return {.error = "Windows audio devices could not be enumerated."};
  }

  auto snapshot = WasapiDeviceSnapshot{
      .inputs = EnumerateDirection(enumerator.Get(), eCapture),
      .outputs = EnumerateDirection(enumerator.Get(), eRender)};
  enumerator.Reset();
  if (uninitialize) {
    CoUninitialize();
  }
  return snapshot;
}

}  // namespace beatrice::common

#else

namespace beatrice::common {

auto EnumerateWasapiDevices() -> WasapiDeviceSnapshot {
  return {.error = "WASAPI is only available on Windows."};
}

}  // namespace beatrice::common

#endif
