#include "NetDiagnostics.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Enums.hpp"
#include "../../GameState.hpp"
#include "NetDamage.hpp"
#include "NetPlayerBridge.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <langinfo.h>
#include <locale.h>
#endif

using ::MphRead::NativeRuntime::EnvironmentGetVariable;
using ::MphRead::NativeRuntime::HasFlag;

namespace
{
    [[nodiscard]] std::string CurrentCultureDecimalSeparator()
    {
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
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* separator = locale != nullptr ? locale->decimal_point : nullptr;
        return separator != nullptr && separator[0] != '\0' ? separator : ".";
#else
        locale_t locale = newlocale(LC_NUMERIC_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return ".";
        }
        const char* separator = nl_langinfo_l(RADIXCHAR, locale);
        std::string result = separator != nullptr && separator[0] != '\0'
            ? separator
            : ".";
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string CurrentCultureNegativeSign()
    {
#if defined(_WIN32)
        wchar_t buffer[16]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SNEGATIVESIGN, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length <= 1)
        {
            return "-";
        }
        const int utf8Length = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0)
        {
            return "-";
        }
        std::string result(static_cast<std::size_t>(utf8Length), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
        return result;
#else
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* sign = locale != nullptr ? locale->negative_sign : nullptr;
        return sign != nullptr && sign[0] != '\0' ? sign : "-";
#else
        locale_t locale = newlocale(LC_MONETARY_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return "-";
        }
#if defined(NEGATIVE_SIGN)
        const char* sign = nl_langinfo_l(NEGATIVE_SIGN, locale);
        std::string result = sign != nullptr && sign[0] != '\0' ? sign : "-";
#else
        std::string result = "-";
#endif
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string CurrentCultureNaNSymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SNAN)
        wchar_t buffer[32]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SNAN, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length > 1)
        {
            const int utf8Length = WideCharToMultiByte(
                CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
            if (utf8Length > 0)
            {
                std::string result(static_cast<std::size_t>(utf8Length), '\0');
                WideCharToMultiByte(
                    CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
                return result;
            }
        }
#endif
        return "NaN";
    }

    [[nodiscard]] std::string CurrentCulturePositiveInfinitySymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SPOSINFINITY)
        wchar_t buffer[32]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SPOSINFINITY, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length > 1)
        {
            const int utf8Length = WideCharToMultiByte(
                CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
            if (utf8Length > 0)
            {
                std::string result(static_cast<std::size_t>(utf8Length), '\0');
                WideCharToMultiByte(
                    CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
                return result;
            }
        }
#endif
        return "\xE2\x88\x9E";
    }

    [[nodiscard]] std::string CurrentCultureNegativeInfinitySymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SNEGINFINITY)
        wchar_t buffer[32]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SNEGINFINITY, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length > 1)
        {
            const int utf8Length = WideCharToMultiByte(
                CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
            if (utf8Length > 0)
            {
                std::string result(static_cast<std::size_t>(utf8Length), '\0');
                WideCharToMultiByte(
                    CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
                return result;
            }
        }
#endif
        return CurrentCultureNegativeSign() + "\xE2\x88\x9E";
    }

    [[nodiscard]] std::string UInt32ToDigits(std::uint32_t value)
    {
        std::array<char, 16> buffer{};
        const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Integer formatting failed.");
        }
        return std::string(buffer.data(), converted.ptr);
    }

    [[nodiscard]] std::string Int32ToCurrentCulture(std::int32_t value)
    {
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        if (value >= 0)
        {
            return UInt32ToDigits(bits);
        }
        const std::uint32_t magnitude = 0U - bits;
        return CurrentCultureNegativeSign() + UInt32ToDigits(magnitude);
    }

    [[nodiscard]] std::string ByteToCurrentCulture(std::uint8_t value)
    {
        return UInt32ToDigits(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::string FormatSingleZeroPointZeroZero(float value)
    {
        if (std::isnan(value))
        {
            return CurrentCultureNaNSymbol();
        }
        if (std::isinf(value))
        {
            return std::signbit(value)
                ? CurrentCultureNegativeInfinitySymbol()
                : CurrentCulturePositiveInfinitySymbol();
        }

        const bool negative = std::signbit(value);
        const float magnitude = std::fabs(value);

        std::uint32_t digits = 0;
        std::int32_t exponent = 0;
        if (magnitude != 0.0F)
        {
            std::array<char, 64> buffer{};
            const auto converted = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), magnitude,
                std::chars_format::scientific, 6);
            if (converted.ec != std::errc{})
            {
                throw std::runtime_error("Single formatting failed.");
            }

            const std::string_view scientific(
                buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data()));
            std::size_t exponentMarker = scientific.find('e');
            if (exponentMarker == std::string_view::npos
                && (exponentMarker = scientific.find('E')) == std::string_view::npos)
            {
                throw std::runtime_error("Single formatting failed.");
            }

            for (std::size_t index = 0; index < exponentMarker; index++)
            {
                const char unit = scientific[index];
                if (unit == '.')
                {
                    continue;
                }
                if (unit < '0' || unit > '9')
                {
                    throw std::runtime_error("Single formatting failed.");
                }
                digits = digits * 10U + static_cast<std::uint32_t>(unit - '0');
            }

            const char* exponentFirst = scientific.data() + exponentMarker + 1;
            const char* const exponentEnd = scientific.data() + scientific.size();
            bool negativeExponent = false;
            if (exponentFirst != exponentEnd
                && (*exponentFirst == '+' || *exponentFirst == '-'))
            {
                negativeExponent = *exponentFirst == '-';
                exponentFirst++;
            }

            const auto parsed = std::from_chars(exponentFirst, exponentEnd, exponent);
            if (parsed.ec != std::errc{} || parsed.ptr != exponentEnd)
            {
                throw std::runtime_error("Single formatting failed.");
            }
            if (negativeExponent)
            {
                exponent = -exponent;
            }
        }

        std::string hundredthsText;
        if (magnitude == 0.0F)
        {
            hundredthsText = "000";
        }
        else if (exponent >= 4)
        {
            hundredthsText = UInt32ToDigits(digits);
            hundredthsText.append(static_cast<std::size_t>(exponent - 4), '0');
        }
        else
        {
            const std::int32_t divisorPower = 4 - exponent;
            std::uint32_t hundredths = 0;
            if (divisorPower <= 7)
            {
                std::uint32_t divisor = 1;
                for (std::int32_t power = 0; power < divisorPower; power++)
                {
                    divisor *= 10U;
                }
                hundredths = digits / divisor;
                const std::uint32_t remainder = digits % divisor;
                if (remainder * 2U >= divisor)
                {
                    hundredths++;
                }
            }
            hundredthsText = UInt32ToDigits(hundredths);
        }

        if (hundredthsText.size() < 3)
        {
            hundredthsText.insert(
                hundredthsText.begin(), 3 - hundredthsText.size(), '0');
        }

        const std::size_t fractionStart = hundredthsText.size() - 2;
        std::string result = hundredthsText.substr(0, fractionStart);
        result += CurrentCultureDecimalSeparator();
        result.append(hundredthsText, fractionStart, 2);
        if (negative)
        {
            result.insert(0, CurrentCultureNegativeSign());
        }
        return result;
    }

    [[nodiscard]] std::string NetRoleToString(MphRead::Mods::Network::NetRole value)
    {
        using MphRead::Mods::Network::NetRole;
        switch (value)
        {
        case NetRole::Offline:
            return "Offline";
        case NetRole::Host:
            return "Host";
        case NetRole::Client:
            return "Client";
        case NetRole::Server:
            return "Server";
        }
        return Int32ToCurrentCulture(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] std::string BeamTypeToString(MphRead::BeamType value)
    {
        switch (value)
        {
        case MphRead::BeamType::None:
            return "None";
        case MphRead::BeamType::PowerBeam:
            return "PowerBeam";
        case MphRead::BeamType::VoltDriver:
            return "VoltDriver";
        case MphRead::BeamType::Missile:
            return "Missile";
        case MphRead::BeamType::Battlehammer:
            return "Battlehammer";
        case MphRead::BeamType::Imperialist:
            return "Imperialist";
        case MphRead::BeamType::Judicator:
            return "Judicator";
        case MphRead::BeamType::Magmaul:
            return "Magmaul";
        case MphRead::BeamType::ShockCoil:
            return "ShockCoil";
        case MphRead::BeamType::OmegaCannon:
            return "OmegaCannon";
        case MphRead::BeamType::Platform:
            return "Platform";
        case MphRead::BeamType::Enemy:
            return "Enemy";
        }
        return Int32ToCurrentCulture(static_cast<std::int32_t>(value));
    }
}

namespace MphRead::Mods::Network
{
    bool NetDiagnostics::Enabled()
    {
        if (!_checked)
        {
            _checked = true;
            _enabled = EnvironmentGetVariable("MPHREAD_NET_DEBUG").has_value();
        }
        return _enabled;
    }

    void NetDiagnostics::SetEnabled(bool value) noexcept
    {
        _checked = true;
        _enabled = value;
    }

    void NetDiagnostics::Report(double time)
    {
        if (!Enabled() || !NetSession::Active())
        {
            return;
        }
        if (time - _lastReport < 1.0)
        {
            return;
        }
        _lastReport = time;

        std::string line;
        line += "[netdbg] role=";
        line += NetRoleToString(NetSession::Role());
        line += " slot=";
        line += Int32ToCurrentCulture(NetSession::LocalSlot());

        std::int32_t active = 0;
        std::int32_t created = 0;
        line += " slots=[";
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            if (player == nullptr)
            {
                line.push_back('-');
                continue;
            }
            created++;
            const bool isActive = HasFlag(player->LoadFlags(), Entities::LoadFlags::Active);
            if (isActive)
            {
                active++;
            }
            line.push_back(isActive
                ? (player->IsBot() ? 'B' : 'A')
                : HasFlag(player->LoadFlags(), Entities::LoadFlags::SlotActive) ? 's' : '.');
        }
        line += "] active=";
        line += Int32ToCurrentCulture(active);
        line += " scoreboard=";
        line += Int32ToCurrentCulture(GameState::ActivePlayers());
        line += " created=";
        line += Int32ToCurrentCulture(created);

        line += " remoteState=[";
        for (std::size_t i = 0; i < NetSession::RemoteStateValid.size(); i++)
        {
            line.push_back(NetSession::RemoteStateValid[i] ? 'y' : 'n');
        }
        line += "] remoteIntent=[";
        for (std::size_t i = 0; i < NetSession::RemoteIntentValid.size(); i++)
        {
            line.push_back(NetSession::RemoteIntentValid[i] ? 'y' : 'n');
        }
        line.push_back(']');

        std::int32_t botRemotes = 0;
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            if (player != nullptr && i != NetSession::LocalSlot() && player->IsBot()
                && HasFlag(player->LoadFlags(), Entities::LoadFlags::Active))
            {
                botRemotes++;
            }
        }
        if (botRemotes > 0)
        {
            line += "  !! ";
            line += Int32ToCurrentCulture(botRemotes);
            line += " remote slot(s) still AI-driven";
        }

        line += " team=[";
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            if (i > 0)
            {
                line.push_back(',');
            }
            line += player == nullptr
                ? "-"
                : Int32ToCurrentCulture(player->TeamIndex());
        }
        line.push_back(']');

        if (!GameState::Teams())
        {
            std::int32_t shared = 0;
            for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
            {
                std::shared_ptr<Entities::PlayerEntity> a
                    = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
                if (a == nullptr
                    || !HasFlag(a->LoadFlags(), Entities::LoadFlags::Active))
                {
                    continue;
                }
                for (std::int32_t j = i + 1; j < Entities::PlayerEntity::MaxPlayers(); j++)
                {
                    std::shared_ptr<Entities::PlayerEntity> b
                        = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(j));
                    if (b != nullptr
                        && HasFlag(b->LoadFlags(), Entities::LoadFlags::Active)
                        && a->TeamIndex() == b->TeamIndex())
                    {
                        shared++;
                    }
                }
            }
            if (shared > 0)
            {
                line += "  !! ";
                line += Int32ToCurrentCulture(shared);
                line += " pair(s) share a team index in a free-for-all: "
                    "bombs and life drain will do nothing between them";
            }
        }

        line += " alt=[";
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers(); i++)
        {
            std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i));
            line.push_back(player == nullptr
                    || !HasFlag(player->LoadFlags(), Entities::LoadFlags::Active)
                ? '-'
                : player->IsAltForm() ? 'A'
                : player->IsMorphing() ? 'm'
                : player->IsUnmorphing() ? 'u'
                : 'b');
        }
        line += "] altSaid=[";
        line += NetPlayerBridge::FormSaidByAuthority();
        line.push_back(']');
        line += " shockcoil=";
        line += Int32ToCurrentCulture(NetDamage::ShockCoilAcquired);
        line.push_back('/');
        line += Int32ToCurrentCulture(NetDamage::ShockCoilSpawned);
        line += " bomb=";
        line += Int32ToCurrentCulture(NetDamage::BombHits);
        line.push_back('/');
        line += Int32ToCurrentCulture(NetDamage::BombPlayerChecks);
        line += " bombTeamSkips=";
        line += Int32ToCurrentCulture(NetDamage::BombTeamSkips);

        line += " dmg[";
        bool first = true;
        for (std::size_t i = 0; i < NetDamage::HitsByBeam.size(); i++)
        {
            if (NetDamage::HitsByBeam[i] == 0)
            {
                continue;
            }
            if (!first)
            {
                line.push_back(' ');
            }
            first = false;
            line += BeamTypeToString(static_cast<MphRead::BeamType>(i));
            line.push_back('=');
            line += Int32ToCurrentCulture(NetDamage::DamageByBeam[i]);
            line.push_back('/');
            line += Int32ToCurrentCulture(NetDamage::HitsByBeam[i]);
        }
        if (NetDamage::BombDamageHits > 0)
        {
            if (!first)
            {
                line.push_back(' ');
            }
            line += "Bomb=";
            line += Int32ToCurrentCulture(NetDamage::BombDamageDealt);
            line.push_back('/');
            line += Int32ToCurrentCulture(NetDamage::BombDamageHits);
        }
        line.push_back(']');

        line += " bombSpawn=";
        line += Int32ToCurrentCulture(NetDamage::BombSpawnMade);
        line.push_back('/');
        line += Int32ToCurrentCulture(NetDamage::BombSpawnCalls);
        line += " det=";
        line += Int32ToCurrentCulture(NetDamage::BombSpawnDetonated);
        line += " stale=";
        line += Int32ToCurrentCulture(NetDamage::BombSpawnStaleCount);
        line += " poolEmpty=";
        line += Int32ToCurrentCulture(NetDamage::BombSpawnPoolEmpty);
        line += " bombNearest=";
        line += NetDamage::BombNearest == std::numeric_limits<float>::max()
            ? "n/a"
            : FormatSingleZeroPointZeroZero(NetDamage::BombNearest);
        line += " bombRadius=";
        line += FormatSingleZeroPointZeroZero(NetDamage::BombRadiusSeen);

        if (NetPlayerBridge::PlacementsRefused > 0)
        {
            line += " placementsRefused=";
            line += Int32ToCurrentCulture(NetPlayerBridge::PlacementsRefused);
        }

        const std::optional<MatchStatePacket> match = NetSession::ServerMatch();
        if (match.has_value())
        {
            line += " serverMap=";
            if (match->RoomKey.has_value())
            {
                line += *match->RoomKey;
            }
            line += " serverPlayers=";
            line += ByteToCurrentCulture(match->PlayerCount);
        }
        std::cout << line << '\n';
    }
}
