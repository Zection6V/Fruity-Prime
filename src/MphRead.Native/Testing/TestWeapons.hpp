#pragma once

#include "../Formats/Types.hpp"
#include "../Metadata/Weapons.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace MphRead::Testing
{
    class TestWeapons final
    {
    public:
        struct RawWeaponInfo
        {
            const BeamType Beam{};
            const BeamType BeamKind{};
            const std::shared_ptr<ManagedArray<std::uint8_t>> DrawFuncIds{};
            const std::shared_ptr<ManagedArray<std::uint16_t>> Colors{};
            const std::uint32_t FlagsAndPriority{};
            const std::uint16_t SplashDamage{};
            const std::uint16_t MinChargeSplashDamage{};
            const std::uint16_t ChargedSplashDamage{};
            const std::shared_ptr<ManagedArray<std::uint8_t>> SplashDmgTypes{};
            const std::uint8_t ShotCooldown{};
            const std::uint8_t ShotCooldownRelated{};
            const std::uint8_t AmmoType{};
            const std::shared_ptr<ManagedArray<std::uint8_t>> BeamTypes{};
            const std::shared_ptr<ManagedArray<std::uint8_t>> MuzzleEffects{};
            const std::shared_ptr<ManagedArray<std::uint8_t>> DmgDirTypes{};
            const std::shared_ptr<ManagedArray<std::uint8_t>> DamageInterpolations{};
            const std::shared_ptr<ManagedArray<Affliction>> Afflictions{};
            const std::uint8_t Padding21{};
            const std::uint16_t MinCharge{};
            const std::uint16_t FullCharge{};
            const std::uint16_t AmmoCost{};
            const std::uint16_t MinChargeCost{};
            const std::uint16_t ChargeCost{};
            const std::uint16_t UnchargedDamage{};
            const std::uint16_t MinChargeDamage{};
            const std::uint16_t ChargedDamage{};
            const std::uint16_t HeadshotDamage{};
            const std::uint16_t MinChargeHeadshotDamage{};
            const std::uint16_t ChargedHeadshotDamage{};
            const std::uint16_t UnchargedLifespan{};
            const std::uint16_t MinChargeLifespan{};
            const std::uint16_t ChargedLifespan{};
            const std::shared_ptr<ManagedArray<std::uint16_t>> SpeedDecay{};
            const std::uint16_t Padding42{};
            const std::shared_ptr<ManagedArray<std::uint16_t>> SpeedInterp{};
            const std::int32_t UnchargedDmgDirMag{};
            const std::int32_t MinChargeDmgDirMag{};
            const std::int32_t ChargedDmgDirMag{};
            const std::int32_t ZoomFov{};
            const std::int32_t UnchargedCylRadius{};
            const std::int32_t MinChargeCylRadius{};
            const std::int32_t ChargedCylRadius{};
            const std::int32_t UnchargedSpeed{};
            const std::int32_t MinChargeSpeed{};
            const std::int32_t ChargedSpeed{};
            const std::int32_t UnchargedFinalSpeed{};
            const std::int32_t MinChargeFinalSpeed{};
            const std::int32_t ChargedFinalSpeed{};
            const std::int32_t UnchargedGravity{};
            const std::int32_t MinChargeGravity{};
            const std::int32_t ChargedGravity{};
            const std::int32_t UnchargedHoming{};
            const std::int32_t MinChargeHoming{};
            const std::int32_t ChargedHoming{};
            const std::int32_t HomingRange{};
            const std::int32_t HomingTolerance{};
            const std::int32_t UnchargedScale{};
            const std::int32_t MinChargeScale{};
            const std::int32_t ChargedScale{};
            const std::int32_t UnchargedDistance{};
            const std::int32_t MinChargeDistance{};
            const std::int32_t ChargedDistance{};
            const std::int32_t UnchargedSpread{};
            const std::int32_t MinChargeSpread{};
            const std::int32_t ChargedSpread{};
            const std::int32_t UnchargedRicochetLossH{};
            const std::int32_t MinChargeRicochetLossH{};
            const std::int32_t ChargedRicochetLossH{};
            const std::int32_t UnchargedRicochetLossV{};
            const std::int32_t MinChargeRicochetLossV{};
            const std::int32_t ChargedRicochetLossV{};
            const std::shared_ptr<ManagedArray<std::uint32_t>> RicochetWeaponPtr{};
            const std::uint16_t ProjectileCount{};
            const std::uint16_t MinChargedProjectileCount{};
            const std::uint16_t ChargeProjectileCount{};
            const std::uint16_t SmokeStart{};
            const std::uint16_t SmokeMinimum{};
            const std::uint16_t SmokeDrain{};
            const std::uint16_t SmokeShotAmount{};
            const std::uint16_t SmokeChargeAmount{};

            RawWeaponInfo() noexcept = default;
            RawWeaponInfo(const RawWeaponInfo&) noexcept = default;
            RawWeaponInfo& operator=(const RawWeaponInfo& other) noexcept;

            [[nodiscard]] std::uint8_t Priority() const noexcept;
            [[nodiscard]] WeaponFlags Flags() const noexcept;

        private:
            explicit RawWeaponInfo(std::span<const std::uint8_t, 0xF0> raw);

            template <typename T>
            [[nodiscard]] static T ReadScalar(
                std::span<const std::uint8_t, 0xF0> raw, std::size_t offset) noexcept;

            template <typename T>
            [[nodiscard]] static std::shared_ptr<ManagedArray<T>> ReadArray2(
                std::span<const std::uint8_t, 0xF0> raw, std::size_t offset);

            friend class TestWeapons;
        };

        static void TestWeaponInfo();
        static void DumpWeaponInfo(RawWeaponInfo weapon);

        TestWeapons() = delete;
        TestWeapons(const TestWeapons&) = delete;
        TestWeapons& operator=(const TestWeapons&) = delete;
        TestWeapons(TestWeapons&&) = delete;
        TestWeapons& operator=(TestWeapons&&) = delete;

    private:
        [[nodiscard]] static std::string EnumToString(WeaponFlags value);
        [[nodiscard]] static std::string EnumToString(Affliction value);

        [[nodiscard]] static std::shared_ptr<const std::vector<RawWeaponInfo>> ParseBytes(
            std::int32_t size, std::int32_t count, const std::vector<std::uint8_t>& array);

        [[nodiscard]] static std::shared_ptr<const std::vector<RawWeaponInfo>> GetRicochets();
        [[nodiscard]] static std::shared_ptr<const std::vector<RawWeaponInfo>> Get1PWeapons();
        [[nodiscard]] static std::shared_ptr<const std::vector<RawWeaponInfo>> GetMPWeapons();
        [[nodiscard]] static std::shared_ptr<const std::vector<RawWeaponInfo>> GetEnemyWeapons();
        [[nodiscard]] static std::shared_ptr<const std::vector<RawWeaponInfo>> GetPlatformWeapons();

        static void Nop() noexcept;
    };
}
