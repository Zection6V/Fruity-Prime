#include "NetLag.hpp"
#include "NativeRuntime/System/Charconv.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "NativeRuntime/System/Globalization.hpp"

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
#if defined(__APPLE__)
#include <xlocale.h>
#endif
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::StringTrimView;

namespace
{
    [[nodiscard]] bool IsNullOrWhiteSpaceLikeDotNet(
        const std::optional<std::string>& value) noexcept
    {
        return !value.has_value() || StringTrimView(*value).empty();
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
        if (!::MphRead::NativeRuntime::Int32TryParseInvariant(rttPart, rtt) || rtt < 0 || rtt > 10000)
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
            if (!::MphRead::NativeRuntime::Int32TryParseInvariant(jitterPart, jitter) || jitter < 0 || jitter > 5000)
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
        if (!value.has_value() && ::MphRead::NativeRuntime::DoubleTryParseInvariant(*value, percent) || percent < 0.0 || percent > 100.0)
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
            text += ", " + ::MphRead::NativeRuntime::ToString(_lossPercent, "0.##")
                + "% packet loss each way";
        }
        return text;
    }
}
