#include "Common.hpp"

#include <cassert>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
    [[nodiscard]] constexpr bool IsParseWhiteSpace(char16_t value) noexcept
    {
        return (value >= u'\u0009' && value <= u'\u000D') || value == u'\u0020';
    }

    [[nodiscard]] std::u16string ToU16(const std::string& value)
    {
        std::u16string result;
        result.reserve(value.size());
        for (unsigned char chr : value)
        {
            result.push_back(static_cast<char16_t>(chr));
        }
        return result;
    }
}

namespace NCSFCommon
{
    [[nodiscard]] static std::wstring WidenCodeUnits(std::u16string_view value);
    [[nodiscard]] static std::vector<std::u16string_view> SplitPreserveEmpty(
        std::u16string_view value, char16_t separator);
    [[nodiscard]] static std::int32_t ParseInt32(std::u16string_view value);
    [[nodiscard]] static std::int32_t AddUnchecked(std::int32_t left, std::int32_t right) noexcept;
    [[nodiscard]] static std::int32_t MultiplyUnchecked(std::int32_t left, std::int32_t right) noexcept;
    [[nodiscard]] static std::int32_t FloatToInt32Unchecked(float value) noexcept;
    [[nodiscard]] static std::u16string FormatIntD2(std::int32_t value);
    [[nodiscard]] static std::u16string FormatSeconds(float value);

    const std::array<std::uint8_t, 4> Common::DataBytes = {
        static_cast<std::uint8_t>('D'),
        static_cast<std::uint8_t>('A'),
        static_cast<std::uint8_t>('T'),
        static_cast<std::uint8_t>('A')
    };

    std::u16string Common::ReadNullTerminatedString(std::span<const std::uint8_t> span)
    {
        std::u16string chars;
        std::size_t pos = 0;
        char16_t chr;
        do
        {
            if (pos >= span.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            chr = static_cast<char16_t>(span[pos++]);
            if (chr != u'\0')
            {
                chars.push_back(chr);
            }
        } while (chr != u'\0');
        return chars;
    }

    void Common::WriteNullTerminatedString(std::span<std::uint8_t> span, std::u16string_view str)
    {
        std::size_t pos = 0;
        for (char16_t chr : str)
        {
            if (pos >= span.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            span[pos++] = static_cast<std::uint8_t>(chr);
        }
        if (pos >= span.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        span[pos] = 0;
    }

    bool Common::VerifyHeader(
        std::span<const std::uint8_t> actual, std::span<const std::uint8_t> expected)
    {
        return actual.size() == expected.size()
            && std::equal(actual.begin(), actual.end(), expected.begin());
    }

    static std::wstring WidenCodeUnits(std::u16string_view value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (char16_t chr : value)
        {
            result.push_back(static_cast<wchar_t>(chr));
        }
        return result;
    }

    std::wregex Common::WildcardStringToRegex(std::u16string_view wildcard)
    {
        std::wstring pattern;
        pattern.reserve(wildcard.size() * 2 + 2);
        pattern.push_back(L'^');
        for (char16_t chr : wildcard)
        {
            if (chr == u'?')
            {
                pattern.push_back(L'.');
            }
            else if (chr == u'*')
            {
                pattern.push_back(L'.');
                pattern.push_back(L'*');
            }
            else
            {
                pattern.push_back(static_cast<wchar_t>(chr));
            }
        }
        pattern.push_back(L'$');
        return std::wregex(pattern, std::regex_constants::ECMAScript | std::regex_constants::optimize);
    }

    Common::KeepInfo::KeepInfo(std::u16string filename, KeepType keep)
        : Filename(std::move(filename)),
          Keep(keep)
    {
    }

    static std::vector<std::u16string_view> SplitPreserveEmpty(
        std::u16string_view value, char16_t separator)
    {
        std::vector<std::u16string_view> parts;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t pos = value.find(separator, start);
            if (pos == std::u16string_view::npos)
            {
                parts.emplace_back(value.substr(start));
                break;
            }
            parts.emplace_back(value.substr(start, pos - start));
            start = pos + 1;
        }
        return parts;
    }

    Common::KeepType Common::IncludeFilename(
        std::u16string_view filename,
        std::u16string_view sdatNumber,
        const std::vector<std::shared_ptr<KeepInfo>>& includesAndExcludes)
    {
        KeepType keep = KeepType::Neither;
        for (const std::shared_ptr<KeepInfo>& info : includesAndExcludes)
        {
            if (!info)
            {
                throw std::runtime_error("Object reference not set to an instance of an object.");
            }

            const std::vector<std::u16string_view> parts = SplitPreserveEmpty(info->Filename, u'/');
            assert(parts.size() <= 2);
            if (parts.size() == 2)
            {
                const std::wregex sdatRegex = WildcardStringToRegex(parts[0]);
                if (std::regex_search(WidenCodeUnits(sdatNumber), sdatRegex))
                {
                    const std::wregex filenameRegex = WildcardStringToRegex(parts[1]);
                    if (std::regex_search(WidenCodeUnits(filename), filenameRegex))
                    {
                        keep = info->Keep;
                    }
                }
            }
            else
            {
                const std::wregex filenameRegex = WildcardStringToRegex(info->Filename);
                if (std::regex_search(WidenCodeUnits(filename), filenameRegex))
                {
                    keep = info->Keep;
                }
            }
        }
        return keep;
    }

    static std::int32_t FloatToInt32Unchecked(float value) noexcept
    {
        if (!std::isfinite(value)
            || value < static_cast<float>(std::numeric_limits<std::int32_t>::min())
            || value >= 2147483648.0F)
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(value);
    }

    static std::u16string FormatIntD2(std::int32_t value)
    {
        const bool negative = value < 0;
        const std::uint32_t magnitude = negative
            ? static_cast<std::uint32_t>(-(static_cast<std::int64_t>(value)))
            : static_cast<std::uint32_t>(value);

        std::string digits = std::to_string(magnitude);
        if (digits.size() < 2)
        {
            digits.insert(digits.begin(), 2 - digits.size(), '0');
        }
        if (negative)
        {
            digits.insert(digits.begin(), '-');
        }
        return ToU16(digits);
    }

    static std::u16string FormatSeconds(float value)
    {
        if (std::isnan(value))
        {
            return u"NaN";
        }
        if (std::isinf(value))
        {
            return value < 0.0F ? u"-Infinity" : u"Infinity";
        }

        const bool negative = std::signbit(value);
        const long double magnitude = std::fabs(static_cast<long double>(value));
        const long double rounded = std::round(magnitude * 10000.0L) / 10000.0L;

        std::ostringstream stream;
        stream << std::fixed << std::setprecision(4) << rounded;
        std::string text = stream.str();

        const std::size_t decimal = text.find('.');
        if (decimal != std::string::npos)
        {
            while (!text.empty() && text.back() == '0')
            {
                text.pop_back();
            }
            if (!text.empty() && text.back() == '.')
            {
                text.pop_back();
            }
        }

        const std::size_t integerEnd = text.find('.');
        const std::size_t integerDigits = integerEnd == std::string::npos ? text.size() : integerEnd;
        if (integerDigits == 1 && text[0] != '0')
        {
            text.insert(text.begin(), '0');
        }
        if (negative)
        {
            text.insert(text.begin(), '-');
        }
        return ToU16(text);
    }

    std::u16string Common::SecondsToString(float seconds)
    {
        const std::int32_t minutes = FloatToInt32Unchecked(seconds / 60.0F);
        seconds -= static_cast<float>(MultiplyUnchecked(minutes, 60));

        std::u16string result = FormatIntD2(minutes);
        result.push_back(u':');
        result += FormatSeconds(seconds);
        return result;
    }

    static std::int32_t ParseInt32(std::u16string_view value)
    {
        std::size_t first = 0;
        while (first < value.size() && IsParseWhiteSpace(value[first]))
        {
            ++first;
        }

        std::size_t last = value.size();
        while (last > first && IsParseWhiteSpace(value[last - 1]))
        {
            --last;
        }

        if (first == last)
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        bool negative = false;
        if (value[first] == u'+' || value[first] == u'-')
        {
            negative = value[first] == u'-';
            ++first;
        }
        if (first == last)
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        std::uint64_t magnitude = 0;
        const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
        for (std::size_t index = first; index < last; ++index)
        {
            const char16_t chr = value[index];
            if (chr < u'0' || chr > u'9')
            {
                throw std::invalid_argument("Input string was not in a correct format.");
            }
            const std::uint64_t digit = static_cast<std::uint64_t>(chr - u'0');
            if (magnitude > (limit - digit) / 10)
            {
                throw std::out_of_range("Value was either too large or too small for an Int32.");
            }
            magnitude = magnitude * 10 + digit;
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

    static std::int32_t AddUnchecked(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    static std::int32_t MultiplyUnchecked(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    std::int32_t Common::StringToMS(std::u16string_view time)
    {
        std::int32_t colons = 0;
        for (char16_t chr : time)
        {
            if (chr == u':')
            {
                ++colons;
            }
        }
        assert(colons <= 2);

        std::int32_t seconds;
        if (colons == 1)
        {
            const std::vector<std::u16string_view> ranges = SplitPreserveEmpty(time, u':');
            seconds = AddUnchecked(MultiplyUnchecked(ParseInt32(ranges[0]), 60), ParseInt32(ranges[1]));
        }
        else if (colons == 2)
        {
            const std::vector<std::u16string_view> ranges = SplitPreserveEmpty(time, u':');
            seconds = AddUnchecked(
                AddUnchecked(
                    MultiplyUnchecked(ParseInt32(ranges[0]), 3600),
                    MultiplyUnchecked(ParseInt32(ranges[1]), 60)),
                ParseInt32(ranges[2]));
        }
        else
        {
            seconds = ParseInt32(time);
        }
        return MultiplyUnchecked(seconds, 1000);
    }

    std::int32_t Common::VLVLength(std::int32_t value) noexcept
    {
        if (value >= 0x10000000)
        {
            return 5;
        }
        if (value >= 0x00200000)
        {
            return 4;
        }
        if (value >= 0x00004000)
        {
            return 3;
        }
        if (value >= 0x00000080)
        {
            return 2;
        }
        return 1;
    }
}
