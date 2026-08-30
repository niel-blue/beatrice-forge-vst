// Copyright (c) 2026 Niel

// VSTGUI does not currently expose FILEOPENDIALOGOPTIONS through
// CNewFileSelector.  Keep the upstream selector implementation intact while
// applying Forge's Windows privacy policy at the COM creation boundary.

#if !defined(_WIN32)
#error This translation unit is Windows-only.
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shobjidl.h>

namespace beatrice::common::detail {

HRESULT WINAPI CreatePrivateFileDialog(REFCLSID class_id,
                                       LPUNKNOWN outer,
                                       DWORD context,
                                       REFIID interface_id,
                                       LPVOID* object) {
  const auto result =
      ::CoCreateInstance(class_id, outer, context, interface_id, object);
  if (FAILED(result) || object == nullptr || *object == nullptr ||
      !IsEqualIID(interface_id, IID_IFileDialog)) {
    return result;
  }

  const auto is_open_dialog = IsEqualCLSID(class_id, CLSID_FileOpenDialog);
  const auto is_save_dialog = IsEqualCLSID(class_id, CLSID_FileSaveDialog);
  if (!is_open_dialog && !is_save_dialog) {
    return result;
  }

  auto* const dialog = static_cast<IFileDialog*>(*object);
  auto options = FILEOPENDIALOGOPTIONS{};
  auto privacy_result = dialog->GetOptions(&options);
  if (SUCCEEDED(privacy_result)) {
    options = static_cast<FILEOPENDIALOGOPTIONS>(
        options | FOS_DONTADDTORECENT |
        (is_save_dialog ? FOS_NOTESTFILECREATE : 0));
    privacy_result = dialog->SetOptions(options);
  }

  // Do not show an unprotected dialog if Windows refuses the privacy flags.
  // VSTGUI keeps the returned interface alive until it observes the failed
  // HRESULT, which also avoids its save-dialog setup dereferencing nullptr.
  return FAILED(privacy_result) ? privacy_result : result;
}

}  // namespace beatrice::common::detail

// Compile the upstream implementation once, replacing only its two
// CoCreateInstance calls with the policy-enforcing factory above.  The CMake
// integration marks the original translation unit HEADER_FILE_ONLY so this
// inclusion cannot introduce duplicate definitions.
#define CoCreateInstance \
  ::beatrice::common::detail::CreatePrivateFileDialog
#include "../../lib/vst3sdk/vstgui4/vstgui/lib/platform/win32/winfileselector.cpp"
#undef CoCreateInstance
