#pragma once

#include "../Formats/Enums.hpp"
#include "../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead::Testing
{
    class TestPlayer final
    {
    public:
        struct ButtonControl
        {
            const ButtonFlags Button{};
            const PressFlags Flags{};
        };

        struct PlayerControls
        {
            const std::uint32_t Flags{};
            const ButtonControl Left{};
            const ButtonControl Right{};
            const ButtonControl Up{};
            const ButtonControl Down{};
            const ButtonControl Field14{};
            const ButtonControl Field18{};
            const ButtonControl Field1C{};
            const ButtonControl Field20{};
            const ButtonControl Field24{};
            const ButtonControl Field28{};
            const ButtonControl Field2C{};
            const ButtonControl Field30{};
            const ButtonControl Shoot{};
            const ButtonControl Jump{}; // repeated touch is checked separately
            const ButtonControl Morph{};
            const ButtonControl Field40{};
            const ButtonControl Field44{};
            const ButtonControl Bomb{};
            const ButtonControl Unused4C{};
            const ButtonControl BoostCharge{};
            const ButtonControl Unused54{};
            const ButtonControl Unused58{};
            const ButtonControl Unused5C{};
            const ButtonControl Unused60{};
            const ButtonControl NoxusAltAttack{};
            const ButtonControl Unused68{}; // unused alt attack
            const ButtonControl SpireAltAttack{};
            const ButtonControl TraceAltAttack{};
            const ButtonControl Unused74{}; // unused alt attack
            const ButtonControl WeavelAltAttack{};
            const ButtonControl Zoom{};
            const ButtonControl Respawn{};
            const ButtonControl Unused84{};
            const std::int32_t Field88{};
            const std::int32_t Field8C{};
            const std::int32_t Field90{};
            const std::int32_t Field94{};
            const std::int32_t Field98{};
        };

        struct PlayerValues
        {
            const Fixed BipedTractionLr{};
            const Fixed BipedTractionFb{};
            const Fixed WalkHSpeedCap{};
            const Fixed StrafeHSpeedCap{};
            const Fixed MinAltHSpeed{}; // boost is considered over if speed drops below this -- also minimum(?) for alt movement SFX
            const Fixed BoostHSpeedCap{};
            const Fixed BipedGravity{}; // gravity to apply to biped form in the air or on slippery terrain
            const Fixed AltGravityAir{}; // gravity to apply to alt form in the air
            const Fixed AltGravityGround{}; // gravity to apply to alt form on the ground
            const Fixed JumpSpeed{}; // 1433 (0.35) is used if the player is prime hunter
            const Fixed WalkSpeedFactor{};
            const Fixed AltGroundSpeedFactor{};
            const Fixed StrafeSpeedFactor{};
            const Fixed AirSpeedFactor{};
            const Fixed StandSpeedFactor{}; // grounded, biped, "moving" flag bit not set
            const Fixed Field3C{}; // alt touch movement factor when grounded gravity
            const Fixed AltCollisionRadius{};
            const Fixed AltCollisionY{};
            const std::uint16_t BoostChargeMin{};
            const std::uint16_t BoostChargeMax{};
            const Fixed BoostSpeedMin{};
            const Fixed BoostSpeedMax{};
            const Fixed AltSpeedCapHInc{}; // if player's h speed is greater than the cap, reduce it by this amount
            const Fixed Field58{}; // h speed in "gravity" alt form at which aim direction is updated
            const Fixed Field5C{}; // h speed in "gravity" alt form at which movement direction (for lost octolith position?) is updated
            const Fixed WalkViewBob{};
            const std::uint32_t Field64{}; // aim factor or something
            const std::uint16_t Field68{}; // gun idle time -- but also FieldFC
            const std::uint16_t Padding6A{};
            const Fixed RegularFov{}; // 39
            const std::uint32_t Field70{}; // camera-related
            const std::uint32_t Field74{}; // aim y offset or something
            const std::uint32_t Field78{}; // camera-related (x?)
            const std::uint32_t Field7C{}; // camera-related (y?)
            const std::uint32_t Field80{}; // camera-related (z?)
            const std::uint32_t Field84{}; // camera-related
            const std::uint32_t Field88{}; // camera-related
            const std::uint32_t Field8C{}; // camera-related
            const std::uint32_t Field90{}; // camera-related
            const Fixed MinCollisionHeight{}; // cylinder bottom
            const Fixed MaxCollisionHeight{}; // cylinder top
            const Fixed BipedCollisionRadius{};
            const Fixed FieldA0{};
            const Fixed FieldA4{};
            const Fixed FieldA8{};
            const std::uint16_t DamageInvuln{};
            const std::uint16_t DamageFlashDuration{};
            const Fixed FieldB0{}; // gun draw offset/factor - gun eff_vec_2
            const Fixed FieldB4{}; // gun draw offset/factor - player vec_1
            const Fixed FieldB8{}; // gun draw offset/factor - player vec_2
            const Fixed SmokeZOffset{};
            const std::uint32_t BombCooldown{};
            const Fixed BombSelfRadius{};
            const Fixed BombSelfRadiusSquared{}; // set in load_some_hunter_stuff
            const Fixed BombRadius{};
            const Fixed BombRadiusSquared{}; // set in load_some_hunter_stuff
            const Fixed BombJumpSpeed{};
            const std::uint32_t BombRefillTime{};
            const std::uint16_t BombDamage{};
            const std::uint16_t BombEnemyDamage{};
            const std::uint16_t FieldE0{};
            const std::uint16_t SpawnInvuln{};
            const std::uint16_t FieldE4{}; // some touch duration threshold
            const std::uint16_t PaddingE6{};
            const std::uint32_t FieldE8{};
            const std::uint32_t FieldEC{};
            const std::uint32_t ViewSwayStartTime{};
            const std::uint32_t SwayTimeIncrement{};
            const std::uint32_t SwayLimit{};
            const std::uint32_t FieldFC{}; // gun idle time -- but also Field68
            const std::uint16_t MpAmmoCap{};
            const std::uint8_t AmmoRecharge{}; // FH leftover, non-functional due to empty weapons being automatically unequipped
            const std::uint8_t Padding103{};
            const std::uint16_t EnergyStart{};
            const std::uint16_t Field106{};
            const std::uint8_t AltGroundNoGravity{}; // don't apply gravity to alt form on ground unless terrain is slippery
            const std::uint8_t Padding109{};
            const std::uint16_t Padding10A{};
            const Fixed FallDamageSpeed{}; // absolute value
            const std::uint32_t FallDamageMax{}; // 80% of this is the actual max
            const std::uint32_t Field114{}; // some kind of camera bob value for movement
            const std::uint32_t Field118{};
            const Fixed JumpPadSlideFactor{};
            const std::uint32_t Field120{};
            const std::uint32_t Field124{};
            const std::uint32_t Field128{};
            const Fixed AltSpinSpeed{}; // used for noxus
            const std::uint32_t Field130{};
            const std::uint32_t Field134{};
            const std::uint32_t Field138{};
            const std::uint32_t Field13C{};
            const Fixed Field140{}; // 140/144/148 are related to spin/wobble when noxus alt form lands
            const Fixed Field144{};
            const Fixed Field148{};
            const std::uint32_t Field14C{}; // speed loss value
            const std::uint16_t Field150{}; // speed counter value
            const std::uint16_t NoxusAltAttackStartup{}; // SFX played at halfway time, anim frame based on elapsed percentage
            const std::uint32_t Field154{};
            const std::uint32_t Field158{};
            const Fixed LungeHSpeed{}; // lunge = trace/weavel
            const Fixed LungeVSpeed{};
            const std::uint16_t AltAttackDamage{}; // includes boost
            const std::uint16_t AltAttackCooldown{}; // includes boost
        };

        [[nodiscard]] static std::shared_ptr<const std::vector<PlayerControls>> GetPlayerControls();
        [[nodiscard]] static std::shared_ptr<const std::vector<PlayerValues>> GetPlayerValues();

    private:
        TestPlayer() = delete;
        TestPlayer(const TestPlayer&) = delete;
        TestPlayer& operator=(const TestPlayer&) = delete;
        TestPlayer(TestPlayer&&) = delete;
        TestPlayer& operator=(TestPlayer&&) = delete;
    };
}
