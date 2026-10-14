#include "MenuLocalization.hpp"
#include "ClassicMenuLanguages.generated.hpp"

#include <cstdint>
#include <string_view>

namespace MphRead::Mods::ClassicMenu
{
    namespace
    {
        using Generated::Record;

        std::string_view TextAt(const Record& row, unsigned column) noexcept
        {
            const char* pool = column == 0 ? Generated::kIds
                : column == 1 ? Generated::kEn
                : column == 2 ? Generated::kJa : Generated::kFr;
            return {pool + row.Offsets[column], row.Lengths[column]};
        }

        unsigned ColumnFor(MenuLanguage language) noexcept
        {
            return language == MenuLanguage::Japanese ? 2U
                : language == MenuLanguage::French ? 3U : 1U;
        }
    }

    MenuLanguage ParseMenuLanguage(std::string_view saved) noexcept
    {
        if (saved == "ja" || saved == "Japanese") return MenuLanguage::Japanese;
        if (saved == "fr" || saved == "French") return MenuLanguage::French;
        return MenuLanguage::English;
    }

    std::uint16_t FindMenuTextIndex(std::string_view english) noexcept
    {
        // The build generator sorts this small index in UTF-8 byte order.
        // Called once per ROM text at load, then only on Fill / Add mutation.
        std::size_t begin = 0, end = Generated::kByEnglish.size();
        while (begin < end)
        {
            const std::size_t middle = begin + (end - begin) / 2;
            const auto index = Generated::kByEnglish[middle];
            if (TextAt(Generated::kRecords[index], 1) < english) begin = middle + 1;
            else end = middle;
        }
        if (begin >= Generated::kByEnglish.size()) return UnknownMenuText;
        const auto index = Generated::kByEnglish[begin];
        return TextAt(Generated::kRecords[index], 1) == english ? index : UnknownMenuText;
    }

    std::string_view TranslateMenuIndex(std::uint16_t index, MenuLanguage language) noexcept
    {
        if (index >= Generated::kRecords.size()) return {};
        return TextAt(Generated::kRecords[index], ColumnFor(language));
    }

    std::string_view TranslateMenuKey(std::string_view key, MenuLanguage language) noexcept
    {
        // The stable translation IDs are sorted at build time.
        std::size_t begin = 0, end = Generated::kRecords.size();
        while (begin < end)
        {
            const std::size_t middle = begin + (end - begin) / 2;
            if (TextAt(Generated::kRecords[middle], 0) < key) begin = middle + 1;
            else end = middle;
        }
        if (begin == Generated::kRecords.size() || TextAt(Generated::kRecords[begin], 0) != key) return {};
        return TextAt(Generated::kRecords[begin], ColumnFor(language));
    }

    std::string_view TranslateMenuText(std::string_view english, MenuLanguage language) noexcept
    {
        if (language == MenuLanguage::English) return english;
        const auto index = FindMenuTextIndex(english);
        return index == UnknownMenuText ? english : TranslateMenuIndex(index, language);
    }

    bool MenuLocalizationCatalogValid() noexcept
    {
        // The generator fails the build on missing/duplicate IDs, malformed
        // UTF-8, missing values and inconsistent placeholders.
        return !Generated::kRecords.empty();
    }

    std::size_t MenuLocalizationEntryCount() noexcept
    {
        return Generated::kRecords.size();
    }
}
