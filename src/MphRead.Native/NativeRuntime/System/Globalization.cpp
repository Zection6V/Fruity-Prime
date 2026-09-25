#include "Globalization.hpp"

#include <array>

#include "Encoding.hpp"
#include "Exceptions.hpp"
#include "HashCode.hpp"
#include "Icu.hpp"
#include "Managed.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <clocale>
#include <locale.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        struct CodeUnitRange
        {
            char16_t First;
            char16_t Last;
        };

        // CharUnicodeInfo's letter (Lu, Ll, Lt, Lm, Lo) and Nd ranges in the
        // BMP.
        constexpr std::array<CodeUnitRange, 401> LetterOrDigitRanges{{
            {0x0030, 0x0039}, {0x0041, 0x005A}, {0x0061, 0x007A}, {0x00AA, 0x00AA},
            {0x00B5, 0x00B5}, {0x00BA, 0x00BA}, {0x00C0, 0x00D6}, {0x00D8, 0x00F6},
            {0x00F8, 0x02C1}, {0x02C6, 0x02D1}, {0x02E0, 0x02E4}, {0x02EC, 0x02EC},
            {0x02EE, 0x02EE}, {0x0370, 0x0374}, {0x0376, 0x0377}, {0x037A, 0x037D},
            {0x037F, 0x037F}, {0x0386, 0x0386}, {0x0388, 0x038A}, {0x038C, 0x038C},
            {0x038E, 0x03A1}, {0x03A3, 0x03F5}, {0x03F7, 0x0481}, {0x048A, 0x052F},
            {0x0531, 0x0556}, {0x0559, 0x0559}, {0x0560, 0x0588}, {0x05D0, 0x05EA},
            {0x05EF, 0x05F2}, {0x0620, 0x064A}, {0x0660, 0x0669}, {0x066E, 0x066F},
            {0x0671, 0x06D3}, {0x06D5, 0x06D5}, {0x06E5, 0x06E6}, {0x06EE, 0x06FC},
            {0x06FF, 0x06FF}, {0x0710, 0x0710}, {0x0712, 0x072F}, {0x074D, 0x07A5},
            {0x07B1, 0x07B1}, {0x07C0, 0x07EA}, {0x07F4, 0x07F5}, {0x07FA, 0x07FA},
            {0x0800, 0x0815}, {0x081A, 0x081A}, {0x0824, 0x0824}, {0x0828, 0x0828},
            {0x0840, 0x0858}, {0x0860, 0x086A}, {0x0870, 0x0887}, {0x0889, 0x088E},
            {0x08A0, 0x08C9}, {0x0904, 0x0939}, {0x093D, 0x093D}, {0x0950, 0x0950},
            {0x0958, 0x0961}, {0x0966, 0x096F}, {0x0971, 0x0980}, {0x0985, 0x098C},
            {0x098F, 0x0990}, {0x0993, 0x09A8}, {0x09AA, 0x09B0}, {0x09B2, 0x09B2},
            {0x09B6, 0x09B9}, {0x09BD, 0x09BD}, {0x09CE, 0x09CE}, {0x09DC, 0x09DD},
            {0x09DF, 0x09E1}, {0x09E6, 0x09F1}, {0x09FC, 0x09FC}, {0x0A05, 0x0A0A},
            {0x0A0F, 0x0A10}, {0x0A13, 0x0A28}, {0x0A2A, 0x0A30}, {0x0A32, 0x0A33},
            {0x0A35, 0x0A36}, {0x0A38, 0x0A39}, {0x0A59, 0x0A5C}, {0x0A5E, 0x0A5E},
            {0x0A66, 0x0A6F}, {0x0A72, 0x0A74}, {0x0A85, 0x0A8D}, {0x0A8F, 0x0A91},
            {0x0A93, 0x0AA8}, {0x0AAA, 0x0AB0}, {0x0AB2, 0x0AB3}, {0x0AB5, 0x0AB9},
            {0x0ABD, 0x0ABD}, {0x0AD0, 0x0AD0}, {0x0AE0, 0x0AE1}, {0x0AE6, 0x0AEF},
            {0x0AF9, 0x0AF9}, {0x0B05, 0x0B0C}, {0x0B0F, 0x0B10}, {0x0B13, 0x0B28},
            {0x0B2A, 0x0B30}, {0x0B32, 0x0B33}, {0x0B35, 0x0B39}, {0x0B3D, 0x0B3D},
            {0x0B5C, 0x0B5D}, {0x0B5F, 0x0B61}, {0x0B66, 0x0B6F}, {0x0B71, 0x0B71},
            {0x0B83, 0x0B83}, {0x0B85, 0x0B8A}, {0x0B8E, 0x0B90}, {0x0B92, 0x0B95},
            {0x0B99, 0x0B9A}, {0x0B9C, 0x0B9C}, {0x0B9E, 0x0B9F}, {0x0BA3, 0x0BA4},
            {0x0BA8, 0x0BAA}, {0x0BAE, 0x0BB9}, {0x0BD0, 0x0BD0}, {0x0BE6, 0x0BEF},
            {0x0C05, 0x0C0C}, {0x0C0E, 0x0C10}, {0x0C12, 0x0C28}, {0x0C2A, 0x0C39},
            {0x0C3D, 0x0C3D}, {0x0C58, 0x0C5A}, {0x0C5D, 0x0C5D}, {0x0C60, 0x0C61},
            {0x0C66, 0x0C6F}, {0x0C80, 0x0C80}, {0x0C85, 0x0C8C}, {0x0C8E, 0x0C90},
            {0x0C92, 0x0CA8}, {0x0CAA, 0x0CB3}, {0x0CB5, 0x0CB9}, {0x0CBD, 0x0CBD},
            {0x0CDD, 0x0CDE}, {0x0CE0, 0x0CE1}, {0x0CE6, 0x0CEF}, {0x0CF1, 0x0CF2},
            {0x0D04, 0x0D0C}, {0x0D0E, 0x0D10}, {0x0D12, 0x0D3A}, {0x0D3D, 0x0D3D},
            {0x0D4E, 0x0D4E}, {0x0D54, 0x0D56}, {0x0D5F, 0x0D61}, {0x0D66, 0x0D6F},
            {0x0D7A, 0x0D7F}, {0x0D85, 0x0D96}, {0x0D9A, 0x0DB1}, {0x0DB3, 0x0DBB},
            {0x0DBD, 0x0DBD}, {0x0DC0, 0x0DC6}, {0x0DE6, 0x0DEF}, {0x0E01, 0x0E30},
            {0x0E32, 0x0E33}, {0x0E40, 0x0E46}, {0x0E50, 0x0E59}, {0x0E81, 0x0E82},
            {0x0E84, 0x0E84}, {0x0E86, 0x0E8A}, {0x0E8C, 0x0EA3}, {0x0EA5, 0x0EA5},
            {0x0EA7, 0x0EB0}, {0x0EB2, 0x0EB3}, {0x0EBD, 0x0EBD}, {0x0EC0, 0x0EC4},
            {0x0EC6, 0x0EC6}, {0x0ED0, 0x0ED9}, {0x0EDC, 0x0EDF}, {0x0F00, 0x0F00},
            {0x0F20, 0x0F29}, {0x0F40, 0x0F47}, {0x0F49, 0x0F6C}, {0x0F88, 0x0F8C},
            {0x1000, 0x102A}, {0x103F, 0x1049}, {0x1050, 0x1055}, {0x105A, 0x105D},
            {0x1061, 0x1061}, {0x1065, 0x1066}, {0x106E, 0x1070}, {0x1075, 0x1081},
            {0x108E, 0x108E}, {0x1090, 0x1099}, {0x10A0, 0x10C5}, {0x10C7, 0x10C7},
            {0x10CD, 0x10CD}, {0x10D0, 0x10FA}, {0x10FC, 0x1248}, {0x124A, 0x124D},
            {0x1250, 0x1256}, {0x1258, 0x1258}, {0x125A, 0x125D}, {0x1260, 0x1288},
            {0x128A, 0x128D}, {0x1290, 0x12B0}, {0x12B2, 0x12B5}, {0x12B8, 0x12BE},
            {0x12C0, 0x12C0}, {0x12C2, 0x12C5}, {0x12C8, 0x12D6}, {0x12D8, 0x1310},
            {0x1312, 0x1315}, {0x1318, 0x135A}, {0x1380, 0x138F}, {0x13A0, 0x13F5},
            {0x13F8, 0x13FD}, {0x1401, 0x166C}, {0x166F, 0x167F}, {0x1681, 0x169A},
            {0x16A0, 0x16EA}, {0x16F1, 0x16F8}, {0x1700, 0x1711}, {0x171F, 0x1731},
            {0x1740, 0x1751}, {0x1760, 0x176C}, {0x176E, 0x1770}, {0x1780, 0x17B3},
            {0x17D7, 0x17D7}, {0x17DC, 0x17DC}, {0x17E0, 0x17E9}, {0x1810, 0x1819},
            {0x1820, 0x1878}, {0x1880, 0x1884}, {0x1887, 0x18A8}, {0x18AA, 0x18AA},
            {0x18B0, 0x18F5}, {0x1900, 0x191E}, {0x1946, 0x196D}, {0x1970, 0x1974},
            {0x1980, 0x19AB}, {0x19B0, 0x19C9}, {0x19D0, 0x19D9}, {0x1A00, 0x1A16},
            {0x1A20, 0x1A54}, {0x1A80, 0x1A89}, {0x1A90, 0x1A99}, {0x1AA7, 0x1AA7},
            {0x1B05, 0x1B33}, {0x1B45, 0x1B4C}, {0x1B50, 0x1B59}, {0x1B83, 0x1BA0},
            {0x1BAE, 0x1BE5}, {0x1C00, 0x1C23}, {0x1C40, 0x1C49}, {0x1C4D, 0x1C7D},
            {0x1C80, 0x1C88}, {0x1C90, 0x1CBA}, {0x1CBD, 0x1CBF}, {0x1CE9, 0x1CEC},
            {0x1CEE, 0x1CF3}, {0x1CF5, 0x1CF6}, {0x1CFA, 0x1CFA}, {0x1D00, 0x1DBF},
            {0x1E00, 0x1F15}, {0x1F18, 0x1F1D}, {0x1F20, 0x1F45}, {0x1F48, 0x1F4D},
            {0x1F50, 0x1F57}, {0x1F59, 0x1F59}, {0x1F5B, 0x1F5B}, {0x1F5D, 0x1F5D},
            {0x1F5F, 0x1F7D}, {0x1F80, 0x1FB4}, {0x1FB6, 0x1FBC}, {0x1FBE, 0x1FBE},
            {0x1FC2, 0x1FC4}, {0x1FC6, 0x1FCC}, {0x1FD0, 0x1FD3}, {0x1FD6, 0x1FDB},
            {0x1FE0, 0x1FEC}, {0x1FF2, 0x1FF4}, {0x1FF6, 0x1FFC}, {0x2071, 0x2071},
            {0x207F, 0x207F}, {0x2090, 0x209C}, {0x2102, 0x2102}, {0x2107, 0x2107},
            {0x210A, 0x2113}, {0x2115, 0x2115}, {0x2119, 0x211D}, {0x2124, 0x2124},
            {0x2126, 0x2126}, {0x2128, 0x2128}, {0x212A, 0x212D}, {0x212F, 0x2139},
            {0x213C, 0x213F}, {0x2145, 0x2149}, {0x214E, 0x214E}, {0x2183, 0x2184},
            {0x2C00, 0x2CE4}, {0x2CEB, 0x2CEE}, {0x2CF2, 0x2CF3}, {0x2D00, 0x2D25},
            {0x2D27, 0x2D27}, {0x2D2D, 0x2D2D}, {0x2D30, 0x2D67}, {0x2D6F, 0x2D6F},
            {0x2D80, 0x2D96}, {0x2DA0, 0x2DA6}, {0x2DA8, 0x2DAE}, {0x2DB0, 0x2DB6},
            {0x2DB8, 0x2DBE}, {0x2DC0, 0x2DC6}, {0x2DC8, 0x2DCE}, {0x2DD0, 0x2DD6},
            {0x2DD8, 0x2DDE}, {0x2E2F, 0x2E2F}, {0x3005, 0x3006}, {0x3031, 0x3035},
            {0x303B, 0x303C}, {0x3041, 0x3096}, {0x309D, 0x309F}, {0x30A1, 0x30FA},
            {0x30FC, 0x30FF}, {0x3105, 0x312F}, {0x3131, 0x318E}, {0x31A0, 0x31BF},
            {0x31F0, 0x31FF}, {0x3400, 0x4DBF}, {0x4E00, 0xA48C}, {0xA4D0, 0xA4FD},
            {0xA500, 0xA60C}, {0xA610, 0xA62B}, {0xA640, 0xA66E}, {0xA67F, 0xA69D},
            {0xA6A0, 0xA6E5}, {0xA717, 0xA71F}, {0xA722, 0xA788}, {0xA78B, 0xA7CA},
            {0xA7D0, 0xA7D1}, {0xA7D3, 0xA7D3}, {0xA7D5, 0xA7D9}, {0xA7F2, 0xA801},
            {0xA803, 0xA805}, {0xA807, 0xA80A}, {0xA80C, 0xA822}, {0xA840, 0xA873},
            {0xA882, 0xA8B3}, {0xA8D0, 0xA8D9}, {0xA8F2, 0xA8F7}, {0xA8FB, 0xA8FB},
            {0xA8FD, 0xA8FE}, {0xA900, 0xA925}, {0xA930, 0xA946}, {0xA960, 0xA97C},
            {0xA984, 0xA9B2}, {0xA9CF, 0xA9D9}, {0xA9E0, 0xA9E4}, {0xA9E6, 0xA9FE},
            {0xAA00, 0xAA28}, {0xAA40, 0xAA42}, {0xAA44, 0xAA4B}, {0xAA50, 0xAA59},
            {0xAA60, 0xAA76}, {0xAA7A, 0xAA7A}, {0xAA7E, 0xAAAF}, {0xAAB1, 0xAAB1},
            {0xAAB5, 0xAAB6}, {0xAAB9, 0xAABD}, {0xAAC0, 0xAAC0}, {0xAAC2, 0xAAC2},
            {0xAADB, 0xAADD}, {0xAAE0, 0xAAEA}, {0xAAF2, 0xAAF4}, {0xAB01, 0xAB06},
            {0xAB09, 0xAB0E}, {0xAB11, 0xAB16}, {0xAB20, 0xAB26}, {0xAB28, 0xAB2E},
            {0xAB30, 0xAB5A}, {0xAB5C, 0xAB69}, {0xAB70, 0xABE2}, {0xABF0, 0xABF9},
            {0xAC00, 0xD7A3}, {0xD7B0, 0xD7C6}, {0xD7CB, 0xD7FB}, {0xF900, 0xFA6D},
            {0xFA70, 0xFAD9}, {0xFB00, 0xFB06}, {0xFB13, 0xFB17}, {0xFB1D, 0xFB1D},
            {0xFB1F, 0xFB28}, {0xFB2A, 0xFB36}, {0xFB38, 0xFB3C}, {0xFB3E, 0xFB3E},
            {0xFB40, 0xFB41}, {0xFB43, 0xFB44}, {0xFB46, 0xFBB1}, {0xFBD3, 0xFD3D},
            {0xFD50, 0xFD8F}, {0xFD92, 0xFDC7}, {0xFDF0, 0xFDFB}, {0xFE70, 0xFE74},
            {0xFE76, 0xFEFC}, {0xFF10, 0xFF19}, {0xFF21, 0xFF3A}, {0xFF41, 0xFF5A},
            {0xFF66, 0xFFBE}, {0xFFC2, 0xFFC7}, {0xFFCA, 0xFFCF}, {0xFFD2, 0xFFD7},
            {0xFFDA, 0xFFDC}
        }};
    }

    bool CharIsLetterOrDigit(char16_t value) noexcept
    {
        std::size_t low = 0;
        std::size_t high = LetterOrDigitRanges.size();
        while (low < high)
        {
            const std::size_t mid = low + (high - low) / 2;
            const CodeUnitRange range = LetterOrDigitRanges[mid];
            if (value < range.First)
            {
                high = mid;
            }
            else if (value > range.Last)
            {
                low = mid + 1;
            }
            else
            {
                return true;
            }
        }
        return false;
    }

    // ==== White space, trimming, replacing =====================================

    bool StringIsNullOrWhiteSpace(std::string_view value) noexcept
    {
        // Character by character, as string.IsNullOrWhiteSpace does: a UTF-8
        // byte is not a character, and U+3000 alone is white space.
        for (std::size_t offset = 0; offset < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, offset);
            if (!CharIsWhiteSpace(scalar.Value))
            {
                return false;
            }
            offset += scalar.Length;
        }
        return true;
    }

    bool StringIsNullOrWhiteSpace(const std::string& value) noexcept
    {
        return StringIsNullOrWhiteSpace(std::string_view(value));
    }

    bool StringIsNullOrWhiteSpace(const char* value) noexcept
    {
        return value == nullptr || StringIsNullOrWhiteSpace(std::string_view(value));
    }

    bool StringIsNullOrWhiteSpace(const std::optional<std::string>& value) noexcept
    {
        return !value.has_value() || StringIsNullOrWhiteSpace(std::string_view(*value));
    }

    std::string_view StringTrimView(std::string_view value) noexcept
    {
        while (!value.empty())
        {
            const Utf8Scalar first = DecodeUtf8Scalar(value, 0);
            if (!CharIsWhiteSpace(first.Value))
            {
                break;
            }
            value.remove_prefix(first.Length);
        }
        while (!value.empty())
        {
            const Utf8Scalar last = DecodeLastUtf8Scalar(value, value.size());
            if (!CharIsWhiteSpace(last.Value))
            {
                break;
            }
            value.remove_suffix(last.Length);
        }
        return value;
    }

    std::string StringTrim(std::string_view value)
    {
        return std::string(StringTrimView(value));
    }

    std::string StringReplace(std::string value, std::string_view oldValue, std::string_view newValue)
    {
        if (oldValue.empty())
        {
            throw System::ArgumentException("String cannot be of zero length. (Parameter 'oldValue')");
        }
        std::string result;
        std::size_t start = 0;
        for (std::size_t found = value.find(oldValue); found != std::string::npos;
             found = value.find(oldValue, start))
        {
            result.append(value, start, found - start);
            result.append(newValue);
            start = found + oldValue.size();
        }
        if (start == 0)
        {
            return value;
        }
        result.append(value, start, std::string::npos);
        return result;
    }

    std::string StringPadLeft(std::string value, std::size_t totalWidth, char padding)
    {
        const std::size_t length = Utf16Length(value);
        if (length < totalWidth)
        {
            value.insert(0, totalWidth - length, padding);
        }
        return value;
    }

    std::string StringPadRight(std::string value, std::size_t totalWidth, char padding)
    {
        const std::size_t length = Utf16Length(value);
        if (length < totalWidth)
        {
            value.append(totalWidth - length, padding);
        }
        return value;
    }

    std::int32_t MathRoundToInt32(double value) noexcept
    {
        // Math.Round(double) is MidpointRounding.ToEven, and the (int) cast
        // saturates as .NET 9 casts.
        return ConvertToInt32Net9(std::nearbyint(value));
    }

    std::string AsciiGetString(std::span<const std::uint8_t> bytes)
    {
        std::string result;
        result.reserve(bytes.size());
        for (const std::uint8_t value : bytes)
        {
            result += value > 0x7FU ? '?' : static_cast<char>(value);
        }
        return result;
    }

    // ==== Case =================================================================

    namespace
    {
        using CaseFunction = std::int32_t (*)(std::int32_t);

        // Unicode's simple case mapping for one code point: ICU's where it is
        // loaded, which is where .NET gets it, and the platform's otherwise.
        [[nodiscard]] char32_t SimpleCase(char32_t value, bool upper) noexcept
        {
            if (value < 0x80U)
            {
                if (upper && value >= U'a' && value <= U'z')
                {
                    return value - (U'a' - U'A');
                }
                if (!upper && value >= U'A' && value <= U'Z')
                {
                    return value + (U'a' - U'A');
                }
                return value;
            }
            if (value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
            {
                return value;
            }
            static const CaseFunction toUpper = Icu::Function<CaseFunction>("u_toupper");
            static const CaseFunction toLower = Icu::Function<CaseFunction>("u_tolower");
            if (const CaseFunction function = upper ? toUpper : toLower; function != nullptr)
            {
                const std::int32_t mapped = function(static_cast<std::int32_t>(value));
                return mapped < 0 ? value : static_cast<char32_t>(mapped);
            }
#if defined(_WIN32)
            if (value <= 0xFFFFU)
            {
                const wchar_t source = static_cast<wchar_t>(value);
                wchar_t target = source;
                if (LCMapStringEx(LOCALE_NAME_INVARIANT, upper ? LCMAP_UPPERCASE : LCMAP_LOWERCASE,
                        &source, 1, &target, 1, nullptr, nullptr, 0) == 1)
                {
                    return static_cast<char32_t>(target);
                }
            }
            return value;
#else
            static const locale_t locale = []() noexcept
            {
                locale_t result = newlocale(LC_CTYPE_MASK, "C.UTF-8", static_cast<locale_t>(0));
                if (result == static_cast<locale_t>(0))
                {
                    result = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", static_cast<locale_t>(0));
                }
                return result;
            }();
            if (locale == static_cast<locale_t>(0))
            {
                return value;
            }
            const wint_t mapped = upper
                ? towupper_l(static_cast<wint_t>(value), locale)
                : towlower_l(static_cast<wint_t>(value), locale);
            return mapped == WEOF ? value : static_cast<char32_t>(mapped);
#endif
        }

        template <class TMap>
        [[nodiscard]] std::string MapScalars(std::string_view value, TMap map)
        {
            std::string result;
            result.reserve(value.size());
            for (std::size_t offset = 0; offset < value.size();)
            {
                const Utf8Scalar scalar = DecodeUtf8Scalar(value, offset);
                if (scalar.Valid())
                {
                    AppendUtf8(result, map(scalar.Value));
                }
                else
                {
                    // Bytes that are not UTF-8 pass through as they came.
                    result.append(value, offset, scalar.Length);
                }
                offset += scalar.Length;
            }
            return result;
        }

        // Compares two strings scalar by scalar under OrdinalCasing, up to
        // the end of the shorter. `leftUsed`/`rightUsed` are the bytes each
        // consumed; the result is the first difference, or 0.
        [[nodiscard]] std::int32_t CompareFolded(std::string_view left, std::string_view right,
            std::size_t& leftUsed, std::size_t& rightUsed) noexcept
        {
            leftUsed = 0;
            rightUsed = 0;
            while (leftUsed < left.size() && rightUsed < right.size())
            {
                const Utf8Scalar a = DecodeUtf8Scalar(left, leftUsed);
                const Utf8Scalar b = DecodeUtf8Scalar(right, rightUsed);
                const char32_t upperA = a.Valid() ? OrdinalCasingToUpper(a.Value)
                    : 0x110000U + static_cast<unsigned char>(left[leftUsed]);
                const char32_t upperB = b.Valid() ? OrdinalCasingToUpper(b.Value)
                    : 0x110000U + static_cast<unsigned char>(right[rightUsed]);
                if (upperA != upperB)
                {
                    return upperA < upperB ? -1 : 1;
                }
                leftUsed += a.Length;
                rightUsed += b.Length;
            }
            return 0;
        }
    }

    char32_t ToUpperInvariant(char32_t value) noexcept
    {
        // pal_casing.c ChangeCaseInvariant: the dotless i stays as it is.
        return value == 0x0131U ? value : SimpleCase(value, true);
    }

    char32_t ToLowerInvariant(char32_t value) noexcept
    {
        // And the dotted capital I.
        return value == 0x0130U ? value : SimpleCase(value, false);
    }

    std::string ToUpperInvariant(std::string_view value)
    {
        return MapScalars(value, [](char32_t scalar) { return ToUpperInvariant(scalar); });
    }

    std::string ToLowerInvariant(std::string_view value)
    {
        return MapScalars(value, [](char32_t scalar) { return ToLowerInvariant(scalar); });
    }

    namespace
    {
        [[nodiscard]] bool TurkishCasing()
        {
            // TextInfo.NeedsTurkishCasing: the culture's language is tr or az.
            static const bool turkish = []
            {
                const std::string& name = CurrentCultureName();
                const std::string language = name.substr(0, name.find('-'));
                return language == "tr" || language == "az";
            }();
            return turkish;
        }
    }

    std::string ToUpperCurrentCulture(std::string_view value)
    {
        if (CurrentCultureName().empty())
        {
            return ToUpperInvariant(value);
        }
        // pal_casing.c ChangeCase / ChangeCaseTurkish.
        const bool turkish = TurkishCasing();
        return MapScalars(value, [turkish](char32_t scalar)
        {
            if (scalar == U'i' && turkish)
            {
                return char32_t{ 0x0130 };
            }
            return scalar == 0x0131U ? U'I' : SimpleCase(scalar, true);
        });
    }

    std::string ToLowerCurrentCulture(std::string_view value)
    {
        if (CurrentCultureName().empty())
        {
            return ToLowerInvariant(value);
        }
        const bool turkish = TurkishCasing();
        return MapScalars(value, [turkish](char32_t scalar)
        {
            if (scalar == U'I' && turkish)
            {
                return char32_t{ 0x0131 };
            }
            return scalar == 0x0130U ? U'i' : SimpleCase(scalar, false);
        });
    }

    char32_t OrdinalCasingToUpper(char32_t value) noexcept
    {
        // OrdinalCasing's table keeps these two apart from 'I' and 'S'.
        if (value == 0x0131U || value == 0x017FU)
        {
            return value;
        }
        return SimpleCase(value, true);
    }

    bool StringEqualsOrdinalIgnoreCase(std::string_view left, std::string_view right) noexcept
    {
        std::size_t leftUsed = 0;
        std::size_t rightUsed = 0;
        return CompareFolded(left, right, leftUsed, rightUsed) == 0
            && leftUsed == left.size() && rightUsed == right.size();
    }

    bool StringStartsWithOrdinalIgnoreCase(std::string_view value, std::string_view prefix) noexcept
    {
        std::size_t valueUsed = 0;
        std::size_t prefixUsed = 0;
        return CompareFolded(value, prefix, valueUsed, prefixUsed) == 0 && prefixUsed == prefix.size();
    }

    bool StringEndsWithOrdinalIgnoreCase(std::string_view value, std::string_view suffix) noexcept
    {
        // Suffix matching walks back scalar by scalar so a multi-byte
        // character in `value` is never cut in half.
        std::size_t valueEnd = value.size();
        std::size_t suffixEnd = suffix.size();
        while (suffixEnd > 0)
        {
            if (valueEnd == 0)
            {
                return false;
            }
            const Utf8Scalar a = DecodeLastUtf8Scalar(value, valueEnd);
            const Utf8Scalar b = DecodeLastUtf8Scalar(suffix, suffixEnd);
            if (a.Valid() != b.Valid())
            {
                return false;
            }
            if (a.Valid() ? OrdinalCasingToUpper(a.Value) != OrdinalCasingToUpper(b.Value)
                          : value.substr(valueEnd - a.Length, a.Length) != suffix.substr(suffixEnd - b.Length, b.Length))
            {
                return false;
            }
            valueEnd -= a.Length;
            suffixEnd -= b.Length;
        }
        return true;
    }

    std::int32_t StringCompareOrdinalIgnoreCase(std::string_view left, std::string_view right) noexcept
    {
        std::size_t leftUsed = 0;
        std::size_t rightUsed = 0;
        if (const std::int32_t compared = CompareFolded(left, right, leftUsed, rightUsed); compared != 0)
        {
            return compared;
        }
        const bool leftDone = leftUsed == left.size();
        const bool rightDone = rightUsed == right.size();
        return leftDone == rightDone ? 0 : (leftDone ? -1 : 1);
    }

    std::int32_t StringHashOrdinalIgnoreCase(std::string_view value) noexcept
    {
        // xxHash32 over the upper-cased scalars, with HashCode's seed.
        using namespace HashCodeDetail;
        std::uint32_t hash = Seed() + Prime5;
        std::uint32_t length = 0;
        for (std::size_t offset = 0; offset < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, offset);
            const std::uint32_t unit = scalar.Valid()
                ? static_cast<std::uint32_t>(OrdinalCasingToUpper(scalar.Value))
                : 0x110000U + static_cast<unsigned char>(value[offset]);
            hash = QueueRound(hash, unit);
            ++length;
            offset += scalar.Length;
        }
        hash += length * 4U;
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    std::string StringReplaceOrdinalIgnoreCase(std::string_view value, std::string_view oldValue, std::string_view newValue)
    {
        if (oldValue.empty())
        {
            throw System::ArgumentException("String cannot be of zero length. (Parameter 'oldValue')");
        }
        std::string result;
        std::size_t start = 0;
        while (start <= value.size())
        {
            const std::ptrdiff_t found = StringIndexOfOrdinalIgnoreCase(value.substr(start), oldValue);
            if (found < 0)
            {
                break;
            }
            result.append(value.substr(start, static_cast<std::size_t>(found)));
            result.append(newValue);
            start += static_cast<std::size_t>(found) + oldValue.size();
        }
        result.append(value.substr(std::min(start, value.size())));
        return result;
    }

    std::vector<std::string> StringSplit(std::string_view value, char separator, bool removeEmptyEntries, bool trimEntries)
    {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t found = value.find(separator, start);
            std::string_view part = value.substr(start, found == std::string_view::npos ? std::string_view::npos : found - start);
            if (trimEntries)
            {
                part = StringTrimView(part);
            }
            if (!removeEmptyEntries || !part.empty())
            {
                parts.emplace_back(part);
            }
            if (found == std::string_view::npos)
            {
                break;
            }
            start = found + 1;
        }
        return parts;
    }

    std::ptrdiff_t StringIndexOfOrdinalIgnoreCase(std::string_view value, std::string_view search) noexcept
    {
        if (search.empty())
        {
            return 0;
        }
        for (std::size_t offset = 0; offset < value.size();)
        {
            if (StringStartsWithOrdinalIgnoreCase(value.substr(offset), search))
            {
                return static_cast<std::ptrdiff_t>(offset);
            }
            offset += DecodeUtf8Scalar(value, offset).Length;
        }
        return -1;
    }

    // ==== The current culture ===================================================

    const std::string& CurrentCultureName()
    {
        static const std::string name = []() -> std::string
        {
            if (Icu::InvariantMode())
            {
                return {};
            }
#if defined(_WIN32)
            wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
            if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0)
            {
                return {};
            }
            return WideToUtf8(localeName);
#else
            using DefaultLocale = const char* (*)();
            const DefaultLocale defaultLocale = Icu::Function<DefaultLocale>("uloc_getDefault");
            const char* raw = defaultLocale != nullptr ? defaultLocale() : nullptr;
            if (raw == nullptr)
            {
                return {};
            }
            // DetectDefaultLocaleName: the POSIX locale is the invariant culture.
            std::string locale = raw;
            if (locale == "en_US_POSIX")
            {
                return {};
            }
            // FixupLocaleName: '_' is '-' in a managed culture name.
            if (const std::size_t at = locale.find('@'); at != std::string::npos)
            {
                locale.resize(at);
            }
            std::replace(locale.begin(), locale.end(), '_', '-');
            return locale;
#endif
        }();
        return name;
    }

    namespace
    {
        // CompareInfo.GetIsAsciiEqualityOrdinal.
        [[nodiscard]] bool IsAsciiEqualityOrdinal()
        {
            static const bool value = []
            {
                const std::string& name = CurrentCultureName();
                return name.empty()
                    || (name.size() >= 2 && name[0] == 'e' && name[1] == 'n'
                        && (name.size() == 2 || name[2] == '-'));
            }();
            return value;
        }

        // CompareInfo.HighCharTable: [0x00, 0x1F] and 0x7F need ICU, except \t \v \f.
        [[nodiscard]] bool IsHighChar(char16_t value) noexcept
        {
            if (value >= 0x80U)
            {
                return true;
            }
            if (value == 0x09U || value == 0x0BU || value == 0x0CU)
            {
                return false;
            }
            return value <= 0x1FU || value == 0x7FU;
        }

        constexpr std::int32_t UColPrimary = 0;
        constexpr std::int32_t UColSecondary = 1;
        constexpr std::int32_t UColNullOrder = static_cast<std::int32_t>(0xFFFFFFFFU);
        constexpr std::int32_t UColIgnorable = 0;
        constexpr std::int32_t UColPrimaryOrderMask = static_cast<std::int32_t>(0xFFFF0000U);
        constexpr std::int32_t UColSecondaryOrderMask = 0x0000FF00;
        constexpr std::int32_t UColTertiaryOrderMask = 0x000000FF;

        struct Collation final
        {
            using StrcollFn = std::int32_t (*)(const void*, const char16_t*, std::int32_t,
                const char16_t*, std::int32_t);
            using OpenFn = void* (*)(const char*, std::int32_t*);
            using CloseFn = void (*)(void*);
            using GetStrengthFn = std::int32_t (*)(const void*);
            using OpenElementsFn = void* (*)(const void*, const char16_t*, std::int32_t, std::int32_t*);
            using StepFn = std::int32_t (*)(void*, std::int32_t*);
            using CloseElementsFn = void (*)(void*);

            OpenFn Open = Icu::Function<OpenFn>("ucol_open");
            CloseFn Close = Icu::Function<CloseFn>("ucol_close");
            GetStrengthFn GetStrength = Icu::Function<GetStrengthFn>("ucol_getStrength");
            OpenElementsFn OpenElements = Icu::Function<OpenElementsFn>("ucol_openElements");
            StepFn Next = Icu::Function<StepFn>("ucol_next");
            StepFn Previous = Icu::Function<StepFn>("ucol_previous");
            CloseElementsFn CloseElements = Icu::Function<CloseElementsFn>("ucol_closeElements");
            StrcollFn Strcoll = Icu::Function<StrcollFn>("ucol_strcoll");
            // GlobalizationNative_GetSortHandle: one collator for the culture, shared.
            void* Collator = nullptr;

            Collation()
            {
                if (Open == nullptr || Close == nullptr || GetStrength == nullptr || OpenElements == nullptr
                    || Next == nullptr || Previous == nullptr || CloseElements == nullptr)
                {
                    return;
                }
                std::int32_t status = 0;
                void* collator = Open(CurrentCultureName().c_str(), &status);
                if (status > 0)
                {
                    if (collator != nullptr)
                    {
                        Close(collator);
                    }
                    return;
                }
                Collator = collator;
            }
        };

        [[nodiscard]] const Collation& CurrentCollation()
        {
            static const Collation collation;
            return collation;
        }

        [[nodiscard]] std::int32_t CollationElementMask(std::int32_t strength) noexcept
        {
            switch (strength)
            {
            case UColPrimary:
                return UColPrimaryOrderMask;
            case UColSecondary:
                return UColPrimaryOrderMask | UColSecondaryOrderMask;
            default:
                return UColPrimaryOrderMask | UColSecondaryOrderMask | UColTertiaryOrderMask;
            }
        }

        // pal_collation.c SimpleAffix_Iterators.
        [[nodiscard]] bool SimpleAffixIterators(const Collation& api, void* patternIterator,
            void* sourceIterator, std::int32_t strength, bool forwardSearch)
        {
            std::int32_t errorCode = 0;
            bool movePattern = true;
            bool moveSource = true;
            std::int32_t patternElement = UColIgnorable;
            std::int32_t sourceElement = UColIgnorable;
            const std::int32_t mask = CollationElementMask(strength);
            while (true)
            {
                if (movePattern)
                {
                    patternElement = forwardSearch
                        ? api.Next(patternIterator, &errorCode)
                        : api.Previous(patternIterator, &errorCode);
                }
                if (moveSource)
                {
                    sourceElement = forwardSearch
                        ? api.Next(sourceIterator, &errorCode)
                        : api.Previous(sourceIterator, &errorCode);
                }
                movePattern = true;
                moveSource = true;
                if (patternElement == UColNullOrder)
                {
                    if (sourceElement == UColNullOrder || sourceElement == UColIgnorable)
                    {
                        return true;
                    }
                    return !(forwardSearch && (sourceElement & UColPrimaryOrderMask) == 0
                        && (sourceElement & UColSecondaryOrderMask) != 0);
                }
                if (patternElement == UColIgnorable)
                {
                    moveSource = false;
                }
                else if (sourceElement == UColIgnorable)
                {
                    movePattern = false;
                }
                else if ((patternElement & mask) != (sourceElement & mask))
                {
                    return false;
                }
            }
        }

        // pal_collation.c SimpleAffix, with a given collator.
        [[nodiscard]] bool SimpleAffixWith(void* collator, std::u16string_view pattern, std::u16string_view text,
            bool forwardSearch)
        {
            const Collation& api = CurrentCollation();
            if (collator == nullptr || api.OpenElements == nullptr)
            {
                return false;
            }
            bool result = false;
            std::int32_t errorCode = 0;
            void* patternIterator = api.OpenElements(collator, pattern.data(),
                static_cast<std::int32_t>(pattern.size()), &errorCode);
            if (errorCode <= 0)
            {
                void* sourceIterator = api.OpenElements(collator, text.data(),
                    static_cast<std::int32_t>(text.size()), &errorCode);
                if (errorCode <= 0)
                {
                    result = SimpleAffixIterators(api, patternIterator, sourceIterator,
                        api.GetStrength(collator), forwardSearch);
                    api.CloseElements(sourceIterator);
                }
                api.CloseElements(patternIterator);
            }
            return result;
        }

        // pal_collation.c SimpleAffix: the current culture's collator.
        [[nodiscard]] bool SimpleAffix(std::u16string_view pattern, std::u16string_view text, bool forwardSearch)
        {
            return SimpleAffixWith(CurrentCollation().Collator, pattern, text, forwardSearch);
        }

        // The invariant culture's collator with CompareOptions.IgnoreCase:
        // the root collation at secondary strength.
        [[nodiscard]] void* InvariantIgnoreCaseCollator()
        {
            static void* const collator = []() -> void*
            {
                using SetStrengthFn = void (*)(void*, std::int32_t);
                const Collation& api = CurrentCollation();
                const auto setStrength = Icu::Function<SetStrengthFn>("ucol_setStrength");
                if (api.Open == nullptr || setStrength == nullptr)
                {
                    return nullptr;
                }
                std::int32_t status = 0;
                void* value = api.Open("", &status);
                if (value == nullptr || status > 0)
                {
                    return nullptr;
                }
                setStrength(value, UColSecondary);
                return value;
            }();
            return collator;
        }

#if defined(_WIN32)
        constexpr std::uint32_t FindStartsWith = 0x00100000U;
        constexpr std::uint32_t FindEndsWith = 0x00200000U;
        constexpr std::uint32_t NormLinguisticCasing = 0x08000000U;

        [[nodiscard]] bool UseNls()
        {
            // .NET falls back to NLS when ICU cannot be loaded.
            static const bool useNls = Icu::UseNlsRequested() || CurrentCollation().Collator == nullptr;
            return useNls;
        }

        // CompareInfo.FindString with FIND_STARTSWITH / FIND_ENDSWITH.
        [[nodiscard]] int NlsFindString(std::uint32_t flags, std::u16string_view source, std::u16string_view value)
        {
            const std::wstring locale = Utf8ToWide(CurrentCultureName());
            int sourceLength = static_cast<int>(source.size());
            const wchar_t* sourceText = reinterpret_cast<const wchar_t*>(source.data());
            const wchar_t empty[] = L"";
            if (sourceLength == 0)
            {
                sourceText = empty;
                sourceLength = -1;
            }
            return ::FindNLSStringEx(locale.c_str(), flags, sourceText, sourceLength,
                reinterpret_cast<const wchar_t*>(value.data()), static_cast<int>(value.size()),
                nullptr, nullptr, nullptr, 0);
        }
#endif

        [[nodiscard]] bool StartsWithCore(std::u16string_view source, std::u16string_view prefix)
        {
#if defined(_WIN32)
            if (UseNls())
            {
                return NlsFindString(FindStartsWith | NormLinguisticCasing, source, prefix) >= 0;
            }
#endif
            return SimpleAffix(prefix, source, true);
        }

        [[nodiscard]] bool EndsWithCore(std::u16string_view source, std::u16string_view suffix)
        {
#if defined(_WIN32)
            if (UseNls())
            {
                return NlsFindString(FindEndsWith | NormLinguisticCasing, source, suffix) >= 0;
            }
#endif
            return SimpleAffix(suffix, source, false);
        }

        // CompareInfo.StartsWithOrdinalHelper: plain ASCII is decided here,
        // anything else goes to the culture.
        [[nodiscard]] bool StartsWithOrdinalHelper(std::u16string_view source, std::u16string_view prefix)
        {
            std::size_t length = std::min(source.size(), prefix.size());
            std::size_t a = 0;
            std::size_t b = 0;
            while (length != 0)
            {
                const char16_t charA = source[a];
                const char16_t charB = prefix[b];
                if (IsHighChar(charA) || IsHighChar(charB))
                {
                    return StartsWithCore(source, prefix);
                }
                if (charA == charB)
                {
                    a++;
                    b++;
                    length--;
                    continue;
                }
                // The match may be affected by special character. Verify that the following character is regular ASCII.
                if ((a + 1 < source.size() && source[a + 1] >= 0x80U)
                    || (b + 1 < prefix.size() && prefix[b + 1] >= 0x80U))
                {
                    return StartsWithCore(source, prefix);
                }
                return false;
            }
            if (source.size() < prefix.size())
            {
                return IsHighChar(prefix[b]) ? StartsWithCore(source, prefix) : false;
            }
            if (source.size() > prefix.size() && IsHighChar(source[a]))
            {
                return StartsWithCore(source, prefix);
            }
            return true;
        }

        // CompareInfo.EndsWithOrdinalHelper.
        [[nodiscard]] bool EndsWithOrdinalHelper(std::u16string_view source, std::u16string_view suffix)
        {
            std::size_t length = std::min(source.size(), suffix.size());
            std::size_t a = source.size();
            std::size_t b = suffix.size();
            while (length != 0)
            {
                const char16_t charA = source[a - 1];
                const char16_t charB = suffix[b - 1];
                if (IsHighChar(charA) || IsHighChar(charB))
                {
                    return EndsWithCore(source, suffix);
                }
                if (charA == charB)
                {
                    a--;
                    b--;
                    length--;
                    continue;
                }
                // The match may be affected by special character. Verify that the preceding character is regular ASCII.
                if ((a - 1 > 0 && source[a - 2] >= 0x80U) || (b - 1 > 0 && suffix[b - 2] >= 0x80U))
                {
                    return EndsWithCore(source, suffix);
                }
                return false;
            }
            if (source.size() < suffix.size())
            {
                return IsHighChar(suffix[b - 1]) ? EndsWithCore(source, suffix) : false;
            }
            if (source.size() > suffix.size() && IsHighChar(source[a - 1]))
            {
                return EndsWithCore(source, suffix);
            }
            return true;
        }
    }

    bool StringStartsWithCurrentCulture(std::string_view value, std::string_view prefix)
    {
        // CompareInfo.IsPrefix with CompareOptions.None.
        const std::u16string source = Utf8ToUtf16(value);
        const std::u16string start = Utf8ToUtf16(prefix);
        if (start.empty())
        {
            return true;
        }
        if (Icu::InvariantMode())
        {
            return source.starts_with(start);
        }
        return IsAsciiEqualityOrdinal() ? StartsWithOrdinalHelper(source, start) : StartsWithCore(source, start);
    }

    bool StringEndsWithCurrentCulture(std::string_view value, std::string_view suffix)
    {
        // CompareInfo.IsSuffix with CompareOptions.None.
        const std::u16string source = Utf8ToUtf16(value);
        const std::u16string end = Utf8ToUtf16(suffix);
        if (end.empty())
        {
            return true;
        }
        if (Icu::InvariantMode())
        {
            return source.ends_with(end);
        }
        return IsAsciiEqualityOrdinal() ? EndsWithOrdinalHelper(source, end) : EndsWithCore(source, end);
    }

    bool StringStartsWithInvariantCultureIgnoreCase(std::string_view value, std::string_view prefix)
    {
        // CompareInfo.Invariant.IsPrefix(value, prefix, IgnoreCase). ASCII
        // letters and digits are decided ordinally, as .NET's fast path does;
        // anything else -- the control characters and NULs the collation
        // ignores among them -- goes to ICU.
        const std::u16string source = Utf8ToUtf16(value);
        const std::u16string start = Utf8ToUtf16(prefix);
        const auto plain = [](std::u16string_view text)
        {
            return std::none_of(text.begin(), text.end(), [](char16_t ch) { return IsHighChar(ch); });
        };
        if (Icu::InvariantMode() || (plain(source) && plain(start)))
        {
            return StringStartsWithOrdinalIgnoreCase(value, prefix);
        }
        if (void* collator = InvariantIgnoreCaseCollator(); collator != nullptr)
        {
            return SimpleAffixWith(collator, start, source, true);
        }
        // No ICU: what the collation would ignore, dropped, then ordinally.
        const auto significant = [](std::string_view text)
        {
            std::string kept;
            for (const char ch : text)
            {
                const auto unit = static_cast<unsigned char>(ch);
                if (unit >= 0x20U || unit == '\t' || unit == '\v' || unit == '\f')
                {
                    if (unit != 0x7FU)
                    {
                        kept.push_back(ch);
                    }
                }
            }
            return kept;
        };
        return StringStartsWithOrdinalIgnoreCase(significant(value), significant(prefix));
    }

    std::int32_t StringCompareCurrentCulture(std::string_view left, std::string_view right)
    {
        // CompareInfo.Compare with CompareOptions.None.
        if (left == right)
        {
            return 0;
        }
        const std::u16string a = Utf8ToUtf16(left);
        const std::u16string b = Utf8ToUtf16(right);
        if (!Icu::InvariantMode())
        {
#if defined(_WIN32)
            if (UseNls())
            {
                const std::wstring locale = Utf8ToWide(CurrentCultureName());
                const int result = ::CompareStringEx(locale.c_str(), 0,
                    reinterpret_cast<const wchar_t*>(a.data()), static_cast<int>(a.size()),
                    reinterpret_cast<const wchar_t*>(b.data()), static_cast<int>(b.size()),
                    nullptr, nullptr, 0);
                if (result != 0)
                {
                    return result - CSTR_EQUAL;
                }
            }
#endif
            const Collation& api = CurrentCollation();
            if (api.Collator != nullptr && api.Strcoll != nullptr)
            {
                return api.Strcoll(api.Collator, a.data(), static_cast<std::int32_t>(a.size()),
                    b.data(), static_cast<std::int32_t>(b.size()));
            }
        }
        // Invariant mode compares ordinally.
        const int compared = a.compare(b);
        return compared < 0 ? -1 : (compared > 0 ? 1 : 0);
    }

    // ==== bool =================================================================

    bool BooleanTryParse(std::string_view value, bool& result) noexcept
    {
        // Boolean.TrimWhiteSpaceAndNull, then the two names, ordinal and
        // case-insensitive.
        const auto trimmed = [](char32_t c) { return c == U'\0' || CharIsWhiteSpace(c); };
        while (!value.empty())
        {
            const Utf8Scalar first = DecodeUtf8Scalar(value, 0);
            if (!trimmed(first.Value))
            {
                break;
            }
            value.remove_prefix(first.Length);
        }
        while (!value.empty())
        {
            const Utf8Scalar last = DecodeLastUtf8Scalar(value, value.size());
            if (!trimmed(last.Value))
            {
                break;
            }
            value.remove_suffix(last.Length);
        }
        if (StringEqualsOrdinalIgnoreCase(value, "True"))
        {
            result = true;
            return true;
        }
        result = false;
        return StringEqualsOrdinalIgnoreCase(value, "False");
    }

    bool BooleanTryParse(const std::optional<std::string_view>& value, bool& result) noexcept
    {
        if (!value.has_value())
        {
            result = false;
            return false;
        }
        return BooleanTryParse(*value, result);
    }
}
