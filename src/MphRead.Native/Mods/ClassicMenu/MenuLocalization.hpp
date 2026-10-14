#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace MphRead::Mods::ClassicMenu
{
    // Independent of Scene/SettingsModel game language.
    enum class MenuLanguage { English, Japanese, French };
    inline constexpr std::uint16_t UnknownMenuText = std::numeric_limits<std::uint16_t>::max();

    [[nodiscard]] MenuLanguage ParseMenuLanguage(std::string_view saved) noexcept;
    [[nodiscard]] std::uint16_t FindMenuTextIndex(std::string_view english) noexcept;
    [[nodiscard]] std::string_view TranslateMenuIndex(std::uint16_t index, MenuLanguage language) noexcept;
    [[nodiscard]] std::string_view TranslateMenuKey(std::string_view key, MenuLanguage language) noexcept;
    [[nodiscard]] std::string_view TranslateMenuText(std::string_view english, MenuLanguage language) noexcept;
    [[nodiscard]] bool MenuLocalizationCatalogValid() noexcept;
    [[nodiscard]] std::size_t MenuLocalizationEntryCount() noexcept;
}
