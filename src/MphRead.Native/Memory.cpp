#include "Memory.hpp"

#include "Formats/Types.hpp"
#include "MemoryClasses.hpp"
#include "Program.hpp"
#include "Scene.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#else
#include <signal.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <libproc.h>
#include <sys/proc_info.h>
#else
#include <dirent.h>
#endif
#endif

namespace
{
    class DllNotFoundException final : public std::runtime_error
    {
    public:
        DllNotFoundException()
            : std::runtime_error("Unable to load shared library 'kernel32.dll'.")
        {
        }
    };

    class IndexOutOfRangeException final : public std::out_of_range
    {
    public:
        IndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };

    class ArgumentOutOfRangeException final : public std::out_of_range
    {
    public:
        explicit ArgumentOutOfRangeException(std::string parameter)
            : std::out_of_range(
                "Specified argument was out of the range of valid values. (Parameter '"
                + std::move(parameter) + "')")
        {
        }
    };

    class ArgumentException final : public std::invalid_argument
    {
    public:
        ArgumentException()
            : std::invalid_argument(
                "The array starting from the specified index is not long enough.")
        {
        }
    };

    class OverflowException final : public std::overflow_error
    {
    public:
        OverflowException()
            : std::overflow_error("Arithmetic operation resulted in an overflow.")
        {
        }
    };

    template <typename T>
    [[nodiscard]] T& ManagedArrayAt(
        MphRead::ManagedArray<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.Length())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ManagedArrayAt(
        const MphRead::ManagedArray<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.Length())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] T& VectorArrayAt(std::vector<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.size())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& VectorArrayAt(
        const std::vector<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.size())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] constexpr std::int32_t UncheckedAdd32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
            + std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] constexpr std::int32_t UncheckedSubtract32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
            - std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] constexpr std::int64_t UncheckedAdd64(
        std::int64_t left, std::int64_t right) noexcept
    {
        const std::uint64_t result = std::bit_cast<std::uint64_t>(left)
            + std::bit_cast<std::uint64_t>(right);
        return std::bit_cast<std::int64_t>(result);
    }

    [[nodiscard]] constexpr std::int64_t UncheckedSubtract64(
        std::int64_t left, std::int64_t right) noexcept
    {
        const std::uint64_t result = std::bit_cast<std::uint64_t>(left)
            - std::bit_cast<std::uint64_t>(right);
        return std::bit_cast<std::int64_t>(result);
    }

    [[nodiscard]] std::int32_t IntPtrToInt32(std::intptr_t value)
    {
        if (value < static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::min())
            || value > static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw OverflowException();
        }
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::intptr_t Int64ToIntPtr(std::int64_t value)
    {
        if constexpr (sizeof(std::intptr_t) < sizeof(std::int64_t))
        {
            if (value < static_cast<std::int64_t>(std::numeric_limits<std::intptr_t>::min())
                || value > static_cast<std::int64_t>(std::numeric_limits<std::intptr_t>::max()))
            {
                throw OverflowException();
            }
        }
        return static_cast<std::intptr_t>(value);
    }

    [[nodiscard]] constexpr bool IsNumberWhiteSpace(char value) noexcept
    {
        const unsigned char ch = static_cast<unsigned char>(value);
        return ch == 0x20U || (ch >= 0x09U && ch <= 0x0DU);
    }

    [[nodiscard]] constexpr std::int32_t HexDigitValue(char value) noexcept
    {
        if (value >= '0' && value <= '9')
        {
            return value - '0';
        }
        if (value >= 'A' && value <= 'F')
        {
            return value - 'A' + 10;
        }
        if (value >= 'a' && value <= 'f')
        {
            return value - 'a' + 10;
        }
        return -1;
    }

    [[nodiscard]] bool HasOnlyAllowedNumberSuffix(
        std::string_view text, std::size_t index) noexcept
    {
        while (index < text.size() && IsNumberWhiteSpace(text[index]))
        {
            ++index;
        }
        while (index < text.size() && text[index] == '\0')
        {
            ++index;
        }
        return index == text.size();
    }

    [[nodiscard]] bool EqualsAsciiIgnoreCase(
        std::string_view left, std::string_view right) noexcept
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

    [[nodiscard]] bool TryParseInt64Decimal(std::string_view text, std::int64_t& value)
    {
        value = 0;
        if (text.empty())
        {
            return false;
        }

        std::size_t index = 0;
        while (index < text.size() && IsNumberWhiteSpace(text[index]))
        {
            ++index;
        }
        if (index == text.size())
        {
            return false;
        }

        bool negative = false;
        if (text[index] == '+' || text[index] == '-')
        {
            negative = text[index] == '-';
            ++index;
        }
        if (index == text.size() || text[index] < '0' || text[index] > '9')
        {
            return false;
        }

        const std::size_t digitStart = index;
        while (index < text.size() && text[index] >= '0' && text[index] <= '9')
        {
            ++index;
        }
        const std::size_t digitEnd = index;
        if (!HasOnlyAllowedNumberSuffix(text, index))
        {
            return false;
        }

        std::size_t significantStart = digitStart;
        while (significantStart < digitEnd && text[significantStart] == '0')
        {
            ++significantStart;
        }
        if (significantStart == digitEnd)
        {
            value = 0;
            return true;
        }

        std::uint64_t magnitude = 0;
        const auto parsed = std::from_chars(
            text.data() + significantStart,
            text.data() + digitEnd,
            magnitude,
            10);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + digitEnd)
        {
            return false;
        }

        constexpr std::uint64_t minMagnitude = UINT64_C(0x8000000000000000);
        if (negative)
        {
            if (magnitude > minMagnitude)
            {
                return false;
            }
            value = magnitude == minMagnitude
                ? std::numeric_limits<std::int64_t>::min()
                : -static_cast<std::int64_t>(magnitude);
            return true;
        }

        if (magnitude > static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
            return false;
        }
        value = static_cast<std::int64_t>(magnitude);
        return true;
    }

    void ReplaceAll(std::string& value, std::string_view from, std::string_view to)
    {
        if (from.empty())
        {
            return;
        }
        std::size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos)
        {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    }

    [[nodiscard]] bool TryParseInt64Hex(std::string text, std::int64_t& value)
    {
        ReplaceAll(text, "0x", "");
        value = 0;
        if (text.empty())
        {
            return false;
        }

        std::size_t index = 0;
        while (index < text.size() && IsNumberWhiteSpace(text[index]))
        {
            ++index;
        }
        if (index == text.size() || HexDigitValue(text[index]) < 0)
        {
            return false;
        }

        while (index < text.size() && text[index] == '0')
        {
            ++index;
        }
        if (index == text.size())
        {
            value = 0;
            return true;
        }
        if (HexDigitValue(text[index]) < 0)
        {
            return HasOnlyAllowedNumberSuffix(text, index);
        }

        std::uint64_t bits = 0;
        std::size_t digitCount = 0;
        while (index < text.size())
        {
            const std::int32_t digit = HexDigitValue(text[index]);
            if (digit < 0)
            {
                break;
            }
            if (digitCount == 16)
            {
                return false;
            }
            bits = (bits << 4U) | static_cast<std::uint64_t>(digit);
            ++digitCount;
            ++index;
        }
        if (!HasOnlyAllowedNumberSuffix(text, index))
        {
            return false;
        }
        value = std::bit_cast<std::int64_t>(bits);
        return true;
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
        else
        {
            result.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
            result.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    [[nodiscard]] std::string DecodeTextFile(std::string_view bytes)
    {
        enum class Encoding
        {
            Utf8,
            Utf16Le,
            Utf16Be,
            Utf32Le,
            Utf32Be
        };

        Encoding encoding = Encoding::Utf8;
        std::size_t offset = 0;
        if (bytes.size() >= 4
            && static_cast<unsigned char>(bytes[0]) == 0x00
            && static_cast<unsigned char>(bytes[1]) == 0x00
            && static_cast<unsigned char>(bytes[2]) == 0xFE
            && static_cast<unsigned char>(bytes[3]) == 0xFF)
        {
            encoding = Encoding::Utf32Be;
            offset = 4;
        }
        else if (bytes.size() >= 4
            && static_cast<unsigned char>(bytes[0]) == 0xFF
            && static_cast<unsigned char>(bytes[1]) == 0xFE
            && static_cast<unsigned char>(bytes[2]) == 0x00
            && static_cast<unsigned char>(bytes[3]) == 0x00)
        {
            encoding = Encoding::Utf32Le;
            offset = 4;
        }
        else if (bytes.size() >= 3
            && static_cast<unsigned char>(bytes[0]) == 0xEF
            && static_cast<unsigned char>(bytes[1]) == 0xBB
            && static_cast<unsigned char>(bytes[2]) == 0xBF)
        {
            offset = 3;
        }
        else if (bytes.size() >= 2
            && static_cast<unsigned char>(bytes[0]) == 0xFF
            && static_cast<unsigned char>(bytes[1]) == 0xFE)
        {
            encoding = Encoding::Utf16Le;
            offset = 2;
        }
        else if (bytes.size() >= 2
            && static_cast<unsigned char>(bytes[0]) == 0xFE
            && static_cast<unsigned char>(bytes[1]) == 0xFF)
        {
            encoding = Encoding::Utf16Be;
            offset = 2;
        }

        if (encoding == Encoding::Utf8)
        {
            return std::string(bytes.substr(offset));
        }

        std::string result;
        auto read16 = [&](std::size_t index) -> std::uint16_t
        {
            const auto a = static_cast<unsigned char>(bytes[index]);
            const auto b = static_cast<unsigned char>(bytes[index + 1]);
            if (encoding == Encoding::Utf16Le)
            {
                return static_cast<std::uint16_t>(a | (static_cast<std::uint16_t>(b) << 8U));
            }
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(a) << 8U) | b);
        };
        auto read32 = [&](std::size_t index) -> std::uint32_t
        {
            const auto a = static_cast<unsigned char>(bytes[index]);
            const auto b = static_cast<unsigned char>(bytes[index + 1]);
            const auto c = static_cast<unsigned char>(bytes[index + 2]);
            const auto d = static_cast<unsigned char>(bytes[index + 3]);
            if (encoding == Encoding::Utf32Le)
            {
                return static_cast<std::uint32_t>(a)
                    | (static_cast<std::uint32_t>(b) << 8U)
                    | (static_cast<std::uint32_t>(c) << 16U)
                    | (static_cast<std::uint32_t>(d) << 24U);
            }
            return (static_cast<std::uint32_t>(a) << 24U)
                | (static_cast<std::uint32_t>(b) << 16U)
                | (static_cast<std::uint32_t>(c) << 8U)
                | static_cast<std::uint32_t>(d);
        };

        if (encoding == Encoding::Utf16Le || encoding == Encoding::Utf16Be)
        {
            while (offset + 1 < bytes.size())
            {
                const std::uint16_t first = read16(offset);
                offset += 2;
                char32_t codePoint = first;
                if (first >= 0xD800U && first <= 0xDBFFU)
                {
                    if (offset + 1 < bytes.size())
                    {
                        const std::uint16_t second = read16(offset);
                        if (second >= 0xDC00U && second <= 0xDFFFU)
                        {
                            offset += 2;
                            codePoint = 0x10000U
                                + ((static_cast<char32_t>(first) - 0xD800U) << 10U)
                                + (static_cast<char32_t>(second) - 0xDC00U);
                        }
                        else
                        {
                            codePoint = 0xFFFDU;
                        }
                    }
                    else
                    {
                        codePoint = 0xFFFDU;
                    }
                }
                else if (first >= 0xDC00U && first <= 0xDFFFU)
                {
                    codePoint = 0xFFFDU;
                }
                AppendUtf8(result, codePoint);
            }
            if (offset < bytes.size())
            {
                AppendUtf8(result, 0xFFFDU);
            }
            return result;
        }

        while (offset + 3 < bytes.size())
        {
            std::uint32_t scalar = read32(offset);
            offset += 4;
            if (scalar > 0x10FFFFU || (scalar >= 0xD800U && scalar <= 0xDFFFU))
            {
                scalar = 0xFFFDU;
            }
            AppendUtf8(result, static_cast<char32_t>(scalar));
        }
        if (offset < bytes.size())
        {
            AppendUtf8(result, 0xFFFDU);
        }
        return result;
    }

    [[nodiscard]] std::vector<std::string> ReadAllLines(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for reading.");
        }
        std::string bytes{
            std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        if (stream.bad())
        {
            throw std::ios_base::failure("Failed while reading file.");
        }

        const std::string text = DecodeTextFile(bytes);
        std::vector<std::string> result;
        std::size_t lineStart = 0;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\r' || text[i] == '\n')
            {
                result.emplace_back(text.substr(lineStart, i - lineStart));
                if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                {
                    ++i;
                }
                lineStart = i + 1;
            }
        }
        if (lineStart < text.size())
        {
            result.emplace_back(text.substr(lineStart));
        }
        return result;
    }

    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::trunc);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for writing.");
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::ios_base::failure("Failed while writing file.");
        }
    }

    void WriteAllLines(
        const std::filesystem::path& path, const std::array<std::string, 2>& lines)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for writing.");
        }
        for (const std::string& line : lines)
        {
#ifdef _WIN32
            stream << line << "\r\n";
#else
            stream << line << '\n';
#endif
        }
        if (!stream)
        {
            throw std::ios_base::failure("Failed while writing file.");
        }
    }

    [[nodiscard]] std::string FormatPointerHex(std::intptr_t value)
    {
        using UnsignedIntPtr = std::make_unsigned_t<std::intptr_t>;
        std::ostringstream stream;
        stream << "0x" << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2) << static_cast<UnsignedIntPtr>(value);
        return stream.str();
    }

    [[nodiscard]] std::locale CurrentLocale()
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

    [[nodiscard]] std::string FormatIntCurrentCulture(std::int32_t value)
    {
        std::ostringstream stream;
        stream.imbue(CurrentLocale());
        stream << value;
        return stream.str();
    }

    [[nodiscard]] std::string FormatWeightLine(
        std::int32_t weight, float percentage, std::string_view target)
    {
        std::ostringstream stream;
        stream.imbue(CurrentLocale());
        stream << "w: " << std::setw(6) << weight << " / 100000 ("
               << std::setw(5) << std::fixed << std::setprecision(1) << percentage
               << "%) -> " << target;
        return stream.str();
    }

    void AppendEnvironmentNewLine(std::string& value)
    {
#ifdef _WIN32
        value += "\r\n";
#else
        value.push_back('\n');
#endif
    }

    void ClearConsole()
    {
#ifdef _WIN32
        HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (output == nullptr || output == INVALID_HANDLE_VALUE)
        {
            return;
        }
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!::GetConsoleScreenBufferInfo(output, &info))
        {
            return;
        }
        const DWORD cells = static_cast<DWORD>(info.dwSize.X)
            * static_cast<DWORD>(info.dwSize.Y);
        DWORD written = 0;
        const COORD home{0, 0};
        ::FillConsoleOutputCharacterW(output, L' ', cells, home, &written);
        ::FillConsoleOutputAttribute(output, info.wAttributes, cells, home, &written);
        ::SetConsoleCursorPosition(output, home);
#else
        std::cout << "\x1B[2J\x1B[H";
#endif
    }

    struct ProcessCandidate final
    {
        std::int32_t Id = 0;
        std::int64_t StartTimeComparisonTicks = 0;
        std::int64_t StartTimeMilliseconds = 0;
    };

    constexpr std::int64_t TicksPerSecond = INT64_C(10000000);
    constexpr std::int64_t TicksPerMillisecond = INT64_C(10000);
    constexpr std::int64_t UnixEpochDateTimeTicks = INT64_C(621355968000000000);

    [[nodiscard]] std::int64_t UnixTicksToMilliseconds(std::int64_t ticks)
    {
        if (ticks >= 0)
        {
            return ticks / TicksPerMillisecond;
        }
        // DateTimeOffset.ToUnixTimeMilliseconds floors toward negative infinity.
        return -static_cast<std::int64_t>(
            (static_cast<std::uint64_t>(-(ticks + 1)) + 1U
                + static_cast<std::uint64_t>(TicksPerMillisecond - 1))
            / static_cast<std::uint64_t>(TicksPerMillisecond));
    }

#if !defined(_WIN32)
    [[nodiscard]] constexpr std::int64_t DaysFromCivil(
        std::int32_t year, std::uint32_t month, std::uint32_t day) noexcept
    {
        year -= month <= 2U ? 1 : 0;
        const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
        const std::uint32_t yearOfEra = static_cast<std::uint32_t>(
            year - static_cast<std::int32_t>(era * 400));
        const std::uint32_t dayOfYear = (153U * (month > 2U ? month - 3U : month + 9U) + 2U) / 5U
            + day - 1U;
        const std::uint32_t dayOfEra = yearOfEra * 365U + yearOfEra / 4U
            - yearOfEra / 100U + dayOfYear;
        return era * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468;
    }

    [[nodiscard]] std::int64_t UnixTicksToLocalDateTimeTicks(std::int64_t unixTicks)
    {
        std::int64_t seconds = unixTicks / TicksPerSecond;
        std::int64_t fractionTicks = unixTicks % TicksPerSecond;
        if (fractionTicks < 0)
        {
            fractionTicks += TicksPerSecond;
            --seconds;
        }

        const std::time_t time = static_cast<std::time_t>(seconds);
        std::tm local{};
        if (::localtime_r(&time, &local) == nullptr)
        {
            throw std::system_error(errno == 0 ? EOVERFLOW : errno,
                std::generic_category());
        }

        const std::int64_t daysSinceUnixEpoch = DaysFromCivil(
            local.tm_year + 1900,
            static_cast<std::uint32_t>(local.tm_mon + 1),
            static_cast<std::uint32_t>(local.tm_mday));
        const std::int64_t wholeSeconds =
            ((daysSinceUnixEpoch + INT64_C(719162)) * INT64_C(86400))
            + static_cast<std::int64_t>(local.tm_hour) * INT64_C(3600)
            + static_cast<std::int64_t>(local.tm_min) * INT64_C(60)
            + static_cast<std::int64_t>(local.tm_sec);
        return wholeSeconds * TicksPerSecond + fractionTicks;
    }
#endif

#ifdef _WIN32
    [[nodiscard]] std::int64_t FileTimeToUnixTicks(const FILETIME& time)
    {
        ULARGE_INTEGER value{};
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        constexpr std::uint64_t UnixEpochFileTime = UINT64_C(116444736000000000);
        if (value.QuadPart >= UnixEpochFileTime)
        {
            return static_cast<std::int64_t>(value.QuadPart - UnixEpochFileTime);
        }
        return -static_cast<std::int64_t>(UnixEpochFileTime - value.QuadPart);
    }

    [[nodiscard]] std::int64_t FileTimeLocalComparisonTicks(const FILETIME& utcTime)
    {
        FILETIME localTime{};
        if (!::FileTimeToLocalFileTime(&utcTime, &localTime))
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }
        ULARGE_INTEGER value{};
        value.LowPart = localTime.dwLowDateTime;
        value.HighPart = localTime.dwHighDateTime;
        if (value.QuadPart > static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
            return std::numeric_limits<std::int64_t>::max();
        }
        return static_cast<std::int64_t>(value.QuadPart);
    }

    [[nodiscard]] bool EqualsProcessName(std::wstring_view executable)
    {
        std::wstring name(executable);
        const std::size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos
            && _wcsicmp(name.substr(dot).c_str(), L".exe") == 0)
        {
            name.resize(dot);
        }
        return _wcsicmp(name.c_str(), L"NO$GBA") == 0;
    }

    [[nodiscard]] std::pair<std::int64_t, std::int64_t>
        QueryProcessStartTime(std::int32_t processId)
    {
        HANDLE handle = ::OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
            static_cast<DWORD>(processId));
        if (handle == nullptr)
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }

        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (!::GetProcessTimes(handle, &creation, &exit, &kernel, &user))
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(handle);
            throw std::system_error(static_cast<int>(error), std::system_category());
        }
        ::CloseHandle(handle);
        const std::int64_t unixTicks = FileTimeToUnixTicks(creation);
        return {
            FileTimeLocalComparisonTicks(creation),
            UnixTicksToMilliseconds(unixTicks)
        };
    }

    [[nodiscard]] std::vector<ProcessCandidate> FindProcessesByName()
    {
        HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }

        std::vector<std::int32_t> matched;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (::Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (EqualsProcessName(entry.szExeFile))
                {
                    matched.push_back(
                        static_cast<std::int32_t>(entry.th32ProcessID));
                }
            }
            while (::Process32NextW(snapshot, &entry));
        }
        else
        {
            const DWORD error = ::GetLastError();
            if (error != ERROR_NO_MORE_FILES)
            {
                ::CloseHandle(snapshot);
                throw std::system_error(static_cast<int>(error), std::system_category());
            }
        }
        ::CloseHandle(snapshot);

        // Process.GetProcessesByName completes before Memory.Start evaluates
        // Process.StartTime for any element of the returned array.
        std::vector<ProcessCandidate> result;
        result.reserve(matched.size());
        for (const std::int32_t processId : matched)
        {
            const auto [ticks, milliseconds] = QueryProcessStartTime(processId);
            result.push_back({processId, ticks, milliseconds});
        }
        return result;
    }
#elif defined(__linux__)
    struct LinuxProcessStat final
    {
        std::string Name;
        std::uint64_t StartTicks = 0;
    };

    [[nodiscard]] bool TryParsePid(std::string_view text, std::int32_t& processId)
    {
        if (text.empty())
        {
            return false;
        }
        std::uint32_t parsed = 0;
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), parsed, 10);
        if (result.ec != std::errc{}
            || result.ptr != text.data() + text.size()
            || parsed > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            return false;
        }
        processId = static_cast<std::int32_t>(parsed);
        return true;
    }

    [[nodiscard]] std::filesystem::path LinuxProcFilePath(
        std::int32_t processId, std::string_view fileName)
    {
        const std::filesystem::path processPath
            = processId == static_cast<std::int32_t>(::getpid())
            ? std::filesystem::path("/proc/self")
            : std::filesystem::path("/proc") / std::to_string(processId);
        return processPath / std::string(fileName);
    }

    [[nodiscard]] std::optional<LinuxProcessStat> TryReadLinuxProcessStat(
        std::int32_t processId)
    {
        const std::filesystem::path path
            = LinuxProcFilePath(processId, "stat");
        std::ifstream stream(path);
        if (!stream)
        {
            return std::nullopt;
        }
        std::string line;
        if (!std::getline(stream, line))
        {
            return std::nullopt;
        }

        const std::size_t openParen = line.find('(');
        const std::size_t closeParen = line.rfind(')');
        if (openParen == std::string::npos
            || closeParen == std::string::npos
            || closeParen <= openParen
            || closeParen + 2 >= line.size())
        {
            return std::nullopt;
        }

        LinuxProcessStat result{};
        result.Name = line.substr(openParen + 1, closeParen - openParen - 1);
        std::istringstream fields(line.substr(closeParen + 2));
        std::string field;
        for (std::int32_t fieldNumber = 3; fieldNumber <= 22; ++fieldNumber)
        {
            if (!(fields >> field))
            {
                return std::nullopt;
            }
            if (fieldNumber == 22)
            {
                const auto parsed = std::from_chars(
                    field.data(), field.data() + field.size(), result.StartTicks, 10);
                if (parsed.ec != std::errc{}
                    || parsed.ptr != field.data() + field.size())
                {
                    return std::nullopt;
                }
            }
        }
        return result;
    }

    [[nodiscard]] LinuxProcessStat ReadLinuxProcessStat(
        std::int32_t processId)
    {
        const std::optional<LinuxProcessStat> result
            = TryReadLinuxProcessStat(processId);
        if (!result)
        {
            throw std::ios_base::failure("Process information is unavailable.");
        }
        return *result;
    }

    [[nodiscard]] bool StartsWithAsciiIgnoreCase(
        std::string_view value, std::string_view prefix) noexcept
    {
        return value.size() >= prefix.size()
            && EqualsAsciiIgnoreCase(value.substr(0, prefix.size()), prefix);
    }

    [[nodiscard]] std::string LinuxProcessName(
        std::int32_t processId, const LinuxProcessStat& stat)
    {
        const std::filesystem::path path
            = LinuxProcFilePath(processId, "cmdline");
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return stat.Name;
        }

        std::string commandLine{
            std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        if (!stream.eof() && stream.fail())
        {
            return stat.Name;
        }

        std::size_t begin = 0;
        for (std::int32_t argument = 0; argument < 2 && begin <= commandLine.size(); ++argument)
        {
            const std::size_t end = commandLine.find('\0', begin);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string_view arg(commandLine.data() + begin, end - begin);
            const std::size_t slash = arg.find_last_of('/');
            const std::string_view name = slash == std::string_view::npos
                ? arg : arg.substr(slash + 1);
            if (StartsWithAsciiIgnoreCase(name, stat.Name))
            {
                return std::string(name);
            }
            begin = end + 1;
        }
        return stat.Name;
    }

    [[nodiscard]] bool LinuxProcMatchesPidNamespace()
    {
        std::array<char, 64> target{};
        const ssize_t length = ::readlink(
            "/proc/self", target.data(), target.size() - 1U);
        if (length <= 0)
        {
            return true;
        }
        std::int32_t procSelfPid = 0;
        if (!TryParsePid(
                std::string_view(target.data(), static_cast<std::size_t>(length)),
                procSelfPid))
        {
            return true;
        }
        return procSelfPid == static_cast<std::int32_t>(::getpid());
    }

    [[nodiscard]] std::int64_t LinuxBootTimeDateTimeTicks()
    {
        timespec boot{};
        if (::clock_gettime(CLOCK_BOOTTIME, &boot) != 0)
        {
            throw std::system_error(errno, std::generic_category());
        }
        const std::int64_t sinceBootTicks
            = static_cast<std::int64_t>(boot.tv_sec) * TicksPerSecond
            + static_cast<std::int64_t>(boot.tv_nsec) / INT64_C(100);

        timespec realtime{};
        if (::clock_gettime(CLOCK_REALTIME_COARSE, &realtime) != 0)
        {
            throw std::system_error(errno, std::generic_category());
        }
        const std::int64_t sinceEpochTicks
            = static_cast<std::int64_t>(realtime.tv_sec) * TicksPerSecond
            + static_cast<std::int64_t>(realtime.tv_nsec) / INT64_C(100);
        return UnixEpochDateTimeTicks + sinceEpochTicks - sinceBootTicks;
    }

    [[nodiscard]] std::int64_t LinuxJiffiesToTimeSpanTicks(std::uint64_t ticks)
    {
        static const long ticksPerSecond = []
        {
            const long value = ::sysconf(_SC_CLK_TCK);
            if (value <= 0)
            {
                throw std::system_error(errno == 0 ? EINVAL : errno,
                    std::generic_category());
            }
            return value;
        }();
        const double seconds = static_cast<double>(ticks)
            / static_cast<double>(ticksPerSecond);
        const double timeSpanTicks = seconds * static_cast<double>(TicksPerSecond);
        if (timeSpanTicks > static_cast<double>(
                std::numeric_limits<std::int64_t>::max())
            || timeSpanTicks < static_cast<double>(
                std::numeric_limits<std::int64_t>::min()))
        {
            throw OverflowException();
        }
        return static_cast<std::int64_t>(timeSpanTicks);
    }

    [[nodiscard]] std::pair<std::int64_t, std::int64_t>
        LinuxProcessStartTime(
            std::int32_t processId, std::int64_t bootTimeDateTimeTicks)
    {
        // Process.StartTime performs a fresh stat read after GetProcessesByName has
        // finished constructing its Process array.
        const LinuxProcessStat stat = ReadLinuxProcessStat(processId);
        const std::int64_t dateTimeTicks = UncheckedAdd64(
            bootTimeDateTimeTicks, LinuxJiffiesToTimeSpanTicks(stat.StartTicks));
        const std::int64_t unixTicks = UncheckedSubtract64(
            dateTimeTicks, UnixEpochDateTimeTicks);
        return {
            UnixTicksToLocalDateTimeTicks(unixTicks),
            UnixTicksToMilliseconds(unixTicks)
        };
    }

    [[nodiscard]] std::vector<std::int32_t> LinuxProcessIds()
    {
        if (!LinuxProcMatchesPidNamespace())
        {
            return {static_cast<std::int32_t>(::getpid())};
        }

        DIR* directory = ::opendir("/proc");
        if (directory == nullptr)
        {
            throw std::system_error(errno, std::generic_category());
        }
        std::vector<std::int32_t> result;
        while (dirent* entry = ::readdir(directory))
        {
            std::int32_t processId = 0;
            if (TryParsePid(entry->d_name, processId))
            {
                result.push_back(processId);
            }
        }
        ::closedir(directory);
        return result;
    }

    [[nodiscard]] std::vector<ProcessCandidate> FindProcessesByName()
    {
        // Process.GetProcessesByName first enumerates/builds the complete Process
        // array, then Memory.Start reads StartTime for each Process in that array.
        std::vector<std::int32_t> matched;
        for (const std::int32_t processId : LinuxProcessIds())
        {
            const std::optional<LinuxProcessStat> stat
                = TryReadLinuxProcessStat(processId);
            if (!stat)
            {
                continue;
            }
            const std::string name = LinuxProcessName(processId, *stat);
            if (!EqualsAsciiIgnoreCase(name, "NO$GBA"))
            {
                continue;
            }
            matched.push_back(processId);
        }

        std::vector<ProcessCandidate> result;
        if (matched.empty())
        {
            return result;
        }
        const std::int64_t bootTimeDateTimeTicks = LinuxBootTimeDateTimeTicks();
        for (const std::int32_t processId : matched)
        {
            const auto [comparisonTicks, milliseconds]
                = LinuxProcessStartTime(processId, bootTimeDateTimeTicks);
            result.push_back({processId, comparisonTicks, milliseconds});
        }
        return result;
    }
#elif defined(__APPLE__)
    [[nodiscard]] std::optional<std::string> AppleProcessName(std::int32_t processId)
    {
        std::array<char, PROC_PIDPATHINFO_MAXSIZE> path{};
        const int pathLength = ::proc_pidpath(
            processId, path.data(), static_cast<std::uint32_t>(path.size()));
        if (pathLength > 0)
        {
            std::filesystem::path executable(std::string(path.data(),
                static_cast<std::size_t>(pathLength)));
            const std::string name = executable.filename().string();
            if (!name.empty())
            {
                return name;
            }
        }

        proc_taskallinfo info{};
        const int bytes = ::proc_pidinfo(processId, PROC_PIDTASKALLINFO, 0,
            &info, static_cast<int>(sizeof(info)));
        if (bytes == static_cast<int>(sizeof(info)))
        {
            return std::string(info.pbsd.pbi_comm);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::pair<std::int64_t, std::int64_t>
        AppleProcessStartTime(std::int32_t processId)
    {
        proc_taskallinfo info{};
        const int bytes = ::proc_pidinfo(processId, PROC_PIDTASKALLINFO, 0,
            &info, static_cast<int>(sizeof(info)));
        if (bytes != static_cast<int>(sizeof(info)))
        {
            throw std::system_error(errno == 0 ? EIO : errno, std::generic_category());
        }

        const double seconds
            = static_cast<double>(info.pbsd.pbi_start_tvsec)
            + static_cast<double>(info.pbsd.pbi_start_tvusec) / 1000000.0;
        const double doubleTicks = seconds * static_cast<double>(TicksPerSecond);
        if (doubleTicks > static_cast<double>(
                std::numeric_limits<std::int64_t>::max())
            || doubleTicks < static_cast<double>(
                std::numeric_limits<std::int64_t>::min())
            || std::isnan(doubleTicks))
        {
            throw OverflowException();
        }
        // TimeSpan.FromSeconds ultimately truncates the scaled double to Int64.
        const std::int64_t unixTicks = static_cast<std::int64_t>(doubleTicks);
        return {
            UnixTicksToLocalDateTimeTicks(unixTicks),
            UnixTicksToMilliseconds(unixTicks)
        };
    }

    [[nodiscard]] std::vector<ProcessCandidate> FindProcessesByName()
    {
        int processCount = ::proc_listallpids(nullptr, 0);
        const bool sandboxFallback = processCount == 0 && errno == EPERM;
        if (processCount <= 0)
        {
            if (sandboxFallback)
            {
                processCount = 1;
            }
            else
            {
                throw std::system_error(errno == 0 ? EIO : errno,
                    std::generic_category());
            }
        }

        std::vector<pid_t> processIds;
        if (sandboxFallback)
        {
            processIds.push_back(::getpid());
        }
        else
        {
            for (;;)
            {
                const auto capacity = static_cast<std::size_t>(
                    static_cast<double>(processCount) * 1.10);
                processIds.assign(std::max<std::size_t>(capacity, 1), 0);
                processCount = ::proc_listallpids(
                    processIds.data(),
                    static_cast<int>(processIds.size() * sizeof(pid_t)));
                if (processCount <= 0)
                {
                    throw std::system_error(errno == 0 ? EIO : errno,
                        std::generic_category());
                }
                if (processCount != static_cast<int>(processIds.size()))
                {
                    processIds.resize(static_cast<std::size_t>(processCount));
                    break;
                }
            }
        }

        std::vector<std::int32_t> matched;
        for (pid_t pid : processIds)
        {
            if (pid < 0 || pid > std::numeric_limits<std::int32_t>::max())
            {
                continue;
            }
            const std::int32_t processId = static_cast<std::int32_t>(pid);
            const std::optional<std::string> name = AppleProcessName(processId);
            if (!name || !EqualsAsciiIgnoreCase(*name, "NO$GBA"))
            {
                continue;
            }
            matched.push_back(processId);
        }

        // As on the managed side, finish GetProcessesByName before evaluating
        // the cached StartTime property in Memory.Start's foreach loop.
        std::vector<ProcessCandidate> result;
        result.reserve(matched.size());
        for (const std::int32_t processId : matched)
        {
            const auto [ticks, milliseconds] = AppleProcessStartTime(processId);
            result.push_back({processId, ticks, milliseconds});
        }
        return result;
    }
#else
    [[nodiscard]] std::vector<ProcessCandidate> FindProcessesByName()
    {
        return {};
    }
#endif

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }
}

namespace MphRead::Memory
{
    std::shared_ptr<Memory::AddressInfo> Memory::Addresses{};

    const std::array<
        std::pair<std::string_view, std::shared_ptr<Memory::AddressInfo>>, 2>
        Memory::AllAddresses = {{
            {
                "a76e",
                std::make_shared<AddressInfo>(
                    0x020BC420,
                    0x020B85F8,
                    0x020AE514,
                    0x020B00D4,
                    0x020B00D4,
                    0x02103760,
                    0x020B84C4,
                    0x020E228C,
                    std::make_shared<AddressInfo::SaveAddressInfo>(
                        0x020BD798,
                        0x020D958C,
                        0x020BC364,
                        0x020EB948,
                        0x020ECEE0))
            },
            {
                "amhp1",
                std::make_shared<AddressInfo>(
                    0x020E845C,
                    0x020E3EE0,
                    0x020D94FC,
                    0x020DB034,
                    0x020DB180,
                    0x021335E0,
                    0x020B84C4,
                    0x020E228C,
                    std::make_shared<AddressInfo::SaveAddressInfo>(
                        0x020E97B0,
                        0x020D958C,
                        0x020E83B8,
                        0x020EB948,
                        0x020ECEE0))
            }
        }};

    Memory::AddressInfo::SaveAddressInfo::SaveAddressInfo(
        std::int32_t story,
        std::int32_t type3,
        std::int32_t settings,
        std::int32_t license,
        std::int32_t friends) noexcept
        : Story(story),
          Type3(type3),
          Settings(settings),
          License(license),
          Friends(friends)
    {
    }

    Memory::AddressInfo::AddressInfo(
        std::int32_t gameState,
        std::int32_t entityListHead,
        std::int32_t frameCount,
        std::int32_t players,
        std::int32_t playerUa,
        std::int32_t camSeqData,
        std::int32_t roomDesc,
        std::int32_t rng2,
        std::shared_ptr<SaveAddressInfo> save) noexcept
        : EntityListHead(entityListHead),
          FrameCount(frameCount),
          PlayerUA(playerUa),
          Players(players),
          CamSeqData(camSeqData),
          GameState(gameState),
          RoomDesc(roomDesc),
          Rng2(rng2),
          Save(std::move(save))
    {
    }

    Memory::Memory(
        std::int32_t processId,
        std::int64_t processStartTimeMilliseconds,
        ::MphRead::Scene* scene)
        : _aggroItems(
              std::make_shared<ManagedArray<std::shared_ptr<AIAggro>>>(25)),
          _scene(scene),
          _processId(processId),
          _processStartTimeMilliseconds(processStartTimeMilliseconds),
          _buffer(std::make_shared<ManagedArray<std::uint8_t>>(
              static_cast<std::size_t>(_size))),
          _players(
              std::make_shared<ManagedArray<std::shared_ptr<CPlayer>>>(4))
    {
    }

    Memory::~Memory()
    {
#ifdef _WIN32
        if (_processHandle != 0)
        {
            ::CloseHandle(reinterpret_cast<HANDLE>(_processHandle));
        }
#endif
    }

    std::shared_ptr<ManagedArray<std::uint8_t>> Memory::Buffer() const noexcept
    {
        return _buffer;
    }

    std::shared_ptr<std::shared_future<void>> Memory::Task() const noexcept
    {
        return _task;
    }

    std::shared_ptr<Memory> Memory::Start(::MphRead::Scene* scene, bool blocking)
    {
        std::optional<ProcessCandidate> foundProcess;
        // Process.StartTime is compared as a DateTime before the selected value is
        // later converted to Unix milliseconds for memory.txt.
        std::int64_t startTimeTicks = std::numeric_limits<std::int64_t>::min();
        const std::vector<ProcessCandidate> processes = FindProcessesByName();
        for (const ProcessCandidate& process : processes)
        {
            if (process.StartTimeComparisonTicks > startTimeTicks)
            {
                foundProcess = process;
                startTimeTicks = process.StartTimeComparisonTicks;
            }
        }
        if (!foundProcess)
        {
            throw ProgramException("Could not find process.");
        }

        auto memory = std::shared_ptr<Memory>(new Memory(
            foundProcess->Id, foundProcess->StartTimeMilliseconds, scene));
        memory->Run(blocking, memory);
        return memory;
    }

    void Memory::SetBaseAddress()
    {
        const std::filesystem::path path("memory.txt");
        std::error_code existsError;
        const bool exists = std::filesystem::is_regular_file(path, existsError);
        if (!exists || existsError)
        {
            WriteAllText(path, "");
        }

        const std::int64_t startTime = _processStartTimeMilliseconds;
        const std::vector<std::string> lines = ReadAllLines(path);
        std::int64_t timestamp = 0;
        std::int64_t saved = 0;
        if (lines.size() >= 2
            && TryParseInt64Decimal(lines[0], timestamp)
            && startTime == timestamp
            && TryParseInt64Hex(lines[1], saved))
        {
            _baseAddress = Int64ToIntPtr(saved);
            return;
        }

        std::cout << "Scanning memory..." << std::endl;

        const std::array<std::uint8_t, 12> search{
            0xFF, 0xDE, 0xFF, 0xE7,
            0xFF, 0xDE, 0xFF, 0xE7,
            0xFF, 0xDE, 0xFF, 0xE7
        };

        SystemInfo systemInfo{};
        GetSystemInfo(systemInfo);
        std::intptr_t minAddr = systemInfo.MinimumApplicationAddress;
        const std::intptr_t maxAddr = systemInfo.MaximumApplicationAddress;
        const std::intptr_t processHandle = OpenProcess(
            0x10 | 0x400, false, _processId);
        MemoryInfo64 memoryInfo{};

        while (static_cast<std::int64_t>(minAddr)
            < static_cast<std::int64_t>(maxAddr))
        {
            static_cast<void>(VirtualQueryEx(
                processHandle, minAddr, memoryInfo, 48));
            if (memoryInfo.Protect == 4 && memoryInfo.State == 0x1000)
            {
                if (memoryInfo.RegionSize < 0)
                {
                    throw std::overflow_error(
                        "Array dimensions exceeded supported range.");
                }

                std::vector<std::uint8_t> buffer(
                    static_cast<std::size_t>(memoryInfo.RegionSize));
                const std::intptr_t baseAddr = Int64ToIntPtr(memoryInfo.BaseAddress);
                std::intptr_t count = 0;
                const bool result = ReadProcessMemory(
                    processHandle,
                    baseAddr,
                    buffer.empty() ? nullptr : buffer.data(),
                    static_cast<std::int32_t>(
                        static_cast<std::uint32_t>(memoryInfo.RegionSize)),
                    count);
                assert(result);
                assert(static_cast<std::int64_t>(count) == memoryInfo.RegionSize);

                const std::int64_t lastStart
                    = memoryInfo.RegionSize - static_cast<std::int64_t>(search.size());
                for (std::int32_t i = 0;
                     static_cast<std::int64_t>(i) <= lastStart;
                     i = UncheckedAdd32(i, 1))
                {
                    bool equal = true;
                    for (std::size_t j = 0; j < search.size(); ++j)
                    {
                        if (buffer.at(static_cast<std::size_t>(i) + j) != search[j])
                        {
                            equal = false;
                            break;
                        }
                    }

                    if (equal)
                    {
                        const std::int32_t zeroIndex = UncheckedSubtract32(i, 0x4000);
                        const std::int32_t zeroIndex1 = UncheckedAdd32(zeroIndex, 1);
                        if (VectorArrayAt(buffer, zeroIndex) == 0
                            && VectorArrayAt(buffer, zeroIndex1) == 0)
                        {
                            const std::int64_t found = UncheckedAdd64(
                                memoryInfo.BaseAddress,
                                static_cast<std::int64_t>(zeroIndex));
                            _baseAddress = Int64ToIntPtr(found);
                            WriteAllLines(path, {
                                std::to_string(startTime),
                                FormatPointerHex(_baseAddress)
                            });
                            // The C# source never closes this OpenProcess handle.
                            static_cast<void>(processHandle);
                            return;
                        }
                    }
                }
            }

            if (memoryInfo.RegionSize == 0)
            {
                throw ProgramException("Failed to scan memory.");
            }
            minAddr = Int64ToIntPtr(UncheckedAdd64(
                static_cast<std::int64_t>(minAddr), memoryInfo.RegionSize));
        }

        throw ProgramException("Failed to find search sequence.");
    }

    void Memory::Run(bool blocking, std::shared_ptr<Memory> self)
    {
        std::shared_ptr<AddressInfo> addresses;
        for (const auto& item : AllAddresses)
        {
            if (item.first == "amhp1")
            {
                addresses = item.second;
                break;
            }
        }
        if (!addresses)
        {
            throw std::out_of_range("The given key was not present in the dictionary.");
        }
        Addresses = std::move(addresses);

        SetBaseAddress();

        auto promise = std::make_shared<std::promise<void>>();
        _task = std::make_shared<std::shared_future<void>>(
            promise->get_future().share());
        std::thread([self = std::move(self), promise = std::move(promise)]() mutable
        {
            try
            {
                self->RunTaskBody();
                promise->set_value();
            }
            catch (...)
            {
                promise->set_exception(std::current_exception());
            }
        }).detach();

        if (blocking)
        {
            _task->get();
        }
    }

    void Memory::RunTaskBody()
    {
        std::string output;
        RefreshMemory();
        (*_players)[0] = std::make_shared<CPlayer>(*this, Addresses->Players);
        (*_players)[1] = std::make_shared<CPlayer>(
            *this, UncheckedAdd32(Addresses->Players, 0xF30));
        (*_players)[2] = std::make_shared<CPlayer>(
            *this, UncheckedAdd32(
                Addresses->Players,
                static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(0xF30) * UINT32_C(2))));
        (*_players)[3] = std::make_shared<CPlayer>(
            *this, UncheckedAdd32(
                Addresses->Players,
                static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(0xF30) * UINT32_C(3))));

        while (_scene == nullptr || !_scene->Exiting())
        {
            _sb.clear();
            RefreshMemory();
            DoProcess();
            const std::string newOutput = _sb;
            if (newOutput != output)
            {
                output = newOutput;
                ClearConsole();
                std::cout << output;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }

    void Memory::RefreshMemory()
    {
        const std::intptr_t processHandle = ProcessHandle();
        std::uint8_t* const buffer = _buffer->Length() == 0
            ? nullptr
            : std::addressof((*_buffer)[0]);
        std::intptr_t count = 0;
        const bool result = ReadProcessMemory(
            processHandle,
            _baseAddress,
            buffer,
            _size,
            count);
        assert(result);
        assert(static_cast<std::int64_t>(count) == _size);
    }

    void Memory::DoProcess()
    {
        const std::uint32_t aggroCount
            = RequireReference((*_players)[1]).AggroCount();
        static_cast<void>(aggroCount);

        const auto list = RequireReference((*_players)[1]).AIAggro();
        assert(list != nullptr);
        if (!list)
        {
            throw System::NullReferenceException();
        }

        for (std::int32_t i = 0; i < list->Length(); ++i)
        {
            const std::shared_ptr<AIAggro> item = list->Item(i);
            RequireReference(item).UpdateSlots(_players);
            ManagedArrayAt(*_aggroItems, i) = item;
        }

        static_cast<void>(5);
    }

    void Memory::PrintAiContext()
    {
        const auto context = RequireReference((*_players)[1]).AIContext();
        assert(context != nullptr);
        if (!context)
        {
            throw System::NullReferenceException();
        }

        std::string tree;
        for (std::int32_t i = 1; i < 20; ++i)
        {
            const std::shared_ptr<AIContext> item = context->Item(i);
            const std::shared_ptr<AIData1> firstItemData1
                = RequireReference(item).AIData1();
            if (!firstItemData1)
            {
                break;
            }

            const std::shared_ptr<AIContext> parent
                = context->Item(UncheckedSubtract32(i, 1));
            std::int32_t childIndex = -1;

            for (std::int32_t j = 0;
                 j < RequireReference(
                     RequireReference(parent).AIData1()).Data1Count();
                 ++j)
            {
                const auto parentData1ForArray
                    = RequireReference(parent).AIData1();
                const auto parentData1Array
                    = RequireReference(parentData1ForArray).Data1();
                const std::shared_ptr<AIData1> parentChild
                    = RequireReference(parentData1Array).Item(j);

                const auto itemData1ForAddress
                    = RequireReference(item).AIData1();
                if (RequireReference(parentChild).Address()
                    == RequireReference(itemData1ForAddress).Address())
                {
                    childIndex = j;
                    break;
                }
            }

            if (!tree.empty())
            {
                tree += " -> ";
            }
            tree += FormatIntCurrentCulture(childIndex);
            _sb += "d";
            _sb += FormatIntCurrentCulture(i);
            _sb += ": ";
            _sb += FormatIntCurrentCulture(childIndex);
            AppendEnvironmentNewLine(_sb);

            const auto parentData1ForOptional
                = RequireReference(parent).AIData1();
            if (parentData1ForOptional
                && parentData1ForOptional->Data1Count() > 1)
            {
                for (std::int32_t k = 0;
                     k < RequireReference(
                         RequireReference(item).AIData1()).Data2Count();
                     ++k)
                {
                    const auto itemData1ForData2
                        = RequireReference(item).AIData1();
                    const auto data2
                        = RequireReference(itemData1ForData2).Data2();
                    std::int32_t index
                        = RequireReference(
                            RequireReference(data2).Item(k)).Data1SelectIdx();

                    std::string target;
                    if (index >= 20)
                    {
                        index = RequireReference(
                            RequireReference(parent).AIData1()).Data1Count();
                        target = "reset";
                    }
                    else
                    {
                        target = FormatIntCurrentCulture(index);
                    }

                    const std::int32_t weight
                        = RequireReference(RequireReference(parent).Weights()).Item(index);
                    const float percentage
                        = static_cast<float>(weight) / 100000.0F * 100.0F;
                    _sb += FormatWeightLine(weight, percentage, target);
                    AppendEnvironmentNewLine(_sb);
                }
            }

            if (RequireReference(
                    RequireReference(item).AIData1()).Data1Count() == 0)
            {
                break;
            }

            AppendEnvironmentNewLine(_sb);
            static_cast<void>(5);
        }

        const std::int32_t frameCount = ReadInt32FromBuffer(
            UncheckedSubtract32(Addresses->FrameCount, 0x02000000));
        bool add = _mem.empty();
        if (!add)
        {
            const std::string& last = _mem.back();
            const std::size_t separator = last.find(": ");
            const std::string previousTree = separator == std::string::npos
                ? std::string()
                : last.substr(separator + 2);
            add = previousTree != tree;
        }
        if (add)
        {
            _mem.push_back(
                FormatIntCurrentCulture(frameCount) + ": " + tree);
        }

        AppendEnvironmentNewLine(_sb);
        for (const std::string& line : _mem)
        {
            _sb += line;
            AppendEnvironmentNewLine(_sb);
        }
    }

    void Memory::GetEntities()
    {
        _temp.clear();
        for (const std::shared_ptr<CEntity>& entity : _entities)
        {
            CEntity& value = RequireReference(entity);
            const std::intptr_t address = value.Address();
            const auto [iterator, inserted] = _temp.emplace(address, entity);
            static_cast<void>(iterator);
            if (!inserted)
            {
                throw std::invalid_argument(
                    "An item with the same key has already been added.");
            }
        }

        _entities.clear();
        const std::shared_ptr<CEntity> head = GetEntity(Addresses->EntityListHead);
        assert(RequireReference(head).EntityType() == MphRead::EntityType::ListHead);
        _entities.push_back(head);

        std::intptr_t nextAddr = RequireReference(head).Next();
        while (nextAddr != RequireReference(head).Address())
        {
            std::shared_ptr<CEntity> entity;
            const auto cached = _temp.find(nextAddr);
            if (cached != _temp.end())
            {
                entity = cached->second;
            }

            if (entity
                && RequireReference(entity).EntityType()
                    == static_cast<MphRead::EntityType>(
                        ReadUInt16FromBuffer(UncheckedSubtract32(
                            IntPtrToInt32(nextAddr), Offset))))
            {
                _entities.push_back(entity);
            }
            else
            {
                entity = GetEntity(nextAddr);
                _entities.push_back(entity);
            }

            nextAddr = RequireReference(entity).Next();
        }
    }

    void Memory::WriteMemory(
        std::intptr_t address,
        std::shared_ptr<ManagedArray<std::uint8_t>> value,
        std::int32_t size)
    {
        WriteMemory(IntPtrToInt32(address), std::move(value), size);
    }

    void Memory::WriteMemory(
        std::int32_t address,
        std::shared_ptr<ManagedArray<std::uint8_t>> value,
        std::int32_t size)
    {
        const std::int32_t offset = UncheckedSubtract32(address, Offset);
        const std::int32_t pointerValue
            = UncheckedAdd32(IntPtrToInt32(_baseAddress), offset);
        const std::intptr_t pointer = static_cast<std::intptr_t>(pointerValue);
        const std::intptr_t processHandle = ProcessHandle();

        const std::uint8_t* source = nullptr;
        if (value && value->Length() != 0)
        {
            source = std::addressof((*value)[0]);
        }

        std::intptr_t count = 0;
        const bool result = WriteProcessMemory(
            processHandle, pointer, source, size, count);
        assert(result);
        assert(IntPtrToInt32(count) == size);

        for (std::int32_t i = 0; i < size; ++i)
        {
            const std::int32_t destinationIndex = UncheckedAdd32(offset, i);
            std::uint8_t& destination
                = ManagedArrayAt(*_buffer, destinationIndex);
            if (!value)
            {
                throw System::NullReferenceException();
            }
            destination = ManagedArrayAt(*value, i);
        }
    }

    std::shared_ptr<CEntity> Memory::GetEntity(std::intptr_t address)
    {
        return GetEntity(IntPtrToInt32(address));
    }

    std::shared_ptr<CEntity> Memory::GetEntity(std::int32_t address)
    {
        const std::int32_t offset = UncheckedSubtract32(address, Offset);
        const auto type = static_cast<MphRead::EntityType>(
            ReadUInt16FromBuffer(offset));

        if (type == MphRead::EntityType::Platform)
        {
            return std::make_shared<CPlatform>(*this, address);
        }
        if (type == MphRead::EntityType::Object)
        {
            return std::make_shared<CObject>(*this, address);
        }
        if (type == MphRead::EntityType::PlayerSpawn)
        {
            return std::make_shared<CPlayerSpawn>(*this, address);
        }
        if (type == MphRead::EntityType::Door)
        {
            return std::make_shared<CDoor>(*this, address);
        }
        if (type == MphRead::EntityType::ItemSpawn)
        {
            return std::make_shared<CItemSpawn>(*this, address);
        }
        if (type == MphRead::EntityType::ItemInstance)
        {
            return std::make_shared<CItemInstance>(*this, address);
        }
        if (type == MphRead::EntityType::EnemySpawn)
        {
            return std::make_shared<CEnemySpawn>(*this, address);
        }
        if (type == MphRead::EntityType::TriggerVolume)
        {
            return std::make_shared<CTriggerVolume>(*this, address);
        }
        if (type == MphRead::EntityType::AreaVolume)
        {
            return std::make_shared<CAreaVolume>(*this, address);
        }
        if (type == MphRead::EntityType::JumpPad)
        {
            return std::make_shared<CJumpPad>(*this, address);
        }
        if (type == MphRead::EntityType::PointModule)
        {
            return std::make_shared<CPointModule>(*this, address);
        }
        if (type == MphRead::EntityType::MorphCamera)
        {
            return std::make_shared<CMorphCamera>(*this, address);
        }
        if (type == MphRead::EntityType::OctolithFlag)
        {
            return std::make_shared<COctolithFlag>(*this, address);
        }
        if (type == MphRead::EntityType::FlagBase)
        {
            return std::make_shared<CFlagBase>(*this, address);
        }
        if (type == MphRead::EntityType::Teleporter)
        {
            return std::make_shared<CTeleporter>(*this, address);
        }
        if (type == MphRead::EntityType::NodeDefense)
        {
            return std::make_shared<CNodeDefense>(*this, address);
        }
        if (type == MphRead::EntityType::LightSource)
        {
            return std::make_shared<CLightSource>(*this, address);
        }
        if (type == MphRead::EntityType::Artifact)
        {
            return std::make_shared<CArtifact>(*this, address);
        }
        if (type == MphRead::EntityType::CameraSequence)
        {
            return std::make_shared<CCameraSequence>(*this, address);
        }
        if (type == MphRead::EntityType::ForceField)
        {
            return std::make_shared<CForceField>(*this, address);
        }
        if (type == MphRead::EntityType::BeamEffect)
        {
            return std::make_shared<CBeamEffect>(*this, address);
        }
        if (type == MphRead::EntityType::Bomb)
        {
            return std::make_shared<CBomb>(*this, address);
        }
        if (type == MphRead::EntityType::EnemyInstance)
        {
            const std::shared_ptr<CEnemyBase> enemy
                = std::make_shared<CEnemyBase>(*this, address);
            switch (RequireReference(enemy).Type())
            {
            case MphRead::EnemyType::Gorea1A:
                return std::make_shared<CEnemy24>(*this, address);
            case MphRead::EnemyType::GoreaHead:
                return std::make_shared<CEnemy25>(*this, address);
            case MphRead::EnemyType::GoreaArm:
                return std::make_shared<CEnemy26>(*this, address);
            case MphRead::EnemyType::GoreaLeg:
                return std::make_shared<CEnemy27>(*this, address);
            case MphRead::EnemyType::Gorea1B:
                return std::make_shared<CEnemy28>(*this, address);
            case MphRead::EnemyType::GoreaSealSphere1:
                return std::make_shared<CEnemy29>(*this, address);
            case MphRead::EnemyType::Trocra:
                return std::make_shared<CEnemy30>(*this, address);
            default:
                return enemy;
            }
        }
        if (type == MphRead::EntityType::Halfturret)
        {
            return std::make_shared<CHalfturret>(*this, address);
        }
        if (type == MphRead::EntityType::Player)
        {
            return std::make_shared<CPlayer>(*this, address);
        }
        if (type == MphRead::EntityType::BeamProjectile)
        {
            return std::make_shared<CBeamProjectile>(*this, address);
        }
        return std::make_shared<CEntity>(*this, address);
    }

    std::intptr_t Memory::ProcessHandle()
    {
#ifdef _WIN32
        if (_processHandle == 0)
        {
            HANDLE handle = ::OpenProcess(
                PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(_processId));
            if (handle == nullptr)
            {
                throw std::system_error(
                    static_cast<int>(::GetLastError()), std::system_category());
            }
            _processHandle = reinterpret_cast<std::intptr_t>(handle);
        }
        return _processHandle;
#else
        // System.Diagnostics.Process.Handle on Unix is a manufactured wait handle.
        // Preserve its pre-P/Invoke process-exit check; if the process is still
        // present, the exact handle bits are immaterial because the immediately
        // following kernel32.dll P/Invoke is the observable failure boundary.
        if (::kill(static_cast<pid_t>(_processId), 0) != 0 && errno == ESRCH)
        {
            throw std::runtime_error("Process has exited.");
        }
        return static_cast<std::intptr_t>(_processId == 0 ? 1 : _processId);
#endif
    }

    std::uint16_t Memory::ReadUInt16FromBuffer(std::int32_t offset) const
    {
        const std::int32_t length = static_cast<std::int32_t>(_buffer->Length());
        if (offset < 0 || offset >= length)
        {
            throw ArgumentOutOfRangeException("startIndex");
        }
        if (offset > length - 2)
        {
            throw ArgumentException();
        }

        const std::uint8_t b0 = (*_buffer)[static_cast<std::size_t>(offset)];
        const std::uint8_t b1
            = (*_buffer)[static_cast<std::size_t>(offset + 1)];
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(b0)
            | (static_cast<std::uint16_t>(b1) << 8U));
    }

    std::int32_t Memory::ReadInt32FromBuffer(std::int32_t offset) const
    {
        const std::int32_t length = static_cast<std::int32_t>(_buffer->Length());
        if (offset < 0 || offset >= length)
        {
            throw ArgumentOutOfRangeException("startIndex");
        }
        if (offset > length - 4)
        {
            throw ArgumentException();
        }

        const std::uint32_t b0 = (*_buffer)[static_cast<std::size_t>(offset)];
        const std::uint32_t b1 = (*_buffer)[static_cast<std::size_t>(offset + 1)];
        const std::uint32_t b2 = (*_buffer)[static_cast<std::size_t>(offset + 2)];
        const std::uint32_t b3 = (*_buffer)[static_cast<std::size_t>(offset + 3)];
        const std::uint32_t bits
            = b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
        return std::bit_cast<std::int32_t>(bits);
    }

    void Memory::GetSystemInfo(SystemInfo& lpSystemInfo)
    {
#ifdef _WIN32
        SYSTEM_INFO info{};
        ::GetSystemInfo(&info);
        lpSystemInfo.ProcessorArchitecture = info.wProcessorArchitecture;
        lpSystemInfo.Reserved = info.wReserved;
        lpSystemInfo.PageSize = info.dwPageSize;
        lpSystemInfo.MinimumApplicationAddress
            = reinterpret_cast<std::intptr_t>(info.lpMinimumApplicationAddress);
        lpSystemInfo.MaximumApplicationAddress
            = reinterpret_cast<std::intptr_t>(info.lpMaximumApplicationAddress);
        lpSystemInfo.ActiveProcessorMask
            = static_cast<std::intptr_t>(info.dwActiveProcessorMask);
        lpSystemInfo.NumberOfProcessors = info.dwNumberOfProcessors;
        lpSystemInfo.ProcessorType = info.dwProcessorType;
        lpSystemInfo.AllocationGranularity = info.dwAllocationGranularity;
        lpSystemInfo.ProcessorLevel = info.wProcessorLevel;
        lpSystemInfo.ProcessorRevision = info.wProcessorRevision;
#else
        static_cast<void>(lpSystemInfo);
        throw DllNotFoundException();
#endif
    }

    std::intptr_t Memory::OpenProcess(
        std::int32_t dwDesiredAccess,
        bool bInheritHandle,
        std::int32_t dwProcessId)
    {
#ifdef _WIN32
        HANDLE handle = ::OpenProcess(
            static_cast<DWORD>(dwDesiredAccess),
            bInheritHandle ? TRUE : FALSE,
            static_cast<DWORD>(dwProcessId));
        return reinterpret_cast<std::intptr_t>(handle);
#else
        static_cast<void>(dwDesiredAccess);
        static_cast<void>(bInheritHandle);
        static_cast<void>(dwProcessId);
        throw DllNotFoundException();
#endif
    }

    std::int32_t Memory::VirtualQueryEx(
        std::intptr_t hProcess,
        std::intptr_t lpAddress,
        MemoryInfo64& lpBuffer,
        std::uint32_t dwLength)
    {
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION info{};
        const SIZE_T result = ::VirtualQueryEx(
            reinterpret_cast<HANDLE>(hProcess),
            reinterpret_cast<LPCVOID>(lpAddress),
            &info,
            static_cast<SIZE_T>(dwLength));
        if (result != 0)
        {
            lpBuffer.BaseAddress = static_cast<std::int64_t>(
                reinterpret_cast<std::intptr_t>(info.BaseAddress));
            lpBuffer.AllocationBase = static_cast<std::int64_t>(
                reinterpret_cast<std::intptr_t>(info.AllocationBase));
            lpBuffer.AllocationProtect
                = static_cast<std::int32_t>(info.AllocationProtect);
            lpBuffer.Padding1 = 0;
            lpBuffer.RegionSize = static_cast<std::int64_t>(info.RegionSize);
            lpBuffer.State = static_cast<std::int32_t>(info.State);
            lpBuffer.Protect = static_cast<std::int32_t>(info.Protect);
            lpBuffer.lType = static_cast<std::int32_t>(info.Type);
            lpBuffer.Padding2 = 0;
        }
        return static_cast<std::int32_t>(result);
#else
        static_cast<void>(hProcess);
        static_cast<void>(lpAddress);
        static_cast<void>(lpBuffer);
        static_cast<void>(dwLength);
        throw DllNotFoundException();
#endif
    }

    bool Memory::ReadProcessMemory(
        std::intptr_t hProcess,
        std::intptr_t lpBaseAddress,
        std::uint8_t* lpBuffer,
        std::int32_t nSize,
        std::intptr_t& lpNumberOfBytesRead)
    {
#ifdef _WIN32
        SIZE_T count = 0;
        const BOOL result = ::ReadProcessMemory(
            reinterpret_cast<HANDLE>(hProcess),
            reinterpret_cast<LPCVOID>(lpBaseAddress),
            lpBuffer,
            static_cast<SIZE_T>(static_cast<std::uint32_t>(nSize)),
            &count);
        lpNumberOfBytesRead = static_cast<std::intptr_t>(count);
        return result != FALSE;
#else
        static_cast<void>(hProcess);
        static_cast<void>(lpBaseAddress);
        static_cast<void>(lpBuffer);
        static_cast<void>(nSize);
        static_cast<void>(lpNumberOfBytesRead);
        throw DllNotFoundException();
#endif
    }

    bool Memory::WriteProcessMemory(
        std::intptr_t hProcess,
        std::intptr_t lpBaseAddress,
        const std::uint8_t* lpBuffer,
        std::int32_t nSize,
        std::intptr_t& lpNumberOfBytesRead)
    {
#ifdef _WIN32
        SIZE_T count = 0;
        const BOOL result = ::WriteProcessMemory(
            reinterpret_cast<HANDLE>(hProcess),
            reinterpret_cast<LPVOID>(lpBaseAddress),
            lpBuffer,
            static_cast<SIZE_T>(static_cast<std::uint32_t>(nSize)),
            &count);
        lpNumberOfBytesRead = static_cast<std::intptr_t>(count);
        return result != FALSE;
#else
        static_cast<void>(hProcess);
        static_cast<void>(lpBaseAddress);
        static_cast<void>(lpBuffer);
        static_cast<void>(nSize);
        static_cast<void>(lpNumberOfBytesRead);
        throw DllNotFoundException();
#endif
    }

    std::uint32_t Memory::GetLastError()
    {
#ifdef _WIN32
        return static_cast<std::uint32_t>(::GetLastError());
#else
        throw DllNotFoundException();
#endif
    }
}
