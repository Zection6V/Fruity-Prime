#pragma once

#include "../../Formats/Enums.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::Entities
{
    class BeamProjectileEntity;
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    enum class ShotAttemptResult : std::int32_t
    {
        Spawned, AttachedEnemy, Cooldown, AutofireCooldown, GunLowered, NoAmmo,
        NoProjectileSlot, DeadOrNotInPlay, StaleLife, OtherNoSpawn
    };

    // ShotAttemptResult.ToString().
    [[nodiscard]] std::string ToString(ShotAttemptResult value);

    // Diagnostic identity only: no extra wire fields or gameplay authority.
    struct ShotKey final
    {
        std::uint64_t AuthorityEpoch = 0;
        std::uint16_t MatchId = 0;
        std::int32_t ShooterSlot = 0;
        std::uint16_t Generation = 0;
        std::uint16_t LifeId = 0;
        std::uint32_t LaunchFrame = 0;

        constexpr ShotKey() noexcept = default;
        constexpr ShotKey(std::uint64_t authorityEpoch, std::uint16_t matchId, std::int32_t shooterSlot,
            std::uint16_t generation, std::uint16_t lifeId, std::uint32_t launchFrame) noexcept
            : AuthorityEpoch(authorityEpoch), MatchId(matchId), ShooterSlot(shooterSlot),
              Generation(generation), LifeId(lifeId), LaunchFrame(launchFrame)
        {
        }

        [[nodiscard]] static ShotKey For(std::int32_t slot, std::uint32_t frame);
        [[nodiscard]] std::string ToString() const;

        [[nodiscard]] friend constexpr bool operator==(const ShotKey& left, const ShotKey& right) noexcept = default;
    };

    class NetShotDiagnostics final
    {
    public:
        NetShotDiagnostics() = delete;

        static constexpr std::int32_t WeaponCount = static_cast<std::int32_t>(::MphRead::BeamType::Enemy) + 2;
        static constexpr std::int32_t ResultCount = 10;
        using Counts = std::array<std::int64_t, WeaponCount>;

        static std::array<std::array<std::int64_t, ResultCount>, WeaponCount> Outcomes;
        static Counts LocalHits;
        static Counts AuthorityHits;
        static Counts Predictions;
        static Counts Claims;
        static Counts Rescues;
        static Counts Refusals;
        static Counts PredictedDamage;
        static Counts AuthorityDamage;
        static Counts LocalHeadshots;
        static Counts AuthorityHeadshots;
        static Counts RewindSamples;
        static Counts RewindFrames;
        static Counts RewindClamps;

        static Counts ContinuousTicks;
        static Counts ContinuousDamageTicks;
        static Counts ContinuousAcquired;
        static Counts ContinuousAmmo;
        static Counts DrainCredit;

        static void Continuous(::MphRead::Entities::BeamProjectileEntity& beam, std::int32_t ammo);
        [[nodiscard]] static std::int32_t Bucket(::MphRead::BeamType weapon) noexcept;
        static void Trace(std::string_view stage, const ShotKey& key, ::MphRead::BeamType weapon,
            std::string_view detail = {});
        // Exactly one outcome for every TryFireWeapon invocation, including
        // early returns.
        static bool Finish(::MphRead::Entities::PlayerEntity& shooter, ShotAttemptResult result,
            OpenTK::Mathematics::Vector3 shot = {}, OpenTK::Mathematics::Vector3 aim = {});
        static void Reset();
        [[nodiscard]] static std::string Describe();
    };
}
