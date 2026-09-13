#include "Common.hpp"

#include <charconv>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <cuchar>
#include <langinfo.h>
#include <locale.h>
#endif

namespace
{
    [[nodiscard]] constexpr bool IsParseWhiteSpace(char16_t value) noexcept
    {
        return (value >= u'\u0009' && value <= u'\u000D') || value == u'\u0020';
    }

    [[nodiscard]] constexpr bool AllowsAsciiHyphen(char16_t value) noexcept
    {
        switch (value)
        {
        case u'\u2012':
        case u'\u207B':
        case u'\u208B':
        case u'\u2212':
        case u'\u2796':
        case u'\uFE63':
        case u'\uFF0D':
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::u16string ToU16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        for (char chr : value)
        {
            result.push_back(static_cast<char16_t>(static_cast<unsigned char>(chr)));
        }
        return result;
    }

#if !defined(_WIN32)
    [[nodiscard]] std::u16string LocaleBytesToU16(const char* value)
    {
        if (value == nullptr || *value == '\0')
        {
            return {};
        }

        std::u16string result;
        std::mbstate_t state{};
        const char* current = value;
        std::size_t remaining = std::strlen(value);
        while (remaining > 0)
        {
            char32_t scalar = U'\0';
            const std::size_t length = std::mbrtoc32(&scalar, current, remaining, &state);
            if (length == static_cast<std::size_t>(-1)
                || length == static_cast<std::size_t>(-2)
                || length == static_cast<std::size_t>(-3))
            {
                return ToU16(value);
            }
            if (length == 0)
            {
                break;
            }
            if (scalar <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(scalar));
            }
            else if (scalar <= 0x10FFFFU)
            {
                scalar -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10U)));
                result.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
            }
            current += length;
            remaining -= length;
        }
        return result;
    }
#endif

#if defined(_WIN32)
    [[nodiscard]] std::u16string WideToU16(std::wstring_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        for (wchar_t chr : value)
        {
            result.push_back(static_cast<char16_t>(chr));
        }
        return result;
    }

    [[nodiscard]] std::u16string GetWindowsThreadLocaleInfo(LCTYPE type)
    {
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
        const LCID localeId = GetThreadLocale();
        if (LCIDToLocaleName(localeId, localeName, LOCALE_NAME_MAX_LENGTH, 0) == 0)
        {
            return {};
        }

        const int required = GetLocaleInfoEx(localeName, type, nullptr, 0);
        if (required <= 1)
        {
            return {};
        }
        std::wstring value(static_cast<std::size_t>(required), L'\0');
        if (GetLocaleInfoEx(localeName, type, value.data(), required) == 0)
        {
            return {};
        }
        value.resize(static_cast<std::size_t>(required - 1));
        return WideToU16(value);
    }
#endif

    [[nodiscard]] NCSFCommon::NumberFormatInfo LoadExecutionThreadNumberFormat()
    {
        NCSFCommon::NumberFormatInfo result{
            u"+",
            u"-",
            u".",
            u"NaN",
            u"Infinity",
            u"-Infinity"
        };

#if defined(_WIN32)
        if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SPOSITIVESIGN); !value.empty())
        {
            result.PositiveSign = std::move(value);
        }
        if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SNEGATIVESIGN); !value.empty())
        {
            result.NegativeSign = std::move(value);
        }
        if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SDECIMAL); !value.empty())
        {
            result.NumberDecimalSeparator = std::move(value);
        }
#if defined(LOCALE_SNAN)
        if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SNAN); !value.empty())
        {
            result.NaNSymbol = std::move(value);
        }
#endif
#if defined(LOCALE_SPOSINFINITY)
        if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SPOSINFINITY); !value.empty())
        {
            result.PositiveInfinitySymbol = std::move(value);
        }
#endif
#if defined(LOCALE_SNEGINFINITY)
        if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SNEGINFINITY); !value.empty())
        {
            result.NegativeInfinitySymbol = std::move(value);
        }
#endif
#else
        locale_t locale = uselocale(static_cast<locale_t>(0));
        if (locale != static_cast<locale_t>(0) && locale != LC_GLOBAL_LOCALE)
        {
            if (std::u16string value = LocaleBytesToU16(nl_langinfo_l(RADIXCHAR, locale)); !value.empty())
            {
                result.NumberDecimalSeparator = std::move(value);
            }
#if defined(POSITIVE_SIGN)
            if (std::u16string value = LocaleBytesToU16(nl_langinfo_l(POSITIVE_SIGN, locale)); !value.empty())
            {
                result.PositiveSign = std::move(value);
            }
#endif
#if defined(NEGATIVE_SIGN)
            if (std::u16string value = LocaleBytesToU16(nl_langinfo_l(NEGATIVE_SIGN, locale)); !value.empty())
            {
                result.NegativeSign = std::move(value);
            }
#endif
        }
#endif
        return result;
    }

    void DebugAssert(bool condition, std::string_view message)
    {
#ifndef NDEBUG
        if (!condition)
        {
            std::clog << "Debug.Assert failed: " << message << '\n';
        }
#else
        static_cast<void>(condition);
        static_cast<void>(message);
#endif
    }

    [[nodiscard]] std::vector<std::u16string_view> SplitPreserveEmpty(
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

    [[nodiscard]] bool StartsWith(std::u16string_view value, std::u16string_view prefix) noexcept
    {
        return prefix.size() <= value.size() && value.substr(0, prefix.size()) == prefix;
    }

    [[nodiscard]] std::int32_t ParseInt32(std::u16string_view value)
    {
        if (value.empty())
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        const NCSFCommon::NumberFormatInfo& format = NCSFCommon::CurrentCultureNumberFormat;
        std::size_t index = 0;
        while (index < value.size() && IsParseWhiteSpace(value[index]))
        {
            ++index;
        }
        if (index >= value.size())
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        bool negative = false;
        const bool invariantSigns = format.PositiveSign == u"+" && format.NegativeSign == u"-";
        if (invariantSigns)
        {
            if (value[index] == u'-')
            {
                negative = true;
                ++index;
            }
            else if (value[index] == u'+')
            {
                ++index;
            }
        }
        else if (format.NegativeSign.size() == 1
            && AllowsAsciiHyphen(format.NegativeSign.front())
            && value[index] == u'-')
        {
            negative = true;
            ++index;
        }
        else
        {
            const std::u16string_view remaining = value.substr(index);
            if (!format.PositiveSign.empty() && StartsWith(remaining, format.PositiveSign))
            {
                index += format.PositiveSign.size();
            }
            else if (!format.NegativeSign.empty() && StartsWith(remaining, format.NegativeSign))
            {
                negative = true;
                index += format.NegativeSign.size();
            }
        }

        if (index >= value.size())
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        bool sawDigit = false;
        bool overflow = false;
        std::uint64_t magnitude = 0;
        const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
        while (index < value.size() && value[index] >= u'0' && value[index] <= u'9')
        {
            sawDigit = true;
            const std::uint64_t digit = static_cast<std::uint64_t>(value[index] - u'0');
            if (!overflow)
            {
                if (magnitude > (limit - digit) / 10ULL)
                {
                    overflow = true;
                }
                else
                {
                    magnitude = magnitude * 10ULL + digit;
                }
            }
            ++index;
        }

        if (!sawDigit)
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        if (index < value.size() && IsParseWhiteSpace(value[index]))
        {
            do
            {
                ++index;
            } while (index < value.size() && IsParseWhiteSpace(value[index]));
        }

        while (index < value.size() && value[index] == u'\0')
        {
            ++index;
        }

        if (index != value.size())
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }
        if (overflow)
        {
            throw std::out_of_range("Value was either too large or too small for an Int32.");
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

    [[nodiscard]] std::int32_t AddUnchecked(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t MultiplyUnchecked(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t FloatToInt32Unchecked(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }
        if (value >= 2147483648.0F)
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        if (value < static_cast<float>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::u16string FormatIntD2(
        std::int32_t value, const NCSFCommon::NumberFormatInfo& format)
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

        std::u16string result;
        if (negative)
        {
            result += format.NegativeSign;
        }
        result += ToU16(digits);
        return result;
    }

    [[nodiscard]] std::u16string FormatSeconds(
        float value, const NCSFCommon::NumberFormatInfo& format)
    {
        if (std::isnan(value))
        {
            return format.NaNSymbol;
        }
        if (std::isinf(value))
        {
            return std::signbit(value)
                ? format.NegativeInfinitySymbol
                : format.PositiveInfinitySymbol;
        }

        const bool negative = std::signbit(value);
        const float magnitude = std::fabs(value);
        std::array<char, 128> buffer{};
        const auto [end, error] = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::fixed, 4);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format a Single value.");
        }

        std::string text(buffer.data(), end);
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
        if (integerDigits == 1 && !text.empty() && text.front() != '0')
        {
            text.insert(text.begin(), '0');
        }

        std::u16string result;
        if (negative)
        {
            result += format.NegativeSign;
        }
        for (char chr : text)
        {
            if (chr == '.')
            {
                result += format.NumberDecimalSeparator;
            }
            else
            {
                result.push_back(static_cast<char16_t>(static_cast<unsigned char>(chr)));
            }
        }
        return result;
    }

    [[nodiscard]] std::u16string ReplaceAll(
        std::u16string value, std::u16string_view oldValue, std::u16string_view newValue)
    {
        std::size_t index = 0;
        while ((index = value.find(oldValue, index)) != std::u16string::npos)
        {
            value.replace(index, oldValue.size(), newValue);
            index += newValue.size();
        }
        return value;
    }

    [[nodiscard]] std::u16string KeepTypeName(NCSFCommon::Common::KeepType keep)
    {
        switch (keep)
        {
        case NCSFCommon::Common::KeepType::Exclude:
            return u"Exclude";
        case NCSFCommon::Common::KeepType::Include:
            return u"Include";
        case NCSFCommon::Common::KeepType::Neither:
            return u"Neither";
        }
        return ToU16(std::to_string(static_cast<std::uint8_t>(keep)));
    }

    [[nodiscard]] std::size_t RecordHashCombine(std::size_t hash, std::size_t value) noexcept
    {
        constexpr std::size_t multiplier = static_cast<std::uint32_t>(2773833001U);
        return hash * multiplier + value;
    }

    enum class UnicodeCategory : std::uint8_t
    {
        Cc,
        Cf,
        Cn,
        Co,
        Cs,
        Ll,
        Lm,
        Lo,
        Lt,
        Lu,
        Mc,
        Me,
        Mn,
        Nd,
        Nl,
        No,
        Pc,
        Pd,
        Pe,
        Pf,
        Pi,
        Po,
        Ps,
        Sc,
        Sk,
        Sm,
        So,
        Zl,
        Zp,
        Zs
    };

    constexpr char16_t UnicodeCategoryData[] =
        u"\x0000\x001F\x0000\x0020\x0020\x001D\x0021\x0023\x0015\x0024\x0024\x0017\x0025\x0027\x0015\x0028\x0028\x0016\x0029\x0029\x0012\x002A\x002A\x0015"
        u"\x002B\x002B\x0019\x002C\x002C\x0015\x002D\x002D\x0011\x002E\x002F\x0015\x0030\x0039\x000D\x003A\x003B\x0015\x003C\x003E\x0019\x003F\x0040\x0015"
        u"\x0041\x005A\x0009\x005B\x005B\x0016\x005C\x005C\x0015\x005D\x005D\x0012\x005E\x005E\x0018\x005F\x005F\x0010\x0060\x0060\x0018\x0061\x007A\x0005"
        u"\x007B\x007B\x0016\x007C\x007C\x0019\x007D\x007D\x0012\x007E\x007E\x0019\x007F\x009F\x0000\x00A0\x00A0\x001D\x00A1\x00A1\x0015\x00A2\x00A5\x0017"
        u"\x00A6\x00A6\x001A\x00A7\x00A7\x0015\x00A8\x00A8\x0018\x00A9\x00A9\x001A\x00AA\x00AA\x0007\x00AB\x00AB\x0014\x00AC\x00AC\x0019\x00AD\x00AD\x0001"
        u"\x00AE\x00AE\x001A\x00AF\x00AF\x0018\x00B0\x00B0\x001A\x00B1\x00B1\x0019\x00B2\x00B3\x000F\x00B4\x00B4\x0018\x00B5\x00B5\x0005\x00B6\x00B7\x0015"
        u"\x00B8\x00B8\x0018\x00B9\x00B9\x000F\x00BA\x00BA\x0007\x00BB\x00BB\x0013\x00BC\x00BE\x000F\x00BF\x00BF\x0015\x00C0\x00D6\x0009\x00D7\x00D7\x0019"
        u"\x00D8\x00DE\x0009\x00DF\x00F6\x0005\x00F7\x00F7\x0019\x00F8\x00FF\x0005\x0100\x0100\x0009\x0101\x0101\x0005\x0102\x0102\x0009\x0103\x0103\x0005"
        u"\x0104\x0104\x0009\x0105\x0105\x0005\x0106\x0106\x0009\x0107\x0107\x0005\x0108\x0108\x0009\x0109\x0109\x0005\x010A\x010A\x0009\x010B\x010B\x0005"
        u"\x010C\x010C\x0009\x010D\x010D\x0005\x010E\x010E\x0009\x010F\x010F\x0005\x0110\x0110\x0009\x0111\x0111\x0005\x0112\x0112\x0009\x0113\x0113\x0005"
        u"\x0114\x0114\x0009\x0115\x0115\x0005\x0116\x0116\x0009\x0117\x0117\x0005\x0118\x0118\x0009\x0119\x0119\x0005\x011A\x011A\x0009\x011B\x011B\x0005"
        u"\x011C\x011C\x0009\x011D\x011D\x0005\x011E\x011E\x0009\x011F\x011F\x0005\x0120\x0120\x0009\x0121\x0121\x0005\x0122\x0122\x0009\x0123\x0123\x0005"
        u"\x0124\x0124\x0009\x0125\x0125\x0005\x0126\x0126\x0009\x0127\x0127\x0005\x0128\x0128\x0009\x0129\x0129\x0005\x012A\x012A\x0009\x012B\x012B\x0005"
        u"\x012C\x012C\x0009\x012D\x012D\x0005\x012E\x012E\x0009\x012F\x012F\x0005\x0130\x0130\x0009\x0131\x0131\x0005\x0132\x0132\x0009\x0133\x0133\x0005"
        u"\x0134\x0134\x0009\x0135\x0135\x0005\x0136\x0136\x0009\x0137\x0138\x0005\x0139\x0139\x0009\x013A\x013A\x0005\x013B\x013B\x0009\x013C\x013C\x0005"
        u"\x013D\x013D\x0009\x013E\x013E\x0005\x013F\x013F\x0009\x0140\x0140\x0005\x0141\x0141\x0009\x0142\x0142\x0005\x0143\x0143\x0009\x0144\x0144\x0005"
        u"\x0145\x0145\x0009\x0146\x0146\x0005\x0147\x0147\x0009\x0148\x0149\x0005\x014A\x014A\x0009\x014B\x014B\x0005\x014C\x014C\x0009\x014D\x014D\x0005"
        u"\x014E\x014E\x0009\x014F\x014F\x0005\x0150\x0150\x0009\x0151\x0151\x0005\x0152\x0152\x0009\x0153\x0153\x0005\x0154\x0154\x0009\x0155\x0155\x0005"
        u"\x0156\x0156\x0009\x0157\x0157\x0005\x0158\x0158\x0009\x0159\x0159\x0005\x015A\x015A\x0009\x015B\x015B\x0005\x015C\x015C\x0009\x015D\x015D\x0005"
        u"\x015E\x015E\x0009\x015F\x015F\x0005\x0160\x0160\x0009\x0161\x0161\x0005\x0162\x0162\x0009\x0163\x0163\x0005\x0164\x0164\x0009\x0165\x0165\x0005"
        u"\x0166\x0166\x0009\x0167\x0167\x0005\x0168\x0168\x0009\x0169\x0169\x0005\x016A\x016A\x0009\x016B\x016B\x0005\x016C\x016C\x0009\x016D\x016D\x0005"
        u"\x016E\x016E\x0009\x016F\x016F\x0005\x0170\x0170\x0009\x0171\x0171\x0005\x0172\x0172\x0009\x0173\x0173\x0005\x0174\x0174\x0009\x0175\x0175\x0005"
        u"\x0176\x0176\x0009\x0177\x0177\x0005\x0178\x0179\x0009\x017A\x017A\x0005\x017B\x017B\x0009\x017C\x017C\x0005\x017D\x017D\x0009\x017E\x0180\x0005"
        u"\x0181\x0182\x0009\x0183\x0183\x0005\x0184\x0184\x0009\x0185\x0185\x0005\x0186\x0187\x0009\x0188\x0188\x0005\x0189\x018B\x0009\x018C\x018D\x0005"
        u"\x018E\x0191\x0009\x0192\x0192\x0005\x0193\x0194\x0009\x0195\x0195\x0005\x0196\x0198\x0009\x0199\x019B\x0005\x019C\x019D\x0009\x019E\x019E\x0005"
        u"\x019F\x01A0\x0009\x01A1\x01A1\x0005\x01A2\x01A2\x0009\x01A3\x01A3\x0005\x01A4\x01A4\x0009\x01A5\x01A5\x0005\x01A6\x01A7\x0009\x01A8\x01A8\x0005"
        u"\x01A9\x01A9\x0009\x01AA\x01AB\x0005\x01AC\x01AC\x0009\x01AD\x01AD\x0005\x01AE\x01AF\x0009\x01B0\x01B0\x0005\x01B1\x01B3\x0009\x01B4\x01B4\x0005"
        u"\x01B5\x01B5\x0009\x01B6\x01B6\x0005\x01B7\x01B8\x0009\x01B9\x01BA\x0005\x01BB\x01BB\x0007\x01BC\x01BC\x0009\x01BD\x01BF\x0005\x01C0\x01C3\x0007"
        u"\x01C4\x01C4\x0009\x01C5\x01C5\x0008\x01C6\x01C6\x0005\x01C7\x01C7\x0009\x01C8\x01C8\x0008\x01C9\x01C9\x0005\x01CA\x01CA\x0009\x01CB\x01CB\x0008"
        u"\x01CC\x01CC\x0005\x01CD\x01CD\x0009\x01CE\x01CE\x0005\x01CF\x01CF\x0009\x01D0\x01D0\x0005\x01D1\x01D1\x0009\x01D2\x01D2\x0005\x01D3\x01D3\x0009"
        u"\x01D4\x01D4\x0005\x01D5\x01D5\x0009\x01D6\x01D6\x0005\x01D7\x01D7\x0009\x01D8\x01D8\x0005\x01D9\x01D9\x0009\x01DA\x01DA\x0005\x01DB\x01DB\x0009"
        u"\x01DC\x01DD\x0005\x01DE\x01DE\x0009\x01DF\x01DF\x0005\x01E0\x01E0\x0009\x01E1\x01E1\x0005\x01E2\x01E2\x0009\x01E3\x01E3\x0005\x01E4\x01E4\x0009"
        u"\x01E5\x01E5\x0005\x01E6\x01E6\x0009\x01E7\x01E7\x0005\x01E8\x01E8\x0009\x01E9\x01E9\x0005\x01EA\x01EA\x0009\x01EB\x01EB\x0005\x01EC\x01EC\x0009"
        u"\x01ED\x01ED\x0005\x01EE\x01EE\x0009\x01EF\x01F0\x0005\x01F1\x01F1\x0009\x01F2\x01F2\x0008\x01F3\x01F3\x0005\x01F4\x01F4\x0009\x01F5\x01F5\x0005"
        u"\x01F6\x01F8\x0009\x01F9\x01F9\x0005\x01FA\x01FA\x0009\x01FB\x01FB\x0005\x01FC\x01FC\x0009\x01FD\x01FD\x0005\x01FE\x01FE\x0009\x01FF\x01FF\x0005"
        u"\x0200\x0200\x0009\x0201\x0201\x0005\x0202\x0202\x0009\x0203\x0203\x0005\x0204\x0204\x0009\x0205\x0205\x0005\x0206\x0206\x0009\x0207\x0207\x0005"
        u"\x0208\x0208\x0009\x0209\x0209\x0005\x020A\x020A\x0009\x020B\x020B\x0005\x020C\x020C\x0009\x020D\x020D\x0005\x020E\x020E\x0009\x020F\x020F\x0005"
        u"\x0210\x0210\x0009\x0211\x0211\x0005\x0212\x0212\x0009\x0213\x0213\x0005\x0214\x0214\x0009\x0215\x0215\x0005\x0216\x0216\x0009\x0217\x0217\x0005"
        u"\x0218\x0218\x0009\x0219\x0219\x0005\x021A\x021A\x0009\x021B\x021B\x0005\x021C\x021C\x0009\x021D\x021D\x0005\x021E\x021E\x0009\x021F\x021F\x0005"
        u"\x0220\x0220\x0009\x0221\x0221\x0005\x0222\x0222\x0009\x0223\x0223\x0005\x0224\x0224\x0009\x0225\x0225\x0005\x0226\x0226\x0009\x0227\x0227\x0005"
        u"\x0228\x0228\x0009\x0229\x0229\x0005\x022A\x022A\x0009\x022B\x022B\x0005\x022C\x022C\x0009\x022D\x022D\x0005\x022E\x022E\x0009\x022F\x022F\x0005"
        u"\x0230\x0230\x0009\x0231\x0231\x0005\x0232\x0232\x0009\x0233\x0239\x0005\x023A\x023B\x0009\x023C\x023C\x0005\x023D\x023E\x0009\x023F\x0240\x0005"
        u"\x0241\x0241\x0009\x0242\x0242\x0005\x0243\x0246\x0009\x0247\x0247\x0005\x0248\x0248\x0009\x0249\x0249\x0005\x024A\x024A\x0009\x024B\x024B\x0005"
        u"\x024C\x024C\x0009\x024D\x024D\x0005\x024E\x024E\x0009\x024F\x0293\x0005\x0294\x0294\x0007\x0295\x02AF\x0005\x02B0\x02C1\x0006\x02C2\x02C5\x0018"
        u"\x02C6\x02D1\x0006\x02D2\x02DF\x0018\x02E0\x02E4\x0006\x02E5\x02EB\x0018\x02EC\x02EC\x0006\x02ED\x02ED\x0018\x02EE\x02EE\x0006\x02EF\x02FF\x0018"
        u"\x0300\x036F\x000C\x0370\x0370\x0009\x0371\x0371\x0005\x0372\x0372\x0009\x0373\x0373\x0005\x0374\x0374\x0006\x0375\x0375\x0018\x0376\x0376\x0009"
        u"\x0377\x0377\x0005\x0378\x0379\x0002\x037A\x037A\x0006\x037B\x037D\x0005\x037E\x037E\x0015\x037F\x037F\x0009\x0380\x0383\x0002\x0384\x0385\x0018"
        u"\x0386\x0386\x0009\x0387\x0387\x0015\x0388\x038A\x0009\x038B\x038B\x0002\x038C\x038C\x0009\x038D\x038D\x0002\x038E\x038F\x0009\x0390\x0390\x0005"
        u"\x0391\x03A1\x0009\x03A2\x03A2\x0002\x03A3\x03AB\x0009\x03AC\x03CE\x0005\x03CF\x03CF\x0009\x03D0\x03D1\x0005\x03D2\x03D4\x0009\x03D5\x03D7\x0005"
        u"\x03D8\x03D8\x0009\x03D9\x03D9\x0005\x03DA\x03DA\x0009\x03DB\x03DB\x0005\x03DC\x03DC\x0009\x03DD\x03DD\x0005\x03DE\x03DE\x0009\x03DF\x03DF\x0005"
        u"\x03E0\x03E0\x0009\x03E1\x03E1\x0005\x03E2\x03E2\x0009\x03E3\x03E3\x0005\x03E4\x03E4\x0009\x03E5\x03E5\x0005\x03E6\x03E6\x0009\x03E7\x03E7\x0005"
        u"\x03E8\x03E8\x0009\x03E9\x03E9\x0005\x03EA\x03EA\x0009\x03EB\x03EB\x0005\x03EC\x03EC\x0009\x03ED\x03ED\x0005\x03EE\x03EE\x0009\x03EF\x03F3\x0005"
        u"\x03F4\x03F4\x0009\x03F5\x03F5\x0005\x03F6\x03F6\x0019\x03F7\x03F7\x0009\x03F8\x03F8\x0005\x03F9\x03FA\x0009\x03FB\x03FC\x0005\x03FD\x042F\x0009"
        u"\x0430\x045F\x0005\x0460\x0460\x0009\x0461\x0461\x0005\x0462\x0462\x0009\x0463\x0463\x0005\x0464\x0464\x0009\x0465\x0465\x0005\x0466\x0466\x0009"
        u"\x0467\x0467\x0005\x0468\x0468\x0009\x0469\x0469\x0005\x046A\x046A\x0009\x046B\x046B\x0005\x046C\x046C\x0009\x046D\x046D\x0005\x046E\x046E\x0009"
        u"\x046F\x046F\x0005\x0470\x0470\x0009\x0471\x0471\x0005\x0472\x0472\x0009\x0473\x0473\x0005\x0474\x0474\x0009\x0475\x0475\x0005\x0476\x0476\x0009"
        u"\x0477\x0477\x0005\x0478\x0478\x0009\x0479\x0479\x0005\x047A\x047A\x0009\x047B\x047B\x0005\x047C\x047C\x0009\x047D\x047D\x0005\x047E\x047E\x0009"
        u"\x047F\x047F\x0005\x0480\x0480\x0009\x0481\x0481\x0005\x0482\x0482\x001A\x0483\x0487\x000C\x0488\x0489\x000B\x048A\x048A\x0009\x048B\x048B\x0005"
        u"\x048C\x048C\x0009\x048D\x048D\x0005\x048E\x048E\x0009\x048F\x048F\x0005\x0490\x0490\x0009\x0491\x0491\x0005\x0492\x0492\x0009\x0493\x0493\x0005"
        u"\x0494\x0494\x0009\x0495\x0495\x0005\x0496\x0496\x0009\x0497\x0497\x0005\x0498\x0498\x0009\x0499\x0499\x0005\x049A\x049A\x0009\x049B\x049B\x0005"
        u"\x049C\x049C\x0009\x049D\x049D\x0005\x049E\x049E\x0009\x049F\x049F\x0005\x04A0\x04A0\x0009\x04A1\x04A1\x0005\x04A2\x04A2\x0009\x04A3\x04A3\x0005"
        u"\x04A4\x04A4\x0009\x04A5\x04A5\x0005\x04A6\x04A6\x0009\x04A7\x04A7\x0005\x04A8\x04A8\x0009\x04A9\x04A9\x0005\x04AA\x04AA\x0009\x04AB\x04AB\x0005"
        u"\x04AC\x04AC\x0009\x04AD\x04AD\x0005\x04AE\x04AE\x0009\x04AF\x04AF\x0005\x04B0\x04B0\x0009\x04B1\x04B1\x0005\x04B2\x04B2\x0009\x04B3\x04B3\x0005"
        u"\x04B4\x04B4\x0009\x04B5\x04B5\x0005\x04B6\x04B6\x0009\x04B7\x04B7\x0005\x04B8\x04B8\x0009\x04B9\x04B9\x0005\x04BA\x04BA\x0009\x04BB\x04BB\x0005"
        u"\x04BC\x04BC\x0009\x04BD\x04BD\x0005\x04BE\x04BE\x0009\x04BF\x04BF\x0005\x04C0\x04C1\x0009\x04C2\x04C2\x0005\x04C3\x04C3\x0009\x04C4\x04C4\x0005"
        u"\x04C5\x04C5\x0009\x04C6\x04C6\x0005\x04C7\x04C7\x0009\x04C8\x04C8\x0005\x04C9\x04C9\x0009\x04CA\x04CA\x0005\x04CB\x04CB\x0009\x04CC\x04CC\x0005"
        u"\x04CD\x04CD\x0009\x04CE\x04CF\x0005\x04D0\x04D0\x0009\x04D1\x04D1\x0005\x04D2\x04D2\x0009\x04D3\x04D3\x0005\x04D4\x04D4\x0009\x04D5\x04D5\x0005"
        u"\x04D6\x04D6\x0009\x04D7\x04D7\x0005\x04D8\x04D8\x0009\x04D9\x04D9\x0005\x04DA\x04DA\x0009\x04DB\x04DB\x0005\x04DC\x04DC\x0009\x04DD\x04DD\x0005"
        u"\x04DE\x04DE\x0009\x04DF\x04DF\x0005\x04E0\x04E0\x0009\x04E1\x04E1\x0005\x04E2\x04E2\x0009\x04E3\x04E3\x0005\x04E4\x04E4\x0009\x04E5\x04E5\x0005"
        u"\x04E6\x04E6\x0009\x04E7\x04E7\x0005\x04E8\x04E8\x0009\x04E9\x04E9\x0005\x04EA\x04EA\x0009\x04EB\x04EB\x0005\x04EC\x04EC\x0009\x04ED\x04ED\x0005"
        u"\x04EE\x04EE\x0009\x04EF\x04EF\x0005\x04F0\x04F0\x0009\x04F1\x04F1\x0005\x04F2\x04F2\x0009\x04F3\x04F3\x0005\x04F4\x04F4\x0009\x04F5\x04F5\x0005"
        u"\x04F6\x04F6\x0009\x04F7\x04F7\x0005\x04F8\x04F8\x0009\x04F9\x04F9\x0005\x04FA\x04FA\x0009\x04FB\x04FB\x0005\x04FC\x04FC\x0009\x04FD\x04FD\x0005"
        u"\x04FE\x04FE\x0009\x04FF\x04FF\x0005\x0500\x0500\x0009\x0501\x0501\x0005\x0502\x0502\x0009\x0503\x0503\x0005\x0504\x0504\x0009\x0505\x0505\x0005"
        u"\x0506\x0506\x0009\x0507\x0507\x0005\x0508\x0508\x0009\x0509\x0509\x0005\x050A\x050A\x0009\x050B\x050B\x0005\x050C\x050C\x0009\x050D\x050D\x0005"
        u"\x050E\x050E\x0009\x050F\x050F\x0005\x0510\x0510\x0009\x0511\x0511\x0005\x0512\x0512\x0009\x0513\x0513\x0005\x0514\x0514\x0009\x0515\x0515\x0005"
        u"\x0516\x0516\x0009\x0517\x0517\x0005\x0518\x0518\x0009\x0519\x0519\x0005\x051A\x051A\x0009\x051B\x051B\x0005\x051C\x051C\x0009\x051D\x051D\x0005"
        u"\x051E\x051E\x0009\x051F\x051F\x0005\x0520\x0520\x0009\x0521\x0521\x0005\x0522\x0522\x0009\x0523\x0523\x0005\x0524\x0524\x0009\x0525\x0525\x0005"
        u"\x0526\x0526\x0009\x0527\x0527\x0005\x0528\x0528\x0009\x0529\x0529\x0005\x052A\x052A\x0009\x052B\x052B\x0005\x052C\x052C\x0009\x052D\x052D\x0005"
        u"\x052E\x052E\x0009\x052F\x052F\x0005\x0530\x0530\x0002\x0531\x0556\x0009\x0557\x0558\x0002\x0559\x0559\x0006\x055A\x055F\x0015\x0560\x0588\x0005"
        u"\x0589\x0589\x0015\x058A\x058A\x0011\x058B\x058C\x0002\x058D\x058E\x001A\x058F\x058F\x0017\x0590\x0590\x0002\x0591\x05BD\x000C\x05BE\x05BE\x0011"
        u"\x05BF\x05BF\x000C\x05C0\x05C0\x0015\x05C1\x05C2\x000C\x05C3\x05C3\x0015\x05C4\x05C5\x000C\x05C6\x05C6\x0015\x05C7\x05C7\x000C\x05C8\x05CF\x0002"
        u"\x05D0\x05EA\x0007\x05EB\x05EE\x0002\x05EF\x05F2\x0007\x05F3\x05F4\x0015\x05F5\x05FF\x0002\x0600\x0605\x0001\x0606\x0608\x0019\x0609\x060A\x0015"
        u"\x060B\x060B\x0017\x060C\x060D\x0015\x060E\x060F\x001A\x0610\x061A\x000C\x061B\x061B\x0015\x061C\x061C\x0001\x061D\x061F\x0015\x0620\x063F\x0007"
        u"\x0640\x0640\x0006\x0641\x064A\x0007\x064B\x065F\x000C\x0660\x0669\x000D\x066A\x066D\x0015\x066E\x066F\x0007\x0670\x0670\x000C\x0671\x06D3\x0007"
        u"\x06D4\x06D4\x0015\x06D5\x06D5\x0007\x06D6\x06DC\x000C\x06DD\x06DD\x0001\x06DE\x06DE\x001A\x06DF\x06E4\x000C\x06E5\x06E6\x0006\x06E7\x06E8\x000C"
        u"\x06E9\x06E9\x001A\x06EA\x06ED\x000C\x06EE\x06EF\x0007\x06F0\x06F9\x000D\x06FA\x06FC\x0007\x06FD\x06FE\x001A\x06FF\x06FF\x0007\x0700\x070D\x0015"
        u"\x070E\x070E\x0002\x070F\x070F\x0001\x0710\x0710\x0007\x0711\x0711\x000C\x0712\x072F\x0007\x0730\x074A\x000C\x074B\x074C\x0002\x074D\x07A5\x0007"
        u"\x07A6\x07B0\x000C\x07B1\x07B1\x0007\x07B2\x07BF\x0002\x07C0\x07C9\x000D\x07CA\x07EA\x0007\x07EB\x07F3\x000C\x07F4\x07F5\x0006\x07F6\x07F6\x001A"
        u"\x07F7\x07F9\x0015\x07FA\x07FA\x0006\x07FB\x07FC\x0002\x07FD\x07FD\x000C\x07FE\x07FF\x0017\x0800\x0815\x0007\x0816\x0819\x000C\x081A\x081A\x0006"
        u"\x081B\x0823\x000C\x0824\x0824\x0006\x0825\x0827\x000C\x0828\x0828\x0006\x0829\x082D\x000C\x082E\x082F\x0002\x0830\x083E\x0015\x083F\x083F\x0002"
        u"\x0840\x0858\x0007\x0859\x085B\x000C\x085C\x085D\x0002\x085E\x085E\x0015\x085F\x085F\x0002\x0860\x086A\x0007\x086B\x086F\x0002\x0870\x0887\x0007"
        u"\x0888\x0888\x0018\x0889\x088E\x0007\x088F\x088F\x0002\x0890\x0891\x0001\x0892\x0897\x0002\x0898\x089F\x000C\x08A0\x08C8\x0007\x08C9\x08C9\x0006"
        u"\x08CA\x08E1\x000C\x08E2\x08E2\x0001\x08E3\x0902\x000C\x0903\x0903\x000A\x0904\x0939\x0007\x093A\x093A\x000C\x093B\x093B\x000A\x093C\x093C\x000C"
        u"\x093D\x093D\x0007\x093E\x0940\x000A\x0941\x0948\x000C\x0949\x094C\x000A\x094D\x094D\x000C\x094E\x094F\x000A\x0950\x0950\x0007\x0951\x0957\x000C"
        u"\x0958\x0961\x0007\x0962\x0963\x000C\x0964\x0965\x0015\x0966\x096F\x000D\x0970\x0970\x0015\x0971\x0971\x0006\x0972\x0980\x0007\x0981\x0981\x000C"
        u"\x0982\x0983\x000A\x0984\x0984\x0002\x0985\x098C\x0007\x098D\x098E\x0002\x098F\x0990\x0007\x0991\x0992\x0002\x0993\x09A8\x0007\x09A9\x09A9\x0002"
        u"\x09AA\x09B0\x0007\x09B1\x09B1\x0002\x09B2\x09B2\x0007\x09B3\x09B5\x0002\x09B6\x09B9\x0007\x09BA\x09BB\x0002\x09BC\x09BC\x000C\x09BD\x09BD\x0007"
        u"\x09BE\x09C0\x000A\x09C1\x09C4\x000C\x09C5\x09C6\x0002\x09C7\x09C8\x000A\x09C9\x09CA\x0002\x09CB\x09CC\x000A\x09CD\x09CD\x000C\x09CE\x09CE\x0007"
        u"\x09CF\x09D6\x0002\x09D7\x09D7\x000A\x09D8\x09DB\x0002\x09DC\x09DD\x0007\x09DE\x09DE\x0002\x09DF\x09E1\x0007\x09E2\x09E3\x000C\x09E4\x09E5\x0002"
        u"\x09E6\x09EF\x000D\x09F0\x09F1\x0007\x09F2\x09F3\x0017\x09F4\x09F9\x000F\x09FA\x09FA\x001A\x09FB\x09FB\x0017\x09FC\x09FC\x0007\x09FD\x09FD\x0015"
        u"\x09FE\x09FE\x000C\x09FF\x0A00\x0002\x0A01\x0A02\x000C\x0A03\x0A03\x000A\x0A04\x0A04\x0002\x0A05\x0A0A\x0007\x0A0B\x0A0E\x0002\x0A0F\x0A10\x0007"
        u"\x0A11\x0A12\x0002\x0A13\x0A28\x0007\x0A29\x0A29\x0002\x0A2A\x0A30\x0007\x0A31\x0A31\x0002\x0A32\x0A33\x0007\x0A34\x0A34\x0002\x0A35\x0A36\x0007"
        u"\x0A37\x0A37\x0002\x0A38\x0A39\x0007\x0A3A\x0A3B\x0002\x0A3C\x0A3C\x000C\x0A3D\x0A3D\x0002\x0A3E\x0A40\x000A\x0A41\x0A42\x000C\x0A43\x0A46\x0002"
        u"\x0A47\x0A48\x000C\x0A49\x0A4A\x0002\x0A4B\x0A4D\x000C\x0A4E\x0A50\x0002\x0A51\x0A51\x000C\x0A52\x0A58\x0002\x0A59\x0A5C\x0007\x0A5D\x0A5D\x0002"
        u"\x0A5E\x0A5E\x0007\x0A5F\x0A65\x0002\x0A66\x0A6F\x000D\x0A70\x0A71\x000C\x0A72\x0A74\x0007\x0A75\x0A75\x000C\x0A76\x0A76\x0015\x0A77\x0A80\x0002"
        u"\x0A81\x0A82\x000C\x0A83\x0A83\x000A\x0A84\x0A84\x0002\x0A85\x0A8D\x0007\x0A8E\x0A8E\x0002\x0A8F\x0A91\x0007\x0A92\x0A92\x0002\x0A93\x0AA8\x0007"
        u"\x0AA9\x0AA9\x0002\x0AAA\x0AB0\x0007\x0AB1\x0AB1\x0002\x0AB2\x0AB3\x0007\x0AB4\x0AB4\x0002\x0AB5\x0AB9\x0007\x0ABA\x0ABB\x0002\x0ABC\x0ABC\x000C"
        u"\x0ABD\x0ABD\x0007\x0ABE\x0AC0\x000A\x0AC1\x0AC5\x000C\x0AC6\x0AC6\x0002\x0AC7\x0AC8\x000C\x0AC9\x0AC9\x000A\x0ACA\x0ACA\x0002\x0ACB\x0ACC\x000A"
        u"\x0ACD\x0ACD\x000C\x0ACE\x0ACF\x0002\x0AD0\x0AD0\x0007\x0AD1\x0ADF\x0002\x0AE0\x0AE1\x0007\x0AE2\x0AE3\x000C\x0AE4\x0AE5\x0002\x0AE6\x0AEF\x000D"
        u"\x0AF0\x0AF0\x0015\x0AF1\x0AF1\x0017\x0AF2\x0AF8\x0002\x0AF9\x0AF9\x0007\x0AFA\x0AFF\x000C\x0B00\x0B00\x0002\x0B01\x0B01\x000C\x0B02\x0B03\x000A"
        u"\x0B04\x0B04\x0002\x0B05\x0B0C\x0007\x0B0D\x0B0E\x0002\x0B0F\x0B10\x0007\x0B11\x0B12\x0002\x0B13\x0B28\x0007\x0B29\x0B29\x0002\x0B2A\x0B30\x0007"
        u"\x0B31\x0B31\x0002\x0B32\x0B33\x0007\x0B34\x0B34\x0002\x0B35\x0B39\x0007\x0B3A\x0B3B\x0002\x0B3C\x0B3C\x000C\x0B3D\x0B3D\x0007\x0B3E\x0B3E\x000A"
        u"\x0B3F\x0B3F\x000C\x0B40\x0B40\x000A\x0B41\x0B44\x000C\x0B45\x0B46\x0002\x0B47\x0B48\x000A\x0B49\x0B4A\x0002\x0B4B\x0B4C\x000A\x0B4D\x0B4D\x000C"
        u"\x0B4E\x0B54\x0002\x0B55\x0B56\x000C\x0B57\x0B57\x000A\x0B58\x0B5B\x0002\x0B5C\x0B5D\x0007\x0B5E\x0B5E\x0002\x0B5F\x0B61\x0007\x0B62\x0B63\x000C"
        u"\x0B64\x0B65\x0002\x0B66\x0B6F\x000D\x0B70\x0B70\x001A\x0B71\x0B71\x0007\x0B72\x0B77\x000F\x0B78\x0B81\x0002\x0B82\x0B82\x000C\x0B83\x0B83\x0007"
        u"\x0B84\x0B84\x0002\x0B85\x0B8A\x0007\x0B8B\x0B8D\x0002\x0B8E\x0B90\x0007\x0B91\x0B91\x0002\x0B92\x0B95\x0007\x0B96\x0B98\x0002\x0B99\x0B9A\x0007"
        u"\x0B9B\x0B9B\x0002\x0B9C\x0B9C\x0007\x0B9D\x0B9D\x0002\x0B9E\x0B9F\x0007\x0BA0\x0BA2\x0002\x0BA3\x0BA4\x0007\x0BA5\x0BA7\x0002\x0BA8\x0BAA\x0007"
        u"\x0BAB\x0BAD\x0002\x0BAE\x0BB9\x0007\x0BBA\x0BBD\x0002\x0BBE\x0BBF\x000A\x0BC0\x0BC0\x000C\x0BC1\x0BC2\x000A\x0BC3\x0BC5\x0002\x0BC6\x0BC8\x000A"
        u"\x0BC9\x0BC9\x0002\x0BCA\x0BCC\x000A\x0BCD\x0BCD\x000C\x0BCE\x0BCF\x0002\x0BD0\x0BD0\x0007\x0BD1\x0BD6\x0002\x0BD7\x0BD7\x000A\x0BD8\x0BE5\x0002"
        u"\x0BE6\x0BEF\x000D\x0BF0\x0BF2\x000F\x0BF3\x0BF8\x001A\x0BF9\x0BF9\x0017\x0BFA\x0BFA\x001A\x0BFB\x0BFF\x0002\x0C00\x0C00\x000C\x0C01\x0C03\x000A"
        u"\x0C04\x0C04\x000C\x0C05\x0C0C\x0007\x0C0D\x0C0D\x0002\x0C0E\x0C10\x0007\x0C11\x0C11\x0002\x0C12\x0C28\x0007\x0C29\x0C29\x0002\x0C2A\x0C39\x0007"
        u"\x0C3A\x0C3B\x0002\x0C3C\x0C3C\x000C\x0C3D\x0C3D\x0007\x0C3E\x0C40\x000C\x0C41\x0C44\x000A\x0C45\x0C45\x0002\x0C46\x0C48\x000C\x0C49\x0C49\x0002"
        u"\x0C4A\x0C4D\x000C\x0C4E\x0C54\x0002\x0C55\x0C56\x000C\x0C57\x0C57\x0002\x0C58\x0C5A\x0007\x0C5B\x0C5C\x0002\x0C5D\x0C5D\x0007\x0C5E\x0C5F\x0002"
        u"\x0C60\x0C61\x0007\x0C62\x0C63\x000C\x0C64\x0C65\x0002\x0C66\x0C6F\x000D\x0C70\x0C76\x0002\x0C77\x0C77\x0015\x0C78\x0C7E\x000F\x0C7F\x0C7F\x001A"
        u"\x0C80\x0C80\x0007\x0C81\x0C81\x000C\x0C82\x0C83\x000A\x0C84\x0C84\x0015\x0C85\x0C8C\x0007\x0C8D\x0C8D\x0002\x0C8E\x0C90\x0007\x0C91\x0C91\x0002"
        u"\x0C92\x0CA8\x0007\x0CA9\x0CA9\x0002\x0CAA\x0CB3\x0007\x0CB4\x0CB4\x0002\x0CB5\x0CB9\x0007\x0CBA\x0CBB\x0002\x0CBC\x0CBC\x000C\x0CBD\x0CBD\x0007"
        u"\x0CBE\x0CBE\x000A\x0CBF\x0CBF\x000C\x0CC0\x0CC4\x000A\x0CC5\x0CC5\x0002\x0CC6\x0CC6\x000C\x0CC7\x0CC8\x000A\x0CC9\x0CC9\x0002\x0CCA\x0CCB\x000A"
        u"\x0CCC\x0CCD\x000C\x0CCE\x0CD4\x0002\x0CD5\x0CD6\x000A\x0CD7\x0CDC\x0002\x0CDD\x0CDE\x0007\x0CDF\x0CDF\x0002\x0CE0\x0CE1\x0007\x0CE2\x0CE3\x000C"
        u"\x0CE4\x0CE5\x0002\x0CE6\x0CEF\x000D\x0CF0\x0CF0\x0002\x0CF1\x0CF2\x0007\x0CF3\x0CF3\x000A\x0CF4\x0CFF\x0002\x0D00\x0D01\x000C\x0D02\x0D03\x000A"
        u"\x0D04\x0D0C\x0007\x0D0D\x0D0D\x0002\x0D0E\x0D10\x0007\x0D11\x0D11\x0002\x0D12\x0D3A\x0007\x0D3B\x0D3C\x000C\x0D3D\x0D3D\x0007\x0D3E\x0D40\x000A"
        u"\x0D41\x0D44\x000C\x0D45\x0D45\x0002\x0D46\x0D48\x000A\x0D49\x0D49\x0002\x0D4A\x0D4C\x000A\x0D4D\x0D4D\x000C\x0D4E\x0D4E\x0007\x0D4F\x0D4F\x001A"
        u"\x0D50\x0D53\x0002\x0D54\x0D56\x0007\x0D57\x0D57\x000A\x0D58\x0D5E\x000F\x0D5F\x0D61\x0007\x0D62\x0D63\x000C\x0D64\x0D65\x0002\x0D66\x0D6F\x000D"
        u"\x0D70\x0D78\x000F\x0D79\x0D79\x001A\x0D7A\x0D7F\x0007\x0D80\x0D80\x0002\x0D81\x0D81\x000C\x0D82\x0D83\x000A\x0D84\x0D84\x0002\x0D85\x0D96\x0007"
        u"\x0D97\x0D99\x0002\x0D9A\x0DB1\x0007\x0DB2\x0DB2\x0002\x0DB3\x0DBB\x0007\x0DBC\x0DBC\x0002\x0DBD\x0DBD\x0007\x0DBE\x0DBF\x0002\x0DC0\x0DC6\x0007"
        u"\x0DC7\x0DC9\x0002\x0DCA\x0DCA\x000C\x0DCB\x0DCE\x0002\x0DCF\x0DD1\x000A\x0DD2\x0DD4\x000C\x0DD5\x0DD5\x0002\x0DD6\x0DD6\x000C\x0DD7\x0DD7\x0002"
        u"\x0DD8\x0DDF\x000A\x0DE0\x0DE5\x0002\x0DE6\x0DEF\x000D\x0DF0\x0DF1\x0002\x0DF2\x0DF3\x000A\x0DF4\x0DF4\x0015\x0DF5\x0E00\x0002\x0E01\x0E30\x0007"
        u"\x0E31\x0E31\x000C\x0E32\x0E33\x0007\x0E34\x0E3A\x000C\x0E3B\x0E3E\x0002\x0E3F\x0E3F\x0017\x0E40\x0E45\x0007\x0E46\x0E46\x0006\x0E47\x0E4E\x000C"
        u"\x0E4F\x0E4F\x0015\x0E50\x0E59\x000D\x0E5A\x0E5B\x0015\x0E5C\x0E80\x0002\x0E81\x0E82\x0007\x0E83\x0E83\x0002\x0E84\x0E84\x0007\x0E85\x0E85\x0002"
        u"\x0E86\x0E8A\x0007\x0E8B\x0E8B\x0002\x0E8C\x0EA3\x0007\x0EA4\x0EA4\x0002\x0EA5\x0EA5\x0007\x0EA6\x0EA6\x0002\x0EA7\x0EB0\x0007\x0EB1\x0EB1\x000C"
        u"\x0EB2\x0EB3\x0007\x0EB4\x0EBC\x000C\x0EBD\x0EBD\x0007\x0EBE\x0EBF\x0002\x0EC0\x0EC4\x0007\x0EC5\x0EC5\x0002\x0EC6\x0EC6\x0006\x0EC7\x0EC7\x0002"
        u"\x0EC8\x0ECE\x000C\x0ECF\x0ECF\x0002\x0ED0\x0ED9\x000D\x0EDA\x0EDB\x0002\x0EDC\x0EDF\x0007\x0EE0\x0EFF\x0002\x0F00\x0F00\x0007\x0F01\x0F03\x001A"
        u"\x0F04\x0F12\x0015\x0F13\x0F13\x001A\x0F14\x0F14\x0015\x0F15\x0F17\x001A\x0F18\x0F19\x000C\x0F1A\x0F1F\x001A\x0F20\x0F29\x000D\x0F2A\x0F33\x000F"
        u"\x0F34\x0F34\x001A\x0F35\x0F35\x000C\x0F36\x0F36\x001A\x0F37\x0F37\x000C\x0F38\x0F38\x001A\x0F39\x0F39\x000C\x0F3A\x0F3A\x0016\x0F3B\x0F3B\x0012"
        u"\x0F3C\x0F3C\x0016\x0F3D\x0F3D\x0012\x0F3E\x0F3F\x000A\x0F40\x0F47\x0007\x0F48\x0F48\x0002\x0F49\x0F6C\x0007\x0F6D\x0F70\x0002\x0F71\x0F7E\x000C"
        u"\x0F7F\x0F7F\x000A\x0F80\x0F84\x000C\x0F85\x0F85\x0015\x0F86\x0F87\x000C\x0F88\x0F8C\x0007\x0F8D\x0F97\x000C\x0F98\x0F98\x0002\x0F99\x0FBC\x000C"
        u"\x0FBD\x0FBD\x0002\x0FBE\x0FC5\x001A\x0FC6\x0FC6\x000C\x0FC7\x0FCC\x001A\x0FCD\x0FCD\x0002\x0FCE\x0FCF\x001A\x0FD0\x0FD4\x0015\x0FD5\x0FD8\x001A"
        u"\x0FD9\x0FDA\x0015\x0FDB\x0FFF\x0002\x1000\x102A\x0007\x102B\x102C\x000A\x102D\x1030\x000C\x1031\x1031\x000A\x1032\x1037\x000C\x1038\x1038\x000A"
        u"\x1039\x103A\x000C\x103B\x103C\x000A\x103D\x103E\x000C\x103F\x103F\x0007\x1040\x1049\x000D\x104A\x104F\x0015\x1050\x1055\x0007\x1056\x1057\x000A"
        u"\x1058\x1059\x000C\x105A\x105D\x0007\x105E\x1060\x000C\x1061\x1061\x0007\x1062\x1064\x000A\x1065\x1066\x0007\x1067\x106D\x000A\x106E\x1070\x0007"
        u"\x1071\x1074\x000C\x1075\x1081\x0007\x1082\x1082\x000C\x1083\x1084\x000A\x1085\x1086\x000C\x1087\x108C\x000A\x108D\x108D\x000C\x108E\x108E\x0007"
        u"\x108F\x108F\x000A\x1090\x1099\x000D\x109A\x109C\x000A\x109D\x109D\x000C\x109E\x109F\x001A\x10A0\x10C5\x0009\x10C6\x10C6\x0002\x10C7\x10C7\x0009"
        u"\x10C8\x10CC\x0002\x10CD\x10CD\x0009\x10CE\x10CF\x0002\x10D0\x10FA\x0005\x10FB\x10FB\x0015\x10FC\x10FC\x0006\x10FD\x10FF\x0005\x1100\x1248\x0007"
        u"\x1249\x1249\x0002\x124A\x124D\x0007\x124E\x124F\x0002\x1250\x1256\x0007\x1257\x1257\x0002\x1258\x1258\x0007\x1259\x1259\x0002\x125A\x125D\x0007"
        u"\x125E\x125F\x0002\x1260\x1288\x0007\x1289\x1289\x0002\x128A\x128D\x0007\x128E\x128F\x0002\x1290\x12B0\x0007\x12B1\x12B1\x0002\x12B2\x12B5\x0007"
        u"\x12B6\x12B7\x0002\x12B8\x12BE\x0007\x12BF\x12BF\x0002\x12C0\x12C0\x0007\x12C1\x12C1\x0002\x12C2\x12C5\x0007\x12C6\x12C7\x0002\x12C8\x12D6\x0007"
        u"\x12D7\x12D7\x0002\x12D8\x1310\x0007\x1311\x1311\x0002\x1312\x1315\x0007\x1316\x1317\x0002\x1318\x135A\x0007\x135B\x135C\x0002\x135D\x135F\x000C"
        u"\x1360\x1368\x0015\x1369\x137C\x000F\x137D\x137F\x0002\x1380\x138F\x0007\x1390\x1399\x001A\x139A\x139F\x0002\x13A0\x13F5\x0009\x13F6\x13F7\x0002"
        u"\x13F8\x13FD\x0005\x13FE\x13FF\x0002\x1400\x1400\x0011\x1401\x166C\x0007\x166D\x166D\x001A\x166E\x166E\x0015\x166F\x167F\x0007\x1680\x1680\x001D"
        u"\x1681\x169A\x0007\x169B\x169B\x0016\x169C\x169C\x0012\x169D\x169F\x0002\x16A0\x16EA\x0007\x16EB\x16ED\x0015\x16EE\x16F0\x000E\x16F1\x16F8\x0007"
        u"\x16F9\x16FF\x0002\x1700\x1711\x0007\x1712\x1714\x000C\x1715\x1715\x000A\x1716\x171E\x0002\x171F\x1731\x0007\x1732\x1733\x000C\x1734\x1734\x000A"
        u"\x1735\x1736\x0015\x1737\x173F\x0002\x1740\x1751\x0007\x1752\x1753\x000C\x1754\x175F\x0002\x1760\x176C\x0007\x176D\x176D\x0002\x176E\x1770\x0007"
        u"\x1771\x1771\x0002\x1772\x1773\x000C\x1774\x177F\x0002\x1780\x17B3\x0007\x17B4\x17B5\x000C\x17B6\x17B6\x000A\x17B7\x17BD\x000C\x17BE\x17C5\x000A"
        u"\x17C6\x17C6\x000C\x17C7\x17C8\x000A\x17C9\x17D3\x000C\x17D4\x17D6\x0015\x17D7\x17D7\x0006\x17D8\x17DA\x0015\x17DB\x17DB\x0017\x17DC\x17DC\x0007"
        u"\x17DD\x17DD\x000C\x17DE\x17DF\x0002\x17E0\x17E9\x000D\x17EA\x17EF\x0002\x17F0\x17F9\x000F\x17FA\x17FF\x0002\x1800\x1805\x0015\x1806\x1806\x0011"
        u"\x1807\x180A\x0015\x180B\x180D\x000C\x180E\x180E\x0001\x180F\x180F\x000C\x1810\x1819\x000D\x181A\x181F\x0002\x1820\x1842\x0007\x1843\x1843\x0006"
        u"\x1844\x1878\x0007\x1879\x187F\x0002\x1880\x1884\x0007\x1885\x1886\x000C\x1887\x18A8\x0007\x18A9\x18A9\x000C\x18AA\x18AA\x0007\x18AB\x18AF\x0002"
        u"\x18B0\x18F5\x0007\x18F6\x18FF\x0002\x1900\x191E\x0007\x191F\x191F\x0002\x1920\x1922\x000C\x1923\x1926\x000A\x1927\x1928\x000C\x1929\x192B\x000A"
        u"\x192C\x192F\x0002\x1930\x1931\x000A\x1932\x1932\x000C\x1933\x1938\x000A\x1939\x193B\x000C\x193C\x193F\x0002\x1940\x1940\x001A\x1941\x1943\x0002"
        u"\x1944\x1945\x0015\x1946\x194F\x000D\x1950\x196D\x0007\x196E\x196F\x0002\x1970\x1974\x0007\x1975\x197F\x0002\x1980\x19AB\x0007\x19AC\x19AF\x0002"
        u"\x19B0\x19C9\x0007\x19CA\x19CF\x0002\x19D0\x19D9\x000D\x19DA\x19DA\x000F\x19DB\x19DD\x0002\x19DE\x19FF\x001A\x1A00\x1A16\x0007\x1A17\x1A18\x000C"
        u"\x1A19\x1A1A\x000A\x1A1B\x1A1B\x000C\x1A1C\x1A1D\x0002\x1A1E\x1A1F\x0015\x1A20\x1A54\x0007\x1A55\x1A55\x000A\x1A56\x1A56\x000C\x1A57\x1A57\x000A"
        u"\x1A58\x1A5E\x000C\x1A5F\x1A5F\x0002\x1A60\x1A60\x000C\x1A61\x1A61\x000A\x1A62\x1A62\x000C\x1A63\x1A64\x000A\x1A65\x1A6C\x000C\x1A6D\x1A72\x000A"
        u"\x1A73\x1A7C\x000C\x1A7D\x1A7E\x0002\x1A7F\x1A7F\x000C\x1A80\x1A89\x000D\x1A8A\x1A8F\x0002\x1A90\x1A99\x000D\x1A9A\x1A9F\x0002\x1AA0\x1AA6\x0015"
        u"\x1AA7\x1AA7\x0006\x1AA8\x1AAD\x0015\x1AAE\x1AAF\x0002\x1AB0\x1ABD\x000C\x1ABE\x1ABE\x000B\x1ABF\x1ACE\x000C\x1ACF\x1AFF\x0002\x1B00\x1B03\x000C"
        u"\x1B04\x1B04\x000A\x1B05\x1B33\x0007\x1B34\x1B34\x000C\x1B35\x1B35\x000A\x1B36\x1B3A\x000C\x1B3B\x1B3B\x000A\x1B3C\x1B3C\x000C\x1B3D\x1B41\x000A"
        u"\x1B42\x1B42\x000C\x1B43\x1B44\x000A\x1B45\x1B4C\x0007\x1B4D\x1B4F\x0002\x1B50\x1B59\x000D\x1B5A\x1B60\x0015\x1B61\x1B6A\x001A\x1B6B\x1B73\x000C"
        u"\x1B74\x1B7C\x001A\x1B7D\x1B7E\x0015\x1B7F\x1B7F\x0002\x1B80\x1B81\x000C\x1B82\x1B82\x000A\x1B83\x1BA0\x0007\x1BA1\x1BA1\x000A\x1BA2\x1BA5\x000C"
        u"\x1BA6\x1BA7\x000A\x1BA8\x1BA9\x000C\x1BAA\x1BAA\x000A\x1BAB\x1BAD\x000C\x1BAE\x1BAF\x0007\x1BB0\x1BB9\x000D\x1BBA\x1BE5\x0007\x1BE6\x1BE6\x000C"
        u"\x1BE7\x1BE7\x000A\x1BE8\x1BE9\x000C\x1BEA\x1BEC\x000A\x1BED\x1BED\x000C\x1BEE\x1BEE\x000A\x1BEF\x1BF1\x000C\x1BF2\x1BF3\x000A\x1BF4\x1BFB\x0002"
        u"\x1BFC\x1BFF\x0015\x1C00\x1C23\x0007\x1C24\x1C2B\x000A\x1C2C\x1C33\x000C\x1C34\x1C35\x000A\x1C36\x1C37\x000C\x1C38\x1C3A\x0002\x1C3B\x1C3F\x0015"
        u"\x1C40\x1C49\x000D\x1C4A\x1C4C\x0002\x1C4D\x1C4F\x0007\x1C50\x1C59\x000D\x1C5A\x1C77\x0007\x1C78\x1C7D\x0006\x1C7E\x1C7F\x0015\x1C80\x1C88\x0005"
        u"\x1C89\x1C8F\x0002\x1C90\x1CBA\x0009\x1CBB\x1CBC\x0002\x1CBD\x1CBF\x0009\x1CC0\x1CC7\x0015\x1CC8\x1CCF\x0002\x1CD0\x1CD2\x000C\x1CD3\x1CD3\x0015"
        u"\x1CD4\x1CE0\x000C\x1CE1\x1CE1\x000A\x1CE2\x1CE8\x000C\x1CE9\x1CEC\x0007\x1CED\x1CED\x000C\x1CEE\x1CF3\x0007\x1CF4\x1CF4\x000C\x1CF5\x1CF6\x0007"
        u"\x1CF7\x1CF7\x000A\x1CF8\x1CF9\x000C\x1CFA\x1CFA\x0007\x1CFB\x1CFF\x0002\x1D00\x1D2B\x0005\x1D2C\x1D6A\x0006\x1D6B\x1D77\x0005\x1D78\x1D78\x0006"
        u"\x1D79\x1D9A\x0005\x1D9B\x1DBF\x0006\x1DC0\x1DFF\x000C\x1E00\x1E00\x0009\x1E01\x1E01\x0005\x1E02\x1E02\x0009\x1E03\x1E03\x0005\x1E04\x1E04\x0009"
        u"\x1E05\x1E05\x0005\x1E06\x1E06\x0009\x1E07\x1E07\x0005\x1E08\x1E08\x0009\x1E09\x1E09\x0005\x1E0A\x1E0A\x0009\x1E0B\x1E0B\x0005\x1E0C\x1E0C\x0009"
        u"\x1E0D\x1E0D\x0005\x1E0E\x1E0E\x0009\x1E0F\x1E0F\x0005\x1E10\x1E10\x0009\x1E11\x1E11\x0005\x1E12\x1E12\x0009\x1E13\x1E13\x0005\x1E14\x1E14\x0009"
        u"\x1E15\x1E15\x0005\x1E16\x1E16\x0009\x1E17\x1E17\x0005\x1E18\x1E18\x0009\x1E19\x1E19\x0005\x1E1A\x1E1A\x0009\x1E1B\x1E1B\x0005\x1E1C\x1E1C\x0009"
        u"\x1E1D\x1E1D\x0005\x1E1E\x1E1E\x0009\x1E1F\x1E1F\x0005\x1E20\x1E20\x0009\x1E21\x1E21\x0005\x1E22\x1E22\x0009\x1E23\x1E23\x0005\x1E24\x1E24\x0009"
        u"\x1E25\x1E25\x0005\x1E26\x1E26\x0009\x1E27\x1E27\x0005\x1E28\x1E28\x0009\x1E29\x1E29\x0005\x1E2A\x1E2A\x0009\x1E2B\x1E2B\x0005\x1E2C\x1E2C\x0009"
        u"\x1E2D\x1E2D\x0005\x1E2E\x1E2E\x0009\x1E2F\x1E2F\x0005\x1E30\x1E30\x0009\x1E31\x1E31\x0005\x1E32\x1E32\x0009\x1E33\x1E33\x0005\x1E34\x1E34\x0009"
        u"\x1E35\x1E35\x0005\x1E36\x1E36\x0009\x1E37\x1E37\x0005\x1E38\x1E38\x0009\x1E39\x1E39\x0005\x1E3A\x1E3A\x0009\x1E3B\x1E3B\x0005\x1E3C\x1E3C\x0009"
        u"\x1E3D\x1E3D\x0005\x1E3E\x1E3E\x0009\x1E3F\x1E3F\x0005\x1E40\x1E40\x0009\x1E41\x1E41\x0005\x1E42\x1E42\x0009\x1E43\x1E43\x0005\x1E44\x1E44\x0009"
        u"\x1E45\x1E45\x0005\x1E46\x1E46\x0009\x1E47\x1E47\x0005\x1E48\x1E48\x0009\x1E49\x1E49\x0005\x1E4A\x1E4A\x0009\x1E4B\x1E4B\x0005\x1E4C\x1E4C\x0009"
        u"\x1E4D\x1E4D\x0005\x1E4E\x1E4E\x0009\x1E4F\x1E4F\x0005\x1E50\x1E50\x0009\x1E51\x1E51\x0005\x1E52\x1E52\x0009\x1E53\x1E53\x0005\x1E54\x1E54\x0009"
        u"\x1E55\x1E55\x0005\x1E56\x1E56\x0009\x1E57\x1E57\x0005\x1E58\x1E58\x0009\x1E59\x1E59\x0005\x1E5A\x1E5A\x0009\x1E5B\x1E5B\x0005\x1E5C\x1E5C\x0009"
        u"\x1E5D\x1E5D\x0005\x1E5E\x1E5E\x0009\x1E5F\x1E5F\x0005\x1E60\x1E60\x0009\x1E61\x1E61\x0005\x1E62\x1E62\x0009\x1E63\x1E63\x0005\x1E64\x1E64\x0009"
        u"\x1E65\x1E65\x0005\x1E66\x1E66\x0009\x1E67\x1E67\x0005\x1E68\x1E68\x0009\x1E69\x1E69\x0005\x1E6A\x1E6A\x0009\x1E6B\x1E6B\x0005\x1E6C\x1E6C\x0009"
        u"\x1E6D\x1E6D\x0005\x1E6E\x1E6E\x0009\x1E6F\x1E6F\x0005\x1E70\x1E70\x0009\x1E71\x1E71\x0005\x1E72\x1E72\x0009\x1E73\x1E73\x0005\x1E74\x1E74\x0009"
        u"\x1E75\x1E75\x0005\x1E76\x1E76\x0009\x1E77\x1E77\x0005\x1E78\x1E78\x0009\x1E79\x1E79\x0005\x1E7A\x1E7A\x0009\x1E7B\x1E7B\x0005\x1E7C\x1E7C\x0009"
        u"\x1E7D\x1E7D\x0005\x1E7E\x1E7E\x0009\x1E7F\x1E7F\x0005\x1E80\x1E80\x0009\x1E81\x1E81\x0005\x1E82\x1E82\x0009\x1E83\x1E83\x0005\x1E84\x1E84\x0009"
        u"\x1E85\x1E85\x0005\x1E86\x1E86\x0009\x1E87\x1E87\x0005\x1E88\x1E88\x0009\x1E89\x1E89\x0005\x1E8A\x1E8A\x0009\x1E8B\x1E8B\x0005\x1E8C\x1E8C\x0009"
        u"\x1E8D\x1E8D\x0005\x1E8E\x1E8E\x0009\x1E8F\x1E8F\x0005\x1E90\x1E90\x0009\x1E91\x1E91\x0005\x1E92\x1E92\x0009\x1E93\x1E93\x0005\x1E94\x1E94\x0009"
        u"\x1E95\x1E9D\x0005\x1E9E\x1E9E\x0009\x1E9F\x1E9F\x0005\x1EA0\x1EA0\x0009\x1EA1\x1EA1\x0005\x1EA2\x1EA2\x0009\x1EA3\x1EA3\x0005\x1EA4\x1EA4\x0009"
        u"\x1EA5\x1EA5\x0005\x1EA6\x1EA6\x0009\x1EA7\x1EA7\x0005\x1EA8\x1EA8\x0009\x1EA9\x1EA9\x0005\x1EAA\x1EAA\x0009\x1EAB\x1EAB\x0005\x1EAC\x1EAC\x0009"
        u"\x1EAD\x1EAD\x0005\x1EAE\x1EAE\x0009\x1EAF\x1EAF\x0005\x1EB0\x1EB0\x0009\x1EB1\x1EB1\x0005\x1EB2\x1EB2\x0009\x1EB3\x1EB3\x0005\x1EB4\x1EB4\x0009"
        u"\x1EB5\x1EB5\x0005\x1EB6\x1EB6\x0009\x1EB7\x1EB7\x0005\x1EB8\x1EB8\x0009\x1EB9\x1EB9\x0005\x1EBA\x1EBA\x0009\x1EBB\x1EBB\x0005\x1EBC\x1EBC\x0009"
        u"\x1EBD\x1EBD\x0005\x1EBE\x1EBE\x0009\x1EBF\x1EBF\x0005\x1EC0\x1EC0\x0009\x1EC1\x1EC1\x0005\x1EC2\x1EC2\x0009\x1EC3\x1EC3\x0005\x1EC4\x1EC4\x0009"
        u"\x1EC5\x1EC5\x0005\x1EC6\x1EC6\x0009\x1EC7\x1EC7\x0005\x1EC8\x1EC8\x0009\x1EC9\x1EC9\x0005\x1ECA\x1ECA\x0009\x1ECB\x1ECB\x0005\x1ECC\x1ECC\x0009"
        u"\x1ECD\x1ECD\x0005\x1ECE\x1ECE\x0009\x1ECF\x1ECF\x0005\x1ED0\x1ED0\x0009\x1ED1\x1ED1\x0005\x1ED2\x1ED2\x0009\x1ED3\x1ED3\x0005\x1ED4\x1ED4\x0009"
        u"\x1ED5\x1ED5\x0005\x1ED6\x1ED6\x0009\x1ED7\x1ED7\x0005\x1ED8\x1ED8\x0009\x1ED9\x1ED9\x0005\x1EDA\x1EDA\x0009\x1EDB\x1EDB\x0005\x1EDC\x1EDC\x0009"
        u"\x1EDD\x1EDD\x0005\x1EDE\x1EDE\x0009\x1EDF\x1EDF\x0005\x1EE0\x1EE0\x0009\x1EE1\x1EE1\x0005\x1EE2\x1EE2\x0009\x1EE3\x1EE3\x0005\x1EE4\x1EE4\x0009"
        u"\x1EE5\x1EE5\x0005\x1EE6\x1EE6\x0009\x1EE7\x1EE7\x0005\x1EE8\x1EE8\x0009\x1EE9\x1EE9\x0005\x1EEA\x1EEA\x0009\x1EEB\x1EEB\x0005\x1EEC\x1EEC\x0009"
        u"\x1EED\x1EED\x0005\x1EEE\x1EEE\x0009\x1EEF\x1EEF\x0005\x1EF0\x1EF0\x0009\x1EF1\x1EF1\x0005\x1EF2\x1EF2\x0009\x1EF3\x1EF3\x0005\x1EF4\x1EF4\x0009"
        u"\x1EF5\x1EF5\x0005\x1EF6\x1EF6\x0009\x1EF7\x1EF7\x0005\x1EF8\x1EF8\x0009\x1EF9\x1EF9\x0005\x1EFA\x1EFA\x0009\x1EFB\x1EFB\x0005\x1EFC\x1EFC\x0009"
        u"\x1EFD\x1EFD\x0005\x1EFE\x1EFE\x0009\x1EFF\x1F07\x0005\x1F08\x1F0F\x0009\x1F10\x1F15\x0005\x1F16\x1F17\x0002\x1F18\x1F1D\x0009\x1F1E\x1F1F\x0002"
        u"\x1F20\x1F27\x0005\x1F28\x1F2F\x0009\x1F30\x1F37\x0005\x1F38\x1F3F\x0009\x1F40\x1F45\x0005\x1F46\x1F47\x0002\x1F48\x1F4D\x0009\x1F4E\x1F4F\x0002"
        u"\x1F50\x1F57\x0005\x1F58\x1F58\x0002\x1F59\x1F59\x0009\x1F5A\x1F5A\x0002\x1F5B\x1F5B\x0009\x1F5C\x1F5C\x0002\x1F5D\x1F5D\x0009\x1F5E\x1F5E\x0002"
        u"\x1F5F\x1F5F\x0009\x1F60\x1F67\x0005\x1F68\x1F6F\x0009\x1F70\x1F7D\x0005\x1F7E\x1F7F\x0002\x1F80\x1F87\x0005\x1F88\x1F8F\x0008\x1F90\x1F97\x0005"
        u"\x1F98\x1F9F\x0008\x1FA0\x1FA7\x0005\x1FA8\x1FAF\x0008\x1FB0\x1FB4\x0005\x1FB5\x1FB5\x0002\x1FB6\x1FB7\x0005\x1FB8\x1FBB\x0009\x1FBC\x1FBC\x0008"
        u"\x1FBD\x1FBD\x0018\x1FBE\x1FBE\x0005\x1FBF\x1FC1\x0018\x1FC2\x1FC4\x0005\x1FC5\x1FC5\x0002\x1FC6\x1FC7\x0005\x1FC8\x1FCB\x0009\x1FCC\x1FCC\x0008"
        u"\x1FCD\x1FCF\x0018\x1FD0\x1FD3\x0005\x1FD4\x1FD5\x0002\x1FD6\x1FD7\x0005\x1FD8\x1FDB\x0009\x1FDC\x1FDC\x0002\x1FDD\x1FDF\x0018\x1FE0\x1FE7\x0005"
        u"\x1FE8\x1FEC\x0009\x1FED\x1FEF\x0018\x1FF0\x1FF1\x0002\x1FF2\x1FF4\x0005\x1FF5\x1FF5\x0002\x1FF6\x1FF7\x0005\x1FF8\x1FFB\x0009\x1FFC\x1FFC\x0008"
        u"\x1FFD\x1FFE\x0018\x1FFF\x1FFF\x0002\x2000\x200A\x001D\x200B\x200F\x0001\x2010\x2015\x0011\x2016\x2017\x0015\x2018\x2018\x0014\x2019\x2019\x0013"
        u"\x201A\x201A\x0016\x201B\x201C\x0014\x201D\x201D\x0013\x201E\x201E\x0016\x201F\x201F\x0014\x2020\x2027\x0015\x2028\x2028\x001B\x2029\x2029\x001C"
        u"\x202A\x202E\x0001\x202F\x202F\x001D\x2030\x2038\x0015\x2039\x2039\x0014\x203A\x203A\x0013\x203B\x203E\x0015\x203F\x2040\x0010\x2041\x2043\x0015"
        u"\x2044\x2044\x0019\x2045\x2045\x0016\x2046\x2046\x0012\x2047\x2051\x0015\x2052\x2052\x0019\x2053\x2053\x0015\x2054\x2054\x0010\x2055\x205E\x0015"
        u"\x205F\x205F\x001D\x2060\x2064\x0001\x2065\x2065\x0002\x2066\x206F\x0001\x2070\x2070\x000F\x2071\x2071\x0006\x2072\x2073\x0002\x2074\x2079\x000F"
        u"\x207A\x207C\x0019\x207D\x207D\x0016\x207E\x207E\x0012\x207F\x207F\x0006\x2080\x2089\x000F\x208A\x208C\x0019\x208D\x208D\x0016\x208E\x208E\x0012"
        u"\x208F\x208F\x0002\x2090\x209C\x0006\x209D\x209F\x0002\x20A0\x20C0\x0017\x20C1\x20CF\x0002\x20D0\x20DC\x000C\x20DD\x20E0\x000B\x20E1\x20E1\x000C"
        u"\x20E2\x20E4\x000B\x20E5\x20F0\x000C\x20F1\x20FF\x0002\x2100\x2101\x001A\x2102\x2102\x0009\x2103\x2106\x001A\x2107\x2107\x0009\x2108\x2109\x001A"
        u"\x210A\x210A\x0005\x210B\x210D\x0009\x210E\x210F\x0005\x2110\x2112\x0009\x2113\x2113\x0005\x2114\x2114\x001A\x2115\x2115\x0009\x2116\x2117\x001A"
        u"\x2118\x2118\x0019\x2119\x211D\x0009\x211E\x2123\x001A\x2124\x2124\x0009\x2125\x2125\x001A\x2126\x2126\x0009\x2127\x2127\x001A\x2128\x2128\x0009"
        u"\x2129\x2129\x001A\x212A\x212D\x0009\x212E\x212E\x001A\x212F\x212F\x0005\x2130\x2133\x0009\x2134\x2134\x0005\x2135\x2138\x0007\x2139\x2139\x0005"
        u"\x213A\x213B\x001A\x213C\x213D\x0005\x213E\x213F\x0009\x2140\x2144\x0019\x2145\x2145\x0009\x2146\x2149\x0005\x214A\x214A\x001A\x214B\x214B\x0019"
        u"\x214C\x214D\x001A\x214E\x214E\x0005\x214F\x214F\x001A\x2150\x215F\x000F\x2160\x2182\x000E\x2183\x2183\x0009\x2184\x2184\x0005\x2185\x2188\x000E"
        u"\x2189\x2189\x000F\x218A\x218B\x001A\x218C\x218F\x0002\x2190\x2194\x0019\x2195\x2199\x001A\x219A\x219B\x0019\x219C\x219F\x001A\x21A0\x21A0\x0019"
        u"\x21A1\x21A2\x001A\x21A3\x21A3\x0019\x21A4\x21A5\x001A\x21A6\x21A6\x0019\x21A7\x21AD\x001A\x21AE\x21AE\x0019\x21AF\x21CD\x001A\x21CE\x21CF\x0019"
        u"\x21D0\x21D1\x001A\x21D2\x21D2\x0019\x21D3\x21D3\x001A\x21D4\x21D4\x0019\x21D5\x21F3\x001A\x21F4\x22FF\x0019\x2300\x2307\x001A\x2308\x2308\x0016"
        u"\x2309\x2309\x0012\x230A\x230A\x0016\x230B\x230B\x0012\x230C\x231F\x001A\x2320\x2321\x0019\x2322\x2328\x001A\x2329\x2329\x0016\x232A\x232A\x0012"
        u"\x232B\x237B\x001A\x237C\x237C\x0019\x237D\x239A\x001A\x239B\x23B3\x0019\x23B4\x23DB\x001A\x23DC\x23E1\x0019\x23E2\x2426\x001A\x2427\x243F\x0002"
        u"\x2440\x244A\x001A\x244B\x245F\x0002\x2460\x249B\x000F\x249C\x24E9\x001A\x24EA\x24FF\x000F\x2500\x25B6\x001A\x25B7\x25B7\x0019\x25B8\x25C0\x001A"
        u"\x25C1\x25C1\x0019\x25C2\x25F7\x001A\x25F8\x25FF\x0019\x2600\x266E\x001A\x266F\x266F\x0019\x2670\x2767\x001A\x2768\x2768\x0016\x2769\x2769\x0012"
        u"\x276A\x276A\x0016\x276B\x276B\x0012\x276C\x276C\x0016\x276D\x276D\x0012\x276E\x276E\x0016\x276F\x276F\x0012\x2770\x2770\x0016\x2771\x2771\x0012"
        u"\x2772\x2772\x0016\x2773\x2773\x0012\x2774\x2774\x0016\x2775\x2775\x0012\x2776\x2793\x000F\x2794\x27BF\x001A\x27C0\x27C4\x0019\x27C5\x27C5\x0016"
        u"\x27C6\x27C6\x0012\x27C7\x27E5\x0019\x27E6\x27E6\x0016\x27E7\x27E7\x0012\x27E8\x27E8\x0016\x27E9\x27E9\x0012\x27EA\x27EA\x0016\x27EB\x27EB\x0012"
        u"\x27EC\x27EC\x0016\x27ED\x27ED\x0012\x27EE\x27EE\x0016\x27EF\x27EF\x0012\x27F0\x27FF\x0019\x2800\x28FF\x001A\x2900\x2982\x0019\x2983\x2983\x0016"
        u"\x2984\x2984\x0012\x2985\x2985\x0016\x2986\x2986\x0012\x2987\x2987\x0016\x2988\x2988\x0012\x2989\x2989\x0016\x298A\x298A\x0012\x298B\x298B\x0016"
        u"\x298C\x298C\x0012\x298D\x298D\x0016\x298E\x298E\x0012\x298F\x298F\x0016\x2990\x2990\x0012\x2991\x2991\x0016\x2992\x2992\x0012\x2993\x2993\x0016"
        u"\x2994\x2994\x0012\x2995\x2995\x0016\x2996\x2996\x0012\x2997\x2997\x0016\x2998\x2998\x0012\x2999\x29D7\x0019\x29D8\x29D8\x0016\x29D9\x29D9\x0012"
        u"\x29DA\x29DA\x0016\x29DB\x29DB\x0012\x29DC\x29FB\x0019\x29FC\x29FC\x0016\x29FD\x29FD\x0012\x29FE\x2AFF\x0019\x2B00\x2B2F\x001A\x2B30\x2B44\x0019"
        u"\x2B45\x2B46\x001A\x2B47\x2B4C\x0019\x2B4D\x2B73\x001A\x2B74\x2B75\x0002\x2B76\x2B95\x001A\x2B96\x2B96\x0002\x2B97\x2BFF\x001A\x2C00\x2C2F\x0009"
        u"\x2C30\x2C5F\x0005\x2C60\x2C60\x0009\x2C61\x2C61\x0005\x2C62\x2C64\x0009\x2C65\x2C66\x0005\x2C67\x2C67\x0009\x2C68\x2C68\x0005\x2C69\x2C69\x0009"
        u"\x2C6A\x2C6A\x0005\x2C6B\x2C6B\x0009\x2C6C\x2C6C\x0005\x2C6D\x2C70\x0009\x2C71\x2C71\x0005\x2C72\x2C72\x0009\x2C73\x2C74\x0005\x2C75\x2C75\x0009"
        u"\x2C76\x2C7B\x0005\x2C7C\x2C7D\x0006\x2C7E\x2C80\x0009\x2C81\x2C81\x0005\x2C82\x2C82\x0009\x2C83\x2C83\x0005\x2C84\x2C84\x0009\x2C85\x2C85\x0005"
        u"\x2C86\x2C86\x0009\x2C87\x2C87\x0005\x2C88\x2C88\x0009\x2C89\x2C89\x0005\x2C8A\x2C8A\x0009\x2C8B\x2C8B\x0005\x2C8C\x2C8C\x0009\x2C8D\x2C8D\x0005"
        u"\x2C8E\x2C8E\x0009\x2C8F\x2C8F\x0005\x2C90\x2C90\x0009\x2C91\x2C91\x0005\x2C92\x2C92\x0009\x2C93\x2C93\x0005\x2C94\x2C94\x0009\x2C95\x2C95\x0005"
        u"\x2C96\x2C96\x0009\x2C97\x2C97\x0005\x2C98\x2C98\x0009\x2C99\x2C99\x0005\x2C9A\x2C9A\x0009\x2C9B\x2C9B\x0005\x2C9C\x2C9C\x0009\x2C9D\x2C9D\x0005"
        u"\x2C9E\x2C9E\x0009\x2C9F\x2C9F\x0005\x2CA0\x2CA0\x0009\x2CA1\x2CA1\x0005\x2CA2\x2CA2\x0009\x2CA3\x2CA3\x0005\x2CA4\x2CA4\x0009\x2CA5\x2CA5\x0005"
        u"\x2CA6\x2CA6\x0009\x2CA7\x2CA7\x0005\x2CA8\x2CA8\x0009\x2CA9\x2CA9\x0005\x2CAA\x2CAA\x0009\x2CAB\x2CAB\x0005\x2CAC\x2CAC\x0009\x2CAD\x2CAD\x0005"
        u"\x2CAE\x2CAE\x0009\x2CAF\x2CAF\x0005\x2CB0\x2CB0\x0009\x2CB1\x2CB1\x0005\x2CB2\x2CB2\x0009\x2CB3\x2CB3\x0005\x2CB4\x2CB4\x0009\x2CB5\x2CB5\x0005"
        u"\x2CB6\x2CB6\x0009\x2CB7\x2CB7\x0005\x2CB8\x2CB8\x0009\x2CB9\x2CB9\x0005\x2CBA\x2CBA\x0009\x2CBB\x2CBB\x0005\x2CBC\x2CBC\x0009\x2CBD\x2CBD\x0005"
        u"\x2CBE\x2CBE\x0009\x2CBF\x2CBF\x0005\x2CC0\x2CC0\x0009\x2CC1\x2CC1\x0005\x2CC2\x2CC2\x0009\x2CC3\x2CC3\x0005\x2CC4\x2CC4\x0009\x2CC5\x2CC5\x0005"
        u"\x2CC6\x2CC6\x0009\x2CC7\x2CC7\x0005\x2CC8\x2CC8\x0009\x2CC9\x2CC9\x0005\x2CCA\x2CCA\x0009\x2CCB\x2CCB\x0005\x2CCC\x2CCC\x0009\x2CCD\x2CCD\x0005"
        u"\x2CCE\x2CCE\x0009\x2CCF\x2CCF\x0005\x2CD0\x2CD0\x0009\x2CD1\x2CD1\x0005\x2CD2\x2CD2\x0009\x2CD3\x2CD3\x0005\x2CD4\x2CD4\x0009\x2CD5\x2CD5\x0005"
        u"\x2CD6\x2CD6\x0009\x2CD7\x2CD7\x0005\x2CD8\x2CD8\x0009\x2CD9\x2CD9\x0005\x2CDA\x2CDA\x0009\x2CDB\x2CDB\x0005\x2CDC\x2CDC\x0009\x2CDD\x2CDD\x0005"
        u"\x2CDE\x2CDE\x0009\x2CDF\x2CDF\x0005\x2CE0\x2CE0\x0009\x2CE1\x2CE1\x0005\x2CE2\x2CE2\x0009\x2CE3\x2CE4\x0005\x2CE5\x2CEA\x001A\x2CEB\x2CEB\x0009"
        u"\x2CEC\x2CEC\x0005\x2CED\x2CED\x0009\x2CEE\x2CEE\x0005\x2CEF\x2CF1\x000C\x2CF2\x2CF2\x0009\x2CF3\x2CF3\x0005\x2CF4\x2CF8\x0002\x2CF9\x2CFC\x0015"
        u"\x2CFD\x2CFD\x000F\x2CFE\x2CFF\x0015\x2D00\x2D25\x0005\x2D26\x2D26\x0002\x2D27\x2D27\x0005\x2D28\x2D2C\x0002\x2D2D\x2D2D\x0005\x2D2E\x2D2F\x0002"
        u"\x2D30\x2D67\x0007\x2D68\x2D6E\x0002\x2D6F\x2D6F\x0006\x2D70\x2D70\x0015\x2D71\x2D7E\x0002\x2D7F\x2D7F\x000C\x2D80\x2D96\x0007\x2D97\x2D9F\x0002"
        u"\x2DA0\x2DA6\x0007\x2DA7\x2DA7\x0002\x2DA8\x2DAE\x0007\x2DAF\x2DAF\x0002\x2DB0\x2DB6\x0007\x2DB7\x2DB7\x0002\x2DB8\x2DBE\x0007\x2DBF\x2DBF\x0002"
        u"\x2DC0\x2DC6\x0007\x2DC7\x2DC7\x0002\x2DC8\x2DCE\x0007\x2DCF\x2DCF\x0002\x2DD0\x2DD6\x0007\x2DD7\x2DD7\x0002\x2DD8\x2DDE\x0007\x2DDF\x2DDF\x0002"
        u"\x2DE0\x2DFF\x000C\x2E00\x2E01\x0015\x2E02\x2E02\x0014\x2E03\x2E03\x0013\x2E04\x2E04\x0014\x2E05\x2E05\x0013\x2E06\x2E08\x0015\x2E09\x2E09\x0014"
        u"\x2E0A\x2E0A\x0013\x2E0B\x2E0B\x0015\x2E0C\x2E0C\x0014\x2E0D\x2E0D\x0013\x2E0E\x2E16\x0015\x2E17\x2E17\x0011\x2E18\x2E19\x0015\x2E1A\x2E1A\x0011"
        u"\x2E1B\x2E1B\x0015\x2E1C\x2E1C\x0014\x2E1D\x2E1D\x0013\x2E1E\x2E1F\x0015\x2E20\x2E20\x0014\x2E21\x2E21\x0013\x2E22\x2E22\x0016\x2E23\x2E23\x0012"
        u"\x2E24\x2E24\x0016\x2E25\x2E25\x0012\x2E26\x2E26\x0016\x2E27\x2E27\x0012\x2E28\x2E28\x0016\x2E29\x2E29\x0012\x2E2A\x2E2E\x0015\x2E2F\x2E2F\x0006"
        u"\x2E30\x2E39\x0015\x2E3A\x2E3B\x0011\x2E3C\x2E3F\x0015\x2E40\x2E40\x0011\x2E41\x2E41\x0015\x2E42\x2E42\x0016\x2E43\x2E4F\x0015\x2E50\x2E51\x001A"
        u"\x2E52\x2E54\x0015\x2E55\x2E55\x0016\x2E56\x2E56\x0012\x2E57\x2E57\x0016\x2E58\x2E58\x0012\x2E59\x2E59\x0016\x2E5A\x2E5A\x0012\x2E5B\x2E5B\x0016"
        u"\x2E5C\x2E5C\x0012\x2E5D\x2E5D\x0011\x2E5E\x2E7F\x0002\x2E80\x2E99\x001A\x2E9A\x2E9A\x0002\x2E9B\x2EF3\x001A\x2EF4\x2EFF\x0002\x2F00\x2FD5\x001A"
        u"\x2FD6\x2FEF\x0002\x2FF0\x2FFF\x001A\x3000\x3000\x001D\x3001\x3003\x0015\x3004\x3004\x001A\x3005\x3005\x0006\x3006\x3006\x0007\x3007\x3007\x000E"
        u"\x3008\x3008\x0016\x3009\x3009\x0012\x300A\x300A\x0016\x300B\x300B\x0012\x300C\x300C\x0016\x300D\x300D\x0012\x300E\x300E\x0016\x300F\x300F\x0012"
        u"\x3010\x3010\x0016\x3011\x3011\x0012\x3012\x3013\x001A\x3014\x3014\x0016\x3015\x3015\x0012\x3016\x3016\x0016\x3017\x3017\x0012\x3018\x3018\x0016"
        u"\x3019\x3019\x0012\x301A\x301A\x0016\x301B\x301B\x0012\x301C\x301C\x0011\x301D\x301D\x0016\x301E\x301F\x0012\x3020\x3020\x001A\x3021\x3029\x000E"
        u"\x302A\x302D\x000C\x302E\x302F\x000A\x3030\x3030\x0011\x3031\x3035\x0006\x3036\x3037\x001A\x3038\x303A\x000E\x303B\x303B\x0006\x303C\x303C\x0007"
        u"\x303D\x303D\x0015\x303E\x303F\x001A\x3040\x3040\x0002\x3041\x3096\x0007\x3097\x3098\x0002\x3099\x309A\x000C\x309B\x309C\x0018\x309D\x309E\x0006"
        u"\x309F\x309F\x0007\x30A0\x30A0\x0011\x30A1\x30FA\x0007\x30FB\x30FB\x0015\x30FC\x30FE\x0006\x30FF\x30FF\x0007\x3100\x3104\x0002\x3105\x312F\x0007"
        u"\x3130\x3130\x0002\x3131\x318E\x0007\x318F\x318F\x0002\x3190\x3191\x001A\x3192\x3195\x000F\x3196\x319F\x001A\x31A0\x31BF\x0007\x31C0\x31E3\x001A"
        u"\x31E4\x31EE\x0002\x31EF\x31EF\x001A\x31F0\x31FF\x0007\x3200\x321E\x001A\x321F\x321F\x0002\x3220\x3229\x000F\x322A\x3247\x001A\x3248\x324F\x000F"
        u"\x3250\x3250\x001A\x3251\x325F\x000F\x3260\x327F\x001A\x3280\x3289\x000F\x328A\x32B0\x001A\x32B1\x32BF\x000F\x32C0\x33FF\x001A\x3400\x4DBF\x0007"
        u"\x4DC0\x4DFF\x001A\x4E00\xA014\x0007\xA015\xA015\x0006\xA016\xA48C\x0007\xA48D\xA48F\x0002\xA490\xA4C6\x001A\xA4C7\xA4CF\x0002\xA4D0\xA4F7\x0007"
        u"\xA4F8\xA4FD\x0006\xA4FE\xA4FF\x0015\xA500\xA60B\x0007\xA60C\xA60C\x0006\xA60D\xA60F\x0015\xA610\xA61F\x0007\xA620\xA629\x000D\xA62A\xA62B\x0007"
        u"\xA62C\xA63F\x0002\xA640\xA640\x0009\xA641\xA641\x0005\xA642\xA642\x0009\xA643\xA643\x0005\xA644\xA644\x0009\xA645\xA645\x0005\xA646\xA646\x0009"
        u"\xA647\xA647\x0005\xA648\xA648\x0009\xA649\xA649\x0005\xA64A\xA64A\x0009\xA64B\xA64B\x0005\xA64C\xA64C\x0009\xA64D\xA64D\x0005\xA64E\xA64E\x0009"
        u"\xA64F\xA64F\x0005\xA650\xA650\x0009\xA651\xA651\x0005\xA652\xA652\x0009\xA653\xA653\x0005\xA654\xA654\x0009\xA655\xA655\x0005\xA656\xA656\x0009"
        u"\xA657\xA657\x0005\xA658\xA658\x0009\xA659\xA659\x0005\xA65A\xA65A\x0009\xA65B\xA65B\x0005\xA65C\xA65C\x0009\xA65D\xA65D\x0005\xA65E\xA65E\x0009"
        u"\xA65F\xA65F\x0005\xA660\xA660\x0009\xA661\xA661\x0005\xA662\xA662\x0009\xA663\xA663\x0005\xA664\xA664\x0009\xA665\xA665\x0005\xA666\xA666\x0009"
        u"\xA667\xA667\x0005\xA668\xA668\x0009\xA669\xA669\x0005\xA66A\xA66A\x0009\xA66B\xA66B\x0005\xA66C\xA66C\x0009\xA66D\xA66D\x0005\xA66E\xA66E\x0007"
        u"\xA66F\xA66F\x000C\xA670\xA672\x000B\xA673\xA673\x0015\xA674\xA67D\x000C\xA67E\xA67E\x0015\xA67F\xA67F\x0006\xA680\xA680\x0009\xA681\xA681\x0005"
        u"\xA682\xA682\x0009\xA683\xA683\x0005\xA684\xA684\x0009\xA685\xA685\x0005\xA686\xA686\x0009\xA687\xA687\x0005\xA688\xA688\x0009\xA689\xA689\x0005"
        u"\xA68A\xA68A\x0009\xA68B\xA68B\x0005\xA68C\xA68C\x0009\xA68D\xA68D\x0005\xA68E\xA68E\x0009\xA68F\xA68F\x0005\xA690\xA690\x0009\xA691\xA691\x0005"
        u"\xA692\xA692\x0009\xA693\xA693\x0005\xA694\xA694\x0009\xA695\xA695\x0005\xA696\xA696\x0009\xA697\xA697\x0005\xA698\xA698\x0009\xA699\xA699\x0005"
        u"\xA69A\xA69A\x0009\xA69B\xA69B\x0005\xA69C\xA69D\x0006\xA69E\xA69F\x000C\xA6A0\xA6E5\x0007\xA6E6\xA6EF\x000E\xA6F0\xA6F1\x000C\xA6F2\xA6F7\x0015"
        u"\xA6F8\xA6FF\x0002\xA700\xA716\x0018\xA717\xA71F\x0006\xA720\xA721\x0018\xA722\xA722\x0009\xA723\xA723\x0005\xA724\xA724\x0009\xA725\xA725\x0005"
        u"\xA726\xA726\x0009\xA727\xA727\x0005\xA728\xA728\x0009\xA729\xA729\x0005\xA72A\xA72A\x0009\xA72B\xA72B\x0005\xA72C\xA72C\x0009\xA72D\xA72D\x0005"
        u"\xA72E\xA72E\x0009\xA72F\xA731\x0005\xA732\xA732\x0009\xA733\xA733\x0005\xA734\xA734\x0009\xA735\xA735\x0005\xA736\xA736\x0009\xA737\xA737\x0005"
        u"\xA738\xA738\x0009\xA739\xA739\x0005\xA73A\xA73A\x0009\xA73B\xA73B\x0005\xA73C\xA73C\x0009\xA73D\xA73D\x0005\xA73E\xA73E\x0009\xA73F\xA73F\x0005"
        u"\xA740\xA740\x0009\xA741\xA741\x0005\xA742\xA742\x0009\xA743\xA743\x0005\xA744\xA744\x0009\xA745\xA745\x0005\xA746\xA746\x0009\xA747\xA747\x0005"
        u"\xA748\xA748\x0009\xA749\xA749\x0005\xA74A\xA74A\x0009\xA74B\xA74B\x0005\xA74C\xA74C\x0009\xA74D\xA74D\x0005\xA74E\xA74E\x0009\xA74F\xA74F\x0005"
        u"\xA750\xA750\x0009\xA751\xA751\x0005\xA752\xA752\x0009\xA753\xA753\x0005\xA754\xA754\x0009\xA755\xA755\x0005\xA756\xA756\x0009\xA757\xA757\x0005"
        u"\xA758\xA758\x0009\xA759\xA759\x0005\xA75A\xA75A\x0009\xA75B\xA75B\x0005\xA75C\xA75C\x0009\xA75D\xA75D\x0005\xA75E\xA75E\x0009\xA75F\xA75F\x0005"
        u"\xA760\xA760\x0009\xA761\xA761\x0005\xA762\xA762\x0009\xA763\xA763\x0005\xA764\xA764\x0009\xA765\xA765\x0005\xA766\xA766\x0009\xA767\xA767\x0005"
        u"\xA768\xA768\x0009\xA769\xA769\x0005\xA76A\xA76A\x0009\xA76B\xA76B\x0005\xA76C\xA76C\x0009\xA76D\xA76D\x0005\xA76E\xA76E\x0009\xA76F\xA76F\x0005"
        u"\xA770\xA770\x0006\xA771\xA778\x0005\xA779\xA779\x0009\xA77A\xA77A\x0005\xA77B\xA77B\x0009\xA77C\xA77C\x0005\xA77D\xA77E\x0009\xA77F\xA77F\x0005"
        u"\xA780\xA780\x0009\xA781\xA781\x0005\xA782\xA782\x0009\xA783\xA783\x0005\xA784\xA784\x0009\xA785\xA785\x0005\xA786\xA786\x0009\xA787\xA787\x0005"
        u"\xA788\xA788\x0006\xA789\xA78A\x0018\xA78B\xA78B\x0009\xA78C\xA78C\x0005\xA78D\xA78D\x0009\xA78E\xA78E\x0005\xA78F\xA78F\x0007\xA790\xA790\x0009"
        u"\xA791\xA791\x0005\xA792\xA792\x0009\xA793\xA795\x0005\xA796\xA796\x0009\xA797\xA797\x0005\xA798\xA798\x0009\xA799\xA799\x0005\xA79A\xA79A\x0009"
        u"\xA79B\xA79B\x0005\xA79C\xA79C\x0009\xA79D\xA79D\x0005\xA79E\xA79E\x0009\xA79F\xA79F\x0005\xA7A0\xA7A0\x0009\xA7A1\xA7A1\x0005\xA7A2\xA7A2\x0009"
        u"\xA7A3\xA7A3\x0005\xA7A4\xA7A4\x0009\xA7A5\xA7A5\x0005\xA7A6\xA7A6\x0009\xA7A7\xA7A7\x0005\xA7A8\xA7A8\x0009\xA7A9\xA7A9\x0005\xA7AA\xA7AE\x0009"
        u"\xA7AF\xA7AF\x0005\xA7B0\xA7B4\x0009\xA7B5\xA7B5\x0005\xA7B6\xA7B6\x0009\xA7B7\xA7B7\x0005\xA7B8\xA7B8\x0009\xA7B9\xA7B9\x0005\xA7BA\xA7BA\x0009"
        u"\xA7BB\xA7BB\x0005\xA7BC\xA7BC\x0009\xA7BD\xA7BD\x0005\xA7BE\xA7BE\x0009\xA7BF\xA7BF\x0005\xA7C0\xA7C0\x0009\xA7C1\xA7C1\x0005\xA7C2\xA7C2\x0009"
        u"\xA7C3\xA7C3\x0005\xA7C4\xA7C7\x0009\xA7C8\xA7C8\x0005\xA7C9\xA7C9\x0009\xA7CA\xA7CA\x0005\xA7CB\xA7CF\x0002\xA7D0\xA7D0\x0009\xA7D1\xA7D1\x0005"
        u"\xA7D2\xA7D2\x0002\xA7D3\xA7D3\x0005\xA7D4\xA7D4\x0002\xA7D5\xA7D5\x0005\xA7D6\xA7D6\x0009\xA7D7\xA7D7\x0005\xA7D8\xA7D8\x0009\xA7D9\xA7D9\x0005"
        u"\xA7DA\xA7F1\x0002\xA7F2\xA7F4\x0006\xA7F5\xA7F5\x0009\xA7F6\xA7F6\x0005\xA7F7\xA7F7\x0007\xA7F8\xA7F9\x0006\xA7FA\xA7FA\x0005\xA7FB\xA801\x0007"
        u"\xA802\xA802\x000C\xA803\xA805\x0007\xA806\xA806\x000C\xA807\xA80A\x0007\xA80B\xA80B\x000C\xA80C\xA822\x0007\xA823\xA824\x000A\xA825\xA826\x000C"
        u"\xA827\xA827\x000A\xA828\xA82B\x001A\xA82C\xA82C\x000C\xA82D\xA82F\x0002\xA830\xA835\x000F\xA836\xA837\x001A\xA838\xA838\x0017\xA839\xA839\x001A"
        u"\xA83A\xA83F\x0002\xA840\xA873\x0007\xA874\xA877\x0015\xA878\xA87F\x0002\xA880\xA881\x000A\xA882\xA8B3\x0007\xA8B4\xA8C3\x000A\xA8C4\xA8C5\x000C"
        u"\xA8C6\xA8CD\x0002\xA8CE\xA8CF\x0015\xA8D0\xA8D9\x000D\xA8DA\xA8DF\x0002\xA8E0\xA8F1\x000C\xA8F2\xA8F7\x0007\xA8F8\xA8FA\x0015\xA8FB\xA8FB\x0007"
        u"\xA8FC\xA8FC\x0015\xA8FD\xA8FE\x0007\xA8FF\xA8FF\x000C\xA900\xA909\x000D\xA90A\xA925\x0007\xA926\xA92D\x000C\xA92E\xA92F\x0015\xA930\xA946\x0007"
        u"\xA947\xA951\x000C\xA952\xA953\x000A\xA954\xA95E\x0002\xA95F\xA95F\x0015\xA960\xA97C\x0007\xA97D\xA97F\x0002\xA980\xA982\x000C\xA983\xA983\x000A"
        u"\xA984\xA9B2\x0007\xA9B3\xA9B3\x000C\xA9B4\xA9B5\x000A\xA9B6\xA9B9\x000C\xA9BA\xA9BB\x000A\xA9BC\xA9BD\x000C\xA9BE\xA9C0\x000A\xA9C1\xA9CD\x0015"
        u"\xA9CE\xA9CE\x0002\xA9CF\xA9CF\x0006\xA9D0\xA9D9\x000D\xA9DA\xA9DD\x0002\xA9DE\xA9DF\x0015\xA9E0\xA9E4\x0007\xA9E5\xA9E5\x000C\xA9E6\xA9E6\x0006"
        u"\xA9E7\xA9EF\x0007\xA9F0\xA9F9\x000D\xA9FA\xA9FE\x0007\xA9FF\xA9FF\x0002\xAA00\xAA28\x0007\xAA29\xAA2E\x000C\xAA2F\xAA30\x000A\xAA31\xAA32\x000C"
        u"\xAA33\xAA34\x000A\xAA35\xAA36\x000C\xAA37\xAA3F\x0002\xAA40\xAA42\x0007\xAA43\xAA43\x000C\xAA44\xAA4B\x0007\xAA4C\xAA4C\x000C\xAA4D\xAA4D\x000A"
        u"\xAA4E\xAA4F\x0002\xAA50\xAA59\x000D\xAA5A\xAA5B\x0002\xAA5C\xAA5F\x0015\xAA60\xAA6F\x0007\xAA70\xAA70\x0006\xAA71\xAA76\x0007\xAA77\xAA79\x001A"
        u"\xAA7A\xAA7A\x0007\xAA7B\xAA7B\x000A\xAA7C\xAA7C\x000C\xAA7D\xAA7D\x000A\xAA7E\xAAAF\x0007\xAAB0\xAAB0\x000C\xAAB1\xAAB1\x0007\xAAB2\xAAB4\x000C"
        u"\xAAB5\xAAB6\x0007\xAAB7\xAAB8\x000C\xAAB9\xAABD\x0007\xAABE\xAABF\x000C\xAAC0\xAAC0\x0007\xAAC1\xAAC1\x000C\xAAC2\xAAC2\x0007\xAAC3\xAADA\x0002"
        u"\xAADB\xAADC\x0007\xAADD\xAADD\x0006\xAADE\xAADF\x0015\xAAE0\xAAEA\x0007\xAAEB\xAAEB\x000A\xAAEC\xAAED\x000C\xAAEE\xAAEF\x000A\xAAF0\xAAF1\x0015"
        u"\xAAF2\xAAF2\x0007\xAAF3\xAAF4\x0006\xAAF5\xAAF5\x000A\xAAF6\xAAF6\x000C\xAAF7\xAB00\x0002\xAB01\xAB06\x0007\xAB07\xAB08\x0002\xAB09\xAB0E\x0007"
        u"\xAB0F\xAB10\x0002\xAB11\xAB16\x0007\xAB17\xAB1F\x0002\xAB20\xAB26\x0007\xAB27\xAB27\x0002\xAB28\xAB2E\x0007\xAB2F\xAB2F\x0002\xAB30\xAB5A\x0005"
        u"\xAB5B\xAB5B\x0018\xAB5C\xAB5F\x0006\xAB60\xAB68\x0005\xAB69\xAB69\x0006\xAB6A\xAB6B\x0018\xAB6C\xAB6F\x0002\xAB70\xABBF\x0005\xABC0\xABE2\x0007"
        u"\xABE3\xABE4\x000A\xABE5\xABE5\x000C\xABE6\xABE7\x000A\xABE8\xABE8\x000C\xABE9\xABEA\x000A\xABEB\xABEB\x0015\xABEC\xABEC\x000A\xABED\xABED\x000C"
        u"\xABEE\xABEF\x0002\xABF0\xABF9\x000D\xABFA\xABFF\x0002\xAC00\xD7A3\x0007\xD7A4\xD7AF\x0002\xD7B0\xD7C6\x0007\xD7C7\xD7CA\x0002\xD7CB\xD7FB\x0007"
        u"\xD7FC\xD7FF\x0002\xD800\xDFFF\x0004\xE000\xF8FF\x0003\xF900\xFA6D\x0007\xFA6E\xFA6F\x0002\xFA70\xFAD9\x0007\xFADA\xFAFF\x0002\xFB00\xFB06\x0005"
        u"\xFB07\xFB12\x0002\xFB13\xFB17\x0005\xFB18\xFB1C\x0002\xFB1D\xFB1D\x0007\xFB1E\xFB1E\x000C\xFB1F\xFB28\x0007\xFB29\xFB29\x0019\xFB2A\xFB36\x0007"
        u"\xFB37\xFB37\x0002\xFB38\xFB3C\x0007\xFB3D\xFB3D\x0002\xFB3E\xFB3E\x0007\xFB3F\xFB3F\x0002\xFB40\xFB41\x0007\xFB42\xFB42\x0002\xFB43\xFB44\x0007"
        u"\xFB45\xFB45\x0002\xFB46\xFBB1\x0007\xFBB2\xFBC2\x0018\xFBC3\xFBD2\x0002\xFBD3\xFD3D\x0007\xFD3E\xFD3E\x0012\xFD3F\xFD3F\x0016\xFD40\xFD4F\x001A"
        u"\xFD50\xFD8F\x0007\xFD90\xFD91\x0002\xFD92\xFDC7\x0007\xFDC8\xFDCE\x0002\xFDCF\xFDCF\x001A\xFDD0\xFDEF\x0002\xFDF0\xFDFB\x0007\xFDFC\xFDFC\x0017"
        u"\xFDFD\xFDFF\x001A\xFE00\xFE0F\x000C\xFE10\xFE16\x0015\xFE17\xFE17\x0016\xFE18\xFE18\x0012\xFE19\xFE19\x0015\xFE1A\xFE1F\x0002\xFE20\xFE2F\x000C"
        u"\xFE30\xFE30\x0015\xFE31\xFE32\x0011\xFE33\xFE34\x0010\xFE35\xFE35\x0016\xFE36\xFE36\x0012\xFE37\xFE37\x0016\xFE38\xFE38\x0012\xFE39\xFE39\x0016"
        u"\xFE3A\xFE3A\x0012\xFE3B\xFE3B\x0016\xFE3C\xFE3C\x0012\xFE3D\xFE3D\x0016\xFE3E\xFE3E\x0012\xFE3F\xFE3F\x0016\xFE40\xFE40\x0012\xFE41\xFE41\x0016"
        u"\xFE42\xFE42\x0012\xFE43\xFE43\x0016\xFE44\xFE44\x0012\xFE45\xFE46\x0015\xFE47\xFE47\x0016\xFE48\xFE48\x0012\xFE49\xFE4C\x0015\xFE4D\xFE4F\x0010"
        u"\xFE50\xFE52\x0015\xFE53\xFE53\x0002\xFE54\xFE57\x0015\xFE58\xFE58\x0011\xFE59\xFE59\x0016\xFE5A\xFE5A\x0012\xFE5B\xFE5B\x0016\xFE5C\xFE5C\x0012"
        u"\xFE5D\xFE5D\x0016\xFE5E\xFE5E\x0012\xFE5F\xFE61\x0015\xFE62\xFE62\x0019\xFE63\xFE63\x0011\xFE64\xFE66\x0019\xFE67\xFE67\x0002\xFE68\xFE68\x0015"
        u"\xFE69\xFE69\x0017\xFE6A\xFE6B\x0015\xFE6C\xFE6F\x0002\xFE70\xFE74\x0007\xFE75\xFE75\x0002\xFE76\xFEFC\x0007\xFEFD\xFEFE\x0002\xFEFF\xFEFF\x0001"
        u"\xFF00\xFF00\x0002\xFF01\xFF03\x0015\xFF04\xFF04\x0017\xFF05\xFF07\x0015\xFF08\xFF08\x0016\xFF09\xFF09\x0012\xFF0A\xFF0A\x0015\xFF0B\xFF0B\x0019"
        u"\xFF0C\xFF0C\x0015\xFF0D\xFF0D\x0011\xFF0E\xFF0F\x0015\xFF10\xFF19\x000D\xFF1A\xFF1B\x0015\xFF1C\xFF1E\x0019\xFF1F\xFF20\x0015\xFF21\xFF3A\x0009"
        u"\xFF3B\xFF3B\x0016\xFF3C\xFF3C\x0015\xFF3D\xFF3D\x0012\xFF3E\xFF3E\x0018\xFF3F\xFF3F\x0010\xFF40\xFF40\x0018\xFF41\xFF5A\x0005\xFF5B\xFF5B\x0016"
        u"\xFF5C\xFF5C\x0019\xFF5D\xFF5D\x0012\xFF5E\xFF5E\x0019\xFF5F\xFF5F\x0016\xFF60\xFF60\x0012\xFF61\xFF61\x0015\xFF62\xFF62\x0016\xFF63\xFF63\x0012"
        u"\xFF64\xFF65\x0015\xFF66\xFF6F\x0007\xFF70\xFF70\x0006\xFF71\xFF9D\x0007\xFF9E\xFF9F\x0006\xFFA0\xFFBE\x0007\xFFBF\xFFC1\x0002\xFFC2\xFFC7\x0007"
        u"\xFFC8\xFFC9\x0002\xFFCA\xFFCF\x0007\xFFD0\xFFD1\x0002\xFFD2\xFFD7\x0007\xFFD8\xFFD9\x0002\xFFDA\xFFDC\x0007\xFFDD\xFFDF\x0002\xFFE0\xFFE1\x0017"
        u"\xFFE2\xFFE2\x0019\xFFE3\xFFE3\x0018\xFFE4\xFFE4\x001A\xFFE5\xFFE6\x0017\xFFE7\xFFE7\x0002\xFFE8\xFFE8\x001A\xFFE9\xFFEC\x0019\xFFED\xFFEE\x001A"
        u"\xFFEF\xFFF8\x0002\xFFF9\xFFFB\x0001\xFFFC\xFFFD\x001A\xFFFE\xFFFF\x0002";

    [[nodiscard]] UnicodeCategory GetUnicodeCategory(char16_t value) noexcept
    {
        constexpr std::size_t rangeCount = (std::size(UnicodeCategoryData) - 1) / 3;
        const std::uint16_t code = static_cast<std::uint16_t>(value);
        std::size_t low = 0;
        std::size_t high = rangeCount;
        while (low < high)
        {
            const std::size_t middle = low + (high - low) / 2;
            const std::size_t offset = middle * 3;
            const std::uint16_t first = static_cast<std::uint16_t>(UnicodeCategoryData[offset]);
            const std::uint16_t last = static_cast<std::uint16_t>(UnicodeCategoryData[offset + 1]);
            if (code < first)
            {
                high = middle;
            }
            else if (code > last)
            {
                low = middle + 1;
            }
            else
            {
                return static_cast<UnicodeCategory>(
                    static_cast<std::uint16_t>(UnicodeCategoryData[offset + 2]));
            }
        }
        return UnicodeCategory::Cn;
    }

    [[nodiscard]] bool IsWordChar(char16_t value) noexcept
    {
        switch (GetUnicodeCategory(value))
        {
        case UnicodeCategory::Ll:
        case UnicodeCategory::Lu:
        case UnicodeCategory::Lt:
        case UnicodeCategory::Lo:
        case UnicodeCategory::Lm:
        case UnicodeCategory::Mn:
        case UnicodeCategory::Nd:
        case UnicodeCategory::Pc:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] bool IsDigitChar(char16_t value) noexcept
    {
        return GetUnicodeCategory(value) == UnicodeCategory::Nd;
    }

    [[nodiscard]] bool IsSpaceChar(char16_t value) noexcept
    {
        if ((value >= u'\u0009' && value <= u'\u000D') || value == u'\u0085')
        {
            return true;
        }
        const UnicodeCategory category = GetUnicodeCategory(value);
        return category == UnicodeCategory::Zs
            || category == UnicodeCategory::Zl
            || category == UnicodeCategory::Zp;
    }

    [[nodiscard]] std::optional<UnicodeCategory> ParseUnicodeCategory(std::u16string_view name)
    {
        if (name == u"Cc")
        {
            return UnicodeCategory::Cc;
        }
        if (name == u"Cf")
        {
            return UnicodeCategory::Cf;
        }
        if (name == u"Cn")
        {
            return UnicodeCategory::Cn;
        }
        if (name == u"Co")
        {
            return UnicodeCategory::Co;
        }
        if (name == u"Cs")
        {
            return UnicodeCategory::Cs;
        }
        if (name == u"Ll")
        {
            return UnicodeCategory::Ll;
        }
        if (name == u"Lm")
        {
            return UnicodeCategory::Lm;
        }
        if (name == u"Lo")
        {
            return UnicodeCategory::Lo;
        }
        if (name == u"Lt")
        {
            return UnicodeCategory::Lt;
        }
        if (name == u"Lu")
        {
            return UnicodeCategory::Lu;
        }
        if (name == u"Mc")
        {
            return UnicodeCategory::Mc;
        }
        if (name == u"Me")
        {
            return UnicodeCategory::Me;
        }
        if (name == u"Mn")
        {
            return UnicodeCategory::Mn;
        }
        if (name == u"Nd")
        {
            return UnicodeCategory::Nd;
        }
        if (name == u"Nl")
        {
            return UnicodeCategory::Nl;
        }
        if (name == u"No")
        {
            return UnicodeCategory::No;
        }
        if (name == u"Pc")
        {
            return UnicodeCategory::Pc;
        }
        if (name == u"Pd")
        {
            return UnicodeCategory::Pd;
        }
        if (name == u"Pe")
        {
            return UnicodeCategory::Pe;
        }
        if (name == u"Pf")
        {
            return UnicodeCategory::Pf;
        }
        if (name == u"Pi")
        {
            return UnicodeCategory::Pi;
        }
        if (name == u"Po")
        {
            return UnicodeCategory::Po;
        }
        if (name == u"Ps")
        {
            return UnicodeCategory::Ps;
        }
        if (name == u"Sc")
        {
            return UnicodeCategory::Sc;
        }
        if (name == u"Sk")
        {
            return UnicodeCategory::Sk;
        }
        if (name == u"Sm")
        {
            return UnicodeCategory::Sm;
        }
        if (name == u"So")
        {
            return UnicodeCategory::So;
        }
        if (name == u"Zl")
        {
            return UnicodeCategory::Zl;
        }
        if (name == u"Zp")
        {
            return UnicodeCategory::Zp;
        }
        if (name == u"Zs")
        {
            return UnicodeCategory::Zs;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool MatchesCategoryGroup(char16_t value, char16_t group) noexcept
    {
        const UnicodeCategory category = GetUnicodeCategory(value);
        switch (group)
        {
        case u'L':
            return category == UnicodeCategory::Ll
                || category == UnicodeCategory::Lu
                || category == UnicodeCategory::Lt
                || category == UnicodeCategory::Lo
                || category == UnicodeCategory::Lm;
        case u'M':
            return category == UnicodeCategory::Mc
                || category == UnicodeCategory::Me
                || category == UnicodeCategory::Mn;
        case u'N':
            return category == UnicodeCategory::Nd
                || category == UnicodeCategory::Nl
                || category == UnicodeCategory::No;
        case u'P':
            return category == UnicodeCategory::Pc
                || category == UnicodeCategory::Pd
                || category == UnicodeCategory::Pe
                || category == UnicodeCategory::Pf
                || category == UnicodeCategory::Pi
                || category == UnicodeCategory::Po
                || category == UnicodeCategory::Ps;
        case u'S':
            return category == UnicodeCategory::Sc
                || category == UnicodeCategory::Sk
                || category == UnicodeCategory::Sm
                || category == UnicodeCategory::So;
        case u'Z':
            return category == UnicodeCategory::Zl
                || category == UnicodeCategory::Zp
                || category == UnicodeCategory::Zs;
        case u'C':
            return category == UnicodeCategory::Cc
                || category == UnicodeCategory::Cf
                || category == UnicodeCategory::Cn
                || category == UnicodeCategory::Co
                || category == UnicodeCategory::Cs;
        default:
            return false;
        }
    }

    [[nodiscard]] bool MatchesNamedBlock(char16_t value, std::u16string_view name) noexcept
    {
        const std::uint16_t code = static_cast<std::uint16_t>(value);
        struct Block final
        {
            std::u16string_view Name;
            std::uint16_t First;
            std::uint16_t Last;
        };
        static constexpr Block blocks[] = {
            {u"IsBasicLatin", 0x0000, 0x007F},
            {u"IsLatin-1Supplement", 0x0080, 0x00FF},
            {u"IsLatinExtended-A", 0x0100, 0x017F},
            {u"IsLatinExtended-B", 0x0180, 0x024F},
            {u"IsIPAExtensions", 0x0250, 0x02AF},
            {u"IsSpacingModifierLetters", 0x02B0, 0x02FF},
            {u"IsCombiningDiacriticalMarks", 0x0300, 0x036F},
            {u"IsGreek", 0x0370, 0x03FF},
            {u"IsGreekandCoptic", 0x0370, 0x03FF},
            {u"IsCyrillic", 0x0400, 0x04FF},
            {u"IsCyrillicSupplement", 0x0500, 0x052F},
            {u"IsArmenian", 0x0530, 0x058F},
            {u"IsHebrew", 0x0590, 0x05FF},
            {u"IsArabic", 0x0600, 0x06FF},
            {u"IsSyriac", 0x0700, 0x074F},
            {u"IsThaana", 0x0780, 0x07BF},
            {u"IsDevanagari", 0x0900, 0x097F},
            {u"IsBengali", 0x0980, 0x09FF},
            {u"IsGurmukhi", 0x0A00, 0x0A7F},
            {u"IsGujarati", 0x0A80, 0x0AFF},
            {u"IsOriya", 0x0B00, 0x0B7F},
            {u"IsTamil", 0x0B80, 0x0BFF},
            {u"IsTelugu", 0x0C00, 0x0C7F},
            {u"IsKannada", 0x0C80, 0x0CFF}
        };
        for (const Block& block : blocks)
        {
            if (name == block.Name)
            {
                return code >= block.First && code <= block.Last;
            }
        }
        return false;
    }

    [[nodiscard]] bool IsKnownNamedBlock(std::u16string_view name) noexcept
    {
        return MatchesNamedBlock(u'\0', name)
            || name == u"IsBasicLatin"
            || name == u"IsLatin-1Supplement"
            || name == u"IsLatinExtended-A"
            || name == u"IsLatinExtended-B"
            || name == u"IsIPAExtensions"
            || name == u"IsSpacingModifierLetters"
            || name == u"IsCombiningDiacriticalMarks"
            || name == u"IsGreek"
            || name == u"IsGreekandCoptic"
            || name == u"IsCyrillic"
            || name == u"IsCyrillicSupplement"
            || name == u"IsArmenian"
            || name == u"IsHebrew"
            || name == u"IsArabic"
            || name == u"IsSyriac"
            || name == u"IsThaana"
            || name == u"IsDevanagari"
            || name == u"IsBengali"
            || name == u"IsGurmukhi"
            || name == u"IsGujarati"
            || name == u"IsOriya"
            || name == u"IsTamil"
            || name == u"IsTelugu"
            || name == u"IsKannada";
    }

    enum class ClassTermKind : std::uint8_t
    {
        Range,
        Word,
        NotWord,
        Digit,
        NotDigit,
        Space,
        NotSpace,
        Category,
        NotCategory,
        CategoryGroup,
        NotCategoryGroup,
        NamedBlock,
        NotNamedBlock
    };

    struct RegexClassTerm final
    {
        ClassTermKind Kind = ClassTermKind::Range;
        char16_t First = u'\0';
        char16_t Last = u'\0';
        UnicodeCategory Category = UnicodeCategory::Cn;
        char16_t CategoryGroup = u'\0';
        std::u16string Name;

        [[nodiscard]] bool Matches(char16_t value) const noexcept
        {
            switch (Kind)
            {
            case ClassTermKind::Range:
                return value >= First && value <= Last;
            case ClassTermKind::Word:
                return IsWordChar(value);
            case ClassTermKind::NotWord:
                return !IsWordChar(value);
            case ClassTermKind::Digit:
                return IsDigitChar(value);
            case ClassTermKind::NotDigit:
                return !IsDigitChar(value);
            case ClassTermKind::Space:
                return IsSpaceChar(value);
            case ClassTermKind::NotSpace:
                return !IsSpaceChar(value);
            case ClassTermKind::Category:
                return GetUnicodeCategory(value) == Category;
            case ClassTermKind::NotCategory:
                return GetUnicodeCategory(value) != Category;
            case ClassTermKind::CategoryGroup:
                return MatchesCategoryGroup(value, CategoryGroup);
            case ClassTermKind::NotCategoryGroup:
                return !MatchesCategoryGroup(value, CategoryGroup);
            case ClassTermKind::NamedBlock:
                return MatchesNamedBlock(value, Name);
            case ClassTermKind::NotNamedBlock:
                return !MatchesNamedBlock(value, Name);
            }
            return false;
        }
    };

    struct RegexCharClass final
    {
        bool Negated = false;
        std::vector<RegexClassTerm> Terms;
        std::shared_ptr<RegexCharClass> Subtraction;

        [[nodiscard]] bool Matches(char16_t value) const noexcept
        {
            bool included = false;
            for (const RegexClassTerm& term : Terms)
            {
                if (term.Matches(value))
                {
                    included = true;
                    break;
                }
            }
            if (Negated)
            {
                included = !included;
            }
            if (included && Subtraction && Subtraction->Matches(value))
            {
                return false;
            }
            return included;
        }
    };

    enum class RegexNodeKind : std::uint8_t
    {
        Empty,
        Literal,
        Dot,
        CharacterClass,
        Start,
        End,
        AbsoluteStart,
        AbsoluteEnd,
        EndBeforeFinalNewline,
        WordBoundary,
        NotWordBoundary,
        Sequence,
        Alternate,
        Repeat,
        Capture,
        Backreference,
        PositiveLookahead,
        NegativeLookahead,
        PositiveLookbehind,
        NegativeLookbehind
    };

    struct RegexNode final
    {
        RegexNodeKind Kind = RegexNodeKind::Empty;
        char16_t Literal = u'\0';
        std::shared_ptr<RegexCharClass> CharacterClass;
        std::vector<std::shared_ptr<RegexNode>> Children;
        std::size_t Minimum = 0;
        std::size_t Maximum = 0;
        std::size_t Group = 0;
    };

    [[nodiscard]] std::shared_ptr<RegexNode> MakeNode(RegexNodeKind kind)
    {
        auto node = std::make_shared<RegexNode>();
        node->Kind = kind;
        return node;
    }

    [[nodiscard]] std::shared_ptr<RegexNode> MakeLiteral(char16_t value)
    {
        auto node = MakeNode(RegexNodeKind::Literal);
        node->Literal = value;
        return node;
    }

    [[nodiscard]] std::shared_ptr<RegexNode> MakeClass(RegexClassTerm term)
    {
        auto node = MakeNode(RegexNodeKind::CharacterClass);
        node->CharacterClass = std::make_shared<RegexCharClass>();
        node->CharacterClass->Terms.push_back(std::move(term));
        return node;
    }

    class DotNetRegexParser final
    {
    private:
        std::u16string_view _pattern;
        std::size_t _position = 0;
        std::size_t _captureCount = 0;
        std::unordered_map<std::u16string, std::size_t> _namedGroups;

        [[noreturn]] static void ThrowInvalid()
        {
            throw std::invalid_argument("Invalid pattern.");
        }

        [[nodiscard]] bool AtEnd() const noexcept
        {
            return _position >= _pattern.size();
        }

        [[nodiscard]] char16_t Peek(std::size_t offset = 0) const noexcept
        {
            return _position + offset < _pattern.size() ? _pattern[_position + offset] : u'\0';
        }

        char16_t Take()
        {
            if (AtEnd())
            {
                ThrowInvalid();
            }
            return _pattern[_position++];
        }

        static bool IsHex(char16_t value) noexcept
        {
            return (value >= u'0' && value <= u'9')
                || (value >= u'a' && value <= u'f')
                || (value >= u'A' && value <= u'F');
        }

        static unsigned HexValue(char16_t value) noexcept
        {
            if (value >= u'0' && value <= u'9')
            {
                return static_cast<unsigned>(value - u'0');
            }
            if (value >= u'a' && value <= u'f')
            {
                return 10U + static_cast<unsigned>(value - u'a');
            }
            return 10U + static_cast<unsigned>(value - u'A');
        }

        char16_t ParseHex(std::size_t digits)
        {
            std::uint32_t value = 0;
            for (std::size_t index = 0; index < digits; ++index)
            {
                if (AtEnd() || !IsHex(Peek()))
                {
                    ThrowInvalid();
                }
                value = (value << 4U) | HexValue(Take());
            }
            return static_cast<char16_t>(value);
        }

        RegexClassTerm ParseProperty(bool negated)
        {
            if (Take() != u'{')
            {
                ThrowInvalid();
            }
            const std::size_t start = _position;
            while (!AtEnd() && Peek() != u'}')
            {
                ++_position;
            }
            if (AtEnd() || _position == start)
            {
                ThrowInvalid();
            }
            const std::u16string name(_pattern.substr(start, _position - start));
            ++_position;

            RegexClassTerm term;
            if (const std::optional<UnicodeCategory> category = ParseUnicodeCategory(name))
            {
                term.Kind = negated ? ClassTermKind::NotCategory : ClassTermKind::Category;
                term.Category = *category;
                return term;
            }
            if (name.size() == 1
                && (name[0] == u'L' || name[0] == u'M' || name[0] == u'N'
                    || name[0] == u'P' || name[0] == u'S' || name[0] == u'Z'
                    || name[0] == u'C'))
            {
                term.Kind = negated
                    ? ClassTermKind::NotCategoryGroup
                    : ClassTermKind::CategoryGroup;
                term.CategoryGroup = name[0];
                return term;
            }
            if (IsKnownNamedBlock(name))
            {
                term.Kind = negated ? ClassTermKind::NotNamedBlock : ClassTermKind::NamedBlock;
                term.Name = name;
                return term;
            }
            ThrowInvalid();
        }

        struct ClassItem final
        {
            bool IsLiteral = false;
            char16_t Literal = u'\0';
            RegexClassTerm Term;
        };

        ClassItem ParseClassEscape()
        {
            if (AtEnd())
            {
                ThrowInvalid();
            }
            const char16_t escape = Take();
            RegexClassTerm term;
            switch (escape)
            {
            case u'w':
                term.Kind = ClassTermKind::Word;
                return {false, u'\0', std::move(term)};
            case u'W':
                term.Kind = ClassTermKind::NotWord;
                return {false, u'\0', std::move(term)};
            case u'd':
                term.Kind = ClassTermKind::Digit;
                return {false, u'\0', std::move(term)};
            case u'D':
                term.Kind = ClassTermKind::NotDigit;
                return {false, u'\0', std::move(term)};
            case u's':
                term.Kind = ClassTermKind::Space;
                return {false, u'\0', std::move(term)};
            case u'S':
                term.Kind = ClassTermKind::NotSpace;
                return {false, u'\0', std::move(term)};
            case u'p':
                return {false, u'\0', ParseProperty(false)};
            case u'P':
                return {false, u'\0', ParseProperty(true)};
            case u'b':
                return {true, u'\b', {}};
            case u'n':
                return {true, u'\n', {}};
            case u'r':
                return {true, u'\r', {}};
            case u't':
                return {true, u'\t', {}};
            case u'f':
                return {true, u'\f', {}};
            case u'v':
                return {true, u'\v', {}};
            case u'a':
                return {true, u'\a', {}};
            case u'e':
                return {true, static_cast<char16_t>(0x001B), {}};
            case u'u':
                return {true, ParseHex(4), {}};
            case u'x':
            {
                if (_position + 2 > _pattern.size())
                {
                    ThrowInvalid();
                }
                return {true, ParseHex(2), {}};
            }
            default:
                return {true, escape, {}};
            }
        }

        ClassItem ParseClassItem()
        {
            if (AtEnd())
            {
                ThrowInvalid();
            }
            if (Peek() == u'\\')
            {
                ++_position;
                return ParseClassEscape();
            }
            return {true, Take(), {}};
        }

        std::shared_ptr<RegexCharClass> ParseCharacterClass()
        {
            if (Take() != u'[')
            {
                ThrowInvalid();
            }

            auto result = std::make_shared<RegexCharClass>();
            if (Peek() == u'^')
            {
                result->Negated = true;
                ++_position;
            }

            bool first = true;
            bool closed = false;
            while (!AtEnd())
            {
                if (Peek() == u']' && !first)
                {
                    ++_position;
                    closed = true;
                    break;
                }

                if (!first && Peek() == u'-' && Peek(1) == u'[')
                {
                    ++_position;
                    result->Subtraction = ParseCharacterClass();
                    if (AtEnd() || Take() != u']')
                    {
                        ThrowInvalid();
                    }
                    closed = true;
                    break;
                }

                ClassItem firstItem = ParseClassItem();
                first = false;

                if (firstItem.IsLiteral
                    && Peek() == u'-'
                    && Peek(1) != u']'
                    && Peek(1) != u'\0'
                    && Peek(1) != u'[')
                {
                    ++_position;
                    ClassItem secondItem = ParseClassItem();
                    if (!secondItem.IsLiteral || secondItem.Literal < firstItem.Literal)
                    {
                        ThrowInvalid();
                    }
                    RegexClassTerm range;
                    range.Kind = ClassTermKind::Range;
                    range.First = firstItem.Literal;
                    range.Last = secondItem.Literal;
                    result->Terms.push_back(std::move(range));
                }
                else
                {
                    if (firstItem.IsLiteral)
                    {
                        RegexClassTerm literal;
                        literal.Kind = ClassTermKind::Range;
                        literal.First = firstItem.Literal;
                        literal.Last = firstItem.Literal;
                        result->Terms.push_back(std::move(literal));
                    }
                    else
                    {
                        result->Terms.push_back(std::move(firstItem.Term));
                    }
                }
            }

            if (!closed)
            {
                ThrowInvalid();
            }
            return result;
        }

        [[nodiscard]] std::shared_ptr<RegexNode> ParseEscape()
        {
            if (Take() != u'\\' || AtEnd())
            {
                ThrowInvalid();
            }
            const char16_t escape = Take();
            RegexClassTerm term;
            switch (escape)
            {
            case u'w':
                term.Kind = ClassTermKind::Word;
                return MakeClass(std::move(term));
            case u'W':
                term.Kind = ClassTermKind::NotWord;
                return MakeClass(std::move(term));
            case u'd':
                term.Kind = ClassTermKind::Digit;
                return MakeClass(std::move(term));
            case u'D':
                term.Kind = ClassTermKind::NotDigit;
                return MakeClass(std::move(term));
            case u's':
                term.Kind = ClassTermKind::Space;
                return MakeClass(std::move(term));
            case u'S':
                term.Kind = ClassTermKind::NotSpace;
                return MakeClass(std::move(term));
            case u'p':
                return MakeClass(ParseProperty(false));
            case u'P':
                return MakeClass(ParseProperty(true));
            case u'b':
                return MakeNode(RegexNodeKind::WordBoundary);
            case u'B':
                return MakeNode(RegexNodeKind::NotWordBoundary);
            case u'A':
                return MakeNode(RegexNodeKind::AbsoluteStart);
            case u'z':
                return MakeNode(RegexNodeKind::AbsoluteEnd);
            case u'Z':
                return MakeNode(RegexNodeKind::EndBeforeFinalNewline);
            case u'n':
                return MakeLiteral(u'\n');
            case u'r':
                return MakeLiteral(u'\r');
            case u't':
                return MakeLiteral(u'\t');
            case u'f':
                return MakeLiteral(u'\f');
            case u'v':
                return MakeLiteral(u'\v');
            case u'a':
                return MakeLiteral(u'\a');
            case u'e':
                return MakeLiteral(static_cast<char16_t>(0x001B));
            case u'u':
                return MakeLiteral(ParseHex(4));
            case u'x':
                return MakeLiteral(ParseHex(2));
            case u'k':
            {
                if (AtEnd() || (Peek() != u'<' && Peek() != u'\''))
                {
                    ThrowInvalid();
                }
                const char16_t close = Take() == u'<' ? u'>' : u'\'';
                const std::size_t start = _position;
                while (!AtEnd() && Peek() != close)
                {
                    ++_position;
                }
                if (AtEnd() || _position == start)
                {
                    ThrowInvalid();
                }
                const std::u16string name(_pattern.substr(start, _position - start));
                ++_position;
                const auto iterator = _namedGroups.find(name);
                if (iterator == _namedGroups.end())
                {
                    ThrowInvalid();
                }
                auto node = MakeNode(RegexNodeKind::Backreference);
                node->Group = iterator->second;
                return node;
            }
            default:
                if (escape >= u'1' && escape <= u'9')
                {
                    std::size_t group = static_cast<std::size_t>(escape - u'0');
                    while (!AtEnd() && Peek() >= u'0' && Peek() <= u'9')
                    {
                        const std::size_t next = group * 10 + static_cast<std::size_t>(Peek() - u'0');
                        if (next > _captureCount)
                        {
                            break;
                        }
                        group = next;
                        ++_position;
                    }
                    if (group == 0 || group > _captureCount)
                    {
                        ThrowInvalid();
                    }
                    auto node = MakeNode(RegexNodeKind::Backreference);
                    node->Group = group;
                    return node;
                }
                return MakeLiteral(escape);
            }
        }

        [[nodiscard]] std::shared_ptr<RegexNode> ParseGroup()
        {
            if (Take() != u'(')
            {
                ThrowInvalid();
            }

            RegexNodeKind wrapper = RegexNodeKind::Capture;
            std::size_t group = 0;
            bool capture = true;

            if (Peek() == u'?')
            {
                ++_position;
                switch (Take())
                {
                case u':':
                    capture = false;
                    wrapper = RegexNodeKind::Empty;
                    break;
                case u'=':
                    capture = false;
                    wrapper = RegexNodeKind::PositiveLookahead;
                    break;
                case u'!':
                    capture = false;
                    wrapper = RegexNodeKind::NegativeLookahead;
                    break;
                case u'<':
                    if (Peek() == u'=')
                    {
                        ++_position;
                        capture = false;
                        wrapper = RegexNodeKind::PositiveLookbehind;
                    }
                    else if (Peek() == u'!')
                    {
                        ++_position;
                        capture = false;
                        wrapper = RegexNodeKind::NegativeLookbehind;
                    }
                    else
                    {
                        const std::size_t start = _position;
                        while (!AtEnd() && Peek() != u'>')
                        {
                            ++_position;
                        }
                        if (AtEnd() || _position == start)
                        {
                            ThrowInvalid();
                        }
                        std::u16string name(_pattern.substr(start, _position - start));
                        ++_position;
                        group = ++_captureCount;
                        if (!_namedGroups.emplace(std::move(name), group).second)
                        {
                            ThrowInvalid();
                        }
                    }
                    break;
                case u'\'':
                {
                    const std::size_t start = _position;
                    while (!AtEnd() && Peek() != u'\'')
                    {
                        ++_position;
                    }
                    if (AtEnd() || _position == start)
                    {
                        ThrowInvalid();
                    }
                    std::u16string name(_pattern.substr(start, _position - start));
                    ++_position;
                    group = ++_captureCount;
                    if (!_namedGroups.emplace(std::move(name), group).second)
                    {
                        ThrowInvalid();
                    }
                    break;
                }
                case u'>':
                    capture = false;
                    wrapper = RegexNodeKind::Empty;
                    break;
                default:
                    ThrowInvalid();
                }
            }
            else
            {
                group = ++_captureCount;
            }

            std::shared_ptr<RegexNode> child = ParseAlternation();
            if (AtEnd() || Take() != u')')
            {
                ThrowInvalid();
            }

            if (!capture && wrapper == RegexNodeKind::Empty)
            {
                return child;
            }
            auto node = MakeNode(wrapper);
            node->Children.push_back(std::move(child));
            if (capture)
            {
                node->Group = group;
            }
            return node;
        }

        [[nodiscard]] std::shared_ptr<RegexNode> ParseAtom()
        {
            if (AtEnd())
            {
                return MakeNode(RegexNodeKind::Empty);
            }

            switch (Peek())
            {
            case u'^':
                ++_position;
                return MakeNode(RegexNodeKind::Start);
            case u'$':
                ++_position;
                return MakeNode(RegexNodeKind::End);
            case u'.':
                ++_position;
                return MakeNode(RegexNodeKind::Dot);
            case u'[':
            {
                auto node = MakeNode(RegexNodeKind::CharacterClass);
                node->CharacterClass = ParseCharacterClass();
                return node;
            }
            case u'(':
                return ParseGroup();
            case u'\\':
                return ParseEscape();
            case u'*':
            case u'+':
            case u'?':
                ThrowInvalid();
            default:
                return MakeLiteral(Take());
            }
        }

        [[nodiscard]] bool TryParseUnsigned(std::size_t& value)
        {
            if (AtEnd() || Peek() < u'0' || Peek() > u'9')
            {
                return false;
            }
            value = 0;
            while (!AtEnd() && Peek() >= u'0' && Peek() <= u'9')
            {
                const std::size_t digit = static_cast<std::size_t>(Take() - u'0');
                if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10)
                {
                    ThrowInvalid();
                }
                value = value * 10 + digit;
            }
            return true;
        }

        [[nodiscard]] std::shared_ptr<RegexNode> ParseQuantified()
        {
            std::shared_ptr<RegexNode> atom = ParseAtom();
            if (AtEnd())
            {
                return atom;
            }

            std::size_t minimum = 0;
            std::size_t maximum = 0;
            bool quantified = true;
            if (Peek() == u'*')
            {
                ++_position;
                maximum = std::numeric_limits<std::size_t>::max();
            }
            else if (Peek() == u'+')
            {
                ++_position;
                minimum = 1;
                maximum = std::numeric_limits<std::size_t>::max();
            }
            else if (Peek() == u'?')
            {
                ++_position;
                maximum = 1;
            }
            else if (Peek() == u'{')
            {
                const std::size_t save = _position++;
                if (!TryParseUnsigned(minimum))
                {
                    _position = save;
                    quantified = false;
                }
                else if (Peek() == u'}')
                {
                    ++_position;
                    maximum = minimum;
                }
                else if (Peek() == u',')
                {
                    ++_position;
                    if (Peek() == u'}')
                    {
                        maximum = std::numeric_limits<std::size_t>::max();
                        ++_position;
                    }
                    else
                    {
                        if (!TryParseUnsigned(maximum) || AtEnd() || Take() != u'}' || maximum < minimum)
                        {
                            ThrowInvalid();
                        }
                    }
                }
                else
                {
                    _position = save;
                    quantified = false;
                }
            }
            else
            {
                quantified = false;
            }

            if (!quantified)
            {
                return atom;
            }
            if (!AtEnd() && Peek() == u'?')
            {
                ++_position;
            }

            auto repeat = MakeNode(RegexNodeKind::Repeat);
            repeat->Minimum = minimum;
            repeat->Maximum = maximum;
            repeat->Children.push_back(std::move(atom));
            return repeat;
        }

        [[nodiscard]] std::shared_ptr<RegexNode> ParseSequence()
        {
            std::vector<std::shared_ptr<RegexNode>> nodes;
            while (!AtEnd() && Peek() != u')' && Peek() != u'|')
            {
                nodes.push_back(ParseQuantified());
            }
            if (nodes.empty())
            {
                return MakeNode(RegexNodeKind::Empty);
            }
            if (nodes.size() == 1)
            {
                return nodes.front();
            }
            auto sequence = MakeNode(RegexNodeKind::Sequence);
            sequence->Children = std::move(nodes);
            return sequence;
        }

        [[nodiscard]] std::shared_ptr<RegexNode> ParseAlternation()
        {
            std::vector<std::shared_ptr<RegexNode>> nodes;
            nodes.push_back(ParseSequence());
            while (!AtEnd() && Peek() == u'|')
            {
                ++_position;
                nodes.push_back(ParseSequence());
            }
            if (nodes.size() == 1)
            {
                return nodes.front();
            }
            auto alternate = MakeNode(RegexNodeKind::Alternate);
            alternate->Children = std::move(nodes);
            return alternate;
        }

    public:
        explicit DotNetRegexParser(std::u16string_view pattern)
            : _pattern(pattern)
        {
        }

        [[nodiscard]] std::shared_ptr<RegexNode> Parse()
        {
            std::shared_ptr<RegexNode> result = ParseAlternation();
            if (!AtEnd())
            {
                ThrowInvalid();
            }
            return result;
        }

        [[nodiscard]] std::size_t CaptureCount() const noexcept
        {
            return _captureCount;
        }
    };

    struct RegexCapture final
    {
        bool HasValue = false;
        std::size_t Start = 0;
        std::size_t End = 0;
    };

    struct RegexMatchState final
    {
        std::size_t Position = 0;
        std::vector<RegexCapture> Captures;
    };

    using RegexStates = std::vector<RegexMatchState>;

    [[nodiscard]] RegexStates MatchRegexNode(
        const RegexNode& node,
        std::u16string_view input,
        const RegexMatchState& state);

    void MatchRepeat(
        const RegexNode& node,
        std::u16string_view input,
        const RegexMatchState& state,
        std::size_t count,
        RegexStates& result)
    {
        if (count >= node.Minimum)
        {
            result.push_back(state);
        }
        if (count == node.Maximum)
        {
            return;
        }

        const RegexStates next = MatchRegexNode(*node.Children.front(), input, state);
        for (const RegexMatchState& candidate : next)
        {
            if (candidate.Position == state.Position)
            {
                if (count + 1 >= node.Minimum)
                {
                    result.push_back(candidate);
                }
                continue;
            }
            MatchRepeat(node, input, candidate, count + 1, result);
        }
    }

    [[nodiscard]] RegexStates MatchRegexNode(
        const RegexNode& node,
        std::u16string_view input,
        const RegexMatchState& state)
    {
        switch (node.Kind)
        {
        case RegexNodeKind::Empty:
            return {state};

        case RegexNodeKind::Literal:
            if (state.Position < input.size() && input[state.Position] == node.Literal)
            {
                RegexMatchState next = state;
                ++next.Position;
                return {std::move(next)};
            }
            return {};

        case RegexNodeKind::Dot:
            if (state.Position < input.size() && input[state.Position] != u'\n')
            {
                RegexMatchState next = state;
                ++next.Position;
                return {std::move(next)};
            }
            return {};

        case RegexNodeKind::CharacterClass:
            if (state.Position < input.size() && node.CharacterClass->Matches(input[state.Position]))
            {
                RegexMatchState next = state;
                ++next.Position;
                return {std::move(next)};
            }
            return {};

        case RegexNodeKind::Start:
        case RegexNodeKind::AbsoluteStart:
            return state.Position == 0 ? RegexStates{state} : RegexStates{};

        case RegexNodeKind::End:
        case RegexNodeKind::EndBeforeFinalNewline:
            if (state.Position == input.size()
                || (state.Position + 1 == input.size() && input[state.Position] == u'\n'))
            {
                return {state};
            }
            return {};

        case RegexNodeKind::AbsoluteEnd:
            return state.Position == input.size() ? RegexStates{state} : RegexStates{};

        case RegexNodeKind::WordBoundary:
        case RegexNodeKind::NotWordBoundary:
        {
            const bool left = state.Position > 0 && IsWordChar(input[state.Position - 1]);
            const bool right = state.Position < input.size() && IsWordChar(input[state.Position]);
            const bool boundary = left != right;
            const bool matches = node.Kind == RegexNodeKind::WordBoundary ? boundary : !boundary;
            return matches ? RegexStates{state} : RegexStates{};
        }

        case RegexNodeKind::Sequence:
        {
            RegexStates states{state};
            for (const std::shared_ptr<RegexNode>& child : node.Children)
            {
                RegexStates next;
                for (const RegexMatchState& current : states)
                {
                    RegexStates produced = MatchRegexNode(*child, input, current);
                    next.insert(
                        next.end(),
                        std::make_move_iterator(produced.begin()),
                        std::make_move_iterator(produced.end()));
                }
                states = std::move(next);
                if (states.empty())
                {
                    break;
                }
            }
            return states;
        }

        case RegexNodeKind::Alternate:
        {
            RegexStates states;
            for (const std::shared_ptr<RegexNode>& child : node.Children)
            {
                RegexStates produced = MatchRegexNode(*child, input, state);
                states.insert(
                    states.end(),
                    std::make_move_iterator(produced.begin()),
                    std::make_move_iterator(produced.end()));
            }
            return states;
        }

        case RegexNodeKind::Repeat:
        {
            RegexStates states;
            MatchRepeat(node, input, state, 0, states);
            return states;
        }

        case RegexNodeKind::Capture:
        {
            RegexMatchState start = state;
            if (node.Group >= start.Captures.size())
            {
                return {};
            }
            const std::size_t captureStart = state.Position;
            RegexStates states = MatchRegexNode(*node.Children.front(), input, start);
            for (RegexMatchState& next : states)
            {
                next.Captures[node.Group] = {true, captureStart, next.Position};
            }
            return states;
        }

        case RegexNodeKind::Backreference:
        {
            if (node.Group >= state.Captures.size() || !state.Captures[node.Group].HasValue)
            {
                return {};
            }
            const RegexCapture& capture = state.Captures[node.Group];
            const std::size_t length = capture.End - capture.Start;
            if (state.Position + length > input.size()
                || input.substr(state.Position, length) != input.substr(capture.Start, length))
            {
                return {};
            }
            RegexMatchState next = state;
            next.Position += length;
            return {std::move(next)};
        }

        case RegexNodeKind::PositiveLookahead:
        {
            RegexStates states = MatchRegexNode(*node.Children.front(), input, state);
            for (RegexMatchState& next : states)
            {
                next.Position = state.Position;
            }
            return states;
        }

        case RegexNodeKind::NegativeLookahead:
            return MatchRegexNode(*node.Children.front(), input, state).empty()
                ? RegexStates{state}
                : RegexStates{};

        case RegexNodeKind::PositiveLookbehind:
        case RegexNodeKind::NegativeLookbehind:
        {
            RegexStates successful;
            for (std::size_t start = 0; start <= state.Position; ++start)
            {
                RegexMatchState candidate = state;
                candidate.Position = start;
                RegexStates produced = MatchRegexNode(*node.Children.front(), input, candidate);
                for (RegexMatchState& next : produced)
                {
                    if (next.Position == state.Position)
                    {
                        next.Position = state.Position;
                        successful.push_back(std::move(next));
                    }
                }
            }
            if (node.Kind == RegexNodeKind::PositiveLookbehind)
            {
                return successful;
            }
            return successful.empty() ? RegexStates{state} : RegexStates{};
        }
        }
        return {};
    }
}

namespace NCSFCommon
{
    thread_local NumberFormatInfo CurrentCultureNumberFormat = LoadExecutionThreadNumberFormat();

    void Common::ThrowNotSupported()
    {
        throw std::logic_error("Specified method is not supported.");
    }

    class Regex::Impl final
    {
    private:
        std::u16string _pattern;
        std::shared_ptr<RegexNode> _root;
        std::size_t _captureCount = 0;

    public:
        explicit Impl(std::u16string pattern)
            : _pattern(std::move(pattern))
        {
            DotNetRegexParser parser(_pattern);
            _root = parser.Parse();
            _captureCount = parser.CaptureCount();
        }

        [[nodiscard]] bool IsMatch(std::u16string_view input) const
        {
            RegexMatchState initial;
            initial.Captures.resize(_captureCount + 1);
            const RegexStates states = MatchRegexNode(*_root, input, initial);
            return !states.empty();
        }

        [[nodiscard]] const std::u16string& Pattern() const noexcept
        {
            return _pattern;
        }
    };

    Regex::Regex(std::u16string pattern)
        : _impl(std::make_shared<const Impl>(std::move(pattern)))
    {
    }

    bool Regex::IsMatch(std::u16string_view input) const
    {
        return _impl->IsMatch(input);
    }

    const std::u16string& Regex::ToString() const noexcept
    {
        return _impl->Pattern();
    }

    namespace
    {
        constexpr std::array<std::uint8_t, 4> DataStorage = {
            static_cast<std::uint8_t>('D'),
            static_cast<std::uint8_t>('A'),
            static_cast<std::uint8_t>('T'),
            static_cast<std::uint8_t>('A')
        };
    }

    const ReadOnlyMemory<std::uint8_t> Common::DataBytes(DataStorage.data(), DataStorage.size());

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

    Regex Common::WildcardStringToRegex(std::u16string_view wildcard)
    {
        std::u16string pattern(wildcard);
        pattern = ReplaceAll(std::move(pattern), u"?", u".");
        pattern = ReplaceAll(std::move(pattern), u"*", u".*");
        pattern.insert(pattern.begin(), u'^');
        pattern.push_back(u'$');
        return Regex(std::move(pattern));
    }

    Common::KeepInfo::KeepInfo(std::u16string filename, KeepType keep)
        : Filename(std::move(filename)),
          Keep(keep)
    {
    }

    const std::type_info& Common::KeepInfo::EqualityContract() const noexcept
    {
        return typeid(*this);
    }

    bool Common::KeepInfo::Equals(const KeepInfo* other) const noexcept
    {
        return other != nullptr
            && EqualityContract() == other->EqualityContract()
            && Filename == other->Filename
            && Keep == other->Keep;
    }

    bool Common::KeepInfo::operator==(const KeepInfo& other) const noexcept
    {
        return Equals(std::addressof(other));
    }

    bool Common::KeepInfo::operator!=(const KeepInfo& other) const noexcept
    {
        return !Equals(std::addressof(other));
    }

    std::size_t Common::KeepInfo::GetHashCode() const noexcept
    {
        std::size_t hash = EqualityContract().hash_code();
        hash = RecordHashCombine(hash, std::hash<std::u16string>{}(Filename));
        hash = RecordHashCombine(
            hash, std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(Keep)));
        return hash;
    }

    bool Common::KeepInfo::PrintMembers(std::u16string& result) const
    {
        result += u"Filename = ";
        result += Filename;
        result += u", Keep = ";
        result += KeepTypeName(Keep);
        return true;
    }

    std::u16string Common::KeepInfo::ToString() const
    {
        std::u16string result = u"KeepInfo { ";
        if (PrintMembers(result))
        {
            result += u" ";
        }
        result += u"}";
        return result;
    }

    void Common::KeepInfo::Deconstruct(std::u16string& filename, KeepType& keep) const
    {
        filename = Filename;
        keep = Keep;
    }

    std::shared_ptr<Common::KeepInfo> Common::KeepInfo::Clone() const
    {
        return std::shared_ptr<KeepInfo>(new KeepInfo(*this));
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
            DebugAssert(parts.size() <= 2, "parts.Length <= 2");
            if (parts.size() == 2)
            {
                if (WildcardStringToRegex(parts[0]).IsMatch(sdatNumber)
                    && WildcardStringToRegex(parts[1]).IsMatch(filename))
                {
                    keep = info->Keep;
                }
            }
            else if (WildcardStringToRegex(info->Filename).IsMatch(filename))
            {
                keep = info->Keep;
            }
        }
        return keep;
    }

    std::u16string Common::SecondsToString(float seconds)
    {
        const std::int32_t minutes = FloatToInt32Unchecked(seconds / 60.0F);
        seconds -= static_cast<float>(MultiplyUnchecked(minutes, 60));

        const NumberFormatInfo& format = CurrentCultureNumberFormat;
        std::u16string result = FormatIntD2(minutes, format);
        result.push_back(u':');
        result += FormatSeconds(seconds, format);
        return result;
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
        DebugAssert(colons <= 2, "colons <= 2");

        std::int32_t seconds;
        if (colons == 1)
        {
            const std::vector<std::u16string_view> ranges = SplitPreserveEmpty(time, u':');
            const std::int32_t first = ParseInt32(ranges[0]);
            const std::int32_t firstSeconds = MultiplyUnchecked(first, 60);
            const std::int32_t second = ParseInt32(ranges[1]);
            seconds = AddUnchecked(firstSeconds, second);
        }
        else if (colons == 2)
        {
            const std::vector<std::u16string_view> ranges = SplitPreserveEmpty(time, u':');
            const std::int32_t first = ParseInt32(ranges[0]);
            const std::int32_t firstSeconds = MultiplyUnchecked(first, 3600);
            const std::int32_t second = ParseInt32(ranges[1]);
            const std::int32_t secondSeconds = MultiplyUnchecked(second, 60);
            const std::int32_t firstTwo = AddUnchecked(firstSeconds, secondSeconds);
            const std::int32_t third = ParseInt32(ranges[2]);
            seconds = AddUnchecked(firstTwo, third);
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
