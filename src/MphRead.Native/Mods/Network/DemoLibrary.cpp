#include "DemoLibrary.hpp"

#include "DemoFile.hpp"
#include "../../Read.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <iostream>
#include <system_error>

namespace
{
    using MphRead::Mods::Network::DemoLibraryDateTime;

    constexpr std::int64_t TicksPerSecond = 10'000'000;
    constexpr std::int64_t TicksPerMinute = TicksPerSecond * 60;
    constexpr std::int64_t TicksPerHour = TicksPerMinute * 60;
    constexpr std::int64_t TicksPerDay = TicksPerHour * 24;

    [[nodiscard]] constexpr bool IsLeapYear(std::int32_t year) noexcept
    {
        return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    }

    [[nodiscard]] bool ParseDigits(std::string_view text, std::size_t offset,
        std::size_t count, std::int32_t& value) noexcept
    {
        value = 0;
        if (offset > text.size() || count > text.size() - offset)
        {
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            const char ch = text[offset + i];
            if (ch < '0' || ch > '9')
            {
                return false;
            }
            value = value * 10 + static_cast<std::int32_t>(ch - '0');
        }
        return true;
    }

    [[nodiscard]] bool TryParseInvariantTimestamp(
        std::string_view stamp, DemoLibraryDateTime& parsed) noexcept
    {
        // Exact C# format: yyyy-MM-dd_HH-mm-ss, DateTimeStyles.None.
        if (stamp.size() != 19
            || stamp[4] != '-' || stamp[7] != '-' || stamp[10] != '_'
            || stamp[13] != '-' || stamp[16] != '-')
        {
            return false;
        }

        std::int32_t year = 0;
        std::int32_t month = 0;
        std::int32_t day = 0;
        std::int32_t hour = 0;
        std::int32_t minute = 0;
        std::int32_t second = 0;
        if (!ParseDigits(stamp, 0, 4, year)
            || !ParseDigits(stamp, 5, 2, month)
            || !ParseDigits(stamp, 8, 2, day)
            || !ParseDigits(stamp, 11, 2, hour)
            || !ParseDigits(stamp, 14, 2, minute)
            || !ParseDigits(stamp, 17, 2, second))
        {
            return false;
        }
        if (year < 1 || year > 9999 || month < 1 || month > 12
            || hour > 23 || minute > 59 || second > 59)
        {
            return false;
        }

        static constexpr std::array<std::int32_t, 13> DaysToMonth365 = {
            0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
        };
        static constexpr std::array<std::int32_t, 13> DaysToMonth366 = {
            0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366
        };
        const auto& days = IsLeapYear(year) ? DaysToMonth366 : DaysToMonth365;
        const std::int32_t daysInMonth = days[static_cast<std::size_t>(month)]
            - days[static_cast<std::size_t>(month - 1)];
        if (day < 1 || day > daysInMonth)
        {
            return false;
        }

        const std::int64_t y = static_cast<std::int64_t>(year - 1);
        const std::int64_t totalDays = y * 365 + y / 4 - y / 100 + y / 400
            + days[static_cast<std::size_t>(month - 1)] + day - 1;
        const std::int64_t ticks = totalDays * TicksPerDay
            + static_cast<std::int64_t>(hour) * TicksPerHour
            + static_cast<std::int64_t>(minute) * TicksPerMinute
            + static_cast<std::int64_t>(second) * TicksPerSecond;
        parsed = DemoLibraryDateTime(ticks);
        return true;
    }

    [[nodiscard]] std::string FormatInvariantOneDecimal(float value)
    {
        std::array<char, 64> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
            value, std::chars_format::fixed, 1);
        if (result.ec != std::errc{})
        {
            throw std::runtime_error("Could not format demo size.");
        }
        return std::string(buffer.data(), result.ptr);
    }

    void WriteListFailure(const std::exception& ex)
    {
        // The C# catch handler evaluates Directory again while constructing
        // the message; if that property now throws, that exception propagates.
        const std::string directory = MphRead::Mods::Network::DemoLibrary::Directory();
        std::cout << "[demo] could not list " << directory << ": " << ex.what() << '\n';
    }
}

namespace MphRead::Mods::Network
{
    DemoRecording::DemoRecording(std::string path, std::string room,
        DemoLibraryDateTime recorded, std::int64_t bytes)
        : _path(std::move(path)),
          _room(std::move(room)),
          _recorded(recorded),
          _bytes(bytes)
    {
    }

    const std::string& DemoRecording::Path() const noexcept
    {
        return _path;
    }

    const std::string& DemoRecording::Room() const noexcept
    {
        return _room;
    }

    DemoLibraryDateTime DemoRecording::Recorded() const noexcept
    {
        return _recorded;
    }

    std::int64_t DemoRecording::Bytes() const noexcept
    {
        return _bytes;
    }

    std::string DemoRecording::FileName() const
    {
        return Detail::DemoLibraryGetFileName(_path);
    }

    std::string DemoLibrary::Directory()
    {
        return Detail::DemoLibraryGetFullPath(Paths::Combine(Paths::Export(), "_demos"));
    }

    std::shared_ptr<const std::vector<DemoRecording>> DemoLibrary::List()
    {
        auto found = std::make_shared<std::vector<DemoRecording>>();
        try
        {
            const std::string directory = Directory();
            if (!Detail::DemoLibraryDirectoryExists(directory))
            {
                return found;
            }

            const auto enumerator = Detail::DemoLibraryEnumerateFiles(
                directory, "*" + std::string(DemoFile::Extension));
            try
            {
                while (Detail::DemoLibraryFileEnumeratorMoveNext(enumerator))
                {
                    const std::string path = Detail::DemoLibraryFileEnumeratorCurrent(enumerator);
                    const auto info = Detail::DemoLibraryCreateFileInfo(path);
                    auto [room, stamp] = ReadName(Detail::DemoLibraryFileInfoName(info));

                    // Null-coalescing in C#: a parsed file-name timestamp avoids
                    // evaluating FileInfo.LastWriteTime entirely.
                    const DemoLibraryDateTime recorded = stamp.has_value()
                        ? *stamp
                        : Detail::DemoLibraryFileInfoLastWriteTime(info);
                    const std::int64_t bytes = Detail::DemoLibraryFileInfoLength(info);
                    found->emplace_back(path, std::move(room), recorded, bytes);
                }
            }
            catch (...)
            {
                Detail::DemoLibraryFileEnumeratorDispose(enumerator);
                throw;
            }
            Detail::DemoLibraryFileEnumeratorDispose(enumerator);
        }
        catch (const Detail::DemoLibraryIOException& ex)
        {
            WriteListFailure(ex);
        }
        catch (const Detail::DemoLibraryUnauthorizedAccessException& ex)
        {
            WriteListFailure(ex);
        }

        // List<T>.Sort(Comparison<T>) is unstable; std::sort intentionally keeps
        // that contract rather than imposing stable ordering on equal stamps.
        std::sort(found->begin(), found->end(), [](const DemoRecording& a, const DemoRecording& b)
        {
            return b.Recorded().CompareTo(a.Recorded()) < 0;
        });
        return found;
    }

    std::pair<std::string, std::optional<DemoLibraryDateTime>>
        DemoLibrary::ReadName(const std::string& fileName)
    {
        std::string name = Detail::DemoLibraryGetFileNameWithoutExtension(fileName);
        constexpr std::size_t stampLength = 19;
        if (name.size() < stampLength + 2
            || name[name.size() - (stampLength + 1)] != '_')
        {
            return {std::move(name), std::nullopt};
        }

        const std::string_view stamp(name.data() + name.size() - stampLength, stampLength);
        DemoLibraryDateTime parsed;
        if (!TryParseInvariantTimestamp(stamp, parsed))
        {
            return {std::move(name), std::nullopt};
        }
        name.resize(name.size() - (stampLength + 1));
        return {std::move(name), parsed};
    }

    std::string DemoLibrary::Describe(const DemoRecording& demo)
    {
        return Detail::DemoLibraryFormatCurrentCultureDateTime(
            demo.Recorded(), "d MMM yyyy, HH:mm")
            + " — " + Size(demo.Bytes());
    }

    std::string DemoLibrary::Size(std::int64_t bytes)
    {
        if (bytes >= 1024 * 1024)
        {
            const float megabytes = static_cast<float>(bytes) / (1024.0F * 1024.0F);
            return FormatInvariantOneDecimal(megabytes) + " MB";
        }
        if (bytes >= 1024)
        {
            return Detail::DemoLibraryFormatCurrentCultureInt64(bytes / 1024) + " KB";
        }
        return Detail::DemoLibraryFormatCurrentCultureInt64(bytes) + " bytes";
    }
}
