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

    [[nodiscard]] std::string_view TrimFloatWhitespace(
        std::string_view value) noexcept
    {
        while (!value.empty()
            && IsAsciiWhitespace(static_cast<unsigned char>(value.front())))
        {
            value.remove_prefix(1);
        }
        while (!value.empty()
            && IsAsciiWhitespace(static_cast<unsigned char>(value.back())))
        {
            value.remove_suffix(1);
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

    [[nodiscard]] bool TryParseSingleInvariant(
        std::string_view text, float& result) noexcept
    {
        result = 0.0F;
        text = TrimFloatWhitespace(text);
        if (text.empty())
        {
            return false;
        }

        if (EqualsIgnoreCaseAscii(text, "NaN"))
        {
            result = std::numeric_limits<float>::quiet_NaN();
            return true;
        }
        if (EqualsIgnoreCaseAscii(text, "Infinity")
            || EqualsIgnoreCaseAscii(text, "+Infinity"))
        {
            result = std::numeric_limits<float>::infinity();
            return true;
        }
        if (EqualsIgnoreCaseAscii(text, "-Infinity"))
        {
            result = -std::numeric_limits<float>::infinity();
            return true;
        }

        bool negative = false;
        if (!IsFloatNumber(text, negative))
        {
            return false;
        }

        std::string_view magnitude = text;
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

        if (DecimalIsBelowOne(text))
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
        if (scalar <= 0xFFFFU)
        {
            const wchar_t source = static_cast<wchar_t>(scalar);
            wchar_t target = source;
            const DWORD flag = upper ? LCMAP_UPPERCASE : LCMAP_LOWERCASE;
            if (LCMapStringEx(
                    LOCALE_NAME_INVARIANT,
                    flag,
                    &source,
                    1,
                    &target,
                    1,
                    nullptr,
                    nullptr,
                    0) == 1)
            {
                return static_cast<std::uint32_t>(target);
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

    [[nodiscard]] std::string FullPath(
        const std::string& path)
    {
        if (path.empty())
        {
            throw std::invalid_argument(
                "The value cannot be an empty string. (Parameter 'path')");
        }
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
            if (pair.first == key)
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

    [[nodiscard]] std::string FormatN0(std::int64_t value)
    {
        std::string result = std::to_string(value);
        const std::size_t start = !result.empty()
            && result.front() == '-'
            ? 1U
            : 0U;
        for (std::size_t position = result.size();
            position > start + 3U;)
        {
            position -= 3U;
            result.insert(position, 1, ',');
        }
        return result;
    }

    [[nodiscard]] std::string SpecialFloat(float value)
    {
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value)
                ? "-Infinity"
                : "Infinity";
        }
        return {};
    }

    [[nodiscard]] bool IsFormattedZero(
        std::string_view value) noexcept
    {
        std::size_t position = 0;
        if (position < value.size()
            && (value[position] == '+' || value[position] == '-'))
        {
            ++position;
        }
        bool digit = false;
        for (; position < value.size(); ++position)
        {
            const char ch = value[position];
            if (ch == '.')
            {
                continue;
            }
            if (ch < '0' || ch > '9')
            {
                return false;
            }
            digit = true;
            if (ch != '0')
            {
                return false;
            }
        }
        return digit;
    }

    [[nodiscard]] std::string FormatFixed(
        float value, std::int32_t decimals)
    {
        if (const std::string special = SpecialFloat(value);
            !special.empty())
        {
            return special;
        }

        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::fixed
            << std::setprecision(decimals)
            << value;
        std::string result = stream.str();
        if (result.size() > 1U
            && result.front() == '-'
            && IsFormattedZero(result))
        {
            result.erase(result.begin());
        }
        return result;
    }

    [[nodiscard]] std::string FormatZero(float value)
    {
        return FormatFixed(value, 0);
    }

    [[nodiscard]] std::string FormatZeroOptionalOne(float value)
    {
        std::string result = FormatFixed(value, 1);
        if (result.size() >= 2U
            && result[result.size() - 2U] == '.'
            && result.back() == '0')
        {
            result.resize(result.size() - 2U);
        }
        return result;
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
