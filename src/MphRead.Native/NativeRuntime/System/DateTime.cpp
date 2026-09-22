#include "DateTime.hpp"

#include "Exceptions.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <ratio>
#include <string>

namespace MphRead::NativeRuntime
{
    namespace
    {
        // DateTime.UnixEpoch.Ticks.
        constexpr std::int64_t UnixEpochTicks = 621355968000000000LL;
        constexpr std::int64_t TicksPerSecond = 10000000LL;
        constexpr std::int64_t TicksPerDay = TicksPerSecond * 60LL * 60LL * 24LL;

        struct CivilDate final
        {
            std::int32_t Year = 1;
            std::int32_t Month = 1;
            std::int32_t Day = 1;
        };

        // Days since 0001-01-01 back to a civil date, by the proleptic
        // Gregorian calendar DateTime uses throughout.
        [[nodiscard]] CivilDate FromDayNumber(std::int64_t days)
        {
            // Shift to a 1970-01-01 epoch and use Howard Hinnant's civil_from_days.
            std::int64_t z = days - 719162LL + 719468LL;
            const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
            const std::int64_t doe = z - era * 146097;
            const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
            const std::int64_t y = yoe + era * 400;
            const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
            const std::int64_t mp = (5 * doy + 2) / 153;
            const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;
            const std::int64_t m = mp < 10 ? mp + 3 : mp - 9;
            CivilDate result;
            result.Year = static_cast<std::int32_t>(m <= 2 ? y + 1 : y);
            result.Month = static_cast<std::int32_t>(m);
            result.Day = static_cast<std::int32_t>(d);
            return result;
        }

        [[nodiscard]] std::string PadLeft(std::int64_t value, std::size_t width)
        {
            std::string text = std::to_string(value);
            while (text.size() < width)
            {
                text.insert(text.begin(), '0');
            }
            return text;
        }

        // The offset the local time zone is at for the given UTC instant:
        // the instant, less the same wall-clock fields read back as local.
        [[nodiscard]] std::int64_t LocalOffsetSeconds(std::int64_t utcSeconds)
        {
            const std::time_t when = static_cast<std::time_t>(utcSeconds);
            std::tm utc{};
#if defined(_WIN32)
            if (gmtime_s(&utc, &when) != 0)
            {
                return 0;
            }
#else
            if (gmtime_r(&when, &utc) == nullptr)
            {
                return 0;
            }
#endif
            utc.tm_isdst = -1;
            const std::time_t roundTrip = std::mktime(&utc);
            if (roundTrip == static_cast<std::time_t>(-1))
            {
                return 0;
            }
            return static_cast<std::int64_t>(when) - static_cast<std::int64_t>(roundTrip);
        }
    }

    std::int64_t ManagedDateTime::UtcOffsetSeconds() const
    {
        if (Kind != 2)
        {
            return 0;
        }
        // The stored ticks are already local, so step back to the instant they
        // describe before asking the zone about it.
        const std::int64_t approxUtcSeconds = (Ticks - UnixEpochTicks) / TicksPerSecond;
        return LocalOffsetSeconds(approxUtcSeconds);
    }

    ManagedDateTime DateTimeUtcNow()
    {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto ticks = std::chrono::duration_cast<
            std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(now).count();
        ManagedDateTime result;
        result.Ticks = UnixEpochTicks + ticks;
        result.Kind = 1;
        return result;
    }

    std::int64_t DateTimeUtcNowTicks()
    {
        return DateTimeUtcNow().Ticks;
    }

    ManagedDateTime DateTimeNow()
    {
        const ManagedDateTime utc = DateTimeUtcNow();
        const std::int64_t utcSeconds = (utc.Ticks - UnixEpochTicks) / TicksPerSecond;
        ManagedDateTime result;
        result.Ticks = utc.Ticks + LocalOffsetSeconds(utcSeconds) * TicksPerSecond;
        result.Kind = 2;
        return result;
    }

    std::string DateTimeToString(const ManagedDateTime& value, std::string_view format)
    {
        const std::int64_t days = value.Ticks / TicksPerDay;
        const std::int64_t timeOfDay = value.Ticks - days * TicksPerDay;
        const CivilDate date = FromDayNumber(days);
        const std::int64_t hour = timeOfDay / (TicksPerSecond * 3600);
        const std::int64_t minute = (timeOfDay / (TicksPerSecond * 60)) % 60;
        const std::int64_t second = (timeOfDay / TicksPerSecond) % 60;
        const std::int64_t milliseconds = (timeOfDay / 10000LL) % 1000LL;

        std::string result;
        std::size_t index = 0;
        while (index < format.size())
        {
            const char specifier = format[index];
            std::size_t run = 1;
            while (index + run < format.size() && format[index + run] == specifier)
            {
                ++run;
            }
            const std::string_view token = format.substr(index, run);
            if (token == "yyyy")
            {
                result += PadLeft(date.Year, 4);
            }
            else if (token == "MM")
            {
                result += PadLeft(date.Month, 2);
            }
            else if (token == "dd")
            {
                result += PadLeft(date.Day, 2);
            }
            else if (token == "HH")
            {
                result += PadLeft(hour, 2);
            }
            else if (token == "mm")
            {
                result += PadLeft(minute, 2);
            }
            else if (token == "ss")
            {
                result += PadLeft(second, 2);
            }
            else if (token == "fff")
            {
                result += PadLeft(milliseconds, 3);
            }
            else if (token == "zzz")
            {
                const std::int64_t offset = value.UtcOffsetSeconds();
                const std::int64_t magnitude = offset < 0 ? -offset : offset;
                result += offset < 0 ? '-' : '+';
                result += PadLeft(magnitude / 3600, 2);
                result += ':';
                result += PadLeft((magnitude / 60) % 60, 2);
            }
            else if (run == 1 && (specifier == '-' || specifier == '_' || specifier == ':'
                || specifier == ' ' || specifier == '.' || specifier == '/'))
            {
                // A literal in every format this program uses. The culture's
                // date and time separators are not substituted here because
                // nothing asks for them.
                result += specifier;
            }
            else
            {
                throw System::FormatException();
            }
            index += run;
        }
        return result;
    }
}
