#include "26_GoreaArm.hpp"

#include "../../Formats/Effects.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::ManagedListAt;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::ScaleVector;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        [[nodiscard]] std::int32_t ManagedMultiplyByTwo(
            std::int32_t value) noexcept
        {
            const std::uint32_t result
                = std::bit_cast<std::uint32_t>(value) * 2U;
            return std::bit_cast<std::int32_t>(result);
        }
    }

    Enemy26Entity::Enemy26Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene)
    {
    }

    std::int32_t Enemy26Entity::ScanId() const noexcept
    {
        return _scanId;
    }

    std::shared_ptr<MphRead::EquipInfo> Enemy26Entity::EquipInfo() const noexcept
    {
        return _equipInfo;
    }

    std::int32_t Enemy26Entity::ColorTimer() const noexcept
    {
        return _colorTimer;
    }

    void Enemy26Entity::EnemyInitialize()
    {
        if (Enemy24Entity* owner = dynamic_cast<Enemy24Entity*>(_owner))
        {
            _gorea1A = owner;
            InitializeCommon(owner->Spawner());
            Flags &= ~EnemyFlags::NoHomingCo;
            Flags &= ~EnemyFlags::Visible;
            _state1 = _state2 = 255;

            ModelInstance& ownerModel
                = RequireReference(ManagedListAt(_owner->GetModels(), 0));
            Model& model = RequireReference(ownerModel.Model());
            _shoulderNode = model.GetNodeByName(
                Index == 0 ? "L_Shoulder" : "R_Shoulder");
            _elbowNode = model.GetNodeByName(
                Index == 0 ? "L_Elbow" : "R_Elbow");
            _upperArmNode = model.GetNodeByName(
                Index == 0 ? "L_UpperArm" : "R_UpperArm");

            Position = static_cast<Vector3>(Position)
                + RequireReference(_shoulderNode).Position;
            _prevPos = Position;
            SetTransform(owner->FacingVector(), owner->UpVector(), Position);
            _hurtVolumeInit = CollisionVolume(
                Vector3::Zero, Fixed::ToFloat(1732));
            _health = 65535;
            _healthMax = 120;

            const std::shared_ptr<WeaponInfo> weapon
                = ManagedListAt(RequireReference(Weapons::GoreaWeapons), 0);
            auto equipInfo = std::make_shared<MphRead::EquipInfo>(weapon, _beams);
            _equipInfo = std::move(equipInfo);
            RequireReference(_equipInfo).SetGetAmmo(
                [this]() { return Ammo; });
            RequireReference(_equipInfo).SetSetAmmo(
                [this](std::int32_t newAmmo) { Ammo = newAmmo; });
            ArmFlags |= GoreaArmFlags::Bit1;
        }
    }

    void Enemy26Entity::Activate()
    {
        const std::int32_t index = static_cast<std::int32_t>(EnemyType::GoreaArm);
        _scanId = ManagedAt(Metadata::EnemyScanIds, index);
        _health = 65535;
        _healthMax = 120;
        Flags |= EnemyFlags::CollidePlayer;
        Flags |= EnemyFlags::CollideBeam;
        Flags &= ~EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags &= ~EnemyFlags::NoHomingCo;
        Damage = 0;
        ArmFlags &= ~GoreaArmFlags::Bit0;
    }

    void Enemy26Entity::UpdateWeapon(std::shared_ptr<MphRead::WeaponInfo> weapon)
    {
        RequireReference(_equipInfo).SetWeapon(weapon);
        WeaponInfo& weaponRef = RequireReference(weapon);
        const std::int32_t cooldown = Index == 0
            ? static_cast<std::int32_t>(weaponRef.ShotCooldown)
            : static_cast<std::int32_t>(weaponRef.AutofireCooldown);
        Cooldown = ManagedMultiplyByTwo(cooldown);
    }

    void Enemy26Entity::GetElbowNodeVectors(
        Vector3& position, Vector3& up, Vector3& facing)
    {
        GetNodeVectors(_elbowNode.get(), position, up, facing);
    }

    void Enemy26Entity::GetNodeVectors(
        Node* node,
        Vector3& position, Vector3& up, Vector3& facing)
    {
        Matrix4 transform = GetNodeTransform(_gorea1A, node);
        position = transform.Row3().Xyz();
        up = Index == 0 ? transform.Row0().Xyz() : transform.Row2().Xyz();
        facing = transform.Row1().Xyz();
    }

    void Enemy26Entity::EnemyProcess()
    {
        if ((ArmFlags & GoreaArmFlags::Bit0) != GoreaArmFlags::None)
        {
            return;
        }

        Matrix4 transform = GetNodeTransform(_gorea1A, _shoulderNode.get());
        Position = transform.Row3().Xyz();

        if (_damageEffect)
        {
            _damageEffect->Transform(
                FacingVector(), UpVector(), Position);
        }

        if (_shotEffect)
        {
            Vector3 position{};
            Vector3 up{};
            Vector3 facing{};
            GetElbowNodeVectors(position, up, facing);
            up = up.Normalized();
            facing = facing.Normalized();
            position = position + ScaleVector(up, Fixed::ToFloat(8343));
            _shotEffect->Transform(facing, up, position);
        }

        if (Damage <= 60)
        {
            if (_damageEffect)
            {
                RequireReference(_scene).UnlinkEffectEntry(_damageEffect);
                _damageEffect.reset();
            }
        }
        else if (!_damageEffect)
        {
            _damageEffect = SpawnEffectGetEntry(
                43, Position, true);
        }

        if (RegenTimer > 0)
        {
            --RegenTimer;
        }
        if (_colorTimer > 0)
        {
            --_colorTimer;
        }
    }

    bool Enemy26Entity::EnemyTakeDamage(EntityBase* source)
    {
        const std::int32_t prevDamage = Damage;
        Damage = UncheckedAdd(
            Damage, 65535 - static_cast<std::int32_t>(_health));

        bool forceBreak = RegenTimer > 0;
        if (!forceBreak)
        {
            Enemy24Entity& gorea = RequireReference(_gorea1A);
            if (gorea.WeaponIndex() == 5)
            {
                BeamProjectileEntity* beam
                    = dynamic_cast<BeamProjectileEntity*>(source);
                if (beam != nullptr
                    && beam->BeamKind() == BeamType::Imperialist)
                {
                    forceBreak = true;
                }
            }
        }
        if (forceBreak)
        {
            Damage = 121;
        }

        if (Damage <= 120)
        {
            std::int32_t damage = Damage;
            if (damage == 120)
            {
                --damage;
            }
            if (damage / 10 > prevDamage / 10)
            {
                _soundSource.PlaySfx(
                    SfxId::GOREA_SHOULDER_DAMAGE2);
                SpawnEffect(44, Position);
            }
        }
        else
        {
            RegenTimer = 0;
            Damage = 120;
            _scanId = 0;
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            Flags |= EnemyFlags::Invincible;
            Flags |= EnemyFlags::NoHomingNc;
            Flags |= EnemyFlags::NoHomingCo;
            ArmFlags |= GoreaArmFlags::Bit0;
            _soundSource.PlaySfx(SfxId::GOREA_SHOULDER_DIE);
            if (_damageEffect)
            {
                RequireReference(_scene).UnlinkEffectEntry(_damageEffect);
                _damageEffect.reset();
            }
        }

        _health = 65535;
        if ((static_cast<EnemyFlags>(Flags) & EnemyFlags::Invincible)
            == static_cast<EnemyFlags>(0))
        {
            _colorTimer = 10 * 2;
            const std::string matName
                = Index == 0 ? "L_ShoulderTarget" : "R_ShoulderTarget";
            Enemy24Entity& gorea = RequireReference(_gorea1A);
            ModelInstance& instance
                = RequireReference(ManagedListAt(gorea.GetModels(), 0));
            Model& model = RequireReference(instance.Model());
            Material& material
                = RequireReference(model.GetMaterialByName(matName));
            material.Diffuse = ColorRgb(29, 9, 0);
        }
        return true;
    }

    void Enemy26Entity::SpawnShotEffect(std::int32_t effectId)
    {
        _shotEffect = SpawnEffectGetEntry(
            effectId, Position, false);
    }

    void Enemy26Entity::StopShotEffect(bool deatch)
    {
        if (_shotEffect)
        {
            if (deatch)
            {
                RequireReference(_scene).DetachEffectEntry(
                    _shotEffect, false);
            }
            else
            {
                RequireReference(_scene).UnlinkEffectEntry(_shotEffect);
            }
            _shotEffect.reset();
        }
    }

    void Enemy26Entity::DrawRegen(ModelInstance& regenModel)
    {
        Vector3 position{};
        Vector3 up{};
        Vector3 facing{};
        GetElbowNodeVectors(position, up, facing);
        DrawRegen(
            regenModel, position, up, facing, Fixed::ToFloat(8343));

        if (Index == 0)
        {
            GetNodeVectors(_upperArmNode.get(), position, up, facing);
        }
        else
        {
            Vector3 elbowPos{};
            Vector3 ignoredUp{};
            Vector3 ignoredFacing{};
            GetElbowNodeVectors(elbowPos, ignoredUp, ignoredFacing);

            Vector3 upperArmPos{};
            Vector3 ignoredUpperArmUp{};
            Vector3 upperArmFacing{};
            GetNodeVectors(
                _upperArmNode.get(),
                upperArmPos, ignoredUpperArmUp, upperArmFacing);

            position = upperArmPos;
            Vector3 between = elbowPos - upperArmPos;
            up = between;
            facing = Vector3(0.0F, 0.0F, 1.0F);
            if (LengthSquared(between) > 1.0F / 128.0F
                && LengthSquared(upperArmFacing) > 1.0F / 128.0F)
            {
                between = between.Normalized();
                upperArmFacing = upperArmFacing.Normalized();
                const Vector3 cross = Vector3::Cross(between, upperArmFacing);
                facing = Vector3::Cross(cross, between);
            }
        }

        DrawRegen(
            regenModel, position, up, facing, Fixed::ToFloat(6702));
    }

    void Enemy26Entity::DrawRegen(
        ModelInstance& regenModel,
        Vector3 position, Vector3 up, Vector3 facing, float factor)
    {
        up = up.Normalized();
        facing = facing.Normalized();
        Matrix4 transform = GetTransformMatrix(facing, up, position);
        transform.M31 *= factor;
        transform.M32 *= factor;
        transform.M33 *= factor;
        UpdateTransforms(regenModel, transform, 0);
        GetDrawItems(regenModel, 0);
    }
}
