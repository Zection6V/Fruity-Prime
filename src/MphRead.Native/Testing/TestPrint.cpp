#include "TestPrint.hpp"

#include "../Formats/Formats.hpp"
#include "../Formats/Model.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace
{
    class ManagedIndexOutOfRangeException final : public std::out_of_range
    {
    public:
        ManagedIndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };

    [[nodiscard]] constexpr std::uint32_t ManagedUInt32(std::int32_t value) noexcept
    {
        return std::bit_cast<std::uint32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t ManagedInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t AddInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) + ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t MultiplyInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) * ManagedUInt32(right));
    }

    template <typename T>
    [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] const T& ManagedAt(const std::vector<T>& values, std::size_t index)
    {
        if (index >= values.size())
        {
            throw ManagedIndexOutOfRangeException();
        }
        return values[index];
    }

    [[nodiscard]] const std::string& RequireString(const std::optional<std::string>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] const std::string& InterpolationString(
        const std::optional<std::string>& value) noexcept
    {
        static const std::string empty;
        return value ? *value : empty;
    }

    struct Utf8CodePoint final
    {
        char32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] Utf8CodePoint DecodeUtf8(
        std::string_view value, std::size_t offset) noexcept
    {
        const auto first = static_cast<unsigned char>(value[offset]);
        if (first < 0x80U)
        {
            return {first, 1};
        }

        auto continuation = [&](std::size_t index) noexcept -> int
        {
            if (index >= value.size())
            {
                return -1;
            }
            const auto byte = static_cast<unsigned char>(value[index]);
            return (byte & 0xC0U) == 0x80U ? static_cast<int>(byte & 0x3FU) : -1;
        };

        if ((first & 0xE0U) == 0xC0U)
        {
            const int c1 = continuation(offset + 1);
            if (c1 >= 0)
            {
                const char32_t cp = static_cast<char32_t>(((first & 0x1FU) << 6) | c1);
                if (cp >= 0x80)
                {
                    return {cp, 2};
                }
            }
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            const int c1 = continuation(offset + 1);
            const int c2 = continuation(offset + 2);
            if (c1 >= 0 && c2 >= 0)
            {
                const char32_t cp = static_cast<char32_t>(
                    ((first & 0x0FU) << 12) | (c1 << 6) | c2);
                if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF))
                {
                    return {cp, 3};
                }
            }
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            const int c1 = continuation(offset + 1);
            const int c2 = continuation(offset + 2);
            const int c3 = continuation(offset + 3);
            if (c1 >= 0 && c2 >= 0 && c3 >= 0)
            {
                const char32_t cp = static_cast<char32_t>(
                    ((first & 0x07U) << 18) | (c1 << 12) | (c2 << 6) | c3);
                if (cp >= 0x10000 && cp <= 0x10FFFF)
                {
                    return {cp, 4};
                }
            }
        }
        return {first, 1};
    }

    [[nodiscard]] constexpr bool ManagedCharIsWhiteSpace(char32_t value) noexcept
    {
        return value == 0x0009 || value == 0x000A || value == 0x000B
            || value == 0x000C || value == 0x000D || value == 0x0020
            || value == 0x0085 || value == 0x00A0 || value == 0x1680
            || (value >= 0x2000 && value <= 0x200A)
            || value == 0x2028 || value == 0x2029 || value == 0x202F
            || value == 0x205F || value == 0x3000;
    }

    [[nodiscard]] bool IsNullOrWhiteSpace(const std::optional<std::string>& value) noexcept
    {
        if (!value || value->empty())
        {
            return true;
        }
        std::size_t offset = 0;
        while (offset < value->size())
        {
            const Utf8CodePoint cp = DecodeUtf8(*value, offset);
            if (!ManagedCharIsWhiteSpace(cp.Value))
            {
                return false;
            }
            offset += cp.Length;
        }
        return true;
    }

    [[nodiscard]] std::string TrimManaged(std::string_view value)
    {
        struct Span final
        {
            std::size_t Offset;
            std::size_t Length;
            char32_t Value;
        };

        std::vector<Span> spans;
        spans.reserve(value.size());
        for (std::size_t offset = 0; offset < value.size();)
        {
            const Utf8CodePoint cp = DecodeUtf8(value, offset);
            spans.push_back({offset, cp.Length, cp.Value});
            offset += cp.Length;
        }

        std::size_t first = 0;
        while (first < spans.size() && ManagedCharIsWhiteSpace(spans[first].Value))
        {
            first++;
        }
        if (first == spans.size())
        {
            return {};
        }

        std::size_t last = spans.size();
        while (last > first && ManagedCharIsWhiteSpace(spans[last - 1].Value))
        {
            last--;
        }

        const std::size_t start = spans[first].Offset;
        const std::size_t end = spans[last - 1].Offset + spans[last - 1].Length;
        return std::string(value.substr(start, end - start));
    }

    [[nodiscard]] std::string ReplaceAll(
        std::string value, std::string_view oldValue, std::string_view newValue)
    {
        if (oldValue.empty())
        {
            throw std::invalid_argument("String cannot be of zero length. (Parameter 'oldValue')");
        }
        std::size_t offset = 0;
        while ((offset = value.find(oldValue, offset)) != std::string::npos)
        {
            value.replace(offset, oldValue.size(), newValue);
            offset += newValue.size();
        }
        return value;
    }

    [[nodiscard]] std::vector<std::string> SplitChar(std::string_view value, char separator)
    {
        std::vector<std::string> result;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t found = value.find(separator, start);
            if (found == std::string_view::npos)
            {
                result.emplace_back(value.substr(start));
                return result;
            }
            result.emplace_back(value.substr(start, found - start));
            start = found + 1;
        }
    }

    [[nodiscard]] std::vector<std::string> SplitString(
        std::string_view value, std::string_view separator)
    {
        std::vector<std::string> result;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t found = value.find(separator, start);
            if (found == std::string_view::npos)
            {
                result.emplace_back(value.substr(start));
                return result;
            }
            result.emplace_back(value.substr(start, found - start));
            start = found + separator.size();
        }
    }

    [[nodiscard]] constexpr std::string_view EnvironmentNewLine() noexcept
    {
#ifdef _WIN32
        return "\r\n";
#else
        return "\n";
#endif
    }

    [[nodiscard]] std::string FormatInt32CurrentCulture(std::int32_t value)
    {
        // Fixed::ToString is the existing Native managed-format owner and carries
        // the Native runtime's thread-local managed negative-sign state.
        return MphRead::Fixed(value).ToString();
    }

    [[nodiscard]] std::string FormatUInt32CurrentCulture(std::uint32_t value)
    {
        // Standard UInt32 "G" formatting has no sign or grouping. Emit its decimal
        // digits directly so a caller-imbued C++ stream locale cannot alter them.
        char buffer[16]{};
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format UInt32.");
        }
        return std::string(buffer, end);
    }

    [[nodiscard]] std::string ManagedCurrentNegativeSign()
    {
        // Reuse the existing managed-format owner instead of maintaining pair-local
        // culture state. .NET integer formatting emits ASCII decimal digits; stripping
        // the final '1' from -1 leaves the current managed NegativeSign.
        std::string formatted = FormatInt32CurrentCulture(-1);
        if (!formatted.empty() && formatted.back() == '1')
        {
            formatted.pop_back();
        }
        return formatted;
    }

    // External parity blocker: the shared Native culture owner exposes the managed
    // negative sign through formatting but has no PositiveSign / general Int32.Parse
    // contract. This fallback therefore uses the canonical negative sign and retains
    // '+' only for the default positive-sign path.
    [[nodiscard]] std::int32_t ParseInt32CurrentCultureFallback(std::string_view value)
    {
        std::size_t first = 0;
        while (first < value.size())
        {
            const Utf8CodePoint cp = DecodeUtf8(value, first);
            if (!ManagedCharIsWhiteSpace(cp.Value))
            {
                break;
            }
            first += cp.Length;
        }

        bool negative = false;
        const std::string negativeSign = ManagedCurrentNegativeSign();
        if (!negativeSign.empty()
            && first + negativeSign.size() <= value.size()
            && value.substr(first, negativeSign.size()) == negativeSign)
        {
            negative = true;
            first += negativeSign.size();
        }
        else if (first < value.size() && value[first] == '+')
        {
            // The shared Native culture owner does not currently expose PositiveSign.
            first++;
        }

        const std::size_t digitStart = first;
        std::uint64_t magnitude = 0;
        const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
        while (first < value.size() && value[first] >= '0' && value[first] <= '9')
        {
            const unsigned digit = static_cast<unsigned>(value[first] - '0');
            if (magnitude > (limit - digit) / 10ULL)
            {
                throw System::OverflowException();
            }
            magnitude = magnitude * 10ULL + digit;
            first++;
        }
        if (first == digitStart)
        {
            throw System::FormatException();
        }

        while (first < value.size())
        {
            const Utf8CodePoint cp = DecodeUtf8(value, first);
            if (!ManagedCharIsWhiteSpace(cp.Value))
            {
                throw System::FormatException();
            }
            first += cp.Length;
        }

        if (negative)
        {
            if (magnitude == 2147483648ULL)
            {
                return std::numeric_limits<std::int32_t>::min();
            }
            return -static_cast<std::int32_t>(magnitude);
        }
        return static_cast<std::int32_t>(magnitude);
    }

    struct SimpleUpperMapping final
    {
        char16_t Source;
        char16_t Target;
    };

    // Unicode 15.0 simple uppercase mappings, matching the Unicode data consumed by .NET 9.
    static constexpr std::array<SimpleUpperMapping, 1190> SimpleUpperMappings{{
        SimpleUpperMapping{char16_t{0x0061}, char16_t{0x0041}},
        SimpleUpperMapping{char16_t{0x0062}, char16_t{0x0042}},
        SimpleUpperMapping{char16_t{0x0063}, char16_t{0x0043}},
        SimpleUpperMapping{char16_t{0x0064}, char16_t{0x0044}},
        SimpleUpperMapping{char16_t{0x0065}, char16_t{0x0045}},
        SimpleUpperMapping{char16_t{0x0066}, char16_t{0x0046}},
        SimpleUpperMapping{char16_t{0x0067}, char16_t{0x0047}},
        SimpleUpperMapping{char16_t{0x0068}, char16_t{0x0048}},
        SimpleUpperMapping{char16_t{0x0069}, char16_t{0x0049}},
        SimpleUpperMapping{char16_t{0x006A}, char16_t{0x004A}},
        SimpleUpperMapping{char16_t{0x006B}, char16_t{0x004B}},
        SimpleUpperMapping{char16_t{0x006C}, char16_t{0x004C}},
        SimpleUpperMapping{char16_t{0x006D}, char16_t{0x004D}},
        SimpleUpperMapping{char16_t{0x006E}, char16_t{0x004E}},
        SimpleUpperMapping{char16_t{0x006F}, char16_t{0x004F}},
        SimpleUpperMapping{char16_t{0x0070}, char16_t{0x0050}},
        SimpleUpperMapping{char16_t{0x0071}, char16_t{0x0051}},
        SimpleUpperMapping{char16_t{0x0072}, char16_t{0x0052}},
        SimpleUpperMapping{char16_t{0x0073}, char16_t{0x0053}},
        SimpleUpperMapping{char16_t{0x0074}, char16_t{0x0054}},
        SimpleUpperMapping{char16_t{0x0075}, char16_t{0x0055}},
        SimpleUpperMapping{char16_t{0x0076}, char16_t{0x0056}},
        SimpleUpperMapping{char16_t{0x0077}, char16_t{0x0057}},
        SimpleUpperMapping{char16_t{0x0078}, char16_t{0x0058}},
        SimpleUpperMapping{char16_t{0x0079}, char16_t{0x0059}},
        SimpleUpperMapping{char16_t{0x007A}, char16_t{0x005A}},
        SimpleUpperMapping{char16_t{0x00B5}, char16_t{0x039C}},
        SimpleUpperMapping{char16_t{0x00E0}, char16_t{0x00C0}},
        SimpleUpperMapping{char16_t{0x00E1}, char16_t{0x00C1}},
        SimpleUpperMapping{char16_t{0x00E2}, char16_t{0x00C2}},
        SimpleUpperMapping{char16_t{0x00E3}, char16_t{0x00C3}},
        SimpleUpperMapping{char16_t{0x00E4}, char16_t{0x00C4}},
        SimpleUpperMapping{char16_t{0x00E5}, char16_t{0x00C5}},
        SimpleUpperMapping{char16_t{0x00E6}, char16_t{0x00C6}},
        SimpleUpperMapping{char16_t{0x00E7}, char16_t{0x00C7}},
        SimpleUpperMapping{char16_t{0x00E8}, char16_t{0x00C8}},
        SimpleUpperMapping{char16_t{0x00E9}, char16_t{0x00C9}},
        SimpleUpperMapping{char16_t{0x00EA}, char16_t{0x00CA}},
        SimpleUpperMapping{char16_t{0x00EB}, char16_t{0x00CB}},
        SimpleUpperMapping{char16_t{0x00EC}, char16_t{0x00CC}},
        SimpleUpperMapping{char16_t{0x00ED}, char16_t{0x00CD}},
        SimpleUpperMapping{char16_t{0x00EE}, char16_t{0x00CE}},
        SimpleUpperMapping{char16_t{0x00EF}, char16_t{0x00CF}},
        SimpleUpperMapping{char16_t{0x00F0}, char16_t{0x00D0}},
        SimpleUpperMapping{char16_t{0x00F1}, char16_t{0x00D1}},
        SimpleUpperMapping{char16_t{0x00F2}, char16_t{0x00D2}},
        SimpleUpperMapping{char16_t{0x00F3}, char16_t{0x00D3}},
        SimpleUpperMapping{char16_t{0x00F4}, char16_t{0x00D4}},
        SimpleUpperMapping{char16_t{0x00F5}, char16_t{0x00D5}},
        SimpleUpperMapping{char16_t{0x00F6}, char16_t{0x00D6}},
        SimpleUpperMapping{char16_t{0x00F8}, char16_t{0x00D8}},
        SimpleUpperMapping{char16_t{0x00F9}, char16_t{0x00D9}},
        SimpleUpperMapping{char16_t{0x00FA}, char16_t{0x00DA}},
        SimpleUpperMapping{char16_t{0x00FB}, char16_t{0x00DB}},
        SimpleUpperMapping{char16_t{0x00FC}, char16_t{0x00DC}},
        SimpleUpperMapping{char16_t{0x00FD}, char16_t{0x00DD}},
        SimpleUpperMapping{char16_t{0x00FE}, char16_t{0x00DE}},
        SimpleUpperMapping{char16_t{0x00FF}, char16_t{0x0178}},
        SimpleUpperMapping{char16_t{0x0101}, char16_t{0x0100}},
        SimpleUpperMapping{char16_t{0x0103}, char16_t{0x0102}},
        SimpleUpperMapping{char16_t{0x0105}, char16_t{0x0104}},
        SimpleUpperMapping{char16_t{0x0107}, char16_t{0x0106}},
        SimpleUpperMapping{char16_t{0x0109}, char16_t{0x0108}},
        SimpleUpperMapping{char16_t{0x010B}, char16_t{0x010A}},
        SimpleUpperMapping{char16_t{0x010D}, char16_t{0x010C}},
        SimpleUpperMapping{char16_t{0x010F}, char16_t{0x010E}},
        SimpleUpperMapping{char16_t{0x0111}, char16_t{0x0110}},
        SimpleUpperMapping{char16_t{0x0113}, char16_t{0x0112}},
        SimpleUpperMapping{char16_t{0x0115}, char16_t{0x0114}},
        SimpleUpperMapping{char16_t{0x0117}, char16_t{0x0116}},
        SimpleUpperMapping{char16_t{0x0119}, char16_t{0x0118}},
        SimpleUpperMapping{char16_t{0x011B}, char16_t{0x011A}},
        SimpleUpperMapping{char16_t{0x011D}, char16_t{0x011C}},
        SimpleUpperMapping{char16_t{0x011F}, char16_t{0x011E}},
        SimpleUpperMapping{char16_t{0x0121}, char16_t{0x0120}},
        SimpleUpperMapping{char16_t{0x0123}, char16_t{0x0122}},
        SimpleUpperMapping{char16_t{0x0125}, char16_t{0x0124}},
        SimpleUpperMapping{char16_t{0x0127}, char16_t{0x0126}},
        SimpleUpperMapping{char16_t{0x0129}, char16_t{0x0128}},
        SimpleUpperMapping{char16_t{0x012B}, char16_t{0x012A}},
        SimpleUpperMapping{char16_t{0x012D}, char16_t{0x012C}},
        SimpleUpperMapping{char16_t{0x012F}, char16_t{0x012E}},
        SimpleUpperMapping{char16_t{0x0131}, char16_t{0x0049}},
        SimpleUpperMapping{char16_t{0x0133}, char16_t{0x0132}},
        SimpleUpperMapping{char16_t{0x0135}, char16_t{0x0134}},
        SimpleUpperMapping{char16_t{0x0137}, char16_t{0x0136}},
        SimpleUpperMapping{char16_t{0x013A}, char16_t{0x0139}},
        SimpleUpperMapping{char16_t{0x013C}, char16_t{0x013B}},
        SimpleUpperMapping{char16_t{0x013E}, char16_t{0x013D}},
        SimpleUpperMapping{char16_t{0x0140}, char16_t{0x013F}},
        SimpleUpperMapping{char16_t{0x0142}, char16_t{0x0141}},
        SimpleUpperMapping{char16_t{0x0144}, char16_t{0x0143}},
        SimpleUpperMapping{char16_t{0x0146}, char16_t{0x0145}},
        SimpleUpperMapping{char16_t{0x0148}, char16_t{0x0147}},
        SimpleUpperMapping{char16_t{0x014B}, char16_t{0x014A}},
        SimpleUpperMapping{char16_t{0x014D}, char16_t{0x014C}},
        SimpleUpperMapping{char16_t{0x014F}, char16_t{0x014E}},
        SimpleUpperMapping{char16_t{0x0151}, char16_t{0x0150}},
        SimpleUpperMapping{char16_t{0x0153}, char16_t{0x0152}},
        SimpleUpperMapping{char16_t{0x0155}, char16_t{0x0154}},
        SimpleUpperMapping{char16_t{0x0157}, char16_t{0x0156}},
        SimpleUpperMapping{char16_t{0x0159}, char16_t{0x0158}},
        SimpleUpperMapping{char16_t{0x015B}, char16_t{0x015A}},
        SimpleUpperMapping{char16_t{0x015D}, char16_t{0x015C}},
        SimpleUpperMapping{char16_t{0x015F}, char16_t{0x015E}},
        SimpleUpperMapping{char16_t{0x0161}, char16_t{0x0160}},
        SimpleUpperMapping{char16_t{0x0163}, char16_t{0x0162}},
        SimpleUpperMapping{char16_t{0x0165}, char16_t{0x0164}},
        SimpleUpperMapping{char16_t{0x0167}, char16_t{0x0166}},
        SimpleUpperMapping{char16_t{0x0169}, char16_t{0x0168}},
        SimpleUpperMapping{char16_t{0x016B}, char16_t{0x016A}},
        SimpleUpperMapping{char16_t{0x016D}, char16_t{0x016C}},
        SimpleUpperMapping{char16_t{0x016F}, char16_t{0x016E}},
        SimpleUpperMapping{char16_t{0x0171}, char16_t{0x0170}},
        SimpleUpperMapping{char16_t{0x0173}, char16_t{0x0172}},
        SimpleUpperMapping{char16_t{0x0175}, char16_t{0x0174}},
        SimpleUpperMapping{char16_t{0x0177}, char16_t{0x0176}},
        SimpleUpperMapping{char16_t{0x017A}, char16_t{0x0179}},
        SimpleUpperMapping{char16_t{0x017C}, char16_t{0x017B}},
        SimpleUpperMapping{char16_t{0x017E}, char16_t{0x017D}},
        SimpleUpperMapping{char16_t{0x017F}, char16_t{0x0053}},
        SimpleUpperMapping{char16_t{0x0180}, char16_t{0x0243}},
        SimpleUpperMapping{char16_t{0x0183}, char16_t{0x0182}},
        SimpleUpperMapping{char16_t{0x0185}, char16_t{0x0184}},
        SimpleUpperMapping{char16_t{0x0188}, char16_t{0x0187}},
        SimpleUpperMapping{char16_t{0x018C}, char16_t{0x018B}},
        SimpleUpperMapping{char16_t{0x0192}, char16_t{0x0191}},
        SimpleUpperMapping{char16_t{0x0195}, char16_t{0x01F6}},
        SimpleUpperMapping{char16_t{0x0199}, char16_t{0x0198}},
        SimpleUpperMapping{char16_t{0x019A}, char16_t{0x023D}},
        SimpleUpperMapping{char16_t{0x019E}, char16_t{0x0220}},
        SimpleUpperMapping{char16_t{0x01A1}, char16_t{0x01A0}},
        SimpleUpperMapping{char16_t{0x01A3}, char16_t{0x01A2}},
        SimpleUpperMapping{char16_t{0x01A5}, char16_t{0x01A4}},
        SimpleUpperMapping{char16_t{0x01A8}, char16_t{0x01A7}},
        SimpleUpperMapping{char16_t{0x01AD}, char16_t{0x01AC}},
        SimpleUpperMapping{char16_t{0x01B0}, char16_t{0x01AF}},
        SimpleUpperMapping{char16_t{0x01B4}, char16_t{0x01B3}},
        SimpleUpperMapping{char16_t{0x01B6}, char16_t{0x01B5}},
        SimpleUpperMapping{char16_t{0x01B9}, char16_t{0x01B8}},
        SimpleUpperMapping{char16_t{0x01BD}, char16_t{0x01BC}},
        SimpleUpperMapping{char16_t{0x01BF}, char16_t{0x01F7}},
        SimpleUpperMapping{char16_t{0x01C5}, char16_t{0x01C4}},
        SimpleUpperMapping{char16_t{0x01C6}, char16_t{0x01C4}},
        SimpleUpperMapping{char16_t{0x01C8}, char16_t{0x01C7}},
        SimpleUpperMapping{char16_t{0x01C9}, char16_t{0x01C7}},
        SimpleUpperMapping{char16_t{0x01CB}, char16_t{0x01CA}},
        SimpleUpperMapping{char16_t{0x01CC}, char16_t{0x01CA}},
        SimpleUpperMapping{char16_t{0x01CE}, char16_t{0x01CD}},
        SimpleUpperMapping{char16_t{0x01D0}, char16_t{0x01CF}},
        SimpleUpperMapping{char16_t{0x01D2}, char16_t{0x01D1}},
        SimpleUpperMapping{char16_t{0x01D4}, char16_t{0x01D3}},
        SimpleUpperMapping{char16_t{0x01D6}, char16_t{0x01D5}},
        SimpleUpperMapping{char16_t{0x01D8}, char16_t{0x01D7}},
        SimpleUpperMapping{char16_t{0x01DA}, char16_t{0x01D9}},
        SimpleUpperMapping{char16_t{0x01DC}, char16_t{0x01DB}},
        SimpleUpperMapping{char16_t{0x01DD}, char16_t{0x018E}},
        SimpleUpperMapping{char16_t{0x01DF}, char16_t{0x01DE}},
        SimpleUpperMapping{char16_t{0x01E1}, char16_t{0x01E0}},
        SimpleUpperMapping{char16_t{0x01E3}, char16_t{0x01E2}},
        SimpleUpperMapping{char16_t{0x01E5}, char16_t{0x01E4}},
        SimpleUpperMapping{char16_t{0x01E7}, char16_t{0x01E6}},
        SimpleUpperMapping{char16_t{0x01E9}, char16_t{0x01E8}},
        SimpleUpperMapping{char16_t{0x01EB}, char16_t{0x01EA}},
        SimpleUpperMapping{char16_t{0x01ED}, char16_t{0x01EC}},
        SimpleUpperMapping{char16_t{0x01EF}, char16_t{0x01EE}},
        SimpleUpperMapping{char16_t{0x01F2}, char16_t{0x01F1}},
        SimpleUpperMapping{char16_t{0x01F3}, char16_t{0x01F1}},
        SimpleUpperMapping{char16_t{0x01F5}, char16_t{0x01F4}},
        SimpleUpperMapping{char16_t{0x01F9}, char16_t{0x01F8}},
        SimpleUpperMapping{char16_t{0x01FB}, char16_t{0x01FA}},
        SimpleUpperMapping{char16_t{0x01FD}, char16_t{0x01FC}},
        SimpleUpperMapping{char16_t{0x01FF}, char16_t{0x01FE}},
        SimpleUpperMapping{char16_t{0x0201}, char16_t{0x0200}},
        SimpleUpperMapping{char16_t{0x0203}, char16_t{0x0202}},
        SimpleUpperMapping{char16_t{0x0205}, char16_t{0x0204}},
        SimpleUpperMapping{char16_t{0x0207}, char16_t{0x0206}},
        SimpleUpperMapping{char16_t{0x0209}, char16_t{0x0208}},
        SimpleUpperMapping{char16_t{0x020B}, char16_t{0x020A}},
        SimpleUpperMapping{char16_t{0x020D}, char16_t{0x020C}},
        SimpleUpperMapping{char16_t{0x020F}, char16_t{0x020E}},
        SimpleUpperMapping{char16_t{0x0211}, char16_t{0x0210}},
        SimpleUpperMapping{char16_t{0x0213}, char16_t{0x0212}},
        SimpleUpperMapping{char16_t{0x0215}, char16_t{0x0214}},
        SimpleUpperMapping{char16_t{0x0217}, char16_t{0x0216}},
        SimpleUpperMapping{char16_t{0x0219}, char16_t{0x0218}},
        SimpleUpperMapping{char16_t{0x021B}, char16_t{0x021A}},
        SimpleUpperMapping{char16_t{0x021D}, char16_t{0x021C}},
        SimpleUpperMapping{char16_t{0x021F}, char16_t{0x021E}},
        SimpleUpperMapping{char16_t{0x0223}, char16_t{0x0222}},
        SimpleUpperMapping{char16_t{0x0225}, char16_t{0x0224}},
        SimpleUpperMapping{char16_t{0x0227}, char16_t{0x0226}},
        SimpleUpperMapping{char16_t{0x0229}, char16_t{0x0228}},
        SimpleUpperMapping{char16_t{0x022B}, char16_t{0x022A}},
        SimpleUpperMapping{char16_t{0x022D}, char16_t{0x022C}},
        SimpleUpperMapping{char16_t{0x022F}, char16_t{0x022E}},
        SimpleUpperMapping{char16_t{0x0231}, char16_t{0x0230}},
        SimpleUpperMapping{char16_t{0x0233}, char16_t{0x0232}},
        SimpleUpperMapping{char16_t{0x023C}, char16_t{0x023B}},
        SimpleUpperMapping{char16_t{0x023F}, char16_t{0x2C7E}},
        SimpleUpperMapping{char16_t{0x0240}, char16_t{0x2C7F}},
        SimpleUpperMapping{char16_t{0x0242}, char16_t{0x0241}},
        SimpleUpperMapping{char16_t{0x0247}, char16_t{0x0246}},
        SimpleUpperMapping{char16_t{0x0249}, char16_t{0x0248}},
        SimpleUpperMapping{char16_t{0x024B}, char16_t{0x024A}},
        SimpleUpperMapping{char16_t{0x024D}, char16_t{0x024C}},
        SimpleUpperMapping{char16_t{0x024F}, char16_t{0x024E}},
        SimpleUpperMapping{char16_t{0x0250}, char16_t{0x2C6F}},
        SimpleUpperMapping{char16_t{0x0251}, char16_t{0x2C6D}},
        SimpleUpperMapping{char16_t{0x0252}, char16_t{0x2C70}},
        SimpleUpperMapping{char16_t{0x0253}, char16_t{0x0181}},
        SimpleUpperMapping{char16_t{0x0254}, char16_t{0x0186}},
        SimpleUpperMapping{char16_t{0x0256}, char16_t{0x0189}},
        SimpleUpperMapping{char16_t{0x0257}, char16_t{0x018A}},
        SimpleUpperMapping{char16_t{0x0259}, char16_t{0x018F}},
        SimpleUpperMapping{char16_t{0x025B}, char16_t{0x0190}},
        SimpleUpperMapping{char16_t{0x025C}, char16_t{0xA7AB}},
        SimpleUpperMapping{char16_t{0x0260}, char16_t{0x0193}},
        SimpleUpperMapping{char16_t{0x0261}, char16_t{0xA7AC}},
        SimpleUpperMapping{char16_t{0x0263}, char16_t{0x0194}},
        SimpleUpperMapping{char16_t{0x0265}, char16_t{0xA78D}},
        SimpleUpperMapping{char16_t{0x0266}, char16_t{0xA7AA}},
        SimpleUpperMapping{char16_t{0x0268}, char16_t{0x0197}},
        SimpleUpperMapping{char16_t{0x0269}, char16_t{0x0196}},
        SimpleUpperMapping{char16_t{0x026A}, char16_t{0xA7AE}},
        SimpleUpperMapping{char16_t{0x026B}, char16_t{0x2C62}},
        SimpleUpperMapping{char16_t{0x026C}, char16_t{0xA7AD}},
        SimpleUpperMapping{char16_t{0x026F}, char16_t{0x019C}},
        SimpleUpperMapping{char16_t{0x0271}, char16_t{0x2C6E}},
        SimpleUpperMapping{char16_t{0x0272}, char16_t{0x019D}},
        SimpleUpperMapping{char16_t{0x0275}, char16_t{0x019F}},
        SimpleUpperMapping{char16_t{0x027D}, char16_t{0x2C64}},
        SimpleUpperMapping{char16_t{0x0280}, char16_t{0x01A6}},
        SimpleUpperMapping{char16_t{0x0282}, char16_t{0xA7C5}},
        SimpleUpperMapping{char16_t{0x0283}, char16_t{0x01A9}},
        SimpleUpperMapping{char16_t{0x0287}, char16_t{0xA7B1}},
        SimpleUpperMapping{char16_t{0x0288}, char16_t{0x01AE}},
        SimpleUpperMapping{char16_t{0x0289}, char16_t{0x0244}},
        SimpleUpperMapping{char16_t{0x028A}, char16_t{0x01B1}},
        SimpleUpperMapping{char16_t{0x028B}, char16_t{0x01B2}},
        SimpleUpperMapping{char16_t{0x028C}, char16_t{0x0245}},
        SimpleUpperMapping{char16_t{0x0292}, char16_t{0x01B7}},
        SimpleUpperMapping{char16_t{0x029D}, char16_t{0xA7B2}},
        SimpleUpperMapping{char16_t{0x029E}, char16_t{0xA7B0}},
        SimpleUpperMapping{char16_t{0x0345}, char16_t{0x0399}},
        SimpleUpperMapping{char16_t{0x0371}, char16_t{0x0370}},
        SimpleUpperMapping{char16_t{0x0373}, char16_t{0x0372}},
        SimpleUpperMapping{char16_t{0x0377}, char16_t{0x0376}},
        SimpleUpperMapping{char16_t{0x037B}, char16_t{0x03FD}},
        SimpleUpperMapping{char16_t{0x037C}, char16_t{0x03FE}},
        SimpleUpperMapping{char16_t{0x037D}, char16_t{0x03FF}},
        SimpleUpperMapping{char16_t{0x03AC}, char16_t{0x0386}},
        SimpleUpperMapping{char16_t{0x03AD}, char16_t{0x0388}},
        SimpleUpperMapping{char16_t{0x03AE}, char16_t{0x0389}},
        SimpleUpperMapping{char16_t{0x03AF}, char16_t{0x038A}},
        SimpleUpperMapping{char16_t{0x03B1}, char16_t{0x0391}},
        SimpleUpperMapping{char16_t{0x03B2}, char16_t{0x0392}},
        SimpleUpperMapping{char16_t{0x03B3}, char16_t{0x0393}},
        SimpleUpperMapping{char16_t{0x03B4}, char16_t{0x0394}},
        SimpleUpperMapping{char16_t{0x03B5}, char16_t{0x0395}},
        SimpleUpperMapping{char16_t{0x03B6}, char16_t{0x0396}},
        SimpleUpperMapping{char16_t{0x03B7}, char16_t{0x0397}},
        SimpleUpperMapping{char16_t{0x03B8}, char16_t{0x0398}},
        SimpleUpperMapping{char16_t{0x03B9}, char16_t{0x0399}},
        SimpleUpperMapping{char16_t{0x03BA}, char16_t{0x039A}},
        SimpleUpperMapping{char16_t{0x03BB}, char16_t{0x039B}},
        SimpleUpperMapping{char16_t{0x03BC}, char16_t{0x039C}},
        SimpleUpperMapping{char16_t{0x03BD}, char16_t{0x039D}},
        SimpleUpperMapping{char16_t{0x03BE}, char16_t{0x039E}},
        SimpleUpperMapping{char16_t{0x03BF}, char16_t{0x039F}},
        SimpleUpperMapping{char16_t{0x03C0}, char16_t{0x03A0}},
        SimpleUpperMapping{char16_t{0x03C1}, char16_t{0x03A1}},
        SimpleUpperMapping{char16_t{0x03C2}, char16_t{0x03A3}},
        SimpleUpperMapping{char16_t{0x03C3}, char16_t{0x03A3}},
        SimpleUpperMapping{char16_t{0x03C4}, char16_t{0x03A4}},
        SimpleUpperMapping{char16_t{0x03C5}, char16_t{0x03A5}},
        SimpleUpperMapping{char16_t{0x03C6}, char16_t{0x03A6}},
        SimpleUpperMapping{char16_t{0x03C7}, char16_t{0x03A7}},
        SimpleUpperMapping{char16_t{0x03C8}, char16_t{0x03A8}},
        SimpleUpperMapping{char16_t{0x03C9}, char16_t{0x03A9}},
        SimpleUpperMapping{char16_t{0x03CA}, char16_t{0x03AA}},
        SimpleUpperMapping{char16_t{0x03CB}, char16_t{0x03AB}},
        SimpleUpperMapping{char16_t{0x03CC}, char16_t{0x038C}},
        SimpleUpperMapping{char16_t{0x03CD}, char16_t{0x038E}},
        SimpleUpperMapping{char16_t{0x03CE}, char16_t{0x038F}},
        SimpleUpperMapping{char16_t{0x03D0}, char16_t{0x0392}},
        SimpleUpperMapping{char16_t{0x03D1}, char16_t{0x0398}},
        SimpleUpperMapping{char16_t{0x03D5}, char16_t{0x03A6}},
        SimpleUpperMapping{char16_t{0x03D6}, char16_t{0x03A0}},
        SimpleUpperMapping{char16_t{0x03D7}, char16_t{0x03CF}},
        SimpleUpperMapping{char16_t{0x03D9}, char16_t{0x03D8}},
        SimpleUpperMapping{char16_t{0x03DB}, char16_t{0x03DA}},
        SimpleUpperMapping{char16_t{0x03DD}, char16_t{0x03DC}},
        SimpleUpperMapping{char16_t{0x03DF}, char16_t{0x03DE}},
        SimpleUpperMapping{char16_t{0x03E1}, char16_t{0x03E0}},
        SimpleUpperMapping{char16_t{0x03E3}, char16_t{0x03E2}},
        SimpleUpperMapping{char16_t{0x03E5}, char16_t{0x03E4}},
        SimpleUpperMapping{char16_t{0x03E7}, char16_t{0x03E6}},
        SimpleUpperMapping{char16_t{0x03E9}, char16_t{0x03E8}},
        SimpleUpperMapping{char16_t{0x03EB}, char16_t{0x03EA}},
        SimpleUpperMapping{char16_t{0x03ED}, char16_t{0x03EC}},
        SimpleUpperMapping{char16_t{0x03EF}, char16_t{0x03EE}},
        SimpleUpperMapping{char16_t{0x03F0}, char16_t{0x039A}},
        SimpleUpperMapping{char16_t{0x03F1}, char16_t{0x03A1}},
        SimpleUpperMapping{char16_t{0x03F2}, char16_t{0x03F9}},
        SimpleUpperMapping{char16_t{0x03F3}, char16_t{0x037F}},
        SimpleUpperMapping{char16_t{0x03F5}, char16_t{0x0395}},
        SimpleUpperMapping{char16_t{0x03F8}, char16_t{0x03F7}},
        SimpleUpperMapping{char16_t{0x03FB}, char16_t{0x03FA}},
        SimpleUpperMapping{char16_t{0x0430}, char16_t{0x0410}},
        SimpleUpperMapping{char16_t{0x0431}, char16_t{0x0411}},
        SimpleUpperMapping{char16_t{0x0432}, char16_t{0x0412}},
        SimpleUpperMapping{char16_t{0x0433}, char16_t{0x0413}},
        SimpleUpperMapping{char16_t{0x0434}, char16_t{0x0414}},
        SimpleUpperMapping{char16_t{0x0435}, char16_t{0x0415}},
        SimpleUpperMapping{char16_t{0x0436}, char16_t{0x0416}},
        SimpleUpperMapping{char16_t{0x0437}, char16_t{0x0417}},
        SimpleUpperMapping{char16_t{0x0438}, char16_t{0x0418}},
        SimpleUpperMapping{char16_t{0x0439}, char16_t{0x0419}},
        SimpleUpperMapping{char16_t{0x043A}, char16_t{0x041A}},
        SimpleUpperMapping{char16_t{0x043B}, char16_t{0x041B}},
        SimpleUpperMapping{char16_t{0x043C}, char16_t{0x041C}},
        SimpleUpperMapping{char16_t{0x043D}, char16_t{0x041D}},
        SimpleUpperMapping{char16_t{0x043E}, char16_t{0x041E}},
        SimpleUpperMapping{char16_t{0x043F}, char16_t{0x041F}},
        SimpleUpperMapping{char16_t{0x0440}, char16_t{0x0420}},
        SimpleUpperMapping{char16_t{0x0441}, char16_t{0x0421}},
        SimpleUpperMapping{char16_t{0x0442}, char16_t{0x0422}},
        SimpleUpperMapping{char16_t{0x0443}, char16_t{0x0423}},
        SimpleUpperMapping{char16_t{0x0444}, char16_t{0x0424}},
        SimpleUpperMapping{char16_t{0x0445}, char16_t{0x0425}},
        SimpleUpperMapping{char16_t{0x0446}, char16_t{0x0426}},
        SimpleUpperMapping{char16_t{0x0447}, char16_t{0x0427}},
        SimpleUpperMapping{char16_t{0x0448}, char16_t{0x0428}},
        SimpleUpperMapping{char16_t{0x0449}, char16_t{0x0429}},
        SimpleUpperMapping{char16_t{0x044A}, char16_t{0x042A}},
        SimpleUpperMapping{char16_t{0x044B}, char16_t{0x042B}},
        SimpleUpperMapping{char16_t{0x044C}, char16_t{0x042C}},
        SimpleUpperMapping{char16_t{0x044D}, char16_t{0x042D}},
        SimpleUpperMapping{char16_t{0x044E}, char16_t{0x042E}},
        SimpleUpperMapping{char16_t{0x044F}, char16_t{0x042F}},
        SimpleUpperMapping{char16_t{0x0450}, char16_t{0x0400}},
        SimpleUpperMapping{char16_t{0x0451}, char16_t{0x0401}},
        SimpleUpperMapping{char16_t{0x0452}, char16_t{0x0402}},
        SimpleUpperMapping{char16_t{0x0453}, char16_t{0x0403}},
        SimpleUpperMapping{char16_t{0x0454}, char16_t{0x0404}},
        SimpleUpperMapping{char16_t{0x0455}, char16_t{0x0405}},
        SimpleUpperMapping{char16_t{0x0456}, char16_t{0x0406}},
        SimpleUpperMapping{char16_t{0x0457}, char16_t{0x0407}},
        SimpleUpperMapping{char16_t{0x0458}, char16_t{0x0408}},
        SimpleUpperMapping{char16_t{0x0459}, char16_t{0x0409}},
        SimpleUpperMapping{char16_t{0x045A}, char16_t{0x040A}},
        SimpleUpperMapping{char16_t{0x045B}, char16_t{0x040B}},
        SimpleUpperMapping{char16_t{0x045C}, char16_t{0x040C}},
        SimpleUpperMapping{char16_t{0x045D}, char16_t{0x040D}},
        SimpleUpperMapping{char16_t{0x045E}, char16_t{0x040E}},
        SimpleUpperMapping{char16_t{0x045F}, char16_t{0x040F}},
        SimpleUpperMapping{char16_t{0x0461}, char16_t{0x0460}},
        SimpleUpperMapping{char16_t{0x0463}, char16_t{0x0462}},
        SimpleUpperMapping{char16_t{0x0465}, char16_t{0x0464}},
        SimpleUpperMapping{char16_t{0x0467}, char16_t{0x0466}},
        SimpleUpperMapping{char16_t{0x0469}, char16_t{0x0468}},
        SimpleUpperMapping{char16_t{0x046B}, char16_t{0x046A}},
        SimpleUpperMapping{char16_t{0x046D}, char16_t{0x046C}},
        SimpleUpperMapping{char16_t{0x046F}, char16_t{0x046E}},
        SimpleUpperMapping{char16_t{0x0471}, char16_t{0x0470}},
        SimpleUpperMapping{char16_t{0x0473}, char16_t{0x0472}},
        SimpleUpperMapping{char16_t{0x0475}, char16_t{0x0474}},
        SimpleUpperMapping{char16_t{0x0477}, char16_t{0x0476}},
        SimpleUpperMapping{char16_t{0x0479}, char16_t{0x0478}},
        SimpleUpperMapping{char16_t{0x047B}, char16_t{0x047A}},
        SimpleUpperMapping{char16_t{0x047D}, char16_t{0x047C}},
        SimpleUpperMapping{char16_t{0x047F}, char16_t{0x047E}},
        SimpleUpperMapping{char16_t{0x0481}, char16_t{0x0480}},
        SimpleUpperMapping{char16_t{0x048B}, char16_t{0x048A}},
        SimpleUpperMapping{char16_t{0x048D}, char16_t{0x048C}},
        SimpleUpperMapping{char16_t{0x048F}, char16_t{0x048E}},
        SimpleUpperMapping{char16_t{0x0491}, char16_t{0x0490}},
        SimpleUpperMapping{char16_t{0x0493}, char16_t{0x0492}},
        SimpleUpperMapping{char16_t{0x0495}, char16_t{0x0494}},
        SimpleUpperMapping{char16_t{0x0497}, char16_t{0x0496}},
        SimpleUpperMapping{char16_t{0x0499}, char16_t{0x0498}},
        SimpleUpperMapping{char16_t{0x049B}, char16_t{0x049A}},
        SimpleUpperMapping{char16_t{0x049D}, char16_t{0x049C}},
        SimpleUpperMapping{char16_t{0x049F}, char16_t{0x049E}},
        SimpleUpperMapping{char16_t{0x04A1}, char16_t{0x04A0}},
        SimpleUpperMapping{char16_t{0x04A3}, char16_t{0x04A2}},
        SimpleUpperMapping{char16_t{0x04A5}, char16_t{0x04A4}},
        SimpleUpperMapping{char16_t{0x04A7}, char16_t{0x04A6}},
        SimpleUpperMapping{char16_t{0x04A9}, char16_t{0x04A8}},
        SimpleUpperMapping{char16_t{0x04AB}, char16_t{0x04AA}},
        SimpleUpperMapping{char16_t{0x04AD}, char16_t{0x04AC}},
        SimpleUpperMapping{char16_t{0x04AF}, char16_t{0x04AE}},
        SimpleUpperMapping{char16_t{0x04B1}, char16_t{0x04B0}},
        SimpleUpperMapping{char16_t{0x04B3}, char16_t{0x04B2}},
        SimpleUpperMapping{char16_t{0x04B5}, char16_t{0x04B4}},
        SimpleUpperMapping{char16_t{0x04B7}, char16_t{0x04B6}},
        SimpleUpperMapping{char16_t{0x04B9}, char16_t{0x04B8}},
        SimpleUpperMapping{char16_t{0x04BB}, char16_t{0x04BA}},
        SimpleUpperMapping{char16_t{0x04BD}, char16_t{0x04BC}},
        SimpleUpperMapping{char16_t{0x04BF}, char16_t{0x04BE}},
        SimpleUpperMapping{char16_t{0x04C2}, char16_t{0x04C1}},
        SimpleUpperMapping{char16_t{0x04C4}, char16_t{0x04C3}},
        SimpleUpperMapping{char16_t{0x04C6}, char16_t{0x04C5}},
        SimpleUpperMapping{char16_t{0x04C8}, char16_t{0x04C7}},
        SimpleUpperMapping{char16_t{0x04CA}, char16_t{0x04C9}},
        SimpleUpperMapping{char16_t{0x04CC}, char16_t{0x04CB}},
        SimpleUpperMapping{char16_t{0x04CE}, char16_t{0x04CD}},
        SimpleUpperMapping{char16_t{0x04CF}, char16_t{0x04C0}},
        SimpleUpperMapping{char16_t{0x04D1}, char16_t{0x04D0}},
        SimpleUpperMapping{char16_t{0x04D3}, char16_t{0x04D2}},
        SimpleUpperMapping{char16_t{0x04D5}, char16_t{0x04D4}},
        SimpleUpperMapping{char16_t{0x04D7}, char16_t{0x04D6}},
        SimpleUpperMapping{char16_t{0x04D9}, char16_t{0x04D8}},
        SimpleUpperMapping{char16_t{0x04DB}, char16_t{0x04DA}},
        SimpleUpperMapping{char16_t{0x04DD}, char16_t{0x04DC}},
        SimpleUpperMapping{char16_t{0x04DF}, char16_t{0x04DE}},
        SimpleUpperMapping{char16_t{0x04E1}, char16_t{0x04E0}},
        SimpleUpperMapping{char16_t{0x04E3}, char16_t{0x04E2}},
        SimpleUpperMapping{char16_t{0x04E5}, char16_t{0x04E4}},
        SimpleUpperMapping{char16_t{0x04E7}, char16_t{0x04E6}},
        SimpleUpperMapping{char16_t{0x04E9}, char16_t{0x04E8}},
        SimpleUpperMapping{char16_t{0x04EB}, char16_t{0x04EA}},
        SimpleUpperMapping{char16_t{0x04ED}, char16_t{0x04EC}},
        SimpleUpperMapping{char16_t{0x04EF}, char16_t{0x04EE}},
        SimpleUpperMapping{char16_t{0x04F1}, char16_t{0x04F0}},
        SimpleUpperMapping{char16_t{0x04F3}, char16_t{0x04F2}},
        SimpleUpperMapping{char16_t{0x04F5}, char16_t{0x04F4}},
        SimpleUpperMapping{char16_t{0x04F7}, char16_t{0x04F6}},
        SimpleUpperMapping{char16_t{0x04F9}, char16_t{0x04F8}},
        SimpleUpperMapping{char16_t{0x04FB}, char16_t{0x04FA}},
        SimpleUpperMapping{char16_t{0x04FD}, char16_t{0x04FC}},
        SimpleUpperMapping{char16_t{0x04FF}, char16_t{0x04FE}},
        SimpleUpperMapping{char16_t{0x0501}, char16_t{0x0500}},
        SimpleUpperMapping{char16_t{0x0503}, char16_t{0x0502}},
        SimpleUpperMapping{char16_t{0x0505}, char16_t{0x0504}},
        SimpleUpperMapping{char16_t{0x0507}, char16_t{0x0506}},
        SimpleUpperMapping{char16_t{0x0509}, char16_t{0x0508}},
        SimpleUpperMapping{char16_t{0x050B}, char16_t{0x050A}},
        SimpleUpperMapping{char16_t{0x050D}, char16_t{0x050C}},
        SimpleUpperMapping{char16_t{0x050F}, char16_t{0x050E}},
        SimpleUpperMapping{char16_t{0x0511}, char16_t{0x0510}},
        SimpleUpperMapping{char16_t{0x0513}, char16_t{0x0512}},
        SimpleUpperMapping{char16_t{0x0515}, char16_t{0x0514}},
        SimpleUpperMapping{char16_t{0x0517}, char16_t{0x0516}},
        SimpleUpperMapping{char16_t{0x0519}, char16_t{0x0518}},
        SimpleUpperMapping{char16_t{0x051B}, char16_t{0x051A}},
        SimpleUpperMapping{char16_t{0x051D}, char16_t{0x051C}},
        SimpleUpperMapping{char16_t{0x051F}, char16_t{0x051E}},
        SimpleUpperMapping{char16_t{0x0521}, char16_t{0x0520}},
        SimpleUpperMapping{char16_t{0x0523}, char16_t{0x0522}},
        SimpleUpperMapping{char16_t{0x0525}, char16_t{0x0524}},
        SimpleUpperMapping{char16_t{0x0527}, char16_t{0x0526}},
        SimpleUpperMapping{char16_t{0x0529}, char16_t{0x0528}},
        SimpleUpperMapping{char16_t{0x052B}, char16_t{0x052A}},
        SimpleUpperMapping{char16_t{0x052D}, char16_t{0x052C}},
        SimpleUpperMapping{char16_t{0x052F}, char16_t{0x052E}},
        SimpleUpperMapping{char16_t{0x0561}, char16_t{0x0531}},
        SimpleUpperMapping{char16_t{0x0562}, char16_t{0x0532}},
        SimpleUpperMapping{char16_t{0x0563}, char16_t{0x0533}},
        SimpleUpperMapping{char16_t{0x0564}, char16_t{0x0534}},
        SimpleUpperMapping{char16_t{0x0565}, char16_t{0x0535}},
        SimpleUpperMapping{char16_t{0x0566}, char16_t{0x0536}},
        SimpleUpperMapping{char16_t{0x0567}, char16_t{0x0537}},
        SimpleUpperMapping{char16_t{0x0568}, char16_t{0x0538}},
        SimpleUpperMapping{char16_t{0x0569}, char16_t{0x0539}},
        SimpleUpperMapping{char16_t{0x056A}, char16_t{0x053A}},
        SimpleUpperMapping{char16_t{0x056B}, char16_t{0x053B}},
        SimpleUpperMapping{char16_t{0x056C}, char16_t{0x053C}},
        SimpleUpperMapping{char16_t{0x056D}, char16_t{0x053D}},
        SimpleUpperMapping{char16_t{0x056E}, char16_t{0x053E}},
        SimpleUpperMapping{char16_t{0x056F}, char16_t{0x053F}},
        SimpleUpperMapping{char16_t{0x0570}, char16_t{0x0540}},
        SimpleUpperMapping{char16_t{0x0571}, char16_t{0x0541}},
        SimpleUpperMapping{char16_t{0x0572}, char16_t{0x0542}},
        SimpleUpperMapping{char16_t{0x0573}, char16_t{0x0543}},
        SimpleUpperMapping{char16_t{0x0574}, char16_t{0x0544}},
        SimpleUpperMapping{char16_t{0x0575}, char16_t{0x0545}},
        SimpleUpperMapping{char16_t{0x0576}, char16_t{0x0546}},
        SimpleUpperMapping{char16_t{0x0577}, char16_t{0x0547}},
        SimpleUpperMapping{char16_t{0x0578}, char16_t{0x0548}},
        SimpleUpperMapping{char16_t{0x0579}, char16_t{0x0549}},
        SimpleUpperMapping{char16_t{0x057A}, char16_t{0x054A}},
        SimpleUpperMapping{char16_t{0x057B}, char16_t{0x054B}},
        SimpleUpperMapping{char16_t{0x057C}, char16_t{0x054C}},
        SimpleUpperMapping{char16_t{0x057D}, char16_t{0x054D}},
        SimpleUpperMapping{char16_t{0x057E}, char16_t{0x054E}},
        SimpleUpperMapping{char16_t{0x057F}, char16_t{0x054F}},
        SimpleUpperMapping{char16_t{0x0580}, char16_t{0x0550}},
        SimpleUpperMapping{char16_t{0x0581}, char16_t{0x0551}},
        SimpleUpperMapping{char16_t{0x0582}, char16_t{0x0552}},
        SimpleUpperMapping{char16_t{0x0583}, char16_t{0x0553}},
        SimpleUpperMapping{char16_t{0x0584}, char16_t{0x0554}},
        SimpleUpperMapping{char16_t{0x0585}, char16_t{0x0555}},
        SimpleUpperMapping{char16_t{0x0586}, char16_t{0x0556}},
        SimpleUpperMapping{char16_t{0x10D0}, char16_t{0x1C90}},
        SimpleUpperMapping{char16_t{0x10D1}, char16_t{0x1C91}},
        SimpleUpperMapping{char16_t{0x10D2}, char16_t{0x1C92}},
        SimpleUpperMapping{char16_t{0x10D3}, char16_t{0x1C93}},
        SimpleUpperMapping{char16_t{0x10D4}, char16_t{0x1C94}},
        SimpleUpperMapping{char16_t{0x10D5}, char16_t{0x1C95}},
        SimpleUpperMapping{char16_t{0x10D6}, char16_t{0x1C96}},
        SimpleUpperMapping{char16_t{0x10D7}, char16_t{0x1C97}},
        SimpleUpperMapping{char16_t{0x10D8}, char16_t{0x1C98}},
        SimpleUpperMapping{char16_t{0x10D9}, char16_t{0x1C99}},
        SimpleUpperMapping{char16_t{0x10DA}, char16_t{0x1C9A}},
        SimpleUpperMapping{char16_t{0x10DB}, char16_t{0x1C9B}},
        SimpleUpperMapping{char16_t{0x10DC}, char16_t{0x1C9C}},
        SimpleUpperMapping{char16_t{0x10DD}, char16_t{0x1C9D}},
        SimpleUpperMapping{char16_t{0x10DE}, char16_t{0x1C9E}},
        SimpleUpperMapping{char16_t{0x10DF}, char16_t{0x1C9F}},
        SimpleUpperMapping{char16_t{0x10E0}, char16_t{0x1CA0}},
        SimpleUpperMapping{char16_t{0x10E1}, char16_t{0x1CA1}},
        SimpleUpperMapping{char16_t{0x10E2}, char16_t{0x1CA2}},
        SimpleUpperMapping{char16_t{0x10E3}, char16_t{0x1CA3}},
        SimpleUpperMapping{char16_t{0x10E4}, char16_t{0x1CA4}},
        SimpleUpperMapping{char16_t{0x10E5}, char16_t{0x1CA5}},
        SimpleUpperMapping{char16_t{0x10E6}, char16_t{0x1CA6}},
        SimpleUpperMapping{char16_t{0x10E7}, char16_t{0x1CA7}},
        SimpleUpperMapping{char16_t{0x10E8}, char16_t{0x1CA8}},
        SimpleUpperMapping{char16_t{0x10E9}, char16_t{0x1CA9}},
        SimpleUpperMapping{char16_t{0x10EA}, char16_t{0x1CAA}},
        SimpleUpperMapping{char16_t{0x10EB}, char16_t{0x1CAB}},
        SimpleUpperMapping{char16_t{0x10EC}, char16_t{0x1CAC}},
        SimpleUpperMapping{char16_t{0x10ED}, char16_t{0x1CAD}},
        SimpleUpperMapping{char16_t{0x10EE}, char16_t{0x1CAE}},
        SimpleUpperMapping{char16_t{0x10EF}, char16_t{0x1CAF}},
        SimpleUpperMapping{char16_t{0x10F0}, char16_t{0x1CB0}},
        SimpleUpperMapping{char16_t{0x10F1}, char16_t{0x1CB1}},
        SimpleUpperMapping{char16_t{0x10F2}, char16_t{0x1CB2}},
        SimpleUpperMapping{char16_t{0x10F3}, char16_t{0x1CB3}},
        SimpleUpperMapping{char16_t{0x10F4}, char16_t{0x1CB4}},
        SimpleUpperMapping{char16_t{0x10F5}, char16_t{0x1CB5}},
        SimpleUpperMapping{char16_t{0x10F6}, char16_t{0x1CB6}},
        SimpleUpperMapping{char16_t{0x10F7}, char16_t{0x1CB7}},
        SimpleUpperMapping{char16_t{0x10F8}, char16_t{0x1CB8}},
        SimpleUpperMapping{char16_t{0x10F9}, char16_t{0x1CB9}},
        SimpleUpperMapping{char16_t{0x10FA}, char16_t{0x1CBA}},
        SimpleUpperMapping{char16_t{0x10FD}, char16_t{0x1CBD}},
        SimpleUpperMapping{char16_t{0x10FE}, char16_t{0x1CBE}},
        SimpleUpperMapping{char16_t{0x10FF}, char16_t{0x1CBF}},
        SimpleUpperMapping{char16_t{0x13F8}, char16_t{0x13F0}},
        SimpleUpperMapping{char16_t{0x13F9}, char16_t{0x13F1}},
        SimpleUpperMapping{char16_t{0x13FA}, char16_t{0x13F2}},
        SimpleUpperMapping{char16_t{0x13FB}, char16_t{0x13F3}},
        SimpleUpperMapping{char16_t{0x13FC}, char16_t{0x13F4}},
        SimpleUpperMapping{char16_t{0x13FD}, char16_t{0x13F5}},
        SimpleUpperMapping{char16_t{0x1C80}, char16_t{0x0412}},
        SimpleUpperMapping{char16_t{0x1C81}, char16_t{0x0414}},
        SimpleUpperMapping{char16_t{0x1C82}, char16_t{0x041E}},
        SimpleUpperMapping{char16_t{0x1C83}, char16_t{0x0421}},
        SimpleUpperMapping{char16_t{0x1C84}, char16_t{0x0422}},
        SimpleUpperMapping{char16_t{0x1C85}, char16_t{0x0422}},
        SimpleUpperMapping{char16_t{0x1C86}, char16_t{0x042A}},
        SimpleUpperMapping{char16_t{0x1C87}, char16_t{0x0462}},
        SimpleUpperMapping{char16_t{0x1C88}, char16_t{0xA64A}},
        SimpleUpperMapping{char16_t{0x1D79}, char16_t{0xA77D}},
        SimpleUpperMapping{char16_t{0x1D7D}, char16_t{0x2C63}},
        SimpleUpperMapping{char16_t{0x1D8E}, char16_t{0xA7C6}},
        SimpleUpperMapping{char16_t{0x1E01}, char16_t{0x1E00}},
        SimpleUpperMapping{char16_t{0x1E03}, char16_t{0x1E02}},
        SimpleUpperMapping{char16_t{0x1E05}, char16_t{0x1E04}},
        SimpleUpperMapping{char16_t{0x1E07}, char16_t{0x1E06}},
        SimpleUpperMapping{char16_t{0x1E09}, char16_t{0x1E08}},
        SimpleUpperMapping{char16_t{0x1E0B}, char16_t{0x1E0A}},
        SimpleUpperMapping{char16_t{0x1E0D}, char16_t{0x1E0C}},
        SimpleUpperMapping{char16_t{0x1E0F}, char16_t{0x1E0E}},
        SimpleUpperMapping{char16_t{0x1E11}, char16_t{0x1E10}},
        SimpleUpperMapping{char16_t{0x1E13}, char16_t{0x1E12}},
        SimpleUpperMapping{char16_t{0x1E15}, char16_t{0x1E14}},
        SimpleUpperMapping{char16_t{0x1E17}, char16_t{0x1E16}},
        SimpleUpperMapping{char16_t{0x1E19}, char16_t{0x1E18}},
        SimpleUpperMapping{char16_t{0x1E1B}, char16_t{0x1E1A}},
        SimpleUpperMapping{char16_t{0x1E1D}, char16_t{0x1E1C}},
        SimpleUpperMapping{char16_t{0x1E1F}, char16_t{0x1E1E}},
        SimpleUpperMapping{char16_t{0x1E21}, char16_t{0x1E20}},
        SimpleUpperMapping{char16_t{0x1E23}, char16_t{0x1E22}},
        SimpleUpperMapping{char16_t{0x1E25}, char16_t{0x1E24}},
        SimpleUpperMapping{char16_t{0x1E27}, char16_t{0x1E26}},
        SimpleUpperMapping{char16_t{0x1E29}, char16_t{0x1E28}},
        SimpleUpperMapping{char16_t{0x1E2B}, char16_t{0x1E2A}},
        SimpleUpperMapping{char16_t{0x1E2D}, char16_t{0x1E2C}},
        SimpleUpperMapping{char16_t{0x1E2F}, char16_t{0x1E2E}},
        SimpleUpperMapping{char16_t{0x1E31}, char16_t{0x1E30}},
        SimpleUpperMapping{char16_t{0x1E33}, char16_t{0x1E32}},
        SimpleUpperMapping{char16_t{0x1E35}, char16_t{0x1E34}},
        SimpleUpperMapping{char16_t{0x1E37}, char16_t{0x1E36}},
        SimpleUpperMapping{char16_t{0x1E39}, char16_t{0x1E38}},
        SimpleUpperMapping{char16_t{0x1E3B}, char16_t{0x1E3A}},
        SimpleUpperMapping{char16_t{0x1E3D}, char16_t{0x1E3C}},
        SimpleUpperMapping{char16_t{0x1E3F}, char16_t{0x1E3E}},
        SimpleUpperMapping{char16_t{0x1E41}, char16_t{0x1E40}},
        SimpleUpperMapping{char16_t{0x1E43}, char16_t{0x1E42}},
        SimpleUpperMapping{char16_t{0x1E45}, char16_t{0x1E44}},
        SimpleUpperMapping{char16_t{0x1E47}, char16_t{0x1E46}},
        SimpleUpperMapping{char16_t{0x1E49}, char16_t{0x1E48}},
        SimpleUpperMapping{char16_t{0x1E4B}, char16_t{0x1E4A}},
        SimpleUpperMapping{char16_t{0x1E4D}, char16_t{0x1E4C}},
        SimpleUpperMapping{char16_t{0x1E4F}, char16_t{0x1E4E}},
        SimpleUpperMapping{char16_t{0x1E51}, char16_t{0x1E50}},
        SimpleUpperMapping{char16_t{0x1E53}, char16_t{0x1E52}},
        SimpleUpperMapping{char16_t{0x1E55}, char16_t{0x1E54}},
        SimpleUpperMapping{char16_t{0x1E57}, char16_t{0x1E56}},
        SimpleUpperMapping{char16_t{0x1E59}, char16_t{0x1E58}},
        SimpleUpperMapping{char16_t{0x1E5B}, char16_t{0x1E5A}},
        SimpleUpperMapping{char16_t{0x1E5D}, char16_t{0x1E5C}},
        SimpleUpperMapping{char16_t{0x1E5F}, char16_t{0x1E5E}},
        SimpleUpperMapping{char16_t{0x1E61}, char16_t{0x1E60}},
        SimpleUpperMapping{char16_t{0x1E63}, char16_t{0x1E62}},
        SimpleUpperMapping{char16_t{0x1E65}, char16_t{0x1E64}},
        SimpleUpperMapping{char16_t{0x1E67}, char16_t{0x1E66}},
        SimpleUpperMapping{char16_t{0x1E69}, char16_t{0x1E68}},
        SimpleUpperMapping{char16_t{0x1E6B}, char16_t{0x1E6A}},
        SimpleUpperMapping{char16_t{0x1E6D}, char16_t{0x1E6C}},
        SimpleUpperMapping{char16_t{0x1E6F}, char16_t{0x1E6E}},
        SimpleUpperMapping{char16_t{0x1E71}, char16_t{0x1E70}},
        SimpleUpperMapping{char16_t{0x1E73}, char16_t{0x1E72}},
        SimpleUpperMapping{char16_t{0x1E75}, char16_t{0x1E74}},
        SimpleUpperMapping{char16_t{0x1E77}, char16_t{0x1E76}},
        SimpleUpperMapping{char16_t{0x1E79}, char16_t{0x1E78}},
        SimpleUpperMapping{char16_t{0x1E7B}, char16_t{0x1E7A}},
        SimpleUpperMapping{char16_t{0x1E7D}, char16_t{0x1E7C}},
        SimpleUpperMapping{char16_t{0x1E7F}, char16_t{0x1E7E}},
        SimpleUpperMapping{char16_t{0x1E81}, char16_t{0x1E80}},
        SimpleUpperMapping{char16_t{0x1E83}, char16_t{0x1E82}},
        SimpleUpperMapping{char16_t{0x1E85}, char16_t{0x1E84}},
        SimpleUpperMapping{char16_t{0x1E87}, char16_t{0x1E86}},
        SimpleUpperMapping{char16_t{0x1E89}, char16_t{0x1E88}},
        SimpleUpperMapping{char16_t{0x1E8B}, char16_t{0x1E8A}},
        SimpleUpperMapping{char16_t{0x1E8D}, char16_t{0x1E8C}},
        SimpleUpperMapping{char16_t{0x1E8F}, char16_t{0x1E8E}},
        SimpleUpperMapping{char16_t{0x1E91}, char16_t{0x1E90}},
        SimpleUpperMapping{char16_t{0x1E93}, char16_t{0x1E92}},
        SimpleUpperMapping{char16_t{0x1E95}, char16_t{0x1E94}},
        SimpleUpperMapping{char16_t{0x1E9B}, char16_t{0x1E60}},
        SimpleUpperMapping{char16_t{0x1EA1}, char16_t{0x1EA0}},
        SimpleUpperMapping{char16_t{0x1EA3}, char16_t{0x1EA2}},
        SimpleUpperMapping{char16_t{0x1EA5}, char16_t{0x1EA4}},
        SimpleUpperMapping{char16_t{0x1EA7}, char16_t{0x1EA6}},
        SimpleUpperMapping{char16_t{0x1EA9}, char16_t{0x1EA8}},
        SimpleUpperMapping{char16_t{0x1EAB}, char16_t{0x1EAA}},
        SimpleUpperMapping{char16_t{0x1EAD}, char16_t{0x1EAC}},
        SimpleUpperMapping{char16_t{0x1EAF}, char16_t{0x1EAE}},
        SimpleUpperMapping{char16_t{0x1EB1}, char16_t{0x1EB0}},
        SimpleUpperMapping{char16_t{0x1EB3}, char16_t{0x1EB2}},
        SimpleUpperMapping{char16_t{0x1EB5}, char16_t{0x1EB4}},
        SimpleUpperMapping{char16_t{0x1EB7}, char16_t{0x1EB6}},
        SimpleUpperMapping{char16_t{0x1EB9}, char16_t{0x1EB8}},
        SimpleUpperMapping{char16_t{0x1EBB}, char16_t{0x1EBA}},
        SimpleUpperMapping{char16_t{0x1EBD}, char16_t{0x1EBC}},
        SimpleUpperMapping{char16_t{0x1EBF}, char16_t{0x1EBE}},
        SimpleUpperMapping{char16_t{0x1EC1}, char16_t{0x1EC0}},
        SimpleUpperMapping{char16_t{0x1EC3}, char16_t{0x1EC2}},
        SimpleUpperMapping{char16_t{0x1EC5}, char16_t{0x1EC4}},
        SimpleUpperMapping{char16_t{0x1EC7}, char16_t{0x1EC6}},
        SimpleUpperMapping{char16_t{0x1EC9}, char16_t{0x1EC8}},
        SimpleUpperMapping{char16_t{0x1ECB}, char16_t{0x1ECA}},
        SimpleUpperMapping{char16_t{0x1ECD}, char16_t{0x1ECC}},
        SimpleUpperMapping{char16_t{0x1ECF}, char16_t{0x1ECE}},
        SimpleUpperMapping{char16_t{0x1ED1}, char16_t{0x1ED0}},
        SimpleUpperMapping{char16_t{0x1ED3}, char16_t{0x1ED2}},
        SimpleUpperMapping{char16_t{0x1ED5}, char16_t{0x1ED4}},
        SimpleUpperMapping{char16_t{0x1ED7}, char16_t{0x1ED6}},
        SimpleUpperMapping{char16_t{0x1ED9}, char16_t{0x1ED8}},
        SimpleUpperMapping{char16_t{0x1EDB}, char16_t{0x1EDA}},
        SimpleUpperMapping{char16_t{0x1EDD}, char16_t{0x1EDC}},
        SimpleUpperMapping{char16_t{0x1EDF}, char16_t{0x1EDE}},
        SimpleUpperMapping{char16_t{0x1EE1}, char16_t{0x1EE0}},
        SimpleUpperMapping{char16_t{0x1EE3}, char16_t{0x1EE2}},
        SimpleUpperMapping{char16_t{0x1EE5}, char16_t{0x1EE4}},
        SimpleUpperMapping{char16_t{0x1EE7}, char16_t{0x1EE6}},
        SimpleUpperMapping{char16_t{0x1EE9}, char16_t{0x1EE8}},
        SimpleUpperMapping{char16_t{0x1EEB}, char16_t{0x1EEA}},
        SimpleUpperMapping{char16_t{0x1EED}, char16_t{0x1EEC}},
        SimpleUpperMapping{char16_t{0x1EEF}, char16_t{0x1EEE}},
        SimpleUpperMapping{char16_t{0x1EF1}, char16_t{0x1EF0}},
        SimpleUpperMapping{char16_t{0x1EF3}, char16_t{0x1EF2}},
        SimpleUpperMapping{char16_t{0x1EF5}, char16_t{0x1EF4}},
        SimpleUpperMapping{char16_t{0x1EF7}, char16_t{0x1EF6}},
        SimpleUpperMapping{char16_t{0x1EF9}, char16_t{0x1EF8}},
        SimpleUpperMapping{char16_t{0x1EFB}, char16_t{0x1EFA}},
        SimpleUpperMapping{char16_t{0x1EFD}, char16_t{0x1EFC}},
        SimpleUpperMapping{char16_t{0x1EFF}, char16_t{0x1EFE}},
        SimpleUpperMapping{char16_t{0x1F00}, char16_t{0x1F08}},
        SimpleUpperMapping{char16_t{0x1F01}, char16_t{0x1F09}},
        SimpleUpperMapping{char16_t{0x1F02}, char16_t{0x1F0A}},
        SimpleUpperMapping{char16_t{0x1F03}, char16_t{0x1F0B}},
        SimpleUpperMapping{char16_t{0x1F04}, char16_t{0x1F0C}},
        SimpleUpperMapping{char16_t{0x1F05}, char16_t{0x1F0D}},
        SimpleUpperMapping{char16_t{0x1F06}, char16_t{0x1F0E}},
        SimpleUpperMapping{char16_t{0x1F07}, char16_t{0x1F0F}},
        SimpleUpperMapping{char16_t{0x1F10}, char16_t{0x1F18}},
        SimpleUpperMapping{char16_t{0x1F11}, char16_t{0x1F19}},
        SimpleUpperMapping{char16_t{0x1F12}, char16_t{0x1F1A}},
        SimpleUpperMapping{char16_t{0x1F13}, char16_t{0x1F1B}},
        SimpleUpperMapping{char16_t{0x1F14}, char16_t{0x1F1C}},
        SimpleUpperMapping{char16_t{0x1F15}, char16_t{0x1F1D}},
        SimpleUpperMapping{char16_t{0x1F20}, char16_t{0x1F28}},
        SimpleUpperMapping{char16_t{0x1F21}, char16_t{0x1F29}},
        SimpleUpperMapping{char16_t{0x1F22}, char16_t{0x1F2A}},
        SimpleUpperMapping{char16_t{0x1F23}, char16_t{0x1F2B}},
        SimpleUpperMapping{char16_t{0x1F24}, char16_t{0x1F2C}},
        SimpleUpperMapping{char16_t{0x1F25}, char16_t{0x1F2D}},
        SimpleUpperMapping{char16_t{0x1F26}, char16_t{0x1F2E}},
        SimpleUpperMapping{char16_t{0x1F27}, char16_t{0x1F2F}},
        SimpleUpperMapping{char16_t{0x1F30}, char16_t{0x1F38}},
        SimpleUpperMapping{char16_t{0x1F31}, char16_t{0x1F39}},
        SimpleUpperMapping{char16_t{0x1F32}, char16_t{0x1F3A}},
        SimpleUpperMapping{char16_t{0x1F33}, char16_t{0x1F3B}},
        SimpleUpperMapping{char16_t{0x1F34}, char16_t{0x1F3C}},
        SimpleUpperMapping{char16_t{0x1F35}, char16_t{0x1F3D}},
        SimpleUpperMapping{char16_t{0x1F36}, char16_t{0x1F3E}},
        SimpleUpperMapping{char16_t{0x1F37}, char16_t{0x1F3F}},
        SimpleUpperMapping{char16_t{0x1F40}, char16_t{0x1F48}},
        SimpleUpperMapping{char16_t{0x1F41}, char16_t{0x1F49}},
        SimpleUpperMapping{char16_t{0x1F42}, char16_t{0x1F4A}},
        SimpleUpperMapping{char16_t{0x1F43}, char16_t{0x1F4B}},
        SimpleUpperMapping{char16_t{0x1F44}, char16_t{0x1F4C}},
        SimpleUpperMapping{char16_t{0x1F45}, char16_t{0x1F4D}},
        SimpleUpperMapping{char16_t{0x1F51}, char16_t{0x1F59}},
        SimpleUpperMapping{char16_t{0x1F53}, char16_t{0x1F5B}},
        SimpleUpperMapping{char16_t{0x1F55}, char16_t{0x1F5D}},
        SimpleUpperMapping{char16_t{0x1F57}, char16_t{0x1F5F}},
        SimpleUpperMapping{char16_t{0x1F60}, char16_t{0x1F68}},
        SimpleUpperMapping{char16_t{0x1F61}, char16_t{0x1F69}},
        SimpleUpperMapping{char16_t{0x1F62}, char16_t{0x1F6A}},
        SimpleUpperMapping{char16_t{0x1F63}, char16_t{0x1F6B}},
        SimpleUpperMapping{char16_t{0x1F64}, char16_t{0x1F6C}},
        SimpleUpperMapping{char16_t{0x1F65}, char16_t{0x1F6D}},
        SimpleUpperMapping{char16_t{0x1F66}, char16_t{0x1F6E}},
        SimpleUpperMapping{char16_t{0x1F67}, char16_t{0x1F6F}},
        SimpleUpperMapping{char16_t{0x1F70}, char16_t{0x1FBA}},
        SimpleUpperMapping{char16_t{0x1F71}, char16_t{0x1FBB}},
        SimpleUpperMapping{char16_t{0x1F72}, char16_t{0x1FC8}},
        SimpleUpperMapping{char16_t{0x1F73}, char16_t{0x1FC9}},
        SimpleUpperMapping{char16_t{0x1F74}, char16_t{0x1FCA}},
        SimpleUpperMapping{char16_t{0x1F75}, char16_t{0x1FCB}},
        SimpleUpperMapping{char16_t{0x1F76}, char16_t{0x1FDA}},
        SimpleUpperMapping{char16_t{0x1F77}, char16_t{0x1FDB}},
        SimpleUpperMapping{char16_t{0x1F78}, char16_t{0x1FF8}},
        SimpleUpperMapping{char16_t{0x1F79}, char16_t{0x1FF9}},
        SimpleUpperMapping{char16_t{0x1F7A}, char16_t{0x1FEA}},
        SimpleUpperMapping{char16_t{0x1F7B}, char16_t{0x1FEB}},
        SimpleUpperMapping{char16_t{0x1F7C}, char16_t{0x1FFA}},
        SimpleUpperMapping{char16_t{0x1F7D}, char16_t{0x1FFB}},
        SimpleUpperMapping{char16_t{0x1F80}, char16_t{0x1F88}},
        SimpleUpperMapping{char16_t{0x1F81}, char16_t{0x1F89}},
        SimpleUpperMapping{char16_t{0x1F82}, char16_t{0x1F8A}},
        SimpleUpperMapping{char16_t{0x1F83}, char16_t{0x1F8B}},
        SimpleUpperMapping{char16_t{0x1F84}, char16_t{0x1F8C}},
        SimpleUpperMapping{char16_t{0x1F85}, char16_t{0x1F8D}},
        SimpleUpperMapping{char16_t{0x1F86}, char16_t{0x1F8E}},
        SimpleUpperMapping{char16_t{0x1F87}, char16_t{0x1F8F}},
        SimpleUpperMapping{char16_t{0x1F90}, char16_t{0x1F98}},
        SimpleUpperMapping{char16_t{0x1F91}, char16_t{0x1F99}},
        SimpleUpperMapping{char16_t{0x1F92}, char16_t{0x1F9A}},
        SimpleUpperMapping{char16_t{0x1F93}, char16_t{0x1F9B}},
        SimpleUpperMapping{char16_t{0x1F94}, char16_t{0x1F9C}},
        SimpleUpperMapping{char16_t{0x1F95}, char16_t{0x1F9D}},
        SimpleUpperMapping{char16_t{0x1F96}, char16_t{0x1F9E}},
        SimpleUpperMapping{char16_t{0x1F97}, char16_t{0x1F9F}},
        SimpleUpperMapping{char16_t{0x1FA0}, char16_t{0x1FA8}},
        SimpleUpperMapping{char16_t{0x1FA1}, char16_t{0x1FA9}},
        SimpleUpperMapping{char16_t{0x1FA2}, char16_t{0x1FAA}},
        SimpleUpperMapping{char16_t{0x1FA3}, char16_t{0x1FAB}},
        SimpleUpperMapping{char16_t{0x1FA4}, char16_t{0x1FAC}},
        SimpleUpperMapping{char16_t{0x1FA5}, char16_t{0x1FAD}},
        SimpleUpperMapping{char16_t{0x1FA6}, char16_t{0x1FAE}},
        SimpleUpperMapping{char16_t{0x1FA7}, char16_t{0x1FAF}},
        SimpleUpperMapping{char16_t{0x1FB0}, char16_t{0x1FB8}},
        SimpleUpperMapping{char16_t{0x1FB1}, char16_t{0x1FB9}},
        SimpleUpperMapping{char16_t{0x1FB3}, char16_t{0x1FBC}},
        SimpleUpperMapping{char16_t{0x1FBE}, char16_t{0x0399}},
        SimpleUpperMapping{char16_t{0x1FC3}, char16_t{0x1FCC}},
        SimpleUpperMapping{char16_t{0x1FD0}, char16_t{0x1FD8}},
        SimpleUpperMapping{char16_t{0x1FD1}, char16_t{0x1FD9}},
        SimpleUpperMapping{char16_t{0x1FE0}, char16_t{0x1FE8}},
        SimpleUpperMapping{char16_t{0x1FE1}, char16_t{0x1FE9}},
        SimpleUpperMapping{char16_t{0x1FE5}, char16_t{0x1FEC}},
        SimpleUpperMapping{char16_t{0x1FF3}, char16_t{0x1FFC}},
        SimpleUpperMapping{char16_t{0x214E}, char16_t{0x2132}},
        SimpleUpperMapping{char16_t{0x2170}, char16_t{0x2160}},
        SimpleUpperMapping{char16_t{0x2171}, char16_t{0x2161}},
        SimpleUpperMapping{char16_t{0x2172}, char16_t{0x2162}},
        SimpleUpperMapping{char16_t{0x2173}, char16_t{0x2163}},
        SimpleUpperMapping{char16_t{0x2174}, char16_t{0x2164}},
        SimpleUpperMapping{char16_t{0x2175}, char16_t{0x2165}},
        SimpleUpperMapping{char16_t{0x2176}, char16_t{0x2166}},
        SimpleUpperMapping{char16_t{0x2177}, char16_t{0x2167}},
        SimpleUpperMapping{char16_t{0x2178}, char16_t{0x2168}},
        SimpleUpperMapping{char16_t{0x2179}, char16_t{0x2169}},
        SimpleUpperMapping{char16_t{0x217A}, char16_t{0x216A}},
        SimpleUpperMapping{char16_t{0x217B}, char16_t{0x216B}},
        SimpleUpperMapping{char16_t{0x217C}, char16_t{0x216C}},
        SimpleUpperMapping{char16_t{0x217D}, char16_t{0x216D}},
        SimpleUpperMapping{char16_t{0x217E}, char16_t{0x216E}},
        SimpleUpperMapping{char16_t{0x217F}, char16_t{0x216F}},
        SimpleUpperMapping{char16_t{0x2184}, char16_t{0x2183}},
        SimpleUpperMapping{char16_t{0x24D0}, char16_t{0x24B6}},
        SimpleUpperMapping{char16_t{0x24D1}, char16_t{0x24B7}},
        SimpleUpperMapping{char16_t{0x24D2}, char16_t{0x24B8}},
        SimpleUpperMapping{char16_t{0x24D3}, char16_t{0x24B9}},
        SimpleUpperMapping{char16_t{0x24D4}, char16_t{0x24BA}},
        SimpleUpperMapping{char16_t{0x24D5}, char16_t{0x24BB}},
        SimpleUpperMapping{char16_t{0x24D6}, char16_t{0x24BC}},
        SimpleUpperMapping{char16_t{0x24D7}, char16_t{0x24BD}},
        SimpleUpperMapping{char16_t{0x24D8}, char16_t{0x24BE}},
        SimpleUpperMapping{char16_t{0x24D9}, char16_t{0x24BF}},
        SimpleUpperMapping{char16_t{0x24DA}, char16_t{0x24C0}},
        SimpleUpperMapping{char16_t{0x24DB}, char16_t{0x24C1}},
        SimpleUpperMapping{char16_t{0x24DC}, char16_t{0x24C2}},
        SimpleUpperMapping{char16_t{0x24DD}, char16_t{0x24C3}},
        SimpleUpperMapping{char16_t{0x24DE}, char16_t{0x24C4}},
        SimpleUpperMapping{char16_t{0x24DF}, char16_t{0x24C5}},
        SimpleUpperMapping{char16_t{0x24E0}, char16_t{0x24C6}},
        SimpleUpperMapping{char16_t{0x24E1}, char16_t{0x24C7}},
        SimpleUpperMapping{char16_t{0x24E2}, char16_t{0x24C8}},
        SimpleUpperMapping{char16_t{0x24E3}, char16_t{0x24C9}},
        SimpleUpperMapping{char16_t{0x24E4}, char16_t{0x24CA}},
        SimpleUpperMapping{char16_t{0x24E5}, char16_t{0x24CB}},
        SimpleUpperMapping{char16_t{0x24E6}, char16_t{0x24CC}},
        SimpleUpperMapping{char16_t{0x24E7}, char16_t{0x24CD}},
        SimpleUpperMapping{char16_t{0x24E8}, char16_t{0x24CE}},
        SimpleUpperMapping{char16_t{0x24E9}, char16_t{0x24CF}},
        SimpleUpperMapping{char16_t{0x2C30}, char16_t{0x2C00}},
        SimpleUpperMapping{char16_t{0x2C31}, char16_t{0x2C01}},
        SimpleUpperMapping{char16_t{0x2C32}, char16_t{0x2C02}},
        SimpleUpperMapping{char16_t{0x2C33}, char16_t{0x2C03}},
        SimpleUpperMapping{char16_t{0x2C34}, char16_t{0x2C04}},
        SimpleUpperMapping{char16_t{0x2C35}, char16_t{0x2C05}},
        SimpleUpperMapping{char16_t{0x2C36}, char16_t{0x2C06}},
        SimpleUpperMapping{char16_t{0x2C37}, char16_t{0x2C07}},
        SimpleUpperMapping{char16_t{0x2C38}, char16_t{0x2C08}},
        SimpleUpperMapping{char16_t{0x2C39}, char16_t{0x2C09}},
        SimpleUpperMapping{char16_t{0x2C3A}, char16_t{0x2C0A}},
        SimpleUpperMapping{char16_t{0x2C3B}, char16_t{0x2C0B}},
        SimpleUpperMapping{char16_t{0x2C3C}, char16_t{0x2C0C}},
        SimpleUpperMapping{char16_t{0x2C3D}, char16_t{0x2C0D}},
        SimpleUpperMapping{char16_t{0x2C3E}, char16_t{0x2C0E}},
        SimpleUpperMapping{char16_t{0x2C3F}, char16_t{0x2C0F}},
        SimpleUpperMapping{char16_t{0x2C40}, char16_t{0x2C10}},
        SimpleUpperMapping{char16_t{0x2C41}, char16_t{0x2C11}},
        SimpleUpperMapping{char16_t{0x2C42}, char16_t{0x2C12}},
        SimpleUpperMapping{char16_t{0x2C43}, char16_t{0x2C13}},
        SimpleUpperMapping{char16_t{0x2C44}, char16_t{0x2C14}},
        SimpleUpperMapping{char16_t{0x2C45}, char16_t{0x2C15}},
        SimpleUpperMapping{char16_t{0x2C46}, char16_t{0x2C16}},
        SimpleUpperMapping{char16_t{0x2C47}, char16_t{0x2C17}},
        SimpleUpperMapping{char16_t{0x2C48}, char16_t{0x2C18}},
        SimpleUpperMapping{char16_t{0x2C49}, char16_t{0x2C19}},
        SimpleUpperMapping{char16_t{0x2C4A}, char16_t{0x2C1A}},
        SimpleUpperMapping{char16_t{0x2C4B}, char16_t{0x2C1B}},
        SimpleUpperMapping{char16_t{0x2C4C}, char16_t{0x2C1C}},
        SimpleUpperMapping{char16_t{0x2C4D}, char16_t{0x2C1D}},
        SimpleUpperMapping{char16_t{0x2C4E}, char16_t{0x2C1E}},
        SimpleUpperMapping{char16_t{0x2C4F}, char16_t{0x2C1F}},
        SimpleUpperMapping{char16_t{0x2C50}, char16_t{0x2C20}},
        SimpleUpperMapping{char16_t{0x2C51}, char16_t{0x2C21}},
        SimpleUpperMapping{char16_t{0x2C52}, char16_t{0x2C22}},
        SimpleUpperMapping{char16_t{0x2C53}, char16_t{0x2C23}},
        SimpleUpperMapping{char16_t{0x2C54}, char16_t{0x2C24}},
        SimpleUpperMapping{char16_t{0x2C55}, char16_t{0x2C25}},
        SimpleUpperMapping{char16_t{0x2C56}, char16_t{0x2C26}},
        SimpleUpperMapping{char16_t{0x2C57}, char16_t{0x2C27}},
        SimpleUpperMapping{char16_t{0x2C58}, char16_t{0x2C28}},
        SimpleUpperMapping{char16_t{0x2C59}, char16_t{0x2C29}},
        SimpleUpperMapping{char16_t{0x2C5A}, char16_t{0x2C2A}},
        SimpleUpperMapping{char16_t{0x2C5B}, char16_t{0x2C2B}},
        SimpleUpperMapping{char16_t{0x2C5C}, char16_t{0x2C2C}},
        SimpleUpperMapping{char16_t{0x2C5D}, char16_t{0x2C2D}},
        SimpleUpperMapping{char16_t{0x2C5E}, char16_t{0x2C2E}},
        SimpleUpperMapping{char16_t{0x2C5F}, char16_t{0x2C2F}},
        SimpleUpperMapping{char16_t{0x2C61}, char16_t{0x2C60}},
        SimpleUpperMapping{char16_t{0x2C65}, char16_t{0x023A}},
        SimpleUpperMapping{char16_t{0x2C66}, char16_t{0x023E}},
        SimpleUpperMapping{char16_t{0x2C68}, char16_t{0x2C67}},
        SimpleUpperMapping{char16_t{0x2C6A}, char16_t{0x2C69}},
        SimpleUpperMapping{char16_t{0x2C6C}, char16_t{0x2C6B}},
        SimpleUpperMapping{char16_t{0x2C73}, char16_t{0x2C72}},
        SimpleUpperMapping{char16_t{0x2C76}, char16_t{0x2C75}},
        SimpleUpperMapping{char16_t{0x2C81}, char16_t{0x2C80}},
        SimpleUpperMapping{char16_t{0x2C83}, char16_t{0x2C82}},
        SimpleUpperMapping{char16_t{0x2C85}, char16_t{0x2C84}},
        SimpleUpperMapping{char16_t{0x2C87}, char16_t{0x2C86}},
        SimpleUpperMapping{char16_t{0x2C89}, char16_t{0x2C88}},
        SimpleUpperMapping{char16_t{0x2C8B}, char16_t{0x2C8A}},
        SimpleUpperMapping{char16_t{0x2C8D}, char16_t{0x2C8C}},
        SimpleUpperMapping{char16_t{0x2C8F}, char16_t{0x2C8E}},
        SimpleUpperMapping{char16_t{0x2C91}, char16_t{0x2C90}},
        SimpleUpperMapping{char16_t{0x2C93}, char16_t{0x2C92}},
        SimpleUpperMapping{char16_t{0x2C95}, char16_t{0x2C94}},
        SimpleUpperMapping{char16_t{0x2C97}, char16_t{0x2C96}},
        SimpleUpperMapping{char16_t{0x2C99}, char16_t{0x2C98}},
        SimpleUpperMapping{char16_t{0x2C9B}, char16_t{0x2C9A}},
        SimpleUpperMapping{char16_t{0x2C9D}, char16_t{0x2C9C}},
        SimpleUpperMapping{char16_t{0x2C9F}, char16_t{0x2C9E}},
        SimpleUpperMapping{char16_t{0x2CA1}, char16_t{0x2CA0}},
        SimpleUpperMapping{char16_t{0x2CA3}, char16_t{0x2CA2}},
        SimpleUpperMapping{char16_t{0x2CA5}, char16_t{0x2CA4}},
        SimpleUpperMapping{char16_t{0x2CA7}, char16_t{0x2CA6}},
        SimpleUpperMapping{char16_t{0x2CA9}, char16_t{0x2CA8}},
        SimpleUpperMapping{char16_t{0x2CAB}, char16_t{0x2CAA}},
        SimpleUpperMapping{char16_t{0x2CAD}, char16_t{0x2CAC}},
        SimpleUpperMapping{char16_t{0x2CAF}, char16_t{0x2CAE}},
        SimpleUpperMapping{char16_t{0x2CB1}, char16_t{0x2CB0}},
        SimpleUpperMapping{char16_t{0x2CB3}, char16_t{0x2CB2}},
        SimpleUpperMapping{char16_t{0x2CB5}, char16_t{0x2CB4}},
        SimpleUpperMapping{char16_t{0x2CB7}, char16_t{0x2CB6}},
        SimpleUpperMapping{char16_t{0x2CB9}, char16_t{0x2CB8}},
        SimpleUpperMapping{char16_t{0x2CBB}, char16_t{0x2CBA}},
        SimpleUpperMapping{char16_t{0x2CBD}, char16_t{0x2CBC}},
        SimpleUpperMapping{char16_t{0x2CBF}, char16_t{0x2CBE}},
        SimpleUpperMapping{char16_t{0x2CC1}, char16_t{0x2CC0}},
        SimpleUpperMapping{char16_t{0x2CC3}, char16_t{0x2CC2}},
        SimpleUpperMapping{char16_t{0x2CC5}, char16_t{0x2CC4}},
        SimpleUpperMapping{char16_t{0x2CC7}, char16_t{0x2CC6}},
        SimpleUpperMapping{char16_t{0x2CC9}, char16_t{0x2CC8}},
        SimpleUpperMapping{char16_t{0x2CCB}, char16_t{0x2CCA}},
        SimpleUpperMapping{char16_t{0x2CCD}, char16_t{0x2CCC}},
        SimpleUpperMapping{char16_t{0x2CCF}, char16_t{0x2CCE}},
        SimpleUpperMapping{char16_t{0x2CD1}, char16_t{0x2CD0}},
        SimpleUpperMapping{char16_t{0x2CD3}, char16_t{0x2CD2}},
        SimpleUpperMapping{char16_t{0x2CD5}, char16_t{0x2CD4}},
        SimpleUpperMapping{char16_t{0x2CD7}, char16_t{0x2CD6}},
        SimpleUpperMapping{char16_t{0x2CD9}, char16_t{0x2CD8}},
        SimpleUpperMapping{char16_t{0x2CDB}, char16_t{0x2CDA}},
        SimpleUpperMapping{char16_t{0x2CDD}, char16_t{0x2CDC}},
        SimpleUpperMapping{char16_t{0x2CDF}, char16_t{0x2CDE}},
        SimpleUpperMapping{char16_t{0x2CE1}, char16_t{0x2CE0}},
        SimpleUpperMapping{char16_t{0x2CE3}, char16_t{0x2CE2}},
        SimpleUpperMapping{char16_t{0x2CEC}, char16_t{0x2CEB}},
        SimpleUpperMapping{char16_t{0x2CEE}, char16_t{0x2CED}},
        SimpleUpperMapping{char16_t{0x2CF3}, char16_t{0x2CF2}},
        SimpleUpperMapping{char16_t{0x2D00}, char16_t{0x10A0}},
        SimpleUpperMapping{char16_t{0x2D01}, char16_t{0x10A1}},
        SimpleUpperMapping{char16_t{0x2D02}, char16_t{0x10A2}},
        SimpleUpperMapping{char16_t{0x2D03}, char16_t{0x10A3}},
        SimpleUpperMapping{char16_t{0x2D04}, char16_t{0x10A4}},
        SimpleUpperMapping{char16_t{0x2D05}, char16_t{0x10A5}},
        SimpleUpperMapping{char16_t{0x2D06}, char16_t{0x10A6}},
        SimpleUpperMapping{char16_t{0x2D07}, char16_t{0x10A7}},
        SimpleUpperMapping{char16_t{0x2D08}, char16_t{0x10A8}},
        SimpleUpperMapping{char16_t{0x2D09}, char16_t{0x10A9}},
        SimpleUpperMapping{char16_t{0x2D0A}, char16_t{0x10AA}},
        SimpleUpperMapping{char16_t{0x2D0B}, char16_t{0x10AB}},
        SimpleUpperMapping{char16_t{0x2D0C}, char16_t{0x10AC}},
        SimpleUpperMapping{char16_t{0x2D0D}, char16_t{0x10AD}},
        SimpleUpperMapping{char16_t{0x2D0E}, char16_t{0x10AE}},
        SimpleUpperMapping{char16_t{0x2D0F}, char16_t{0x10AF}},
        SimpleUpperMapping{char16_t{0x2D10}, char16_t{0x10B0}},
        SimpleUpperMapping{char16_t{0x2D11}, char16_t{0x10B1}},
        SimpleUpperMapping{char16_t{0x2D12}, char16_t{0x10B2}},
        SimpleUpperMapping{char16_t{0x2D13}, char16_t{0x10B3}},
        SimpleUpperMapping{char16_t{0x2D14}, char16_t{0x10B4}},
        SimpleUpperMapping{char16_t{0x2D15}, char16_t{0x10B5}},
        SimpleUpperMapping{char16_t{0x2D16}, char16_t{0x10B6}},
        SimpleUpperMapping{char16_t{0x2D17}, char16_t{0x10B7}},
        SimpleUpperMapping{char16_t{0x2D18}, char16_t{0x10B8}},
        SimpleUpperMapping{char16_t{0x2D19}, char16_t{0x10B9}},
        SimpleUpperMapping{char16_t{0x2D1A}, char16_t{0x10BA}},
        SimpleUpperMapping{char16_t{0x2D1B}, char16_t{0x10BB}},
        SimpleUpperMapping{char16_t{0x2D1C}, char16_t{0x10BC}},
        SimpleUpperMapping{char16_t{0x2D1D}, char16_t{0x10BD}},
        SimpleUpperMapping{char16_t{0x2D1E}, char16_t{0x10BE}},
        SimpleUpperMapping{char16_t{0x2D1F}, char16_t{0x10BF}},
        SimpleUpperMapping{char16_t{0x2D20}, char16_t{0x10C0}},
        SimpleUpperMapping{char16_t{0x2D21}, char16_t{0x10C1}},
        SimpleUpperMapping{char16_t{0x2D22}, char16_t{0x10C2}},
        SimpleUpperMapping{char16_t{0x2D23}, char16_t{0x10C3}},
        SimpleUpperMapping{char16_t{0x2D24}, char16_t{0x10C4}},
        SimpleUpperMapping{char16_t{0x2D25}, char16_t{0x10C5}},
        SimpleUpperMapping{char16_t{0x2D27}, char16_t{0x10C7}},
        SimpleUpperMapping{char16_t{0x2D2D}, char16_t{0x10CD}},
        SimpleUpperMapping{char16_t{0xA641}, char16_t{0xA640}},
        SimpleUpperMapping{char16_t{0xA643}, char16_t{0xA642}},
        SimpleUpperMapping{char16_t{0xA645}, char16_t{0xA644}},
        SimpleUpperMapping{char16_t{0xA647}, char16_t{0xA646}},
        SimpleUpperMapping{char16_t{0xA649}, char16_t{0xA648}},
        SimpleUpperMapping{char16_t{0xA64B}, char16_t{0xA64A}},
        SimpleUpperMapping{char16_t{0xA64D}, char16_t{0xA64C}},
        SimpleUpperMapping{char16_t{0xA64F}, char16_t{0xA64E}},
        SimpleUpperMapping{char16_t{0xA651}, char16_t{0xA650}},
        SimpleUpperMapping{char16_t{0xA653}, char16_t{0xA652}},
        SimpleUpperMapping{char16_t{0xA655}, char16_t{0xA654}},
        SimpleUpperMapping{char16_t{0xA657}, char16_t{0xA656}},
        SimpleUpperMapping{char16_t{0xA659}, char16_t{0xA658}},
        SimpleUpperMapping{char16_t{0xA65B}, char16_t{0xA65A}},
        SimpleUpperMapping{char16_t{0xA65D}, char16_t{0xA65C}},
        SimpleUpperMapping{char16_t{0xA65F}, char16_t{0xA65E}},
        SimpleUpperMapping{char16_t{0xA661}, char16_t{0xA660}},
        SimpleUpperMapping{char16_t{0xA663}, char16_t{0xA662}},
        SimpleUpperMapping{char16_t{0xA665}, char16_t{0xA664}},
        SimpleUpperMapping{char16_t{0xA667}, char16_t{0xA666}},
        SimpleUpperMapping{char16_t{0xA669}, char16_t{0xA668}},
        SimpleUpperMapping{char16_t{0xA66B}, char16_t{0xA66A}},
        SimpleUpperMapping{char16_t{0xA66D}, char16_t{0xA66C}},
        SimpleUpperMapping{char16_t{0xA681}, char16_t{0xA680}},
        SimpleUpperMapping{char16_t{0xA683}, char16_t{0xA682}},
        SimpleUpperMapping{char16_t{0xA685}, char16_t{0xA684}},
        SimpleUpperMapping{char16_t{0xA687}, char16_t{0xA686}},
        SimpleUpperMapping{char16_t{0xA689}, char16_t{0xA688}},
        SimpleUpperMapping{char16_t{0xA68B}, char16_t{0xA68A}},
        SimpleUpperMapping{char16_t{0xA68D}, char16_t{0xA68C}},
        SimpleUpperMapping{char16_t{0xA68F}, char16_t{0xA68E}},
        SimpleUpperMapping{char16_t{0xA691}, char16_t{0xA690}},
        SimpleUpperMapping{char16_t{0xA693}, char16_t{0xA692}},
        SimpleUpperMapping{char16_t{0xA695}, char16_t{0xA694}},
        SimpleUpperMapping{char16_t{0xA697}, char16_t{0xA696}},
        SimpleUpperMapping{char16_t{0xA699}, char16_t{0xA698}},
        SimpleUpperMapping{char16_t{0xA69B}, char16_t{0xA69A}},
        SimpleUpperMapping{char16_t{0xA723}, char16_t{0xA722}},
        SimpleUpperMapping{char16_t{0xA725}, char16_t{0xA724}},
        SimpleUpperMapping{char16_t{0xA727}, char16_t{0xA726}},
        SimpleUpperMapping{char16_t{0xA729}, char16_t{0xA728}},
        SimpleUpperMapping{char16_t{0xA72B}, char16_t{0xA72A}},
        SimpleUpperMapping{char16_t{0xA72D}, char16_t{0xA72C}},
        SimpleUpperMapping{char16_t{0xA72F}, char16_t{0xA72E}},
        SimpleUpperMapping{char16_t{0xA733}, char16_t{0xA732}},
        SimpleUpperMapping{char16_t{0xA735}, char16_t{0xA734}},
        SimpleUpperMapping{char16_t{0xA737}, char16_t{0xA736}},
        SimpleUpperMapping{char16_t{0xA739}, char16_t{0xA738}},
        SimpleUpperMapping{char16_t{0xA73B}, char16_t{0xA73A}},
        SimpleUpperMapping{char16_t{0xA73D}, char16_t{0xA73C}},
        SimpleUpperMapping{char16_t{0xA73F}, char16_t{0xA73E}},
        SimpleUpperMapping{char16_t{0xA741}, char16_t{0xA740}},
        SimpleUpperMapping{char16_t{0xA743}, char16_t{0xA742}},
        SimpleUpperMapping{char16_t{0xA745}, char16_t{0xA744}},
        SimpleUpperMapping{char16_t{0xA747}, char16_t{0xA746}},
        SimpleUpperMapping{char16_t{0xA749}, char16_t{0xA748}},
        SimpleUpperMapping{char16_t{0xA74B}, char16_t{0xA74A}},
        SimpleUpperMapping{char16_t{0xA74D}, char16_t{0xA74C}},
        SimpleUpperMapping{char16_t{0xA74F}, char16_t{0xA74E}},
        SimpleUpperMapping{char16_t{0xA751}, char16_t{0xA750}},
        SimpleUpperMapping{char16_t{0xA753}, char16_t{0xA752}},
        SimpleUpperMapping{char16_t{0xA755}, char16_t{0xA754}},
        SimpleUpperMapping{char16_t{0xA757}, char16_t{0xA756}},
        SimpleUpperMapping{char16_t{0xA759}, char16_t{0xA758}},
        SimpleUpperMapping{char16_t{0xA75B}, char16_t{0xA75A}},
        SimpleUpperMapping{char16_t{0xA75D}, char16_t{0xA75C}},
        SimpleUpperMapping{char16_t{0xA75F}, char16_t{0xA75E}},
        SimpleUpperMapping{char16_t{0xA761}, char16_t{0xA760}},
        SimpleUpperMapping{char16_t{0xA763}, char16_t{0xA762}},
        SimpleUpperMapping{char16_t{0xA765}, char16_t{0xA764}},
        SimpleUpperMapping{char16_t{0xA767}, char16_t{0xA766}},
        SimpleUpperMapping{char16_t{0xA769}, char16_t{0xA768}},
        SimpleUpperMapping{char16_t{0xA76B}, char16_t{0xA76A}},
        SimpleUpperMapping{char16_t{0xA76D}, char16_t{0xA76C}},
        SimpleUpperMapping{char16_t{0xA76F}, char16_t{0xA76E}},
        SimpleUpperMapping{char16_t{0xA77A}, char16_t{0xA779}},
        SimpleUpperMapping{char16_t{0xA77C}, char16_t{0xA77B}},
        SimpleUpperMapping{char16_t{0xA77F}, char16_t{0xA77E}},
        SimpleUpperMapping{char16_t{0xA781}, char16_t{0xA780}},
        SimpleUpperMapping{char16_t{0xA783}, char16_t{0xA782}},
        SimpleUpperMapping{char16_t{0xA785}, char16_t{0xA784}},
        SimpleUpperMapping{char16_t{0xA787}, char16_t{0xA786}},
        SimpleUpperMapping{char16_t{0xA78C}, char16_t{0xA78B}},
        SimpleUpperMapping{char16_t{0xA791}, char16_t{0xA790}},
        SimpleUpperMapping{char16_t{0xA793}, char16_t{0xA792}},
        SimpleUpperMapping{char16_t{0xA794}, char16_t{0xA7C4}},
        SimpleUpperMapping{char16_t{0xA797}, char16_t{0xA796}},
        SimpleUpperMapping{char16_t{0xA799}, char16_t{0xA798}},
        SimpleUpperMapping{char16_t{0xA79B}, char16_t{0xA79A}},
        SimpleUpperMapping{char16_t{0xA79D}, char16_t{0xA79C}},
        SimpleUpperMapping{char16_t{0xA79F}, char16_t{0xA79E}},
        SimpleUpperMapping{char16_t{0xA7A1}, char16_t{0xA7A0}},
        SimpleUpperMapping{char16_t{0xA7A3}, char16_t{0xA7A2}},
        SimpleUpperMapping{char16_t{0xA7A5}, char16_t{0xA7A4}},
        SimpleUpperMapping{char16_t{0xA7A7}, char16_t{0xA7A6}},
        SimpleUpperMapping{char16_t{0xA7A9}, char16_t{0xA7A8}},
        SimpleUpperMapping{char16_t{0xA7B5}, char16_t{0xA7B4}},
        SimpleUpperMapping{char16_t{0xA7B7}, char16_t{0xA7B6}},
        SimpleUpperMapping{char16_t{0xA7B9}, char16_t{0xA7B8}},
        SimpleUpperMapping{char16_t{0xA7BB}, char16_t{0xA7BA}},
        SimpleUpperMapping{char16_t{0xA7BD}, char16_t{0xA7BC}},
        SimpleUpperMapping{char16_t{0xA7BF}, char16_t{0xA7BE}},
        SimpleUpperMapping{char16_t{0xA7C1}, char16_t{0xA7C0}},
        SimpleUpperMapping{char16_t{0xA7C3}, char16_t{0xA7C2}},
        SimpleUpperMapping{char16_t{0xA7C8}, char16_t{0xA7C7}},
        SimpleUpperMapping{char16_t{0xA7CA}, char16_t{0xA7C9}},
        SimpleUpperMapping{char16_t{0xA7D1}, char16_t{0xA7D0}},
        SimpleUpperMapping{char16_t{0xA7D7}, char16_t{0xA7D6}},
        SimpleUpperMapping{char16_t{0xA7D9}, char16_t{0xA7D8}},
        SimpleUpperMapping{char16_t{0xA7F6}, char16_t{0xA7F5}},
        SimpleUpperMapping{char16_t{0xAB53}, char16_t{0xA7B3}},
        SimpleUpperMapping{char16_t{0xAB70}, char16_t{0x13A0}},
        SimpleUpperMapping{char16_t{0xAB71}, char16_t{0x13A1}},
        SimpleUpperMapping{char16_t{0xAB72}, char16_t{0x13A2}},
        SimpleUpperMapping{char16_t{0xAB73}, char16_t{0x13A3}},
        SimpleUpperMapping{char16_t{0xAB74}, char16_t{0x13A4}},
        SimpleUpperMapping{char16_t{0xAB75}, char16_t{0x13A5}},
        SimpleUpperMapping{char16_t{0xAB76}, char16_t{0x13A6}},
        SimpleUpperMapping{char16_t{0xAB77}, char16_t{0x13A7}},
        SimpleUpperMapping{char16_t{0xAB78}, char16_t{0x13A8}},
        SimpleUpperMapping{char16_t{0xAB79}, char16_t{0x13A9}},
        SimpleUpperMapping{char16_t{0xAB7A}, char16_t{0x13AA}},
        SimpleUpperMapping{char16_t{0xAB7B}, char16_t{0x13AB}},
        SimpleUpperMapping{char16_t{0xAB7C}, char16_t{0x13AC}},
        SimpleUpperMapping{char16_t{0xAB7D}, char16_t{0x13AD}},
        SimpleUpperMapping{char16_t{0xAB7E}, char16_t{0x13AE}},
        SimpleUpperMapping{char16_t{0xAB7F}, char16_t{0x13AF}},
        SimpleUpperMapping{char16_t{0xAB80}, char16_t{0x13B0}},
        SimpleUpperMapping{char16_t{0xAB81}, char16_t{0x13B1}},
        SimpleUpperMapping{char16_t{0xAB82}, char16_t{0x13B2}},
        SimpleUpperMapping{char16_t{0xAB83}, char16_t{0x13B3}},
        SimpleUpperMapping{char16_t{0xAB84}, char16_t{0x13B4}},
        SimpleUpperMapping{char16_t{0xAB85}, char16_t{0x13B5}},
        SimpleUpperMapping{char16_t{0xAB86}, char16_t{0x13B6}},
        SimpleUpperMapping{char16_t{0xAB87}, char16_t{0x13B7}},
        SimpleUpperMapping{char16_t{0xAB88}, char16_t{0x13B8}},
        SimpleUpperMapping{char16_t{0xAB89}, char16_t{0x13B9}},
        SimpleUpperMapping{char16_t{0xAB8A}, char16_t{0x13BA}},
        SimpleUpperMapping{char16_t{0xAB8B}, char16_t{0x13BB}},
        SimpleUpperMapping{char16_t{0xAB8C}, char16_t{0x13BC}},
        SimpleUpperMapping{char16_t{0xAB8D}, char16_t{0x13BD}},
        SimpleUpperMapping{char16_t{0xAB8E}, char16_t{0x13BE}},
        SimpleUpperMapping{char16_t{0xAB8F}, char16_t{0x13BF}},
        SimpleUpperMapping{char16_t{0xAB90}, char16_t{0x13C0}},
        SimpleUpperMapping{char16_t{0xAB91}, char16_t{0x13C1}},
        SimpleUpperMapping{char16_t{0xAB92}, char16_t{0x13C2}},
        SimpleUpperMapping{char16_t{0xAB93}, char16_t{0x13C3}},
        SimpleUpperMapping{char16_t{0xAB94}, char16_t{0x13C4}},
        SimpleUpperMapping{char16_t{0xAB95}, char16_t{0x13C5}},
        SimpleUpperMapping{char16_t{0xAB96}, char16_t{0x13C6}},
        SimpleUpperMapping{char16_t{0xAB97}, char16_t{0x13C7}},
        SimpleUpperMapping{char16_t{0xAB98}, char16_t{0x13C8}},
        SimpleUpperMapping{char16_t{0xAB99}, char16_t{0x13C9}},
        SimpleUpperMapping{char16_t{0xAB9A}, char16_t{0x13CA}},
        SimpleUpperMapping{char16_t{0xAB9B}, char16_t{0x13CB}},
        SimpleUpperMapping{char16_t{0xAB9C}, char16_t{0x13CC}},
        SimpleUpperMapping{char16_t{0xAB9D}, char16_t{0x13CD}},
        SimpleUpperMapping{char16_t{0xAB9E}, char16_t{0x13CE}},
        SimpleUpperMapping{char16_t{0xAB9F}, char16_t{0x13CF}},
        SimpleUpperMapping{char16_t{0xABA0}, char16_t{0x13D0}},
        SimpleUpperMapping{char16_t{0xABA1}, char16_t{0x13D1}},
        SimpleUpperMapping{char16_t{0xABA2}, char16_t{0x13D2}},
        SimpleUpperMapping{char16_t{0xABA3}, char16_t{0x13D3}},
        SimpleUpperMapping{char16_t{0xABA4}, char16_t{0x13D4}},
        SimpleUpperMapping{char16_t{0xABA5}, char16_t{0x13D5}},
        SimpleUpperMapping{char16_t{0xABA6}, char16_t{0x13D6}},
        SimpleUpperMapping{char16_t{0xABA7}, char16_t{0x13D7}},
        SimpleUpperMapping{char16_t{0xABA8}, char16_t{0x13D8}},
        SimpleUpperMapping{char16_t{0xABA9}, char16_t{0x13D9}},
        SimpleUpperMapping{char16_t{0xABAA}, char16_t{0x13DA}},
        SimpleUpperMapping{char16_t{0xABAB}, char16_t{0x13DB}},
        SimpleUpperMapping{char16_t{0xABAC}, char16_t{0x13DC}},
        SimpleUpperMapping{char16_t{0xABAD}, char16_t{0x13DD}},
        SimpleUpperMapping{char16_t{0xABAE}, char16_t{0x13DE}},
        SimpleUpperMapping{char16_t{0xABAF}, char16_t{0x13DF}},
        SimpleUpperMapping{char16_t{0xABB0}, char16_t{0x13E0}},
        SimpleUpperMapping{char16_t{0xABB1}, char16_t{0x13E1}},
        SimpleUpperMapping{char16_t{0xABB2}, char16_t{0x13E2}},
        SimpleUpperMapping{char16_t{0xABB3}, char16_t{0x13E3}},
        SimpleUpperMapping{char16_t{0xABB4}, char16_t{0x13E4}},
        SimpleUpperMapping{char16_t{0xABB5}, char16_t{0x13E5}},
        SimpleUpperMapping{char16_t{0xABB6}, char16_t{0x13E6}},
        SimpleUpperMapping{char16_t{0xABB7}, char16_t{0x13E7}},
        SimpleUpperMapping{char16_t{0xABB8}, char16_t{0x13E8}},
        SimpleUpperMapping{char16_t{0xABB9}, char16_t{0x13E9}},
        SimpleUpperMapping{char16_t{0xABBA}, char16_t{0x13EA}},
        SimpleUpperMapping{char16_t{0xABBB}, char16_t{0x13EB}},
        SimpleUpperMapping{char16_t{0xABBC}, char16_t{0x13EC}},
        SimpleUpperMapping{char16_t{0xABBD}, char16_t{0x13ED}},
        SimpleUpperMapping{char16_t{0xABBE}, char16_t{0x13EE}},
        SimpleUpperMapping{char16_t{0xABBF}, char16_t{0x13EF}},
        SimpleUpperMapping{char16_t{0xFF41}, char16_t{0xFF21}},
        SimpleUpperMapping{char16_t{0xFF42}, char16_t{0xFF22}},
        SimpleUpperMapping{char16_t{0xFF43}, char16_t{0xFF23}},
        SimpleUpperMapping{char16_t{0xFF44}, char16_t{0xFF24}},
        SimpleUpperMapping{char16_t{0xFF45}, char16_t{0xFF25}},
        SimpleUpperMapping{char16_t{0xFF46}, char16_t{0xFF26}},
        SimpleUpperMapping{char16_t{0xFF47}, char16_t{0xFF27}},
        SimpleUpperMapping{char16_t{0xFF48}, char16_t{0xFF28}},
        SimpleUpperMapping{char16_t{0xFF49}, char16_t{0xFF29}},
        SimpleUpperMapping{char16_t{0xFF4A}, char16_t{0xFF2A}},
        SimpleUpperMapping{char16_t{0xFF4B}, char16_t{0xFF2B}},
        SimpleUpperMapping{char16_t{0xFF4C}, char16_t{0xFF2C}},
        SimpleUpperMapping{char16_t{0xFF4D}, char16_t{0xFF2D}},
        SimpleUpperMapping{char16_t{0xFF4E}, char16_t{0xFF2E}},
        SimpleUpperMapping{char16_t{0xFF4F}, char16_t{0xFF2F}},
        SimpleUpperMapping{char16_t{0xFF50}, char16_t{0xFF30}},
        SimpleUpperMapping{char16_t{0xFF51}, char16_t{0xFF31}},
        SimpleUpperMapping{char16_t{0xFF52}, char16_t{0xFF32}},
        SimpleUpperMapping{char16_t{0xFF53}, char16_t{0xFF33}},
        SimpleUpperMapping{char16_t{0xFF54}, char16_t{0xFF34}},
        SimpleUpperMapping{char16_t{0xFF55}, char16_t{0xFF35}},
        SimpleUpperMapping{char16_t{0xFF56}, char16_t{0xFF36}},
        SimpleUpperMapping{char16_t{0xFF57}, char16_t{0xFF37}},
        SimpleUpperMapping{char16_t{0xFF58}, char16_t{0xFF38}},
        SimpleUpperMapping{char16_t{0xFF59}, char16_t{0xFF39}},
        SimpleUpperMapping{char16_t{0xFF5A}, char16_t{0xFF3A}}
    }};

    [[nodiscard]] char32_t SimpleUpperInvariant(char32_t value) noexcept
    {
        if (value > 0xFFFFU)
        {
            // TestPrint.cs uppercases part[0], a single UTF-16 code unit. For a
            // supplementary scalar that code unit is a high surrogate and remains unchanged;
            // rejoining it with part[1..] leaves the original scalar unchanged.
            return value;
        }
        const char16_t key = static_cast<char16_t>(value);
        const auto found = std::lower_bound(
            SimpleUpperMappings.begin(), SimpleUpperMappings.end(), key,
            [](const SimpleUpperMapping& mapping, char16_t candidate)
            {
                return mapping.Source < candidate;
            });
        return found != SimpleUpperMappings.end() && found->Source == key
            ? static_cast<char32_t>(found->Target)
            : value;
    }

    void AppendUtf8Scalar(std::string& output, char32_t value)
    {
        if (value <= 0x7FU)
        {
            output.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (value >> 6)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::string UpperFirstInvariant(std::string_view part)
    {
        if (part.empty())
        {
            throw ManagedIndexOutOfRangeException();
        }

        const Utf8CodePoint first = DecodeUtf8(part, 0);
        const unsigned char firstByte = static_cast<unsigned char>(part[0]);
        if (first.Length == 1 && firstByte >= 0x80U)
        {
            return std::string(part);
        }

        const char32_t upper = SimpleUpperInvariant(first.Value);
        if (upper == first.Value)
        {
            return std::string(part);
        }

        std::string result;
        result.reserve(part.size() + 2U);
        AppendUtf8Scalar(result, upper);
        result.append(part.substr(first.Length));
        return result;
    }

    [[nodiscard]] std::string FormatHexUInt32(std::uint32_t value, int minimumDigits)
    {
        char buffer[16]{};
        const auto [end, error] = std::to_chars(
            buffer, buffer + sizeof(buffer), value, 16);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format UInt32 as hexadecimal.");
        }
        std::string digits(buffer, end);
        for (char& ch : digits)
        {
            if (ch >= 'a' && ch <= 'f')
            {
                ch = static_cast<char>(ch - ('a' - 'A'));
            }
        }
        if (static_cast<int>(digits.size()) < minimumDigits)
        {
            digits.insert(
                digits.begin(),
                static_cast<std::size_t>(minimumDigits - static_cast<int>(digits.size())),
                '0');
        }
        return digits;
    }

    [[nodiscard]] std::string FormatHexInt32(std::int32_t value, int minimumDigits)
    {
        if (value < 0)
        {
            minimumDigits = std::max(minimumDigits, 8);
        }
        return FormatHexUInt32(ManagedUInt32(value), minimumDigits);
    }

    [[nodiscard]] std::string FormatBinary(std::uint32_t value)
    {
        if (value == 0)
        {
            return "0";
        }
        std::string result;
        result.reserve(32);
        bool started = false;
        for (int bit = 31; bit >= 0; --bit)
        {
            const bool set = (value & (std::uint32_t{1} << bit)) != 0;
            if (set)
            {
                started = true;
            }
            if (started)
            {
                result.push_back(set ? '1' : '0');
            }
        }
        return result;
    }

    [[nodiscard]] std::string PolygonModeName(MphRead::PolygonMode value)
    {
        switch (value)
        {
        case MphRead::PolygonMode::Modulate:
            return "Modulate";
        case MphRead::PolygonMode::Decal:
            return "Decal";
        case MphRead::PolygonMode::Toon:
            return "Toon";
        case MphRead::PolygonMode::Shadow:
            return "Shadow";
        }
        return FormatUInt32CurrentCulture(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::string CullingModeName(MphRead::CullingMode value)
    {
        switch (value)
        {
        case MphRead::CullingMode::Neither:
            return "Neither";
        case MphRead::CullingMode::Front:
            return "Front";
        case MphRead::CullingMode::Back:
            return "Back";
        }
        return FormatUInt32CurrentCulture(static_cast<std::uint32_t>(value));
    }

// External parity blocker: no shared Native System.Diagnostics.DebugProvider /
    // CLR debugger contract exists yet. Keep the existing compile-compatible fallbacks
    // private to this translation unit; they are not claimed as managed-equivalent.
#if defined(DEBUG)
    void DebugWriteLineFallback(const std::string& value)
    {
        std::clog << value << '\n';
    }

    [[noreturn]] void DebugAssertFailedFallback() noexcept
    {
        std::abort();
    }

    void DebugAssertFallback(bool condition) noexcept
    {
        if (!condition)
        {
            DebugAssertFailedFallback();
        }
    }
#endif

#if defined(DEBUG)
#define MPH_TESTPRINT_DEBUG_WRITE_LINE(value) DebugWriteLineFallback(value)
#define MPH_TESTPRINT_DEBUG_ASSERT(condition) DebugAssertFallback(condition)
#else
#define MPH_TESTPRINT_DEBUG_WRITE_LINE(value) do { } while (false)
#define MPH_TESTPRINT_DEBUG_ASSERT(condition) do { } while (false)
#endif

    void DebuggerBreakFallback()
    {
#if defined(_WIN32)
        __debugbreak();
#else
        std::raise(SIGTRAP);
#endif
    }

    [[nodiscard]] float ManagedMax(float left, float right) noexcept
    {
        // .NET 9 Math.Max(float,float): propagate NaN and prefer +0 over -0.
        if (left != right)
        {
            if (!std::isnan(left))
            {
                return right < left ? left : right;
            }
            return left;
        }
        return std::signbit(right) ? left : right;
    }
}

namespace MphRead::Testing
{
    void TestPrint::PrintStruct(
        const std::optional<std::string>& name, std::int32_t size)
    {
        MPH_TESTPRINT_DEBUG_ASSERT(size > 0 && size % 4 == 0);
        std::cout << "struct " << InterpolationString(name) << '\n';
        std::cout << "{\n";
        std::int32_t offset = 0;
        while (offset < size)
        {
            std::cout << "  int field_" << FormatHexInt32(offset, 1) << ";\n";
            offset = AddInt32(offset, 4);
        }
        std::cout << "}\n";
    }

    void TestPrint::GetPolygonAttrs(
        const std::shared_ptr<MphRead::Model>& model,
        std::int32_t polygonId)
    {
        Model& modelRef = Require(model);
        if (!modelRef.Materials)
        {
            throw System::NullReferenceException();
        }
        for (const std::shared_ptr<Material>& material : *modelRef.Materials)
        {
            GetPolygonAttrs(model, material, polygonId);
        }
    }

    void TestPrint::GetPolygonAttrs(
        const std::shared_ptr<MphRead::Model>& model,
        const std::shared_ptr<MphRead::Material>& material,
        std::int32_t polygonId)
    {
        MPH_TESTPRINT_DEBUG_ASSERT(polygonId >= 0);
        Model& modelRef = Require(model);
        Material& materialRef = Require(material);

        const std::uint32_t v19 = polygonId == 1 ? 0x4000U : 0U;
        const std::uint32_t v20 = v19 | 0x8000U;
        const std::uint32_t polygonModeBits =
            ManagedUInt32(MultiplyInt32(
                16,
                ManagedInt32(static_cast<std::uint32_t>(materialRef.PolygonMode))));
        const std::uint32_t cullingBits =
            static_cast<std::uint32_t>(materialRef.Culling) << 6U;
        const std::uint32_t polygonBits = ManagedUInt32(polygonId) << 24U;
        const std::uint32_t alphaBits =
            static_cast<std::uint32_t>(materialRef.Alpha) << 16U;
        const std::uint32_t attr = v20
            | static_cast<std::uint32_t>(materialRef.Lighting)
            | polygonModeBits | cullingBits | polygonBits | alphaBits;

        std::cout << modelRef.Name << " - " << materialRef.Name << '\n';
        std::cout << "light = "
                  << FormatUInt32CurrentCulture(materialRef.Lighting)
                  << ", mode = "
                  << FormatInt32CurrentCulture(
                      ManagedInt32(static_cast<std::uint32_t>(materialRef.PolygonMode)))
                  << " (" << PolygonModeName(materialRef.PolygonMode) << "), cull = "
                  << FormatUInt32CurrentCulture(
                      static_cast<std::uint32_t>(materialRef.Culling))
                  << " (" << CullingModeName(materialRef.Culling) << "), alpha = "
                  << FormatUInt32CurrentCulture(materialRef.Alpha)
                  << ", id = " << FormatInt32CurrentCulture(polygonId) << '\n';
        DumpPolygonAttr(attr);
    }

    void TestPrint::DumpPolygonAttr(std::uint32_t attr)
    {
        std::cout << "0x" << FormatHexUInt32(attr, 2) << '\n';
        std::cout << FormatBinary(attr) << '\n';
        std::cout << "light1: " << FormatUInt32CurrentCulture(attr & 0x1U) << '\n';
        std::cout << "light2: " << FormatUInt32CurrentCulture((attr >> 1U) & 0x1U) << '\n';
        std::cout << "light3: " << FormatUInt32CurrentCulture((attr >> 2U) & 0x1U) << '\n';
        std::cout << "light4: " << FormatUInt32CurrentCulture((attr >> 3U) & 0x1U) << '\n';
        std::cout << "mode: " << FormatUInt32CurrentCulture((attr >> 4U) & 0x2U) << '\n';
        std::cout << "back: " << FormatUInt32CurrentCulture((attr >> 6U) & 0x1U) << '\n';
        std::cout << "front: " << FormatUInt32CurrentCulture((attr >> 7U) & 0x1U) << '\n';
        std::cout << "clear: " << FormatUInt32CurrentCulture((attr >> 11U) & 0x1U) << '\n';
        std::cout << "far: " << FormatUInt32CurrentCulture((attr >> 12U) & 0x1U) << '\n';
        std::cout << "1dot: " << FormatUInt32CurrentCulture((attr >> 13U) & 0x1U) << '\n';
        std::cout << "depth: " << FormatUInt32CurrentCulture((attr >> 14U) & 0x1U) << '\n';
        std::cout << "fog: " << FormatUInt32CurrentCulture((attr >> 15U) & 0x1U) << '\n';
        std::cout << "alpha: " << FormatUInt32CurrentCulture((attr >> 16U) & 0x1FU) << '\n';
        std::cout << "id: " << FormatUInt32CurrentCulture((attr >> 24U) & 0x3FU) << '\n';
        std::cout << '\n';
    }

    OpenTK::Mathematics::Vector3 TestPrint::LightCalc(
        OpenTK::Mathematics::Vector3 light_vec,
        OpenTK::Mathematics::Vector3 light_col,
        OpenTK::Mathematics::Vector3 normal_vec,
        OpenTK::Mathematics::Vector3 dif_col,
        OpenTK::Mathematics::Vector3 amb_col,
        OpenTK::Mathematics::Vector3 spe_col)
    {
        using OpenTK::Mathematics::Vector3;

        const Vector3 sight_vec(0.0F, 0.0F, -1.0F);
        const float dif_factor = ManagedMax(
            0.0F, -Vector3::Dot(light_vec, normal_vec));
        const Vector3 half_vec(
            (light_vec.X + sight_vec.X) / 2.0F,
            (light_vec.Y + sight_vec.Y) / 2.0F,
            (light_vec.Z + sight_vec.Z) / 2.0F);
        float spe_factor = ManagedMax(
            0.0F,
            Vector3::Dot(
                Vector3(-half_vec.X, -half_vec.Y, -half_vec.Z), normal_vec));
        spe_factor *= spe_factor;

        const Vector3 spe_out(
            spe_col.X * light_col.X * spe_factor,
            spe_col.Y * light_col.Y * spe_factor,
            spe_col.Z * light_col.Z * spe_factor);
        const Vector3 dif_out(
            dif_col.X * light_col.X * dif_factor,
            dif_col.Y * light_col.Y * dif_factor,
            dif_col.Z * light_col.Z * dif_factor);
        const Vector3 amb_out(
            amb_col.X * light_col.X,
            amb_col.Y * light_col.Y,
            amb_col.Z * light_col.Z);

        return Vector3(
            (spe_out.X + dif_out.X) + amb_out.X,
            (spe_out.Y + dif_out.Y) + amb_out.Y,
            (spe_out.Z + dif_out.Z) + amb_out.Z);
    }

    // PrintEntityEditor is intentionally not defined here. TestPrint.cs requires the
    // CLR System.Type / PropertyInfo metadata contract, and no shared Native reflection
    // owner exists yet. The public declaration is preserved without a pair-local fake facade.

    void TestPrint::ParseStruct(
        const std::optional<std::string>& className,
        const std::optional<std::string>& baseClass,
        const std::optional<std::string>& data)
    {
#if !defined(DEBUG)
        (void)className;
#endif
        if (IsNullOrWhiteSpace(data))
        {
            return;
        }

        std::int32_t index = 0;
        std::int32_t offset = 0;
        if (baseClass && *baseClass == "CEntity")
        {
            offset = 0x18;
        }
        else if (baseClass && *baseClass == "CEnemyBase")
        {
            offset = 0x170;
        }

        const std::unordered_map<std::string, std::string> byteEnums{
            {"ENEMY_TYPE", "EnemyType"},
            {"HUNTER", "Hunter"},
            {"GAME_MODE", "GameMode"}
        };
        const std::unordered_map<std::string, std::string> ushortEnums{
            {"ITEM_TYPE", "ItemType"}
        };
        const std::unordered_map<std::string, std::string> uintEnums{
            {"EVENT_TYPE", "Message"},
            {"DOOR_TYPE", "DoorType"},
            {"COLLISION_VOLUME_TYPE", "VolumeType"}
        };

        if (baseClass)
        {
            MPH_TESTPRINT_DEBUG_WRITE_LINE("public class " + InterpolationString(className)
                + " : " + *baseClass);
        }
        else
        {
            MPH_TESTPRINT_DEBUG_WRITE_LINE("public class " + InterpolationString(className)
                + " : MemoryClass");
        }
        MPH_TESTPRINT_DEBUG_WRITE_LINE("    {");

        std::vector<std::string> news;
        for (const std::string& line : SplitString(RequireString(data), EnvironmentNewLine()))
        {
            std::string normalized = TrimManaged(line);
            normalized = ReplaceAll(std::move(normalized), "signed ", "signed");
            normalized = ReplaceAll(std::move(normalized), " *", "* ");
            normalized = ReplaceAll(std::move(normalized), ";", "");
            std::vector<std::string> split = SplitChar(normalized, ' ');
            MPH_TESTPRINT_DEBUG_ASSERT(split.size() == 2);

            std::string name;
            for (const std::string& part : SplitChar(ManagedAt(split, 1), '_'))
            {
                name += UpperFirstInvariant(part);
            }

            std::string comment;
            std::string type;
            std::string getter;
            std::string setter;
            std::int32_t size = 0;
            bool enums = false;
            bool embed = false;
            std::string cast;
            const std::string& sourceType = ManagedAt(split, 0);

            if (sourceType.find('*') != std::string::npos)
            {
                type = "IntPtr";
                getter = "ReadPointer";
                setter = "WritePointer";
                size = 4;
                comment = " // " + sourceType;
            }
            else if (sourceType == "EntityPtrUnion")
            {
                type = "IntPtr";
                getter = "ReadPointer";
                setter = "WritePointer";
                size = 4;
                comment = " // CEntity*";
            }
            else if (sourceType == "EntityIdOrRef")
            {
                type = "IntPtr";
                getter = "ReadPointer";
                setter = "WritePointer";
                size = 4;
                comment = " // EntityIdOrRef";
            }
            else if (sourceType == "int")
            {
                type = "int";
                getter = "ReadInt32";
                setter = "WriteInt32";
                size = 4;
            }
            else if (sourceType == "unsignedint")
            {
                type = "uint";
                getter = "ReadUInt32";
                setter = "WriteUInt32";
                size = 4;
            }
            else if (sourceType == "signed__int16")
            {
                type = "short";
                getter = "ReadInt16";
                setter = "WriteInt16";
                size = 2;
            }
            else if (sourceType == "__int16" || sourceType == "unsigned__int16")
            {
                type = "ushort";
                getter = "ReadUInt16";
                setter = "WriteUInt16";
                size = 2;
            }
            else if (sourceType == "char" || sourceType == "__int8"
                || sourceType == "unsigned__int8")
            {
                type = "byte";
                getter = "ReadByte";
                setter = "WriteByte";
                size = 1;
            }
            else if (sourceType == "signed__int8")
            {
                type = "sbyte";
                getter = "ReadSByte";
                setter = "WriteSByte";
                size = 1;
            }
            else if (sourceType == "Color3")
            {
                type = "ColorRgb";
                getter = "ReadColor3";
                setter = "WriteColor3";
                size = 3;
            }
            else if (sourceType == "VecFx32")
            {
                type = "Vector3";
                getter = "ReadVec3";
                setter = "WriteVec3";
                size = 12;
            }
            else if (sourceType == "Vec4")
            {
                type = "Vector4";
                getter = "ReadVec4";
                setter = "WriteVec4";
                size = 16;
            }
            else if (sourceType == "MtxFx43")
            {
                type = "Matrix4x3";
                getter = "ReadMtx43";
                setter = "WriteMtx43";
                size = 48;
            }
            else if (sourceType == "RoomState")
            {
                type = "RoomState";
                size = 60;
                embed = true;
            }
            else if (sourceType == "CModel")
            {
                type = "CModel";
                size = 0x48;
                embed = true;
            }
            else if (sourceType == "BeamInfo")
            {
                type = "BeamInfo";
                size = 0x14;
                embed = true;
            }
            else if (sourceType == "EntityCollision")
            {
                type = "EntityCollision";
                size = 0xB4;
                embed = true;
            }
            else if (sourceType == "SFXParameters")
            {
                type = "SfxParameters";
                size = 4;
                embed = true;
            }
            else if (sourceType == "CollisionVolume")
            {
                type = "CollisionVolume";
                size = 0x40;
                embed = true;
            }
            else if (sourceType == "Light")
            {
                type = "Light";
                size = 0xF;
                embed = true;
            }
            else if (sourceType == "LightInfo")
            {
                type = "LightInfo";
                size = 0x1F;
                embed = true;
            }
            else if (sourceType == "CameraInfo")
            {
                type = "CameraInfo";
                size = 0x11C;
                embed = true;
            }
            else if (sourceType == "PlayerControls")
            {
                type = "PlayerControls";
                size = 0x9C;
                embed = true;
            }
            else if (sourceType == "ButtonControlUnion")
            {
                type = "ButtonControlUnion";
                size = 4;
                embed = true;
            }
            else if (sourceType == "PlayerInput")
            {
                type = "PlayerInput";
                size = 0x48;
                embed = true;
            }
            else if (sourceType == "CBeamProjectile")
            {
                type = "CBeamProjectile";
                size = 0x158;
                embed = true;
            }
            else if (sourceType == "EquipInfo")
            {
                type = "EquipInfo";
                size = 0x14;
                embed = true;
            }
            else if (sourceType == "AIButton")
            {
                type = "AiButton";
                size = 6;
                embed = true;
            }
            else if (const auto found = byteEnums.find(sourceType); found != byteEnums.end())
            {
                type = found->second;
                getter = "ReadByte";
                setter = "WriteByte";
                size = 1;
                enums = true;
                cast = "byte";
            }
            else if (const auto found = ushortEnums.find(sourceType); found != ushortEnums.end())
            {
                type = found->second;
                getter = "ReadUInt16";
                setter = "WriteUInt16";
                size = 2;
                enums = true;
                cast = "ushort";
            }
            else if (const auto found = uintEnums.find(sourceType); found != uintEnums.end())
            {
                type = found->second;
                getter = "ReadUInt32";
                setter = "WriteUInt32";
                size = 4;
                enums = true;
                cast = "uint";
            }
            else
            {
                type = sourceType;
                getter = "Read";
                setter = "Write";
                size = 4;
                embed = true;
                DebuggerBreakFallback();
            }

            std::int32_t number = 0;
            const bool array = name.find('[') != std::string::npos;
            std::string param;
            if (array)
            {
                split = SplitChar(name, '[');
                const std::vector<std::string> closeSplit =
                    SplitChar(ManagedAt(split, 1), ']');
                number = ParseInt32CurrentCultureFallback(ManagedAt(closeSplit, 0));
                MPH_TESTPRINT_DEBUG_ASSERT(number > 1);
                name = ManagedAt(split, 0);
                if (comment.empty())
                {
                    comment = " // " + type;
                }
                comment += "[" + FormatInt32CurrentCulture(number) + "]";
                if (embed)
                {
                    param = "," + std::string(EnvironmentNewLine())
                        + "                " + FormatInt32CurrentCulture(size)
                        + ", (Memory m, int a) => new " + type + "(m, a)";
                    type = "StructArray<" + type + ">";
                }
                else if (enums && size == 1)
                {
                    type = "U8EnumArray<" + type + ">";
                }
                else if (enums && size == 2)
                {
                    type = "U16EnumArray<" + type + ">";
                }
                else if (enums && size == 4)
                {
                    type = "U32EnumArray<" + type + ">";
                }
                else
                {
                    type = ReplaceAll(getter, "Read", "");
                    type = ReplaceAll(std::move(type), "Pointer", "IntPtr") + "Array";
                }
                size = MultiplyInt32(size, number);
            }

            MPH_TESTPRINT_DEBUG_WRITE_LINE("        private const int _off" + FormatInt32CurrentCulture(index)
                + " = 0x" + FormatHexInt32(offset, 1) + ";" + comment);
            if (array)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name + " { get; }");
                news.push_back("            " + name + " = new " + type
                    + "(memory, address + _off" + FormatInt32CurrentCulture(index) + ", "
                    + FormatInt32CurrentCulture(number) + param + ");");
            }
            else if (embed)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name + " { get; }");
                news.push_back("            " + name + " = new " + type
                    + "(memory, address + _off" + FormatInt32CurrentCulture(index) + ");");
            }
            else if (enums)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name
                    + " { get => (" + type + ")" + getter + "(_off"
                    + FormatInt32CurrentCulture(index) + "); set => " + setter + "(_off"
                    + FormatInt32CurrentCulture(index) + ", (" + cast + ")value); }");
            }
            else
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name
                    + " { get => " + getter + "(_off" + FormatInt32CurrentCulture(index)
                    + "); set => " + setter + "(_off" + FormatInt32CurrentCulture(index)
                    + ", value); }");
            }
            MPH_TESTPRINT_DEBUG_WRITE_LINE("");
            index = AddInt32(index, 1);
            offset = AddInt32(offset, size);
        }

        MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + InterpolationString(className)
            + "(Memory memory, int address) : base(memory, address)");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        {");
        for (const std::string& line : news)
        {
#if !defined(DEBUG)
            (void)line;
#endif
            MPH_TESTPRINT_DEBUG_WRITE_LINE(line);
        }
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        }");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + InterpolationString(className)
            + "(Memory memory, IntPtr address) : base(memory, address)");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        {");
        for (const std::string& line : news)
        {
#if !defined(DEBUG)
            (void)line;
#endif
            MPH_TESTPRINT_DEBUG_WRITE_LINE(line);
        }
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        }");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("    }");
        DebuggerBreakFallback();
    }

    void TestPrint::Nop() noexcept
    {
    }
}

#undef MPH_TESTPRINT_DEBUG_WRITE_LINE
#undef MPH_TESTPRINT_DEBUG_ASSERT
