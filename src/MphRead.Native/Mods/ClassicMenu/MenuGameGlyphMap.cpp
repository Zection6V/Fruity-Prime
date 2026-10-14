#include "MenuGameGlyphMap.hpp"

#include "../../Strings.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace MphRead::Mods::ClassicMenu
{
    std::int32_t FindGameGlyphCode(char32_t unicode)
    {
        if (unicode >= U' ' && unicode <= U'~')
            return static_cast<std::int32_t>(unicode);

        // The original game's table is owned by Strings. ClassicMenu alone
        // needs the reverse index, so it is constructed and cached here once.
        static const auto lookup = []
        {
            const auto source = Text::Strings::NonAsciiGlyphMappings();
            std::vector<std::pair<char32_t, std::int32_t>> entries;
            entries.reserve(source.size());
            for (std::size_t i = 0; i < source.size(); ++i)
            {
                const std::string& value = source[i];
                if (value.empty() || value == " ") continue;
                const NativeRuntime::Utf8Scalar scalar = NativeRuntime::RuneDecodeFromUtf8(value);
                if (!scalar.Valid() || scalar.Length != value.size() || scalar.Value < 0x80)
                    continue;
                entries.emplace_back(scalar.Value, static_cast<std::int32_t>(i + 128));
            }
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b)
                {
                    return a.first < b.first
                        || (a.first == b.first && a.second < b.second);
                });
            entries.erase(std::unique(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.first == b.first; }),
                entries.end());
            return entries;
        }();

        const auto found = std::lower_bound(lookup.begin(), lookup.end(), unicode,
            [](const auto& entry, char32_t value) { return entry.first < value; });
        return found != lookup.end() && found->first == unicode ? found->second : -1;
    }
}
