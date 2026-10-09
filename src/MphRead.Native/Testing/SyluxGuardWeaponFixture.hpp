#pragma once
#include "../Metadata/Metadata.hpp"

namespace MphRead::Testing
{
    // Controlled fixtures still run production Spawn's charge/phase/ammo logic.
    inline std::shared_ptr<WeaponInfo> GuardWeapon(const WeaponInfo& w, WeaponFlags flags,
        std::uint16_t projectiles)
    {
        return std::make_shared<WeaponInfo>(
            w.Beam, w.BeamKind, w.DrawFuncIds, w.Colors,
            w.Priority, flags, w.SplashDamage, w.MinChargeSplashDamage,
            w.ChargedSplashDamage, w.SplashDamageTypes, w.ShotCooldown, w.AutofireCooldown,
            w.AmmoType, w.CollisionEffects, w.MuzzleEffects, w.DmgDirTypes,
            w.DamageInterpolations, w.Afflictions, w.Padding21, 10,
            20, 10, 20, 40,
            w.UnchargedDamage, w.MinChargeDamage, w.ChargedDamage, w.HeadshotDamage,
            w.MinChargeHeadshotDamage, w.ChargedHeadshotDamage, w.UnchargedLifespan, w.MinChargeLifespan,
            w.ChargedLifespan, w.SpeedDecayTimes, w.Padding42, w.SpeedInterpolations,
            w.UnchargedDmgDirMag, w.MinChargeDmgDirMag, w.ChargedDmgDirMag, w.ZoomFov,
            w.UnchargedCylRadius, w.MinChargeCylRadius, w.ChargedCylRadius, w.UnchargedSpeed,
            w.MinChargeSpeed, w.ChargedSpeed, w.UnchargedFinalSpeed, w.MinChargeFinalSpeed,
            w.ChargedFinalSpeed, w.UnchargedGravity, w.MinChargeGravity, w.ChargedGravity,
            w.UnchargedHoming, w.MinChargeHoming, w.ChargedHoming, w.HomingRange,
            w.HomingTolerance, w.UnchargedSplashRadius, w.MinChargeSplashRadius, w.ChargedSplashRadius,
            w.UnchargedDistance, w.MinChargeDistance, w.ChargedDistance, w.UnchargedSpread,
            w.MinChargeSpread, w.ChargedSpread, w.UnchargedRicochetLossH, w.MinChargeRicochetLossH,
            w.ChargedRicochetLossH, w.UnchargedRicochetLossV, w.MinChargeRicochetLossV, w.ChargedRicochetLossV,
            projectiles, projectiles, projectiles, w.SmokeStart,
            w.SmokeMinimum, w.SmokeDrain, w.SmokeShotAmount, w.SmokeChargeAmount,
            w.Description);
    }
}
