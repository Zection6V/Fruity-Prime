#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace MphRead::Mods::ClassicMenu
{
    // The Mods menu has its own language, unrelated to gameplay language.
    enum class MenuLanguage { English, Japanese, French };

    [[nodiscard]] MenuLanguage ParseMenuLanguage(std::string_view saved) noexcept;

    // Stable IDs for authored UI; unknown IDs return an empty string.
    [[nodiscard]] const std::string& TranslateMenuKey(std::string_view key, MenuLanguage language);

    // Compatibility with original DS/ROM strings and existing Composer calls:
    // English source text is resolved to a stable ID via Languages/en.json.
    // Unknown strings (including user/server-supplied names) are unchanged.
    [[nodiscard]] const std::string& TranslateMenuText(const std::string& english, MenuLanguage language);

    // Diagnostics and invariant checks for the three embedded catalogs.
    [[nodiscard]] bool MenuLocalizationCatalogValid();
    [[nodiscard]] std::size_t MenuLocalizationEntryCount();
}
