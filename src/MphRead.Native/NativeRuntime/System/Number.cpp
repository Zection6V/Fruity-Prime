#include "Number.hpp"

#include "Charconv.hpp"
#include "Encoding.hpp"
#include "Exceptions.hpp"
#include "Globalization.hpp"
#include "Icu.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        // ==== The current culture's NumberFormatInfo ===========================

        [[nodiscard]] bool AllowsHyphen(std::string_view negativeSign) noexcept
        {
            // NumberFormatInfo.AllowHyphenDuringParsing: a minus sign that is
            // one of these dashes also accepts U+002D.
            if (negativeSign.empty())
            {
                return false;
            }
            const Utf8Scalar scalar = DecodeUtf8Scalar(negativeSign, 0);
            if (scalar.Length != negativeSign.size())
            {
                return false;
            }
            switch (scalar.Value)
            {
            case U'‒':
            case U'⁻':
            case U'₋':
            case U'−':
            case U'➖':
            case U'﹣':
            case U'－':
                return true;
            default:
                return false;
            }
        }

        // pal_localeNumberData.c NormalizeNumericPattern: an ICU pattern
        // reduced to the shape .NET's pattern tables are written in -- the
        // digits as one 'n', one space, and the signs and symbols as they are.
        [[nodiscard]] std::string NormalizePattern(std::u16string_view pattern, bool negative)
        {
            std::u16string_view part = pattern;
            const std::size_t semicolon = pattern.find(u';');
            if (semicolon != std::u16string_view::npos)
            {
                part = negative ? pattern.substr(semicolon + 1) : pattern.substr(0, semicolon);
            }
            bool hasMinus = false;
            if (negative)
            {
                hasMinus = part.find_first_of(u"-()") != std::u16string_view::npos;
            }
            std::string normalized;
            if (negative && !hasMinus)
            {
                normalized.push_back('-');
            }
            bool digit = false;
            bool space = false;
            for (const char16_t ch : part)
            {
                switch (ch)
                {
                case u'#':
                case u'0':
                case u'@':
                case u',':
                case u'.':
                    if (!digit)
                    {
                        digit = true;
                        normalized.push_back('n');
                    }
                    break;
                case u' ':
                case u' ':
                case u' ':
                    if (!space)
                    {
                        space = true;
                        normalized.push_back(' ');
                    }
                    break;
                case u'-':
                case u'(':
                case u')':
                case u'%':
                    normalized.push_back(static_cast<char>(ch));
                    break;
                default:
                    break;
                }
            }
            return normalized;
        }

        template <std::size_t N>
        [[nodiscard]] std::optional<std::int32_t> PatternIndex(
            const std::array<std::string_view, N>& table, std::string_view normalized) noexcept
        {
            for (std::size_t i = 0; i < N; ++i)
            {
                if (table[i] == normalized)
                {
                    return static_cast<std::int32_t>(i);
                }
            }
            return std::nullopt;
        }

        constexpr std::array<std::string_view, 5> NumberNegativePatterns{
            "(n)", "-n", "- n", "n-", "n -" };
        constexpr std::array<std::string_view, 4> PercentPositivePatterns{
            "n %", "n%", "%n", "% n" };
        constexpr std::array<std::string_view, 12> PercentNegativePatterns{
            "-n %", "-n%", "-%n", "%-n", "%n-", "n-%", "n%-", "-% n", "n %-", "% n-", "% -n", "n- %" };

        // The grouping of "#,##,##0.###": the primary size is the last group,
        // a different one before it is the secondary.
        [[nodiscard]] std::vector<std::int32_t> GroupSizes(std::u16string_view pattern)
        {
            const std::size_t end = std::min(pattern.find(u';'), pattern.find(u'.'));
            const std::u16string_view integral = pattern.substr(0, std::min(end, pattern.size()));
            std::vector<std::int32_t> groups;
            std::int32_t run = 0;
            bool any = false;
            for (const char16_t ch : integral)
            {
                if (ch == u',')
                {
                    if (any)
                    {
                        groups.push_back(run);
                    }
                    run = 0;
                    any = true;
                }
                else if (ch == u'#' || ch == u'0' || ch == u'@')
                {
                    ++run;
                }
            }
            if (!any)
            {
                return { 0 };
            }
            const std::int32_t primary = run;
            if (!groups.empty() && groups.back() != primary)
            {
                return { primary, groups.back() };
            }
            return { primary };
        }

        struct IcuNumber final
        {
            using Open = void* (*)(std::int32_t, const char16_t*, std::int32_t,
                const char*, void*, std::int32_t*);
            using Close = void (*)(void*);
            using GetSymbol = std::int32_t (*)(const void*, std::int32_t, char16_t*,
                std::int32_t, std::int32_t*);
            using ToPattern = std::int32_t (*)(const void*, std::int8_t, char16_t*,
                std::int32_t, std::int32_t*);

            Open OpenFormat = Icu::Function<Open>("unum_open");
            Close CloseFormat = Icu::Function<Close>("unum_close");
            GetSymbol Symbol = Icu::Function<GetSymbol>("unum_getSymbol");
            ToPattern Pattern = Icu::Function<ToPattern>("unum_toPattern");

            [[nodiscard]] bool Available() const noexcept
            {
                return OpenFormat != nullptr && CloseFormat != nullptr && Symbol != nullptr;
            }
        };

        // UNumberFormatSymbol and UNumberFormatStyle.
        constexpr std::int32_t UnumDecimalSeparator = 0;
        constexpr std::int32_t UnumGroupingSeparator = 1;
        constexpr std::int32_t UnumPercent = 3;
        constexpr std::int32_t UnumMinusSign = 6;
        constexpr std::int32_t UnumPlusSign = 7;
        constexpr std::int32_t UnumPermill = 12;
        constexpr std::int32_t UnumInfinity = 14;
        constexpr std::int32_t UnumNaN = 15;
        constexpr std::int32_t UnumStyleDecimal = 1;
        constexpr std::int32_t UnumStylePercent = 3;

        // A UTF-16 result from an ICU "preflight" API: the length asked for
        // first, then the text.
        template <class TCall>
        [[nodiscard]] std::u16string IcuText(TCall call)
        {
            std::array<char16_t, 64> stack{};
            std::int32_t status = 0;
            const std::int32_t length = call(stack.data(), static_cast<std::int32_t>(stack.size()), &status);
            if (length < 0)
            {
                return {};
            }
            if (length < static_cast<std::int32_t>(stack.size()) && status <= 0)
            {
                return std::u16string(stack.data(), static_cast<std::size_t>(length));
            }
            std::u16string buffer(static_cast<std::size_t>(length) + 1, u'\0');
            status = 0;
            const std::int32_t written = call(buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
            if (written < 0 || status > 0)
            {
                return {};
            }
            buffer.resize(static_cast<std::size_t>(written));
            return buffer;
        }

        // IcuGetNFIValues for `locale` (an ICU locale id).
        [[nodiscard]] bool LoadFromIcu(const char* locale, NumberFormatInfo& info)
        {
            static const IcuNumber api;
            if (!api.Available())
            {
                return false;
            }
            std::int32_t status = 0;
            void* decimal = api.OpenFormat(UnumStyleDecimal, nullptr, 0, locale, nullptr, &status);
            if (decimal == nullptr || status > 0)
            {
                if (decimal != nullptr)
                {
                    api.CloseFormat(decimal);
                }
                return false;
            }
            const auto symbol = [&](void* format, std::int32_t which)
            {
                return Utf16ToUtf8(IcuText([&](char16_t* buffer, std::int32_t size, std::int32_t* error)
                {
                    return api.Symbol(format, which, buffer, size, error);
                }));
            };
            const auto pattern = [&](void* format)
            {
                if (api.Pattern == nullptr)
                {
                    return std::u16string();
                }
                return IcuText([&](char16_t* buffer, std::int32_t size, std::int32_t* error)
                {
                    return api.Pattern(format, 0, buffer, size, error);
                });
            };

            if (std::string value = symbol(decimal, UnumDecimalSeparator); !value.empty())
            {
                info.NumberDecimalSeparator = std::move(value);
            }
            info.NumberGroupSeparator = symbol(decimal, UnumGroupingSeparator);
            if (std::string value = symbol(decimal, UnumPlusSign); !value.empty())
            {
                info.PositiveSign = std::move(value);
            }
            if (std::string value = symbol(decimal, UnumMinusSign); !value.empty())
            {
                info.NegativeSign = std::move(value);
            }
            // ICU's POSIX locale spells infinity "INF"; .NET keeps its own.
            if (std::string value = symbol(decimal, UnumInfinity);
                !value.empty() && !StringEqualsOrdinalIgnoreCase(value, "inf"))
            {
                info.PositiveInfinitySymbol = std::move(value);
            }
            if (std::string value = symbol(decimal, UnumNaN); !value.empty())
            {
                info.NaNSymbol = std::move(value);
            }
            if (std::string value = symbol(decimal, UnumPercent); !value.empty())
            {
                info.PercentSymbol = std::move(value);
            }
            if (std::string value = symbol(decimal, UnumPermill); !value.empty())
            {
                info.PerMilleSymbol = std::move(value);
            }
            const std::u16string decimalPattern = pattern(decimal);
            api.CloseFormat(decimal);
            if (!decimalPattern.empty())
            {
                info.NumberGroupSizes = GroupSizes(decimalPattern);
                info.NumberNegativePattern = PatternIndex(NumberNegativePatterns,
                    NormalizePattern(decimalPattern, true)).value_or(1);
            }

            status = 0;
            if (void* percent = api.OpenFormat(UnumStylePercent, nullptr, 0, locale, nullptr, &status);
                percent != nullptr)
            {
                const std::u16string percentPattern = status <= 0 ? pattern(percent) : std::u16string();
                api.CloseFormat(percent);
                if (!percentPattern.empty())
                {
                    info.PercentPositivePattern = PatternIndex(PercentPositivePatterns,
                        NormalizePattern(percentPattern, false)).value_or(0);
                    info.PercentNegativePattern = PatternIndex(PercentNegativePatterns,
                        NormalizePattern(percentPattern, true)).value_or(0);
                }
            }
            // CultureData.Icu: negative infinity is the minus sign and infinity.
            info.NegativeInfinitySymbol = info.NegativeSign + info.PositiveInfinitySymbol;
            return true;
        }

#if defined(_WIN32)
        [[nodiscard]] std::optional<std::string> LocaleString(const wchar_t* locale, LCTYPE type)
        {
            const int length = GetLocaleInfoEx(locale, type, nullptr, 0);
            if (length <= 0)
            {
                return std::nullopt;
            }
            std::wstring value(static_cast<std::size_t>(length), L'\0');
            if (GetLocaleInfoEx(locale, type, value.data(), length) == 0)
            {
                return std::nullopt;
            }
            value.resize(static_cast<std::size_t>(length - 1));
            return WideToUtf8(value);
        }

        [[nodiscard]] std::optional<std::int32_t> LocaleInt(const wchar_t* locale, LCTYPE type)
        {
            const std::optional<std::string> text = LocaleString(locale, type);
            std::int32_t value = 0;
            if (!text.has_value()
                || std::from_chars(text->data(), text->data() + text->size(), value).ec != std::errc{})
            {
                return std::nullopt;
            }
            return value;
        }

        // CultureData.ConvertWin32GroupString: "3;0" is {3}, "3;2;0" is
        // {3, 2}, and a list without the closing 0 stops grouping after it.
        [[nodiscard]] std::vector<std::int32_t> Win32GroupSizes(std::string_view text)
        {
            if (text.empty() || text.front() == '0')
            {
                return { 0 };
            }
            std::vector<std::int32_t> sizes;
            std::size_t start = 0;
            while (start <= text.size())
            {
                const std::size_t end = std::min(text.find(';', start), text.size());
                std::int32_t size = 0;
                std::from_chars(text.data() + start, text.data() + end, size);
                sizes.push_back(size);
                start = end + 1;
            }
            if (sizes.size() > 1 && sizes.back() == 0)
            {
                sizes.pop_back();
            }
            else
            {
                sizes.push_back(0);
            }
            return sizes;
        }

#ifndef LOCALE_INEGATIVEPERCENT
#define LOCALE_INEGATIVEPERCENT 0x00000074
#endif
#ifndef LOCALE_IPOSITIVEPERCENT
#define LOCALE_IPOSITIVEPERCENT 0x00000075
#endif
#ifndef LOCALE_SPERCENT
#define LOCALE_SPERCENT 0x00000076
#endif
#ifndef LOCALE_SPERMILLE
#define LOCALE_SPERMILLE 0x00000077
#endif

        // The values CurrentCulture takes from the user's own settings in
        // the Region control panel, whichever library supplied the rest.
        void ApplyUserOverrides(const wchar_t* locale, NumberFormatInfo& info)
        {
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SDECIMAL); value && !value->empty())
            {
                info.NumberDecimalSeparator = std::move(*value);
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_STHOUSAND))
            {
                info.NumberGroupSeparator = std::move(*value);
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SPOSITIVESIGN))
            {
                info.PositiveSign = value->empty() ? "+" : std::move(*value);
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SNEGATIVESIGN))
            {
                info.NegativeSign = std::move(*value);
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SGROUPING))
            {
                info.NumberGroupSizes = Win32GroupSizes(*value);
            }
            if (std::optional<std::int32_t> value = LocaleInt(locale, LOCALE_INEGNUMBER);
                value && *value >= 0 && *value <= 4)
            {
                info.NumberNegativePattern = *value;
            }
            if (std::optional<std::int32_t> value = LocaleInt(locale, LOCALE_IDIGITS); value && *value >= 0)
            {
                info.NumberDecimalDigits = *value;
            }
        }

        void LoadFromNls(const wchar_t* locale, NumberFormatInfo& info)
        {
            ApplyUserOverrides(locale, info);
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SNAN); value && !value->empty())
            {
                info.NaNSymbol = std::move(*value);
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SPOSINFINITY); value && !value->empty())
            {
                info.PositiveInfinitySymbol = std::move(*value);
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SNEGINFINITY); value && !value->empty())
            {
                info.NegativeInfinitySymbol = std::move(*value);
            }
            else
            {
                info.NegativeInfinitySymbol = info.NegativeSign + info.PositiveInfinitySymbol;
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SPERCENT); value && !value->empty())
            {
                info.PercentSymbol = std::move(*value);
            }
            if (std::optional<std::string> value = LocaleString(locale, LOCALE_SPERMILLE); value && !value->empty())
            {
                info.PerMilleSymbol = std::move(*value);
            }
            if (std::optional<std::int32_t> value = LocaleInt(locale, LOCALE_IPOSITIVEPERCENT);
                value && *value >= 0 && *value <= 3)
            {
                info.PercentPositivePattern = *value;
            }
            if (std::optional<std::int32_t> value = LocaleInt(locale, LOCALE_INEGATIVEPERCENT);
                value && *value >= 0 && *value <= 11)
            {
                info.PercentNegativePattern = *value;
            }
        }
#else
        // No ICU: the C library's idea of the user's locale, for the two
        // symbols it knows. .NET itself refuses to start without ICU here.
        void LoadFromCLocale(NumberFormatInfo& info)
        {
            try
            {
                const std::locale locale("");
                const auto& punctuation = std::use_facet<std::numpunct<wchar_t>>(locale);
                info.NumberDecimalSeparator = WideToUtf8(std::wstring(1, punctuation.decimal_point()));
                info.NumberGroupSeparator = WideToUtf8(std::wstring(1, punctuation.thousands_sep()));
            }
            catch (const std::exception&)
            {
            }
        }
#endif

        [[nodiscard]] NumberFormatInfo LoadCurrent()
        {
            NumberFormatInfo info;
            if (Icu::InvariantMode())
            {
                return info;
            }
#if defined(_WIN32)
            wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
            if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) == 0)
            {
                return info;
            }
            if (!Icu::UseNlsRequested() && LoadFromIcu(WideToUtf8(locale).c_str(), info))
            {
                ApplyUserOverrides(locale, info);
            }
            else
            {
                LoadFromNls(locale, info);
            }
#else
            using DefaultLocale = const char* (*)();
            static const DefaultLocale defaultLocale = Icu::Function<DefaultLocale>("uloc_getDefault");
            const char* name = defaultLocale != nullptr ? defaultLocale() : nullptr;
            if (name != nullptr && std::string_view(name) == "en_US_POSIX")
            {
                // The POSIX locale is the invariant culture.
                return info;
            }
            if (!LoadFromIcu(name, info))
            {
                LoadFromCLocale(info);
            }
#endif
            info.AllowHyphenDuringParsing = AllowsHyphen(info.NegativeSign);
            return info;
        }

        // ==== The digit buffer ==================================================

        // Number.NumberBuffer: the value is 0.Digits x 10^Scale, Digits has
        // no trailing zeros, and an empty Digits is zero.
        struct NumberBuffer final
        {
            std::string Digits;
            std::int32_t Scale = 0;
            bool Negative = false;
            bool FloatingPoint = false;
        };

        // Reads to_chars output ("-1.2345e+03", "0.0450", "12") into a buffer.
        void ReadDigits(std::string_view text, NumberBuffer& number)
        {
            number.Digits.clear();
            number.Scale = 0;
            std::size_t index = 0;
            if (index < text.size() && text[index] == '-')
            {
                ++index;
            }
            std::int32_t integralDigits = 0;
            bool point = false;
            for (; index < text.size(); ++index)
            {
                const char ch = text[index];
                if (ch == '.')
                {
                    point = true;
                    continue;
                }
                if (ch < '0' || ch > '9')
                {
                    break;
                }
                number.Digits.push_back(ch);
                if (!point)
                {
                    ++integralDigits;
                }
            }
            std::int32_t exponent = 0;
            if (index < text.size() && (text[index] == 'e' || text[index] == 'E'))
            {
                ++index;
                if (index < text.size() && text[index] == '+')
                {
                    ++index;
                }
                std::from_chars(text.data() + index, text.data() + text.size(), exponent);
            }
            const std::size_t first = number.Digits.find_first_not_of('0');
            if (first == std::string::npos)
            {
                number.Digits.clear();
                return;
            }
            number.Digits.erase(0, first);
            number.Digits.erase(number.Digits.find_last_not_of('0') + 1);
            number.Scale = integralDigits - static_cast<std::int32_t>(first) + exponent;
        }

        enum class Cutoff
        {
            Shortest,
            Significant,
            Fractional,
        };

        // Dragon4Double / Dragon4Single: the exact digits of `value`, cut at
        // `precision` significant or fractional digits and correctly rounded,
        // or the shortest that read back.
        template <class TFloat>
        [[nodiscard]] NumberBuffer FloatDigits(TFloat value, Cutoff cutoff, std::int32_t precision)
        {
            NumberBuffer number;
            number.FloatingPoint = true;
            number.Negative = std::signbit(value);
            if (value == 0)
            {
                return number;
            }
            // Past these the exact expansion of any float or double is all
            // zeros, which the formatter pads in anyway.
            precision = std::min(precision, 1100);
            std::string buffer(static_cast<std::size_t>(precision) + 400, '\0');
            std::to_chars_result result{};
            switch (cutoff)
            {
            case Cutoff::Shortest:
                result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                    std::chars_format::scientific);
                break;
            case Cutoff::Significant:
                result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                    std::chars_format::scientific, std::max(precision, 1) - 1);
                break;
            case Cutoff::Fractional:
                result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                    std::chars_format::fixed, precision);
                break;
            }
            if (result.ec != std::errc{})
            {
                throw System::FormatException();
            }
            ReadDigits(std::string_view(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())), number);
            return number;
        }

        [[nodiscard]] NumberBuffer IntegerDigits(std::uint64_t magnitude, bool negative)
        {
            NumberBuffer number;
            number.Negative = negative;
            std::array<char, 24> buffer{};
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude);
            ReadDigits(std::string_view(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())), number);
            return number;
        }

        // Number.RoundNumber: keep `position` digits, rounding half up on the
        // digits already there unless they are already correctly rounded.
        void RoundNumber(NumberBuffer& number, std::int32_t position, bool correctlyRounded)
        {
            std::string& digits = number.Digits;
            std::int32_t i = 0;
            while (i < position && i < static_cast<std::int32_t>(digits.size()))
            {
                ++i;
            }
            if (i == position && i < static_cast<std::int32_t>(digits.size())
                && !correctlyRounded && digits[static_cast<std::size_t>(i)] >= '5')
            {
                while (i > 0 && digits[static_cast<std::size_t>(i - 1)] == '9')
                {
                    --i;
                }
                if (i > 0)
                {
                    ++digits[static_cast<std::size_t>(i - 1)];
                }
                else
                {
                    ++number.Scale;
                    digits[0] = '1';
                    i = 1;
                }
            }
            else
            {
                while (i > 0 && digits[static_cast<std::size_t>(i - 1)] == '0')
                {
                    --i;
                }
            }
            if (i == 0)
            {
                // A floating-point zero keeps its sign: (-0.0).ToString() is "-0".
                if (!number.FloatingPoint)
                {
                    number.Negative = false;
                }
                number.Scale = 0;
            }
            digits.resize(static_cast<std::size_t>(i));
        }

        // ==== Standard formats ==================================================

        struct Specifier final
        {
            char Format = '\0';
            // -1 when the format gave none.
            std::int32_t Precision = -1;
        };

        // Number.ParseFormatSpecifier: a letter and up to nine digits is a
        // standard format; anything else is a custom one ('\0').
        [[nodiscard]] Specifier ParseSpecifier(std::string_view format)
        {
            if (format.empty())
            {
                return { 'G', -1 };
            }
            const char first = format.front();
            if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')))
            {
                return {};
            }
            if (format.size() == 1)
            {
                return { first, -1 };
            }
            if (format.size() <= 10 && std::all_of(format.begin() + 1, format.end(),
                [](char ch) { return ch >= '0' && ch <= '9'; }))
            {
                std::int32_t precision = 0;
                std::from_chars(format.data() + 1, format.data() + format.size(), precision);
                return { first, precision };
            }
            return {};
        }

        void AppendDigit(std::string& output, const std::string& digits, std::size_t& index)
        {
            output.push_back(index < digits.size() ? digits[index++] : '0');
        }

        void FormatExponent(std::string& output, const NumberFormatInfo& info, std::int32_t value,
            char exponentChar, std::int32_t minDigits, bool positiveSign)
        {
            output.push_back(exponentChar);
            if (value < 0)
            {
                output += info.NegativeSign;
                value = -value;
            }
            else if (positiveSign)
            {
                output += info.PositiveSign;
            }
            const std::string digits = std::to_string(value);
            if (static_cast<std::int32_t>(digits.size()) < minDigits)
            {
                output.append(static_cast<std::size_t>(minDigits) - digits.size(), '0');
            }
            output += digits;
        }

        // Number.FormatFixed: the integral digits, grouped when `groups` is
        // given, and exactly `decimals` more after the separator.
        void FormatFixed(std::string& output, const NumberBuffer& number, std::int32_t decimals,
            const std::vector<std::int32_t>* groups, const NumberFormatInfo& info)
        {
            std::int32_t position = number.Scale;
            std::size_t index = 0;
            if (position > 0)
            {
                std::string integral;
                for (std::int32_t i = 0; i < position; ++i)
                {
                    AppendDigit(integral, number.Digits, index);
                }
                if (groups != nullptr && !groups->empty() && !info.NumberGroupSeparator.empty())
                {
                    // Separator positions counted from the right.
                    std::vector<std::size_t> cuts;
                    std::size_t total = 0;
                    std::size_t group = 0;
                    std::int32_t size = (*groups)[0];
                    while (size > 0 && total + static_cast<std::size_t>(size) < integral.size())
                    {
                        total += static_cast<std::size_t>(size);
                        cuts.push_back(integral.size() - total);
                        if (group + 1 < groups->size())
                        {
                            ++group;
                            size = (*groups)[group];
                        }
                    }
                    std::size_t start = 0;
                    for (auto it = cuts.rbegin(); it != cuts.rend(); ++it)
                    {
                        output.append(integral, start, *it - start);
                        output += info.NumberGroupSeparator;
                        start = *it;
                    }
                    output.append(integral, start, std::string::npos);
                }
                else
                {
                    output += integral;
                }
            }
            else
            {
                output.push_back('0');
            }
            if (decimals > 0)
            {
                output += info.NumberDecimalSeparator;
                if (position < 0)
                {
                    const std::int32_t zeros = std::min(-position, decimals);
                    output.append(static_cast<std::size_t>(zeros), '0');
                    position += zeros;
                    decimals -= zeros;
                }
                for (; decimals > 0; --decimals)
                {
                    AppendDigit(output, number.Digits, index);
                }
            }
        }

        void FormatScientific(std::string& output, const NumberBuffer& number, std::int32_t digits,
            const NumberFormatInfo& info, char exponentChar)
        {
            std::size_t index = 0;
            AppendDigit(output, number.Digits, index);
            if (digits != 1)
            {
                output += info.NumberDecimalSeparator;
            }
            while (--digits > 0)
            {
                AppendDigit(output, number.Digits, index);
            }
            const std::int32_t exponent = number.Digits.empty() ? 0 : number.Scale - 1;
            FormatExponent(output, info, exponent, exponentChar, 3, true);
        }

        void FormatGeneral(std::string& output, const NumberBuffer& number, std::int32_t maxDigits,
            const NumberFormatInfo& info, char exponentChar, bool suppressScientific = false)
        {
            std::int32_t position = number.Scale;
            bool scientific = false;
            if (!suppressScientific && (position > maxDigits || position < -3))
            {
                position = 1;
                scientific = true;
            }
            std::size_t index = 0;
            if (position > 0)
            {
                do
                {
                    AppendDigit(output, number.Digits, index);
                }
                while (--position > 0);
            }
            else
            {
                output.push_back('0');
            }
            if (index < number.Digits.size() || position < 0)
            {
                output += info.NumberDecimalSeparator;
                for (; position < 0; ++position)
                {
                    output.push_back('0');
                }
                output.append(number.Digits, index, std::string::npos);
            }
            if (scientific)
            {
                FormatExponent(output, info, number.Scale - 1, exponentChar, 2, true);
            }
        }

        // Replaces the 'n' of one of .NET's pattern strings with `body`.
        void ApplyPattern(std::string& output, std::string_view pattern, const std::string& body,
            const NumberFormatInfo& info)
        {
            for (const char ch : pattern)
            {
                switch (ch)
                {
                case 'n':
                    output += body;
                    break;
                case '-':
                    output += info.NegativeSign;
                    break;
                case '%':
                    output += info.PercentSymbol;
                    break;
                default:
                    output.push_back(ch);
                    break;
                }
            }
        }

        // Number.NumberToString for a standard format.
        [[nodiscard]] std::string StandardFormat(NumberBuffer& number, char format, std::int32_t maxDigits,
            const NumberFormatInfo& info, bool correctlyRounded)
        {
            std::string output;
            switch (format)
            {
            case 'F':
            case 'f':
                if (maxDigits < 0)
                {
                    maxDigits = info.NumberDecimalDigits;
                }
                RoundNumber(number, number.Scale + maxDigits, correctlyRounded);
                if (number.Negative)
                {
                    output += info.NegativeSign;
                }
                FormatFixed(output, number, maxDigits, nullptr, info);
                return output;
            case 'N':
            case 'n':
            {
                if (maxDigits < 0)
                {
                    maxDigits = info.NumberDecimalDigits;
                }
                RoundNumber(number, number.Scale + maxDigits, correctlyRounded);
                std::string body;
                FormatFixed(body, number, maxDigits, &info.NumberGroupSizes, info);
                if (number.Negative)
                {
                    ApplyPattern(output, NumberNegativePatterns[static_cast<std::size_t>(
                        std::clamp(info.NumberNegativePattern, 0, 4))], body, info);
                }
                else
                {
                    output = std::move(body);
                }
                return output;
            }
            case 'E':
            case 'e':
                if (maxDigits < 0)
                {
                    maxDigits = 6;
                }
                ++maxDigits;
                RoundNumber(number, maxDigits, correctlyRounded);
                if (number.Negative)
                {
                    output += info.NegativeSign;
                }
                FormatScientific(output, number, maxDigits, info, format);
                return output;
            case 'G':
            case 'g':
                if (maxDigits < 1)
                {
                    maxDigits = static_cast<std::int32_t>(number.Digits.size());
                }
                RoundNumber(number, maxDigits, correctlyRounded);
                if (number.Negative)
                {
                    output += info.NegativeSign;
                }
                FormatGeneral(output, number, maxDigits, info, format == 'G' ? 'E' : 'e');
                return output;
            case 'P':
            case 'p':
            {
                if (maxDigits < 0)
                {
                    maxDigits = info.PercentDecimalDigits;
                }
                number.Scale += 2;
                RoundNumber(number, number.Scale + maxDigits, correctlyRounded);
                std::string body;
                FormatFixed(body, number, maxDigits, &info.NumberGroupSizes, info);
                if (number.Negative)
                {
                    ApplyPattern(output, PercentNegativePatterns[static_cast<std::size_t>(
                        std::clamp(info.PercentNegativePattern, 0, 11))], body, info);
                }
                else
                {
                    ApplyPattern(output, PercentPositivePatterns[static_cast<std::size_t>(
                        std::clamp(info.PercentPositivePattern, 0, 3))], body, info);
                }
                return output;
            }
            default:
                throw System::FormatException("Format specifier was invalid.");
            }
        }

        // ==== Custom formats =====================================================

        // Number.FindSection: where section `section` of "a;b;c" starts, or 0
        // when it does not exist or is empty.
        [[nodiscard]] std::size_t FindSection(std::string_view format, std::int32_t section)
        {
            if (section == 0)
            {
                return 0;
            }
            std::size_t src = 0;
            while (true)
            {
                if (src >= format.size())
                {
                    return 0;
                }
                const char ch = format[src++];
                switch (ch)
                {
                case '\'':
                case '"':
                    while (src < format.size() && format[src++] != ch)
                    {
                    }
                    break;
                case '\\':
                    if (src < format.size())
                    {
                        ++src;
                    }
                    break;
                case ';':
                    if (--section != 0)
                    {
                        break;
                    }
                    if (src < format.size() && format[src] != ';')
                    {
                        return src;
                    }
                    return 0;
                default:
                    break;
                }
            }
        }

        [[nodiscard]] bool StartsWithAt(std::string_view format, std::size_t at, std::string_view token) noexcept
        {
            return format.substr(at, token.size()) == token;
        }

        // Number.NumberToStringFormat.
        [[nodiscard]] std::string CustomFormat(NumberBuffer& number, std::string_view format,
            const NumberFormatInfo& info)
        {
            constexpr std::string_view PerMille = "\xE2\x80\xB0";
            std::int32_t digitCount = 0;
            std::int32_t decimalPos = -1;
            std::int32_t firstDigit = 0x7FFFFFFF;
            std::int32_t lastDigit = 0;
            bool scientific = false;
            bool thousandSeps = false;
            std::size_t section = FindSection(format,
                number.Digits.empty() ? 2 : (number.Negative ? 1 : 0));

            while (true)
            {
                digitCount = 0;
                decimalPos = -1;
                firstDigit = 0x7FFFFFFF;
                lastDigit = 0;
                scientific = false;
                thousandSeps = false;
                std::int32_t thousandPos = -1;
                std::int32_t thousandCount = 0;
                std::int32_t scaleAdjust = 0;
                std::size_t src = section;
                while (src < format.size() && format[src] != ';')
                {
                    if (StartsWithAt(format, src, PerMille))
                    {
                        scaleAdjust += 3;
                        src += PerMille.size();
                        continue;
                    }
                    const char ch = format[src++];
                    switch (ch)
                    {
                    case '#':
                        ++digitCount;
                        break;
                    case '0':
                        if (firstDigit == 0x7FFFFFFF)
                        {
                            firstDigit = digitCount;
                        }
                        ++digitCount;
                        lastDigit = digitCount;
                        break;
                    case '.':
                        if (decimalPos < 0)
                        {
                            decimalPos = digitCount;
                        }
                        break;
                    case ',':
                        if (digitCount > 0 && decimalPos < 0)
                        {
                            if (thousandPos >= 0)
                            {
                                if (thousandPos == digitCount)
                                {
                                    ++thousandCount;
                                    break;
                                }
                                thousandSeps = true;
                            }
                            thousandPos = digitCount;
                            thousandCount = 1;
                        }
                        break;
                    case '%':
                        scaleAdjust += 2;
                        break;
                    case '\'':
                    case '"':
                        while (src < format.size() && format[src++] != ch)
                        {
                        }
                        break;
                    case '\\':
                        if (src < format.size())
                        {
                            ++src;
                        }
                        break;
                    case 'E':
                    case 'e':
                        if ((src < format.size() && format[src] == '0')
                            || (src + 1 < format.size() && (format[src] == '+' || format[src] == '-')
                                && format[src + 1] == '0'))
                        {
                            while (++src < format.size() && format[src] == '0')
                            {
                            }
                            scientific = true;
                        }
                        break;
                    default:
                        break;
                    }
                }
                if (decimalPos < 0)
                {
                    decimalPos = digitCount;
                }
                if (thousandPos >= 0)
                {
                    if (thousandPos == decimalPos)
                    {
                        scaleAdjust -= thousandCount * 3;
                    }
                    else
                    {
                        thousandSeps = true;
                    }
                }
                if (!number.Digits.empty())
                {
                    number.Scale += scaleAdjust;
                    const std::int32_t position = scientific
                        ? digitCount
                        : number.Scale + digitCount - decimalPos;
                    RoundNumber(number, position, false);
                    if (number.Digits.empty())
                    {
                        const std::size_t zeroSection = FindSection(format, 2);
                        if (zeroSection != section)
                        {
                            section = zeroSection;
                            continue;
                        }
                    }
                }
                else
                {
                    if (!number.FloatingPoint)
                    {
                        number.Negative = false;
                    }
                    number.Scale = 0;
                }
                break;
            }

            firstDigit = firstDigit < decimalPos ? decimalPos - firstDigit : 0;
            lastDigit = lastDigit > decimalPos ? decimalPos - lastDigit : 0;
            std::int32_t digPos = 0;
            std::int32_t adjust = 0;
            if (scientific)
            {
                digPos = decimalPos;
                adjust = 0;
            }
            else
            {
                digPos = std::max(number.Scale, decimalPos);
                adjust = number.Scale - decimalPos;
            }

            // Where group separators go, counted in digits from the point.
            std::vector<std::int32_t> separators;
            if (thousandSeps && !info.NumberGroupSeparator.empty() && !info.NumberGroupSizes.empty())
            {
                std::size_t groupIndex = 0;
                std::int32_t total = info.NumberGroupSizes[0];
                std::int32_t size = total;
                const std::int32_t totalDigits = digPos + (adjust < 0 ? adjust : 0);
                const std::int32_t digits = std::max(firstDigit, totalDigits);
                while (digits > total && size != 0)
                {
                    separators.push_back(total);
                    if (groupIndex + 1 < info.NumberGroupSizes.size())
                    {
                        ++groupIndex;
                        size = info.NumberGroupSizes[groupIndex];
                    }
                    total += size;
                }
            }
            std::int32_t separatorIndex = static_cast<std::int32_t>(separators.size()) - 1;

            std::string output;
            // A float that rounds away to nothing keeps its sign here too:
            // (-0.001).ToString("0.00") is "-0.00" since .NET Core 3.0.
            if (number.Negative && section == 0)
            {
                output += info.NegativeSign;
            }

            const auto maybeSeparator = [&]
            {
                if (thousandSeps && digPos > 1 && separatorIndex >= 0
                    && digPos == separators[static_cast<std::size_t>(separatorIndex)] + 1)
                {
                    output += info.NumberGroupSeparator;
                    --separatorIndex;
                }
            };

            bool decimalWritten = false;
            std::size_t cur = 0;
            std::size_t src = section;
            while (src < format.size() && format[src] != ';')
            {
                if (StartsWithAt(format, src, PerMille))
                {
                    src += PerMille.size();
                    output += info.PerMilleSymbol;
                    continue;
                }
                char ch = format[src++];
                if (adjust > 0 && (ch == '#' || ch == '0' || ch == '.'))
                {
                    while (adjust > 0)
                    {
                        AppendDigit(output, number.Digits, cur);
                        maybeSeparator();
                        --digPos;
                        --adjust;
                    }
                }
                switch (ch)
                {
                case '#':
                case '0':
                {
                    char digit = '\0';
                    if (adjust < 0)
                    {
                        ++adjust;
                        digit = digPos <= firstDigit ? '0' : '\0';
                    }
                    else if (cur < number.Digits.size())
                    {
                        digit = number.Digits[cur++];
                    }
                    else
                    {
                        digit = digPos > lastDigit ? '0' : '\0';
                    }
                    if (digit != '\0')
                    {
                        output.push_back(digit);
                        maybeSeparator();
                    }
                    --digPos;
                    break;
                }
                case '.':
                    if (digPos != 0 || decimalWritten)
                    {
                        break;
                    }
                    if (lastDigit < 0 || (decimalPos < digitCount && cur < number.Digits.size()))
                    {
                        output += info.NumberDecimalSeparator;
                        decimalWritten = true;
                    }
                    break;
                case '%':
                    output += info.PercentSymbol;
                    break;
                case ',':
                    break;
                case '\'':
                case '"':
                    while (src < format.size() && format[src] != ch)
                    {
                        output.push_back(format[src++]);
                    }
                    if (src < format.size())
                    {
                        ++src;
                    }
                    break;
                case '\\':
                    if (src < format.size())
                    {
                        output.push_back(format[src++]);
                    }
                    break;
                case 'E':
                case 'e':
                {
                    bool positiveSign = false;
                    std::int32_t minDigits = 0;
                    if (scientific)
                    {
                        if (src < format.size() && format[src] == '0')
                        {
                            ++minDigits;
                        }
                        else if (src + 1 < format.size() && format[src] == '+' && format[src + 1] == '0')
                        {
                            positiveSign = true;
                        }
                        else if (!(src + 1 < format.size() && format[src] == '-' && format[src + 1] == '0'))
                        {
                            output.push_back(ch);
                            break;
                        }
                        while (++src < format.size() && format[src] == '0')
                        {
                            ++minDigits;
                        }
                        minDigits = std::min(minDigits, 10);
                        const std::int32_t exponent = number.Digits.empty() ? 0 : number.Scale - decimalPos;
                        FormatExponent(output, info, exponent, ch, minDigits, positiveSign);
                        scientific = false;
                    }
                    else
                    {
                        output.push_back(ch);
                        if (src < format.size() && (format[src] == '+' || format[src] == '-'))
                        {
                            output.push_back(format[src++]);
                        }
                        while (src < format.size() && format[src] == '0')
                        {
                            output.push_back(format[src++]);
                        }
                    }
                    break;
                }
                default:
                    output.push_back(ch);
                    break;
                }
            }
            return output;
        }

        template <class TFloat>
        [[nodiscard]] std::string FloatToString(TFloat value, std::string_view format,
            const NumberFormatInfo& info)
        {
            // IBinaryFloatParseAndFormatInfo: the digits a custom format
            // starts from (MaxPrecisionCustomFormat), and the fewest the
            // shortest form is laid out against before it turns to E
            // notation (MaxRoundTripDigits).
            constexpr std::int32_t Precision = sizeof(TFloat) == 4 ? 7 : 15;
            constexpr std::int32_t RoundTripDigits = sizeof(TFloat) == 4 ? 9 : 17;
            if (std::isnan(value))
            {
                return info.NaNSymbol;
            }
            if (std::isinf(value))
            {
                return value < 0 ? info.NegativeInfinitySymbol : info.PositiveInfinitySymbol;
            }
            const Specifier specifier = ParseSpecifier(format);
            if (specifier.Format == '\0')
            {
                NumberBuffer number = FloatDigits(value, Cutoff::Significant, Precision);
                return CustomFormat(number, format, info);
            }
            std::int32_t precision = specifier.Precision;
            char standard = specifier.Format;
            switch (standard)
            {
            case 'R':
            case 'r':
                standard = standard == 'R' ? 'G' : 'g';
                precision = -1;
                [[fallthrough]];
            case 'G':
            case 'g':
            {
                if (precision == 0)
                {
                    precision = -1;
                }
                NumberBuffer number = precision < 0
                    ? FloatDigits(value, Cutoff::Shortest, 0)
                    : FloatDigits(value, Cutoff::Significant, precision);
                const std::int32_t maxDigits = precision < 0
                    ? std::max(static_cast<std::int32_t>(number.Digits.size()), RoundTripDigits)
                    : precision;
                return StandardFormat(number, standard, maxDigits, info, true);
            }
            case 'E':
            case 'e':
            {
                const std::int32_t digits = (precision < 0 ? 6 : precision) + 1;
                NumberBuffer number = FloatDigits(value, Cutoff::Significant, digits);
                return StandardFormat(number, standard, digits - 1, info, true);
            }
            case 'F':
            case 'f':
            case 'N':
            case 'n':
            {
                const std::int32_t digits = precision < 0 ? info.NumberDecimalDigits : precision;
                NumberBuffer number = FloatDigits(value, Cutoff::Fractional, digits);
                return StandardFormat(number, standard, digits, info, true);
            }
            case 'P':
            case 'p':
            {
                const std::int32_t digits = precision < 0 ? info.PercentDecimalDigits : precision;
                NumberBuffer number = FloatDigits(value, Cutoff::Fractional, digits + 2);
                return StandardFormat(number, standard, digits, info, true);
            }
            default:
                throw System::FormatException("Format specifier was invalid.");
            }
        }

        // ==== Parsing ============================================================

        [[nodiscard]] constexpr bool IsWhite(char ch) noexcept
        {
            return ch == ' ' || (ch >= '\t' && ch <= '\r');
        }

        [[nodiscard]] constexpr bool IsDigit(char ch) noexcept
        {
            return ch >= '0' && ch <= '9';
        }

        // Number.MatchChars: `token` at `at`, where a no-break space in the
        // token also matches an ordinary one. Returns the bytes matched.
        [[nodiscard]] std::size_t MatchChars(std::string_view text, std::size_t at, std::string_view token) noexcept
        {
            if (token.empty())
            {
                return 0;
            }
            std::size_t t = at;
            std::size_t k = 0;
            while (k < token.size())
            {
                if (t >= text.size())
                {
                    return 0;
                }
                const Utf8Scalar scalar = DecodeUtf8Scalar(token, k);
                if ((scalar.Value == U' ' || scalar.Value == U' ') && text[t] == ' ')
                {
                    ++t;
                }
                else if (text.substr(t, scalar.Length) == token.substr(k, scalar.Length))
                {
                    t += scalar.Length;
                }
                else
                {
                    return 0;
                }
                k += scalar.Length;
            }
            return t - at;
        }

        [[nodiscard]] std::size_t MatchNegativeSign(std::string_view text, std::size_t at,
            const NumberFormatInfo& info) noexcept
        {
            if (const std::size_t matched = MatchChars(text, at, info.NegativeSign); matched != 0)
            {
                return matched;
            }
            return info.AllowHyphenDuringParsing && at < text.size() && text[at] == '-' ? 1 : 0;
        }

        // Number.TryParseNumber followed by the TrailingZeros check its
        // callers make: the whole text, or the text and then only NULs.
        // `decimalKind` keeps what a decimal keeps and a float drops: the
        // trailing zeros ("1.50") and the scale of a zero ("0.00").
        [[nodiscard]] bool TryParseNumber(std::string_view text, NumberStyles styles,
            const NumberFormatInfo& info, NumberBuffer& number, bool decimalKind = false)
        {
            constexpr std::uint32_t StateSign = 0x01;
            constexpr std::uint32_t StateParens = 0x02;
            constexpr std::uint32_t StateDigits = 0x04;
            constexpr std::uint32_t StateNonZero = 0x08;
            constexpr std::uint32_t StateDecimal = 0x10;

            number.Digits.clear();
            number.Scale = 0;
            number.Negative = false;
            std::uint32_t state = 0;
            std::size_t p = 0;
            const auto at = [&](std::size_t index) { return index < text.size() ? text[index] : '\0'; };

            while (true)
            {
                const char ch = at(p);
                // White space after a sign only where the culture puts one there.
                if (!IsWhite(ch) || !HasStyle(styles, NumberStyles::AllowLeadingWhite)
                    || ((state & StateSign) != 0 && info.NumberNegativePattern != 2))
                {
                    std::size_t matched = 0;
                    if (HasStyle(styles, NumberStyles::AllowLeadingSign) && (state & StateSign) == 0
                        && ((matched = MatchChars(text, p, info.PositiveSign)) != 0
                            || ((matched = MatchNegativeSign(text, p, info)) != 0 && (number.Negative = true))))
                    {
                        state |= StateSign;
                        p += matched;
                        continue;
                    }
                    if (ch == '(' && HasStyle(styles, NumberStyles::AllowParentheses) && (state & StateSign) == 0)
                    {
                        state |= StateSign | StateParens;
                        number.Negative = true;
                        ++p;
                        continue;
                    }
                    break;
                }
                ++p;
            }

            while (true)
            {
                const char ch = at(p);
                std::size_t matched = 0;
                if (IsDigit(ch))
                {
                    state |= StateDigits;
                    if (ch != '0' || (state & StateNonZero) != 0)
                    {
                        number.Digits.push_back(ch);
                        if ((state & StateDecimal) == 0)
                        {
                            ++number.Scale;
                        }
                        state |= StateNonZero;
                    }
                    else if ((state & StateDecimal) != 0)
                    {
                        --number.Scale;
                    }
                    ++p;
                }
                else if (HasStyle(styles, NumberStyles::AllowDecimalPoint) && (state & StateDecimal) == 0
                    && (matched = MatchChars(text, p, info.NumberDecimalSeparator)) != 0)
                {
                    state |= StateDecimal;
                    p += matched;
                }
                else if (HasStyle(styles, NumberStyles::AllowThousands) && (state & StateDigits) != 0
                    && (state & StateDecimal) == 0
                    && (matched = MatchChars(text, p, info.NumberGroupSeparator)) != 0)
                {
                    p += matched;
                }
                else
                {
                    break;
                }
            }

            if ((state & StateDigits) == 0)
            {
                return false;
            }
            if ((at(p) == 'E' || at(p) == 'e') && HasStyle(styles, NumberStyles::AllowExponent))
            {
                const std::size_t mark = p;
                ++p;
                bool negativeExponent = false;
                if (const std::size_t matched = MatchChars(text, p, info.PositiveSign); matched != 0)
                {
                    p += matched;
                }
                else if (const std::size_t negative = MatchNegativeSign(text, p, info); negative != 0)
                {
                    p += negative;
                    negativeExponent = true;
                }
                if (IsDigit(at(p)))
                {
                    std::int32_t exponent = 0;
                    while (IsDigit(at(p)))
                    {
                        // Past this the value is 0 or infinity whatever follows.
                        if (exponent < 100000)
                        {
                            exponent = exponent * 10 + (at(p) - '0');
                        }
                        ++p;
                    }
                    number.Scale += negativeExponent ? -exponent : exponent;
                }
                else
                {
                    p = mark;
                }
            }

            while (true)
            {
                const char ch = at(p);
                if (!IsWhite(ch) || !HasStyle(styles, NumberStyles::AllowTrailingWhite))
                {
                    std::size_t matched = 0;
                    if (HasStyle(styles, NumberStyles::AllowTrailingSign) && (state & StateSign) == 0
                        && ((matched = MatchChars(text, p, info.PositiveSign)) != 0
                            || ((matched = MatchNegativeSign(text, p, info)) != 0 && (number.Negative = true))))
                    {
                        state |= StateSign;
                        p += matched;
                        continue;
                    }
                    if (ch == ')' && (state & StateParens) != 0)
                    {
                        state &= ~StateParens;
                        ++p;
                        continue;
                    }
                    break;
                }
                ++p;
            }
            if ((state & StateParens) != 0)
            {
                return false;
            }
            if (!decimalKind)
            {
                if ((state & StateNonZero) == 0)
                {
                    number.Scale = 0;
                }
                number.Digits.erase(number.Digits.find_last_not_of('0') + 1);
            }
            // Number.TrailingZeros.
            return text.find_first_not_of('\0', p) == std::string_view::npos;
        }

        // float.NaN and double.NaN are 0.0 / 0.0 as x86 computes it: a quiet
        // NaN with the sign bit set, which is what parsing "NaN" gives too.
        template <class TFloat>
        [[nodiscard]] TFloat DotNetNaN() noexcept
        {
            return std::copysign(std::numeric_limits<TFloat>::quiet_NaN(), TFloat(-1));
        }

        // Number.TryParseFloat's second chance: the culture's names for
        // infinity and NaN, in any case, trimmed of char.IsWhiteSpace.
        template <class TFloat>
        [[nodiscard]] bool TryParseSpecial(std::string_view text, const NumberFormatInfo& info, TFloat& value)
        {
            const std::string_view trimmed = StringTrimView(text);
            const auto equals = [](std::string_view left, std::string_view right)
            {
                return StringEqualsOrdinalIgnoreCase(left, right);
            };
            if (equals(trimmed, info.PositiveInfinitySymbol))
            {
                value = std::numeric_limits<TFloat>::infinity();
                return true;
            }
            if (equals(trimmed, info.NegativeInfinitySymbol))
            {
                value = -std::numeric_limits<TFloat>::infinity();
                return true;
            }
            if (equals(trimmed, info.NaNSymbol))
            {
                value = DotNetNaN<TFloat>();
                return true;
            }
            if (StringStartsWithOrdinalIgnoreCase(trimmed, info.PositiveSign))
            {
                const std::string_view rest = trimmed.substr(info.PositiveSign.size());
                if (equals(rest, info.PositiveInfinitySymbol))
                {
                    value = std::numeric_limits<TFloat>::infinity();
                    return true;
                }
                if (equals(rest, info.NaNSymbol))
                {
                    value = DotNetNaN<TFloat>();
                    return true;
                }
                return false;
            }
            if ((StringStartsWithOrdinalIgnoreCase(trimmed, info.NegativeSign)
                    && equals(trimmed.substr(info.NegativeSign.size()), info.NaNSymbol))
                || (info.AllowHyphenDuringParsing && !trimmed.empty() && trimmed.front() == '-'
                    && equals(trimmed.substr(1), info.NaNSymbol)))
            {
                value = DotNetNaN<TFloat>();
                return true;
            }
            return false;
        }

        template <class TFloat>
        [[nodiscard]] bool TryParseFloat(std::string_view text, NumberStyles styles,
            const NumberFormatInfo& info, TFloat& value)
        {
            NumberBuffer number;
            if (!TryParseNumber(text, styles, info, number))
            {
                if (TryParseSpecial(text, info, value))
                {
                    return true;
                }
                value = 0;
                return false;
            }
            if (number.Digits.empty())
            {
                value = number.Negative ? -TFloat(0) : TFloat(0);
                return true;
            }
            // The digits as "0.DDDDe<scale>", which from_chars rounds
            // correctly straight to the target type.
            std::string decimal = "0." + number.Digits + "e" + std::to_string(number.Scale);
            TFloat parsed = 0;
            const auto result = FromChars(decimal.data(), decimal.data() + decimal.size(), parsed,
                std::chars_format::general);
            if (result.ec == std::errc::result_out_of_range)
            {
                // Too large is infinity and too small is zero; .NET succeeds
                // with either.
                parsed = number.Scale > 0 ? std::numeric_limits<TFloat>::infinity() : TFloat(0);
            }
            else if (result.ec != std::errc{})
            {
                value = 0;
                return false;
            }
            value = number.Negative ? -parsed : parsed;
            return true;
        }

        [[nodiscard]] std::int32_t HexValue(char ch) noexcept
        {
            if (ch >= '0' && ch <= '9')
            {
                return ch - '0';
            }
            if (ch >= 'a' && ch <= 'f')
            {
                return ch - 'a' + 10;
            }
            if (ch >= 'A' && ch <= 'F')
            {
                return ch - 'A' + 10;
            }
            return -1;
        }

        enum class ParseStatus
        {
            Ok,
            Failed,
            Overflow,
        };

        // Number.TryParseBinaryIntegerHexNumberStyle.
        [[nodiscard]] ParseStatus TryParseHex(std::string_view text, NumberStyles styles, std::int32_t bits,
            std::uint64_t& value)
        {
            std::size_t p = 0;
            if (HasStyle(styles, NumberStyles::AllowLeadingWhite))
            {
                while (p < text.size() && IsWhite(text[p]))
                {
                    ++p;
                }
            }
            if (p >= text.size() || HexValue(text[p]) < 0)
            {
                return ParseStatus::Failed;
            }
            std::uint64_t result = 0;
            std::int32_t significant = 0;
            bool overflow = false;
            for (; p < text.size() && HexValue(text[p]) >= 0; ++p)
            {
                const std::int32_t digit = HexValue(text[p]);
                if (significant == 0 && digit == 0)
                {
                    continue;
                }
                if (++significant > bits / 4)
                {
                    overflow = true;
                    continue;
                }
                result = (result << 4) | static_cast<std::uint64_t>(digit);
            }
            if (HasStyle(styles, NumberStyles::AllowTrailingWhite))
            {
                while (p < text.size() && IsWhite(text[p]))
                {
                    ++p;
                }
            }
            if (text.find_first_not_of('\0', p) != std::string_view::npos)
            {
                return ParseStatus::Failed;
            }
            if (overflow)
            {
                return ParseStatus::Overflow;
            }
            value = result;
            return ParseStatus::Ok;
        }
    }

    // ==== Public ==================================================================

    const NumberFormatInfo& NumberFormatInfo::InvariantInfo() noexcept
    {
        static const NumberFormatInfo invariant;
        return invariant;
    }

    namespace
    {
        thread_local bool InvariantThread = false;
    }

    const NumberFormatInfo& NumberFormatInfo::CurrentInfo()
    {
        if (InvariantThread)
        {
            return InvariantInfo();
        }
        static const NumberFormatInfo current = LoadCurrent();
        return current;
    }

    void UseInvariantCultureOnThisThread() noexcept
    {
        InvariantThread = true;
    }

    bool CurrentCultureIsInvariantOnThisThread() noexcept
    {
        return InvariantThread;
    }

    std::string NumberToString(double value, std::string_view format, const NumberFormatInfo& info)
    {
        return FloatToString(value, format, info);
    }

    std::string NumberToString(float value, std::string_view format, const NumberFormatInfo& info)
    {
        return FloatToString(value, format, info);
    }

    std::string IntegerToString(std::uint64_t magnitude, bool negative, std::uint64_t bits,
        std::int32_t byteWidth, std::string_view format, const NumberFormatInfo& info)
    {
        const Specifier specifier = ParseSpecifier(format);
        const std::int32_t precision = specifier.Precision;
        switch (specifier.Format)
        {
        case '\0':
        {
            NumberBuffer number = IntegerDigits(magnitude, negative);
            return CustomFormat(number, format, info);
        }
        case 'G':
        case 'g':
            if (precision <= 0)
            {
                std::string text = negative ? info.NegativeSign : std::string();
                text += std::to_string(magnitude);
                return text;
            }
            break;
        case 'D':
        case 'd':
        {
            std::string digits = std::to_string(magnitude);
            if (precision > static_cast<std::int32_t>(digits.size()))
            {
                digits.insert(0, static_cast<std::size_t>(precision) - digits.size(), '0');
            }
            return negative ? info.NegativeSign + digits : digits;
        }
        case 'X':
        case 'x':
        case 'B':
        case 'b':
        {
            const bool binary = specifier.Format == 'B' || specifier.Format == 'b';
            const std::uint64_t mask = byteWidth >= 8
                ? ~std::uint64_t(0)
                : (std::uint64_t(1) << (byteWidth * 8)) - 1;
            std::array<char, 72> buffer{};
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                bits & mask, binary ? 2 : 16);
            std::string digits(buffer.data(), result.ptr);
            if (specifier.Format == 'X')
            {
                std::transform(digits.begin(), digits.end(), digits.begin(),
                    [](char ch) { return ch >= 'a' && ch <= 'f' ? static_cast<char>(ch - 'a' + 'A') : ch; });
            }
            if (precision > static_cast<std::int32_t>(digits.size()))
            {
                digits.insert(0, static_cast<std::size_t>(precision) - digits.size(), '0');
            }
            return digits;
        }
        case 'R':
        case 'r':
            throw System::FormatException("Format specifier was invalid.");
        default:
            break;
        }
        NumberBuffer number = IntegerDigits(magnitude, negative);
        std::int32_t maxDigits = precision;
        if ((specifier.Format == 'G' || specifier.Format == 'g') && maxDigits < 1)
        {
            maxDigits = static_cast<std::int32_t>(number.Digits.size());
        }
        return StandardFormat(number, specifier.Format, maxDigits, info, false);
    }

    namespace
    {
        // Number.DecimalToNumber: every digit of the coefficient, trailing
        // zeros included, which is how 1.50m prints as "1.50".
        [[nodiscard]] NumberBuffer DecimalDigits(const DecimalBits& value)
        {
            std::array<std::uint32_t, 3> limbs{ value.Lo, value.Mid, value.Hi };
            std::string reversed;
            while ((limbs[0] | limbs[1] | limbs[2]) != 0)
            {
                std::uint64_t remainder = 0;
                for (std::size_t i = limbs.size(); i-- > 0;)
                {
                    const std::uint64_t current = (remainder << 32) | limbs[i];
                    limbs[i] = static_cast<std::uint32_t>(current / 10);
                    remainder = current % 10;
                }
                reversed.push_back(static_cast<char>('0' + remainder));
            }
            NumberBuffer number;
            number.Negative = value.Negative;
            number.Digits.assign(reversed.rbegin(), reversed.rend());
            number.Scale = static_cast<std::int32_t>(number.Digits.size()) - value.Scale;
            return number;
        }

        void TrimTrailingZeros(NumberBuffer& number)
        {
            number.Digits.erase(number.Digits.find_last_not_of('0') + 1);
        }
    }

    std::string DecimalToString(const DecimalBits& value, std::string_view format, const NumberFormatInfo& info)
    {
        NumberBuffer number = DecimalDigits(value);
        const Specifier specifier = ParseSpecifier(format);
        if (specifier.Format == '\0')
        {
            TrimTrailingZeros(number);
            return CustomFormat(number, format, info);
        }
        if ((specifier.Format == 'G' || specifier.Format == 'g') && specifier.Precision <= 0)
        {
            // Every digit, never in E notation, and no sign on a zero.
            std::string output;
            if (number.Negative && !number.Digits.empty())
            {
                output += info.NegativeSign;
            }
            FormatGeneral(output, number, -1, info, 'E', true);
            return output;
        }
        switch (specifier.Format)
        {
        case 'F': case 'f': case 'N': case 'n': case 'E': case 'e':
        case 'G': case 'g': case 'P': case 'p':
            TrimTrailingZeros(number);
            return StandardFormat(number, specifier.Format, specifier.Precision, info, false);
        default:
            throw System::FormatException("Format specifier was invalid.");
        }
    }

    bool TryParseDecimal(std::string_view text, NumberStyles styles, const NumberFormatInfo& info,
        DecimalBits& value)
    {
        value = DecimalBits{};
        NumberBuffer number;
        if (!TryParseNumber(text, styles, info, number, true))
        {
            return false;
        }
        // Number.NumberToDecimal.
        constexpr std::int32_t DecimalPrecision = 29;
        const std::string& digits = number.Digits;
        std::size_t p = 0;
        const auto digit = [&]() -> std::uint32_t
        {
            return p < digits.size() ? static_cast<std::uint32_t>(digits[p]) : 0U;
        };
        std::int32_t e = number.Scale;
        const bool sign = number.Negative;
        std::uint32_t c = digit();
        if (c == 0)
        {
            value.Negative = sign;
            value.Scale = static_cast<std::uint8_t>(std::clamp(-e, 0, 28));
            return true;
        }
        if (e > DecimalPrecision)
        {
            return false;
        }

        std::uint64_t low64 = 0;
        while (e > -28)
        {
            --e;
            low64 *= 10;
            low64 += c - '0';
            ++p;
            c = digit();
            if (low64 >= UINT64_MAX / 10)
            {
                break;
            }
            if (c == 0)
            {
                while (e > 0)
                {
                    --e;
                    low64 *= 10;
                    if (low64 >= UINT64_MAX / 10)
                    {
                        break;
                    }
                }
                break;
            }
        }

        std::uint32_t high = 0;
        while ((e > 0 || (c != 0 && e > -28))
            && (high < UINT32_MAX / 10 || (high == UINT32_MAX / 10
                && (low64 < 0x9999999999999999ULL || (low64 == 0x9999999999999999ULL && c <= '5')))))
        {
            const std::uint64_t tmpLow = static_cast<std::uint64_t>(static_cast<std::uint32_t>(low64)) * 10U;
            const std::uint64_t tmp64 = (low64 >> 32) * 10U + (tmpLow >> 32);
            low64 = static_cast<std::uint32_t>(tmpLow) + (tmp64 << 32);
            high = static_cast<std::uint32_t>(tmp64 >> 32) + high * 10U;
            if (c != 0)
            {
                c -= '0';
                low64 += c;
                if (low64 < c)
                {
                    ++high;
                }
                ++p;
                c = digit();
            }
            --e;
        }

        bool round = c >= '5';
        if (round && c == '5' && (low64 & 1) == 0)
        {
            ++p;
            c = digit();
            bool hasZeroTail = true;
            while (c != 0 && hasZeroTail)
            {
                hasZeroTail &= c == '0';
                ++p;
                c = digit();
            }
            round = !hasZeroTail;
        }
        if (round && ++low64 == 0 && ++high == 0)
        {
            low64 = 0x999999999999999AULL;
            high = UINT32_MAX / 10;
            ++e;
        }

        if (e > 0)
        {
            return false;
        }
        value.Negative = sign;
        if (e <= -DecimalPrecision)
        {
            value.Scale = DecimalPrecision - 1;
        }
        else
        {
            value.Lo = static_cast<std::uint32_t>(low64);
            value.Mid = static_cast<std::uint32_t>(low64 >> 32);
            value.Hi = high;
            value.Scale = static_cast<std::uint8_t>(-e);
        }
        return true;
    }

    bool TryParseDouble(std::string_view text, NumberStyles styles, const NumberFormatInfo& info, double& value)
    {
        return TryParseFloat(text, styles, info, value);
    }

    bool TryParseSingle(std::string_view text, NumberStyles styles, const NumberFormatInfo& info, float& value)
    {
        return TryParseFloat(text, styles, info, value);
    }

    namespace
    {
        [[nodiscard]] ParseStatus ParseIntegerCore(std::string_view text, NumberStyles styles,
            const NumberFormatInfo& info, bool isSigned, std::int32_t bits, std::int64_t& value)
        {
        value = 0;
        const std::uint64_t unsignedMax = bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
        if (HasStyle(styles, NumberStyles::AllowHexSpecifier))
        {
            std::uint64_t raw = 0;
            if (const ParseStatus status = TryParseHex(text, styles, bits, raw); status != ParseStatus::Ok)
            {
                return status;
            }
            // Hex digits are the bits of the type: "FFFFFFFF" is -1 for an int.
            if (isSigned && bits < 64 && (raw & (std::uint64_t(1) << (bits - 1))) != 0)
            {
                raw |= ~unsignedMax;
            }
            value = static_cast<std::int64_t>(raw);
            return ParseStatus::Ok;
        }

        NumberBuffer number;
        if (!TryParseNumber(text, styles, info, number))
        {
            return ParseStatus::Failed;
        }
        // Number.TryNumberToInt32 and friends: any fractional digit left is
        // a failure, and so is anything out of range.
        if (static_cast<std::int32_t>(number.Digits.size()) > number.Scale)
        {
            // A fractional part that is not zero: "1.5" is not an integer.
            return ParseStatus::Overflow;
        }
        if (number.Scale > 20)
        {
            return ParseStatus::Overflow;
        }
        std::uint64_t magnitude = 0;
        for (std::int32_t i = 0; i < number.Scale; ++i)
        {
            const std::uint64_t digit = i < static_cast<std::int32_t>(number.Digits.size())
                ? static_cast<std::uint64_t>(number.Digits[static_cast<std::size_t>(i)] - '0') : 0;
            if (magnitude > (~std::uint64_t(0) - digit) / 10)
            {
                return ParseStatus::Overflow;
            }
            magnitude = magnitude * 10 + digit;
        }
        if (isSigned)
        {
            const std::uint64_t limit = (unsignedMax >> 1) + (number.Negative ? 1 : 0);
            if (magnitude > limit)
            {
                return ParseStatus::Overflow;
            }
            value = number.Negative
                ? static_cast<std::int64_t>(std::uint64_t(0) - magnitude)
                : static_cast<std::int64_t>(magnitude);
            return ParseStatus::Ok;
        }
        if (magnitude > unsignedMax || (number.Negative && magnitude != 0))
        {
            return ParseStatus::Overflow;
        }
        value = static_cast<std::int64_t>(magnitude);
        return ParseStatus::Ok;
        }

        [[nodiscard]] std::string IntegerTypeName(bool isSigned, std::int32_t bits)
        {
            switch (bits)
            {
            case 8:
                return isSigned ? "SByte" : "Byte";
            case 16:
                return isSigned ? "Int16" : "UInt16";
            case 32:
                return isSigned ? "Int32" : "UInt32";
            default:
                return isSigned ? "Int64" : "UInt64";
            }
        }
    }

    bool TryParseInteger(std::string_view text, NumberStyles styles, const NumberFormatInfo& info,
        bool isSigned, std::int32_t bits, std::int64_t& value)
    {
        const bool ok = ParseIntegerCore(text, styles, info, isSigned, bits, value) == ParseStatus::Ok;
        if (!ok)
        {
            value = 0;
        }
        return ok;
    }

    std::int64_t ParseInteger(std::string_view text, NumberStyles styles, const NumberFormatInfo& info,
        bool isSigned, std::int32_t bits)
    {
        std::int64_t value = 0;
        switch (ParseIntegerCore(text, styles, info, isSigned, bits, value))
        {
        case ParseStatus::Ok:
            return value;
        case ParseStatus::Overflow:
        {
            // SR.Overflow_Int32 and its siblings.
            const std::string type = bits == 8
                ? (isSigned ? "a signed byte" : "an unsigned byte")
                : (isSigned ? "an " : "a ") + IntegerTypeName(isSigned, bits);
            throw System::OverflowException("Value was either too large or too small for " + type + ".");
        }
        default:
            throw System::FormatException(
                "The input string '" + std::string(text) + "' was not in a correct format.");
        }
    }

    bool SingleTryParseCurrentCulture(std::string_view text, float& value)
    {
        return TryParseSingle(text, NumberStyles::Float | NumberStyles::AllowThousands,
            NumberFormatInfo::CurrentInfo(), value);
    }

    bool SingleTryParseInvariant(std::string_view text, float& value)
    {
        return TryParseSingle(text, NumberStyles::Float, NumberFormatInfo::InvariantInfo(), value);
    }

    bool DoubleTryParseCurrentCulture(std::string_view text, double& value)
    {
        return TryParseDouble(text, NumberStyles::Float | NumberStyles::AllowThousands,
            NumberFormatInfo::CurrentInfo(), value);
    }

    bool DoubleTryParseInvariant(std::string_view text, double& value)
    {
        return TryParseDouble(text, NumberStyles::Float, NumberFormatInfo::InvariantInfo(), value);
    }

    bool Int32TryParseCurrentCulture(std::string_view text, std::int32_t& value)
    {
        return TryParseInteger(text, NumberStyles::Integer, NumberFormatInfo::CurrentInfo(), value);
    }

    bool Int32TryParseInvariant(std::string_view text, std::int32_t& value)
    {
        return TryParseInteger(text, NumberStyles::Integer, NumberFormatInfo::InvariantInfo(), value);
    }

    bool Int32TryParseHexNumber(std::string_view text, std::int32_t& value)
    {
        return TryParseInteger(text, NumberStyles::HexNumber, NumberFormatInfo::CurrentInfo(), value);
    }

    std::int32_t Int32ParseHexNumber(std::string_view text)
    {
        return ParseInteger<std::int32_t>(text, NumberStyles::HexNumber, NumberFormatInfo::CurrentInfo());
    }
}
