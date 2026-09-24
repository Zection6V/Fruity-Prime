#include "NetDamage.hpp"

#include "../../GameState.hpp"
#include "NetHitPrediction.hpp"
#include "NetHooks.hpp"
#include "NetLog.hpp"
#include "NetSession.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numbers>
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

using ::MphRead::NativeRuntime::MathClamp;
using ::MphRead::NativeRuntime::MathMax;
using ::OpenTK::Mathematics::IsZero;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::Multiply;

namespace
{
    [[nodiscard]] std::int32_t UncheckedAddInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t UncheckedSubtractInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left)
            - static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    void UncheckedIncrement(std::int32_t& value) noexcept
    {
        value = UncheckedAddInt32(value, 1);
    }

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

    [[nodiscard]] bool IsTruthyDotNetInvariantSetting(std::string_view value) noexcept
    {
        if (value == "1")
        {
            return true;
        }
        return value.size() == 4
            && (value[0] == 't' || value[0] == 'T')
            && (value[1] == 'r' || value[1] == 'R')
            && (value[2] == 'u' || value[2] == 'U')
            && (value[3] == 'e' || value[3] == 'E');
    }

    [[nodiscard]] bool IsInvariantLocaleName(std::string_view name) noexcept
    {
        return name.empty() || name == "C" || name == "POSIX" || name.starts_with("C.");
    }

    [[nodiscard]] std::string CurrentCulturePositiveInfinitySymbol()
    {
#if defined(_WIN32)
        wchar_t buffer[32]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SPOSINFINITY, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length <= 1)
        {
            return "Infinity";
        }
        const int utf8Length = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0)
        {
            return "Infinity";
        }
        std::string result(static_cast<std::size_t>(utf8Length), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
        return result;
#else
        const char* invariantSetting = std::getenv("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT");
        if (invariantSetting != nullptr
            && IsTruthyDotNetInvariantSetting(invariantSetting))
        {
            return "Infinity";
        }

        const char* localeName = std::getenv("LC_ALL");
        if (localeName == nullptr || localeName[0] == '\0')
        {
            localeName = std::getenv("LC_NUMERIC");
        }
        if (localeName == nullptr || localeName[0] == '\0')
        {
            localeName = std::getenv("LANG");
        }
        if (localeName == nullptr || IsInvariantLocaleName(localeName))
        {
            return "Infinity";
        }
        return "\xE2\x88\x9E";
#endif
    }

    [[nodiscard]] std::string FormatZeroPointHashHash(float value)
    {
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? "-\xE2\x88\x9E" : CurrentCulturePositiveInfinitySymbol();
        }

        const float magnitude = std::fabs(value);
        if (magnitude == 0.0F)
        {
            return "0";
        }

        // .NET 9 custom Single formatting first materializes a seven-
        // significant-digit NumberBuffer, then applies the custom-format
        // rounding. Reproduce that staging before the 0.## two-place round.
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

        std::uint32_t digits = 0;
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
        if (exponentFirst != exponentEnd && (*exponentFirst == '+' || *exponentFirst == '-'))
        {
            negativeExponent = *exponentFirst == '-';
            exponentFirst++;
        }

        std::int32_t exponent = 0;
        const auto parsed = std::from_chars(exponentFirst, exponentEnd, exponent);
        if (parsed.ec != std::errc{} || parsed.ptr != exponentEnd)
        {
            throw std::runtime_error("Single formatting failed.");
        }
        if (negativeExponent)
        {
            exponent = -exponent;
        }

        std::string hundredthsText;
        if (exponent >= 4)
        {
            hundredthsText = std::to_string(digits);
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
            hundredthsText = std::to_string(hundredths);
        }

        if (hundredthsText.size() < 3)
        {
            hundredthsText.insert(
                hundredthsText.begin(), 3 - hundredthsText.size(), '0');
        }

        const std::size_t fractionStart = hundredthsText.size() - 2;
        std::string result = hundredthsText.substr(0, fractionStart);
        const char tenths = hundredthsText[fractionStart];
        const char hundredths = hundredthsText[fractionStart + 1];
        if (hundredths != '0')
        {
            result += CurrentCultureDecimalSeparator();
            result.push_back(tenths);
            result.push_back(hundredths);
        }
        else if (tenths != '0')
        {
            result += CurrentCultureDecimalSeparator();
            result.push_back(tenths);
        }

        if (std::signbit(value) && result != "0")
        {
            result.insert(result.begin(), '-');
        }
        return result;
    }

    [[nodiscard]] std::string Int32ToString(std::int32_t value)
    {
        return std::to_string(value);
    }

    [[nodiscard]] std::string ByteToString(std::uint8_t value)
    {
        return std::to_string(static_cast<unsigned int>(value));
    }
}

namespace MphRead::Mods::Network
{
    void NetDamage::NoteFired(Entities::PlayerEntity& shooter,
        OpenTK::Mathematics::Vector3 shotVec, OpenTK::Mathematics::Vector3 aimVec)
    {
        if (!NetSession::Active())
        {
            return;
        }
        const std::int32_t slot = shooter.SlotIndex();
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        UncheckedIncrement(Fired[static_cast<std::size_t>(slot)]);
        if (LengthSquared(shotVec) > 0.0001F && LengthSquared(aimVec) > 0.0001F)
        {
            const float dot = MathClamp(
                OpenTK::Mathematics::Vector3::Dot(
                    shotVec.Normalized(), aimVec.Normalized()),
                -1.0F, 1.0F);
            const double degrees = std::acos(static_cast<double>(dot))
                * 180.0 / std::numbers::pi;
            AimDrift[static_cast<std::size_t>(slot)] += degrees;
            WorstDrift[static_cast<std::size_t>(slot)] = MathMax(
                WorstDrift[static_cast<std::size_t>(slot)], degrees);
        }
    }

    void NetDamage::NotePlayerOverlap(
        Entities::EntityBase* owner, Entities::PlayerEntity& target)
    {
        if (!NetSession::Active())
        {
            return;
        }

        auto* shooter = dynamic_cast<Entities::PlayerEntity*>(owner);
        if (shooter == nullptr)
        {
            return;
        }

        const std::int32_t shooterSlot = shooter->SlotIndex();
        const std::int32_t targetSlot = target.SlotIndex();
        if (shooterSlot >= 0 && shooterSlot < Slots
            && targetSlot >= 0 && targetSlot < Slots)
        {
            UncheckedIncrement(PlayerOverlapsByShooter[
                static_cast<std::size_t>(shooterSlot)][
                static_cast<std::size_t>(targetSlot)]);
        }
    }

    void NetDamage::ResetForRoomChange()
    {
        Resolved.fill(0);
        Replayed.fill(0);
        Fired.fill(0);
        PlayerChecks.fill(0);
        PlayerOverlaps.fill(0);
        PlayerAccepted.fill(0);
        for (auto& row : PlayerOverlapsByShooter)
        {
            row.fill(0);
        }
        AimDrift.fill(0.0);
        WorstDrift.fill(0.0);
        ShockCoilSpawned = 0;
        ShockCoilAcquired = 0;
        BombPlayerChecks = 0;
        BombTeamSkips = 0;
        BombHits = 0;
        DamageByBeam.fill(0);
        HitsByBeam.fill(0);
        BombDamageDealt = 0;
        BombDamageHits = 0;
        BombSpawnCalls = 0;
        BombSpawnMade = 0;
        BombSpawnDetonated = 0;
        BombSpawnStaleCount = 0;
        BombSpawnPoolEmpty = 0;
        BombNearest = std::numeric_limits<float>::max();
        BombRadiusSeen = 0.0F;
        _replaying = false;
        _replayBeam = MphRead::BeamType::None;
    }

    void NetDamage::ForgetSlot(std::int32_t slot)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        _sequence[index] = 0;
        _attacker[index] = 0;
        _beam[index] = 0;
        _flags[index] = 0;
        _direction[index] = OpenTK::Mathematics::Vector3::Zero;
        _lastSeen[index] = 0;
        _everSeen[index] = false;
        Resolved[index] = 0;
        Replayed[index] = 0;
    }

    void NetDamage::Reset()
    {
        _sequence.fill(0);
        _attacker.fill(0);
        _beam.fill(0);
        _flags.fill(0);
        _direction.fill(OpenTK::Mathematics::Vector3::Zero);
        _lastSeen.fill(0);
        _everSeen.fill(false);
        Resolved.fill(0);
        Replayed.fill(0);
        Fired.fill(0);
        PlayerChecks.fill(0);
        PlayerOverlaps.fill(0);
        PlayerAccepted.fill(0);
        for (auto& row : PlayerOverlapsByShooter)
        {
            row.fill(0);
        }
        AimDrift.fill(0.0);
        WorstDrift.fill(0.0);
        ShockCoilSpawned = 0;
        ShockCoilAcquired = 0;
        BombPlayerChecks = 0;
        BombTeamSkips = 0;
        BombHits = 0;
        DamageByBeam.fill(0);
        HitsByBeam.fill(0);
        BombDamageDealt = 0;
        BombDamageHits = 0;
        BombSpawnCalls = 0;
        BombSpawnMade = 0;
        BombSpawnDetonated = 0;
        BombSpawnStaleCount = 0;
        BombSpawnPoolEmpty = 0;
        BombNearest = std::numeric_limits<float>::max();
        BombRadiusSeen = 0.0F;
        _replaying = false;
        _replayBeam = MphRead::BeamType::None;
    }

    bool NetDamage::Suppress(Entities::PlayerEntity& victim,
        Entities::EntityBase* source, Entities::DamageFlags flags)
    {
        if (!NetSession::Active() || _replaying)
        {
            return false;
        }
        if (NetSession::IsHost() || NetSession::IsAuthority())
        {
            return false;
        }
        return !NetHitPrediction::Predicts(victim, source, flags);
    }

    void NetDamage::Note(Entities::PlayerEntity& victim,
        Entities::PlayerEntity* attacker, MphRead::BeamType beam,
        Entities::DamageFlags flags,
        std::optional<OpenTK::Mathematics::Vector3> direction,
        std::uint32_t amount, bool fromBomb)
    {
        if (!NetSession::Active() || _replaying || NetHitPrediction::Predicting())
        {
            return;
        }

        if (fromBomb)
        {
            BombDamageDealt = UncheckedAddInt32(
                BombDamageDealt,
                std::bit_cast<std::int32_t>(amount));
            UncheckedIncrement(BombDamageHits);
        }
        else
        {
            const std::int32_t beamIndex = static_cast<std::int32_t>(beam);
            if (beamIndex >= 0
                && beamIndex < static_cast<std::int32_t>(DamageByBeam.size()))
            {
                const std::size_t index = static_cast<std::size_t>(beamIndex);
                DamageByBeam[index] = UncheckedAddInt32(
                    DamageByBeam[index],
                    std::bit_cast<std::int32_t>(amount));
                UncheckedIncrement(HitsByBeam[index]);
            }
        }

        const std::int32_t slot = victim.SlotIndex();
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        _sequence[index] = static_cast<std::uint8_t>(_sequence[index] + 1U);
        UncheckedIncrement(Resolved[index]);
        _attacker[index] = attacker != nullptr
            && attacker->SlotIndex() >= 0 && attacker->SlotIndex() < Slots
            ? static_cast<std::uint8_t>(attacker->SlotIndex())
            : NoSlot;
        _beam[index] = beam == MphRead::BeamType::None
            ? NoBeam
            : static_cast<std::uint8_t>(beam);
        _flags[index] = static_cast<std::uint8_t>(
            static_cast<std::int32_t>(flags) & RelayedFlags);
        _direction[index] = ClampImpulse(
            direction.value_or(OpenTK::Mathematics::Vector3::Zero));
    }

    OpenTK::Mathematics::Vector3 NetDamage::ClampImpulse(
        OpenTK::Mathematics::Vector3 impulse)
    {
        if (!std::isfinite(impulse.X) || !std::isfinite(impulse.Y)
            || !std::isfinite(impulse.Z))
        {
            return OpenTK::Mathematics::Vector3::Zero;
        }

        const float length = Length(impulse);
        if (length <= MaxImpulse)
        {
            return impulse;
        }

        std::string message = "knockback clamped from ";
        message += FormatZeroPointHashHash(length);
        message += " to ";
        message += FormatZeroPointHashHash(MaxImpulse);
        NetLog::Event(message);

        return Multiply(impulse, MaxImpulse / length);
    }

    void NetDamage::SaveScores()
    {
        std::copy_n(GameState::Points().begin(), Slots, _savedPoints.begin());
        std::copy_n(GameState::Kills().begin(), Slots, _savedKills.begin());
        std::copy_n(GameState::Deaths().begin(), Slots, _savedDeaths.begin());
    }

    void NetDamage::RestoreScores()
    {
        std::copy_n(_savedPoints.begin(), Slots, GameState::Points().begin());
        std::copy_n(_savedKills.begin(), Slots, GameState::Kills().begin());
        std::copy_n(_savedDeaths.begin(), Slots, GameState::Deaths().begin());
    }

    void NetDamage::Write(std::int32_t slot, PlayerState& state)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        state.DamageSeq = _sequence[index];
        state.AttackerSlot = _attacker[index];
        state.DamageBeam = _beam[index];
        state.DamageFlags = _flags[index];
        state.HitDirection = _direction[index];
    }

    void NetDamage::Replay(Entities::PlayerEntity& player, const PlayerState& state)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || slot >= Slots)
        {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(slot);

        if (!_everSeen[index])
        {
            _everSeen[index] = true;
            _lastSeen[index] = state.DamageSeq;
            return;
        }

        const std::uint8_t landed = static_cast<std::uint8_t>(
            static_cast<std::int32_t>(state.DamageSeq)
            - static_cast<std::int32_t>(_lastSeen[index]));
        if (landed == 0)
        {
            return;
        }

        _lastSeen[index] = state.DamageSeq;
        if (landed > MaxCatchUp)
        {
            std::string message = "slot ";
            message += Int32ToString(slot);
            message += " damage sequence jumped ";
            message += ByteToString(landed);
            message += "; resynced";
            NetLog::Event(message);
            return;
        }

        Replayed[index] = UncheckedAddInt32(
            Replayed[index], static_cast<std::int32_t>(landed));
        const bool lethal = state.Health == 0;
        const bool mine = static_cast<std::int32_t>(state.AttackerSlot)
            == NetHooks::LocalSlot();
        const bool predicted = mine
            && NetHitPrediction::Confirm(slot, landed);

        if (player.Health() <= 0)
        {
            return;
        }

        Entities::PlayerEntity* attacker = nullptr;
        if (static_cast<std::size_t>(state.AttackerSlot)
            < Entities::PlayerEntity::Players().size())
        {
            attacker = Entities::PlayerEntity::Players()[
                static_cast<std::size_t>(state.AttackerSlot)].get();
        }

        if (predicted && !lethal)
        {
            return;
        }

        std::int32_t amount = std::max<std::int32_t>(
            1, UncheckedSubtractInt32(
                player.Health(), static_cast<std::int32_t>(state.Health)));
        if (!lethal)
        {
            amount = std::min<std::int32_t>(
                amount,
                std::max<std::int32_t>(
                    1, UncheckedSubtractInt32(player.Health(), 1)));
        }

        Entities::DamageFlags flags = static_cast<Entities::DamageFlags>(
            static_cast<std::int32_t>(state.DamageFlags))
            | Entities::DamageFlags::NoDmgInvuln;
        if (lethal)
        {
            flags |= Entities::DamageFlags::Death;
        }

        const OpenTK::Mathematics::Vector3 impulse
            = ClampImpulse(state.HitDirection);
        const std::optional<OpenTK::Mathematics::Vector3> direction
            = IsZero(impulse)
            ? std::nullopt
            : std::optional<OpenTK::Mathematics::Vector3>(impulse);

        _replaying = true;
        _replayBeam = state.DamageBeam == NoBeam
            ? MphRead::BeamType::None
            : static_cast<MphRead::BeamType>(
                std::bit_cast<std::int8_t>(state.DamageBeam));
        SaveScores();

        try
        {
            player.TakeDamage(
                static_cast<std::uint32_t>(amount), flags, direction, attacker);
        }
        catch (...)
        {
            RestoreScores();
            _replaying = false;
            _replayBeam = MphRead::BeamType::None;
            throw;
        }

        RestoreScores();
        _replaying = false;
        _replayBeam = MphRead::BeamType::None;
    }
}
