#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"
#include "NetProtocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace MphRead::Mods::Network
{
    class NetDamage final
    {
    public:
        NetDamage() = delete;

        static constexpr std::uint8_t NoSlot = 0xFF;
        static constexpr std::uint8_t NoBeam = 0xFF;

        [[nodiscard]] static bool Replaying() noexcept { return _replaying; }
        [[nodiscard]] static MphRead::BeamType ReplayBeam() noexcept { return _replayBeam; }

        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> Resolved{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> Replayed{};

        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> Fired{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> PlayerChecks{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> PlayerOverlaps{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> PlayerAccepted{};
        inline static std::array<
            std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity>,
            Entities::PlayerEntity::SlotCapacity> PlayerOverlapsByShooter{};

        inline static std::array<double, Entities::PlayerEntity::SlotCapacity> AimDrift{};
        inline static std::array<double, Entities::PlayerEntity::SlotCapacity> WorstDrift{};

        inline static std::int32_t ShockCoilSpawned = 0;
        inline static std::int32_t ShockCoilAcquired = 0;
        inline static std::int32_t BombPlayerChecks = 0;
        inline static std::int32_t BombTeamSkips = 0;
        inline static std::int32_t BombHits = 0;

        inline static std::array<std::int32_t, static_cast<std::size_t>(MphRead::BeamType::Enemy) + 1>
            DamageByBeam{};
        inline static std::array<std::int32_t, static_cast<std::size_t>(MphRead::BeamType::Enemy) + 1>
            HitsByBeam{};
        inline static std::int32_t BombDamageDealt = 0;
        inline static std::int32_t BombDamageHits = 0;

        inline static std::int32_t BombSpawnCalls = 0;
        inline static std::int32_t BombSpawnMade = 0;
        inline static std::int32_t BombSpawnDetonated = 0;
        inline static std::int32_t BombSpawnStaleCount = 0;
        inline static std::int32_t BombSpawnPoolEmpty = 0;

        inline static float BombNearest = std::numeric_limits<float>::max();
        inline static float BombRadiusSeen = 0.0F;

        static void NoteFired(Entities::PlayerEntity& shooter,
            OpenTK::Mathematics::Vector3 shotVec, OpenTK::Mathematics::Vector3 aimVec);
        static void NotePlayerOverlap(Entities::EntityBase* owner, Entities::PlayerEntity& target);

        static void ResetForRoomChange();
        static void ForgetSlot(std::int32_t slot);
        static void Reset();

        [[nodiscard]] static bool Suppress(Entities::PlayerEntity& victim,
            Entities::EntityBase* source, Entities::DamageFlags flags);

        static void Note(Entities::PlayerEntity& victim, Entities::PlayerEntity* attacker,
            MphRead::BeamType beam, Entities::DamageFlags flags,
            std::optional<OpenTK::Mathematics::Vector3> direction,
            std::uint32_t amount = 0, bool fromBomb = false);

        static void Write(std::int32_t slot, PlayerState& state);
        static void Replay(Entities::PlayerEntity& player, const PlayerState& state);

    private:
        static constexpr std::int32_t Slots = Entities::PlayerEntity::SlotCapacity;
        static constexpr std::int32_t RelayedFlags
            = static_cast<std::int32_t>(Entities::DamageFlags::Headshot)
            | static_cast<std::int32_t>(Entities::DamageFlags::Deathalt)
            | static_cast<std::int32_t>(Entities::DamageFlags::Burn);
        static constexpr std::uint8_t MaxCatchUp = 32;
        static constexpr float MaxImpulse = 1.5F;

        inline static std::array<std::uint8_t, Slots> _sequence{};
        inline static std::array<std::uint8_t, Slots> _attacker{};
        inline static std::array<std::uint8_t, Slots> _beam{};
        inline static std::array<std::uint8_t, Slots> _flags{};
        inline static std::array<OpenTK::Mathematics::Vector3, Slots> _direction{};

        inline static std::array<std::uint8_t, Slots> _lastSeen{};
        inline static std::array<bool, Slots> _everSeen{};

        inline static bool _replaying = false;
        inline static MphRead::BeamType _replayBeam = MphRead::BeamType::None;

        inline static std::array<std::int32_t, Slots> _savedPoints{};
        inline static std::array<std::int32_t, Slots> _savedKills{};
        inline static std::array<std::int32_t, Slots> _savedDeaths{};

        [[nodiscard]] static OpenTK::Mathematics::Vector3 ClampImpulse(
            OpenTK::Mathematics::Vector3 impulse);
        static void SaveScores();
        static void RestoreScores();
    };
}
