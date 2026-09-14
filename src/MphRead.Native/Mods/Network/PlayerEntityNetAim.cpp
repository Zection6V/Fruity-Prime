#include "PlayerEntityNetAim.hpp"

#include "../../Entities/EntityBase.hpp"
#include "../../Entities/PlayerSpawnEntity.hpp"
#include "../../Formats/Model.hpp"
#include "../../Scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace MphRead::Entities::PlayerEntityNetAimDetail
{
    // Declaration-only seams into other PlayerEntity partial contributors and
    // later-order owners. They expose existing state/calls only and carry no
    // independent gameplay policy.

    [[nodiscard]] OpenTK::Mathematics::Vector3 Position(const PlayerEntity& player);
    void SetPosition(PlayerEntity& player, OpenTK::Mathematics::Vector3 value);
    [[nodiscard]] OpenTK::Mathematics::Vector3 PrevPosition(const PlayerEntity& player);
    void SetPrevPosition(PlayerEntity& player, OpenTK::Mathematics::Vector3 value);
    [[nodiscard]] OpenTK::Mathematics::Vector3 Speed(const PlayerEntity& player);
    void SetSpeed(PlayerEntity& player, OpenTK::Mathematics::Vector3 value);

    [[nodiscard]] std::int32_t SlotIndex(const PlayerEntity& player);
    [[nodiscard]] std::int32_t MainPlayerIndex();
    [[nodiscard]] bool IsBot(const PlayerEntity& player);
    [[nodiscard]] std::int32_t Health(const PlayerEntity& player);
    [[nodiscard]] std::uint8_t LoadFlagsBits(const PlayerEntity& player);
    [[nodiscard]] std::uint32_t Flags1Bits(const PlayerEntity& player);
    void SetFlags1Bits(PlayerEntity& player, std::uint32_t value);
    [[nodiscard]] std::uint32_t Flags2Bits(const PlayerEntity& player);
    void SetFlags2Bits(PlayerEntity& player, std::uint32_t value);

    [[nodiscard]] OpenTK::Mathematics::Vector3 GunVector(const PlayerEntity& player);
    void SetGunVector(PlayerEntity& player, OpenTK::Mathematics::Vector3 value);
    void SetAimY(PlayerEntity& player, float value);
    [[nodiscard]] OpenTK::Mathematics::Vector3 AimPosition(const PlayerEntity& player);
    void SetAimPosition(PlayerEntity& player, OpenTK::Mathematics::Vector3 value);
    [[nodiscard]] OpenTK::Mathematics::Vector3 MuzzlePosition(const PlayerEntity& player);
    [[nodiscard]] OpenTK::Mathematics::Vector3 FacingVector(const PlayerEntity& player);
    void SetFacingVector(PlayerEntity& player, OpenTK::Mathematics::Vector3 value);
    [[nodiscard]] OpenTK::Mathematics::Vector3 UpVector(const PlayerEntity& player);
    void UpdateAimFacing(PlayerEntity& player);
    void SetTransform(
        PlayerEntity& player,
        OpenTK::Mathematics::Vector3 facing,
        OpenTK::Mathematics::Vector3 up,
        OpenTK::Mathematics::Vector3 position);
    void UpdateAimX(PlayerEntity& player, float value);
    void UpdateAimY(PlayerEntity& player, float value);
    void UpdateHudShiftX(PlayerEntity& player, float value);
    void UpdateHudShiftY(PlayerEntity& player, float value);

    [[nodiscard]] OpenTK::Mathematics::Vector3 CameraPosition(const PlayerEntity& player);
    void SetCameraPosition(PlayerEntity& player, OpenTK::Mathematics::Vector3 value);
    [[nodiscard]] bool Field6D0(const PlayerEntity& player);
    [[nodiscard]] std::int32_t AimYOffset(const PlayerEntity& player);
    [[nodiscard]] std::int32_t AimDistance(const PlayerEntity& player);
    [[nodiscard]] std::uint8_t AltFormStrafe(const PlayerEntity& player);
    [[nodiscard]] float Field70(const PlayerEntity& player);
    [[nodiscard]] float Field74(const PlayerEntity& player);

    [[nodiscard]] MphRead::Scene& SceneFor(PlayerEntity& player);
    [[nodiscard]] const MphRead::Scene& SceneFor(const PlayerEntity& player);
    [[nodiscard]] Formats::Culling::NodeRef NodeRef(const PlayerEntity& player);
    void SetNodeRef(PlayerEntity& player, Formats::Culling::NodeRef value);
    [[nodiscard]] Formats::Culling::NodeRef SceneUpdateNodeRef(
        MphRead::Scene& scene,
        Formats::Culling::NodeRef current,
        OpenTK::Mathematics::Vector3 previous,
        OpenTK::Mathematics::Vector3 position);
    [[nodiscard]] bool ScenePartCouldContain(
        MphRead::Scene& scene,
        std::int32_t partIndex,
        OpenTK::Mathematics::Vector3 position);
    [[nodiscard]] Formats::Culling::NodeRef SceneGetNodeRefByPosition(
        MphRead::Scene& scene,
        OpenTK::Mathematics::Vector3 position);
    void RefreshCollisionVolume(PlayerEntity& player);
    [[nodiscard]] OpenTK::Mathematics::Vector3 CollisionSpherePosition(
        const PlayerEntity& player);

    void Spawn(
        PlayerEntity& player,
        OpenTK::Mathematics::Vector3 position,
        OpenTK::Mathematics::Vector3 facing,
        OpenTK::Mathematics::Vector3 up,
        Formats::Culling::NodeRef nodeRef,
        bool respawn);

    [[nodiscard]] bool IsAltForm(const PlayerEntity& player);
    [[nodiscard]] bool IsMorphing(const PlayerEntity& player);
    [[nodiscard]] bool IsUnmorphing(const PlayerEntity& player);
    [[nodiscard]] bool TrySwitchForms(PlayerEntity& player, bool force);
    void UpdateForm(PlayerEntity& player, bool altForm);
    void SwitchCamera(
        PlayerEntity& player,
        std::int32_t cameraType,
        OpenTK::Mathematics::Vector3 facing);

    [[nodiscard]] MphRead::Hunter Hunter(const PlayerEntity& player);
    void SetHunter(PlayerEntity& player, MphRead::Hunter hunter);
    void ClearSyluxBomb(PlayerEntity& player, std::int32_t index);
    void SetSyluxBombCount(PlayerEntity& player, std::uint8_t value);
    [[nodiscard]] OpenTK::Mathematics::Vector3 AimTargetOffset(const PlayerEntity& player);

    [[nodiscard]] std::int32_t DamageIndicatorTimerCount(const PlayerEntity& player);
    [[nodiscard]] std::uint16_t DamageIndicatorTimer(
        const PlayerEntity& player, std::int32_t index);

    [[nodiscard]] MphRead::BeamType CurrentWeapon(const PlayerEntity& player);
    void SetAvailableWeapon(PlayerEntity& player, MphRead::BeamType weapon, bool value);
    void SetAvailableCharge(PlayerEntity& player, MphRead::BeamType weapon, bool value);
    [[nodiscard]] bool AvailableCharge(const PlayerEntity& player, MphRead::BeamType weapon);
    [[nodiscard]] bool TryEquipWeapon(
        PlayerEntity& player, MphRead::BeamType weapon, bool silent);
    [[nodiscard]] std::int32_t Ammo(const PlayerEntity& player, std::int32_t type);
    void SetAmmo(PlayerEntity& player, std::int32_t type, std::int32_t value);
    [[nodiscard]] std::int32_t AmmoMax(const PlayerEntity& player, std::int32_t type);

    [[nodiscard]] bool EquipWeaponPresent(const PlayerEntity& player);
    [[nodiscard]] bool EquipZoomed(const PlayerEntity& player);
    void SetEquipZoomed(PlayerEntity& player, bool value);
    [[nodiscard]] std::uint32_t EquipWeaponFlags(const PlayerEntity& player);
    [[nodiscard]] MphRead::BeamType EquipWeaponBeam(const PlayerEntity& player);
    [[nodiscard]] std::uint16_t EquipChargeLevel(const PlayerEntity& player);
    [[nodiscard]] std::uint16_t EquipWeaponMinCharge(const PlayerEntity& player);
    [[nodiscard]] std::uint16_t EquipWeaponFullCharge(const PlayerEntity& player);
    [[nodiscard]] MphRead::Affliction EquipWeaponAffliction(
        const PlayerEntity& player, std::int32_t index);
    void UpdateZoom(PlayerEntity& player, bool value);

    [[nodiscard]] MphRead::BeamType AffinityBeam(MphRead::Hunter hunter);
    [[nodiscard]] std::uint32_t WeaponFlags(MphRead::BeamType weapon);
    [[nodiscard]] std::uint8_t WeaponAmmoType(MphRead::BeamType weapon);

    [[nodiscard]] bool IsMainPlayer(const PlayerEntity& player);
    void ResetCombatVisor(PlayerEntity& player);
    void SetDrawIceLayer(PlayerEntity& player, bool value);
    void PlayFreezeSfx(PlayerEntity& player);
    void PlayDisruptSfx(PlayerEntity& player);
    void EndAltAttack(PlayerEntity& player);
    void HudOnDisrupted(PlayerEntity& player);
    void CreateBurnEffect(PlayerEntity& player);

    [[nodiscard]] std::uint16_t FrozenTimer(const PlayerEntity& player);
    void SetFrozenTimer(PlayerEntity& player, std::uint16_t value);
    void SetFrozenGfxTimer(PlayerEntity& player, std::uint16_t value);
    [[nodiscard]] std::uint16_t TimeSinceFrozen(const PlayerEntity& player);
    [[nodiscard]] std::uint16_t DisruptedTimer(const PlayerEntity& player);
    void SetDisruptedTimer(PlayerEntity& player, std::uint16_t value);
    [[nodiscard]] std::uint16_t BurnTimer(const PlayerEntity& player);
    void SetBurnTimer(PlayerEntity& player, std::uint16_t value);

    [[nodiscard]] std::int32_t Biped2AnimValue(const PlayerEntity& player);
    [[nodiscard]] std::uint16_t Biped2FlagsBits(const PlayerEntity& player);

    void TakeDamage(
        PlayerEntity& player,
        std::int32_t damage,
        std::int32_t flags,
        EntityBase* source,
        PlayerEntity* attacker);
    [[nodiscard]] std::int32_t ActivePlayers();
    [[nodiscard]] float ScoreboardHeight(const PlayerEntity& player);
    [[nodiscard]] std::int32_t BeamEffectivenessCount(const PlayerEntity& player);
    [[nodiscard]] std::int32_t BeamEffectivenessValue(
        const PlayerEntity& player, std::int32_t index);

    [[nodiscard]] bool NetSessionActive();
    [[nodiscard]] std::int32_t NetHooksLocalSlot();
    [[nodiscard]] bool RemoteIntentValid(std::int32_t slot);
    [[nodiscard]] OpenTK::Mathematics::Vector3 RemoteIntentAim(std::int32_t slot);
    [[nodiscard]] bool NetLogEnabled();
    void NetLogCollisionRange(
        std::int32_t slot,
        const std::string& label,
        OpenTK::Mathematics::Vector3 previous,
        OpenTK::Mathematics::Vector3 position);
    void NetLogEvent(const std::string& message);
    void IncrementNodeLookupsUnresolved();

    [[nodiscard]] bool NetTestScriptEnabled();
    [[nodiscard]] float NetTestScriptAimDeltaX();
    [[nodiscard]] float NetTestScriptAimDeltaY();
    [[nodiscard]] bool SpectatorModeIsSpectating();
    [[nodiscard]] float GamepadAimDeltaX();
    [[nodiscard]] float GamepadAimDeltaY();
    void SetInputHasInput(PlayerEntity& player, bool value);

    [[nodiscard]] std::string ManagedVector3Text(OpenTK::Mathematics::Vector3 value);
}

namespace
{
    using Vector3 = OpenTK::Mathematics::Vector3;
    namespace Detail = MphRead::Entities::PlayerEntityNetAimDetail;

    constexpr std::uint8_t LoadFlagSpawned = 0x80U;
    constexpr std::uint32_t PlayerFlagMorphing = 0x800U;
    constexpr std::uint32_t PlayerFlagUnmorphing = 0x1000U;
    constexpr std::uint32_t PlayerFlagNoAimInput = 0x1000000U;
    constexpr std::uint32_t PlayerFlagShooting = 0x4U;
    constexpr std::uint32_t PlayerFlagSpectating = 0x40000U;

    constexpr std::uint32_t WeaponFlagPartialCharge = 0x100U;
    constexpr std::uint32_t WeaponFlagCanCharge = 0x200U;
    constexpr std::uint32_t WeaponFlagCanZoom = 0x800U;

    constexpr std::uint16_t AnimFlagEnded = 0x10U;

    [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
    {
        return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
    }

    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, float scale) noexcept
    {
        return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
    }

    [[nodiscard]] constexpr Vector3 AddY(Vector3 value, float y) noexcept
    {
        return Vector3(value.X, value.Y + y, value.Z);
    }

    [[nodiscard]] constexpr Vector3 NegativeUnitZ() noexcept
    {
        return Vector3(0.0F, 0.0F, -1.0F);
    }

    [[nodiscard]] constexpr Vector3 UnitY() noexcept
    {
        return Vector3(0.0F, 1.0F, 0.0F);
    }

    [[nodiscard]] constexpr bool HasFlag(std::uint32_t value, std::uint32_t flag) noexcept
    {
        return (value & flag) == flag;
    }

    [[nodiscard]] std::int32_t ManagedClamp(
        std::int32_t value, std::int32_t min, std::int32_t max)
    {
        if (min > max)
        {
            throw std::invalid_argument("min cannot be greater than max.");
        }
        if (value < min)
        {
            return min;
        }
        if (value > max)
        {
            return max;
        }
        return value;
    }

    [[nodiscard]] std::string BoolText(bool value)
    {
        return value ? "True" : "False";
    }

    [[nodiscard]] std::string BeamTypeText(MphRead::BeamType value)
    {
        switch (value)
        {
        case MphRead::BeamType::None: return "None";
        case MphRead::BeamType::PowerBeam: return "PowerBeam";
        case MphRead::BeamType::VoltDriver: return "VoltDriver";
        case MphRead::BeamType::Missile: return "Missile";
        case MphRead::BeamType::Battlehammer: return "Battlehammer";
        case MphRead::BeamType::Imperialist: return "Imperialist";
        case MphRead::BeamType::Judicator: return "Judicator";
        case MphRead::BeamType::Magmaul: return "Magmaul";
        case MphRead::BeamType::ShockCoil: return "ShockCoil";
        case MphRead::BeamType::OmegaCannon: return "OmegaCannon";
        case MphRead::BeamType::Platform: return "Platform";
        case MphRead::BeamType::Enemy: return "Enemy";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] std::string AfflictionText(MphRead::Affliction value)
    {
        const std::uint8_t bits = static_cast<std::uint8_t>(value);
        if (bits == 0U)
        {
            return "None";
        }
        if ((bits & ~0x7U) != 0U)
        {
            return std::to_string(static_cast<std::uint32_t>(bits));
        }
        std::string text;
        if ((bits & 0x1U) != 0U)
        {
            text = "Freeze";
        }
        if ((bits & 0x2U) != 0U)
        {
            if (!text.empty()) text += ", ";
            text += "Disrupt";
        }
        if ((bits & 0x4U) != 0U)
        {
            if (!text.empty()) text += ", ";
            text += "Burn";
        }
        return text;
    }

    [[nodiscard]] std::string PlayerAnimationText(std::int32_t value)
    {
        switch (value)
        {
        case -1: return "None";
        case 0: return "Morph";
        case 1: return "Flourish";
        case 2: return "WalkForward";
        case 3: return "Unmorph";
        case 4: return "DamageBack";
        case 5: return "DamageFront";
        case 6: return "DamageLeft";
        case 7: return "DamageRight";
        case 8: return "Idle";
        case 9: return "LandNeutral";
        case 10: return "LandLeft";
        case 11: return "LandRight";
        case 12: return "JumpNeutral";
        case 13: return "JumpBack";
        case 14: return "JumpForward";
        case 15: return "JumpLeft";
        case 16: return "JumpRight";
        case 17: return "Unused17";
        case 18: return "WalkBackward";
        case 19: return "Spawn";
        case 20: return "WalkLeft";
        case 21: return "WalkRight";
        case 22: return "Turn";
        case 23: return "Charge";
        case 24: return "ChargeShoot";
        case 25: return "Shoot";
        default: return std::to_string(value);
        }
    }
}

namespace MphRead::Entities
{
    void PlayerEntity::ModRecordNetworkPosition(std::uint32_t frame)
    {
        const std::int32_t count = std::min(
            _networkPositionHistoryCount, NetworkHistoryLength - 1);
        for (std::int32_t i = count; i > 0; --i)
        {
            _networkPositionHistory[static_cast<std::size_t>(i)]
                = _networkPositionHistory[static_cast<std::size_t>(i - 1)];
            _networkPositionFrames[static_cast<std::size_t>(i)]
                = _networkPositionFrames[static_cast<std::size_t>(i - 1)];
        }
        _networkPositionHistory[0] = Detail::Position(*this);
        _networkPositionFrames[0] = frame;
        _networkPositionHistoryCount = std::min(count + 1, NetworkHistoryLength);
    }

    bool PlayerEntity::ModGetNetworkPosition(
        std::uint32_t frame, OpenTK::Mathematics::Vector3& position)
    {
        for (std::int32_t i = 0; i < _networkPositionHistoryCount; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            if (_networkPositionFrames[index] <= frame)
            {
                position = _networkPositionHistory[index];
                return true;
            }
        }
        position = OpenTK::Mathematics::Vector3::Zero;
        return false;
    }

    OpenTK::Mathematics::Vector3 PlayerEntity::ModGunVector() const
    {
        return Detail::GunVector(*this);
    }

    void PlayerEntity::ModRefreshNetworkAim()
    {
        if (!Detail::NetSessionActive())
        {
            return;
        }
        const std::int32_t slotForLocal = Detail::SlotIndex(*this);
        const std::int32_t localSlot = Detail::NetHooksLocalSlot();
        if (slotForLocal == localSlot)
        {
            return;
        }
        const std::int32_t slotForValid = Detail::SlotIndex(*this);
        if (!Detail::RemoteIntentValid(slotForValid))
        {
            return;
        }
        const std::int32_t slotForAim = Detail::SlotIndex(*this);
        ModSetAim(Detail::RemoteIntentAim(slotForAim));
    }

    void PlayerEntity::ModSetAim(OpenTK::Mathematics::Vector3 aim)
    {
        if (!(LengthSquared(aim) > 0.0001F))
        {
            return;
        }

        if (Detail::NetSessionActive())
        {
            const std::int32_t slot = Detail::SlotIndex(*this);
            const std::int32_t localSlot = Detail::NetHooksLocalSlot();
            if (slot != localSlot)
            {
                Vector3 cameraPosition{};
                const bool field6D0 = Detail::Field6D0(*this);
                if (field6D0)
                {
                    cameraPosition = Detail::Position(*this);
                }
                else
                {
                    const Vector3 position = Detail::Position(*this);
                    const std::int32_t aimYOffset = Detail::AimYOffset(*this);
                    cameraPosition = AddY(
                        position, MphRead::Fixed::ToFloat(aimYOffset));
                }
                Detail::SetCameraPosition(*this, cameraPosition);
            }
        }

        const Vector3 gun = aim.Normalized();
        Detail::SetGunVector(*this, gun);
        const float flat = std::sqrt(gun.X * gun.X + gun.Z * gun.Z);
        constexpr float RadiansToDegrees = 57.2957795130823208768F;
        const float aimY = std::clamp(
            std::atan2(gun.Y, flat) * RadiansToDegrees, -85.0F, 85.0F);
        Detail::SetAimY(*this, aimY);
        const Vector3 cameraPosition = Detail::CameraPosition(*this);
        const std::int32_t aimDistance = Detail::AimDistance(*this);
        Detail::SetAimPosition(
            *this,
            cameraPosition + Multiply(gun, MphRead::Fixed::ToFloat(aimDistance)));
        Detail::UpdateAimFacing(*this);
    }

    Formats::Culling::NodeRef PlayerEntity::ModWalkNodeRef(
        MphRead::Scene& scene,
        Formats::Culling::NodeRef current,
        OpenTK::Mathematics::Vector3 previous,
        OpenTK::Mathematics::Vector3 position)
    {
        if (current == Formats::Culling::NodeRef::None
            || LengthSquared(position - previous) >= _nodeWalkStepMax * _nodeWalkStepMax)
        {
            return Formats::Culling::NodeRef::None;
        }

        Formats::Culling::NodeRef walked
            = Detail::SceneUpdateNodeRef(scene, current, previous, position);
        if (walked.PartIndex == -1
            || !Detail::ScenePartCouldContain(scene, walked.PartIndex, position))
        {
            return Formats::Culling::NodeRef::None;
        }
        return walked;
    }

    void PlayerEntity::ModPlaceAt(OpenTK::Mathematics::Vector3 position)
    {
        const Vector3 previous = Detail::Position(*this);
        Detail::SetPosition(*this, position);
        Detail::SetPrevPosition(*this, position);
        ModRefreshNodeRef(previous);
    }

    bool PlayerEntity::ModPlacementBelongsHere(OpenTK::Mathematics::Vector3 position)
    {
        constexpr float reach = 12.0F;
        bool any = false;
        auto enumerator = Detail::SceneFor(*this).GetPlayerSpawnEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            const std::shared_ptr<PlayerSpawnEntity> spawn = enumerator.Current();
            any = true;
            const Vector3 between
                = static_cast<Vector3>(spawn->Position) - position;
            if (LengthSquared(between) <= reach * reach)
            {
                return true;
            }
        }
        return !any;
    }

    void PlayerEntity::ModRefreshNodeRef(OpenTK::Mathematics::Vector3 previousPosition)
    {
        Detail::RefreshCollisionVolume(*this);
        MphRead::Scene& scene = Detail::SceneFor(*this);
        const Formats::Culling::NodeRef currentForWalk = Detail::NodeRef(*this);
        const Vector3 positionForWalk = Detail::Position(*this);
        Formats::Culling::NodeRef walked = ModWalkNodeRef(
            scene, currentForWalk, previousPosition, positionForWalk);
        if (walked != Formats::Culling::NodeRef::None)
        {
            Detail::SetNodeRef(*this, walked);
            _modNodeUnresolved = false;
            return;
        }

        const Vector3 directPosition = Detail::Position(*this);
        Formats::Culling::NodeRef found
            = Detail::SceneGetNodeRefByPosition(scene, directPosition);
        if (found.PartIndex == -1)
        {
            found = Detail::SceneGetNodeRefByPosition(
                scene, Detail::CollisionSpherePosition(*this));
        }
        if (found.PartIndex == -1)
        {
            const Vector3 upperPosition = Detail::Position(*this);
            found = Detail::SceneGetNodeRefByPosition(
                scene, upperPosition + Multiply(UnitY(), 0.5F));
        }
        if (found.PartIndex == -1)
        {
            const Vector3 lowerPosition = Detail::Position(*this);
            found = Detail::SceneGetNodeRefByPosition(
                scene, lowerPosition - Multiply(UnitY(), 0.5F));
        }
        if (found.PartIndex != -1)
        {
            Detail::SetNodeRef(*this, found);
            _modNodeUnresolved = false;
            return;
        }

        _modNodeUnresolved = true;
        Detail::IncrementNodeLookupsUnresolved();
        const Formats::Culling::NodeRef currentForCheck = Detail::NodeRef(*this);
        if (currentForCheck != Formats::Culling::NodeRef::None)
        {
            const Formats::Culling::NodeRef currentForUpdate = Detail::NodeRef(*this);
            const Vector3 updatePosition = Detail::Position(*this);
            const Formats::Culling::NodeRef updated = Detail::SceneUpdateNodeRef(
                scene, currentForUpdate, previousPosition, updatePosition);
            Detail::SetNodeRef(*this, updated);
        }
    }

    bool PlayerEntity::ModNodeUnresolved() const noexcept
    {
        return _modNodeUnresolved;
    }

    void PlayerEntity::ModLogCollisionRange()
    {
        if (!Detail::NetLogEnabled() || !Detail::NetSessionActive())
        {
            return;
        }
        const std::int32_t slot = Detail::SlotIndex(*this);
        const Vector3 previous = Detail::PrevPosition(*this);
        const Vector3 position = Detail::Position(*this);
        Detail::NetLogCollisionRange(slot, "pre-check", previous, position);
    }

    void PlayerEntity::ModSetFacing(OpenTK::Mathematics::Vector3 facing)
    {
        if (!(LengthSquared(facing) > 0.0001F))
        {
            return;
        }
        const Vector3 normalized = facing.Normalized();
        Detail::SetFacingVector(*this, normalized);
        const Vector3 up = Detail::UpVector(*this);
        const Vector3 position = Detail::Position(*this);
        Detail::SetTransform(*this, normalized, up, position);
    }

    void PlayerEntity::ModSetSpectating(bool value)
    {
        std::uint32_t flags = Detail::Flags2Bits(*this);
        if (value)
        {
            flags |= PlayerFlagSpectating;
        }
        else
        {
            flags &= ~PlayerFlagSpectating;
        }
        Detail::SetFlags2Bits(*this, flags);
    }

    bool PlayerEntity::ModInPlay() const
    {
        return Detail::Health(*this) > 0
            && !HasFlag(Detail::Flags2Bits(*this), PlayerFlagSpectating);
    }

    bool PlayerEntity::ModIsInPlay() const
    {
        return (Detail::LoadFlagsBits(*this) & LoadFlagSpawned) == LoadFlagSpawned
            && Detail::Health(*this) > 0;
    }

    void PlayerEntity::ModNetSpawn(
        OpenTK::Mathematics::Vector3 position,
        OpenTK::Mathematics::Vector3 facing)
    {
        const Vector3 forward = LengthSquared(facing) > 0.0001F
            ? facing.Normalized()
            : NegativeUnitZ();
        const Vector3 up = UnitY();
        MphRead::Scene& scene = Detail::SceneFor(*this);
        const Formats::Culling::NodeRef nodeRef = ModSpawnNodeRef(scene, position);
        Detail::Spawn(*this, position, forward, up, nodeRef, true);
    }

    Formats::Culling::NodeRef PlayerEntity::ModSpawnNodeRef(
        MphRead::Scene& scene, OpenTK::Mathematics::Vector3 position)
    {
        std::shared_ptr<EntityBase> closest{};
        float closestDist = 2.0F * 2.0F;
        auto enumerator = scene.Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            const std::shared_ptr<EntityBase> entity = enumerator.Current();
            if (entity->Type != MphRead::EntityType::PlayerSpawn
                || entity->NodeRef == Formats::Culling::NodeRef::None)
            {
                continue;
            }
            const float dist
                = LengthSquared(static_cast<Vector3>(entity->Position) - position);
            if (dist < closestDist)
            {
                closestDist = dist;
                closest = entity;
            }
        }
        if (closest != nullptr)
        {
            return closest->NodeRef;
        }
        return Detail::SceneGetNodeRefByPosition(scene, position);
    }

    std::pair<float, float> PlayerEntity::ModAimDeltaTowards(
        OpenTK::Mathematics::Vector3 target)
    {
        const Vector3 desired = ModAimVectorTowards(target);
        const Vector3 gun = Detail::GunVector(*this);
        const float flatLength
            = std::sqrt(desired.X * desired.X + desired.Z * desired.Z);
        const float aimFlat
            = std::sqrt(gun.X * gun.X + gun.Z * gun.Z);
        if (flatLength < 0.001F || aimFlat < 0.001F)
        {
            return {0.0F, 0.0F};
        }

        const float tx = desired.X / flatLength;
        const float tz = desired.Z / flatLength;
        const float ax = gun.X / aimFlat;
        const float az = gun.Z / aimFlat;
        constexpr float RadiansToDegrees = 57.2957795130823208768F;
        const float turn = -std::atan2(
            ax * tz - az * tx, ax * tx + az * tz) * RadiansToDegrees;
        const float targetPitch
            = std::atan2(desired.Y, flatLength) * RadiansToDegrees;
        const float currentPitch
            = std::atan2(gun.Y, aimFlat) * RadiansToDegrees;
        return {turn, targetPitch - currentPitch};
    }

    OpenTK::Mathematics::Vector3 PlayerEntity::ModAimVectorTowards(
        OpenTK::Mathematics::Vector3 target) const
    {
        const Vector3 eye = Detail::CameraPosition(*this);
        const float aimDistance = MphRead::Fixed::ToFloat(Detail::AimDistance(*this));
        const Vector3 muzzleForFrom = Detail::MuzzlePosition(*this);
        const Vector3 fromMuzzle = target - muzzleForFrom;
        if (LengthSquared(fromMuzzle) < 0.0001F || aimDistance <= 0.0F)
        {
            return target - eye;
        }

        const Vector3 direction = fromMuzzle.Normalized();
        const Vector3 muzzleForOffset = Detail::MuzzlePosition(*this);
        const Vector3 offset = muzzleForOffset - eye;
        const float b = Vector3::Dot(offset, direction);
        const float c = Vector3::Dot(offset, offset) - aimDistance * aimDistance;
        const float discriminant = b * b - c;
        if (discriminant < 0.0F)
        {
            return target - eye;
        }
        const float t = -b + std::sqrt(discriminant);
        const Vector3 muzzleForReturn = Detail::MuzzlePosition(*this);
        return muzzleForReturn + Multiply(direction, t) - eye;
    }

    OpenTK::Mathematics::Vector3 PlayerEntity::ModAimTarget() const
    {
        const Vector3 position = Detail::Position(*this);
        const Vector3 offset = Detail::AimTargetOffset(*this);
        return position + offset;
    }

    void PlayerEntity::ModSetHunter(MphRead::Hunter hunter)
    {
        if (hunter != Detail::Hunter(*this))
        {
            Detail::ClearSyluxBomb(*this, 0);
            Detail::ClearSyluxBomb(*this, 1);
            Detail::ClearSyluxBomb(*this, 2);
            Detail::SetSyluxBombCount(*this, 0);
        }
        Detail::SetHunter(*this, hunter);
    }

    bool PlayerEntity::ModDamageIndicatorActive() const
    {
        for (std::int32_t i = 0;
            i < Detail::DamageIndicatorTimerCount(*this); ++i)
        {
            if (Detail::DamageIndicatorTimer(*this, i) > 0)
            {
                return true;
            }
        }
        return false;
    }

    void PlayerEntity::ModStartFormSwitch()
    {
        const bool switched = Detail::TrySwitchForms(*this, true);
        const std::int32_t slot = Detail::SlotIndex(*this);
        const std::string formState = ModFormState();
        Detail::NetLogEvent(
            "slot " + std::to_string(slot)
            + " form switch requested -> " + BoolText(switched)
            + ", now " + formState);
    }

    void PlayerEntity::ModForceForm(bool altForm)
    {
        if (altForm == Detail::IsAltForm(*this))
        {
            return;
        }
        const std::int32_t slot = Detail::SlotIndex(*this);
        const std::string formState = ModFormState();
        Detail::NetLogEvent(
            "slot " + std::to_string(slot)
            + " form forced to " + (altForm ? std::string("alt") : std::string("biped"))
            + " from " + formState);

        std::uint32_t flags = Detail::Flags1Bits(*this);
        flags &= ~PlayerFlagMorphing;
        Detail::SetFlags1Bits(*this, flags);
        flags = Detail::Flags1Bits(*this);
        flags &= ~PlayerFlagUnmorphing;
        Detail::SetFlags1Bits(*this, flags);

        Detail::UpdateForm(*this, altForm);
        if (altForm)
        {
            const std::uint8_t altFormStrafe = Detail::AltFormStrafe(*this);
            const std::int32_t cameraType = altFormStrafe != 0 ? 2 : 1;
            const float field70 = Detail::Field70(*this);
            const float field74 = Detail::Field74(*this);
            const Vector3 cameraFacing(field70, 0.0F, field74);
            Detail::SwitchCamera(*this, cameraType, cameraFacing);
        }
        else
        {
            const Vector3 facing = Detail::FacingVector(*this);
            Detail::SwitchCamera(*this, 0, facing);
        }
    }

    void PlayerEntity::ModSetWeapon(MphRead::BeamType weapon)
    {
        if (weapon == Detail::CurrentWeapon(*this)
            || static_cast<std::int32_t>(weapon)
                < static_cast<std::int32_t>(MphRead::BeamType::PowerBeam)
            || static_cast<std::int32_t>(weapon)
                > static_cast<std::int32_t>(MphRead::BeamType::OmegaCannon))
        {
            return;
        }
        Detail::SetAvailableWeapon(*this, weapon, true);
        Detail::SetAvailableCharge(*this, weapon, true);
        static_cast<void>(Detail::TryEquipWeapon(*this, weapon, true));
    }

    std::pair<std::int32_t, std::int32_t> PlayerEntity::ModAmmo() const
    {
        const std::int32_t ua = Detail::Ammo(*this, 0);
        const std::int32_t missiles = Detail::Ammo(*this, 1);
        return {ua, missiles};
    }

    void PlayerEntity::ModSetAmmo(std::int32_t ua, std::int32_t missiles)
    {
        const std::int32_t uaMax = Detail::AmmoMax(*this, 0);
        Detail::SetAmmo(*this, 0, ManagedClamp(ua, 0, uaMax));
        const std::int32_t missileMax = Detail::AmmoMax(*this, 1);
        Detail::SetAmmo(*this, 1, ManagedClamp(missiles, 0, missileMax));
    }

    void PlayerEntity::ModSetZoom(bool zoomed)
    {
        const bool canZoom = Detail::EquipWeaponPresent(*this)
            && (Detail::EquipWeaponFlags(*this) & WeaponFlagCanZoom) != 0;
        const bool wanted = zoomed && canZoom;
        if (Detail::EquipZoomed(*this) != wanted)
        {
            Detail::UpdateZoom(*this, wanted);
        }
    }

    void PlayerEntity::ModArmAffinityWeapon()
    {
        const MphRead::Hunter hunter = Detail::Hunter(*this);
        const MphRead::BeamType beam = Detail::AffinityBeam(hunter);
        ModArmWeapon(beam);
    }

    void PlayerEntity::ModArmWeapon(MphRead::BeamType beam)
    {
        if (static_cast<std::int32_t>(beam)
                < static_cast<std::int32_t>(MphRead::BeamType::PowerBeam)
            || static_cast<std::int32_t>(beam)
                > static_cast<std::int32_t>(MphRead::BeamType::OmegaCannon))
        {
            return;
        }
        Detail::SetAvailableWeapon(*this, beam, true);
        Detail::SetAvailableCharge(*this, beam, true);
        const std::int32_t ammoType
            = static_cast<std::int32_t>(Detail::WeaponAmmoType(beam));
        const std::int32_t ammoMax = Detail::AmmoMax(*this, ammoType);
        Detail::SetAmmo(*this, ammoType, ammoMax);
        if (Detail::CurrentWeapon(*this) != beam)
        {
            static_cast<void>(Detail::TryEquipWeapon(*this, beam, true));
        }
    }

    void PlayerEntity::ModArmZoomWeapon()
    {
        MphRead::BeamType beam = MphRead::BeamType::Imperialist;
        if ((Detail::WeaponFlags(beam) & WeaponFlagCanZoom) == 0)
        {
            beam = MphRead::BeamType::Judicator;
        }
        ModArmWeapon(beam);
    }

    void PlayerEntity::ModApplyScriptAim(float deltaX, float deltaY)
    {
        Detail::UpdateAimY(*this, deltaY);
        Detail::UpdateAimX(*this, deltaX);
    }

    std::string PlayerEntity::ModWeaponState() const
    {
        const MphRead::BeamType currentForText = Detail::CurrentWeapon(*this);
        const MphRead::BeamType equipBeam = Detail::EquipWeaponBeam(*this);
        const std::uint16_t chargeLevel = Detail::EquipChargeLevel(*this);
        const std::uint16_t minCharge = Detail::EquipWeaponMinCharge(*this);
        const MphRead::BeamType currentForCharge = Detail::CurrentWeapon(*this);
        const bool chargeable = Detail::AvailableCharge(*this, currentForCharge);
        const MphRead::Affliction affliction = Detail::EquipWeaponAffliction(*this, 1);
        const bool shooting = HasFlag(
            Detail::Flags2Bits(*this), PlayerFlagShooting);

        return BeamTypeText(currentForText)
            + " equip=" + BeamTypeText(equipBeam)
            + " charge=" + std::to_string(chargeLevel)
            + "/" + std::to_string(static_cast<std::int32_t>(minCharge) * 2)
            + " chargeable=" + BoolText(chargeable)
            + " affliction=" + AfflictionText(affliction)
            + " shooting=" + BoolText(shooting);
    }

    std::int32_t PlayerEntity::ModChargeLevel() const
    {
        return static_cast<std::int32_t>(Detail::EquipChargeLevel(*this));
    }

    bool PlayerEntity::ModChargeReady() const
    {
        const std::uint32_t flags = Detail::EquipWeaponFlags(*this);
        if ((flags & WeaponFlagCanCharge) == 0)
        {
            return false;
        }
        const std::int32_t needed
            = (flags & WeaponFlagPartialCharge) != 0
                ? static_cast<std::int32_t>(Detail::EquipWeaponMinCharge(*this)) * 2
                : static_cast<std::int32_t>(Detail::EquipWeaponFullCharge(*this)) * 2;
        return static_cast<std::int32_t>(Detail::EquipChargeLevel(*this)) >= needed;
    }

    bool PlayerEntity::ModFrozen() const
    {
        return Detail::FrozenTimer(*this) > 0;
    }

    void PlayerEntity::ModSetFrozen(bool frozen)
    {
        if (Detail::Health(*this) <= 0)
        {
            return;
        }

        const std::uint16_t timer = Detail::FrozenTimer(*this);
        if (frozen)
        {
            if (timer == 0)
            {
                Detail::PlayFreezeSfx(*this);
                if (Detail::IsMainPlayer(*this))
                {
                    Detail::ResetCombatVisor(*this);
                    Detail::SetDrawIceLayer(*this, true);
                }
                const std::uint16_t next
                    = Detail::TimeSinceFrozen(*this) > 60 * 2
                        ? static_cast<std::uint16_t>(75 * 2)
                        : static_cast<std::uint16_t>(15 * 2);
                Detail::SetFrozenTimer(*this, next);
                Detail::SetFrozenGfxTimer(
                    *this, static_cast<std::uint16_t>(next + 5 * 2));
                Detail::EndAltAttack(*this);
            }
            else if (timer < 2)
            {
                Detail::SetFrozenTimer(*this, 2);
            }
        }
        else if (timer > 1)
        {
            Detail::SetFrozenTimer(*this, 1);
        }
    }

    void PlayerEntity::ModRefreshVolume()
    {
        Detail::RefreshCollisionVolume(*this);
    }

    bool PlayerEntity::ModBurning() const
    {
        return Detail::BurnTimer(*this) > 0;
    }

    bool PlayerEntity::ModDisrupted() const
    {
        return Detail::DisruptedTimer(*this) > 0;
    }

    void PlayerEntity::ModSetDisrupted(bool disrupted)
    {
        if (Detail::Health(*this) <= 0)
        {
            return;
        }

        const std::uint16_t timer = Detail::DisruptedTimer(*this);
        if (disrupted)
        {
            if (timer == 0)
            {
                Detail::SetDisruptedTimer(*this, 60 * 2);
                if (Detail::IsMainPlayer(*this))
                {
                    Detail::HudOnDisrupted(*this);
                    Detail::PlayDisruptSfx(*this);
                }
            }
            else if (timer < 2)
            {
                Detail::SetDisruptedTimer(*this, 2);
            }
        }
        else if (timer > 1)
        {
            Detail::SetDisruptedTimer(*this, 1);
        }
    }

    void PlayerEntity::ModSetBurning(bool burning)
    {
        if (Detail::Health(*this) <= 0)
        {
            return;
        }

        const std::uint16_t timer = Detail::BurnTimer(*this);
        if (burning)
        {
            if (timer == 0)
            {
                Detail::SetBurnTimer(*this, 150 * 2);
                Detail::CreateBurnEffect(*this);
            }
            else if (timer < 2)
            {
                Detail::SetBurnTimer(*this, 2);
            }
        }
        else if (timer > 1)
        {
            Detail::SetBurnTimer(*this, 1);
        }
    }

    bool PlayerEntity::ModCanZoom() const
    {
        return Detail::EquipWeaponPresent(*this)
            && (Detail::EquipWeaponFlags(*this) & WeaponFlagCanZoom) != 0;
    }

    std::string PlayerEntity::ModFormState() const
    {
        std::string form = Detail::IsAltForm(*this) ? "alt" : "biped";
        if (Detail::IsMorphing(*this))
        {
            form += "+morphing";
        }
        if (Detail::IsUnmorphing(*this))
        {
            form += "+unmorphing";
        }

        std::string result
            = form + "/" + PlayerAnimationText(Detail::Biped2AnimValue(*this));
        if ((Detail::Biped2FlagsBits(*this) & AnimFlagEnded) != 0)
        {
            result += "/ended";
        }
        const std::uint16_t frozenTimer = Detail::FrozenTimer(*this);
        if (frozenTimer > 0)
        {
            result += "/frozen:" + std::to_string(frozenTimer);
        }
        if (Detail::Health(*this) == 0)
        {
            result += "/dead";
        }
        return result;
    }

    void PlayerEntity::ModRepairVectors()
    {
        const Vector3 facing = Detail::FacingVector(*this);
        if (Finite(facing))
        {
            _modLastGoodFacing = facing;
        }
        else
        {
            Detail::SetFacingVector(*this, _modLastGoodFacing);
            const std::int32_t slot = Detail::SlotIndex(*this);
            const std::string formState = ModFormState();
            const Vector3 gunForText = Detail::GunVector(*this);
            const std::string gunText = Detail::ManagedVector3Text(gunForText);
            Detail::NetLogEvent(
                "slot " + std::to_string(slot)
                + " facing repaired, " + formState + ", gun=" + gunText);
        }

        const Vector3 gun = Detail::GunVector(*this);
        if (Finite(gun))
        {
            _modLastGoodGunVec = gun;
        }
        else
        {
            Detail::SetGunVector(*this, _modLastGoodGunVec);
            const std::int32_t slot = Detail::SlotIndex(*this);
            Detail::NetLogEvent(
                "slot " + std::to_string(slot) + " aim repaired");
        }

        const Vector3 positionForFinite = Detail::Position(*this);
        if (Finite(positionForFinite))
        {
            _modLastGoodPosition = Detail::Position(*this);
        }
        else
        {
            Detail::SetPosition(*this, _modLastGoodPosition);
            const std::int32_t slot = Detail::SlotIndex(*this);
            Detail::NetLogEvent(
                "slot " + std::to_string(slot) + " position repaired");
        }

        if (!Finite(Detail::Speed(*this)))
        {
            Detail::SetSpeed(*this, OpenTK::Mathematics::Vector3::Zero);
        }
        if (!Finite(Detail::AimPosition(*this)))
        {
            const Vector3 positionForAim = Detail::Position(*this);
            const Vector3 gunForAim = Detail::GunVector(*this);
            Detail::SetAimPosition(*this, positionForAim + gunForAim);
        }
    }

    bool PlayerEntity::Finite(OpenTK::Mathematics::Vector3 value) noexcept
    {
        return std::isfinite(value.X)
            && std::isfinite(value.Y)
            && std::isfinite(value.Z);
    }

    void PlayerEntity::ModNetDie()
    {
        constexpr std::int32_t NoDmgInvuln = 1;
        constexpr std::int32_t Death = 4;
        Detail::TakeDamage(*this, 1, Death | NoDmgInvuln, nullptr, nullptr);
    }

    std::pair<std::int32_t, float> PlayerEntity::ModScoreboardSize() const
    {
        const std::int32_t rows = Detail::ActivePlayers();
        const float height = Detail::ScoreboardHeight(*this);
        return {rows, height};
    }

    bool PlayerEntity::ModCanBeHurt() const
    {
        for (std::int32_t i = 0;
            i < Detail::BeamEffectivenessCount(*this); ++i)
        {
            if (Detail::BeamEffectivenessValue(*this, i) != 0)
            {
                return true;
            }
        }
        return false;
    }

    void PlayerEntity::ApplyModAim()
    {
        ApplyGamepadAim();
        if (!Detail::NetSessionActive())
        {
            return;
        }

        const std::int32_t slotForLocal = Detail::SlotIndex(*this);
        const std::int32_t localSlot = Detail::NetHooksLocalSlot();
        if (slotForLocal == localSlot)
        {
            if (Detail::NetTestScriptEnabled())
            {
                const float deltaY = Detail::NetTestScriptAimDeltaY();
                Detail::UpdateAimY(*this, deltaY);
                const float deltaX = Detail::NetTestScriptAimDeltaX();
                Detail::UpdateAimX(*this, deltaX);
            }
            return;
        }
        const std::int32_t slotForValid = Detail::SlotIndex(*this);
        if (!Detail::RemoteIntentValid(slotForValid))
        {
            return;
        }
        const std::int32_t slotForAim = Detail::SlotIndex(*this);
        ModSetAim(Detail::RemoteIntentAim(slotForAim));
    }

    void PlayerEntity::ApplyGamepadAim()
    {
        if (Detail::IsBot(*this))
        {
            return;
        }
        const std::int32_t slot = Detail::SlotIndex(*this);
        const std::int32_t mainPlayerIndex = Detail::MainPlayerIndex();
        if (slot != mainPlayerIndex)
        {
            return;
        }
        if (Detail::SpectatorModeIsSpectating())
        {
            return;
        }
        const std::uint32_t flags1 = Detail::Flags1Bits(*this);
        if (HasFlag(flags1, PlayerFlagNoAimInput))
        {
            return;
        }

        const float x = Detail::GamepadAimDeltaX();
        const float y = Detail::GamepadAimDeltaY();
        if (x == 0.0F && y == 0.0F)
        {
            return;
        }

        ModNoteInput();
        Detail::UpdateHudShiftY(*this, y);
        Detail::UpdateHudShiftX(*this, x);
        Detail::UpdateAimY(*this, y);
        Detail::UpdateAimX(*this, x);
    }

    void PlayerEntity::ModNoteInput()
    {
        Detail::SetInputHasInput(*this, true);
    }
}

namespace MphRead::Mods::Network::Detail
{
    [[nodiscard]] bool NetPlayerBridgeZoomed(Entities::PlayerEntity& player)
    {
        return Entities::PlayerEntityNetAimDetail::EquipZoomed(player);
    }

    [[nodiscard]] OpenTK::Mathematics::Vector3 NetPlayerBridgeGunVector(
        Entities::PlayerEntity& player)
    {
        return player.ModGunVector();
    }

    [[nodiscard]] std::int32_t NetPlayerBridgeAmmoUa(Entities::PlayerEntity& player)
    {
        return player.ModAmmo().first;
    }

    [[nodiscard]] std::int32_t NetPlayerBridgeAmmoMissiles(Entities::PlayerEntity& player)
    {
        return player.ModAmmo().second;
    }

    void NetPlayerBridgeSetWeapon(
        Entities::PlayerEntity& player, MphRead::BeamType weapon)
    {
        player.ModSetWeapon(weapon);
    }

    void NetPlayerBridgeSetAmmo(
        Entities::PlayerEntity& player, std::int32_t ua, std::int32_t missiles)
    {
        player.ModSetAmmo(ua, missiles);
    }

    void NetPlayerBridgeNoteInput(Entities::PlayerEntity& player)
    {
        player.ModNoteInput();
    }

    void NetPlayerBridgeSetZoom(Entities::PlayerEntity& player, bool zoomed)
    {
        player.ModSetZoom(zoomed);
    }

    void NetPlayerBridgeSetSpectating(Entities::PlayerEntity& player, bool spectating)
    {
        player.ModSetSpectating(spectating);
    }

    [[nodiscard]] std::string NetPlayerBridgeFormState(Entities::PlayerEntity& player)
    {
        return player.ModFormState();
    }

    void NetPlayerBridgeNetDie(Entities::PlayerEntity& player)
    {
        player.ModNetDie();
    }

    void NetPlayerBridgeNetSpawn(
        Entities::PlayerEntity& player,
        OpenTK::Mathematics::Vector3 position,
        OpenTK::Mathematics::Vector3 facing)
    {
        player.ModNetSpawn(position, facing);
    }

    [[nodiscard]] bool NetPlayerBridgePlacementBelongsHere(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position)
    {
        return player.ModPlacementBelongsHere(position);
    }

    void NetPlayerBridgeSetFrozen(Entities::PlayerEntity& player, bool frozen)
    {
        player.ModSetFrozen(frozen);
    }

    void NetPlayerBridgeSetFacing(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 facing)
    {
        player.ModSetFacing(facing);
    }

    void NetPlayerBridgeSetEquipZoomed(Entities::PlayerEntity& player, bool zoomed)
    {
        Entities::PlayerEntityNetAimDetail::SetEquipZoomed(player, zoomed);
    }

    void NetPlayerBridgeSetDisrupted(Entities::PlayerEntity& player, bool disrupted)
    {
        player.ModSetDisrupted(disrupted);
    }

    void NetPlayerBridgeSetBurning(Entities::PlayerEntity& player, bool burning)
    {
        player.ModSetBurning(burning);
    }

    void NetPlayerBridgeStartFormSwitch(Entities::PlayerEntity& player)
    {
        player.ModStartFormSwitch();
    }

    void NetPlayerBridgeForceForm(Entities::PlayerEntity& player, bool altForm)
    {
        player.ModForceForm(altForm);
    }

    [[nodiscard]] bool NetPlayerBridgeGetNetworkPosition(
        Entities::PlayerEntity& player,
        std::uint32_t frame,
        OpenTK::Mathematics::Vector3& position)
    {
        return player.ModGetNetworkPosition(frame, position);
    }

    [[nodiscard]] bool NetPlayerBridgeFrozen(Entities::PlayerEntity& player)
    {
        return player.ModFrozen();
    }

    void NetPlayerBridgeRefreshNodeRef(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 previous)
    {
        player.ModRefreshNodeRef(previous);
    }

    void NetPlayerBridgeRefreshVolume(Entities::PlayerEntity& player)
    {
        player.ModRefreshVolume();
    }
}
