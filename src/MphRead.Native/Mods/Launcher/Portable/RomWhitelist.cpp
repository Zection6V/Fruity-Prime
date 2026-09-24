#include "RomWhitelist.hpp"

#include "../../../NativeRuntime/System/Cryptography.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Launcher
{
    namespace
    {
        // Dictionary<string, string>(StringComparer.OrdinalIgnoreCase). The
        // keys are compared against a lowercase hex hash, so an ordinal match
        // against these lowercase keys is the same comparison.
        constexpr std::array<std::pair<std::string_view, std::string_view>, 7> Known = {{
            {"c71a2d5fd41727c31f1619fb3085d4df", "Europe 1.0 (AMHP0)"},
            {"378297159f176802e27384e18e33a1c4", "Europe 1.1 (AMHP1)"},
            {"42850a19d7be2ee5e067df6984aa900e", "Japan 1.0 (AMHJ0)"},
            {"11db1b8b49065f955f68e10062416ab3", "Japan 1.1 (AMHJ1)"},
            {"e83239677f0e2b1f75210bb0978f9007", "Korea 1.0 (AMHK0)"},
            {"b4c8a9398866b49c7be17d75736a223b", "USA 1.0 (AMHE0)"},
            {"9fe5f1eb1eb9dc5d90130408f813b39e", "USA 1.1 (AMHE1)"},
        }};
    }

    std::optional<std::string> RomWhitelist::Hash(const std::string& path)
    {
        try
        {
            const std::array<std::uint8_t, 16> digest
                = ::MphRead::NativeRuntime::Md5ComputeHashOfFile(path);
            std::string text = ::MphRead::NativeRuntime::ConvertToHexString(digest);
            // ToLowerInvariant over hex digits.
            for (char& value : text)
            {
                if (value >= 'A' && value <= 'Z')
                {
                    value = static_cast<char>(value - 'A' + 'a');
                }
            }
            return text;
        }
        catch (const System::IO::IOException&)
        {
            return std::nullopt;
        }
        catch (const System::UnauthorizedAccessException&)
        {
            return std::nullopt;
        }
    }

    bool RomWhitelist::TryIdentify(
        const std::string& path, std::optional<std::string>& label)
    {
        const std::optional<std::string> hash = Hash(path);
        if (hash.has_value())
        {
            for (const auto& entry : Known)
            {
                if (entry.first == *hash)
                {
                    label = std::string(entry.second);
                    return true;
                }
            }
        }
        label = std::nullopt;
        return false;
    }
}
