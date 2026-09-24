#include "NetLog.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Culling.hpp"
#include "../../Scene.hpp"
#include "../../GameState.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#else
#error Unsupported platform for AppContext.BaseDirectory parity.
#endif

#if !defined(_WIN32)
#include <langinfo.h>
#include <locale.h>
#endif

namespace
{
    [[nodiscard]] bool HasFlag(
        MphRead::Entities::LoadFlags value, MphRead::Entities::LoadFlags flag) noexcept
    {
        return (value & flag) != MphRead::Entities::LoadFlags::None;
    }

#if defined(_WIN32)
    [[nodiscard]] std::string LocaleInfoUtf8(LCTYPE type, std::string fallback)
    {
        wchar_t buffer[32]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, type, buffer,
            static_cast<int>(std::size(buffer)));
        if (length <= 1)
        {
            return fallback;
        }
        const int bytes = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0)
        {
            return fallback;
        }
        std::string result(static_cast<std::size_t>(bytes), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, result.data(), bytes, nullptr, nullptr);
        return result;
    }
#endif

    [[nodiscard]] std::string DecimalSeparator()
    {
#if defined(_WIN32)
        return LocaleInfoUtf8(LOCALE_SDECIMAL, ".");
#else
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* value = locale != nullptr ? locale->decimal_point : nullptr;
        return value != nullptr && value[0] != '\0' ? value : ".";
#else
        locale_t locale = newlocale(LC_NUMERIC_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return ".";
        }
        const char* value = nl_langinfo_l(RADIXCHAR, locale);
        std::string result = value != nullptr && value[0] != '\0' ? value : ".";
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string NegativeSign()
    {
#if defined(_WIN32)
        return LocaleInfoUtf8(LOCALE_SNEGATIVESIGN, "-");
#else
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* value = locale != nullptr ? locale->negative_sign : nullptr;
        return value != nullptr && value[0] != '\0' ? value : "-";
#else
        locale_t locale = newlocale(LC_MONETARY_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return "-";
        }
#if defined(NEGATIVE_SIGN)
        const char* value = nl_langinfo_l(NEGATIVE_SIGN, locale);
        std::string result = value != nullptr && value[0] != '\0' ? value : "-";
#else
        std::string result = "-";
#endif
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string NaNSymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SNAN)
        return LocaleInfoUtf8(LOCALE_SNAN, "NaN");
#else
        return "NaN";
#endif
    }

    [[nodiscard]] std::string PositiveInfinitySymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SPOSINFINITY)
        return LocaleInfoUtf8(LOCALE_SPOSINFINITY, "\xE2\x88\x9E");
#else
        return "\xE2\x88\x9E";
#endif
    }

    [[nodiscard]] std::string NegativeInfinitySymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SNEGINFINITY)
        return LocaleInfoUtf8(LOCALE_SNEGINFINITY, NegativeSign() + "\xE2\x88\x9E");
#else
        return NegativeSign() + "\xE2\x88\x9E";
#endif
    }

    [[nodiscard]] std::string TimeSeparator()
    {
#if defined(_WIN32)
        return LocaleInfoUtf8(LOCALE_STIME, ":");
#else
#if defined(__ANDROID__)
        std::tm sample{};
        sample.tm_hour = 11;
        sample.tm_min = 22;
        sample.tm_sec = 33;
        std::array<char, 64> buffer{};
        if (std::strftime(buffer.data(), buffer.size(), "%X", &sample) == 0)
        {
            return ":";
        }
        const std::string_view formatted(buffer.data());
        const std::size_t hour = formatted.find("11");
        if (hour == std::string_view::npos)
        {
            return ":";
        }
        const std::size_t minute = formatted.find("22", hour + 2);
        if (minute == std::string_view::npos || minute <= hour + 2)
        {
            return ":";
        }
        const std::string_view separator = formatted.substr(hour + 2, minute - (hour + 2));
        return separator.empty() ? ":" : std::string(separator);
#else
        locale_t locale = newlocale(LC_TIME_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return ":";
        }
        const char* raw = nl_langinfo_l(T_FMT, locale);
        std::string result = ":";
        if (raw != nullptr)
        {
            const std::string_view format(raw);
            std::size_t first = format.find("%H");
            if (first == std::string_view::npos)
            {
                first = format.find("%I");
            }
            if (first != std::string_view::npos)
            {
                first += 2;
                const std::size_t next = format.find('%', first);
                if (next != std::string_view::npos && next > first)
                {
                    result.assign(format.substr(first, next - first));
                }
            }
        }
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string Int32Text(std::int32_t value)
    {
        std::array<char, 32> buffer{};
        const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Int32 formatting failed.");
        }
        std::string result(buffer.data(), converted.ptr);
        if (!result.empty() && result[0] == '-')
        {
            result.erase(result.begin());
            result.insert(0, NegativeSign());
        }
        return result;
    }

    [[nodiscard]] std::string UInt16Text(std::uint16_t value)
    {
        std::array<char, 16> buffer{};
        const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("UInt16 formatting failed.");
        }
        return std::string(buffer.data(), converted.ptr);
    }

    [[nodiscard]] std::string ByteText(std::uint8_t value)
    {
        std::array<char, 8> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), static_cast<unsigned int>(value));
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Byte formatting failed.");
        }
        return std::string(buffer.data(), converted.ptr);
    }

    [[nodiscard]] std::string FixedText(float value, int decimals)
    {
        if (std::isnan(value))
        {
            return NaNSymbol();
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? NegativeInfinitySymbol() : PositiveInfinitySymbol();
        }
        std::array<char, 96> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value,
            std::chars_format::fixed, decimals);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Single formatting failed.");
        }
        std::string result(buffer.data(), converted.ptr);
        if (!result.empty() && result[0] == '-')
        {
            result.erase(result.begin());
            result.insert(0, NegativeSign());
        }
        const std::size_t point = result.find('.');
        if (point != std::string::npos)
        {
            result.replace(point, 1, DecimalSeparator());
        }
        return result;
    }

    struct Utf8Unit final
    {
        char32_t CodePoint = 0;
        std::size_t Length = 1;
        bool Valid = false;
    };

    [[nodiscard]] Utf8Unit DecodeUtf8(std::string_view text, std::size_t index) noexcept
    {
        const auto first = static_cast<unsigned char>(text[index]);
        if (first < 0x80U)
        {
            return {first, 1, true};
        }
        std::size_t length = 0;
        char32_t codePoint = 0;
        char32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            length = 2;
            codePoint = first & 0x1FU;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            length = 3;
            codePoint = first & 0x0FU;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            length = 4;
            codePoint = first & 0x07U;
            minimum = 0x10000U;
        }
        else
        {
            return {first, 1, false};
        }
        if (index + length > text.size())
        {
            return {first, 1, false};
        }
        for (std::size_t offset = 1; offset < length; offset++)
        {
            const auto byte = static_cast<unsigned char>(text[index + offset]);
            if ((byte & 0xC0U) != 0x80U)
            {
                return {first, 1, false};
            }
            codePoint = (codePoint << 6U) | (byte & 0x3FU);
        }
        if (codePoint < minimum || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            return {first, 1, false};
        }
        return {codePoint, length, true};
    }

    [[nodiscard]] std::size_t Utf16Length(std::string_view text) noexcept
    {
        std::size_t length = 0;
        for (std::size_t index = 0; index < text.size();)
        {
            const Utf8Unit unit = DecodeUtf8(text, index);
            length += unit.Valid && unit.CodePoint > 0xFFFFU ? 2U : 1U;
            index += unit.Length;
        }
        return length;
    }

    [[nodiscard]] std::string AlignLeft(std::string text, std::size_t width)
    {
        const std::size_t length = Utf16Length(text);
        if (length < width)
        {
            text.append(width - length, ' ');
        }
        return text;
    }

    [[nodiscard]] bool IsLetterOrDigit(char32_t codePoint)
    {
        if ((codePoint >= U'0' && codePoint <= U'9')
            || (codePoint >= U'A' && codePoint <= U'Z')
            || (codePoint >= U'a' && codePoint <= U'z'))
        {
            return true;
        }
        if (codePoint > 0xFFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            return false;
        }
#if defined(_WIN32)
        const wchar_t value = static_cast<wchar_t>(codePoint);
        WORD type = 0;
        if (GetStringTypeW(CT_CTYPE1, &value, 1, &type) == 0)
        {
            throw std::system_error(
                static_cast<int>(GetLastError()), std::system_category());
        }
        return (type & (C1_ALPHA | C1_DIGIT)) != 0;
#else
        static const std::locale locale("");
        return std::use_facet<std::ctype<wchar_t>>(locale).is(
            std::ctype_base::alpha | std::ctype_base::digit,
            static_cast<wchar_t>(codePoint));
#endif
    }

    [[nodiscard]] std::string SafeClientName(std::string_view clientName)
    {
        std::string safe;
        for (std::size_t index = 0; index < clientName.size();)
        {
            const Utf8Unit unit = DecodeUtf8(clientName, index);
            if (!unit.Valid)
            {
                safe.push_back('_');
            }
            else if (unit.CodePoint > 0xFFFFU)
            {
                safe += "__";
            }
            else if (IsLetterOrDigit(unit.CodePoint))
            {
                safe.append(clientName.substr(index, unit.Length));
            }
            else
            {
                safe.push_back('_');
            }
            index += unit.Length;
        }
        return safe;
    }

    struct LocalClock final
    {
        std::tm Calendar{};
        int Millisecond = 0;
    };

    [[nodiscard]] LocalClock LocalNow()
    {
        const auto now = std::chrono::system_clock::now();
        const auto wholeSeconds = std::chrono::floor<std::chrono::seconds>(now);
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - wholeSeconds).count();
        const std::time_t time = std::chrono::system_clock::to_time_t(wholeSeconds);
        LocalClock result{};
#if defined(_WIN32)
        if (localtime_s(&result.Calendar, &time) != 0)
#else
        if (localtime_r(&time, &result.Calendar) == nullptr)
#endif
        {
            throw std::runtime_error("DateTime.Now conversion failed.");
        }
        result.Millisecond = static_cast<int>(millis);
        return result;
    }

    [[nodiscard]] std::string Digits(int value, int count)
    {
        std::string result(static_cast<std::size_t>(count), '0');
        for (int index = count - 1; index >= 0; index--)
        {
            result[static_cast<std::size_t>(index)] = static_cast<char>('0' + value % 10);
            value /= 10;
        }
        return result;
    }

    [[nodiscard]] std::string NowWithMilliseconds()
    {
        const LocalClock now = LocalNow();
        const std::string separator = TimeSeparator();
        return Digits(now.Calendar.tm_hour, 2)
            + separator + Digits(now.Calendar.tm_min, 2)
            + separator + Digits(now.Calendar.tm_sec, 2)
            + "." + Digits(now.Millisecond, 3);
    }

    [[nodiscard]] std::string StartedNow()
    {
        const LocalClock now = LocalNow();
        const std::string separator = TimeSeparator();
        return Digits(now.Calendar.tm_year + 1900, 4)
            + "-" + Digits(now.Calendar.tm_mon + 1, 2)
            + "-" + Digits(now.Calendar.tm_mday, 2)
            + " " + Digits(now.Calendar.tm_hour, 2)
            + separator + Digits(now.Calendar.tm_min, 2)
            + separator + Digits(now.Calendar.tm_sec, 2);
    }

    [[nodiscard]] std::filesystem::path ExecutableDirectory()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                throw std::system_error(
                    static_cast<int>(GetLastError()), std::system_category());
            }
            if (length < buffer.size())
            {
                return std::filesystem::path(
                    std::wstring(buffer.data(), static_cast<std::size_t>(length))).parent_path();
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        (void)_NSGetExecutablePath(nullptr, &size);
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            throw std::runtime_error("AppContext.BaseDirectory resolution failed.");
        }
        return std::filesystem::weakly_canonical(
            std::filesystem::path(buffer.data())).parent_path();
#elif defined(__linux__)
        std::vector<char> buffer(512);
        for (;;)
        {
            const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (length < 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
            if (static_cast<std::size_t>(length) < buffer.size())
            {
                return std::filesystem::path(
                    std::string(buffer.data(), static_cast<std::size_t>(length))).parent_path();
            }
            buffer.resize(buffer.size() * 2U);
        }
#endif
    }

    [[nodiscard]] std::string NullableText(const std::optional<std::string>& value)
    {
        return value.has_value() ? *value : std::string();
    }

    [[nodiscard]] std::shared_ptr<MphRead::Entities::PlayerEntity> PlayerAt(
        std::int32_t slot)
    {
        const auto& players = MphRead::Entities::PlayerEntity::Players();
        if (slot < 0 || static_cast<std::size_t>(slot) >= players.size())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return players[static_cast<std::size_t>(slot)];
    }
}

namespace MphRead::Mods::Network
{
    std::unique_ptr<std::ofstream> NetLog::_writer{};
    double NetLog::_lastWrite = 0.0;
    bool NetLog::_failed = false;
    const double NetLog::Interval = NetLog::ReadInterval();
    bool NetLog::_enabled = false;

    bool NetLog::Enabled() noexcept
    {
        return _enabled;
    }

    double NetLog::ReadInterval()
    {
        const char* value = std::getenv("MPHREAD_NETLOG_INTERVAL");
        if (value != nullptr)
        {
            std::istringstream stream(value);
            stream.imbue(std::locale::classic());
            stream >> std::ws;
            double parsed = 0.0;
            if (stream >> parsed)
            {
                stream >> std::ws;
                if (stream.eof() && parsed > 0.0 && parsed <= 10.0)
                {
                    return parsed;
                }
            }
        }
        return 1.0;
    }

    void NetLog::Open(const std::string& clientName)
    {
        if (_failed || _writer != nullptr)
        {
            return;
        }
        try
        {
            const std::string safe = SafeClientName(clientName);
            const std::filesystem::path baseDirectory = ExecutableDirectory();
            const std::string fileName = "netlog-" + safe + ".txt";
            const std::u8string utf8FileName(
                reinterpret_cast<const char8_t*>(fileName.data()), fileName.size());
            const std::filesystem::path path
                = baseDirectory / std::filesystem::path(utf8FileName);
            auto writer = std::make_unique<std::ofstream>();
            writer->exceptions(std::ios::failbit | std::ios::badbit);
            writer->open(path, std::ios::out | std::ios::trunc);
            _writer = std::move(writer);
            _enabled = true;
            Line("=== MphRead net log for \"" + clientName + "\" ===");
            Line("started " + StartedNow());
        }
        catch (const std::ios_base::failure& ex)
        {
            _failed = true;
            std::cout << "[netlog] could not open log: " << ex.what() << std::endl;
        }
    }

    void NetLog::Close()
    {
        if (_writer != nullptr)
        {
            _writer->close();
        }
        _writer.reset();
        _enabled = false;
        _lastWrite = 0.0;
    }

    void NetLog::CollisionRange(
        std::int32_t slot, const std::string& label,
        OpenTK::Mathematics::Vector3 prev, OpenTK::Mathematics::Vector3 current)
    {
        const OpenTK::Mathematics::Vector3 vector = current - prev;
        const float delta = std::sqrt(
            vector.X * vector.X + vector.Y * vector.Y + vector.Z * vector.Z);
        std::string message = "collision ";
        message += label;
        message += " slot=";
        message += Int32Text(slot);
        message += " prev=(";
        message += FixedText(prev.X, 2);
        message += ",";
        message += FixedText(prev.Y, 2);
        message += ",";
        message += FixedText(prev.Z, 2);
        message += ") cur=(";
        message += FixedText(current.X, 2);
        message += ",";
        message += FixedText(current.Y, 2);
        message += ",";
        message += FixedText(current.Z, 2);
        message += ") delta=";
        message += FixedText(delta, 2);
        Event(message);
    }

    void NetLog::Event(const std::string& message)
    {
        Line("[" + NowWithMilliseconds() + "] EVENT  " + message);
    }

    void NetLog::Snapshot(double time)
    {
        SnapshotInternal(time, nullptr);
    }

    void NetLog::Snapshot(double time, MphRead::Scene& scene)
    {
        SnapshotInternal(time, &scene);
    }

    void NetLog::SnapshotInternal(double time, MphRead::Scene* scene)
    {
        if (_writer == nullptr || time - _lastWrite < Interval)
        {
            return;
        }
        _lastWrite = time;

        std::string state;
        state += "[";
        state += NowWithMilliseconds();
        state += "] STATE  role=";
        state += ToString(NetSession::Role());
        state += " slot=";
        state += Int32Text(NetSession::LocalSlot());
        state += " authority=";
        state += NetSession::IsAuthority() ? "True" : "False";
        state += " main=";
        state += Int32Text(Entities::PlayerEntity::MainPlayerIndex());
        state += " mode=";
        state += ::MphRead::ToString(GameState::Mode());
        state += " matchTime=";
        state += FixedText(GameState::MatchTime(), 1);
        state += " matchState=";
        state += ::MphRead::ToString(GameState::MatchState());
        state += " goal=";
        state += Int32Text(GameState::PointGoal());
        state += " ";

        const std::optional<MatchStatePacket> match = NetSession::ServerMatch();
        if (match.has_value())
        {
            state += "serverTime=";
            state += FixedText(match->TimeRemaining, 1);
            state += " serverMap=";
            state += NullableText(match->RoomKey);
            state += " serverPeers=";
            state += ByteText(match->PlayerCount);
            state += " ";
        }
        Line(state);

        for (std::int32_t slot = 0;
            slot < Entities::PlayerEntity::MaxPlayers(); slot++)
        {
            const std::shared_ptr<Entities::PlayerEntity> player = PlayerAt(slot);
            if (player == nullptr)
            {
                Line("           slot " + Int32Text(slot) + ": (no entity)");
                continue;
            }
            const bool active = HasFlag(
                player->LoadFlags(), Entities::LoadFlags::Active);
            const bool occupied = slot < static_cast<std::int32_t>(NetSession::SlotOccupied.size())
                && NetSession::SlotOccupied[slot];
            if (!active && !occupied)
            {
                continue;
            }

            std::string line;
            line += "           slot ";
            line += Int32Text(slot);
            line += ": name=";
            line += AlignLeft(GameState::Nicknames()[slot], 10);
            line += " occupied=";
            line += occupied ? "y" : "n";
            line += " active=";
            line += active ? "y" : "n";
            line += " spawned=";
            line += HasFlag(player->LoadFlags(), Entities::LoadFlags::Spawned) ? "y" : "n";
            line += " bot=";
            line += player->IsBot() ? "y" : "n";
            line += " hp=";
            line += AlignLeft(Int32Text(player->Health()), 3);
            line += " score=";
            line += Int32Text(GameState::Points()[slot]);
            line += "/";
            line += Int32Text(GameState::TeamPoints()[slot]);
            line += "p ";
            line += Int32Text(GameState::Kills()[slot]);
            line += "k";
            line += Int32Text(GameState::Deaths()[slot]);
            line += "d respawnTimer=";
            line += AlignLeft(UInt16Text(player->RespawnTimer()), 5);
            line += " pos=(";
            line += FixedText(player->Position.X, 2);
            line += ",";
            line += FixedText(player->Position.Y, 2);
            line += ",";
            line += FixedText(player->Position.Z, 2);
            line += ") form=";
            line += AlignLeft(player->ModFormState(), 24);
            line += " nodeRef=";
            line += DescribeNodeRef(*player);
            line += " inScene=";
            line += InScene(scene, *player) ? "y" : "n";
            line += " stateValid=";
            line += NetSession::RemoteStateValid[slot] ? "y" : "n";
            line += " intentValid=";
            line += NetSession::RemoteIntentValid[slot] ? "y" : "n";
            Line(line);
        }
    }

    bool NetLog::InScene(MphRead::Scene* scene, Entities::PlayerEntity& player)
    {
        if (scene == nullptr)
        {
            return false;
        }
        auto enumerator = scene->Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            const std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
            if (entity.get() == &player)
            {
                return true;
            }
        }
        return false;
    }

    std::string NetLog::DescribeNodeRef(Entities::PlayerEntity& player)
    {
        try
        {
            const Formats::Culling::NodeRef nodeRef = player.NodeRef;
            if (nodeRef == Formats::Culling::NodeRef::None)
            {
                return "none";
            }
            std::string result = nodeRef.RoomName.HasValue() ? *nodeRef.RoomName : "?";
            result += ":";
            result += Int32Text(nodeRef.PartIndex);
            result += "/";
            result += Int32Text(nodeRef.NodeIndex);
            return result;
        }
        catch (...)
        {
            return "?";
        }
    }

    void NetLog::Line(const std::string& text)
    {
        try
        {
            if (_writer != nullptr)
            {
                *_writer << text << std::endl;
            }
        }
        catch (const std::ios_base::failure&)
        {
        }
    }
}
