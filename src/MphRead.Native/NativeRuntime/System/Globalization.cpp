#include "Globalization.hpp"

#include "Exceptions.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <limits>
#include <locale>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        [[nodiscard]] bool IsManagedWhiteSpace(char32_t value) noexcept
        {
            return (value >= U'\u0009' && value <= U'\u000D') || value == U'\u0020'
                || value == U'\u0085' || value == U'\u00A0' || value == U'\u1680'
                || (value >= U'\u2000' && value <= U'\u200A') || value == U'\u2028'
                || value == U'\u2029' || value == U'\u202F' || value == U'\u205F'
                || value == U'\u3000';
        }

        [[nodiscard]] std::pair<char32_t, std::size_t> DecodeUtf8At(
            std::string_view value, std::size_t offset) noexcept
        {
            const auto byte = static_cast<unsigned char>(value[offset]);
            if (byte < 0x80)
            {
                return {byte, offset + 1};
            }
            auto continuation = [&](std::size_t index) -> std::optional<unsigned char>
            {
                if (index >= value.size())
                {
                    return std::nullopt;
                }
                const auto next = static_cast<unsigned char>(value[index]);
                if ((next & 0xC0U) != 0x80U)
                {
                    return std::nullopt;
                }
                return next;
            };
            if (byte >= 0xC2U && byte <= 0xDFU)
            {
                if (auto b1 = continuation(offset + 1))
                {
                    return {static_cast<char32_t>(((byte & 0x1FU) << 6U) | (*b1 & 0x3FU)), offset + 2};
                }
            }
            else if (byte >= 0xE0U && byte <= 0xEFU)
            {
                auto b1 = continuation(offset + 1);
                auto b2 = continuation(offset + 2);
                if (b1 && b2 && !(byte == 0xE0U && *b1 < 0xA0U)
                    && !(byte == 0xEDU && *b1 >= 0xA0U))
                {
                    return {static_cast<char32_t>(((byte & 0x0FU) << 12U)
                        | ((*b1 & 0x3FU) << 6U) | (*b2 & 0x3FU)), offset + 3};
                }
            }
            else if (byte >= 0xF0U && byte <= 0xF4U)
            {
                auto b1 = continuation(offset + 1);
                auto b2 = continuation(offset + 2);
                auto b3 = continuation(offset + 3);
                if (b1 && b2 && b3 && !(byte == 0xF0U && *b1 < 0x90U)
                    && !(byte == 0xF4U && *b1 >= 0x90U))
                {
                    return {static_cast<char32_t>(((byte & 0x07U) << 18U)
                        | ((*b1 & 0x3FU) << 12U) | ((*b2 & 0x3FU) << 6U)
                        | (*b3 & 0x3FU)), offset + 4};
                }
            }
            return {byte, offset + 1};
        }

        [[nodiscard]] std::string TrimManagedWhiteSpace(std::string value)
        {
            std::size_t first = std::string::npos;
            std::size_t lastEnd = 0;
            for (std::size_t offset = 0; offset < value.size();)
            {
                const std::size_t start = offset;
                auto [codePoint, next] = DecodeUtf8At(value, offset);
                offset = next;
                if (!IsManagedWhiteSpace(codePoint))
                {
                    if (first == std::string::npos)
                    {
                        first = start;
                    }
                    lastEnd = next;
                }
            }
            if (first == std::string::npos)
            {
                return {};
            }
            return value.substr(first, lastEnd - first);
        }

        [[nodiscard]] std::locale CurrentUserLocale()
        {
            try
            {
                return std::locale("");
            }
            catch (const std::runtime_error&)
            {
                return std::locale::classic();
            }
        }

        void AppendUtf8(std::string& result, char32_t codePoint)
        {
            if (codePoint <= 0x7FU)
            {
                result.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FFU)
            {
                result.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
            else if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
            else if (codePoint <= 0x10FFFFU)
            {
                result.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
        }

        [[nodiscard]] std::string WideToUtf8(std::wstring_view value)
        {
            std::string result;
            result.reserve(value.size());
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                char32_t codePoint = static_cast<char32_t>(value[i]);
                if constexpr (sizeof(wchar_t) == 2)
                {
                    if (codePoint >= 0xD800U && codePoint <= 0xDBFFU && i + 1 < value.size())
                    {
                        const char32_t low = static_cast<char32_t>(value[i + 1]);
                        if (low >= 0xDC00U && low <= 0xDFFFU)
                        {
                            codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U) + (low - 0xDC00U);
                            ++i;
                        }
                    }
                }
                AppendUtf8(result, codePoint);
            }
            return result;
        }

        [[nodiscard]] bool EqualsAsciiIgnoreCase(std::string_view left, std::string_view right) noexcept
        {
            if (left.size() != right.size())
            {
                return false;
            }
            for (std::size_t i = 0; i < left.size(); ++i)
            {
                const unsigned char a = static_cast<unsigned char>(left[i]);
                const unsigned char b = static_cast<unsigned char>(right[i]);
                const unsigned char foldedA = a >= 'A' && a <= 'Z'
                    ? static_cast<unsigned char>(a + ('a' - 'A')) : a;
                const unsigned char foldedB = b >= 'A' && b <= 'Z'
                    ? static_cast<unsigned char>(b + ('a' - 'A')) : b;
                if (foldedA != foldedB)
                {
                    return false;
                }
            }
            return true;
        }

        struct ManagedNumberFormat final
        {
            std::string DecimalSeparator = ".";
            std::string GroupSeparator = ",";
            std::string PositiveSign = "+";
            std::string NegativeSign = "-";
            std::string PositiveInfinitySymbol = "Infinity";
            std::string NegativeInfinitySymbol = "-Infinity";
            std::string NaNSymbol = "NaN";
            int NumberNegativePattern = 1;
            bool AllowHyphenDuringParsing = false;
        };


        [[nodiscard]] bool ManagedAllowsHyphenFallback(std::string_view negativeSign) noexcept
        {
            if (negativeSign.empty())
            {
                return false;
            }
            const auto [codePoint, next] = DecodeUtf8At(negativeSign, 0);
            if (next != negativeSign.size())
            {
                return false;
            }
            switch (codePoint)
            {
            case U'\u2012': // Figure Dash
            case U'\u207B': // Superscript Minus
            case U'\u208B': // Subscript Minus
            case U'\u2212': // Minus Sign
            case U'\u2796': // Heavy Minus Sign
            case U'\uFE63': // Small Hyphen-Minus
            case U'\uFF0D': // Fullwidth Hyphen-Minus
                return true;
            default:
                return false;
            }
        }

    #if defined(_WIN32)
        [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view value)
        {
            std::u16string result;
            result.reserve(value.size());
            for (std::size_t offset = 0; offset < value.size();)
            {
                const auto [codePoint, next] = DecodeUtf8At(value, offset);
                offset = next;
                if (codePoint <= 0xFFFFU)
                {
                    result.push_back(static_cast<char16_t>(codePoint));
                }
                else if (codePoint <= 0x10FFFFU)
                {
                    const char32_t scalar = codePoint - 0x10000U;
                    result.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10U)));
                    result.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
                }
            }
            return result;
        }

    #endif

        [[nodiscard]] std::string Utf16ToUtf8(std::u16string_view value)
        {
            std::string result;
            result.reserve(value.size());
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                char32_t codePoint = static_cast<char32_t>(value[i]);
                if (codePoint >= 0xD800U && codePoint <= 0xDBFFU && i + 1 < value.size())
                {
                    const char32_t low = static_cast<char32_t>(value[i + 1]);
                    if (low >= 0xDC00U && low <= 0xDFFFU)
                    {
                        codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U) + (low - 0xDC00U);
                        ++i;
                    }
                }
                AppendUtf8(result, codePoint);
            }
            return result;
        }

        [[nodiscard]] bool EnvironmentFlagEnabled(const char* name, bool defaultValue = false) noexcept
        {
            const char* raw = std::getenv(name);
            if (raw != nullptr)
            {
                const std::string_view value(raw);
                if (value == "1" || EqualsAsciiIgnoreCase(value, "true"))
                {
                    return true;
                }
                if (value == "0" || EqualsAsciiIgnoreCase(value, "false"))
                {
                    return false;
                }
            }
            return defaultValue;
        }

    #if defined(_WIN32)
        struct WindowsIcuNumberApi final
        {
            using OpenNumberFn = void* (*)(std::int32_t, const char16_t*, std::int32_t,
                const char*, void*, std::int32_t*);
            using CloseNumberFn = void (*)(void*);
            using GetSymbolFn = std::int32_t (*)(const void*, std::int32_t, char16_t*,
                std::int32_t, std::int32_t*);

            HMODULE I18n = nullptr;
            HMODULE Uc = nullptr;
            OpenNumberFn OpenNumber = nullptr;
            CloseNumberFn CloseNumber = nullptr;
            GetSymbolFn GetSymbol = nullptr;

            [[nodiscard]] bool Available() const noexcept
            {
                return OpenNumber != nullptr && CloseNumber != nullptr && GetSymbol != nullptr;
            }
        };

        [[nodiscard]] FARPROC ResolveWindowsIcuSymbol(HMODULE module, std::string_view base)
        {
            if (module == nullptr)
            {
                return nullptr;
            }
            const std::string plain(base);
            if (FARPROC proc = GetProcAddress(module, plain.c_str()))
            {
                return proc;
            }
            for (int major = 100; major >= 40; --major)
            {
                const std::string renamed = plain + "_" + std::to_string(major);
                if (FARPROC proc = GetProcAddress(module, renamed.c_str()))
                {
                    return proc;
                }
            }
            return nullptr;
        }

        [[nodiscard]] HMODULE OpenWindowsIcuLibrary(bool i18n)
        {
            std::vector<std::string> candidates;
            if (const char* appLocal = std::getenv("DOTNET_SYSTEM_GLOBALIZATION_APPLOCALICU");
                appLocal != nullptr && *appLocal != '\0')
            {
                std::string suffix(appLocal);
                if (const std::size_t colon = suffix.find(':'); colon != std::string::npos)
                {
                    suffix.erase(0, colon + 1);
                }
                suffix.erase(std::remove(suffix.begin(), suffix.end(), '.'), suffix.end());
                candidates.push_back(std::string(i18n ? "icuin" : "icuuc") + suffix + ".dll");
            }
            candidates.push_back("icu.dll");
            candidates.push_back(i18n ? "icuin.dll" : "icuuc.dll");
            for (const std::string& candidate : candidates)
            {
                if (HMODULE module = LoadLibraryA(candidate.c_str()))
                {
                    return module;
                }
            }
            return nullptr;
        }

        [[nodiscard]] WindowsIcuNumberApi& CurrentWindowsIcuNumberApi()
        {
            static WindowsIcuNumberApi api = []
            {
                WindowsIcuNumberApi value{};
                value.I18n = OpenWindowsIcuLibrary(true);
                value.Uc = OpenWindowsIcuLibrary(false);
                if (value.I18n == nullptr && value.Uc != nullptr)
                {
                    value.I18n = value.Uc;
                }
                if (value.Uc == nullptr && value.I18n != nullptr)
                {
                    value.Uc = value.I18n;
                }
                value.OpenNumber = reinterpret_cast<WindowsIcuNumberApi::OpenNumberFn>(
                    ResolveWindowsIcuSymbol(value.I18n, "unum_open"));
                value.CloseNumber = reinterpret_cast<WindowsIcuNumberApi::CloseNumberFn>(
                    ResolveWindowsIcuSymbol(value.I18n, "unum_close"));
                value.GetSymbol = reinterpret_cast<WindowsIcuNumberApi::GetSymbolFn>(
                    ResolveWindowsIcuSymbol(value.I18n, "unum_getSymbol"));
                return value;
            }();
            return api;
        }

        [[nodiscard]] std::string WindowsIcuNumberSymbol(WindowsIcuNumberApi& api,
            const void* number, std::int32_t symbol)
        {
            std::array<char16_t, 32> stack{};
            std::int32_t status = 0;
            std::int32_t length = api.GetSymbol(number, symbol, stack.data(),
                static_cast<std::int32_t>(stack.size()), &status);
            if (length < 0)
            {
                return {};
            }
            if (length < static_cast<std::int32_t>(stack.size()) && status <= 0)
            {
                return Utf16ToUtf8(std::u16string_view(stack.data(),
                    static_cast<std::size_t>(length)));
            }
            std::vector<char16_t> buffer(static_cast<std::size_t>(length) + 1);
            status = 0;
            length = api.GetSymbol(number, symbol, buffer.data(),
                static_cast<std::int32_t>(buffer.size()), &status);
            if (length < 0 || status > 0)
            {
                return {};
            }
            return Utf16ToUtf8(std::u16string_view(buffer.data(),
                static_cast<std::size_t>(length)));
        }

        [[nodiscard]] std::string WindowsLocaleString(const wchar_t* localeName, LCTYPE type)
        {
            const int length = GetLocaleInfoEx(localeName, type, nullptr, 0);
            if (length <= 1)
            {
                return {};
            }
            std::wstring value(static_cast<std::size_t>(length), L'\0');
            if (GetLocaleInfoEx(localeName, type, value.data(), length) == 0)
            {
                return {};
            }
            value.resize(static_cast<std::size_t>(length - 1));
            return WideToUtf8(value);
        }

        [[nodiscard]] int WindowsLocaleNumberNegativePattern(const wchar_t* localeName)
        {
            const std::string value = WindowsLocaleString(localeName, LOCALE_INEGNUMBER);
            int pattern = 1;
            const char* first = value.data();
            const char* last = first + value.size();
            const auto parsed = std::from_chars(first, last, pattern);
            if (parsed.ec == std::errc{} && parsed.ptr == last && pattern >= 0 && pattern <= 4)
            {
                return pattern;
            }
            return 1;
        }

        [[nodiscard]] bool TryLoadWindowsIcuManagedNumberFormat(const wchar_t* localeName,
            ManagedNumberFormat& result)
        {
            WindowsIcuNumberApi& api = CurrentWindowsIcuNumberApi();
            if (!api.Available())
            {
                return false;
            }
            const std::string locale = WideToUtf8(localeName);
            std::int32_t status = 0;
            void* number = api.OpenNumber(1, nullptr, 0, locale.c_str(), nullptr, &status);
            if (number == nullptr || status > 0)
            {
                if (number != nullptr)
                {
                    api.CloseNumber(number);
                }
                return false;
            }

            const std::string decimal = WindowsIcuNumberSymbol(api, number, 0);
            const std::string group = WindowsIcuNumberSymbol(api, number, 1);
            const std::string negative = WindowsIcuNumberSymbol(api, number, 6);
            const std::string positive = WindowsIcuNumberSymbol(api, number, 7);
            const std::string infinity = WindowsIcuNumberSymbol(api, number, 14);
            const std::string nan = WindowsIcuNumberSymbol(api, number, 15);
            api.CloseNumber(number);

            if (!decimal.empty())
            {
                result.DecimalSeparator = decimal;
            }
            result.GroupSeparator = group;
            result.PositiveSign = positive.empty() ? "+" : positive;
            result.NegativeSign = negative;
            if (!infinity.empty() && !EqualsAsciiIgnoreCase(infinity, "inf"))
            {
                result.PositiveInfinitySymbol = infinity;
            }
            if (!nan.empty())
            {
                result.NaNSymbol = nan;
            }

            // CultureData.Icu builds NegativeInfinitySymbol from the culture's ICU
            // negative sign and infinity symbol before GetNFIValues applies any
            // Windows user overrides to the ordinary number signs/separators.
            result.NegativeInfinitySymbol = negative + result.PositiveInfinitySymbol;

            // CurrentCulture uses user overrides. On Windows, .NET obtains the
            // NumberFormatInfo parsing signs and number separators from NLS even
            // when ICU supplies the culture data used for the special symbols.
            const std::string overrideDecimal = WindowsLocaleString(localeName, LOCALE_SDECIMAL);
            const std::string overrideGroup = WindowsLocaleString(localeName, LOCALE_STHOUSAND);
            const std::string overridePositive = WindowsLocaleString(localeName, LOCALE_SPOSITIVESIGN);
            const std::string overrideNegative = WindowsLocaleString(localeName, LOCALE_SNEGATIVESIGN);
            if (!overrideDecimal.empty())
            {
                result.DecimalSeparator = overrideDecimal;
            }
            result.GroupSeparator = overrideGroup;
            result.PositiveSign = overridePositive.empty() ? "+" : overridePositive;
            result.NegativeSign = overrideNegative;
            result.NumberNegativePattern = WindowsLocaleNumberNegativePattern(localeName);
            result.AllowHyphenDuringParsing = ManagedAllowsHyphenFallback(result.NegativeSign);
            return true;
        }

        [[nodiscard]] bool TryLoadWindowsNlsManagedNumberFormat(const wchar_t* localeName,
            ManagedNumberFormat& result)
        {
            const std::string decimal = WindowsLocaleString(localeName, LOCALE_SDECIMAL);
            const std::string group = WindowsLocaleString(localeName, LOCALE_STHOUSAND);
            const std::string positive = WindowsLocaleString(localeName, LOCALE_SPOSITIVESIGN);
            const std::string negative = WindowsLocaleString(localeName, LOCALE_SNEGATIVESIGN);
            const std::string nan = WindowsLocaleString(localeName, LOCALE_SNAN);
            const std::string positiveInfinity = WindowsLocaleString(localeName, LOCALE_SPOSINFINITY);
            const std::string negativeInfinity = WindowsLocaleString(localeName, LOCALE_SNEGINFINITY);

            if (!decimal.empty())
            {
                result.DecimalSeparator = decimal;
            }
            result.GroupSeparator = group;
            result.PositiveSign = positive.empty() ? "+" : positive;
            result.NegativeSign = negative;
            if (!nan.empty())
            {
                result.NaNSymbol = nan;
            }
            if (!positiveInfinity.empty())
            {
                result.PositiveInfinitySymbol = positiveInfinity;
            }
            if (!negativeInfinity.empty())
            {
                result.NegativeInfinitySymbol = negativeInfinity;
            }
            else
            {
                result.NegativeInfinitySymbol = result.NegativeSign + result.PositiveInfinitySymbol;
            }
            result.NumberNegativePattern = WindowsLocaleNumberNegativePattern(localeName);
            result.AllowHyphenDuringParsing = ManagedAllowsHyphenFallback(result.NegativeSign);
            return true;
        }

        [[nodiscard]] bool TryLoadPlatformManagedNumberFormat(ManagedNumberFormat& result)
        {
            if (EnvironmentFlagEnabled("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT"))
            {
                return true;
            }

            wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
            if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0)
            {
                return false;
            }

            // .NET 9 uses ICU on supported Windows systems unless UseNls is
            // explicitly enabled, then falls back to NLS when ICU is unavailable.
            if (!EnvironmentFlagEnabled("DOTNET_SYSTEM_GLOBALIZATION_USENLS")
                && TryLoadWindowsIcuManagedNumberFormat(localeName, result))
            {
                return true;
            }
            return TryLoadWindowsNlsManagedNumberFormat(localeName, result);
        }

    #else
        struct IcuNumberApi final
        {
            using OpenNumberFn = void* (*)(std::int32_t, const char16_t*, std::int32_t,
                const char*, void*, std::int32_t*);
            using CloseNumberFn = void (*)(void*);
            using GetSymbolFn = std::int32_t (*)(const void*, std::int32_t, char16_t*,
                std::int32_t, std::int32_t*);
            using ToPatternFn = std::int32_t (*)(const void*, std::int8_t, char16_t*,
                std::int32_t, std::int32_t*);
            using GetDefaultLocaleFn = const char* (*)();
            using FoldCaseFn = std::int32_t (*)(std::int32_t, std::uint32_t);

            void* I18n = nullptr;
            void* Uc = nullptr;
            OpenNumberFn OpenNumber = nullptr;
            CloseNumberFn CloseNumber = nullptr;
            GetSymbolFn GetSymbol = nullptr;
            ToPatternFn ToPattern = nullptr;
            GetDefaultLocaleFn GetDefaultLocale = nullptr;
            FoldCaseFn FoldCase = nullptr;

            [[nodiscard]] bool Available() const noexcept
            {
                return OpenNumber != nullptr && CloseNumber != nullptr && GetSymbol != nullptr;
            }
        };

        [[nodiscard]] void* OpenIcuLibrary(std::string_view stem)
        {
            std::vector<std::string> candidates;
            candidates.emplace_back(std::string(stem) + ".so");
            for (int major = 100; major >= 40; --major)
            {
                candidates.emplace_back(std::string(stem) + ".so." + std::to_string(major));
            }
    #if defined(__APPLE__)
            candidates.emplace_back("/usr/lib/libicucore.A.dylib");
            candidates.emplace_back("/usr/lib/libicucore.dylib");
            candidates.emplace_back(std::string(stem) + ".dylib");
    #endif
            for (const std::string& candidate : candidates)
            {
                if (void* handle = dlopen(candidate.c_str(), RTLD_LAZY | RTLD_LOCAL))
                {
                    return handle;
                }
            }
            return nullptr;
        }

        [[nodiscard]] void* ResolveIcuSymbol(void* handle, std::string_view base)
        {
            if (handle == nullptr)
            {
                return nullptr;
            }
            const std::string plain(base);
            if (void* symbol = dlsym(handle, plain.c_str()))
            {
                return symbol;
            }
            for (int major = 100; major >= 40; --major)
            {
                const std::string renamed = plain + "_" + std::to_string(major);
                if (void* symbol = dlsym(handle, renamed.c_str()))
                {
                    return symbol;
                }
            }
            return nullptr;
        }

        [[nodiscard]] IcuNumberApi& CurrentIcuNumberApi()
        {
            static IcuNumberApi api = []
            {
                IcuNumberApi value{};
                value.I18n = OpenIcuLibrary("libicui18n");
                value.Uc = OpenIcuLibrary("libicuuc");
    #if defined(__APPLE__)
                if (value.I18n == nullptr)
                {
                    value.I18n = OpenIcuLibrary("libicucore");
                }
                if (value.Uc == nullptr)
                {
                    value.Uc = value.I18n;
                }
    #endif
                value.OpenNumber = reinterpret_cast<IcuNumberApi::OpenNumberFn>(
                    ResolveIcuSymbol(value.I18n, "unum_open"));
                value.CloseNumber = reinterpret_cast<IcuNumberApi::CloseNumberFn>(
                    ResolveIcuSymbol(value.I18n, "unum_close"));
                value.GetSymbol = reinterpret_cast<IcuNumberApi::GetSymbolFn>(
                    ResolveIcuSymbol(value.I18n, "unum_getSymbol"));
                value.ToPattern = reinterpret_cast<IcuNumberApi::ToPatternFn>(
                    ResolveIcuSymbol(value.I18n, "unum_toPattern"));
                value.GetDefaultLocale = reinterpret_cast<IcuNumberApi::GetDefaultLocaleFn>(
                    ResolveIcuSymbol(value.Uc, "uloc_getDefault"));
                value.FoldCase = reinterpret_cast<IcuNumberApi::FoldCaseFn>(
                    ResolveIcuSymbol(value.Uc, "u_foldCase"));
                return value;
            }();
            return api;
        }

        [[nodiscard]] std::string IcuNumberSymbol(IcuNumberApi& api, const void* number,
            std::int32_t symbol)
        {
            std::array<char16_t, 32> stack{};
            std::int32_t status = 0;
            std::int32_t length = api.GetSymbol(number, symbol, stack.data(),
                static_cast<std::int32_t>(stack.size()), &status);
            if (length < 0)
            {
                return {};
            }
            if (length < static_cast<std::int32_t>(stack.size()) && status <= 0)
            {
                return Utf16ToUtf8(std::u16string_view(stack.data(),
                    static_cast<std::size_t>(length)));
            }

            std::vector<char16_t> buffer(static_cast<std::size_t>(length) + 1);
            status = 0;
            length = api.GetSymbol(number, symbol, buffer.data(),
                static_cast<std::int32_t>(buffer.size()), &status);
            if (length < 0 || status > 0)
            {
                return {};
            }
            return Utf16ToUtf8(std::u16string_view(buffer.data(),
                static_cast<std::size_t>(length)));
        }

        [[nodiscard]] int IcuNumberNegativePattern(IcuNumberApi& api, const void* number)
        {
            if (api.ToPattern == nullptr)
            {
                return 1;
            }

            std::int32_t status = 0;
            const std::int32_t required = api.ToPattern(number, 0, nullptr, 0, &status);
            if (required < 0)
            {
                return 1;
            }

            std::vector<char16_t> pattern(static_cast<std::size_t>(required) + 1, u'\0');
            status = 0;
            const std::int32_t length = api.ToPattern(number, 0, pattern.data(),
                static_cast<std::int32_t>(pattern.size()), &status);
            if (length < 0 || status > 0)
            {
                return 1;
            }

            std::size_t start = 0;
            std::size_t end = static_cast<std::size_t>(length);
            for (std::size_t i = 0; i < end; ++i)
            {
                if (pattern[i] == u';')
                {
                    start = i + 1;
                }
            }

            bool minusAdded = false;
            for (std::size_t i = start; i < end; ++i)
            {
                if (pattern[i] == u'-' || pattern[i] == u'(' || pattern[i] == u')')
                {
                    minusAdded = true;
                    break;
                }
            }

            std::string normalized;
            if (!minusAdded)
            {
                normalized.push_back('-');
            }
            bool digitAdded = false;
            bool spaceAdded = false;
            for (std::size_t i = start; i < end; ++i)
            {
                switch (pattern[i])
                {
                case u'#':
                case u'0':
                    if (!digitAdded)
                    {
                        digitAdded = true;
                        normalized.push_back('n');
                    }
                    break;
                case u' ':
                case u'\u00A0':
                    if (!spaceAdded)
                    {
                        spaceAdded = true;
                        normalized.push_back(' ');
                    }
                    break;
                case u'-':
                case u'(':
                case u')':
                    normalized.push_back(static_cast<char>(pattern[i]));
                    break;
                default:
                    break;
                }
            }

            static constexpr std::array<std::string_view, 5> patterns{
                "(n)", "-n", "- n", "n-", "n -"
            };
            for (std::size_t i = 0; i < patterns.size(); ++i)
            {
                if (normalized == patterns[i])
                {
                    return static_cast<int>(i);
                }
            }
            return 1;
        }

        [[nodiscard]] bool TryLoadPlatformManagedNumberFormat(ManagedNumberFormat& result)
        {
            if (EnvironmentFlagEnabled("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT"))
            {
                return true;
            }

            IcuNumberApi& api = CurrentIcuNumberApi();
            if (!api.Available())
            {
                return false;
            }

            const char* locale = api.GetDefaultLocale != nullptr ? api.GetDefaultLocale() : nullptr;
            std::int32_t status = 0;
            void* number = api.OpenNumber(1, nullptr, 0, locale, nullptr, &status); // UNUM_DECIMAL
            if (number == nullptr || status > 0)
            {
                if (number != nullptr)
                {
                    api.CloseNumber(number);
                }
                return false;
            }

            const std::string decimal = IcuNumberSymbol(api, number, 0); // decimal separator
            const std::string group = IcuNumberSymbol(api, number, 1); // grouping separator
            const std::string negative = IcuNumberSymbol(api, number, 6); // minus sign
            const std::string positive = IcuNumberSymbol(api, number, 7); // plus sign
            const std::string infinity = IcuNumberSymbol(api, number, 14); // infinity
            const std::string nan = IcuNumberSymbol(api, number, 15); // NaN
            const int numberNegativePattern = IcuNumberNegativePattern(api, number);
            api.CloseNumber(number);

            if (!decimal.empty())
            {
                result.DecimalSeparator = decimal;
            }
            result.GroupSeparator = group;
            if (!positive.empty())
            {
                result.PositiveSign = positive;
            }
            if (!negative.empty())
            {
                result.NegativeSign = negative;
            }
            // ICU's POSIX locale may expose the C spelling "INF". .NET maps the
            // POSIX/C environment to its managed invariant number format instead;
            // do not re-introduce the C-only token that Single.TryParse rejects.
            if (!infinity.empty() && !EqualsAsciiIgnoreCase(infinity, "inf"))
            {
                result.PositiveInfinitySymbol = infinity;
            }
            if (!nan.empty())
            {
                result.NaNSymbol = nan;
            }
            // .NET's ICU globalization path defines negative infinity by prefixing
            // the positive infinity symbol with NumberFormatInfo.NegativeSign.
            result.NegativeInfinitySymbol = result.NegativeSign + result.PositiveInfinitySymbol;
            result.NumberNegativePattern = numberNegativePattern;
            result.AllowHyphenDuringParsing = ManagedAllowsHyphenFallback(result.NegativeSign);
            return true;
        }

        [[nodiscard]] char32_t FoldManagedOrdinalCodePoint(char32_t value) noexcept
        {
            if (value <= 0x7FU)
            {
                const unsigned char ch = static_cast<unsigned char>(value);
                return ch >= 'A' && ch <= 'Z' ? static_cast<char32_t>(ch + ('a' - 'A')) : value;
            }
            IcuNumberApi& api = CurrentIcuNumberApi();
            if (api.FoldCase == nullptr)
            {
                return value;
            }
            return static_cast<char32_t>(api.FoldCase(static_cast<std::int32_t>(value), 0));
        }
    #endif

        [[nodiscard]] std::string FormatLocaleSpecial(const std::locale& locale, float value)
        {
            std::wostringstream output;
            output.imbue(locale);
            output << value;
            return WideToUtf8(output.str());
        }

        [[nodiscard]] bool IsCNaNSymbol(std::string_view value) noexcept
        {
            return EqualsAsciiIgnoreCase(value, "nan") || EqualsAsciiIgnoreCase(value, "+nan")
                || EqualsAsciiIgnoreCase(value, "-nan");
        }

        [[nodiscard]] bool IsCInfinitySymbol(std::string_view value) noexcept
        {
            return EqualsAsciiIgnoreCase(value, "inf") || EqualsAsciiIgnoreCase(value, "+inf")
                || EqualsAsciiIgnoreCase(value, "-inf") || EqualsAsciiIgnoreCase(value, "infinity")
                || EqualsAsciiIgnoreCase(value, "+infinity") || EqualsAsciiIgnoreCase(value, "-infinity");
        }

        void ApplyStandardLocaleFallback(ManagedNumberFormat& result)
        {
            const std::locale locale = CurrentUserLocale();
            try
            {
                const auto& punctuation = std::use_facet<std::numpunct<wchar_t>>(locale);
                result.DecimalSeparator = WideToUtf8(std::wstring(1, punctuation.decimal_point()));
                result.GroupSeparator = WideToUtf8(std::wstring(1, punctuation.thousands_sep()));
            }
            catch (const std::bad_cast&)
            {
                const auto& punctuation = std::use_facet<std::numpunct<char>>(locale);
                result.DecimalSeparator.assign(1, punctuation.decimal_point());
                result.GroupSeparator.assign(1, punctuation.thousands_sep());
            }

            try
            {
                const auto& money = std::use_facet<std::moneypunct<wchar_t, false>>(locale);
                const std::string positiveSign = WideToUtf8(money.positive_sign());
                const std::string negativeSign = WideToUtf8(money.negative_sign());
                if (!positiveSign.empty())
                {
                    result.PositiveSign = positiveSign;
                }
                if (!negativeSign.empty())
                {
                    result.NegativeSign = negativeSign;
                }
            }
            catch (const std::bad_cast&)
            {
            }

            try
            {
                const std::string nan = FormatLocaleSpecial(locale,
                    std::numeric_limits<float>::quiet_NaN());
                const std::string positiveInfinity = FormatLocaleSpecial(locale,
                    std::numeric_limits<float>::infinity());
                const std::string negativeInfinity = FormatLocaleSpecial(locale,
                    -std::numeric_limits<float>::infinity());
                if (!nan.empty() && !IsCNaNSymbol(nan))
                {
                    result.NaNSymbol = nan;
                }
                if (!positiveInfinity.empty() && !IsCInfinitySymbol(positiveInfinity))
                {
                    result.PositiveInfinitySymbol = positiveInfinity;
                }
                if (!negativeInfinity.empty() && !IsCInfinitySymbol(negativeInfinity))
                {
                    result.NegativeInfinitySymbol = negativeInfinity;
                }
                else
                {
                    result.NegativeInfinitySymbol = result.NegativeSign + result.PositiveInfinitySymbol;
                }
            }
            catch (const std::runtime_error&)
            {
                result.NegativeInfinitySymbol = result.NegativeSign + result.PositiveInfinitySymbol;
            }
            result.AllowHyphenDuringParsing = ManagedAllowsHyphenFallback(result.NegativeSign);
        }

        [[nodiscard]] ManagedNumberFormat CurrentManagedNumberFormat()
        {
            ManagedNumberFormat result{};
            if (!TryLoadPlatformManagedNumberFormat(result))
            {
                ApplyStandardLocaleFallback(result);
            }
            return result;
        }

        [[nodiscard]] bool IsManagedSpaceReplacingChar(char32_t value) noexcept
        {
            return value == U'\u00A0' || value == U'\u202F';
        }

        [[nodiscard]] std::size_t MatchManagedNumberToken(std::string_view text,
            std::size_t index, std::string_view token) noexcept
        {
            if (token.empty() || index > text.size())
            {
                return 0;
            }

            std::size_t textIndex = index;
            std::size_t tokenIndex = 0;
            while (tokenIndex < token.size())
            {
                if (textIndex >= text.size())
                {
                    return 0;
                }
                const auto [expected, nextToken] = DecodeUtf8At(token, tokenIndex);
                const auto [actual, nextText] = DecodeUtf8At(text, textIndex);
                if (actual != expected
                    && !(actual == U'\u0020' && IsManagedSpaceReplacingChar(expected)))
                {
                    return 0;
                }
                tokenIndex = nextToken;
                textIndex = nextText;
            }
            return textIndex - index;
        }

        [[nodiscard]] std::size_t MatchManagedOrdinalIgnoreCaseToken(std::string_view text,
            std::size_t index, std::string_view token) noexcept
        {
            if (token.empty() || index > text.size())
            {
                return 0;
            }

    #if defined(_WIN32)
            const std::u16string expected = Utf8ToUtf16(token);
            std::u16string actual;
            actual.reserve(expected.size());
            std::size_t textIndex = index;
            while (textIndex < text.size() && actual.size() < expected.size())
            {
                const auto [codePoint, nextText] = DecodeUtf8At(text, textIndex);
                if (codePoint <= 0xFFFFU)
                {
                    actual.push_back(static_cast<char16_t>(codePoint));
                }
                else if (codePoint <= 0x10FFFFU)
                {
                    const char32_t scalar = codePoint - 0x10000U;
                    actual.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10U)));
                    actual.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
                }
                textIndex = nextText;
            }
            if (actual.size() != expected.size())
            {
                return 0;
            }
            const int comparison = CompareStringOrdinal(
                reinterpret_cast<LPCWCH>(actual.data()), static_cast<int>(actual.size()),
                reinterpret_cast<LPCWCH>(expected.data()), static_cast<int>(expected.size()), TRUE);
            return comparison == CSTR_EQUAL ? textIndex - index : 0;
    #else
            std::size_t textIndex = index;
            std::size_t tokenIndex = 0;
            while (tokenIndex < token.size())
            {
                if (textIndex >= text.size())
                {
                    return 0;
                }
                const auto [expected, nextToken] = DecodeUtf8At(token, tokenIndex);
                const auto [actual, nextText] = DecodeUtf8At(text, textIndex);
                if (FoldManagedOrdinalCodePoint(actual) != FoldManagedOrdinalCodePoint(expected))
                {
                    return 0;
                }
                tokenIndex = nextToken;
                textIndex = nextText;
            }
            return textIndex - index;
    #endif
        }

        [[nodiscard]] bool EqualsManagedOrdinalIgnoreCase(std::string_view left,
            std::string_view right) noexcept
        {
            const std::size_t matched = MatchManagedOrdinalIgnoreCaseToken(left, 0, right);
            return matched != 0 && matched == left.size();
        }

        [[nodiscard]] std::size_t MatchNegativeSign(std::string_view text, std::size_t index,
            const ManagedNumberFormat& format) noexcept
        {
            if (const std::size_t matched = MatchManagedNumberToken(text, index, format.NegativeSign);
                matched != 0)
            {
                return matched;
            }
            if (format.AllowHyphenDuringParsing && index < text.size() && text[index] == '-')
            {
                return 1;
            }
            return 0;
        }

        [[nodiscard]] bool TryParseManagedSingleSpecial(std::string_view text,
            const ManagedNumberFormat& format, float& value) noexcept
        {
            if (EqualsManagedOrdinalIgnoreCase(text, format.PositiveInfinitySymbol))
            {
                value = std::numeric_limits<float>::infinity();
                return true;
            }
            if (EqualsManagedOrdinalIgnoreCase(text, format.NegativeInfinitySymbol))
            {
                value = -std::numeric_limits<float>::infinity();
                return true;
            }
            if (EqualsManagedOrdinalIgnoreCase(text, format.NaNSymbol))
            {
                value = std::numeric_limits<float>::quiet_NaN();
                return true;
            }

            if (const std::size_t positiveLength = MatchManagedOrdinalIgnoreCaseToken(
                text, 0, format.PositiveSign); positiveLength != 0)
            {
                const std::string_view remainder = text.substr(positiveLength);
                if (EqualsManagedOrdinalIgnoreCase(remainder, format.PositiveInfinitySymbol))
                {
                    value = std::numeric_limits<float>::infinity();
                    return true;
                }
                if (EqualsManagedOrdinalIgnoreCase(remainder, format.NaNSymbol))
                {
                    value = std::numeric_limits<float>::quiet_NaN();
                    return true;
                }
            }

            // Number.TryParseFloat uses an OrdinalIgnoreCase prefix comparison for
            // the special-value negative sign; unlike ordinary numeric parsing,
            // this path does not apply MatchChars' NBSP/NNBSP-to-ASCII-space rule.
            if (const std::size_t negativeLength = MatchManagedOrdinalIgnoreCaseToken(
                text, 0, format.NegativeSign); negativeLength != 0)
            {
                if (EqualsManagedOrdinalIgnoreCase(text.substr(negativeLength), format.NaNSymbol))
                {
                    value = std::numeric_limits<float>::quiet_NaN();
                    return true;
                }
                if (format.AllowHyphenDuringParsing && !text.empty() && text.front() == '-'
                    && EqualsManagedOrdinalIgnoreCase(text.substr(1), format.NaNSymbol))
                {
                    value = std::numeric_limits<float>::quiet_NaN();
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool IsManagedNumberWhiteSpace(unsigned char value) noexcept;

        [[nodiscard]] bool NormalizeManagedSingleNumber(std::string_view text,
            const ManagedNumberFormat& format, std::string& normalized)
        {
            normalized.clear();
            normalized.reserve(text.size());

            std::size_t index = 0;
            bool sawSign = false;
            if (const std::size_t positiveSignLength = MatchManagedNumberToken(
                text, index, format.PositiveSign); positiveSignLength != 0)
            {
                sawSign = true;
                normalized.push_back('+');
                index += positiveSignLength;
            }
            else if (const std::size_t negativeSignLength = MatchNegativeSign(text, index, format);
                negativeSignLength != 0)
            {
                sawSign = true;
                normalized.push_back('-');
                index += negativeSignLength;
            }
            if (sawSign && format.NumberNegativePattern == 2)
            {
                while (index < text.size()
                    && IsManagedNumberWhiteSpace(static_cast<unsigned char>(text[index])))
                {
                    ++index;
                }
            }

            bool sawDigit = false;
            bool sawDecimal = false;
            while (index < text.size())
            {
                const char ch = text[index];
                if (ch >= '0' && ch <= '9')
                {
                    sawDigit = true;
                    normalized.push_back(ch);
                    ++index;
                    continue;
                }
                if (!sawDecimal)
                {
                    const std::size_t decimalLength = MatchManagedNumberToken(
                        text, index, format.DecimalSeparator);
                    if (decimalLength != 0)
                    {
                        sawDecimal = true;
                        normalized.push_back('.');
                        index += decimalLength;
                        continue;
                    }
                }
                if (!sawDecimal && sawDigit && format.GroupSeparator != format.DecimalSeparator)
                {
                    const std::size_t groupLength = MatchManagedNumberToken(
                        text, index, format.GroupSeparator);
                    if (groupLength != 0)
                    {
                        // NumberStyles.AllowThousands does not validate group sizes;
                        // once an integral digit has been seen, repeated/trailing group
                        // separators are accepted by the managed parser as well.
                        index += groupLength;
                        continue;
                    }
                }
                break;
            }

            if (!sawDigit)
            {
                return false;
            }

            if (index < text.size() && (text[index] == 'e' || text[index] == 'E'))
            {
                normalized.push_back(text[index]);
                ++index;
                if (const std::size_t positiveSignLength = MatchManagedNumberToken(
                    text, index, format.PositiveSign); positiveSignLength != 0)
                {
                    normalized.push_back('+');
                    index += positiveSignLength;
                }
                else if (const std::size_t negativeSignLength = MatchNegativeSign(text, index, format);
                    negativeSignLength != 0)
                {
                    normalized.push_back('-');
                    index += negativeSignLength;
                }
                const std::size_t exponentStart = index;
                while (index < text.size() && text[index] >= '0' && text[index] <= '9')
                {
                    normalized.push_back(text[index]);
                    ++index;
                }
                if (index == exponentStart)
                {
                    return false;
                }
            }

            return index == text.size();
        }

        [[nodiscard]] bool IsManagedNumberWhiteSpace(unsigned char value) noexcept
        {
            return value == 0x20U || (value >= 0x09U && value <= 0x0DU);
        }

        [[nodiscard]] std::string TrimManagedNumberWhiteSpace(std::string value)
        {
            std::size_t first = 0;
            while (first < value.size()
                && IsManagedNumberWhiteSpace(static_cast<unsigned char>(value[first])))
            {
                ++first;
            }
            std::size_t last = value.size();
            while (last > first
                && IsManagedNumberWhiteSpace(static_cast<unsigned char>(value[last - 1])))
            {
                --last;
            }
            return value.substr(first, last - first);
        }

        void RemoveManagedTrailingNulls(std::string& value)
        {
            while (!value.empty() && value.back() == '\0')
            {
                value.pop_back();
            }
        }

        [[nodiscard]] bool TryParseSingle(std::string text, const ManagedNumberFormat& format,
            float& value)
        {
            std::string special = TrimManagedWhiteSpace(text);
            if (!special.empty() && TryParseManagedSingleSpecial(special, format, value))
            {
                return true;
            }

            RemoveManagedTrailingNulls(text);
            text = TrimManagedNumberWhiteSpace(std::move(text));
            if (text.empty())
            {
                value = 0.0F;
                return false;
            }

            std::string normalized;
            if (!NormalizeManagedSingleNumber(text, format, normalized))
            {
                value = 0.0F;
                return false;
            }

            // The lexical gate above is the managed Float|AllowThousands grammar.
            // from_chars supplies locale-independent correctly-rounded binary32
            // conversion without re-introducing strtof's C-only token grammar.
            // It reports values that round beyond binary32 (including underflow to
            // zero) as out_of_range, so use binary64 only to distinguish the two
            // managed outcomes: infinity on overflow and signed zero on underflow.
            const char* first = normalized.data();
            const char* last = first + normalized.size();
            if (first != last && *first == '+')
            {
                ++first; // floating from_chars intentionally has no leading '+'
            }
            float parsed = 0.0F;
            const auto result = std::from_chars(first, last, parsed, std::chars_format::general);
            if (result.ec == std::errc{} && result.ptr == last)
            {
                value = parsed;
                return true;
            }
            if (result.ec != std::errc::result_out_of_range || result.ptr != last)
            {
                value = 0.0F;
                return false;
            }

            double wide = 0.0;
            const auto wideResult = std::from_chars(first, last, wide, std::chars_format::general);
            const bool negative = !normalized.empty() && normalized.front() == '-';
            if (wideResult.ec == std::errc{} && wideResult.ptr == last)
            {
                if (std::fabs(wide) > static_cast<double>(std::numeric_limits<float>::max()))
                {
                    value = negative ? -std::numeric_limits<float>::infinity()
                        : std::numeric_limits<float>::infinity();
                }
                else
                {
                    value = negative ? -0.0F : 0.0F;
                }
                return true;
            }
            if (wideResult.ec != std::errc::result_out_of_range || wideResult.ptr != last)
            {
                value = 0.0F;
                return false;
            }

            // A syntactically valid decimal outside binary64 is unambiguously on
            // one side of zero. Recover that direction from its decimal scale.
            const std::size_t exponentPos = normalized.find_first_of("eE");
            const std::string_view mantissa(normalized.data(),
                exponentPos == std::string::npos ? normalized.size() : exponentPos);
            std::int64_t explicitExponent = 0;
            if (exponentPos != std::string::npos)
            {
                std::size_t exponentIndex = exponentPos + 1;
                bool exponentNegative = false;
                if (exponentIndex < normalized.size()
                    && (normalized[exponentIndex] == '+' || normalized[exponentIndex] == '-'))
                {
                    exponentNegative = normalized[exponentIndex] == '-';
                    ++exponentIndex;
                }
                constexpr std::int64_t exponentLimit = std::numeric_limits<std::int64_t>::max() / 4;
                while (exponentIndex < normalized.size())
                {
                    const std::int32_t digit = normalized[exponentIndex] - '0';
                    if (explicitExponent <= (exponentLimit - digit) / 10)
                    {
                        explicitExponent = explicitExponent * 10 + digit;
                    }
                    else
                    {
                        explicitExponent = exponentLimit;
                    }
                    ++exponentIndex;
                }
                if (exponentNegative)
                {
                    explicitExponent = -explicitExponent;
                }
            }

            const std::size_t mantissaStart = !mantissa.empty()
                && (mantissa.front() == '+' || mantissa.front() == '-') ? 1 : 0;
            const std::size_t decimalPos = mantissa.find('.');
            const std::size_t digitsBeforeDecimal = decimalPos == std::string::npos
                ? mantissa.size() - mantissaStart : decimalPos - mantissaStart;
            std::size_t digitIndex = 0;
            std::size_t firstNonZero = std::string::npos;
            for (std::size_t i = mantissaStart; i < mantissa.size(); ++i)
            {
                if (mantissa[i] == '.')
                {
                    continue;
                }
                if (firstNonZero == std::string::npos && mantissa[i] != '0')
                {
                    firstNonZero = digitIndex;
                }
                ++digitIndex;
            }
            if (firstNonZero == std::string::npos)
            {
                value = negative ? -0.0F : 0.0F;
                return true;
            }
            const std::int64_t baseScale = static_cast<std::int64_t>(digitsBeforeDecimal)
                - static_cast<std::int64_t>(firstNonZero) - 1;
            std::int64_t scale = 0;
            if (explicitExponent > 0
                && baseScale > std::numeric_limits<std::int64_t>::max() - explicitExponent)
            {
                scale = std::numeric_limits<std::int64_t>::max();
            }
            else if (explicitExponent < 0
                && baseScale < std::numeric_limits<std::int64_t>::min() - explicitExponent)
            {
                scale = std::numeric_limits<std::int64_t>::min();
            }
            else
            {
                scale = baseScale + explicitExponent;
            }
            value = scale >= 0
                ? (negative ? -std::numeric_limits<float>::infinity()
                    : std::numeric_limits<float>::infinity())
                : (negative ? -0.0F : 0.0F);
            return true;
        }

        [[nodiscard]] bool TryParseSingleCurrentCulture(std::string text, float& value)
        {
            return TryParseSingle(std::move(text), CurrentManagedNumberFormat(), value);
        }

        [[nodiscard]] bool TryParseManagedHexInt32(std::string text, std::int32_t& value)
        {
            RemoveManagedTrailingNulls(text);
            text = TrimManagedNumberWhiteSpace(std::move(text));
            if (text.empty())
            {
                value = 0;
                return false;
            }

            const std::size_t firstNonZero = text.find_first_not_of('0');
            if (firstNonZero == std::string::npos)
            {
                value = 0;
                return true;
            }
            const std::size_t significantDigits = text.size() - firstNonZero;
            if (significantDigits > 8)
            {
                value = 0;
                return false;
            }

            const char* first = text.data() + firstNonZero;
            const char* last = text.data() + text.size();
            std::uint32_t bits = 0;
            const auto result = std::from_chars(first, last, bits, 16);
            if (result.ec != std::errc{} || result.ptr != last)
            {
                value = 0;
                return false;
            }
            value = std::bit_cast<std::int32_t>(bits);
            return true;
        }
    }

    bool StringIsNullOrWhiteSpace(std::string_view value) noexcept
    {
        for (const char item : value)
        {
            if (!IsManagedWhiteSpace(static_cast<char32_t>(
                    static_cast<unsigned char>(item))))
            {
                return false;
            }
        }
        return true;
    }

    bool StringEqualsOrdinalIgnoreCase(
        std::string_view left, std::string_view right) noexcept
    {
        // OrdinalIgnoreCase folds only the invariant ASCII range plus the
        // simple case mapping; the callers here compare ASCII tokens.
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            char a = left[i];
            char b = right[i];
            if (a >= 'a' && a <= 'z')
            {
                a = static_cast<char>(a - ('a' - 'A'));
            }
            if (b >= 'a' && b <= 'z')
            {
                b = static_cast<char>(b - ('a' - 'A'));
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    std::int32_t MathRoundToInt32(double value) noexcept
    {
        // Math.Round(double) is MidpointRounding.ToEven.
        return static_cast<std::int32_t>(std::nearbyint(value));
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

    std::string StringTrim(std::string value)
    {
        return TrimManagedWhiteSpace(std::move(value));
    }

    bool SingleTryParseCurrentCulture(std::string text, float& value)
    {
        return TryParseSingleCurrentCulture(std::move(text), value);
    }

    bool Int32TryParseCurrentCulture(std::string_view value, std::int32_t& result)
    {
        // The UTF-16 string the managed parser sees, spelled here as the code
        // points the other overload takes.
        std::u32string wide;
        std::size_t index = 0;
        while (index < value.size())
        {
            const unsigned char lead = static_cast<unsigned char>(value[index]);
            char32_t code = lead;
            std::size_t extra = 0;
            if (lead >= 0xF0)
            {
                code = lead & 0x07U;
                extra = 3;
            }
            else if (lead >= 0xE0)
            {
                code = lead & 0x0FU;
                extra = 2;
            }
            else if (lead >= 0xC0)
            {
                code = lead & 0x1FU;
                extra = 1;
            }
            ++index;
            for (std::size_t i = 0; i < extra && index < value.size(); ++i, ++index)
            {
                code = (code << 6) | (static_cast<unsigned char>(value[index]) & 0x3FU);
            }
            wide.push_back(code);
        }
        return Int32TryParseCurrentCulture(std::u32string_view(wide), result);
    }

    bool SingleTryParseInvariantFloat(std::string text, float& value)
    {
        // The invariant culture's NumberFormatInfo is the struct's own
        // defaults: "." for the point, "," for groups, "-" for the sign.
        const ManagedNumberFormat invariant;
        return TryParseSingle(std::move(text), invariant, value);
    }

    bool DoubleTryParseInvariant(std::string text, double& value)
    {
        // The parse grammar is the single-precision one; only the rounding at
        // the end differs, so the digits are read through the same path.
        float parsed = 0.0F;
        const ManagedNumberFormat invariant;
        if (!TryParseSingle(std::move(text), invariant, parsed))
        {
            value = 0.0;
            return false;
        }
        value = static_cast<double>(parsed);
        return true;
    }

    std::string DoubleToStringFixed(double value, std::int32_t decimals)
    {
        const ManagedNumberFormat format = CurrentManagedNumberFormat();
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
        char buffer[512]{};
        const std::to_chars_result result = std::to_chars(
            std::begin(buffer), std::end(buffer), std::fabs(value),
            std::chars_format::fixed, decimals);
        if (result.ec != std::errc{})
        {
            throw System::FormatException();
        }
        std::string text(buffer, result.ptr);
        const std::size_t point = text.find('.');
        if (point != std::string::npos)
        {
            text.replace(point, 1, format.DecimalSeparator);
        }
        // The custom format keeps a sign only when a digit survived.
        bool anyDigit = false;
        for (const char item : text)
        {
            if (item >= '1' && item <= '9')
            {
                anyDigit = true;
                break;
            }
        }
        if (negative && anyDigit)
        {
            return format.NegativeSign + text;
        }
        return text;
    }

    std::string DoubleToStringFixed2(double value)
    {
        return DoubleToStringFixed(value, 2);
    }

    std::string SingleToStringZeroPointHash(float value)
    {
        const ManagedNumberFormat format = CurrentManagedNumberFormat();
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

        // "0.#": one optional decimal, rounded half away from zero, and the
        // point dropped when that digit rounds to nothing.
        const bool negative = std::signbit(value);
        const double magnitude = std::fabs(static_cast<double>(value));
        const double scaled = std::floor(magnitude * 10.0 + 0.5);
        const std::int64_t whole = static_cast<std::int64_t>(scaled / 10.0);
        const std::int64_t fraction = static_cast<std::int64_t>(scaled) - whole * 10;
        std::string text = std::to_string(whole);
        if (fraction != 0)
        {
            text += format.DecimalSeparator;
            text += static_cast<char>('0' + fraction);
        }
        if (negative && !(whole == 0 && fraction == 0))
        {
            return format.NegativeSign + text;
        }
        return text;
    }

    std::string Int32ToString(std::int32_t value)
    {
        return Int64ToString(value);
    }

    std::string Int64ToString(std::int64_t value)
    {
        // "G" for an integer is the digits and, when negative, the culture's
        // negative sign -- never a group separator.
        const bool negative = value < 0;
        std::string digits;
        std::uint64_t magnitude = negative
            ? (~static_cast<std::uint64_t>(value)) + 1U
            : static_cast<std::uint64_t>(value);
        do
        {
            digits.insert(digits.begin(), static_cast<char>('0' + (magnitude % 10U)));
            magnitude /= 10U;
        }
        while (magnitude != 0U);
        if (!negative)
        {
            return digits;
        }
        return CurrentManagedNumberFormat().NegativeSign + digits;
    }

    std::string DoubleToStringNoDecimals(double value)
    {
        const ManagedNumberFormat format = CurrentManagedNumberFormat();
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

        // The custom format "0" rounds half away from zero, which is what
        // Number.RoundNumber does and is not what std::round's formatting
        // siblings do.
        const bool negative = std::signbit(value);
        const double rounded = std::floor(std::fabs(value) + 0.5);
        char buffer[512]{};
        const std::to_chars_result result = std::to_chars(
            std::begin(buffer), std::end(buffer), rounded, std::chars_format::fixed, 0);
        if (result.ec != std::errc{})
        {
            throw System::FormatException();
        }
        std::string text(buffer, result.ptr);
        // "0" keeps a sign only when a digit survived the rounding.
        if (negative && text != "0")
        {
            return format.NegativeSign + text;
        }
        return text;
    }

    std::string SingleToString(float value)
    {
        const ManagedNumberFormat format = CurrentManagedNumberFormat();
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

        // "G" with no precision: the shortest form that round-trips, which is
        // what Single.ToString() has produced since .NET Core 3.0.
        char buffer[64]{};
        const std::to_chars_result result = std::to_chars(
            std::begin(buffer), std::end(buffer), value, std::chars_format::general);
        if (result.ec != std::errc{})
        {
            throw System::FormatException();
        }
        std::string text(buffer, result.ptr);

        // to_chars writes the invariant forms; the managed one is the
        // culture's.
        const std::size_t exponent = text.find('e');
        if (exponent != std::string::npos)
        {
            text[exponent] = 'E';
            if (exponent + 1 < text.size()
                && text[exponent + 1] != '+' && text[exponent + 1] != '-')
            {
                text.insert(exponent + 1, 1, '+');
            }
        }
        const std::size_t point = text.find('.');
        if (point != std::string::npos)
        {
            text.replace(point, 1, format.DecimalSeparator);
        }
        if (!text.empty() && text.front() == '-')
        {
            text.replace(0, 1, format.NegativeSign);
        }
        return text;
    }

    std::string CurrentDecimalSeparator()
    {
        return CurrentManagedNumberFormat().DecimalSeparator;
    }

    bool Int32TryParseHexNumber(std::string text, std::int32_t& value)
    {
        return TryParseManagedHexInt32(std::move(text), value);
    }
}

namespace MphRead::NativeRuntime
{
    namespace
    {
        // ---- UTF conversions -------------------------------------------------
        [[nodiscard]] std::u16string Utf32ToUtf16(std::u32string_view value)
        {
            std::u16string result;
            result.reserve(value.size());
            for (const char32_t code : value)
            {
                if (code >= 0x10000U)
                {
                    const char32_t adjusted = code - 0x10000U;
                    result.push_back(static_cast<char16_t>(0xD800U + (adjusted >> 10)));
                    result.push_back(static_cast<char16_t>(0xDC00U + (adjusted & 0x3FFU)));
                }
                else
                {
                    result.push_back(static_cast<char16_t>(code));
                }
            }
            return result;
        }

        // ---- GlobalizationMode ----------------------------------------------
        [[nodiscard]] bool GlobalizationInvariantMode()
        {
            static const bool invariant = EnvironmentFlagEnabled("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT");
            return invariant;
        }

        // The culture the process runs under: CultureInfo.CurrentCulture.Name.
        [[nodiscard]] const std::string& CurrentCultureName()
        {
            static const std::string name = []() -> std::string
            {
                if (GlobalizationInvariantMode())
                {
                    return std::string();
                }
#if defined(_WIN32)
                wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
                if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0)
                {
                    return std::string();
                }
                return WideToUtf8(localeName);
#else
                IcuNumberApi& api = CurrentIcuNumberApi();
                if (!api.Available() || api.GetDefaultLocale == nullptr)
                {
                    return std::string();
                }
                // DetectDefaultLocaleName: the POSIX locale maps to the invariant culture.
                std::string locale = api.GetDefaultLocale();
                if (locale == "en_US_POSIX")
                {
                    return std::string();
                }
                // FixupLocaleName: '_' is '-' in a managed culture name.
                const std::size_t at = locale.find('@');
                if (at != std::string::npos)
                {
                    locale.resize(at);
                }
                std::replace(locale.begin(), locale.end(), '_', '-');
                return locale;
#endif
            }();
            return name;
        }

        // CompareInfo.GetIsAsciiEqualityOrdinal.
        [[nodiscard]] bool IsAsciiEqualityOrdinal()
        {
            static const bool value = []()
            {
                if (GlobalizationInvariantMode())
                {
                    return true;
                }
                const std::string& sortName = CurrentCultureName();
                return sortName.empty()
                    || (sortName.size() >= 2 && sortName[0] == 'e' && sortName[1] == 'n'
                        && (sortName.size() == 2 || sortName[2] == '-'));
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

        // ---- ICU collation ---------------------------------------------------
        constexpr std::int32_t UColPrimary = 0;
        constexpr std::int32_t UColSecondary = 1;
        constexpr std::int32_t UColNullOrder = static_cast<std::int32_t>(0xFFFFFFFFU);
        constexpr std::int32_t UColIgnorable = 0;
        constexpr std::int32_t UColPrimaryOrderMask = static_cast<std::int32_t>(0xFFFF0000U);
        constexpr std::int32_t UColSecondaryOrderMask = 0x0000FF00;
        constexpr std::int32_t UColTertiaryOrderMask = 0x000000FF;

        struct IcuCollationApi final
        {
            using OpenFn = void* (*)(const char*, std::int32_t*);
            using CloseFn = void (*)(void*);
            using GetStrengthFn = std::int32_t (*)(const void*);
            using OpenElementsFn = void* (*)(const void*, const char16_t*, std::int32_t, std::int32_t*);
            using StepFn = std::int32_t (*)(void*, std::int32_t*);
            using CloseElementsFn = void (*)(void*);

            OpenFn Open = nullptr;
            CloseFn Close = nullptr;
            GetStrengthFn GetStrength = nullptr;
            OpenElementsFn OpenElements = nullptr;
            StepFn Next = nullptr;
            StepFn Previous = nullptr;
            CloseElementsFn CloseElements = nullptr;

            [[nodiscard]] bool Available() const noexcept
            {
                return Open != nullptr && Close != nullptr && GetStrength != nullptr
                    && OpenElements != nullptr && Next != nullptr && Previous != nullptr
                    && CloseElements != nullptr;
            }
        };

        [[nodiscard]] IcuCollationApi& CurrentIcuCollationApi()
        {
            static IcuCollationApi api = []()
            {
                IcuCollationApi value{};
#if defined(_WIN32)
                WindowsIcuNumberApi& shared = CurrentWindowsIcuNumberApi();
                HMODULE i18n = shared.I18n;
                if (i18n == nullptr)
                {
                    return value;
                }
                value.Open = reinterpret_cast<IcuCollationApi::OpenFn>(
                    reinterpret_cast<void*>(ResolveWindowsIcuSymbol(i18n, "ucol_open")));
                value.Close = reinterpret_cast<IcuCollationApi::CloseFn>(
                    reinterpret_cast<void*>(ResolveWindowsIcuSymbol(i18n, "ucol_close")));
                value.GetStrength = reinterpret_cast<IcuCollationApi::GetStrengthFn>(
                    reinterpret_cast<void*>(ResolveWindowsIcuSymbol(i18n, "ucol_getStrength")));
                value.OpenElements = reinterpret_cast<IcuCollationApi::OpenElementsFn>(
                    reinterpret_cast<void*>(ResolveWindowsIcuSymbol(i18n, "ucol_openElements")));
                value.Next = reinterpret_cast<IcuCollationApi::StepFn>(
                    reinterpret_cast<void*>(ResolveWindowsIcuSymbol(i18n, "ucol_next")));
                value.Previous = reinterpret_cast<IcuCollationApi::StepFn>(
                    reinterpret_cast<void*>(ResolveWindowsIcuSymbol(i18n, "ucol_previous")));
                value.CloseElements = reinterpret_cast<IcuCollationApi::CloseElementsFn>(
                    reinterpret_cast<void*>(ResolveWindowsIcuSymbol(i18n, "ucol_closeElements")));
#else
                IcuNumberApi& shared = CurrentIcuNumberApi();
                void* i18n = shared.I18n;
                if (i18n == nullptr)
                {
                    return value;
                }
                value.Open = reinterpret_cast<IcuCollationApi::OpenFn>(ResolveIcuSymbol(i18n, "ucol_open"));
                value.Close = reinterpret_cast<IcuCollationApi::CloseFn>(ResolveIcuSymbol(i18n, "ucol_close"));
                value.GetStrength = reinterpret_cast<IcuCollationApi::GetStrengthFn>(
                    ResolveIcuSymbol(i18n, "ucol_getStrength"));
                value.OpenElements = reinterpret_cast<IcuCollationApi::OpenElementsFn>(
                    ResolveIcuSymbol(i18n, "ucol_openElements"));
                value.Next = reinterpret_cast<IcuCollationApi::StepFn>(ResolveIcuSymbol(i18n, "ucol_next"));
                value.Previous = reinterpret_cast<IcuCollationApi::StepFn>(ResolveIcuSymbol(i18n, "ucol_previous"));
                value.CloseElements = reinterpret_cast<IcuCollationApi::CloseElementsFn>(
                    ResolveIcuSymbol(i18n, "ucol_closeElements"));
#endif
                return value;
            }();
            return api;
        }

        // GlobalizationNative_GetSortHandle: one collator for the culture, shared.
        [[nodiscard]] void* CurrentCollator()
        {
            static void* collator = []() -> void*
            {
                IcuCollationApi& api = CurrentIcuCollationApi();
                if (!api.Available())
                {
                    return nullptr;
                }
                std::int32_t status = 0;
                void* value = api.Open(CurrentCultureName().c_str(), &status);
                if (status > 0)
                {
                    if (value != nullptr)
                    {
                        api.Close(value);
                    }
                    return nullptr;
                }
                return value;
            }();
            return collator;
        }

        // GetCollationElementMask.
        [[nodiscard]] std::int32_t GetCollationElementMask(std::int32_t strength) noexcept
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

        // SimpleAffix_Iterators.
        [[nodiscard]] bool SimpleAffixIterators(IcuCollationApi& api, void* patternIterator,
            void* sourceIterator, std::int32_t strength, bool forwardSearch)
        {
            std::int32_t errorCode = 0;
            bool movePattern = true;
            bool moveSource = true;
            std::int32_t patternElement = UColIgnorable;
            std::int32_t sourceElement = UColIgnorable;
            const std::int32_t collationElementMask = GetCollationElementMask(strength);
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
                    if (sourceElement == UColNullOrder)
                    {
                        return true;
                    }
                    if (sourceElement == UColIgnorable)
                    {
                        return true;
                    }
                    if (forwardSearch && (sourceElement & UColPrimaryOrderMask) == 0
                        && (sourceElement & UColSecondaryOrderMask) != 0)
                    {
                        return false;
                    }
                    return true;
                }
                if (patternElement == UColIgnorable)
                {
                    moveSource = false;
                }
                else if (sourceElement == UColIgnorable)
                {
                    movePattern = false;
                }
                else if ((patternElement & collationElementMask) != (sourceElement & collationElementMask))
                {
                    return false;
                }
            }
        }

        // SimpleAffix.
        [[nodiscard]] bool SimpleAffix(std::u16string_view pattern, std::u16string_view text, bool forwardSearch)
        {
            IcuCollationApi& api = CurrentIcuCollationApi();
            void* collator = CurrentCollator();
            if (!api.Available() || collator == nullptr)
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
                    const std::int32_t strength = api.GetStrength(collator);
                    result = SimpleAffixIterators(api, patternIterator, sourceIterator, strength, forwardSearch);
                    api.CloseElements(sourceIterator);
                }
                api.CloseElements(patternIterator);
            }
            return result;
        }

#if defined(_WIN32)
        constexpr std::uint32_t FindStartsWith = 0x00100000U;
        constexpr std::uint32_t FindEndsWith = 0x00200000U;
        constexpr std::uint32_t NormLinguisticCasing = 0x08000000U;

        [[nodiscard]] bool UseNls()
        {
            static const bool useNls = []()
            {
                if (EnvironmentFlagEnabled("DOTNET_SYSTEM_GLOBALIZATION_USENLS"))
                {
                    return true;
                }
                // .NET falls back to NLS when ICU cannot be loaded.
                return !CurrentIcuCollationApi().Available() || CurrentCollator() == nullptr;
            }();
            return useNls;
        }

        // CompareInfo.FindString with FIND_STARTSWITH / FIND_ENDSWITH.
        [[nodiscard]] int NlsFindString(std::uint32_t flags, std::u16string_view source, std::u16string_view value)
        {
            const std::string& cultureName = CurrentCultureName();
            const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, cultureName.data(),
                static_cast<int>(cultureName.size()), nullptr, 0);
            std::wstring locale(static_cast<std::size_t>(wideLength), L' ');
            ::MultiByteToWideChar(CP_UTF8, 0, cultureName.data(), static_cast<int>(cultureName.size()),
                locale.data(), wideLength);
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

        // CompareInfo.StartsWithOrdinalHelper, and the ICU call it falls back to.
        [[nodiscard]] bool StartsWithCore(std::u16string_view source, std::u16string_view prefix);

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
                if (a + 1 < source.size() && source[a + 1] >= 0x80U)
                {
                    return StartsWithCore(source, prefix);
                }
                if (b + 1 < prefix.size() && prefix[b + 1] >= 0x80U)
                {
                    return StartsWithCore(source, prefix);
                }
                return false;
            }
            if (source.size() < prefix.size())
            {
                if (IsHighChar(prefix[b]))
                {
                    return StartsWithCore(source, prefix);
                }
                return false;
            }
            if (source.size() > prefix.size())
            {
                if (IsHighChar(source[a]))
                {
                    return StartsWithCore(source, prefix);
                }
            }
            return true;
        }

        // CompareInfo.EndsWithOrdinalHelper, and the ICU call it falls back to.
        [[nodiscard]] bool EndsWithCore(std::u16string_view source, std::u16string_view suffix);

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
                if (a - 1 > 0 && source[a - 2] >= 0x80U)
                {
                    return EndsWithCore(source, suffix);
                }
                if (b - 1 > 0 && suffix[b - 2] >= 0x80U)
                {
                    return EndsWithCore(source, suffix);
                }
                return false;
            }
            if (source.size() < suffix.size())
            {
                if (IsHighChar(suffix[b - 1]))
                {
                    return EndsWithCore(source, suffix);
                }
                return false;
            }
            if (source.size() > suffix.size())
            {
                if (IsHighChar(source[a - 1]))
                {
                    return EndsWithCore(source, suffix);
                }
            }
            return true;
        }

        bool StartsWithCore(std::u16string_view source, std::u16string_view prefix)
        {
#if defined(_WIN32)
            if (UseNls())
            {
                return NlsFindString(FindStartsWith | NormLinguisticCasing, source, prefix) >= 0;
            }
#endif
            return SimpleAffix(prefix, source, true);
        }

        bool EndsWithCore(std::u16string_view source, std::u16string_view suffix)
        {
#if defined(_WIN32)
            if (UseNls())
            {
                return NlsFindString(FindEndsWith | NormLinguisticCasing, source, suffix) >= 0;
            }
#endif
            return SimpleAffix(suffix, source, false);
        }

        // CompareInfo.IsPrefix / IsSuffix with CompareOptions.None.
        [[nodiscard]] bool IsPrefix(std::u16string_view source, std::u16string_view prefix)
        {
            if (prefix.empty())
            {
                return true;
            }
            if (GlobalizationInvariantMode())
            {
                return source.size() >= prefix.size() && source.compare(0, prefix.size(), prefix) == 0;
            }
            if (IsAsciiEqualityOrdinal())
            {
                return StartsWithOrdinalHelper(source, prefix);
            }
            return StartsWithCore(source, prefix);
        }

        [[nodiscard]] bool IsSuffix(std::u16string_view source, std::u16string_view suffix)
        {
            if (suffix.empty())
            {
                return true;
            }
            if (GlobalizationInvariantMode())
            {
                return source.size() >= suffix.size()
                    && source.compare(source.size() - suffix.size(), suffix.size(), suffix) == 0;
            }
            if (IsAsciiEqualityOrdinal())
            {
                return EndsWithOrdinalHelper(source, suffix);
            }
            return EndsWithCore(source, suffix);
        }

        // Number.IsWhite.
        [[nodiscard]] constexpr bool IsParseWhite(char32_t value) noexcept
        {
            return value == 0x20U || (value >= 0x09U && value <= 0x0DU);
        }

        // Number.TrailingZeros.
        [[nodiscard]] bool TrailingZeros(std::u32string_view value, std::size_t index) noexcept
        {
            for (std::size_t i = index; i < value.size(); i++)
            {
                if (value[i] != U'\0')
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] std::u32string Utf8ToUtf32(std::string_view value)
        {
            std::u32string result;
            result.reserve(value.size());
            std::size_t index = 0;
            while (index < value.size())
            {
                const auto [code, length] = DecodeUtf8At(value, index);
                index += length;
                result.push_back(code);
            }
            return result;
        }

        // Number.IsDigit.
        [[nodiscard]] constexpr bool IsParseDigit(char32_t value) noexcept
        {
            return value >= U'0' && value <= U'9';
        }
    }

    bool StringEndsWithCurrentCulture(const std::string& value, const std::string& suffix)
    {
        // string.EndsWith(string): CurrentCulture, CompareOptions.None.
        return IsSuffix(Utf8ToUtf16(value), Utf8ToUtf16(suffix));
    }

    bool StringStartsWithCurrentCulture(std::u32string_view value, std::u32string_view prefix)
    {
        // string.StartsWith(string): CurrentCulture, CompareOptions.None.
        return IsPrefix(Utf32ToUtf16(value), Utf32ToUtf16(prefix));
    }

    bool Int32TryParseCurrentCulture(std::u32string_view value, std::int32_t& result)
    {
        // Number.TryParseBinaryIntegerStyle with NumberStyles.Integer (leading and
        // trailing white, leading sign) and the current culture's signs.
        constexpr std::int32_t MaxDigitCount = 10;
        constexpr std::uint32_t MaxValueDiv10 = 214748364U;
        const ManagedNumberFormat format = CurrentManagedNumberFormat();

        std::uint32_t answer = 0;
        bool isNegative = false;
        bool overflow = false;
        std::size_t index = 0;
        char32_t num = 0;

        if (value.empty())
        {
            goto FalseExit;
        }
        num = value[0];

        if (IsParseWhite(num))
        {
            do
            {
                index++;
                if (index >= value.size())
                {
                    goto FalseExit;
                }
                num = value[index];
            }
            while (IsParseWhite(num));
        }

        if (format.PositiveSign == "+" && format.NegativeSign == "-")
        {
            if (num == U'-' || num == U'+')
            {
                isNegative = num == U'-';
                index++;
                if (index >= value.size())
                {
                    goto FalseExit;
                }
                num = value[index];
            }
        }
        else if (ManagedAllowsHyphenFallback(format.NegativeSign) && num == U'-')
        {
            isNegative = true;
            index++;
            if (index >= value.size())
            {
                goto FalseExit;
            }
            num = value[index];
        }
        else
        {
            const std::u32string positiveSign = Utf8ToUtf32(format.PositiveSign);
            const std::u32string negativeSign = Utf8ToUtf32(format.NegativeSign);
            const std::u32string_view rest = value.substr(index);
            if (!positiveSign.empty() && rest.size() >= positiveSign.size()
                && rest.compare(0, positiveSign.size(), positiveSign) == 0)
            {
                index += positiveSign.size();
                if (index >= value.size())
                {
                    goto FalseExit;
                }
                num = value[index];
            }
            else if (!negativeSign.empty() && rest.size() >= negativeSign.size()
                && rest.compare(0, negativeSign.size(), negativeSign) == 0)
            {
                isNegative = true;
                index += negativeSign.size();
                if (index >= value.size())
                {
                    goto FalseExit;
                }
                num = value[index];
            }
        }

        if (!IsParseDigit(num))
        {
            goto FalseExit;
        }

        if (num == U'0')
        {
            do
            {
                index++;
                if (index >= value.size())
                {
                    goto DoneAtEnd;
                }
                num = value[index];
            }
            while (num == U'0');
            if (!IsParseDigit(num))
            {
                goto HasTrailingChars;
            }
        }

        answer = static_cast<std::uint32_t>(num - U'0');
        index++;

        for (std::int32_t i = 0; i < MaxDigitCount - 2; i++)
        {
            if (index >= value.size())
            {
                goto DoneAtEnd;
            }
            num = value[index];
            if (!IsParseDigit(num))
            {
                goto HasTrailingChars;
            }
            index++;
            answer = answer * 10U + static_cast<std::uint32_t>(num - U'0');
        }

        if (index >= value.size())
        {
            goto DoneAtEnd;
        }
        num = value[index];
        if (!IsParseDigit(num))
        {
            goto HasTrailingChars;
        }
        index++;

        overflow = answer > MaxValueDiv10;
        answer = answer * 10U + static_cast<std::uint32_t>(num - U'0');
        overflow = overflow
            || answer > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                + (isNegative ? 1U : 0U);

        if (index >= value.size())
        {
            goto DoneAtEndButPotentialOverflow;
        }

        num = value[index];
        while (IsParseDigit(num))
        {
            overflow = true;
            index++;
            if (index >= value.size())
            {
                goto OverflowExit;
            }
            num = value[index];
        }
        goto HasTrailingChars;

    DoneAtEndButPotentialOverflow:
        if (overflow)
        {
            goto OverflowExit;
        }

    DoneAtEnd:
        result = isNegative
            ? static_cast<std::int32_t>(0U - answer)
            : static_cast<std::int32_t>(answer);
        return true;

    FalseExit:
        result = 0;
        return false;

    OverflowExit:
        result = 0;
        return false;

    HasTrailingChars:
        if (IsParseWhite(num))
        {
            for (index++; index < value.size(); index++)
            {
                if (!IsParseWhite(value[index]))
                {
                    break;
                }
            }
            if (index >= value.size())
            {
                goto DoneAtEndButPotentialOverflow;
            }
        }
        if (!TrailingZeros(value, index))
        {
            goto FalseExit;
        }
        goto DoneAtEndButPotentialOverflow;
    }
}
