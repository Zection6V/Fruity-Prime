#include "Common.hpp"

#include <charconv>
#include <cmath>
#include <clocale>
#include <cstring>
#include <cwchar>
#include <cuchar>
#include <iostream>
#include <limits>
#include <locale>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    constexpr std::uint32_t Pcre2Ucp = 0x00020000U;
    constexpr int Pcre2ErrorNoMatch = -1;

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

    [[nodiscard]] std::u16string WideToU16(std::wstring_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        for (wchar_t chr : value)
        {
            const std::uint32_t scalar = static_cast<std::uint32_t>(chr);
            if constexpr (sizeof(wchar_t) == sizeof(char16_t))
            {
                result.push_back(static_cast<char16_t>(scalar));
            }
            else if (scalar <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(scalar));
            }
            else if (scalar <= 0x10FFFFU)
            {
                const std::uint32_t value32 = scalar - 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (value32 >> 10U)));
                result.push_back(static_cast<char16_t>(0xDC00U + (value32 & 0x3FFU)));
            }
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

    struct NumberFormatData final
    {
        std::u16string PositiveSign = u"+";
        std::u16string NegativeSign = u"-";
        std::u16string DecimalSeparator = u".";
        std::u16string NaNSymbol = u"NaN";
        std::u16string PositiveInfinitySymbol = u"Infinity";
        std::u16string NegativeInfinitySymbol = u"-Infinity";
    };

#if defined(_WIN32)
    [[nodiscard]] std::u16string GetWindowsLocaleInfo(LCTYPE type)
    {
        const int required = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type, nullptr, 0);
        if (required <= 1)
        {
            return {};
        }
        std::wstring value(static_cast<std::size_t>(required), L'\0');
        if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type, value.data(), required) == 0)
        {
            return {};
        }
        value.resize(static_cast<std::size_t>(required - 1));
        return WideToU16(value);
    }
#endif

    [[nodiscard]] NumberFormatData CurrentNumberFormat()
    {
        NumberFormatData result;
#if defined(_WIN32)
        if (std::u16string value = GetWindowsLocaleInfo(LOCALE_SPOSITIVESIGN); !value.empty())
        {
            result.PositiveSign = std::move(value);
        }
        if (std::u16string value = GetWindowsLocaleInfo(LOCALE_SNEGATIVESIGN); !value.empty())
        {
            result.NegativeSign = std::move(value);
        }
        if (std::u16string value = GetWindowsLocaleInfo(LOCALE_SDECIMAL); !value.empty())
        {
            result.DecimalSeparator = std::move(value);
        }
#if defined(LOCALE_SNAN)
        if (std::u16string value = GetWindowsLocaleInfo(LOCALE_SNAN); !value.empty())
        {
            result.NaNSymbol = std::move(value);
        }
#endif
#if defined(LOCALE_SPOSINFINITY)
        if (std::u16string value = GetWindowsLocaleInfo(LOCALE_SPOSINFINITY); !value.empty())
        {
            result.PositiveInfinitySymbol = std::move(value);
        }
#endif
#if defined(LOCALE_SNEGINFINITY)
        if (std::u16string value = GetWindowsLocaleInfo(LOCALE_SNEGINFINITY); !value.empty())
        {
            result.NegativeInfinitySymbol = std::move(value);
        }
#endif
#else
        try
        {
            const std::locale locale;
            const auto& punctuation = std::use_facet<std::numpunct<wchar_t>>(locale);
            result.DecimalSeparator = WideToU16(std::wstring(1, punctuation.decimal_point()));
        }
        catch (const std::runtime_error&)
        {
        }

        if (const lconv* convention = std::localeconv(); convention != nullptr)
        {
            if (std::u16string value = LocaleBytesToU16(convention->positive_sign); !value.empty())
            {
                result.PositiveSign = std::move(value);
            }
            if (std::u16string value = LocaleBytesToU16(convention->negative_sign); !value.empty())
            {
                result.NegativeSign = std::move(value);
            }
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

        const NumberFormatData format = CurrentNumberFormat();
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

    [[nodiscard]] std::u16string FormatIntD2(std::int32_t value, const NumberFormatData& format)
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

    [[nodiscard]] std::u16string FormatSeconds(float value, const NumberFormatData& format)
    {
        if (std::isnan(value))
        {
            return format.NaNSymbol;
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? format.NegativeInfinitySymbol : format.PositiveInfinitySymbol;
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
                result += format.DecimalSeparator;
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

    [[nodiscard]] std::size_t HashCombine(std::size_t hash, std::size_t value) noexcept
    {
        return hash ^ (value + static_cast<std::size_t>(0x9E3779B9U) + (hash << 6U) + (hash >> 2U));
    }

    class Pcre2Api final
    {
    public:
        using Size = std::size_t;
        using Code = void;
        using MatchData = void;
        using CompileFunction = Code* (*)(
            const std::uint16_t*, Size, std::uint32_t, int*, Size*, void*);
        using CodeFreeFunction = void (*)(Code*);
        using MatchDataCreateFunction = MatchData* (*)(const Code*, void*);
        using MatchFunction = int (*)(
            const Code*, const std::uint16_t*, Size, Size, std::uint32_t, MatchData*, void*);
        using MatchDataFreeFunction = void (*)(MatchData*);

    private:
#if defined(_WIN32)
        HMODULE _module = nullptr;
#else
        void* _module = nullptr;
#endif

        template <typename T>
        [[nodiscard]] T LoadFunction(const char* name) const noexcept
        {
#if defined(_WIN32)
            FARPROC raw = _module == nullptr ? nullptr : GetProcAddress(_module, name);
            T result = nullptr;
            static_assert(sizeof(result) == sizeof(raw));
            std::memcpy(&result, &raw, sizeof(result));
            return result;
#else
            void* raw = _module == nullptr ? nullptr : dlsym(_module, name);
            T result = nullptr;
            static_assert(sizeof(result) == sizeof(raw));
            std::memcpy(&result, &raw, sizeof(result));
            return result;
#endif
        }

    public:
        CompileFunction Compile = nullptr;
        CodeFreeFunction CodeFree = nullptr;
        MatchDataCreateFunction MatchDataCreateFromPattern = nullptr;
        MatchFunction Match = nullptr;
        MatchDataFreeFunction MatchDataFree = nullptr;

        Pcre2Api() noexcept
        {
#if defined(_WIN32)
            constexpr const wchar_t* names[] = {L"pcre2-16.dll", L"libpcre2-16-0.dll"};
            for (const wchar_t* name : names)
            {
                _module = LoadLibraryW(name);
                if (_module != nullptr)
                {
                    break;
                }
            }
#else
            constexpr const char* names[] = {"libpcre2-16.so.0", "libpcre2-16.so"};
            for (const char* name : names)
            {
                _module = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
                if (_module != nullptr)
                {
                    break;
                }
            }
#endif
            Compile = LoadFunction<CompileFunction>("pcre2_compile_16");
            CodeFree = LoadFunction<CodeFreeFunction>("pcre2_code_free_16");
            MatchDataCreateFromPattern = LoadFunction<MatchDataCreateFunction>(
                "pcre2_match_data_create_from_pattern_16");
            Match = LoadFunction<MatchFunction>("pcre2_match_16");
            MatchDataFree = LoadFunction<MatchDataFreeFunction>("pcre2_match_data_free_16");
            if (!Available())
            {
#if defined(_WIN32)
                if (_module != nullptr)
                {
                    FreeLibrary(_module);
                    _module = nullptr;
                }
#else
                if (_module != nullptr)
                {
                    dlclose(_module);
                    _module = nullptr;
                }
#endif
            }
        }

        Pcre2Api(const Pcre2Api&) = delete;
        Pcre2Api& operator=(const Pcre2Api&) = delete;

        ~Pcre2Api()
        {
#if defined(_WIN32)
            if (_module != nullptr)
            {
                FreeLibrary(_module);
            }
#else
            if (_module != nullptr)
            {
                dlclose(_module);
            }
#endif
        }

        [[nodiscard]] bool Available() const noexcept
        {
            return _module != nullptr
                && Compile != nullptr
                && CodeFree != nullptr
                && MatchDataCreateFromPattern != nullptr
                && Match != nullptr
                && MatchDataFree != nullptr;
        }
    };

    [[nodiscard]] const std::shared_ptr<Pcre2Api>& GetPcre2Api()
    {
        static const std::shared_ptr<Pcre2Api> api = std::make_shared<Pcre2Api>();
        return api;
    }
}

namespace NCSFCommon
{
    void Common::ThrowNotSupported()
    {
        throw std::logic_error("Specified method is not supported.");
    }

    class Regex::Impl final
    {
    private:
        std::u16string _pattern;
        std::shared_ptr<Pcre2Api> _api;
        Pcre2Api::Code* _code = nullptr;

    public:
        explicit Impl(std::u16string pattern)
            : _pattern(std::move(pattern)),
              _api(GetPcre2Api())
        {
            if (_api->Available())
            {
                int error = 0;
                Pcre2Api::Size errorOffset = 0;
                _code = _api->Compile(
                    reinterpret_cast<const std::uint16_t*>(_pattern.data()),
                    _pattern.size(), Pcre2Ucp, &error, &errorOffset, nullptr);
                if (_code == nullptr)
                {
                    throw std::invalid_argument(
                        "Invalid regular expression at offset " + std::to_string(errorOffset)
                        + " (PCRE2 error " + std::to_string(error) + ").");
                }
            }
            else
            {
                throw std::runtime_error(
                    "A .NET-compatible regular expression engine is unavailable.");
            }
        }

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;

        ~Impl()
        {
            if (_code != nullptr)
            {
                _api->CodeFree(_code);
            }
        }

        [[nodiscard]] bool IsMatch(std::u16string_view input) const
        {
            if (_code != nullptr)
            {
                Pcre2Api::MatchData* data = _api->MatchDataCreateFromPattern(_code, nullptr);
                if (data == nullptr)
                {
                    throw std::bad_alloc();
                }
                const int result = _api->Match(
                    _code,
                    reinterpret_cast<const std::uint16_t*>(input.data()),
                    input.size(), 0, 0, data, nullptr);
                _api->MatchDataFree(data);
                if (result >= 0)
                {
                    return true;
                }
                if (result == Pcre2ErrorNoMatch)
                {
                    return false;
                }
                throw std::runtime_error("Regular expression matching failed with error "
                    + std::to_string(result) + ".");
            }

            throw std::runtime_error(
                "A .NET-compatible regular expression engine is unavailable.");
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

    bool Common::KeepInfo::Equals(const KeepInfo& other) const noexcept
    {
        return EqualityContract() == other.EqualityContract()
            && Filename == other.Filename
            && Keep == other.Keep;
    }

    bool Common::KeepInfo::operator==(const KeepInfo& other) const noexcept
    {
        return Equals(other);
    }

    bool Common::KeepInfo::operator!=(const KeepInfo& other) const noexcept
    {
        return !Equals(other);
    }

    std::size_t Common::KeepInfo::GetHashCode() const noexcept
    {
        std::size_t hash = EqualityContract().hash_code();
        hash = HashCombine(hash, std::hash<std::u16string>{}(Filename));
        hash = HashCombine(hash, std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(Keep)));
        return hash;
    }

    std::u16string Common::KeepInfo::ToString() const
    {
        std::u16string result = u"KeepInfo { Filename = ";
        result += Filename;
        result += u", Keep = ";
        result += KeepTypeName(Keep);
        result += u" }";
        return result;
    }

    void Common::KeepInfo::Deconstruct(std::u16string& filename, KeepType& keep) const
    {
        filename = Filename;
        keep = Keep;
    }

    std::shared_ptr<Common::KeepInfo> Common::KeepInfo::CloneWith(
        std::u16string filename, KeepType keep) const
    {
        return std::make_shared<KeepInfo>(std::move(filename), keep);
    }

    std::shared_ptr<Common::KeepInfo> Common::KeepInfo::WithFilename(std::u16string filename) const
    {
        return CloneWith(std::move(filename), Keep);
    }

    std::shared_ptr<Common::KeepInfo> Common::KeepInfo::WithKeep(KeepType keep) const
    {
        return CloneWith(Filename, keep);
    }

    std::shared_ptr<Common::KeepInfo> Common::KeepInfo::With(
        std::u16string filename, KeepType keep) const
    {
        return CloneWith(std::move(filename), keep);
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

        const NumberFormatData format = CurrentNumberFormat();
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
