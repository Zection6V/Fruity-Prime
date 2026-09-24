#include "49_ForceFieldLock.hpp"

#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../ForceFieldEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <any>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::ScaleVector;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        [[nodiscard]] ForceFieldEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            ForceFieldEntity* typedSpawner = dynamic_cast<ForceFieldEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] float Dot(Vector3 left, Vector3 right) noexcept
        {
            return left.X * right.X + left.Y * right.Y + left.Z * right.Z;
        }

        [[nodiscard]] bool AnimationEnded(ModelInstance& model)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            return (ManagedAt(animInfo.Flags, 0) & AnimFlags::Ended) != AnimFlags::None;
        }

        [[nodiscard]] std::int32_t AnimationIndex(ModelInstance& model)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            return ManagedAt(animInfo.Index, 0);
        }

        [[nodiscard]] std::int32_t AnimationFrame(ModelInstance& model)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            return ManagedAt(animInfo.Frame, 0);
        }

        [[nodiscard]] MessageObject BoxInt32(std::int32_t value)
        {
            return std::make_shared<const std::any>(value);
        }

        [[nodiscard]] BeamProjectileEntity* CastBeam(EntityBase* source)
        {
            if (source == nullptr)
            {
                return nullptr;
            }
            BeamProjectileEntity* beam = dynamic_cast<BeamProjectileEntity*>(source);
            if (beam == nullptr)
            {
                throw SceneDetail::InvalidCastException();
            }
            return beam;
        }

        [[nodiscard]] std::int32_t UInt32ToInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }
    }

    Enemy49Entity::Enemy49Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _forceField(CastSpawner(data.Spawner))
    {
    }

    void Enemy49Entity::EnemyInitialize()
    {
        ForceFieldEntity& forceField = RequireReference(_forceField);
        const auto data = forceField.Data();

        Vector3 position = data.Header.Position.ToFloatVector();
        _fieldPosition = position;
        _vec1 = data.Header.UpVector.ToFloatVector();
        _vec2 = data.Header.FacingVector.ToFloatVector();
        position = position + ScaleVector(_vec2, Fixed::ToFloat(409));
        SetTransform(_vec2, _vec1, position);

        Flags |= EnemyFlags::NoMaxDistance;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::NoBombDamage;
        _health = _healthMax = 1;
        _boundingRadius = 0.5F;
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, _boundingRadius);

        ClearEffectiveness();
        switch (data.Type)
        {
        case 0:
            SetEffectiveness(BeamType::PowerBeam, Effectiveness::Normal);
            break;
        case 1:
            SetEffectiveness(BeamType::VoltDriver, Effectiveness::Normal);
            break;
        case 2:
            SetEffectiveness(BeamType::Missile, Effectiveness::Normal);
            break;
        case 3:
            SetEffectiveness(BeamType::Battlehammer, Effectiveness::Normal);
            break;
        case 4:
            SetEffectiveness(BeamType::Imperialist, Effectiveness::Normal);
            break;
        case 5:
            SetEffectiveness(BeamType::Judicator, Effectiveness::Normal);
            break;
        case 6:
            SetEffectiveness(BeamType::Magmaul, Effectiveness::Normal);
            break;
        case 7:
            SetEffectiveness(BeamType::ShockCoil, Effectiveness::Normal);
            break;
        case 8:
            Flags &= ~EnemyFlags::NoBombDamage;
            break;
        }

        SetUpModel("ForceFieldLock");
        SetRecolor(forceField.Recolor());

        const std::int32_t weaponIndex = UInt32ToInt32(data.Type);
        const Weapons::WeaponList& weapons1P
            = RequireReference(Weapons::Weapons1P);
        if (weaponIndex < 0
            || static_cast<std::size_t>(weaponIndex) >= weapons1P.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        _equipInfo = std::make_shared<EquipInfo>();
        _equipInfo->SetWeapon(
            weapons1P[static_cast<std::size_t>(weaponIndex)]);
        _equipInfo->SetBeams(_beams);
        _equipInfo->SetGetAmmo([this]() { return _ammo; });
        _equipInfo->SetSetAmmo([this](std::int32_t newAmmo) { _ammo = newAmmo; });
    }

    void Enemy49Entity::ClearEffectiveness()
    {
        for (std::size_t i = 0; i < BeamEffectiveness.size(); ++i)
        {
            BeamEffectiveness[i] = Effectiveness::Zero;
        }
    }

    void Enemy49Entity::SetEffectiveness(BeamType type, Effectiveness effectiveness)
    {
        const std::int32_t index = static_cast<std::int32_t>(type);
        assert(index < static_cast<std::int32_t>(BeamEffectiveness.size()));
        if (index < 0 || static_cast<std::size_t>(index) >= BeamEffectiveness.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        BeamEffectiveness[static_cast<std::size_t>(index)] = effectiveness;
    }

    void Enemy49Entity::EnemyProcess()
    {
        if (Active)
        {
            for (std::size_t i = 0; i < _models.Size(); ++i)
            {
                UpdateAnimFrames(_models[i]);
            }
        }

        const Vector3 cameraPosition = RequireReference(MainPlayer().CameraInfo()).Position;
        if (Dot(cameraPosition - _fieldPosition, _vec2) < 0.0F)
        {
            _vec2 = ScaleVector(_vec2, -1.0F);
            const Vector3 position
                = _fieldPosition + ScaleVector(_vec2, Fixed::ToFloat(409));
            SetTransform(_vec2, _vec1, position);
            _prevPos = Position;
        }

        ModelInstance& model = _models[0];
        if (AnimationEnded(model))
        {
            if (_shotFrames > 0)
            {
                --_shotFrames;
                assert(_equipInfo != nullptr);
                const Vector3 spawnDir
                    = (_targetPosition - static_cast<Vector3>(Position)).Normalized();
                const Vector3 spawnPos
                    = static_cast<Vector3>(Position) + ScaleVector(spawnDir, 0.1F);
                (void)BeamProjectileEntity::Spawn(
                    SharedFrom<EntityBase>(this),
                    _equipInfo,
                    spawnPos,
                    spawnDir,
                    BeamSpawnFlags::None,
                    NodeRef,
                    _scene);
            }
            if (_shotFrames == 0)
            {
                model.SetAnimation(0);
            }
        }

        ForceFieldEntity& forceField = RequireReference(_forceField);
        const float width = forceField.Width() - 0.3F;
        const float height = forceField.Height() - 0.3F;
        Vector3 between = static_cast<Vector3>(Position) - _fieldPosition;
        const float rightPct = Dot(between, forceField.FieldRightVector()) / width;
        const float upPct = Dot(between, forceField.FieldUpVector()) / height;
        const float pct = rightPct * rightPct + upPct * upPct;
        if (pct >= 1.0F)
        {
            const Vector3 fieldFacing = forceField.FieldFacingVector();
            const float dot1 = Dot(between, fieldFacing);
            between = (between - ScaleVector(fieldFacing, dot1)).Normalized();
            const float dot2 = Dot(_ownSpeed, between) * 2.0F;
            _ownSpeed = _ownSpeed - ScaleVector(between, dot2);
            const float inv = 1.0F / std::sqrt(pct);
            const float rf = rightPct * inv * width;
            const float uf = upPct * inv * height;
            const Vector3 fieldRight = forceField.FieldRightVector();
            const Vector3 fieldUp = forceField.FieldUpVector();
            Position = Vector3(
                _fieldPosition.X + fieldRight.X * rf + fieldUp.X * uf,
                _fieldPosition.Y + fieldRight.Y * rf + fieldUp.Y * uf,
                _fieldPosition.Z + fieldRight.Z * rf + fieldUp.Z * uf);
        }

        const float magSqr
            = _ownSpeed.X * _ownSpeed.X
            + _ownSpeed.Y * _ownSpeed.Y
            + _ownSpeed.Z * _ownSpeed.Z;
        if (magSqr <= 0.0004F)
        {
            if (_shotFrames == 0)
            {
                if (AnimationIndex(model) == 1)
                {
                    if (AnimationFrame(model) >= 10)
                    {
                        const float randRight
                            = static_cast<float>(Rng::GetRandomInt2(0x666))
                            / 4096.0F - 0.2F;
                        const float randUp
                            = static_cast<float>(Rng::GetRandomInt2(0x666))
                            / 4096.0F - 0.2F;
                        const Vector3 fieldUp = forceField.FieldUpVector();
                        const Vector3 fieldRight = forceField.FieldRightVector();
                        _ownSpeed = Vector3(
                            fieldUp.X * randUp + fieldRight.X * randRight,
                            fieldUp.Y * randUp + fieldRight.Y * randRight,
                            fieldUp.Z * randUp + fieldRight.Z * randRight);
                    }
                }
                else
                {
                    model.SetAnimation(1, AnimFlags::NoLoop);
                }
            }
        }
        else if (RequireReference(_scene).FrameCount() % 2 == 0)
        {
            _ownSpeed = ScaleVector(_ownSpeed, Fixed::ToFloat(3973));
        }

        _speed = ScaleVector(_ownSpeed, 0.5F);
    }

    bool Enemy49Entity::EnemyTakeDamage(EntityBase* source)
    {
        if (_health > 0)
        {
            if (source != nullptr && source->Type == EntityType::BeamProjectile)
            {
                LockHit(source);
            }
        }
        else
        {
            RequireReference(_scene).SendMessage(
                Message::Unlock, this, _owner, BoxInt32(0), BoxInt32(0));
        }
        return false;
    }

    void Enemy49Entity::LockHit(EntityBase* source)
    {
        BeamProjectileEntity* beam = CastBeam(source);
        if (_shotFrames == 0
            && GetEffectiveness(RequireReference(beam).Beam()) == Effectiveness::Zero)
        {
            const std::shared_ptr<EntityBase> owner = RequireReference(beam).Owner();
            if (owner.get() == PlayerEntity::Main().get())
            {
                const ForceFieldEntity& forceField = RequireReference(_forceField);
                _shotFrames = forceField.Data().Type == 7
                    ? static_cast<std::uint8_t>(30 * 2)
                    : static_cast<std::uint8_t>(1);
                RequireReference(owner).GetPosition(_targetPosition);
                _models[0].SetAnimation(2, AnimFlags::NoLoop);
            }
        }
    }
}
