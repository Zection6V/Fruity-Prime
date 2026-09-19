#include "Q3Convert.hpp"

#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"
#include "CustomRooms.hpp"
#include "MapBuilder.hpp"
#include "MapDefinition.hpp"
#include "MapTextureBake.hpp"
#include "Q3Bsp.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <locale.h>
#include <memory>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    using MphRead::ItemType;
    using MphRead::Mods::MapGen::MapDefinition;
    using MphRead::Mods::MapGen::Q3Entity;
    using MphRead::Mods::MapGen::Q3StringEqual;

    enum class Utf8DecodeStatus
    {
        Done,
        NeedMoreData,
        InvalidData
    };

    [[noreturn]] void NullReference()
    {
        throw System::NullReferenceException();
    }

    [[noreturn]] void ArrayBounds()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }

    template <typename T>
    [[nodiscard]] T* Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            NullReference();
        }
        return value.get();
    }

    template <typename T>
    [[nodiscard]] const T* Require(const std::shared_ptr<const T>& value)
    {
        if (!value)
        {
            NullReference();
        }
        return value.get();
    }

    template <typename T>
    [[nodiscard]] const T& ListAt(const std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range(
                "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ArrayAt(const std::vector<T>* values, std::size_t index)
    {
        if (values == nullptr)
        {
            NullReference();
        }
        if (index >= values->size())
        {
            ArrayBounds();
        }
        return (*values)[index];
    }

    template <typename T>
    [[nodiscard]] T& ArrayAt(std::vector<T>* values, std::size_t index)
    {
        if (values == nullptr)
        {
            NullReference();
        }
        if (index >= values->size())
        {
            ArrayBounds();
        }
        return (*values)[index];
    }

    [[nodiscard]] constexpr std::int32_t WrapAdd(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] float ManagedMin(float left, float right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        if (left == right && left == 0.0F)
        {
            return std::signbit(left) || std::signbit(right) ? -0.0F : 0.0F;
        }
        return left < right ? left : right;
    }

    [[nodiscard]] float ManagedMax(float left, float right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        if (left == right && left == 0.0F)
        {
            return std::signbit(left) && std::signbit(right) ? -0.0F : 0.0F;
        }
        return left > right ? left : right;
    }

    [[nodiscard]] float RoundToEven(float value) noexcept
    {
        if (!std::isfinite(value) || value == 0.0F)
        {
            return value;
        }
        if (std::fabs(value) >= 8388608.0F)
        {
            return value;
        }

        const float lower = std::floor(value);
        const float difference = value - lower;
        float rounded;
        if (difference < 0.5F)
        {
            rounded = lower;
        }
        else if (difference > 0.5F)
        {
            rounded = lower + 1.0F;
        }
        else
        {
            rounded = std::fmod(std::fabs(lower), 2.0F) == 0.0F
                ? lower
                : lower + 1.0F;
        }
        if (rounded == 0.0F)
        {
            return std::copysign(0.0F, value);
        }
        return rounded;
    }

    [[nodiscard]] bool IsAsciiWhitespace(unsigned char value) noexcept
    {
        return value == 0x20U || (value >= 0x09U && value <= 0x0DU);
    }

    [[nodiscard]] std::size_t LeadingUnicodeWhitespaceBytes(
        std::string_view value) noexcept
    {
        if (value.empty())
        {
            return 0;
        }
        const auto b0 = static_cast<unsigned char>(value[0]);
        if (IsAsciiWhitespace(b0))
        {
            return 1;
        }
        if (value.size() >= 2)
        {
            const auto b1 = static_cast<unsigned char>(value[1]);
            if (b0 == 0xC2U && (b1 == 0x85U || b1 == 0xA0U))
            {
                return 2;
            }
        }
        if (value.size() >= 3)
        {
            const auto b1 = static_cast<unsigned char>(value[1]);
            const auto b2 = static_cast<unsigned char>(value[2]);
            if ((b0 == 0xE1U && b1 == 0x9AU && b2 == 0x80U)
                || (b0 == 0xE2U && b1 == 0x80U
                    && ((b2 >= 0x80U && b2 <= 0x8AU)
                        || b2 == 0xA8U || b2 == 0xA9U || b2 == 0xAFU))
                || (b0 == 0xE2U && b1 == 0x81U && b2 == 0x9FU)
                || (b0 == 0xE3U && b1 == 0x80U && b2 == 0x80U))
            {
                return 3;
            }
        }
        return 0;
    }

    [[nodiscard]] std::size_t TrailingUnicodeWhitespaceBytes(
        std::string_view value) noexcept
    {
        if (value.empty())
        {
            return 0;
        }
        const auto last = static_cast<unsigned char>(value.back());
        if (IsAsciiWhitespace(last))
        {
            return 1;
        }
        if (value.size() >= 2)
        {
            const auto b0 = static_cast<unsigned char>(value[value.size() - 2]);
            if (b0 == 0xC2U && (last == 0x85U || last == 0xA0U))
            {
                return 2;
            }
        }
        if (value.size() >= 3)
        {
            const auto b0 = static_cast<unsigned char>(value[value.size() - 3]);
            const auto b1 = static_cast<unsigned char>(value[value.size() - 2]);
            if ((b0 == 0xE1U && b1 == 0x9AU && last == 0x80U)
                || (b0 == 0xE2U && b1 == 0x80U
                    && ((last >= 0x80U && last <= 0x8AU)
                        || last == 0xA8U || last == 0xA9U || last == 0xAFU))
                || (b0 == 0xE2U && b1 == 0x81U && last == 0x9FU)
                || (b0 == 0xE3U && b1 == 0x80U && last == 0x80U))
            {
                return 3;
            }
        }
        return 0;
    }

    [[nodiscard]] std::string_view TrimUnicodeWhitespace(
        std::string_view value) noexcept
    {
        for (;;)
        {
            const std::size_t count = LeadingUnicodeWhitespaceBytes(value);
            if (count == 0)
            {
                break;
            }
            value.remove_prefix(count);
        }
        for (;;)
        {
            const std::size_t count = TrailingUnicodeWhitespaceBytes(value);
            if (count == 0)
            {
                break;
            }
            value.remove_suffix(count);
        }
        return value;
    }

    [[nodiscard]] bool EqualsIgnoreCaseAscii(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            unsigned char a = static_cast<unsigned char>(left[i]);
            unsigned char b = static_cast<unsigned char>(right[i]);
            if (a >= 'a' && a <= 'z')
            {
                a = static_cast<unsigned char>(a - ('a' - 'A'));
            }
            if (b >= 'a' && b <= 'z')
            {
                b = static_cast<unsigned char>(b - ('a' - 'A'));
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool IsFloatNumber(
        std::string_view text, bool& negative) noexcept
    {
        negative = false;
        if (text.empty())
        {
            return false;
        }
        std::size_t position = 0;
        if (text[position] == '+' || text[position] == '-')
        {
            negative = text[position] == '-';
            ++position;
        }
        if (position == text.size())
        {
            return false;
        }

        bool digits = false;
        while (position < text.size()
            && text[position] >= '0' && text[position] <= '9')
        {
            digits = true;
            ++position;
        }
        if (position < text.size() && text[position] == '.')
        {
            ++position;
            while (position < text.size()
                && text[position] >= '0' && text[position] <= '9')
            {
                digits = true;
                ++position;
            }
        }
        if (!digits)
        {
            return false;
        }
        if (position < text.size()
            && (text[position] == 'e' || text[position] == 'E'))
        {
            ++position;
            if (position < text.size()
                && (text[position] == '+' || text[position] == '-'))
            {
                ++position;
            }
            const std::size_t exponentStart = position;
            while (position < text.size()
                && text[position] >= '0' && text[position] <= '9')
            {
                ++position;
            }
            if (position == exponentStart)
            {
                return false;
            }
        }
        return position == text.size();
    }

    [[nodiscard]] bool DecimalIsBelowOne(std::string_view text) noexcept
    {
        std::size_t position = 0;
        if (!text.empty() && (text.front() == '+' || text.front() == '-'))
        {
            position = 1;
        }

        std::int64_t digitsBeforeDecimal = 0;
        std::int64_t digitIndex = 0;
        std::int64_t firstNonzero = -1;
        bool beforeDecimal = true;
        while (position < text.size()
            && text[position] != 'e' && text[position] != 'E')
        {
            const char ch = text[position++];
            if (ch == '.')
            {
                beforeDecimal = false;
                continue;
            }
            if (beforeDecimal)
            {
                ++digitsBeforeDecimal;
            }
            if (firstNonzero < 0 && ch != '0')
            {
                firstNonzero = digitIndex;
            }
            ++digitIndex;
        }
        if (firstNonzero < 0)
        {
            return true;
        }

        std::int64_t exponent = 0;
        if (position < text.size())
        {
            ++position;
            bool exponentNegative = false;
            if (position < text.size()
                && (text[position] == '+' || text[position] == '-'))
            {
                exponentNegative = text[position] == '-';
                ++position;
            }
            constexpr std::int64_t Limit = 1'000'000;
            while (position < text.size())
            {
                const std::int64_t digit = text[position++] - '0';
                exponent = std::min(Limit, exponent * 10 + digit);
            }
            if (exponentNegative)
            {
                exponent = -exponent;
            }
        }

        const std::int64_t scientificExponent
            = digitsBeforeDecimal - firstNonzero - 1 + exponent;
        return scientificExponent < 0;
    }

    [[nodiscard]] bool TryParseNumericSingleInvariant(
        std::string_view text, float& result) noexcept
    {
        result = 0.0F;
        std::size_t start = 0;
        while (start < text.size()
            && IsAsciiWhitespace(static_cast<unsigned char>(text[start])))
        {
            ++start;
        }
        if (start == text.size())
        {
            return false;
        }

        std::size_t end = start;
        if (text[end] == '+' || text[end] == '-')
        {
            ++end;
        }
        const std::size_t digitsStart = end;
        bool digits = false;
        while (end < text.size() && text[end] >= '0' && text[end] <= '9')
        {
            digits = true;
            ++end;
        }
        if (end < text.size() && text[end] == '.')
        {
            ++end;
            while (end < text.size() && text[end] >= '0' && text[end] <= '9')
            {
                digits = true;
                ++end;
            }
        }
        if (!digits || end == digitsStart)
        {
            return false;
        }
        if (end < text.size() && (text[end] == 'e' || text[end] == 'E'))
        {
            const std::size_t exponentMarker = end++;
            if (end < text.size() && (text[end] == '+' || text[end] == '-'))
            {
                ++end;
            }
            const std::size_t exponentStart = end;
            while (end < text.size() && text[end] >= '0' && text[end] <= '9')
            {
                ++end;
            }
            if (end == exponentStart)
            {
                end = exponentMarker;
            }
        }

        const std::size_t numericEnd = end;
        while (end < text.size()
            && IsAsciiWhitespace(static_cast<unsigned char>(text[end])))
        {
            ++end;
        }
        while (end < text.size() && text[end] == '\0')
        {
            ++end;
        }
        if (end != text.size())
        {
            return false;
        }

        const std::string_view numeric = text.substr(
            start, numericEnd - start);
        bool negative = false;
        if (!IsFloatNumber(numeric, negative))
        {
            return false;
        }

        std::string_view magnitude = numeric;
        if (magnitude.front() == '+' || magnitude.front() == '-')
        {
            magnitude.remove_prefix(1);
        }

        float parsed = 0.0F;
        const char* first = magnitude.data();
        const char* last = first + magnitude.size();
        const auto conversion = std::from_chars(
            first, last, parsed, std::chars_format::general);
        if (conversion.ptr == last && conversion.ec == std::errc{})
        {
            result = negative ? -parsed : parsed;
            return true;
        }
        if (conversion.ptr != last
            || conversion.ec != std::errc::result_out_of_range)
        {
            return false;
        }

        long double wide = 0.0L;
        const auto wideConversion = std::from_chars(
            first, last, wide, std::chars_format::general);
        if (wideConversion.ptr == last && wideConversion.ec == std::errc{})
        {
            parsed = static_cast<float>(wide);
            result = negative ? -parsed : parsed;
            return true;
        }
        if (wideConversion.ptr != last
            || wideConversion.ec != std::errc::result_out_of_range)
        {
            return false;
        }

        if (DecimalIsBelowOne(numeric))
        {
            result = negative ? -0.0F : 0.0F;
        }
        else
        {
            result = negative
                ? -std::numeric_limits<float>::infinity()
                : std::numeric_limits<float>::infinity();
        }
        return true;
    }

    [[nodiscard]] float ManagedNaN() noexcept
    {
        return std::bit_cast<float>(0xFFC00000U);
    }

    [[nodiscard]] bool TryParseSingleInvariant(
        std::string_view text, float& result) noexcept
    {
        if (TryParseNumericSingleInvariant(text, result))
        {
            return true;
        }

        const std::string_view value = TrimUnicodeWhitespace(text);
        if (EqualsIgnoreCaseAscii(value, "Infinity")
            || EqualsIgnoreCaseAscii(value, "+Infinity"))
        {
            result = std::numeric_limits<float>::infinity();
            return true;
        }
        if (EqualsIgnoreCaseAscii(value, "-Infinity"))
        {
            result = -std::numeric_limits<float>::infinity();
            return true;
        }
        if (EqualsIgnoreCaseAscii(value, "NaN")
            || EqualsIgnoreCaseAscii(value, "+NaN")
            || EqualsIgnoreCaseAscii(value, "-NaN"))
        {
            result = ManagedNaN();
            return true;
        }

        result = 0.0F;
        return false;
    }

    [[nodiscard]] Utf8DecodeStatus DecodeUtf8Scalar(
        const std::uint8_t* data,
        std::size_t size,
        std::size_t& index,
        std::uint32_t& scalar) noexcept
    {
        const std::size_t start = index;
        if (start >= size)
        {
            scalar = 0xFFFDU;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t first = data[start];
        if (first <= 0x7FU)
        {
            scalar = first;
            index = start + 1;
            return Utf8DecodeStatus::Done;
        }
        if (first < 0xC2U || first > 0xF4U)
        {
            scalar = 0xFFFDU;
            index = start + 1;
            return Utf8DecodeStatus::InvalidData;
        }
        if (start + 1 >= size)
        {
            scalar = 0xFFFDU;
            index = size;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t second = data[start + 1];
        if ((second & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            index = start + 1;
            return Utf8DecodeStatus::InvalidData;
        }
        if (first <= 0xDFU)
        {
            scalar = ((first & 0x1FU) << 6) | (second & 0x3FU);
            index = start + 2;
            return Utf8DecodeStatus::Done;
        }
        if ((first == 0xE0U && second < 0xA0U)
            || (first == 0xEDU && second >= 0xA0U)
            || (first == 0xF0U && second < 0x90U)
            || (first == 0xF4U && second >= 0x90U))
        {
            scalar = 0xFFFDU;
            index = start + 1;
            return Utf8DecodeStatus::InvalidData;
        }
        if (start + 2 >= size)
        {
            scalar = 0xFFFDU;
            index = size;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t third = data[start + 2];
        if ((third & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            index = start + 2;
            return Utf8DecodeStatus::InvalidData;
        }
        if (first <= 0xEFU)
        {
            scalar = ((first & 0x0FU) << 12)
                | ((second & 0x3FU) << 6)
                | (third & 0x3FU);
            index = start + 3;
            return Utf8DecodeStatus::Done;
        }
        if (start + 3 >= size)
        {
            scalar = 0xFFFDU;
            index = size;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t fourth = data[start + 3];
        if ((fourth & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            index = start + 3;
            return Utf8DecodeStatus::InvalidData;
        }

        scalar = ((first & 0x07U) << 18)
            | ((second & 0x3FU) << 12)
            | ((third & 0x3FU) << 6)
            | (fourth & 0x3FU);
        index = start + 4;
        return Utf8DecodeStatus::Done;
    }

    void AppendUtf8(std::string& output, std::uint32_t scalar)
    {
        if (scalar <= 0x7FU)
        {
            output.push_back(static_cast<char>(scalar));
        }
        else if (scalar <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (scalar >> 6)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else if (scalar <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (scalar >> 12)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (scalar >> 18)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
    }

#if !defined(_WIN32)
    [[nodiscard]] void* FindVersionedIcuSymbol(
        void* library, const char* base) noexcept
    {
        if (library == nullptr)
        {
            return nullptr;
        }
        if (void* symbol = dlsym(library, base); symbol != nullptr)
        {
            return symbol;
        }
        char name[96]{};
        for (int version = 99; version >= 50; --version)
        {
            const int count = std::snprintf(
                name, sizeof(name), "%s_%d", base, version);
            if (count <= 0
                || static_cast<std::size_t>(count) >= sizeof(name))
            {
                continue;
            }
            if (void* symbol = dlsym(library, name); symbol != nullptr)
            {
                return symbol;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::uint32_t IcuCase(
        std::uint32_t scalar, bool upper) noexcept
    {
        using CaseFunction = std::int32_t (*)(std::int32_t);
        struct Functions final
        {
            CaseFunction Upper = nullptr;
            CaseFunction Lower = nullptr;
        };
        static const Functions functions = []() noexcept
        {
            void* library = dlopen("libicuuc.so", RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
            if (library == nullptr)
            {
                library = dlopen(
                    "/usr/lib/libicucore.A.dylib",
                    RTLD_LAZY | RTLD_LOCAL);
            }
#endif
            return Functions{
                reinterpret_cast<CaseFunction>(
                    FindVersionedIcuSymbol(library, "u_toupper")),
                reinterpret_cast<CaseFunction>(
                    FindVersionedIcuSymbol(library, "u_tolower"))
            };
        }();

        const CaseFunction function = upper
            ? functions.Upper
            : functions.Lower;
        if (function == nullptr || scalar > 0x10FFFFU)
        {
            return scalar;
        }
        const std::int32_t mapped = function(
            static_cast<std::int32_t>(scalar));
        return mapped < 0
            ? scalar
            : static_cast<std::uint32_t>(mapped);
    }
#endif

    [[nodiscard]] std::uint32_t InvariantCaseScalar(
        std::uint32_t scalar, bool upper) noexcept
    {
        if (scalar >= 'a' && scalar <= 'z' && upper)
        {
            return scalar - ('a' - 'A');
        }
        if (scalar >= 'A' && scalar <= 'Z' && !upper)
        {
            return scalar + ('a' - 'A');
        }

#if defined(_WIN32)
        wchar_t source[2]{};
        int sourceLength = 0;
        if (scalar <= 0xFFFFU)
        {
            source[0] = static_cast<wchar_t>(scalar);
            sourceLength = 1;
        }
        else if (scalar <= 0x10FFFFU)
        {
            const std::uint32_t value = scalar - 0x10000U;
            source[0] = static_cast<wchar_t>(
                0xD800U + (value >> 10));
            source[1] = static_cast<wchar_t>(
                0xDC00U + (value & 0x3FFU));
            sourceLength = 2;
        }
        if (sourceLength != 0)
        {
            wchar_t target[2]{};
            const DWORD flag = upper ? LCMAP_UPPERCASE : LCMAP_LOWERCASE;
            const int mapped = LCMapStringEx(
                LOCALE_NAME_INVARIANT,
                flag,
                source,
                sourceLength,
                target,
                2,
                nullptr,
                nullptr,
                0);
            if (mapped == 1)
            {
                return static_cast<std::uint32_t>(target[0]);
            }
            if (mapped == 2
                && target[0] >= 0xD800
                && target[0] <= 0xDBFF
                && target[1] >= 0xDC00
                && target[1] <= 0xDFFF)
            {
                return 0x10000U
                    + ((static_cast<std::uint32_t>(target[0]) - 0xD800U) << 10)
                    + (static_cast<std::uint32_t>(target[1]) - 0xDC00U);
            }
        }
#else
        const std::uint32_t mapped = IcuCase(scalar, upper);
        if (mapped != scalar)
        {
            return mapped;
        }

        static locale_t locale = []() noexcept
        {
            locale_t value = newlocale(
                LC_CTYPE_MASK, "C.UTF-8", nullptr);
            if (value == nullptr)
            {
                value = newlocale(
                    LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
            }
            return value;
        }();
        if (locale != nullptr
            && scalar <= static_cast<std::uint32_t>(WCHAR_MAX))
        {
            const wint_t mappedWide = upper
                ? towupper_l(static_cast<wint_t>(scalar), locale)
                : towlower_l(static_cast<wint_t>(scalar), locale);
            if (mappedWide != WEOF)
            {
                return static_cast<std::uint32_t>(mappedWide);
            }
        }
#endif

        if (upper)
        {
            if (scalar >= 0x00E0U && scalar <= 0x00F6U)
            {
                return scalar - 0x20U;
            }
            if (scalar >= 0x00F8U && scalar <= 0x00FEU)
            {
                return scalar - 0x20U;
            }
            if (scalar == 0x00FFU)
            {
                return 0x0178U;
            }
            if (scalar >= 0x03B1U && scalar <= 0x03C1U)
            {
                return scalar - 0x20U;
            }
            if (scalar >= 0x03C3U && scalar <= 0x03CBU)
            {
                return scalar - 0x20U;
            }
            if (scalar >= 0x0430U && scalar <= 0x044FU)
            {
                return scalar - 0x20U;
            }
        }
        else
        {
            if (scalar >= 0x00C0U && scalar <= 0x00D6U)
            {
                return scalar + 0x20U;
            }
            if (scalar >= 0x00D8U && scalar <= 0x00DEU)
            {
                return scalar + 0x20U;
            }
            if (scalar == 0x0178U)
            {
                return 0x00FFU;
            }
            if (scalar >= 0x0391U && scalar <= 0x03A1U)
            {
                return scalar + 0x20U;
            }
            if (scalar >= 0x03A3U && scalar <= 0x03ABU)
            {
                return scalar + 0x20U;
            }
            if (scalar >= 0x0410U && scalar <= 0x042FU)
            {
                return scalar + 0x20U;
            }
        }
        return scalar;
    }

    [[nodiscard]] std::string InvariantCase(
        const std::string& value, bool upper)
    {
        std::string result;
        result.reserve(value.size());
        std::size_t index = 0;
        while (index < value.size())
        {
            std::uint32_t scalar = 0;
            (void)DecodeUtf8Scalar(
                reinterpret_cast<const std::uint8_t*>(value.data()),
                value.size(),
                index,
                scalar);
            AppendUtf8(
                result,
                InvariantCaseScalar(scalar, upper));
        }
        return result;
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(
        std::string_view value)
    {
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (unsigned char ch : value)
        {
            converted.push_back(static_cast<char8_t>(ch));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] std::string PathToUtf8(
        const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t ch : value)
        {
            result.push_back(static_cast<char>(ch));
        }
        return result;
#else
        return path.u8string();
#endif
    }

    [[nodiscard]] std::string CombinePath(
        const std::string& first,
        const std::string& second)
    {
        if (first.empty())
        {
            return second;
        }
        if (second.empty())
        {
            return first;
        }
        return PathToUtf8(
            PathFromUtf8(first) / PathFromUtf8(second));
    }

    [[nodiscard]] std::string FileName(
        const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).filename());
    }

    void ValidatePathText(const std::string& path)
    {
        if (path.find('\0') != std::string::npos)
        {
            throw std::invalid_argument(
                "Null character in path. (Parameter 'path')");
        }
    }

    [[nodiscard]] bool WindowsEffectivelyEmpty(
        const std::string& path) noexcept
    {
#if defined(_WIN32)
        return !path.empty()
            && std::all_of(
                path.begin(), path.end(),
                [](char ch) noexcept { return ch == ' '; });
#else
        (void)path;
        return false;
#endif
    }

    [[nodiscard]] std::string FullPath(
        const std::string& path)
    {
        if (path.empty())
        {
            throw std::invalid_argument(
                "The value cannot be an empty string. (Parameter 'path')");
        }
        if (WindowsEffectivelyEmpty(path))
        {
            throw std::invalid_argument(
                "The path is empty. (Parameter 'path')");
        }
        ValidatePathText(path);
        return PathToUtf8(
            std::filesystem::absolute(
                PathFromUtf8(path)).lexically_normal());
    }

    [[nodiscard]] bool FileExists(
        const std::optional<std::string>& path) noexcept
    {
        if (!path.has_value() || path->empty())
        {
            return false;
        }
        try
        {
            std::error_code error;
            const bool exists = std::filesystem::is_regular_file(
                PathFromUtf8(*path), error);
            return !error && exists;
        }
        catch (...)
        {
            return false;
        }
    }

    void CreateDirectory(const std::string& path)
    {
        if (path.empty())
        {
            throw std::invalid_argument(
                "The value cannot be an empty string. (Parameter 'path')");
        }
        if (WindowsEffectivelyEmpty(path))
        {
            throw std::invalid_argument(
                "The path is empty. (Parameter 'path')");
        }
        ValidatePathText(path);
        (void)std::filesystem::create_directories(
            PathFromUtf8(path));
    }

    void CopyFile(
        const std::string& source,
        const std::string& destination)
    {
        (void)std::filesystem::copy_file(
            PathFromUtf8(source),
            PathFromUtf8(destination),
            std::filesystem::copy_options::overwrite_existing);
    }

    [[nodiscard]] bool EqualsAsciiKeyOrdinalIgnoreCase(
        std::string_view value, std::string_view key) noexcept
    {
        if (value.size() != key.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < key.size(); ++i)
        {
            unsigned char left = static_cast<unsigned char>(value[i]);
            unsigned char right = static_cast<unsigned char>(key[i]);
            if (left >= 'a' && left <= 'z')
            {
                left = static_cast<unsigned char>(left - ('a' - 'A'));
            }
            if (right >= 'a' && right <= 'z')
            {
                right = static_cast<unsigned char>(right - ('a' - 'A'));
            }
            if (left != right)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] const std::string* EntityValue(
        const Q3Entity* entity,
        std::string_view key)
    {
        if (entity == nullptr)
        {
            NullReference();
        }
        for (const auto& pair : *entity)
        {
            if (EqualsAsciiKeyOrdinalIgnoreCase(pair.first, key))
            {
                return std::addressof(pair.second);
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool EqualsOrdinalIgnoreCase(
        const std::string& left,
        const std::string& right) noexcept
    {
        return Q3StringEqual{}(left, right);
    }

    [[nodiscard]] bool StartsWithOrdinalIgnoreCase(
        const std::string& value,
        const std::string& prefix) noexcept
    {
        if (prefix.size() > value.size())
        {
            return false;
        }
        return Q3StringEqual{}(
            value.substr(0, prefix.size()),
            prefix);
    }

    [[nodiscard]] double AverageAxis(
        const std::vector<std::shared_ptr<std::vector<float>>>& values,
        std::size_t axis)
    {
        if (values.empty())
        {
            throw std::invalid_argument(
                "Sequence contains no elements");
        }

        double sum = 0.0;
        std::int64_t count = 0;
        for (const std::shared_ptr<std::vector<float>>& value : values)
        {
            sum += static_cast<double>(
                ArrayAt(Require(value), axis));
            if (count == std::numeric_limits<std::int64_t>::max())
            {
                throw std::overflow_error(
                    "Arithmetic operation resulted in an overflow.");
            }
            ++count;
        }
        return sum / static_cast<double>(count);
    }

    struct NumberSymbols final
    {
        std::string DecimalSeparator = ".";
        std::string GroupSeparator = ",";
        std::string NegativeSign = "-";
        std::string NaN = "NaN";
        std::string PositiveInfinity = "Infinity";
        std::string NegativeInfinity = "-Infinity";
        std::vector<std::int32_t> GroupSizes{3};
    };

#if defined(_WIN32)
    [[nodiscard]] std::string Utf16ToUtf8(
        const std::uint16_t* data, std::size_t length)
    {
        std::string result;
        for (std::size_t i = 0; i < length; ++i)
        {
            std::uint32_t scalar = data[i];
            if (scalar >= 0xD800U && scalar <= 0xDBFFU
                && i + 1 < length
                && data[i + 1] >= 0xDC00U
                && data[i + 1] <= 0xDFFFU)
            {
                scalar = 0x10000U
                    + ((scalar - 0xD800U) << 10)
                    + (data[++i] - 0xDC00U);
            }
            else if (scalar >= 0xD800U && scalar <= 0xDFFFU)
            {
                scalar = 0xFFFDU;
            }
            AppendUtf8(result, scalar);
        }
        return result;
    }

    [[nodiscard]] std::string WindowsLocaleString(
        LCTYPE type, const std::string& fallback)
    {
        wchar_t buffer[128]{};
        const int count = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT,
            type,
            buffer,
            static_cast<int>(std::size(buffer)));
        if (count <= 1)
        {
            return fallback;
        }

        std::vector<std::uint16_t> utf16;
        utf16.reserve(static_cast<std::size_t>(count - 1));
        for (int i = 0; i < count - 1; ++i)
        {
            utf16.push_back(
                static_cast<std::uint16_t>(buffer[i]));
        }
        return Utf16ToUtf8(utf16.data(), utf16.size());
    }

    [[nodiscard]] std::vector<std::int32_t> WindowsGroupingSizes(
        const std::string& text)
    {
        if (text.empty())
        {
            return {3};
        }
        if (text.front() == '0')
        {
            return {0};
        }

        std::vector<std::int32_t> result;
        std::size_t position = 0;
        while (position < text.size())
        {
            if (text[position] < '1' || text[position] > '9')
            {
                return {3};
            }
            result.push_back(text[position] - '0');
            ++position;
            if (position == text.size())
            {
                result.push_back(0);
                return result;
            }
            if (text[position] != ';')
            {
                return {3};
            }
            ++position;
            if (position == text.size())
            {
                return {3};
            }
            if (text[position] == '0')
            {
                return position + 1 == text.size()
                    ? result
                    : std::vector<std::int32_t>{3};
            }
        }
        return result.empty()
            ? std::vector<std::int32_t>{3}
            : result;
    }
#endif

    [[nodiscard]] NumberSymbols CurrentNumberSymbols() noexcept
    {
        NumberSymbols symbols;
        try
        {
#if defined(_WIN32)
            symbols.DecimalSeparator = WindowsLocaleString(
                LOCALE_SDECIMAL, symbols.DecimalSeparator);
            symbols.GroupSeparator = WindowsLocaleString(
                LOCALE_STHOUSAND, symbols.GroupSeparator);
            symbols.NegativeSign = WindowsLocaleString(
                LOCALE_SNEGATIVESIGN, symbols.NegativeSign);
#if defined(LOCALE_SNAN)
            symbols.NaN = WindowsLocaleString(
                LOCALE_SNAN, symbols.NaN);
#endif
#if defined(LOCALE_SPOSINFINITY)
            symbols.PositiveInfinity = WindowsLocaleString(
                LOCALE_SPOSINFINITY, symbols.PositiveInfinity);
#endif
#if defined(LOCALE_SNEGINFINITY)
            symbols.NegativeInfinity = WindowsLocaleString(
                LOCALE_SNEGINFINITY,
                symbols.NegativeSign + symbols.PositiveInfinity);
#else
            symbols.NegativeInfinity
                = symbols.NegativeSign + symbols.PositiveInfinity;
#endif
            symbols.GroupSizes = WindowsGroupingSizes(
                WindowsLocaleString(LOCALE_SGROUPING, "3;0"));
#else
            struct IcuNumbers final
            {
                using Open = void* (*)(
                    std::int32_t,
                    const std::uint16_t*,
                    std::int32_t,
                    const char*,
                    void*,
                    std::int32_t*);
                using GetSymbol = std::int32_t (*)(
                    const void*,
                    std::int32_t,
                    std::uint16_t*,
                    std::int32_t,
                    std::int32_t*);
                using GetAttribute = std::int32_t (*)(
                    const void*, std::int32_t);
                using Close = void (*)(void*);

                void* Library = nullptr;
                Open OpenFormatter = nullptr;
                GetSymbol Symbol = nullptr;
                GetAttribute Attribute = nullptr;
                Close CloseFormatter = nullptr;

                IcuNumbers() noexcept
                {
                    Library = dlopen(
                        "libicui18n.so",
                        RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
                    if (Library == nullptr)
                    {
                        Library = dlopen(
                            "/usr/lib/libicucore.A.dylib",
                            RTLD_LAZY | RTLD_LOCAL);
                    }
#endif
                    OpenFormatter = reinterpret_cast<Open>(
                        FindVersionedIcuSymbol(Library, "unum_open"));
                    Symbol = reinterpret_cast<GetSymbol>(
                        FindVersionedIcuSymbol(Library, "unum_getSymbol"));
                    Attribute = reinterpret_cast<GetAttribute>(
                        FindVersionedIcuSymbol(Library, "unum_getAttribute"));
                    CloseFormatter = reinterpret_cast<Close>(
                        FindVersionedIcuSymbol(Library, "unum_close"));
                }
            };
            static const IcuNumbers icu{};
            if (icu.OpenFormatter != nullptr
                && icu.Symbol != nullptr
                && icu.Attribute != nullptr
                && icu.CloseFormatter != nullptr)
            {
                std::int32_t status = 0;
                void* formatter = icu.OpenFormatter(
                    1,
                    nullptr,
                    0,
                    nullptr,
                    nullptr,
                    &status);
                if (formatter != nullptr && status <= 0)
                {
                    const auto readSymbol = [&](
                        std::int32_t symbol,
                        const std::string& fallback)
                    {
                        std::uint16_t buffer[128]{};
                        std::int32_t localStatus = 0;
                        const std::int32_t length = icu.Symbol(
                            formatter,
                            symbol,
                            buffer,
                            static_cast<std::int32_t>(
                                std::size(buffer)),
                            &localStatus);
                        if (localStatus > 0
                            || length < 0
                            || length
                                > static_cast<std::int32_t>(
                                    std::size(buffer)))
                        {
                            return fallback;
                        }
                        std::string result;
                        for (std::int32_t i = 0; i < length; ++i)
                        {
                            std::uint32_t scalar = buffer[i];
                            if (scalar >= 0xD800U
                                && scalar <= 0xDBFFU
                                && i + 1 < length
                                && buffer[i + 1] >= 0xDC00U
                                && buffer[i + 1] <= 0xDFFFU)
                            {
                                scalar = 0x10000U
                                    + ((scalar - 0xD800U) << 10)
                                    + (buffer[++i] - 0xDC00U);
                            }
                            else if (scalar >= 0xD800U
                                && scalar <= 0xDFFFU)
                            {
                                scalar = 0xFFFDU;
                            }
                            AppendUtf8(result, scalar);
                        }
                        return result;
                    };

                    symbols.DecimalSeparator = readSymbol(
                        0, symbols.DecimalSeparator);
                    symbols.GroupSeparator = readSymbol(
                        1, symbols.GroupSeparator);
                    symbols.NegativeSign = readSymbol(
                        6, symbols.NegativeSign);
                    symbols.PositiveInfinity = readSymbol(
                        14, symbols.PositiveInfinity);
                    symbols.NaN = readSymbol(
                        15, symbols.NaN);
                    symbols.NegativeInfinity
                        = symbols.NegativeSign
                        + symbols.PositiveInfinity;

                    const std::int32_t primary
                        = icu.Attribute(formatter, 10);
                    const std::int32_t secondary
                        = icu.Attribute(formatter, 15);
                    symbols.GroupSizes = secondary == 0
                        ? std::vector<std::int32_t>{primary}
                        : std::vector<std::int32_t>{
                            primary, secondary};
                    icu.CloseFormatter(formatter);
                    return symbols;
                }
                if (formatter != nullptr)
                {
                    icu.CloseFormatter(formatter);
                }
            }

            const std::locale locale("");
            const auto& punctuation
                = std::use_facet<std::numpunct<char>>(locale);
            symbols.DecimalSeparator.assign(
                1, punctuation.decimal_point());
            symbols.GroupSeparator.assign(
                1, punctuation.thousands_sep());
            const std::string grouping = punctuation.grouping();
            if (grouping.empty())
            {
                symbols.GroupSizes = {0};
            }
            else
            {
                symbols.GroupSizes.clear();
                for (unsigned char size : grouping)
                {
                    if (size == static_cast<unsigned char>(CHAR_MAX))
                    {
                        symbols.GroupSizes.push_back(0);
                        break;
                    }
                    if (size == 0)
                    {
                        break;
                    }
                    symbols.GroupSizes.push_back(
                        static_cast<std::int32_t>(size));
                }
                if (symbols.GroupSizes.empty())
                {
                    symbols.GroupSizes = {0};
                }
            }
#endif
        }
        catch (...)
        {
        }
        return symbols;
    }

    [[nodiscard]] std::string GroupDigits(
        std::string digits,
        const NumberSymbols& symbols)
    {
        if (digits.empty()
            || symbols.GroupSeparator.empty()
            || symbols.GroupSizes.empty()
            || symbols.GroupSizes.front() <= 0)
        {
            return digits;
        }

        std::size_t remaining = digits.size();
        std::size_t groupIndex = 0;
        std::int32_t groupSize = symbols.GroupSizes.front();
        while (groupSize > 0
            && remaining > static_cast<std::size_t>(groupSize))
        {
            remaining -= static_cast<std::size_t>(groupSize);
            digits.insert(remaining, symbols.GroupSeparator);
            if (groupIndex + 1 < symbols.GroupSizes.size())
            {
                ++groupIndex;
                groupSize = symbols.GroupSizes[groupIndex];
            }
        }
        return digits;
    }

    [[nodiscard]] std::string FormatN0(std::int64_t value)
    {
        const NumberSymbols symbols = CurrentNumberSymbols();
        std::string digits = std::to_string(value);
        bool negative = !digits.empty() && digits.front() == '-';
        if (negative)
        {
            digits.erase(digits.begin());
        }
        digits = GroupDigits(std::move(digits), symbols);
        return negative
            ? symbols.NegativeSign + digits
            : digits;
    }

    struct DecimalDigits final
    {
        std::string Digits;
        std::int32_t Scale = 0;
    };

    [[nodiscard]] DecimalDigits SevenSignificantDigits(
        float value)
    {
        char buffer[64]{};
        const auto conversion = std::to_chars(
            buffer,
            buffer + sizeof(buffer),
            std::fabs(value),
            std::chars_format::general,
            7);
        if (conversion.ec != std::errc{})
        {
            throw std::runtime_error(
                "Could not format Single value.");
        }

        std::string text(buffer, conversion.ptr);
        std::int32_t exponent = 0;
        const std::size_t exponentAt = text.find_first_of("eE");
        if (exponentAt != std::string::npos)
        {
            const std::string_view exponentText(
                text.data() + exponentAt + 1,
                text.size() - exponentAt - 1);
            const char* first = exponentText.data();
            const char* last = first + exponentText.size();
            const auto parsed = std::from_chars(
                first, last, exponent);
            if (parsed.ptr != last || parsed.ec != std::errc{})
            {
                throw std::runtime_error(
                    "Could not format Single value.");
            }
            text.resize(exponentAt);
        }

        const std::size_t decimalAt = text.find('.');
        const std::int32_t integerDigits = decimalAt
            == std::string::npos
            ? static_cast<std::int32_t>(text.size())
            : static_cast<std::int32_t>(decimalAt);
        if (decimalAt != std::string::npos)
        {
            text.erase(decimalAt, 1);
        }

        std::size_t firstNonzero = 0;
        while (firstNonzero < text.size()
            && text[firstNonzero] == '0')
        {
            ++firstNonzero;
        }
        if (firstNonzero == text.size())
        {
            return {};
        }

        DecimalDigits result;
        result.Scale = integerDigits
            + exponent
            - static_cast<std::int32_t>(firstNonzero);
        result.Digits = text.substr(firstNonzero);
        while (!result.Digits.empty()
            && result.Digits.back() == '0')
        {
            result.Digits.pop_back();
        }
        return result;
    }

    void RoundDecimalDigits(
        DecimalDigits& number, std::int32_t position)
    {
        std::int32_t i = 0;
        while (i < position
            && static_cast<std::size_t>(i)
                < number.Digits.size())
        {
            ++i;
        }

        if (i == position
            && i >= 0
            && static_cast<std::size_t>(i)
                < number.Digits.size()
            && number.Digits[static_cast<std::size_t>(i)] >= '5')
        {
            while (i > 0
                && number.Digits[
                    static_cast<std::size_t>(i - 1)] == '9')
            {
                --i;
            }
            if (i > 0)
            {
                ++number.Digits[
                    static_cast<std::size_t>(i - 1)];
                number.Digits.resize(
                    static_cast<std::size_t>(i));
            }
            else
            {
                ++number.Scale;
                number.Digits.assign(1, '1');
                i = 1;
            }
        }
        else
        {
            while (i > 0
                && number.Digits[
                    static_cast<std::size_t>(i - 1)] == '0')
            {
                --i;
            }
            if (i <= 0)
            {
                number.Digits.clear();
            }
            else
            {
                number.Digits.resize(
                    static_cast<std::size_t>(i));
            }
        }

        if (number.Digits.empty())
        {
            number.Scale = 0;
        }
    }

    [[nodiscard]] std::string FormatCustomFloat(
        float value,
        std::int32_t fractionalDigits,
        bool optionalFraction)
    {
        const NumberSymbols symbols = CurrentNumberSymbols();
        if (std::isnan(value))
        {
            return symbols.NaN;
        }
        if (std::isinf(value))
        {
            return std::signbit(value)
                ? symbols.NegativeInfinity
                : symbols.PositiveInfinity;
        }

        const bool negative = std::signbit(value);
        DecimalDigits number = SevenSignificantDigits(value);
        RoundDecimalDigits(
            number,
            number.Scale + fractionalDigits);

        const bool zero = number.Digits.empty();
        std::string result;
        if (number.Scale > 0)
        {
            result.reserve(
                static_cast<std::size_t>(number.Scale)
                + (fractionalDigits > 0 ? 4U : 0U));
            for (std::int32_t i = 0; i < number.Scale; ++i)
            {
                const std::size_t index
                    = static_cast<std::size_t>(i);
                result.push_back(
                    index < number.Digits.size()
                        ? number.Digits[index]
                        : '0');
            }
        }
        else
        {
            result = "0";
        }

        if (fractionalDigits > 0)
        {
            std::string fraction;
            fraction.reserve(
                static_cast<std::size_t>(fractionalDigits));
            for (std::int32_t place = 1;
                place <= fractionalDigits;
                ++place)
            {
                const std::int32_t digitIndex
                    = number.Scale + place - 1;
                char digit = '0';
                if (digitIndex >= 0
                    && static_cast<std::size_t>(digitIndex)
                        < number.Digits.size())
                {
                    digit = number.Digits[
                        static_cast<std::size_t>(digitIndex)];
                }
                fraction.push_back(digit);
            }
            if (optionalFraction)
            {
                while (!fraction.empty()
                    && fraction.back() == '0')
                {
                    fraction.pop_back();
                }
            }
            if (!fraction.empty())
            {
                result += symbols.DecimalSeparator;
                result += fraction;
            }
        }

        if (negative && !zero)
        {
            result.insert(0, symbols.NegativeSign);
        }
        return result;
    }

    [[nodiscard]] std::string FormatZero(float value)
    {
        return FormatCustomFloat(value, 0, false);
    }

    [[nodiscard]] std::string FormatZeroOptionalOne(float value)
    {
        return FormatCustomFloat(value, 1, true);
    }

    [[nodiscard]] std::string ItemTypeToString(ItemType value)
    {
        switch (value)
        {
        case ItemType::None: return "None";
        case ItemType::HealthMedium: return "HealthMedium";
        case ItemType::HealthSmall: return "HealthSmall";
        case ItemType::HealthBig: return "HealthBig";
        case ItemType::DoubleDamage: return "DoubleDamage";
        case ItemType::EnergyTank: return "EnergyTank";
        case ItemType::VoltDriver: return "VoltDriver";
        case ItemType::MissileExpansion: return "MissileExpansion";
        case ItemType::Battlehammer: return "Battlehammer";
        case ItemType::Imperialist: return "Imperialist";
        case ItemType::Judicator: return "Judicator";
        case ItemType::Magmaul: return "Magmaul";
        case ItemType::ShockCoil: return "ShockCoil";
        case ItemType::OmegaCannon: return "OmegaCannon";
        case ItemType::UASmall: return "UASmall";
        case ItemType::UABig: return "UABig";
        case ItemType::MissileSmall: return "MissileSmall";
        case ItemType::MissileBig: return "MissileBig";
        case ItemType::Cloak: return "Cloak";
        case ItemType::UAExpansion: return "UAExpansion";
        case ItemType::ArtifactKey: return "ArtifactKey";
        case ItemType::Deathalt: return "Deathalt";
        case ItemType::AffinityWeapon: return "AffinityWeapon";
        case ItemType::PickWpnMissile: return "PickWpnMissile";
        }
        return std::to_string(
            static_cast<std::int32_t>(value));
    }

    [[nodiscard]] std::string JoinMultiplayerItems(
        const MphRead::Mods::MapGen::ItemTypeHashSet& values)
    {
        std::string result;
        bool first = true;
        for (const ItemType value : values)
        {
            if (!first)
            {
                result += ", ";
            }
            first = false;
            result += ItemTypeToString(value);
        }
        return result;
    }

    [[nodiscard]] std::string JoinStrings(
        const std::vector<std::string>& values,
        std::size_t count)
    {
        std::string result;
        const std::size_t limit = std::min(count, values.size());
        for (std::size_t i = 0; i < limit; ++i)
        {
            if (i != 0)
            {
                result += ", ";
            }
            result += values[i];
        }
        return result;
    }
}

namespace MphRead::Mods::MapGen
{
    std::int32_t Q3Convert::Run(
        const std::optional<std::string>& source,
        const std::optional<std::string>& mapName,
        const std::optional<std::string>& roomName,
        const std::optional<std::string>& outputDir,
        bool dropClip,
        const std::optional<float>& forcedScale,
        std::int32_t textureSize)
    {
        if (!FileExists(source))
        {
            std::cout
                << "No such file: "
                << (source.has_value() ? *source : std::string())
                << '\n';
            return 1;
        }

        const std::string& sourceValue = *source;
        std::shared_ptr<Q3Bsp> bsp;
        try
        {
            bsp = Q3Bsp::Load(sourceValue, mapName);
        }
        catch (const std::exception& exception)
        {
            std::cout << exception.what() << '\n';
            return 1;
        }

        std::optional<std::string> selectedMapName = mapName;
        if (!selectedMapName.has_value())
        {
            const std::vector<std::string> maps
                = Q3Bsp::ListMaps(sourceValue);
            if (!maps.empty())
            {
                selectedMapName = maps.front();
            }
        }

        const std::string roomValue = roomName.has_value()
            ? *roomName
            : selectedMapName.has_value()
                ? *selectedMapName
                : "CUSTOM";
        const std::string room = InvariantCase(
            roomValue, true);
        const std::string prefix = InvariantCase(
            room, false);
        const std::string directory = outputDir.has_value()
            ? *outputDir
            : CombinePath(CustomRooms::MapDirectory(), prefix);
        CreateDirectory(directory);

        std::shared_ptr<std::vector<float>> min;
        std::shared_ptr<std::vector<float>> max;
        Bounds(Require(bsp), min, max, false);
        if (ArrayAt(min.get(), 0) > ArrayAt(max.get(), 0))
        {
            std::cout
                << (selectedMapName.has_value()
                    ? *selectedMapName
                    : std::string())
                << " has no drawn surfaces.\n";
            return 1;
        }

        const float xExtent
            = ArrayAt(max.get(), 0) - ArrayAt(min.get(), 0);
        const float yExtent
            = ArrayAt(max.get(), 1) - ArrayAt(min.get(), 1);
        const float zExtent
            = ArrayAt(max.get(), 2) - ArrayAt(min.get(), 2);
        const float widest = ManagedMax(
            xExtent,
            ManagedMax(yExtent, zExtent));
        const float unit = forcedScale.has_value()
            ? *forcedScale
            : RoundToEven(ManagedMax(
                35.0F,
                widest / TargetExtent));

        std::shared_ptr<std::vector<float>> reachMin;
        std::shared_ptr<std::vector<float>> reachMax;
        Bounds(Require(bsp), reachMin, reachMax, true);

        const std::string levelName = FileName(sourceValue);
        const std::string beside
            = CombinePath(directory, levelName);
        if (FullPath(beside) != FullPath(sourceValue))
        {
            CopyFile(sourceValue, beside);
        }

        const std::string texturePath
            = CombinePath(directory, prefix + ".tex");
        auto archivePaths = std::make_shared<
            std::vector<std::optional<std::string>>>();
        archivePaths->push_back(sourceValue);
        const std::shared_ptr<MapTextureBake::Result> baked
            = MapTextureBake::Bake(
                bsp,
                archivePaths,
                std::optional<std::string>(texturePath),
                textureSize);
        MapTextureBake::Result* bakedValue = Require(baked);
        const std::vector<std::string>* missing
            = Require(bakedValue->Missing);

        std::cout
            << "  " << bakedValue->Baked
            << " textures at "
            << textureSize << 'x' << textureSize
            << " -> " << FormatN0(bakedValue->Bytes)
            << " B  " << FileName(texturePath)
            << '\n';
        if (!missing->empty())
        {
            std::cout
                << "  no image for " << missing->size()
                << ": " << JoinStrings(*missing, 6)
                << (missing->size() > 6 ? " ..." : "")
                << '\n';
            std::cout
                << "  those surfaces are dropped rather than painted with somebody else's"
                << " texture; pass another .pk3 in the same folder if it has them\n";
        }

        auto definition = std::make_shared<MapDefinition>();
        definition->Name(room);
        definition->InGameName(
            roomName.has_value()
                ? roomName
                : selectedMapName.has_value()
                    ? selectedMapName
                    : std::optional<std::string>(room));
        definition->ScaleFactor(
            ScaleFactor(reachMin.get(), reachMax.get(), unit));
        definition->KillHeight(
            RoundToEven(ArrayAt(min.get(), 2) / unit)
            - 5.0F);
        definition->FarClip(
            RoundToEven(ManagedMin(
                400.0F,
                widest / unit * 1.2F)));

        auto import = std::make_shared<MapImport>();
        import->Source(levelName);
        import->MapName(selectedMapName);
        import->UnitsPerUnit(unit);
        import->Textures(FileName(texturePath));
        import->KeepSky(true);
        import->KeepClip(!dropClip);
        import->KeepSpawns(true);
        definition->Import(import);

        std::int32_t clipBrushes = 0;
        for (const std::shared_ptr<Q3Brush>& brushRef
            : Require(bsp)->Brushes())
        {
            Q3Brush* brush = Require(brushRef);
            Q3Texture* solidTexture = Require(
                ListAt(
                    Require(bsp)->Textures(),
                    brush->Texture()));
            if ((solidTexture->Contents()
                    & Q3Bsp::ContentsSolid) != 0)
            {
                continue;
            }
            Q3Texture* clipTexture = Require(
                ListAt(
                    Require(bsp)->Textures(),
                    brush->Texture()));
            if ((clipTexture->Contents()
                    & Q3Bsp::ContentsPlayerClip) != 0)
            {
                if (clipBrushes
                    == std::numeric_limits<std::int32_t>::max())
                {
                    throw std::overflow_error(
                        "Arithmetic operation resulted in an overflow.");
                }
                ++clipBrushes;
            }
        }

        AddSpawns(
            Require(definition),
            Require(bsp),
            unit);

        const std::string path
            = CombinePath(directory, prefix + ".json");
        definition->Save(path);

        MapDefinition::SpawnList* spawns
            = definition->Spawns();
        if (spawns == nullptr)
        {
            NullReference();
        }

        std::cout
            << "  " << spawns->size()
            << " spawn points, "
            << FormatZeroOptionalOne(unit)
            << " Quake units per unit"
            << " -> "
            << FormatZero(
                (ArrayAt(max.get(), 0)
                    - ArrayAt(min.get(), 0)) / unit)
            << " x "
            << FormatZero(
                (ArrayAt(max.get(), 2)
                    - ArrayAt(min.get(), 2)) / unit)
            << " x "
            << FormatZero(
                (ArrayAt(max.get(), 1)
                    - ArrayAt(min.get(), 1)) / unit)
            << " units\n";
        std::cout << "  wrote " << path << '\n';

        if (spawns->size() < 4)
        {
            std::cout
                << "  only " << spawns->size()
                << " places to appear: this level was not"
                << " built for a deathmatch. Add spawns to the map file before playing it with a full house.\n";
        }
        if (clipBrushes > 0 && !dropClip)
        {
            std::cout
                << "  " << clipBrushes
                << " player-clip brushes kept. They are the level's invisible"
                << " walls; on a race map they fence the route. -noclip converts without them.\n";
        }
        std::cout
            << "  no weapons or powerups were placed: where those go decides how the map"
            << " plays. Add them under \"items\", from:\n";
        std::cout
            << "  "
            << JoinMultiplayerItems(MapBuilder::MultiplayerItems)
            << '\n';
        std::cout
            << "  then: FruityPrime -mapgen \""
            << room
            << "\"\n";
        return 0;
    }

    void Q3Convert::Bounds(
        Q3Bsp* bsp,
        std::shared_ptr<std::vector<float>>& min,
        std::shared_ptr<std::vector<float>>& max,
        bool sky)
    {
        if (bsp == nullptr)
        {
            NullReference();
        }

        min = std::make_shared<std::vector<float>>(
            std::initializer_list<float>{
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()});
        max = std::make_shared<std::vector<float>>(
            std::initializer_list<float>{
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()});

        for (const std::shared_ptr<Q3Face>& faceRef
            : bsp->Faces())
        {
            Q3Face* face = Require(faceRef);
            if (face->Type() != 1
                && face->Type() != 2
                && face->Type() != 3)
            {
                continue;
            }

            Q3Texture* texture = Require(
                ListAt(bsp->Textures(), face->Texture()));
            if ((texture->Flags()
                    & (Q3Bsp::SurfaceNoDraw
                        | Q3Bsp::SurfaceHint
                        | Q3Bsp::SurfaceSkip)) != 0
                || (!sky
                    && (texture->Flags()
                        & Q3Bsp::SurfaceSky) != 0))
            {
                continue;
            }

            for (std::int32_t i = face->Vertex();
                i < WrapAdd(
                    face->Vertex(),
                    face->VertexCount());
                i = WrapAdd(i, 1))
            {
                Q3Vertex* vertex = Require(
                    ListAt(bsp->Vertices(), i));
                const std::vector<float>* position
                    = Require(vertex->Position());
                for (std::size_t axis = 0; axis < 3; ++axis)
                {
                    ArrayAt(min.get(), axis) = ManagedMin(
                        ArrayAt(min.get(), axis),
                        ArrayAt(position, axis));
                    ArrayAt(max.get(), axis) = ManagedMax(
                        ArrayAt(max.get(), axis),
                        ArrayAt(position, axis));
                }
            }
        }
    }

    std::int32_t Q3Convert::ScaleFactor(
        const std::vector<float>* min,
        const std::vector<float>* max,
        float unit)
    {
        float reach = 0.0F;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            reach = ManagedMax(
                reach,
                ManagedMax(
                    std::fabs(ArrayAt(min, axis)),
                    std::fabs(ArrayAt(max, axis)))
                    / unit);
        }

        std::int32_t factor = 0;
        while (8.0F
                * std::pow(
                    2.0F,
                    static_cast<float>(factor))
                < reach
            && factor < 10)
        {
            factor = WrapAdd(factor, 1);
        }
        return factor;
    }

    void Q3Convert::AddSpawns(
        MapDefinition* definition,
        Q3Bsp* bsp,
        float unit)
    {
        if (definition == nullptr || bsp == nullptr)
        {
            NullReference();
        }

        std::vector<std::shared_ptr<std::vector<float>>> starts;
        std::vector<std::shared_ptr<std::vector<float>>> fallbacks;

        for (const std::shared_ptr<Q3Entity>& entityRef
            : bsp->Entities())
        {
            const Q3Entity* entity = Require(entityRef);
            const std::string* classname
                = EntityValue(entity, "classname");
            const std::string* origin
                = EntityValue(entity, "origin");
            if (classname == nullptr || origin == nullptr)
            {
                continue;
            }

            std::shared_ptr<std::vector<float>> position
                = ParseVector(*origin);
            if (StartsWithOrdinalIgnoreCase(
                    *classname,
                    "info_player_deathmatch")
                || EqualsOrdinalIgnoreCase(
                    *classname,
                    "info_player_start"))
            {
                starts.push_back(std::move(position));
            }
            else if (StartsWithOrdinalIgnoreCase(
                    *classname,
                    "target_")
                || EqualsOrdinalIgnoreCase(
                    *classname,
                    "info_player_intermission"))
            {
                fallbacks.push_back(std::move(position));
            }
        }

        std::vector<std::shared_ptr<std::vector<float>>> chosen;
        if (starts.size() >= 4)
        {
            chosen = starts;
        }
        else
        {
            chosen.reserve(starts.size() + fallbacks.size());
            chosen.insert(
                chosen.end(),
                starts.begin(),
                starts.end());
            chosen.insert(
                chosen.end(),
                fallbacks.begin(),
                fallbacks.end());
        }

        MapImport* import = definition->Import();
        if (import == nullptr)
        {
            NullReference();
        }
        import->KeepSpawns(starts.size() >= 4);

        import = definition->Import();
        if (import == nullptr)
        {
            NullReference();
        }
        if (import->KeepSpawns())
        {
            return;
        }

        auto centre = std::make_shared<std::vector<float>>(
            std::initializer_list<float>{
                chosen.empty()
                    ? 0.0F
                    : static_cast<float>(
                        AverageAxis(chosen, 0)),
                chosen.empty()
                    ? 0.0F
                    : static_cast<float>(
                        AverageAxis(chosen, 1))});

        for (const std::shared_ptr<std::vector<float>>& positionRef
            : chosen)
        {
            const std::vector<float>* position
                = Require(positionRef);
            const float x = ArrayAt(position, 0) / unit;
            const float y = ArrayAt(position, 2) / unit
                - 24.0F / unit;
            const float z = -ArrayAt(position, 1) / unit;
            const float toCentre = std::atan2(
                ArrayAt(centre.get(), 0) / unit - x,
                -ArrayAt(centre.get(), 1) / unit - z);

            auto spawn = std::make_shared<MapSpawn>();
            spawn->Position(
                std::make_shared<std::vector<float>>(
                    std::initializer_list<float>{
                        Round(x),
                        Round(y),
                        Round(z)}));
            spawn->Yaw(
                Round(
                    toCentre
                    * 180.0F
                    / std::numbers::pi_v<float>));

            MapDefinition::SpawnList* spawns
                = definition->Spawns();
            if (spawns == nullptr)
            {
                NullReference();
            }
            spawns->push_back(std::move(spawn));
        }
    }

    float Q3Convert::Round(float value) noexcept
    {
        constexpr float Power = 100.0F;
        constexpr float RoundLimit = 1.0e8F;
        if (std::fabs(value) < RoundLimit)
        {
            return RoundToEven(value * Power) / Power;
        }
        return value;
    }

    std::shared_ptr<std::vector<float>> Q3Convert::ParseVector(
        const std::string& value)
    {
        auto result = std::make_shared<std::vector<float>>(
            3, 0.0F);

        std::vector<std::string_view> parts;
        std::size_t position = 0;
        while (position < value.size())
        {
            while (position < value.size()
                && value[position] == ' ')
            {
                ++position;
            }
            if (position >= value.size())
            {
                break;
            }
            const std::size_t start = position;
            while (position < value.size()
                && value[position] != ' ')
            {
                ++position;
            }
            parts.emplace_back(
                value.data() + start,
                position - start);
        }

        const std::size_t count
            = std::min<std::size_t>(3, parts.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            float parsed = 0.0F;
            (void)TryParseSingleInvariant(
                parts[i],
                parsed);
            ArrayAt(result.get(), i) = parsed;
        }
        return result;
    }
}
