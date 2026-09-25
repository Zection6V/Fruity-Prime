#include "NetDamage.hpp"

#include "../../GameState.hpp"
#include "NetHitPrediction.hpp"
#include "NetHooks.hpp"
#include "NetLog.hpp"
#include "NetSession.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"
#include "NativeRuntime/System/Globalization.hpp"

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
#endif

using ::MphRead::NativeRuntime::IncrementInPlace;
using ::MphRead::NativeRuntime::MathClamp;
using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedSubtract;
using ::OpenTK::Mathematics::IsZero;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::Multiply;

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

        IncrementInPlace(Fired[static_cast<std::size_t>(slot)]);
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
            IncrementInPlace(PlayerOverlapsByShooter[
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
            BombDamageDealt = UncheckedAdd(
                BombDamageDealt,
                std::bit_cast<std::int32_t>(amount));
            IncrementInPlace(BombDamageHits);
        }
        else
        {
            const std::int32_t beamIndex = static_cast<std::int32_t>(beam);
            if (beamIndex >= 0
                && beamIndex < static_cast<std::int32_t>(DamageByBeam.size()))
            {
                const std::size_t index = static_cast<std::size_t>(beamIndex);
                DamageByBeam[index] = UncheckedAdd(
                    DamageByBeam[index],
                    std::bit_cast<std::int32_t>(amount));
                IncrementInPlace(HitsByBeam[index]);
            }
        }

        const std::int32_t slot = victim.SlotIndex();
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        _sequence[index] = static_cast<std::uint8_t>(_sequence[index] + 1U);
        IncrementInPlace(Resolved[index]);
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
        message += ::MphRead::NativeRuntime::ToString(length, "0.##");
        message += " to ";
        message += ::MphRead::NativeRuntime::ToString(MaxImpulse);
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
            message += ::MphRead::NativeRuntime::ToString(slot);
            message += " damage sequence jumped ";
            message += ::MphRead::NativeRuntime::ToString(landed);
            message += "; resynced";
            NetLog::Event(message);
            return;
        }

        Replayed[index] = UncheckedAdd(
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
            1, UncheckedSubtract(
                player.Health(), static_cast<std::int32_t>(state.Health)));
        if (!lethal)
        {
            amount = std::min<std::int32_t>(
                amount,
                std::max<std::int32_t>(
                    1, UncheckedSubtract(player.Health(), 1)));
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
