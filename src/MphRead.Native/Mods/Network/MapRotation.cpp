#include "MapRotation.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network::Detail
{
    // GameMode is owned by a different C# source file and does not yet have a
    // Native declaration on develop2. These seams preserve MapRotation.cs's
    // exact enum defaults, parsing, equality values, and enum ToString behavior
    // without defining a partial or competing Native GameMode.
    GameMode MapRotationGameModeNone();
    GameMode MapRotationGameModeBattle();
    bool MapRotationGameModeTryParseIgnoreCase(std::string_view value, GameMode& result);
    std::string MapRotationGameModeToString(GameMode value);

    // C++ has no exact System.String.Trim/current-culture System.Int32 or
    // current-culture custom numeric-format equivalent. Keep those BCL
    // semantics at a narrow runtime boundary rather than substituting a C++
    // locale approximation.
    std::string MapRotationStringTrim(std::string_view value);
    bool MapRotationInt32TryParseCurrentCulture(std::string_view value, std::int32_t& result);
    std::string MapRotationFormatSingle0PointHashCurrentCulture(float value);
    std::string MapRotationFormatInt32CurrentCulture(std::int32_t value);

    // The invariant Single.TryParse overload is likewise kept explicit so its
    // NumberStyles.Float grammar, IEEE-754 conversion, and overflow behavior
    // remain the C# contract.
    bool MapRotationSingleTryParseInvariantFloat(std::string_view value, float& result);

    // File and Console are runtime-owned in C#. These boundaries must preserve
    // File.ReadAllLines/File.WriteAllLines/File.Exists and Console.WriteLine,
    // including encoding/newline/error behavior and side-effect ordering.
    std::vector<std::string> MapRotationFileReadAllLines(std::string_view path);
    void MapRotationFileWriteAllLines(std::string_view path,
        const std::vector<std::string>& lines);
    bool MapRotationFileExists(std::string_view path);
    void MapRotationConsoleWriteLine(std::string_view value);
}

namespace
{
    std::vector<std::string> SplitOnPipe(std::string_view value)
    {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t separator = value.find('|', start);
            if (separator == std::string_view::npos)
            {
                parts.emplace_back(value.substr(start));
                break;
            }
            parts.emplace_back(value.substr(start, separator - start));
            start = separator + 1;
        }
        return parts;
    }

    void RemoveAsciiSpaces(std::string& value)
    {
        value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
    }
}

namespace MphRead::Mods::Network
{
    RotationEntry::RotationEntry()
        : RoomKey("MP3 PROVING GROUND"),
          Mode(Detail::MapRotationGameModeBattle()),
          TimeLimit(7.0f * 60.0f),
          PointGoal(7)
    {
    }

    RotationEntry::RotationEntry(std::string roomKey, GameMode mode, float timeLimit,
        std::int32_t pointGoal)
        : RoomKey(std::move(roomKey)),
          Mode(mode),
          TimeLimit(timeLimit),
          PointGoal(pointGoal)
    {
    }

    std::string RotationEntry::ToString() const
    {
        std::string value = RoomKey;
        value += " (";
        value += Detail::MapRotationGameModeToString(Mode);
        value += ", ";
        value += Detail::MapRotationFormatSingle0PointHashCurrentCulture(TimeLimit / 60.0f);
        value += " min, ";
        value += Detail::MapRotationFormatInt32CurrentCulture(PointGoal);
        value += " pts)";
        return value;
    }

    const std::shared_ptr<const RotationEntry> MapRotation::_fallback(
        new RotationEntry());

    const std::vector<std::shared_ptr<const RotationEntry>>& MapRotation::Entries() const
    {
        return _entries;
    }

    std::shared_ptr<const RotationEntry> MapRotation::Current() const
    {
        if (_override != nullptr)
        {
            return _override;
        }
        if (!_entries.empty())
        {
            return _entries[static_cast<std::size_t>(_index)];
        }
        return _fallback;
    }

    std::shared_ptr<const RotationEntry> MapRotation::Next() const
    {
        if (_pending != nullptr)
        {
            return _pending;
        }
        if (!_entries.empty())
        {
            const std::int32_t count = static_cast<std::int32_t>(_entries.size());
            const std::int32_t next = (_index + 1) % count;
            return _entries[static_cast<std::size_t>(next)];
        }
        return _fallback;
    }

    std::int32_t MapRotation::Index() const
    {
        return _index;
    }

    std::shared_ptr<MapRotation> MapRotation::SingleMatch(const std::string& roomKey,
        GameMode mode, float timeLimit, std::int32_t pointGoal)
    {
        auto rotation = std::make_shared<MapRotation>();
        const GameMode actualMode = mode == Detail::MapRotationGameModeNone()
            ? Detail::MapRotationGameModeBattle()
            : mode;
        rotation->_entries.emplace_back(new RotationEntry(
            roomKey, actualMode, timeLimit, pointGoal));
        return rotation;
    }

    void MapRotation::PlayNext(const std::string& roomKey, GameMode mode)
    {
        const std::shared_ptr<const RotationEntry> current = Current();
        const GameMode actualMode = mode == Detail::MapRotationGameModeNone()
            ? current->Mode
            : mode;
        _pending = std::shared_ptr<const RotationEntry>(new RotationEntry(
            roomKey, actualMode, current->TimeLimit, current->PointGoal));
    }

    std::shared_ptr<const RotationEntry> MapRotation::Advance()
    {
        if (_pending != nullptr)
        {
            _override = _pending;
            _pending = nullptr;
            return Current();
        }
        if (_override != nullptr)
        {
            _override = nullptr;
        }
        if (!_entries.empty())
        {
            const std::int32_t count = static_cast<std::int32_t>(_entries.size());
            _index = (_index + 1) % count;
        }
        return Current();
    }

    std::shared_ptr<MapRotation> MapRotation::Load(const std::string& path)
    {
        auto rotation = std::make_shared<MapRotation>();
        for (const std::string& raw : Detail::MapRotationFileReadAllLines(path))
        {
            std::string line = raw;
            const std::size_t comment = line.find('#');
            if (comment != std::string::npos)
            {
                line = line.substr(0, comment);
            }
            line = Detail::MapRotationStringTrim(line);
            if (line.empty())
            {
                continue;
            }

            const std::vector<std::string> parts = SplitOnPipe(line);
            const std::string roomKey = Detail::MapRotationStringTrim(parts[0]);
            if (roomKey.empty())
            {
                continue;
            }

            GameMode mode = Detail::MapRotationGameModeBattle();
            if (parts.size() > 1)
            {
                std::string modeText = Detail::MapRotationStringTrim(parts[1]);
                RemoveAsciiSpaces(modeText);
                GameMode parsedMode = mode;
                if (Detail::MapRotationGameModeTryParseIgnoreCase(modeText, parsedMode))
                {
                    mode = parsedMode;
                }
            }

            float timeLimit = 7.0f * 60.0f;
            if (parts.size() > 2)
            {
                const std::string minutesText = Detail::MapRotationStringTrim(parts[2]);
                float minutes = 0.0f;
                if (Detail::MapRotationSingleTryParseInvariantFloat(minutesText, minutes))
                {
                    timeLimit = minutes * 60.0f;
                }
            }

            std::int32_t pointGoal = 7;
            if (parts.size() > 3)
            {
                const std::string pointsText = Detail::MapRotationStringTrim(parts[3]);
                std::int32_t parsedPoints = 0;
                if (Detail::MapRotationInt32TryParseCurrentCulture(pointsText, parsedPoints))
                {
                    pointGoal = parsedPoints;
                }
            }

            rotation->_entries.emplace_back(new RotationEntry(
                roomKey, mode, timeLimit, pointGoal));
        }
        return rotation;
    }

    void MapRotation::WriteDefault(const std::string& path)
    {
        Detail::MapRotationFileWriteAllLines(path,
        {
            "# MphRead dedicated server map rotation.",
            "# One match per line:  ROOM KEY | mode | minutes | points",
            "# Mode and the numbers are optional; '#' starts a comment.",
            "# Room keys are the names MphRead uses internally -- run the",
            "# game's console menu to see the full list.",
            "",
            "MP1 SANCTORUS      | Battle | 7 | 7",
            "MP3 PROVING GROUND | Battle | 7 | 7",
            "MP4 HIGHGROUND     | Battle | 7 | 7",
            "MP2 HARVESTER      | Battle | 7 | 7",
            "MP6 HEADSHOT       | Battle | 7 | 7"
        });
    }

    std::shared_ptr<MapRotation> MapRotation::LoadOrCreate(const std::string& path)
    {
        if (!Detail::MapRotationFileExists(path))
        {
            WriteDefault(path);
            Detail::MapRotationConsoleWriteLine(
                std::string("[server] wrote a starter rotation to ") + path);
        }
        std::shared_ptr<MapRotation> rotation = Load(path);
        if (rotation->_entries.empty())
        {
            Detail::MapRotationConsoleWriteLine(
                std::string("[server] ") + path
                + " has no usable entries; using a single default map");
            rotation->_entries.push_back(_fallback);
        }
        return rotation;
    }
}
