#pragma once

// Native counterpart of the per-player runtime state in
// Entities/Players/PlayerEntity.cs.
//
// The native simulation is organised around gameplay::Session and
// net::PlayerState, which carry what the wire needs.  This record carries what
// the managed PlayerEntity additionally keeps per player: the hunter's tuning
// constants, the form and animation state, and the references to the models
// and entities it owns.  A C# reference member becomes a pointer.
//
// Members marked `property` or `computed` are auto-properties or expression
// properties in the managed class; they are storage here so the behaviour
// port has one place to write them.

#include "Formats/Types.hpp"
#include "Formats/enum_tables.hpp"
#include "Entities/entity_records.hpp"
#include "Formats/formats_layouts.hpp"
#include "Metadata/metadata.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::players {

using namespace fruityprime::formats;
using namespace fruityprime::entities;

// Engine objects the player holds references to, implemented elsewhere.
class EntityBase;
class HalfturretEntity;
class ModelInstance;
class AvailableArray;
class BombEntity;
class EnemyInstanceEntity;
class EnemySpawnEntity;
class EquipInfo;
class JumpPadEntity;
class MorphCameraEntity;
class OctolithFlagEntity;

struct PlayerRuntimeState {
    ModelInstance* BipedModel2{};
    ModelInstance* DoubleDamageModel{};
    std::int32_t DoubleDmgBindingId{};
    std::vector<formats::Vector3> KandenSegPos;
    bool IsMainPlayer{};
    std::int32_t HealthMax{};
    AvailableArray* AvailableWeapons{};
    std::vector<metadata::Effectiveness> BeamEffectiveness;
    bool IsPrimeHunter{};
    CollisionVolume Volume{};
    float Field70{};
    float Field74{};
    bool Field6D0{};
    HalfturretEntity* Halfturret{};
    EntityBase* BurnedBy{};
    EntityBase* ShockCoilTarget{};
    bool IsAltForm{};
    bool IsMorphing{};
    bool IsUnmorphing{};
    float DeathCountdown{};
    bool DoubleDamage{};
    std::uint16_t ShockCoilTimer{};
    float CurAlpha{1.0F};
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
    std::uint8_t SyluxBombCount{};  // property
    std::vector<BombEntity*> SyluxBombs;  // property
    EquipInfo* EquipInfo{};  // property
    BeamType CurrentWeapon{};  // property
    BeamType PreviousWeapon{};  // property
    BeamType WeaponSelection{};  // property
    GunAnimation GunAnimation{};  // property
    bool SwipeBoostRequested{};  // property
    float SwipeBoostX{};  // property
    float SwipeBoostY{};  // property
    Team Team{};  // property
    std::int32_t TeamIndex{-1};  // property
    std::int32_t SlotIndex{};  // property
    bool IsBot{};  // property
    LoadFlags LoadFlags{};  // property
    PlayerValues Values{};  // property
    EnemySpawnEntity* EnemySpawner{};  // property
    EnemyInstanceEntity* AttachedEnemy{};  // property
    EntityBase* Field35C{};
    JumpPadEntity* LastJumpPad{};
    EntityBase* LastTarget{};
    MorphCameraEntity* MorphCamera{};  // property
    OctolithFlagEntity* OctolithFlag{};  // property
    PlayerFlags1 Flags1{};  // property
    PlayerFlags2 Flags2{};  // property
    formats::Vector3 Speed{};  // property
    formats::Vector3 Acceleration{};  // property
    formats::Vector3 PrevSpeed{};  // property
    formats::Vector3 PrevPosition{};  // property
    formats::Vector3 IdlePosition{};  // property
    std::uint16_t TimeSinceShot{};  // property
    std::uint16_t RespawnTimer{};  // property
    bool IgnoreItemPickups{};  // property
    formats::Vector3 ForcedSpawnPos{};  // property
    bool ReloadInit{};  // property
};

} // namespace fruityprime::players
