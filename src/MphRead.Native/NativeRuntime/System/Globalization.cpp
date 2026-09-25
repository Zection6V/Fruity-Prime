#include "Globalization.hpp"

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
