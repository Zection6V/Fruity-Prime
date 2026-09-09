#pragma once

// Native counterpart of the plain record types declared beside the entity
// classes in Entities/.  The entity behaviour itself lives in the matching
// native units; these are the data those units carry.  A C# reference member
// becomes a pointer here.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::entities {

using namespace fruityprime::formats;

class EntityBase;
class EntityCollision;

// BeamEffectEntity.cs
struct BeamEffectEntityData {
    std::int32_t Type{};
    bool NoSplat{};
    formats::Matrix4 Transform{};
    EntityCollision* EntityCollision{};
};

// EnemyInstanceEntity.cs
struct EnemyInstanceEntityData {
    EnemyType Type{};
    EntityBase* Spawner{};
};

// EnemyInstanceEntity.cs
// EnemyBehavior<T> is generic over the enemy entity its predicate drives, and
// carries that predicate alongside the state to move to.
template <typename T>
struct EnemyBehavior {
    std::uint8_t NextState{};
    bool (*Function)(T&) = nullptr;
};

// ItemInstanceEntity.cs
struct ItemInstanceEntityData {
    formats::Vector3 Position{};
    ItemType ItemType{};
    std::int32_t DespawnTimer{};
};

// ItemInstanceEntity.cs
struct FhItemInstanceEntityData {
    formats::Vector3 Position{};
    FhItemType ItemType{};
};

// PlayerCollision.cs
struct DamageResult {
    bool TakeDamage{};
    std::uint32_t Damage{};
};

// PlayerEntity.cs
struct PlayerValues {
    Hunter Hunter{};
    std::int32_t WalkBipedTraction{};
    std::int32_t StrafeBipedTraction{};
    std::int32_t WalkSpeedCap{};
    std::int32_t StrafeSpeedCap{};
    std::int32_t AltMinHSpeed{};
    std::int32_t BoostSpeedCap{};
    std::int32_t BipedGravity{};
    std::int32_t AltAirGravity{};
    std::int32_t AltGroundGravity{};
    std::int32_t JumpSpeed{};
    std::int32_t WalkSpeedFactor{};
    std::int32_t AltGroundSpeedFactor{};
    std::int32_t StrafeSpeedFactor{};
    std::int32_t AirSpeedFactor{};
    std::int32_t StandSpeedFactor{};
    std::int32_t RollAltTraction{};
    std::int32_t AltColRadius{};
    std::int32_t AltColYPos{};
    std::uint16_t BoostChargeMin{};
    std::uint16_t BoostChargeMax{};
    std::int32_t BoostSpeedMin{};
    std::int32_t BoostSpeedMax{};
    std::int32_t AltHSpeedCapIncrement{};
    std::int32_t Field58{};
    std::int32_t Field5C{};
    std::int32_t WalkBobMax{};
    std::int32_t AimDistance{};
    std::uint16_t CamSwitchTime{};
    std::uint16_t Padding6A{};
    std::int32_t NormalFov{};
    std::int32_t ZoomSensitivityFactor{};
    std::int32_t AimYOffset{};
    std::int32_t Field78{};
    std::int32_t Field7C{};
    std::int32_t Field80{};
    std::int32_t Field84{};
    std::int32_t Field88{};
    std::int32_t Field8C{};
    std::int32_t Field90{};
    std::int32_t MinPickupHeight{};
    std::int32_t MaxPickupHeight{};
    std::int32_t BipedColRadius{};
    std::int32_t LockOnTolerance{};
    std::int32_t LockOnMinDistance{};
    std::int32_t LockOnMaxDistance{};
    std::int16_t DamageInvuln{};
    std::uint16_t DamageFlashTime{};
    std::int32_t FieldB0{};
    std::int32_t FieldB4{};
    std::int32_t FieldB8{};
    std::int32_t MuzzleOffset{};
    std::int32_t BombCooldown{};
    std::int32_t BombSelfRadius{};
    std::int32_t BombSelfRadiusSquared{};
    std::int32_t BombRadius{};
    std::int32_t BombRadiusSquared{};
    std::int32_t BombJumpSpeed{};
    std::int32_t BombRefillTime{};
    std::int16_t BombDamage{};
    std::int16_t BombEnemyDamage{};
    std::int16_t LockOnSnapTime{};
    std::int16_t SpawnInvulnerability{};
    std::uint16_t AimMinTouchTime{};
    std::uint16_t PaddingE6{};
    std::int32_t AutoAimFindTolerance{};
    std::int32_t AutoAimHoldTolerance{};
    std::int32_t SwayStartTime{};
    std::int32_t SwayIncrement{};
    std::int32_t SwayLimit{};
    std::int32_t GunIdleTime{};
    std::int16_t MpAmmoCap{};
    std::uint8_t AmmoRecharge{};
    std::uint8_t Padding103{};
    std::uint16_t EnergyTank{};
    std::int16_t AmmoTank{};
    std::uint8_t AltFormStrafe{};
    std::uint8_t Padding109{};
    std::uint16_t Padding10A{};
    std::int32_t FallDamageSpeed{};
    std::int32_t FallDamageMax{};
    std::int32_t ViewTiltIncrement{};
    std::int32_t ViewTiltFactor{};
    std::int32_t JumpPadSlideFactor{};
    std::int32_t AltTiltAngleCap{};
    std::int32_t AltMinWobble{};
    std::int32_t AltMaxWobble{};
    std::int32_t AltMinSpinAccel{};
    std::int32_t AltMaxSpinAccel{};
    std::int32_t AltMinSpinSpeed{};
    std::int32_t AltMaxSpinSpeed{};
    std::int32_t AltTiltAngleMax{};
    std::int32_t AltBounceWobble{};
    std::int32_t AltBounceTilt{};
    std::int32_t AltBounceSpin{};
    std::int32_t AltAttackKnockbackAccel{};
    std::int16_t AltAttackKnockbackTime{};
    std::uint16_t AltAttackStartup{};
    std::int32_t Field154{};
    std::int32_t Field158{};
    std::int32_t LungeHSpeed{};
    std::int32_t LungeVSpeed{};
    std::uint16_t AltAttackDamage{};
    std::int16_t AltAttackCooldown{};
};

} // namespace fruityprime::entities
