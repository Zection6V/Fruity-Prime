#pragma once

#include <string>
#include <string_view>

namespace MphRead::Mods::ClassicMenu
{
    // The ROM and game logic always use English; only presentation is localized.
    enum class MenuLanguage { English, Japanese, French };

    [[nodiscard]] MenuLanguage ParseMenuLanguage(std::string_view saved) noexcept;
    [[nodiscard]] const std::string& TranslateMenuText(const std::string& english, MenuLanguage language);
}
