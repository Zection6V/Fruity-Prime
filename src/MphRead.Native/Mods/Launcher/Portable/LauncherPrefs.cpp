#include "LauncherPrefs.hpp"

#include "../../../Formats/Enums.hpp"
#include "../../Branding.hpp"
#include "../../Network/NetMaster.hpp"
#include "../../Network/NetProtocol.hpp"
#include "../../Network/PlayerColors.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdlib.h>
#include <sys/stat.h>
#elif defined(__FreeBSD__)
#include <limits.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#elif defined(__OpenBSD__)
#include <stdlib.h>
#include <sys/stat.h>
#elif defined(__sun)
#include <stdlib.h>
#include <sys/stat.h>
#elif defined(__linux__)
#include <sys/auxv.h>
#include <stdlib.h>
#include <sys/stat.h>
#elif defined(__unix__)
#include <stdlib.h>
#include <sys/stat.h>
#endif

namespace MphRead::Mods::Launcher::Detail
{
    [[nodiscard]] MphRead::Mods::WindowStartMode LauncherPrefsWindowModeParse(
        std::string_view value, MphRead::Mods::WindowStartMode fallback);
}

namespace
{
    struct Utf8CodePoint final
    {
        std::uint32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] std::optional<Utf8CodePoint> DecodeUtf8Forward(
        std::string_view text, std::size_t position) noexcept
    {
        if (position >= text.size())
        {
            return std::nullopt;
        }

        const auto first = static_cast<unsigned char>(text[position]);
        if (first <= 0x7FU)
        {
            return Utf8CodePoint{first, 1};
        }

        std::uint32_t value = 0;
        std::size_t length = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            value = first & 0x1FU;
            length = 2;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            value = first & 0x0FU;
            length = 3;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            value = first & 0x07U;
            length = 4;
            minimum = 0x10000U;
        }
        else
        {
            return std::nullopt;
        }

        if (position + length > text.size())
        {
            return std::nullopt;
        }
        for (std::size_t index = 1; index < length; ++index)
        {
            const auto next = static_cast<unsigned char>(text[position + index]);
            if ((next & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }
            value = (value << 6) | (next & 0x3FU);
        }

        if (value < minimum || value > 0x10FFFFU
            || (value >= 0xD800U && value <= 0xDFFFU))
        {
            return std::nullopt;
        }
        return Utf8CodePoint{value, length};
    }

    [[nodiscard]] std::optional<std::pair<Utf8CodePoint, std::size_t>>
        DecodeUtf8Backward(std::string_view text, std::size_t end) noexcept
    {
        if (end == 0 || end > text.size())
        {
            return std::nullopt;
        }

        std::size_t start = end - 1;
        while (start > 0
            && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
        {
            --start;
        }

        const std::optional<Utf8CodePoint> decoded = DecodeUtf8Forward(text, start);
        if (!decoded.has_value() || start + decoded->Length != end)
        {
            return std::nullopt;
        }
        return std::make_pair(*decoded, start);
    }

    [[nodiscard]] bool IsDotNetWhitespace(std::uint32_t value) noexcept
    {
        if (value >= 0x0009U && value <= 0x000DU)
        {
            return true;
        }

        switch (value)
        {
        case 0x0020U:
        case 0x0085U:
        case 0x00A0U:
        case 0x1680U:
        case 0x2000U:
        case 0x2001U:
        case 0x2002U:
        case 0x2003U:
        case 0x2004U:
        case 0x2005U:
        case 0x2006U:
        case 0x2007U:
        case 0x2008U:
        case 0x2009U:
        case 0x200AU:
        case 0x2028U:
        case 0x2029U:
        case 0x202FU:
        case 0x205FU:
        case 0x3000U:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::string_view TrimDotNetWhitespace(
        std::string_view value) noexcept
    {
        std::size_t first = 0;
        std::size_t last = value.size();

        while (first < last)
        {
            const std::optional<Utf8CodePoint> decoded
                = DecodeUtf8Forward(value.substr(0, last), first);
            if (!decoded.has_value() || !IsDotNetWhitespace(decoded->Value))
            {
                break;
            }
            first += decoded->Length;
        }

        while (last > first)
        {
            const auto decoded = DecodeUtf8Backward(value, last);
            if (!decoded.has_value()
                || !IsDotNetWhitespace(decoded->first.Value))
            {
                break;
            }
            last = decoded->second;
        }

        return value.substr(first, last - first);
    }

    [[nodiscard]] std::string_view TrimBooleanInput(
        std::string_view value) noexcept
    {
        for (;;)
        {
            const std::string_view trimmed = TrimDotNetWhitespace(value);
            if (trimmed.data() != value.data() || trimmed.size() != value.size())
            {
                value = trimmed;
                continue;
            }
            if (!value.empty() && value.front() == '\0')
            {
                value.remove_prefix(1);
                continue;
            }
            if (!value.empty() && value.back() == '\0')
            {
                value.remove_suffix(1);
                continue;
            }
            return value;
        }
    }

    [[nodiscard]] char AsciiLower(char value) noexcept
    {
        return value >= 'A' && value <= 'Z'
            ? static_cast<char>(value + ('a' - 'A'))
            : value;
    }

    [[nodiscard]] bool EqualsIgnoreCase(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (AsciiLower(left[index]) != AsciiLower(right[index]))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool TryParseBoolean(
        std::string_view value, bool& result) noexcept
    {
        value = TrimBooleanInput(value);
        if (EqualsIgnoreCase(value, "true"))
        {
            result = true;
            return true;
        }
        if (EqualsIgnoreCase(value, "false"))
        {
            result = false;
            return true;
        }
        result = false;
        return false;
    }

    [[nodiscard]] bool IsIntegerWhitespace(char value) noexcept
    {
        const unsigned char character = static_cast<unsigned char>(value);
        return character == 0x20U
            || (character >= 0x09U && character <= 0x0DU);
    }

    [[nodiscard]] bool HasValidIntegerTrailingCharacters(
        std::string_view value, std::size_t index) noexcept
    {
        while (index < value.size() && IsIntegerWhitespace(value[index]))
        {
            ++index;
        }
        while (index < value.size() && value[index] == '\0')
        {
            ++index;
        }
        return index == value.size();
    }

    [[nodiscard]] bool TryParseInt32(
        std::string_view value, std::int32_t& result) noexcept
    {
        value = TrimDotNetWhitespace(value);
        if (value.empty())
        {
            result = 0;
            return false;
        }

        bool negative = false;
        std::size_t index = 0;
        if (value[index] == '+' || value[index] == '-')
        {
            negative = value[index] == '-';
            ++index;
        }
        if (index == value.size())
        {
            result = 0;
            return false;
        }

        constexpr std::uint64_t PositiveLimit
            = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
        constexpr std::uint64_t NegativeLimit = PositiveLimit + 1U;
        const std::uint64_t limit = negative ? NegativeLimit : PositiveLimit;

        std::uint64_t parsed = 0;
        bool hasDigit = false;
        for (; index < value.size(); ++index)
        {
            const char character = value[index];
            if (character < '0' || character > '9')
            {
                if (!hasDigit || !HasValidIntegerTrailingCharacters(value, index))
                {
                    result = 0;
                    return false;
                }
                break;
            }
            hasDigit = true;
            const std::uint64_t digit
                = static_cast<std::uint64_t>(character - '0');
            if (parsed > (limit - digit) / 10U)
            {
                result = 0;
                return false;
            }
            parsed = parsed * 10U + digit;
        }
        if (!hasDigit)
        {
            result = 0;
            return false;
        }

        if (!negative)
        {
            result = static_cast<std::int32_t>(parsed);
        }
        else if (parsed == NegativeLimit)
        {
            result = std::numeric_limits<std::int32_t>::min();
        }
        else
        {
            result = -static_cast<std::int32_t>(parsed);
        }
        return true;
    }

    [[nodiscard]] bool TryParseByteNumeric(
        std::string_view value, std::uint8_t& result) noexcept
    {
        value = TrimDotNetWhitespace(value);
        if (value.empty())
        {
            result = 0;
            return false;
        }

        bool negative = false;
        std::size_t index = 0;
        if (value[index] == '+' || value[index] == '-')
        {
            negative = value[index] == '-';
            ++index;
        }
        if (index == value.size())
        {
            result = 0;
            return false;
        }

        std::uint32_t parsed = 0;
        bool hasDigit = false;
        for (; index < value.size(); ++index)
        {
            const char character = value[index];
            if (character < '0' || character > '9')
            {
                if (!hasDigit || !HasValidIntegerTrailingCharacters(value, index))
                {
                    result = 0;
                    return false;
                }
                break;
            }
            hasDigit = true;
            const std::uint32_t digit
                = static_cast<std::uint32_t>(character - '0');
            if (parsed > (255U - digit) / 10U)
            {
                result = 0;
                return false;
            }
            parsed = parsed * 10U + digit;
        }
        if (!hasDigit)
        {
            result = 0;
            return false;
        }

        if (negative && parsed != 0)
        {
            result = 0;
            return false;
        }
        result = static_cast<std::uint8_t>(parsed);
        return true;
    }

    struct HunterName final
    {
        std::string_view Name;
        std::uint8_t Value;
    };

    constexpr std::array<HunterName, 9> HunterNames{{
        {"Samus", 0},
        {"Kanden", 1},
        {"Trace", 2},
        {"Sylux", 3},
        {"Noxus", 4},
        {"Spire", 5},
        {"Weavel", 6},
        {"Guardian", 7},
        {"Random", 8}
    }};

    [[nodiscard]] bool TryParseHunter(
        std::string_view value, MphRead::Hunter& result) noexcept
    {
        value = TrimDotNetWhitespace(value);
        if (value.empty())
        {
            result = static_cast<MphRead::Hunter>(0);
            return false;
        }

        const char first = value.front();
        if ((first >= '0' && first <= '9') || first == '+' || first == '-')
        {
            std::uint8_t numeric = 0;
            if (TryParseByteNumeric(value, numeric))
            {
                result = static_cast<MphRead::Hunter>(numeric);
                return true;
            }
        }

        std::uint8_t combined = 0;
        std::size_t position = 0;
        for (;;)
        {
            const std::size_t comma = value.find(',', position);
            const std::string_view part = TrimDotNetWhitespace(
                comma == std::string_view::npos
                    ? value.substr(position)
                    : value.substr(position, comma - position));
            if (part.empty())
            {
                result = static_cast<MphRead::Hunter>(0);
                return false;
            }

            bool found = false;
            for (const HunterName& entry : HunterNames)
            {
                if (EqualsIgnoreCase(part, entry.Name))
                {
                    combined = static_cast<std::uint8_t>(combined | entry.Value);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                result = static_cast<MphRead::Hunter>(0);
                return false;
            }

            if (comma == std::string_view::npos)
            {
                result = static_cast<MphRead::Hunter>(combined);
                return true;
            }
            position = comma + 1;
        }
    }

    [[nodiscard]] std::string HunterToString(MphRead::Hunter hunter)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(hunter);
        for (const HunterName& entry : HunterNames)
        {
            if (entry.Value == value)
            {
                return std::string(entry.Name);
            }
        }

        std::array<char, 3> buffer{};
        const auto [end, error] = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Could not format Hunter.");
        }
        return std::string(buffer.data(), end);
    }

    [[nodiscard]] std::string Int32ToInvariant(std::int32_t value)
    {
        std::array<char, 12> buffer{};
        const auto [end, error] = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Could not format Int32.");
        }
        return std::string(buffer.data(), end);
    }

    void AppendUtf8(std::string& output, std::uint32_t value)
    {
        if (value > 0x10FFFFU
            || (value >= 0xD800U && value <= 0xDFFFU))
        {
            value = 0xFFFDU;
        }

        if (value <= 0x7FU)
        {
            output.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (value >> 6)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else if (value <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(
                static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (value >> 18)));
            output.push_back(
                static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
            output.push_back(
                static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::string DecodeUtf8Text(std::string_view bytes)
    {
        std::string output;
        output.reserve(bytes.size());

        for (std::size_t index = 0; index < bytes.size();)
        {
            const unsigned char first
                = static_cast<unsigned char>(bytes[index]);
            if (first <= 0x7FU)
            {
                output.push_back(static_cast<char>(first));
                ++index;
                continue;
            }

            std::size_t expected = 0;
            if (first >= 0xC2U && first <= 0xDFU)
            {
                expected = 2;
            }
            else if (first >= 0xE0U && first <= 0xEFU)
            {
                expected = 3;
            }
            else if (first >= 0xF0U && first <= 0xF4U)
            {
                expected = 4;
            }
            else
            {
                AppendUtf8(output, 0xFFFDU);
                ++index;
                continue;
            }

            std::size_t available = 1;
            while (available < expected
                && index + available < bytes.size()
                && (static_cast<unsigned char>(bytes[index + available])
                    & 0xC0U) == 0x80U)
            {
                ++available;
            }

            if (available < expected)
            {
                AppendUtf8(output, 0xFFFDU);
                index += available;
                continue;
            }

            const unsigned char second
                = static_cast<unsigned char>(bytes[index + 1]);
            if ((first == 0xE0U && second < 0xA0U)
                || (first == 0xEDU && second >= 0xA0U)
                || (first == 0xF0U && second < 0x90U)
                || (first == 0xF4U && second > 0x8FU))
            {
                AppendUtf8(output, 0xFFFDU);
                ++index;
                continue;
            }

            output.append(bytes.substr(index, expected));
            index += expected;
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf16Text(
        std::string_view bytes, bool bigEndian)
    {
        std::string output;
        output.reserve(bytes.size());

        const auto readUnit = [&](std::size_t index) -> std::uint16_t
        {
            const auto first = static_cast<unsigned char>(bytes[index]);
            const auto second = static_cast<unsigned char>(bytes[index + 1]);
            return bigEndian
                ? static_cast<std::uint16_t>((first << 8) | second)
                : static_cast<std::uint16_t>(first | (second << 8));
        };

        std::size_t index = 0;
        while (index + 1 < bytes.size())
        {
            const std::uint16_t first = readUnit(index);
            index += 2;

            if (first >= 0xD800U && first <= 0xDBFFU)
            {
                if (index + 1 < bytes.size())
                {
                    const std::uint16_t second = readUnit(index);
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        index += 2;
                        const std::uint32_t codePoint = 0x10000U
                            + ((static_cast<std::uint32_t>(first) - 0xD800U)
                                << 10)
                            + (static_cast<std::uint32_t>(second) - 0xDC00U);
                        AppendUtf8(output, codePoint);
                        continue;
                    }
                }
                AppendUtf8(output, 0xFFFDU);
            }
            else if (first >= 0xDC00U && first <= 0xDFFFU)
            {
                AppendUtf8(output, 0xFFFDU);
            }
            else
            {
                AppendUtf8(output, first);
            }
        }

        if (index < bytes.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf32Text(
        std::string_view bytes, bool bigEndian)
    {
        std::string output;
        output.reserve(bytes.size());

        std::size_t index = 0;
        while (index + 3 < bytes.size())
        {
            const auto b0 = static_cast<unsigned char>(bytes[index]);
            const auto b1 = static_cast<unsigned char>(bytes[index + 1]);
            const auto b2 = static_cast<unsigned char>(bytes[index + 2]);
            const auto b3 = static_cast<unsigned char>(bytes[index + 3]);
            index += 4;

            const std::uint32_t codePoint = bigEndian
                ? (static_cast<std::uint32_t>(b0) << 24)
                    | (static_cast<std::uint32_t>(b1) << 16)
                    | (static_cast<std::uint32_t>(b2) << 8)
                    | static_cast<std::uint32_t>(b3)
                : static_cast<std::uint32_t>(b0)
                    | (static_cast<std::uint32_t>(b1) << 8)
                    | (static_cast<std::uint32_t>(b2) << 16)
                    | (static_cast<std::uint32_t>(b3) << 24);
            AppendUtf8(output, codePoint);
        }

        if (index < bytes.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

#if defined(_WIN32)
    void AppendWtf8(std::string& output, std::uint32_t value)
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
        else if (value <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(
                static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (value >> 18)));
            output.push_back(
                static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
            output.push_back(
                static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::string Utf8FromWide(
        const wchar_t* value, std::size_t length)
    {
        static_assert(sizeof(wchar_t) == sizeof(std::uint16_t));

        std::string result;
        result.reserve(length);
        for (std::size_t index = 0; index < length; ++index)
        {
            const std::uint32_t first
                = static_cast<std::uint16_t>(value[index]);
            if (first >= 0xD800U && first <= 0xDBFFU && index + 1 < length)
            {
                const std::uint32_t second
                    = static_cast<std::uint16_t>(value[index + 1]);
                if (second >= 0xDC00U && second <= 0xDFFFU)
                {
                    const std::uint32_t codePoint = 0x10000U
                        + ((first - 0xD800U) << 10)
                        + (second - 0xDC00U);
                    AppendWtf8(result, codePoint);
                    ++index;
                    continue;
                }
            }
            AppendWtf8(result, first);
        }
        return result;
    }

    [[nodiscard]] std::wstring WideFromWtf8(std::string_view value)
    {
        std::wstring result;
        result.reserve(value.size());

        for (std::size_t index = 0; index < value.size();)
        {
            const unsigned char first
                = static_cast<unsigned char>(value[index]);
            if (first <= 0x7FU)
            {
                result.push_back(static_cast<wchar_t>(first));
                ++index;
                continue;
            }

            std::uint32_t codePoint = 0xFFFDU;
            std::size_t length = 1;
            if (first >= 0xC2U && first <= 0xDFU
                && index + 1 < value.size())
            {
                const unsigned char b1
                    = static_cast<unsigned char>(value[index + 1]);
                if ((b1 & 0xC0U) == 0x80U)
                {
                    codePoint = ((first & 0x1FU) << 6) | (b1 & 0x3FU);
                    length = 2;
                }
            }
            else if (first >= 0xE0U && first <= 0xEFU
                && index + 2 < value.size())
            {
                const unsigned char b1
                    = static_cast<unsigned char>(value[index + 1]);
                const unsigned char b2
                    = static_cast<unsigned char>(value[index + 2]);
                if ((b1 & 0xC0U) == 0x80U && (b2 & 0xC0U) == 0x80U
                    && !(first == 0xE0U && b1 < 0xA0U))
                {
                    codePoint = ((first & 0x0FU) << 12)
                        | ((b1 & 0x3FU) << 6)
                        | (b2 & 0x3FU);
                    length = 3;
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U
                && index + 3 < value.size())
            {
                const unsigned char b1
                    = static_cast<unsigned char>(value[index + 1]);
                const unsigned char b2
                    = static_cast<unsigned char>(value[index + 2]);
                const unsigned char b3
                    = static_cast<unsigned char>(value[index + 3]);
                if ((b1 & 0xC0U) == 0x80U
                    && (b2 & 0xC0U) == 0x80U
                    && (b3 & 0xC0U) == 0x80U
                    && !(first == 0xF0U && b1 < 0x90U)
                    && !(first == 0xF4U && b1 > 0x8FU))
                {
                    codePoint = ((first & 0x07U) << 18)
                        | ((b1 & 0x3FU) << 12)
                        | ((b2 & 0x3FU) << 6)
                        | (b3 & 0x3FU);
                    length = 4;
                }
            }

            index += length;
            if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<wchar_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000U;
                result.push_back(static_cast<wchar_t>(
                    0xD800U + (codePoint >> 10)));
                result.push_back(static_cast<wchar_t>(
                    0xDC00U + (codePoint & 0x3FFU)));
            }
        }
        return result;
    }
#endif

    [[nodiscard]] std::filesystem::path PathFromManagedString(
        std::string_view value)
    {
#if defined(_WIN32)
        return std::filesystem::path(WideFromWtf8(value));
#else
        return std::filesystem::path(value);
#endif
    }

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__sun) \
    || defined(__linux__) \
    || (defined(__unix__) && !defined(__EMSCRIPTEN__) && !defined(__wasi__))
    [[nodiscard]] std::optional<std::string> RealPath(const char* path)
    {
        std::unique_ptr<char, decltype(&std::free)> resolved(
            realpath(path, nullptr), &std::free);
        if (!resolved)
        {
            return std::nullopt;
        }
        return DecodeUtf8Text(resolved.get());
    }
#endif

    [[nodiscard]] std::optional<std::string> ProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }
            if (length < buffer.size())
            {
                return Utf8FromWide(buffer.data(), length);
            }
            if (buffer.size()
                > static_cast<std::size_t>(
                    std::numeric_limits<DWORD>::max()) / 2U)
            {
                return std::nullopt;
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 1;
        char probe = 0;
        if (_NSGetExecutablePath(&probe, &size) == 0)
        {
            return std::nullopt;
        }
        if (size == 0)
        {
            return std::nullopt;
        }
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            return std::nullopt;
        }
        return RealPath(buffer.data());
#elif defined(__FreeBSD__)
        static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        char path[PATH_MAX];
        std::size_t length = sizeof(path);
        if (sysctl(name, 4, path, &length, nullptr, 0) != 0 || length == 0)
        {
            return std::nullopt;
        }
        return DecodeUtf8Text(
            std::string_view(path, path[length - 1] == '\0'
                ? length - 1
                : length));
#elif defined(__OpenBSD__)
        return RealPath("/proc/curproc/exe");
#elif defined(__sun)
        const char* path = getexecname();
        return path == nullptr ? std::nullopt : RealPath(path);
#elif defined(__EMSCRIPTEN__) || defined(__wasi__)
        return std::nullopt;
#elif defined(__linux__)
        if (std::optional<std::string> path = RealPath("/proc/self/exe"))
        {
            return path;
        }
#if defined(AT_EXECFN)
        const auto executable
            = reinterpret_cast<const char*>(getauxval(AT_EXECFN));
        if (executable != nullptr)
        {
            return RealPath(executable);
        }
#endif
        return std::nullopt;
#elif defined(__unix__)
        return RealPath("/proc/curproc/exe");
#else
        return std::nullopt;
#endif
    }

    [[nodiscard]] std::string CurrentDirectoryWithSeparator()
    {
        const std::filesystem::path current = std::filesystem::current_path();
#if defined(_WIN32)
        std::string result = Utf8FromWide(
            current.native().data(), current.native().size());
        if (result.empty()
            || (result.back() != '\\' && result.back() != '/'))
        {
            result.push_back('\\');
        }
#else
        std::string result = DecodeUtf8Text(current.native());
        if (result.empty() || result.back() != '/')
        {
            result.push_back('/');
        }
#endif
        return result;
    }

    [[nodiscard]] std::string AppContextBaseDirectory()
    {
        const std::optional<std::string> processPath = ProcessPath();
        if (processPath.has_value())
        {
#if defined(_WIN32)
            const std::size_t separator = processPath->find_last_of("/\\");
#else
            const std::size_t separator = processPath->find_last_of('/');
#endif
            if (separator != std::string::npos)
            {
                return processPath->substr(0, separator + 1);
            }
        }
        return CurrentDirectoryWithSeparator();
    }

    [[nodiscard]] bool IsDirectorySeparator(char value) noexcept
    {
#if defined(_WIN32)
        return value == '\\' || value == '/';
#else
        return value == '/';
#endif
    }

    [[nodiscard]] std::string CombineLauncherPath(
        std::string_view directory)
    {
        constexpr std::string_view FileName = "launcher.txt";
        if (directory.empty())
        {
            return std::string(FileName);
        }

        std::string path(directory);
        if (!IsDirectorySeparator(path.back()))
        {
#if defined(_WIN32)
            path.push_back('\\');
#else
            path.push_back('/');
#endif
        }
        path.append(FileName);
        return path;
    }

    [[nodiscard]] bool FileExists(std::string_view path) noexcept
    {
        if (path.empty() || path.find('\0') != std::string_view::npos)
        {
            return false;
        }

        try
        {
            const std::filesystem::path nativePath
                = PathFromManagedString(path);
#if defined(_WIN32)
            const DWORD attributes = GetFileAttributesW(nativePath.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES
                && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
            struct stat info{};
            if (::lstat(nativePath.c_str(), &info) != 0)
            {
                return false;
            }
            if (S_ISLNK(info.st_mode))
            {
                struct stat target{};
                if (::stat(nativePath.c_str(), &target) != 0)
                {
                    return true;
                }
                return !S_ISDIR(target.st_mode);
            }
            return !S_ISDIR(info.st_mode);
#endif
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] std::string ReadAllText(std::string_view path)
    {
        std::ifstream stream(
            PathFromManagedString(path), std::ios::in | std::ios::binary);
        if (!stream.is_open())
        {
            throw std::ios_base::failure("Could not open launcher preferences.");
        }
        stream.exceptions(std::ios::badbit);

        const std::string bytes{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};

        const auto byteAt = [&](std::size_t index) -> unsigned char
        {
            return static_cast<unsigned char>(bytes[index]);
        };

        if (bytes.size() >= 4
            && byteAt(0) == 0xFFU && byteAt(1) == 0xFEU
            && byteAt(2) == 0x00U && byteAt(3) == 0x00U)
        {
            return DecodeUtf32Text(std::string_view(bytes).substr(4), false);
        }
        if (bytes.size() >= 4
            && byteAt(0) == 0x00U && byteAt(1) == 0x00U
            && byteAt(2) == 0xFEU && byteAt(3) == 0xFFU)
        {
            return DecodeUtf32Text(std::string_view(bytes).substr(4), true);
        }
        if (bytes.size() >= 3
            && byteAt(0) == 0xEFU && byteAt(1) == 0xBBU
            && byteAt(2) == 0xBFU)
        {
            return DecodeUtf8Text(std::string_view(bytes).substr(3));
        }
        if (bytes.size() >= 2
            && byteAt(0) == 0xFFU && byteAt(1) == 0xFEU)
        {
            return DecodeUtf16Text(std::string_view(bytes).substr(2), false);
        }
        if (bytes.size() >= 2
            && byteAt(0) == 0xFEU && byteAt(1) == 0xFFU)
        {
            return DecodeUtf16Text(std::string_view(bytes).substr(2), true);
        }
        return DecodeUtf8Text(bytes);
    }

    [[nodiscard]] std::vector<std::string> ReadAllLines(
        std::string_view path)
    {
        const std::string text = ReadAllText(path);
        std::vector<std::string> lines;

        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = start;
            while (end < text.size()
                && text[end] != '\r' && text[end] != '\n')
            {
                ++end;
            }
            lines.emplace_back(text.substr(start, end - start));

            if (end == text.size())
            {
                break;
            }
            if (text[end] == '\r'
                && end + 1 < text.size() && text[end + 1] == '\n')
            {
                start = end + 2;
            }
            else
            {
                start = end + 1;
            }
        }
        return lines;
    }

    void WriteAllLines(
        std::string_view path, const std::vector<std::string>& lines)
    {
        if (path.find('\0') != std::string_view::npos)
        {
            throw std::invalid_argument("Path contains a null character.");
        }

        std::ofstream stream(
            PathFromManagedString(path),
            std::ios::out | std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            throw std::ios_base::failure("Could not write launcher preferences.");
        }
        stream.exceptions(std::ios::badbit | std::ios::failbit);

#if defined(_WIN32)
        constexpr std::string_view NewLine = "\r\n";
#else
        constexpr std::string_view NewLine = "\n";
#endif

        for (const std::string& line : lines)
        {
            stream.write(line.data(), static_cast<std::streamsize>(line.size()));
            stream.write(
                NewLine.data(), static_cast<std::streamsize>(NewLine.size()));
        }
        stream.close();
    }
}

namespace MphRead::Mods::Launcher
{
    std::string LauncherPrefs::_directory = AppContextBaseDirectory();
    std::string LauncherPrefs::_serverAddress(LauncherPrefs::DefaultServer);
    std::int32_t LauncherPrefs::_serverPort = Network::NetConfig::DefaultPort;
    std::string LauncherPrefs::_masterHost = Network::NetMasterConfig::DefaultHost;
    std::int32_t LauncherPrefs::_masterPort = Network::NetMasterConfig::DefaultPort;
    std::int32_t LauncherPrefs::_lastRole = 0;
    std::string LauncherPrefs::_playerName = "Player";
    MphRead::Hunter LauncherPrefs::_lastHunter = static_cast<MphRead::Hunter>(0);
    std::int32_t LauncherPrefs::_lastColor = 0;
    std::int32_t LauncherPrefs::_bots = 3;
    std::int32_t LauncherPrefs::_botLevel = 1;
    std::int32_t LauncherPrefs::_hostPort = Network::NetConfig::DefaultPort;
    bool LauncherPrefs::_listHostedGame = true;
    bool LauncherPrefs::_hostOnMaster = true;
    std::int32_t LauncherPrefs::_lastKind = 0;
    bool LauncherPrefs::_autoUpdate = true;
    MphRead::Mods::WindowStartMode LauncherPrefs::_windowMode
        = static_cast<MphRead::Mods::WindowStartMode>(0);
    bool LauncherPrefs::_debugLogs = true;

    const std::string& LauncherPrefs::Directory() noexcept
    {
        return _directory;
    }

    void LauncherPrefs::Directory(std::string value)
    {
        _directory = std::move(value);
    }

    const std::string& LauncherPrefs::ServerAddress() noexcept
    {
        return _serverAddress;
    }

    void LauncherPrefs::ServerAddress(std::string value)
    {
        _serverAddress = std::move(value);
    }

    std::int32_t LauncherPrefs::ServerPort() noexcept
    {
        return _serverPort;
    }

    void LauncherPrefs::ServerPort(std::int32_t value) noexcept
    {
        _serverPort = value;
    }

    const std::string& LauncherPrefs::MasterHost() noexcept
    {
        return _masterHost;
    }

    void LauncherPrefs::MasterHost(std::string value)
    {
        _masterHost = std::move(value);
    }

    std::int32_t LauncherPrefs::MasterPort() noexcept
    {
        return _masterPort;
    }

    void LauncherPrefs::MasterPort(std::int32_t value) noexcept
    {
        _masterPort = value;
    }

    std::int32_t LauncherPrefs::LastRole() noexcept
    {
        return _lastRole;
    }

    void LauncherPrefs::LastRole(std::int32_t value) noexcept
    {
        _lastRole = value;
    }

    const std::string& LauncherPrefs::PlayerName() noexcept
    {
        return _playerName;
    }

    void LauncherPrefs::PlayerName(std::string value)
    {
        _playerName = std::move(value);
    }

    MphRead::Hunter LauncherPrefs::LastHunter() noexcept
    {
        return _lastHunter;
    }

    void LauncherPrefs::LastHunter(MphRead::Hunter value) noexcept
    {
        _lastHunter = value;
    }

    std::int32_t LauncherPrefs::LastColor() noexcept
    {
        return _lastColor;
    }

    void LauncherPrefs::LastColor(std::int32_t value) noexcept
    {
        _lastColor = value;
    }

    std::int32_t LauncherPrefs::Bots() noexcept
    {
        return _bots;
    }

    void LauncherPrefs::Bots(std::int32_t value) noexcept
    {
        _bots = value;
    }

    std::int32_t LauncherPrefs::BotLevel() noexcept
    {
        return _botLevel;
    }

    void LauncherPrefs::BotLevel(std::int32_t value) noexcept
    {
        _botLevel = value;
    }

    std::int32_t LauncherPrefs::HostPort() noexcept
    {
        return _hostPort;
    }

    void LauncherPrefs::HostPort(std::int32_t value) noexcept
    {
        _hostPort = value;
    }

    bool LauncherPrefs::ListHostedGame() noexcept
    {
        return _listHostedGame;
    }

    void LauncherPrefs::ListHostedGame(bool value) noexcept
    {
        _listHostedGame = value;
    }

    bool LauncherPrefs::HostOnMaster() noexcept
    {
        return _hostOnMaster;
    }

    void LauncherPrefs::HostOnMaster(bool value) noexcept
    {
        _hostOnMaster = value;
    }

    std::int32_t LauncherPrefs::LastKind() noexcept
    {
        return _lastKind;
    }

    void LauncherPrefs::LastKind(std::int32_t value) noexcept
    {
        _lastKind = value;
    }

    bool LauncherPrefs::AutoUpdate() noexcept
    {
        return _autoUpdate;
    }

    void LauncherPrefs::AutoUpdate(bool value) noexcept
    {
        _autoUpdate = value;
    }

    MphRead::Mods::WindowStartMode LauncherPrefs::WindowMode() noexcept
    {
        return _windowMode;
    }

    void LauncherPrefs::WindowMode(
        MphRead::Mods::WindowStartMode value) noexcept
    {
        _windowMode = value;
    }

    bool LauncherPrefs::DebugLogs() noexcept
    {
        return _debugLogs;
    }

    void LauncherPrefs::DebugLogs(bool value) noexcept
    {
        _debugLogs = value;
    }

    std::string LauncherPrefs::Path()
    {
        return CombineLauncherPath(_directory);
    }

    void LauncherPrefs::Load()
    {
        if (!FileExists(Path()))
        {
            return;
        }

        try
        {
            for (const std::string& raw : ReadAllLines(Path()))
            {
                const std::string_view line = TrimDotNetWhitespace(raw);
                const std::size_t split = line.find('=');
                if (line.empty() || line.front() == '#'
                    || split == std::string_view::npos || split == 0)
                {
                    continue;
                }

                const std::string_view key
                    = TrimDotNetWhitespace(line.substr(0, split));
                const std::string_view value
                    = TrimDotNetWhitespace(line.substr(split + 1));

                if (key == "server_address")
                {
                    _serverAddress.assign(value);
                }
                else if (key == "master_host")
                {
                    if (!value.empty())
                    {
                        _masterHost.assign(value);
                    }
                }
                else if (key == "master_port")
                {
                    std::int32_t masterPort = 0;
                    if (TryParseInt32(value, masterPort)
                        && masterPort > 0 && masterPort <= 65535)
                    {
                        _masterPort = masterPort;
                    }
                }
                else if (key == "server_port")
                {
                    std::int32_t port = 0;
                    if (TryParseInt32(value, port))
                    {
                        _serverPort = port;
                    }
                }
                else if (key == "player_name")
                {
                    if (!value.empty())
                    {
                        _playerName.assign(value);
                    }
                }
                else if (key == "last_role")
                {
                    std::int32_t role = 0;
                    if (TryParseInt32(value, role))
                    {
                        _lastRole = role;
                    }
                }
                else if (key == "hunter")
                {
                    MphRead::Hunter hunter = static_cast<MphRead::Hunter>(0);
                    if (TryParseHunter(value, hunter))
                    {
                        _lastHunter = hunter;
                    }
                }
                else if (key == "color")
                {
                    std::int32_t color = 0;
                    if (TryParseInt32(value, color))
                    {
                        _lastColor = Network::PlayerColors::Clamp(color);
                    }
                }
                else if (key == "bots")
                {
                    std::int32_t bots = 0;
                    if (TryParseInt32(value, bots))
                    {
                        _bots = bots;
                    }
                }
                else if (key == "bot_level")
                {
                    std::int32_t level = 0;
                    if (TryParseInt32(value, level))
                    {
                        _botLevel = level;
                    }
                }
                else if (key == "host_on_master")
                {
                    bool hostOnMaster = false;
                    if (TryParseBoolean(value, hostOnMaster))
                    {
                        _hostOnMaster = hostOnMaster;
                    }
                }
                else if (key == "list_hosted")
                {
                    bool listHosted = false;
                    if (TryParseBoolean(value, listHosted))
                    {
                        _listHostedGame = listHosted;
                    }
                }
                else if (key == "host_port")
                {
                    std::int32_t hostPort = 0;
                    if (TryParseInt32(value, hostPort))
                    {
                        _hostPort = hostPort;
                    }
                }
                else if (key == "window_mode")
                {
                    _windowMode = Detail::LauncherPrefsWindowModeParse(
                        value, _windowMode);
                }
                else if (key == "auto_update")
                {
                    bool autoUpdate = false;
                    if (TryParseBoolean(value, autoUpdate))
                    {
                        _autoUpdate = autoUpdate;
                    }
                }
                else if (key == "debug_logs")
                {
                    bool debugLogs = false;
                    if (TryParseBoolean(value, debugLogs))
                    {
                        _debugLogs = debugLogs;
                    }
                }
                else if (key == "last_kind")
                {
                    std::int32_t kind = 0;
                    if (TryParseInt32(value, kind))
                    {
                        _lastKind = kind;
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    void LauncherPrefs::Save()
    {
        try
        {
            const std::string path = Path();
            std::vector<std::string> lines;
            lines.reserve(18);
            lines.emplace_back(
                "# " + std::string(Branding::Name) + " launcher preferences.");
            lines.emplace_back("server_address=" + _serverAddress);
            lines.emplace_back(
                "server_port=" + Int32ToInvariant(_serverPort));
            lines.emplace_back("master_host=" + _masterHost);
            lines.emplace_back(
                "master_port=" + Int32ToInvariant(_masterPort));
            lines.emplace_back("last_role=" + Int32ToInvariant(_lastRole));
            lines.emplace_back("player_name=" + _playerName);
            lines.emplace_back("hunter=" + HunterToString(_lastHunter));
            lines.emplace_back("color=" + Int32ToInvariant(_lastColor));
            lines.emplace_back("bots=" + Int32ToInvariant(_bots));
            lines.emplace_back("bot_level=" + Int32ToInvariant(_botLevel));
            lines.emplace_back("host_port=" + Int32ToInvariant(_hostPort));
            lines.emplace_back(
                std::string("list_hosted=")
                    + (_listHostedGame ? "true" : "false"));
            lines.emplace_back(
                std::string("host_on_master=")
                    + (_hostOnMaster ? "true" : "false"));
            lines.emplace_back("last_kind=" + Int32ToInvariant(_lastKind));
            lines.emplace_back(
                std::string("auto_update=")
                    + (_autoUpdate ? "true" : "false"));
            lines.emplace_back(
                std::string("debug_logs=")
                    + (_debugLogs ? "true" : "false"));
            lines.emplace_back(
                std::string("window_mode=")
                    + (static_cast<std::int32_t>(_windowMode) == 1
                        ? "borderless"
                        : "windowed"));
            WriteAllLines(path, lines);
        }
        catch (...)
        {
        }
    }
}
