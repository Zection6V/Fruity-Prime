#include "NetLag.hpp"
#include "../../NativeRuntime/System/Stopwatch.hpp"
#include "../../NativeRuntime/System/Random.hpp"
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

        [[nodiscard]] ::MphRead::NativeRuntime::Random& Random()
        {
            if (_initializationFailure)
            {
                std::rethrow_exception(_initializationFailure);
            }
            return *_random;
        }

    private:
        std::optional<::MphRead::NativeRuntime::Random> _random{};
        std::exception_ptr _initializationFailure{};
    };

    [[nodiscard]] ::MphRead::NativeRuntime::Random& RandomState()
    {
        // Mirrors CLR type initialization: the one Random is initialized when
        // NetLag is first touched, and an initialization failure remains sticky.
        static NetLagRuntime runtime;
        return runtime.Random();
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
        ::MphRead::NativeRuntime::Random& random = RandomState();
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
            ms * static_cast<double>(::MphRead::NativeRuntime::StopwatchFrequency()) / 1000.0);
    }

    bool NetLag::Drops()
    {
        ::MphRead::NativeRuntime::Random& random = RandomState();
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
