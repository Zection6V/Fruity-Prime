#include "MechanicsDump.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Formats/Enums.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/Player.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../GameState.hpp"
#include "../../Read.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <system_error>

namespace MphRead
{
    namespace
    {
        template <typename T>
        std::string ManagedNumber(T value)
        {
            char buffer[64]{};
            const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
            if (ec != std::errc{})
            {
                throw std::runtime_error("Numeric formatting failed.");
            }
            std::string result(buffer, ptr);
            if constexpr (std::is_floating_point_v<T>)
            {
                const std::size_t exponent = result.find('e');
                if (exponent != std::string::npos)
                {
                    result[exponent] = 'E';
                    std::size_t digits = exponent + 1;
                    if (digits < result.size() && (result[digits] == '+' || result[digits] == '-'))
                    {
                        ++digits;
                    }
                    if (result.size() - digits == 1)
                    {
                        result.insert(digits, 1, '0');
                    }
                }
            }
            return result;
        }

        std::string ManagedValue(bool value)
        {
            return value ? "True" : "False";
        }

        template <typename T>
        std::string ManagedValue(T value)
            requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return ManagedNumber(static_cast<long long>(value));
            }
            else
            {
                return ManagedNumber(static_cast<unsigned long long>(value));
            }
        }

        template <typename T>
        std::string ManagedValue(T value)
            requires std::is_floating_point_v<T>
        {
            return ManagedNumber(value);
        }

        std::string ManagedValue(Hunter value)
        {
            switch (value)
            {
            case Hunter::Samus: return "Samus";
            case Hunter::Kanden: return "Kanden";
            case Hunter::Trace: return "Trace";
            case Hunter::Sylux: return "Sylux";
            case Hunter::Noxus: return "Noxus";
            case Hunter::Spire: return "Spire";
            case Hunter::Weavel: return "Weavel";
            case Hunter::Guardian: return "Guardian";
            case Hunter::Random: return "Random";
            }
            return ManagedValue(static_cast<std::uint8_t>(value));
        }

        std::string ManagedValue(WeaponType value)
        {
            switch (value)
            {
            case WeaponType::PowerBeam: return "PowerBeam";
            case WeaponType::VoltDriver: return "VoltDriver";
            case WeaponType::Missile: return "Missile";
            case WeaponType::Battlehammer: return "Battlehammer";
            case WeaponType::Imperialist: return "Imperialist";
            case WeaponType::Judicator: return "Judicator";
            case WeaponType::Magmaul: return "Magmaul";
            case WeaponType::ShockCoil: return "ShockCoil";
            case WeaponType::OmegaCannon: return "OmegaCannon";
            case WeaponType::Platform: return "Platform";
            case WeaponType::Enemy: return "Enemy";
            }
            return ManagedValue(static_cast<std::int32_t>(value));
        }

        template <typename T>
        void AppendLine(std::string& output, const char* label, T value)
        {
            output += label;
            output += ManagedValue(value);
            output += '\n';
        }

        float Seconds(auto value)
        {
            return static_cast<float>(value) / 30.0f;
        }

        float DegreesFromCosine(auto value)
        {
            return std::acos(Fixed::ToFloat(value)) * (180.0f / std::numbers::pi_v<float>);
        }

        std::string Hex4(auto value)
        {
            using T = decltype(value);
            using U = std::make_unsigned_t<T>;
            char buffer[32]{};
            const auto unsignedValue = static_cast<U>(value);
            const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), unsignedValue, 16);
            if (ec != std::errc{})
            {
                throw std::runtime_error("Numeric formatting failed.");
            }
            std::string result(buffer, ptr);
            for (char& c : result)
            {
                if (c >= 'a' && c <= 'f')
                {
                    c = static_cast<char>(c - 'a' + 'A');
                }
            }
            if (result.size() < 4)
            {
                result.insert(0, 4 - result.size(), '0');
            }
            return result;
        }
    }

    void MechanicsDump::Run()
    {
        std::string output;
        GetWeaponOutput(output);
        GetPlayerOutput(output);
        GetMiscOutput(output);

        const std::string path = Paths::Combine(Paths::FileSystem(), "mechanics.txt");
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path, std::ios::binary | std::ios::trunc);
        file.write(output.data(), static_cast<std::streamsize>(output.size()));
        file.close();
    }

    void MechanicsDump::GetWeaponOutput(std::string& output)
    {
        for (std::size_t i = 0; i < Weapons::Values.size(); ++i)
        {
            const WeaponInfo& info = Weapons::Values[i];
            output += "==== ";
            output += info.Name;
            output += " ====\n";
            for (int j = 0; j < 2; ++j)
            {
                const bool alt = j == 1;
                output += "\n    ";
                output += alt ? "Alt" : "Aff";
                output += " --\n";
                AppendLine(output, "Damage: ", info.Damage[j]);
                AppendLine(output, "Headshot damage: ", info.HeadshotDamage[j]);
                AppendLine(output, "Splash damage: ", info.SplashDamage[j]);
                AppendLine(output, "Splash radius: ", Fixed::ToFloat(info.SplashRadius[j]));
                AppendLine(output, "Lifespan: ", Seconds(info.Lifespan[j]));
                output.insert(output.size() - 1, " sec");
                AppendLine(output, "Speed decay: ", Fixed::ToFloat(info.SpeedDecay[j]));
                AppendLine(output, "Speed: ", Fixed::ToFloat(info.Speed[j]));
                AppendLine(output, "Final speed: ", Fixed::ToFloat(info.FinalSpeed[j]));
                AppendLine(output, "Gravity: ", Fixed::ToFloat(info.Gravity[j]));
                AppendLine(output, "Homing: ", info.Homing[j]);
                if (info.Homing[j] != 0)
                {
                    AppendLine(output, "Homing distance: ", Fixed::ToFloat(info.HomingRange[j]));
                    AppendLine(output, "Homing tolerance: ", Fixed::ToFloat(info.HomingTolerance[j]));
                    AppendLine(output, "Homing angle: ", Fixed::ToFloat(info.HomingAngle[j]));
                }
                AppendLine(output, "Ricochet: ", info.RicochetWeapon[j]);
                if (info.RicochetWeapon[j] != 0)
                {
                    AppendLine(output, "Ricochet loss H: ", Fixed::ToFloat(info.RicochetLossH[j]));
                    AppendLine(output, "Ricochet loss V: ", Fixed::ToFloat(info.RicochetLossV[j]));
                }
                AppendLine(output, "Max ricochet count: ", info.RicochetCount[j]);
                AppendLine(output, "Knockback: ", Fixed::ToFloat(info.Knockback[j]));
                AppendLine(output, "Zoom FOV: ", Fixed::ToFloat(info.ZoomFov));
                AppendLine(output, "Min charge: ", Seconds(info.MinCharge));
                output.insert(output.size() - 1, " sec");
                AppendLine(output, "Full charge: ", Seconds(info.FullCharge));
                output.insert(output.size() - 1, " sec");
                AppendLine(output, "Ammo cost: ", info.AmmoCost[j]);
                AppendLine(output, "Collision effect: ", info.CollisionEffect[j]);
                AppendLine(output, "Damage direction type: ", info.DmgDirType[j]);
                AppendLine(output, "Damage interpolation: ", info.DmgInterp[j]);
                AppendLine(output, "Projectile count: ", info.ProjectileCount[j]);
                AppendLine(output, "Smoke start: ", info.SmokeStart[j]);
                AppendLine(output, "Smoke minimum: ", info.SmokeMinimum[j]);
                AppendLine(output, "Smoke drain: ", info.SmokeDrain[j]);
            }
            output += "\n\n";
        }
        output += "==== Platform damage (affinity) ====\n";
        for (std::size_t i = 0; i < Weapons::PlatformDamages.size(); ++i)
        {
            output += ManagedValue(static_cast<WeaponType>(i));
            output += ": ";
            output += ManagedValue(Weapons::PlatformDamages[i]);
            output += '\n';
        }
        output += "==== Platform damage (non-affinity) ====\n";
        for (std::size_t i = 0; i < Weapons::PlatformDamagesNoFilter.size(); ++i)
        {
            output += ManagedValue(static_cast<WeaponType>(i));
            output += ": ";
            output += ManagedValue(Weapons::PlatformDamagesNoFilter[i]);
            output += '\n';
        }
    }

    void MechanicsDump::GetPlayerOutput(std::string& output)
    {
        output += "\n==== Jump/Fall ====\n";
#define FIXED_LINE(label, member) AppendLine(output, label, Fixed::ToFloat(Entities::PlayerValues::member))
        FIXED_LINE("Gravity: ", Gravity);
        FIXED_LINE("Alt gravity: ", AltGravity);
        FIXED_LINE("Y velocity after jump: ", JumpPadAccel);
        FIXED_LINE("Y velocity after jump prime: ", JumpPadAccelPrime);
        FIXED_LINE("Y velocity after jump no gravity: ", JumpPadAccelNoGravity);
        FIXED_LINE("Min Y velocity after jump: ", MinJumpPadAccel);
        FIXED_LINE("Min Y velocity after jump no gravity: ", MinJumpPadAccelNoGravity);
        FIXED_LINE("Max horizontal velocity after jump: ", MaxJumpPadHSpeed);
        FIXED_LINE("Max fall speed: ", TerminalVelocity);
        FIXED_LINE("Space jump height: ", SpaceJumpHeight);
        FIXED_LINE("Min space jump height: ", MinSpaceJumpHeight);
        FIXED_LINE("Space jump Y velocity: ", SpaceJumpSpeed);
        FIXED_LINE("Space jump Y velocity NOG: ", SpaceJumpSpeedNoGravity);
        AppendLine(output, "Time to regain space jump: ", Seconds(Entities::PlayerValues::SpaceJumpTime));
        output.insert(output.size() - 1, " sec");
        FIXED_LINE("Shadow height: ", ShadowHeight);

        output += "\n==== Movement ====\n";
        FIXED_LINE("Max speed: ", WalkSpeed);
        FIXED_LINE("Max strafe speed: ", StrafeSpeed);
        FIXED_LINE("Max speed while airborne (unused): ", AirSpeed);
        FIXED_LINE("Max speed alt: ", AltWalkSpeed);
        FIXED_LINE("Max backpedal speed: ", BackSpeed);
        FIXED_LINE("Friction while walking: ", WalkFriction);
        FIXED_LINE("Friction when stopping: ", StopFriction);
        FIXED_LINE("Friction when stopping fast: ", StopFrictionFast);
        FIXED_LINE("Friction while alt (unused): ", AltFriction);
        FIXED_LINE("Acceleration Y limit (unused): ", VerticalTarget);
        FIXED_LINE("Acceleration Y limit alt (unused): ", VerticalTargetAlt);
        AppendLine(output, "Time to reach max speed: ", 1.0f / 30.0f * 256.0f / Fixed::ToFloat(Entities::PlayerValues::VelocityIncrement));
        output.insert(output.size() - 1, " sec");
        AppendLine(output, "Time for full stop: ", 1.0f / 30.0f * 256.0f / Fixed::ToFloat(Entities::PlayerValues::StopIncrement));
        output.insert(output.size() - 1, " sec");
        AppendLine(output, "Time for full stop fast: ", 1.0f / 30.0f * 256.0f / Fixed::ToFloat(Entities::PlayerValues::StopIncrementFast));
        output.insert(output.size() - 1, " sec");
        output += "Alt acceleration Y: ";
        output += ManagedValue(Fixed::ToFloat(Entities::PlayerValues::AltMinVertAccel));
        output += " to ";
        output += ManagedValue(Fixed::ToFloat(Entities::PlayerValues::AltMaxVertAccel));
        output += '\n';
        FIXED_LINE("Spire min boost: ", SpireStepMin);
        FIXED_LINE("Spire max boost: ", SpireStepMax);

        output += "\n==== Biped Collision ====\n";
        FIXED_LINE("Collision radius: ", BipedColRadius);
        FIXED_LINE("Collision height: ", BipedColHeight);
        FIXED_LINE("Collision height crouched: ", BipedColHeightCrouched);
        FIXED_LINE("Step height: ", StepHeight);
        FIXED_LINE("Max distance from platform: ", StepMax);
        FIXED_LINE("Max distance from platform alt: ", StepMaxAlt);
        FIXED_LINE("Min stand angle cosine: ", StandMinCosine);
        AppendLine(output, "Min stand angle: ", DegreesFromCosine(Entities::PlayerValues::StandMinCosine));
        FIXED_LINE("Min stand angle cosine alt: ", StandMinCosineAlt);
        AppendLine(output, "Min stand angle alt: ", DegreesFromCosine(Entities::PlayerValues::StandMinCosineAlt));
        FIXED_LINE("Max stand angle cosine (unused): ", StandMaxCosine);
        AppendLine(output, "Max stand angle (unused): ", DegreesFromCosine(Entities::PlayerValues::StandMaxCosine));
        FIXED_LINE("Max stand angle cosine alt (unused): ", StandMaxCosineAlt);
        AppendLine(output, "Max stand angle alt (unused): ", DegreesFromCosine(Entities::PlayerValues::StandMaxCosineAlt));
        FIXED_LINE("Wall jump min angle cosine: ", WallJumpMinCosine);
        AppendLine(output, "Wall jump min angle: ", DegreesFromCosine(Entities::PlayerValues::WallJumpMinCosine));
        FIXED_LINE("Wall jump max angle cosine: ", WallJumpMaxCosine);
        AppendLine(output, "Wall jump max angle: ", DegreesFromCosine(Entities::PlayerValues::WallJumpMaxCosine));
        FIXED_LINE("Wall jump horizontal speed: ", WallJumpHSpeed);
        FIXED_LINE("Wall jump vertical speed: ", WallJumpVSpeed);
        AppendLine(output, "Time in wall jump after release: ", Seconds(Entities::PlayerValues::WallJumpTime));
        output.insert(output.size() - 1, " sec");
        FIXED_LINE("Crouch height: ", CrouchHeight);

        output += "\n==== Alt Form ====\n";
        FIXED_LINE("Alt col radius: ", AltColRadius);
        FIXED_LINE("Alt col y offset: ", AltColYpos);
        FIXED_LINE("Alt col scale: ", AltColScale);
        FIXED_LINE("Alt col radius 2: ", AltColRadius2);
        FIXED_LINE("Alt col y offset 2: ", AltColYpos2);
        FIXED_LINE("Alt col scale 2: ", AltColScale2);
        FIXED_LINE("Alt col radius 3: ", AltColRadius3);
        FIXED_LINE("Alt col y offset 3: ", AltColYpos3);
        FIXED_LINE("Alt col scale 3: ", AltColScale3);
        FIXED_LINE("Alt col radius 4: ", AltColRadius4);
        FIXED_LINE("Alt col y offset 4: ", AltColYpos4);
        FIXED_LINE("Alt col scale 4: ", AltColScale4);
        FIXED_LINE("Alt form radius: ", AltRadius);
        FIXED_LINE("Alt form flag radius: ", AltFlagRadius);
        FIXED_LINE("Samus morph ball hurt radius: ", BallHurtRadius);
        FIXED_LINE("Force to unmorph: ", UnmorphForce);
        FIXED_LINE("Head Y offset, standing: ", HeadPos);
        FIXED_LINE("Head Y offset, crouched: ", HeadPosCrouched);
        FIXED_LINE("Head Y offset, rolling: ", HeadPosAlt);
        FIXED_LINE("Frozen Y offset ", FrozenOffset);
        output += "========\n        ";
#undef FIXED_LINE

        for (const auto& [hunter, active] : Metadata::GetHunters())
        {
            (void)active;
            const Entities::PlayerValues& values = Metadata::GetPlayerValues(hunter);
            output += "=== ";
            output += ManagedValue(hunter);
            output += " ====\n";
#define VF(label, member) AppendLine(output, label, Fixed::ToFloat(values.member))
#define VV(label, member) AppendLine(output, label, values.member)
            VF("Camera FOV: ", Field70);
            VF("Roll min FOV: ", RollMinFov);
            VF("Roll max FOV: ", RollMaxFov);
            AppendLine(output, "Lvl 1 charge time: ", Seconds(values.Field74));
            AppendLine(output, "Lvl 2 charge time: ", Seconds(values.Field76));
            VF("Bomb force: ", BombForce);
            VF("Bomb self force: ", BombSelfForce);
            VF("Bomb radius: ", BombRadius);
            VV("Bomb damage: ", BombDamage);
            VV("Bomb self damage: ", BombSelfDamage);
            AppendLine(output, "Bomb jump control lockout: ", Seconds(values.BombSelfLock));
            AppendLine(output, "Bomb cooldown: ", Seconds(values.BombCooldown));
            VV("Alt attack damage: ", AltAttackDamage);
            VF("Alt attack knockback: ", AltAttackKnockback);
            VV("Boost damage: ", BoostDamage);
            AppendLine(output, "Boost min charge time: ", Seconds(values.BoostChargeMin));
            AppendLine(output, "Boost max charge time: ", Seconds(values.BoostChargeMax));
            VF("Field 9E: ", Field9E);
            VF("Field A0: ", FieldA0);
            VV("Field A2: ", FieldA2);
            VV("Field A4: ", FieldA4);
            VV("Field A6: ", FieldA6);
            VV("Field A8: ", FieldA8);
            VV("Field AA: ", FieldAA);
            VV("Field AC: ", FieldAC);
            VV("Field AE: ", FieldAE);
#undef VF
#undef VV
            output += '\n';
        }
    }

    void MechanicsDump::GetMiscOutput(std::string& output)
    {
        output += "\n==== Jump Pad ====\n";
#define FIXED_LINE(label, member) AppendLine(output, label, Fixed::ToFloat(Entities::PlayerValues::member))
#define VALUE_LINE(label, member) AppendLine(output, label, Entities::PlayerValues::member)
        FIXED_LINE("Accel during jump: ", JumpPadAccel);
        FIXED_LINE("Accel during jump prime: ", JumpPadAccelPrime);
        FIXED_LINE("Accel during jump no gravity: ", JumpPadAccelNoGravity);
        FIXED_LINE("Min jump accel: ", MinJumpPadAccel);
        FIXED_LINE("Min jump accel no gravity: ", MinJumpPadAccelNoGravity);
        FIXED_LINE("Max horizontal jump speed: ", MaxJumpPadHSpeed);

        output += "\n==== Damage/Health ====\n";
        FIXED_LINE("Min damage: ", MinDamage);
        FIXED_LINE("Normal damage multiplier: ", NormalDmgMult);
        FIXED_LINE("Alt damage multiplier: ", AltDmgMult);
        VALUE_LINE("Full health: ", FullHealth);
        VALUE_LINE("Low health: ", LowHealth);
        VALUE_LINE("Death health: ", DeathHealth);
        FIXED_LINE("Unused damage multiplier: ", Field6C);
        VALUE_LINE("Unused fall damage: ", Field6E);

        output += "\n==== Camera ====\n";
        FIXED_LINE("Biped camera accel X: ", BipedLookAccelX);
        FIXED_LINE("Biped camera accel Y: ", BipedLookAccelY);
        FIXED_LINE("Alt camera accel X: ", AltLookAccelX);
        FIXED_LINE("Alt camera accel Y: ", AltLookAccelY);
        FIXED_LINE("Min pitch: ", MinPitch);
        FIXED_LINE("Max pitch: ", MaxPitch);
        FIXED_LINE("Min cam angle: ", MinCamAngle);
        FIXED_LINE("Max cam angle: ", MaxCamAngle);
        FIXED_LINE("Min cam speed: ", MinCamSpeed);
        FIXED_LINE("Max cam speed: ", MaxCamSpeed);
        FIXED_LINE("Min cam accel: ", MinCamAccel);
        FIXED_LINE("Max cam accel: ", MaxCamAccel);
        FIXED_LINE("Unused cam fall rate: ", Field62);

        output += "\n==== Morph Camera ====\n";
        FIXED_LINE("Morph cam offset Y: ", MorphCamOffsetY);
        FIXED_LINE("Morph cam radius: ", MorphCamRadius);
        FIXED_LINE("Max morph cam look-up: ", MaxMorphCameraAngle);
        FIXED_LINE("Min morph cam look-up: ", MinMorphCameraAngle);

        output += "\n==== Targeting ====\n";
        FIXED_LINE("Aim distance: ", AimDistance);
        FIXED_LINE("Lock-on distance: ", LockDistance);
        FIXED_LINE("Unmorph range: ", UnmorphRange);
        FIXED_LINE("Target Y offset: ", TargetOffset);
        FIXED_LINE("Target Y interpolation: ", TargetInterpY);
        FIXED_LINE("Target XZ interpolation: ", TargetInterpXZ);
        FIXED_LINE("Aim Y offset: ", AimYOffset);
        FIXED_LINE("Aim vertical field: ", AimFieldV);
        FIXED_LINE("Aim horizontal field: ", AimFieldH);
        FIXED_LINE("Lock vertical field: ", LockFieldV);
        FIXED_LINE("Lock horizontal field: ", LockFieldH);

        output += "\n==== Motion ====\n";
        FIXED_LINE("Normal max input: ", NormalMaxInput);
        FIXED_LINE("Alt max input: ", AltMaxInput);
        FIXED_LINE("Min face angle cosine: ", MinFaceCosine);
        AppendLine(output, "Min face angle: ", DegreesFromCosine(Entities::PlayerValues::MinFaceCosine));
        FIXED_LINE("Max player sound distance: ", MaxSoundDistance);

        output += "\n==== Other ====\n";
        output += "Standing layer mask: " + Hex4(Entities::PlayerValues::StandingLayerMask) + "\n";
        output += "Alt form layer mask: " + Hex4(Entities::PlayerValues::AltFormLayerMask) + "\n";
        output += "Weapon layer mask: " + Hex4(Entities::PlayerValues::WeaponLayerMask) + "\n";
        output += "Ammo layer mask: " + Hex4(Entities::PlayerValues::AmmoLayerMask) + "\n";
        output += "Effect layer mask: " + Hex4(Entities::PlayerValues::EffectLayerMask) + "\n";
        FIXED_LINE("Coord fuzz: ", CoordFuzz);
        FIXED_LINE("Enemy health bar vertical offset: ", HealthYOffset);
        FIXED_LINE("Lock-on angle check (cosine): ", Cosine30);
        output += "========\n\n==== Collision ====\n";
#undef FIXED_LINE
#undef VALUE_LINE

        AppendLine(output, "Collision vectors: ", Formats::CollisionDetection::MaxCollisionVectors);
        AppendLine(output, "Collision entries: ", Formats::CollisionDetection::MaxCollisionEntries);
        AppendLine(output, "Max penetration: ", Fixed::ToFloat(Formats::CollisionDetection::MaxPenetration));
        AppendLine(output, "Bounce minimum: ", Fixed::ToFloat(Formats::CollisionDetection::BounceMin));
        AppendLine(output, "Bounce maximum: ", Fixed::ToFloat(Formats::CollisionDetection::BounceMax));
        AppendLine(output, "Friction: ", Fixed::ToFloat(Formats::CollisionDetection::CollisionFriction));

        output += "\n==== Multiplayer ====\n";
#define PE_VALUE(label, member) AppendLine(output, label, Entities::PlayerEntity::member)
#define PE_FIXED(label, member) AppendLine(output, label, Fixed::ToFloat(Entities::PlayerEntity::member))
#define PE_SEC(label, member) do { AppendLine(output, label, Seconds(Entities::PlayerEntity::member)); output.insert(output.size() - 1, " sec"); } while (false)
        PE_VALUE("Max players: ", MaxPlayers);
        PE_SEC("Respawn time: ", RespawnTime);
        PE_SEC("Deathalt respawn time: ", DeathaltRespawnTime);
        PE_SEC("Uplink respawn time: ", UplinkRespawnTime);
        PE_SEC("Spawn invulnerability time: ", SpawnInvulnTime);
        PE_FIXED("Base muzzle offset: ", BaseMuzzleOffset);
        PE_FIXED("Pickup height max: ", PickupHeightMax);
        PE_FIXED("Pickup XY distance max: ", PickupXYDistMax);
        PE_FIXED("Rare pickup chance: ", RarePickupChance);
        PE_FIXED("Cloak alpha: ", CloakAlpha);
        PE_SEC("Double damage timer: ", DoubleDmgTimer);
        PE_SEC("Cloak timer: ", CloakTimer);
        PE_SEC("Deathalt timer: ", DeathaltTimer);
        PE_SEC("Disrupt timer: ", DisruptTimer);
        PE_SEC("Freeze timer: ", FreezeTimer);
        PE_SEC("Burn timer: ", BurnTimer);
        PE_SEC("Spawn weapon timer: ", SpawnWeaponTimer);
        PE_SEC("Spawn weapon timer affinity: ", SpawnWeaponTimerAffinity);

        output += "\n==== Multiplayer Health ====\n";
        PE_VALUE("Health normal value: ", HealthNormalValue);
        PE_SEC("Health normal respawn time: ", HealthNormalRespawn);
        PE_VALUE("Health big value: ", HealthBigValue);
        PE_SEC("Health big respawn time: ", HealthBigRespawn);

        output += "\n==== Multiplayer Ammo ====\n";
        PE_VALUE("Ammo small value: ", AmmoSmallValue);
        PE_SEC("Ammo small respawn time: ", AmmoSmallRespawn);
        PE_VALUE("Ammo big value: ", AmmoBigValue);
        PE_SEC("Ammo big respawn time: ", AmmoBigRespawn);

        output += "\n==== Multiplayer Weapons ====\n";
        PE_SEC("Weapon pickup respawn time: ", WeaponRespawn);
        PE_SEC("Affinity pickup respawn time: ", AffinityRespawn);
        PE_SEC("Octolith pickup respawn time: ", OctolithRespawn);
#undef PE_VALUE
#undef PE_FIXED
#undef PE_SEC
        output += "========\n        ";

        output += "\n==== Multiplayer Match ====\n";
#define GS_VALUE(label, member) AppendLine(output, label, GameState::member)
#define GS_SEC(label, member) do { AppendLine(output, label, Seconds(GameState::member)); output.insert(output.size() - 1, " sec"); } while (false)
        GS_SEC("Battle time: ", BattleTimeLimit);
        GS_VALUE("Battle point goal: ", BattlePointGoal);
        GS_SEC("Survival time: ", SurvivalTimeLimit);
        GS_VALUE("Survival life goal: ", SurvivalLifeGoal);
        GS_SEC("Bounty time: ", BountyTimeLimit);
        GS_VALUE("Bounty point goal: ", BountyPointGoal);
        GS_SEC("Capture time: ", CaptureTimeLimit);
        GS_VALUE("Capture point goal: ", CapturePointGoal);
        GS_SEC("Nodes time: ", NodesTimeLimit);
        GS_VALUE("Nodes point goal: ", NodesPointGoal);
        GS_SEC("Defender time: ", DefenderTimeLimit);
        GS_VALUE("Defender point goal: ", DefenderPointGoal);
        GS_SEC("Prime Hunter time: ", PrimeTimeLimit);
        GS_VALUE("Prime Hunter point goal: ", PrimePointGoal);
#undef GS_VALUE
#undef GS_SEC
        output += "========\n        ";
    }
}
