#include "NetLag.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif
#else
#include <cerrno>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <unistd.h>
#endif

namespace
{
    constexpr std::array<std::string_view, 25> DotNetWhiteSpaceUtf8{
        "\x09", "\x0A", "\x0B", "\x0C", "\x0D", "\x20",
        "\xC2\x85", "\xC2\xA0", "\xE1\x9A\x80",
        "\xE2\x80\x80", "\xE2\x80\x81", "\xE2\x80\x82", "\xE2\x80\x83",
        "\xE2\x80\x84", "\xE2\x80\x85", "\xE2\x80\x86", "\xE2\x80\x87",
        "\xE2\x80\x88", "\xE2\x80\x89", "\xE2\x80\x8A",
        "\xE2\x80\xA8", "\xE2\x80\xA9", "\xE2\x80\xAF",
        "\xE2\x81\x9F", "\xE3\x80\x80"
    };

    [[nodiscard]] std::string_view TrimDotNetWhiteSpace(std::string_view value) noexcept
    {
        bool removed = true;
        while (removed && !value.empty())
        {
            removed = false;
            for (const std::string_view whiteSpace : DotNetWhiteSpaceUtf8)
            {
                if (value.starts_with(whiteSpace))
                {
                    value.remove_prefix(whiteSpace.size());
                    removed = true;
                    break;
                }
            }
        }

        removed = true;
        while (removed && !value.empty())
        {
            removed = false;
            for (const std::string_view whiteSpace : DotNetWhiteSpaceUtf8)
            {
                if (value.ends_with(whiteSpace))
                {
                    value.remove_suffix(whiteSpace.size());
                    removed = true;
                    break;
                }
            }
        }
        return value;
    }

    [[nodiscard]] bool IsNullOrWhiteSpaceLikeDotNet(
        const std::optional<std::string>& value) noexcept
    {
        return !value.has_value() || TrimDotNetWhiteSpace(*value).empty();
    }

    [[nodiscard]] constexpr bool IsNumberWhite(char value) noexcept
    {
        const unsigned char unit = static_cast<unsigned char>(value);
        return unit == 0x20U || (unit >= 0x09U && unit <= 0x0DU);
    }

    [[nodiscard]] bool EqualsOrdinalIgnoreCaseAscii(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); index++)
        {
            unsigned char a = static_cast<unsigned char>(left[index]);
            unsigned char b = static_cast<unsigned char>(right[index]);
            if (a >= 0x80U || b >= 0x80U)
            {
                return false;
            }
            if (a >= static_cast<unsigned char>('A') && a <= static_cast<unsigned char>('Z'))
            {
                a = static_cast<unsigned char>(a + ('a' - 'A'));
            }
            if (b >= static_cast<unsigned char>('A') && b <= static_cast<unsigned char>('Z'))
            {
                b = static_cast<unsigned char>(b + ('a' - 'A'));
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool TryParseInt32InvariantInteger(
        std::string_view value, std::int32_t& parsed) noexcept
    {
        parsed = 0;
        std::size_t index = 0;
        while (index < value.size() && IsNumberWhite(value[index]))
        {
            index++;
        }
        if (index == value.size())
        {
            return false;
        }

        bool negative = false;
        if (value[index] == '+' || value[index] == '-')
        {
            negative = value[index] == '-';
            index++;
        }
        if (index == value.size())
        {
            return false;
        }

        constexpr std::uint64_t PositiveLimit =
            static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
        constexpr std::uint64_t NegativeLimit = PositiveLimit + 1U;
        const std::uint64_t limit = negative ? NegativeLimit : PositiveLimit;
        std::uint64_t magnitude = 0;
        bool anyDigit = false;
        while (index < value.size() && value[index] >= '0' && value[index] <= '9')
        {
            anyDigit = true;
            const std::uint64_t digit = static_cast<std::uint64_t>(value[index] - '0');
            if (magnitude > (limit - digit) / 10U)
            {
                return false;
            }
            magnitude = magnitude * 10U + digit;
            index++;
        }
        if (!anyDigit)
        {
            return false;
        }

        if (index < value.size() && IsNumberWhite(value[index]))
        {
            do
            {
                index++;
            }
            while (index < value.size() && IsNumberWhite(value[index]));
        }
        while (index < value.size() && value[index] == '\0')
        {
            index++;
        }
        if (index != value.size())
        {
            return false;
        }

        if (!negative)
        {
            parsed = static_cast<std::int32_t>(magnitude);
        }
        else if (magnitude == NegativeLimit)
        {
            parsed = std::numeric_limits<std::int32_t>::min();
        }
        else
        {
            parsed = -static_cast<std::int32_t>(magnitude);
        }
        return true;
    }

    [[nodiscard]] bool ExtractInvariantFloatCore(
        std::string_view value, bool& negative, std::string_view& core) noexcept
    {
        std::size_t index = 0;
        while (index < value.size() && IsNumberWhite(value[index]))
        {
            index++;
        }
        if (index == value.size())
        {
            return false;
        }

        negative = false;
        if (value[index] == '+' || value[index] == '-')
        {
            negative = value[index] == '-';
            index++;
        }
        const std::size_t coreStart = index;

        bool anyDigit = false;
        while (index < value.size() && value[index] >= '0' && value[index] <= '9')
        {
            anyDigit = true;
            index++;
        }
        if (index < value.size() && value[index] == '.')
        {
            index++;
            while (index < value.size() && value[index] >= '0' && value[index] <= '9')
            {
                anyDigit = true;
                index++;
            }
        }
        if (!anyDigit)
        {
            return false;
        }
        if (index < value.size() && (value[index] == 'e' || value[index] == 'E'))
        {
            index++;
            if (index < value.size() && (value[index] == '+' || value[index] == '-'))
            {
                index++;
            }
            const std::size_t exponentStart = index;
            while (index < value.size() && value[index] >= '0' && value[index] <= '9')
            {
                index++;
            }
            if (index == exponentStart)
            {
                return false;
            }
        }
        const std::size_t coreEnd = index;

        if (index < value.size() && IsNumberWhite(value[index]))
        {
            do
            {
                index++;
            }
            while (index < value.size() && IsNumberWhite(value[index]));
        }
        while (index < value.size() && value[index] == '\0')
        {
            index++;
        }
        if (index != value.size())
        {
            return false;
        }

        core = value.substr(coreStart, coreEnd - coreStart);
        return true;
    }

    [[nodiscard]] bool MantissaIsZero(std::string_view value) noexcept
    {
        for (const char unit : value)
        {
            if (unit == 'e' || unit == 'E' || unit == '\0')
            {
                break;
            }
            if (unit >= '1' && unit <= '9')
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] int CompareDecimalMagnitudeToSize(
        std::string_view digits, std::size_t threshold) noexcept
    {
        while (digits.size() > 1 && digits.front() == '0')
        {
            digits.remove_prefix(1);
        }

        std::array<char, 3 * sizeof(std::size_t)> thresholdBuffer{};
        const auto converted = std::to_chars(
            thresholdBuffer.data(), thresholdBuffer.data() + thresholdBuffer.size(), threshold);
        const std::string_view thresholdText(
            thresholdBuffer.data(), static_cast<std::size_t>(converted.ptr - thresholdBuffer.data()));

        if (digits.size() < thresholdText.size())
        {
            return -1;
        }
        if (digits.size() > thresholdText.size())
        {
            return 1;
        }
        const int comparison = digits.compare(thresholdText);
        return comparison < 0 ? -1 : (comparison > 0 ? 1 : 0);
    }

    [[nodiscard]] bool OutOfRangeIsOverflow(std::string_view value) noexcept
    {
        const std::size_t exponentMarker = value.find_first_of("eE");
        const std::string_view mantissa = exponentMarker == std::string_view::npos
            ? value
            : value.substr(0, exponentMarker);
        const std::size_t decimal = mantissa.find('.');
        const std::size_t integerDigits = decimal == std::string_view::npos
            ? mantissa.size()
            : decimal;

        std::size_t digitIndex = 0;
        std::size_t firstSignificantDigit = std::string_view::npos;
        for (const char unit : mantissa)
        {
            if (unit == '.')
            {
                continue;
            }
            if (unit >= '1' && unit <= '9' && firstSignificantDigit == std::string_view::npos)
            {
                firstSignificantDigit = digitIndex;
            }
            digitIndex++;
        }

        // MantissaIsZero has already handled the all-zero case. Express the
        // first significant digit's decimal exponent as sign + magnitude so
        // even an arbitrarily long exponent text cannot overflow a C++ integer.
        const bool baseNonNegative = integerDigits > firstSignificantDigit;
        const std::size_t baseMagnitude = baseNonNegative
            ? integerDigits - firstSignificantDigit - 1
            : firstSignificantDigit - integerDigits + 1;

        if (exponentMarker == std::string_view::npos)
        {
            return baseNonNegative;
        }

        std::size_t index = exponentMarker + 1;
        bool exponentNegative = false;
        if (value[index] == '+' || value[index] == '-')
        {
            exponentNegative = value[index] == '-';
            index++;
        }
        std::string_view exponentDigits = value.substr(index);
        while (exponentDigits.size() > 1 && exponentDigits.front() == '0')
        {
            exponentDigits.remove_prefix(1);
        }

        if (!exponentNegative)
        {
            if (baseNonNegative)
            {
                return true;
            }
            return CompareDecimalMagnitudeToSize(exponentDigits, baseMagnitude) >= 0;
        }

        if (!baseNonNegative)
        {
            return false;
        }
        return CompareDecimalMagnitudeToSize(exponentDigits, baseMagnitude) <= 0;
    }

    [[nodiscard]] bool TryParseDoubleInvariantFloat(
        const std::optional<std::string>& source, double& parsed) noexcept
    {
        parsed = 0.0;
        if (!source.has_value())
        {
            return false;
        }

        bool negative = false;
        std::string_view numeric;
        if (ExtractInvariantFloatCore(*source, negative, numeric))
        {
            if (MantissaIsZero(numeric))
            {
                parsed = negative ? -0.0 : 0.0;
                return true;
            }

            double magnitude = 0.0;
            const auto conversion = std::from_chars(
                numeric.data(), numeric.data() + numeric.size(), magnitude, std::chars_format::general);
            if (conversion.ec == std::errc{} && conversion.ptr == numeric.data() + numeric.size())
            {
                parsed = negative ? -magnitude : magnitude;
                return true;
            }
            if (conversion.ec == std::errc::result_out_of_range
                && conversion.ptr == numeric.data() + numeric.size())
            {
                magnitude = OutOfRangeIsOverflow(numeric)
                    ? std::numeric_limits<double>::infinity()
                    : 0.0;
                parsed = negative ? -magnitude : magnitude;
                return true;
            }
        }

        // .NET's floating parser falls back to the culture's special symbols after
        // numeric parsing fails and trims them with Char.IsWhiteSpace semantics.
        std::string_view value = TrimDotNetWhiteSpace(*source);
        if (EqualsOrdinalIgnoreCaseAscii(value, "Infinity"))
        {
            parsed = std::numeric_limits<double>::infinity();
            return true;
        }
        if (EqualsOrdinalIgnoreCaseAscii(value, "-Infinity"))
        {
            parsed = -std::numeric_limits<double>::infinity();
            return true;
        }
        if (EqualsOrdinalIgnoreCaseAscii(value, "NaN")
            || EqualsOrdinalIgnoreCaseAscii(value, "+NaN")
            || EqualsOrdinalIgnoreCaseAscii(value, "-NaN"))
        {
            parsed = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        if (EqualsOrdinalIgnoreCaseAscii(value, "+Infinity"))
        {
            parsed = std::numeric_limits<double>::infinity();
            return true;
        }
        return false;
    }

    void RuntimeRandomBytes(void* buffer, std::size_t size)
    {
#if defined(_WIN32)
        const NTSTATUS status = BCryptGenRandom(
            nullptr, static_cast<PUCHAR>(buffer), static_cast<ULONG>(size),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status != 0)
        {
            if (status == static_cast<NTSTATUS>(0xC0000017L))
            {
                throw std::bad_alloc();
            }
            throw std::runtime_error("BCryptGenRandom failed");
        }
#else
        int descriptor;
        do
        {
            descriptor = ::open("/dev/urandom", O_RDONLY
#ifdef O_CLOEXEC
                | O_CLOEXEC
#endif
            );
        }
        while (descriptor == -1 && errno == EINTR);
        if (descriptor == -1)
        {
            throw std::runtime_error("/dev/urandom unavailable");
        }

        auto* output = static_cast<unsigned char*>(buffer);
        std::size_t offset = 0;
        while (offset < size)
        {
            const ssize_t count = ::read(descriptor, output + offset, size - offset);
            if (count < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                const int savedErrno = errno;
                ::close(descriptor);
                throw std::system_error(savedErrno, std::generic_category(), "read /dev/urandom");
            }
            if (count == 0)
            {
                ::close(descriptor);
                throw std::runtime_error("short read from /dev/urandom");
            }
            offset += static_cast<std::size_t>(count);
        }
        ::close(descriptor);
#endif
    }

    class DotNetRandom final
    {
    public:
        DotNetRandom()
        {
            do
            {
                if constexpr (sizeof(void*) == 8)
                {
                    RuntimeRandomBytes(_state64.data(), sizeof(_state64));
                }
                else
                {
                    RuntimeRandomBytes(_state32.data(), sizeof(_state32));
                }
            }
            while (AllZero());
        }

        [[nodiscard]] double NextDouble() noexcept
        {
            if constexpr (sizeof(void*) == 8)
            {
                return static_cast<double>(NextUInt64() >> 11)
                    * (1.0 / static_cast<double>(std::uint64_t{1} << 53));
            }
            else
            {
                const std::uint32_t high = NextUInt32();
                const std::uint32_t low = NextUInt32();
                const std::uint64_t value =
                    (static_cast<std::uint64_t>(high) << 32) | low;
                return static_cast<double>(value >> 11)
                    * (1.0 / static_cast<double>(std::uint64_t{1} << 53));
            }
        }

    private:
        [[nodiscard]] bool AllZero() const noexcept
        {
            if constexpr (sizeof(void*) == 8)
            {
                return (_state64[0] | _state64[1] | _state64[2] | _state64[3]) == 0;
            }
            else
            {
                return (_state32[0] | _state32[1] | _state32[2] | _state32[3]) == 0;
            }
        }

        [[nodiscard]] std::uint64_t NextUInt64() noexcept
        {
            std::uint64_t s0 = _state64[0];
            std::uint64_t s1 = _state64[1];
            std::uint64_t s2 = _state64[2];
            std::uint64_t s3 = _state64[3];

            const std::uint64_t result = std::rotl(s1 * 5U, 7) * 9U;
            const std::uint64_t t = s1 << 17;
            s2 ^= s0;
            s3 ^= s1;
            s1 ^= s2;
            s0 ^= s3;
            s2 ^= t;
            s3 = std::rotl(s3, 45);

            _state64[0] = s0;
            _state64[1] = s1;
            _state64[2] = s2;
            _state64[3] = s3;
            return result;
        }

        [[nodiscard]] std::uint32_t NextUInt32() noexcept
        {
            std::uint32_t s0 = _state32[0];
            std::uint32_t s1 = _state32[1];
            std::uint32_t s2 = _state32[2];
            std::uint32_t s3 = _state32[3];

            const std::uint32_t result = std::rotl(s1 * 5U, 7) * 9U;
            const std::uint32_t t = s1 << 9;
            s2 ^= s0;
            s3 ^= s1;
            s1 ^= s2;
            s0 ^= s3;
            s2 ^= t;
            s3 = std::rotl(s3, 11);

            _state32[0] = s0;
            _state32[1] = s1;
            _state32[2] = s2;
            _state32[3] = s3;
            return result;
        }

        std::array<std::uint64_t, 4> _state64{};
        std::array<std::uint32_t, 4> _state32{};
    };

    [[nodiscard]] std::int64_t RuntimeStopwatchFrequency() noexcept
    {
#if defined(_WIN32)
        LARGE_INTEGER frequency{};
        (void)QueryPerformanceFrequency(&frequency);
        return static_cast<std::int64_t>(frequency.QuadPart);
#else
        return 1'000'000'000LL;
#endif
    }

    [[nodiscard]] std::string CurrentCultureDecimalSeparator()
    {
        // CultureInfo.CurrentCulture is CLR-owned. The Native boundary maps it
        // to the host's ambient user numeric locale without changing that locale.
#if defined(_WIN32)
        wchar_t buffer[16]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length <= 1)
        {
            return ".";
        }
        const int utf8Length = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0)
        {
            return ".";
        }
        std::string result(static_cast<std::size_t>(utf8Length), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
        return result;
#else
        locale_t locale = newlocale(LC_NUMERIC_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return ".";
        }
        const char* separator = nl_langinfo_l(RADIXCHAR, locale);
        std::string result = separator != nullptr && separator[0] != '\0' ? separator : ".";
        freelocale(locale);
        return result;
#endif
    }

    [[nodiscard]] std::string FormatLossPercentCurrentCulture(double value)
    {
        // .NET 9 custom Double formatting stages through a 15-significant-digit
        // NumberBuffer before applying the custom 0.## rounding. Preserve that
        // staging instead of rounding the original binary64 directly to 2 places.
        std::array<char, 128> buffer{};
        const auto result = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value,
            std::chars_format::scientific, 14);
        if (result.ec != std::errc{})
        {
            throw std::runtime_error("double formatting failed");
        }

        const std::string_view scientific(
            buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
        const std::size_t exponentMarker = scientific.find('e');
        if (exponentMarker == std::string_view::npos)
        {
            throw std::runtime_error("double formatting failed");
        }

        std::uint64_t digits = 0;
        for (std::size_t index = 0; index < exponentMarker; index++)
        {
            const char unit = scientific[index];
            if (unit == '.')
            {
                continue;
            }
            if (unit < '0' || unit > '9')
            {
                throw std::runtime_error("double formatting failed");
            }
            digits = digits * 10U + static_cast<std::uint64_t>(unit - '0');
        }

        const char* exponentFirst = scientific.data() + exponentMarker + 1;
        const char* const exponentEnd = scientific.data() + scientific.size();
        bool negativeExponent = false;
        if (exponentFirst != exponentEnd && (*exponentFirst == '+' || *exponentFirst == '-'))
        {
            negativeExponent = *exponentFirst == '-';
            exponentFirst++;
        }

        int exponent = 0;
        const auto exponentResult = std::from_chars(exponentFirst, exponentEnd, exponent);
        if (exponentResult.ec != std::errc{} || exponentResult.ptr != exponentEnd)
        {
            throw std::runtime_error("double formatting failed");
        }
        if (negativeExponent)
        {
            exponent = -exponent;
        }

        std::uint64_t hundredths = 0;
        if (exponent >= -3)
        {
            unsigned divisorPower = static_cast<unsigned>(12 - exponent);
            std::uint64_t divisor = 1;
            while (divisorPower-- > 0)
            {
                divisor *= 10U;
            }

            hundredths = digits / divisor;
            const std::uint64_t remainder = digits % divisor;
            if (remainder >= (divisor + 1U) / 2U)
            {
                hundredths++;
            }
        }

        std::string text = std::to_string(hundredths / 100U);
        const unsigned fraction = static_cast<unsigned>(hundredths % 100U);
        if (fraction != 0)
        {
            const std::string separator = CurrentCultureDecimalSeparator();
            text += separator;
            text.push_back(static_cast<char>('0' + fraction / 10U));
            if ((fraction % 10U) != 0)
            {
                text.push_back(static_cast<char>('0' + fraction % 10U));
            }
        }
        return text;
    }

    class NetLagRuntime final
    {
    public:
        NetLagRuntime() noexcept
        {
            try
            {
                _random.emplace();
            }
            catch (...)
            {
                _initializationFailure = std::current_exception();
            }
        }

        [[nodiscard]] DotNetRandom& Random()
        {
            if (_initializationFailure)
            {
                std::rethrow_exception(_initializationFailure);
            }
            return *_random;
        }

    private:
        std::optional<DotNetRandom> _random{};
        std::exception_ptr _initializationFailure{};
    };

    [[nodiscard]] DotNetRandom& RandomState()
    {
        // Mirrors CLR type initialization: the one Random is initialized when
        // NetLag is first touched, and an initialization failure remains sticky.
        static NetLagRuntime runtime;
        return runtime.Random();
    }

    [[nodiscard]] std::int64_t StopwatchFrequency() noexcept
    {
        static const std::int64_t frequency = RuntimeStopwatchFrequency();
        return frequency;
    }
}

namespace MphRead::Mods::Network
{
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
        "NetLag requires .NET Double-compatible IEEE-754 binary64.");
    static_assert(sizeof(void*) == 4 || sizeof(void*) == 8,
        "NetLag requires a 32-bit or 64-bit target for .NET Random parity.");

    std::int32_t NetLag::_roundTripMs = 0;
    std::int32_t NetLag::_jitterMs = 0;
    double NetLag::_lossPercent = 0.0;

    std::int32_t NetLag::RoundTripMs()
    {
        (void)RandomState();
        return _roundTripMs;
    }

    std::int32_t NetLag::JitterMs()
    {
        (void)RandomState();
        return _jitterMs;
    }

    double NetLag::LossPercent()
    {
        (void)RandomState();
        return _lossPercent;
    }

    bool NetLag::Active()
    {
        (void)RandomState();
        return _roundTripMs > 0 || _lossPercent > 0.0;
    }

    std::int64_t NetLag::HoldTicks()
    {
        DotNetRandom& random = RandomState();
        if (_roundTripMs <= 0 && _jitterMs <= 0)
        {
            return 0;
        }
        double ms = _roundTripMs / 2.0;
        if (_jitterMs > 0)
        {
            ms += random.NextDouble() * _jitterMs;
        }
        return static_cast<std::int64_t>(
            ms * static_cast<double>(StopwatchFrequency()) / 1000.0);
    }

    bool NetLag::Drops()
    {
        DotNetRandom& random = RandomState();
        return _lossPercent > 0.0 && random.NextDouble() * 100.0 < _lossPercent;
    }

    bool NetLag::Configure(const std::optional<std::string>& value)
    {
        (void)RandomState();
        if (IsNullOrWhiteSpaceLikeDotNet(value))
        {
            return false;
        }

        const std::string_view text = *value;
        const std::size_t firstSeparator = text.find_first_of(":,");
        const std::string_view rttPart = firstSeparator == std::string_view::npos
            ? text
            : text.substr(0, firstSeparator);

        std::int32_t rtt = 0;
        if (!TryParseInt32InvariantInteger(rttPart, rtt) || rtt < 0 || rtt > 10000)
        {
            return false;
        }

        std::int32_t jitter = 0;
        if (firstSeparator != std::string_view::npos)
        {
            const std::size_t secondSeparator = text.find_first_of(":,", firstSeparator + 1);
            const std::string_view jitterPart = secondSeparator == std::string_view::npos
                ? text.substr(firstSeparator + 1)
                : text.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1);
            if (!TryParseInt32InvariantInteger(jitterPart, jitter) || jitter < 0 || jitter > 5000)
            {
                return false;
            }
        }

        _roundTripMs = rtt;
        _jitterMs = jitter;
        return true;
    }

    bool NetLag::ConfigureLoss(const std::optional<std::string>& value)
    {
        (void)RandomState();
        double percent = 0.0;
        if (!TryParseDoubleInvariantFloat(value, percent) || percent < 0.0 || percent > 100.0)
        {
            return false;
        }
        _lossPercent = percent;
        return true;
    }

    std::optional<std::string> NetLag::Describe()
    {
        (void)RandomState();
        if (!(_roundTripMs > 0 || _lossPercent > 0.0))
        {
            return std::nullopt;
        }

        std::string text = _roundTripMs > 0
            ? "+" + std::to_string(_roundTripMs) + " ms round trip"
            : "no added latency";
        if (_jitterMs > 0)
        {
            text += " (jitter up to " + std::to_string(_jitterMs) + " ms each way)";
        }
        if (_lossPercent > 0.0)
        {
            text += ", " + FormatLossPercentCurrentCulture(_lossPercent)
                + "% packet loss each way";
        }
        return text;
    }
}
