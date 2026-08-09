// Copyright (c) 2026 Project Beatrice and Contributors

#include "ui/control_help_text.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>  // NOLINT(misc-include-cleaner)
#else
#include <locale>
#include <string>
#endif

namespace beatrice::ui {

auto IsJapaneseEnvironment() noexcept -> bool {
  static const auto japanese = []() noexcept {
#if defined(_WIN32)
    // Query the user's Windows settings directly. std::locale("").name() can
    // be empty inside a DAW even when both Windows Culture and UI Culture are
    // ja-JP, which previously disabled every tooltip before it was attached.
    const auto ui_language = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(ui_language) == LANG_JAPANESE) {
      return true;
    }

    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH]{};
    const auto length = GetUserDefaultLocaleName(
        locale_name, LOCALE_NAME_MAX_LENGTH);
    return length >= 3 &&
           (locale_name[0] == L'j' || locale_name[0] == L'J') &&
           (locale_name[1] == L'a' || locale_name[1] == L'A') &&
           (locale_name[2] == L'-' || locale_name[2] == L'_');
#else
    try {
      const auto locale_name = std::locale("").name();
      return locale_name.rfind("ja", 0) == 0 ||
             locale_name.find("Japanese") != std::string::npos ||
             locale_name.find("_JP") != std::string::npos ||
             locale_name.find("-JP") != std::string::npos;
    } catch (...) {
      return false;
    }
#endif
  }();
  return japanese;
}

}  // namespace beatrice::ui
