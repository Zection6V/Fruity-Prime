#include "05_Petrasyl3.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../Formats/Types.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] Enemy05Entity& RequireEnemy(Enemy05Entity* enemy)
        {
            if (enemy == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *enemy;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            const std::shared_ptr<PlayerEntity> player = PlayerEntity::Main();
            if (!player)
            {
                throw System::NullReferenceException();
            }
            return *player;
        }

        [[nodiscard]] constexpr Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] constexpr Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] constexpr Vector3 Scale(Vector3 value, float scalar) noexcept
        {
            return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
        }

        [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float scalar) noexcept
        {
            return Vector3(value.X / scalar, value.Y / scalar, value.Z / scalar);
        }

    }
}

namespace MphRead::Entities::Enemies
{
    Enemy05Entity::Enemy05Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(2);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy05Entity::EnemyInitialize()
    {
        _health = _healthMax = 8;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 0.5F;

        if (_spawner == nullptr)
        {
            throw System::NullReferenceException();
        }
        _hurtVolumeInit = CollisionVolume(_spawner->Data.Fields.S04().Volume0);
        SetUpModel(Metadata::EnemyModelNames.at(5));
        _models[0].SetAnimation(
            9, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _models[0].SetAnimation(2, 1, SetFlags::Texcoord);

        Vector3 facing = _spawner->Data.Header.FacingVector.ToFloatVector().Normalized();
        Vector3 position = _spawner->Data.Fields.S04().Position.ToFloatVector()
            + _spawner->Data.Header.Position.ToFloatVector();
        position = AddY(position, Fixed::ToFloat(5461));
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), position);
        _initialPos = position;
        _weaveOffset = Fixed::ToFloat(_spawner->Data.Fields.S04().WeaveOffset);
        _field1B0 = Fixed::ToFloat(_spawner->Data.Fields.S04().Field88);
        _field184 = facing;
        _field190 = facing;
        _bobOffset = Fixed::ToFloat(Rng::GetRandomInt2(0x1AAB) + 1365U) / 2.0F;
        _bobSpeed = Fixed::ToFloat(Rng::GetRandomInt2(0x3000)) + 1.0F;
        _targetY = Position.Y;
        _field170 = 20 * 2;
        UpdateState();
    }

    void Enemy05Entity::UpdateState()
    {
        if (_state2 == 0)
        {
            _models[0].SetAnimation(
                6, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            _field170 = 20 * 2;
            _soundSource.PlaySfx(SfxId::MOCHTROID_TELEPORT_IN);
        }
        else if (_state2 == 1)
        {
            _models[0].SetAnimation(
                0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
            Flags &= ~EnemyFlags::NoHomingNc;
            Flags &= ~EnemyFlags::Invincible;
            const float field184X = Fixed::ToFloat(Rng::GetRandomInt2(4096) - 2048U);
            const float field184Y = Fixed::ToFloat(Rng::GetRandomInt2(4096) - 2048U);
            const float field184Z = Fixed::ToFloat(Rng::GetRandomInt2(4096) - 2048U);
            _field184 = Vector3(field184X, field184Y, field184Z);
            if (_field184.X == 0.0F && _field184.Z == 0.0F)
            {
                _field184 = FacingVector();
            }
            else
            {
                _field184 = _field184.Normalized();
            }
            _speed = Divide(MphRead::Entities::Enemies::Scale(_field184, 0.05F), 2.0F);
        }
    }

    void Enemy05Entity::UpdateMovement()
    {
        _soundSource.PlaySfx(SfxId::MOCHTROID_FLY, true);

        const Vector3 currentPosition = static_cast<Vector3>(Position);
        const Vector3 toTarget(
            currentPosition.X - _initialPos.X,
            _targetY - _initialPos.Y,
            currentPosition.Z - _initialPos.Z);

        _bobAngle += _bobSpeed / 2.0F;
        if (_bobAngle >= 360.0F)
        {
            _targetY = Position.Y;
            _bobAngle -= 360.0F;
        }
        const float ySin = std::sin(DegreesToRadians(_bobAngle));
        const float ySpeedInc = _targetY + ySin * _bobOffset - Position.Y;

        if (_field1A0 > 0)
        {
            _field1A0--;
        }
        if (_field1A0 == 0)
        {
            if (_targetY >= _initialPos.Y)
            {
                if (_targetY <= _initialPos.Y + _field1B0)
                {
                    if (toTarget.X * toTarget.X + toTarget.Z * toTarget.Z
                        <= _weaveOffset * _weaveOffset)
                    {
                        if (_scene == nullptr)
                        {
                            throw System::NullReferenceException();
                        }
                        auto enumerator = _scene->GetEnemyInstanceEntities().GetEnumerator();
                        while (enumerator.MoveNext())
                        {
                            const std::shared_ptr<EnemyInstanceEntity> enemy = enumerator.Current();
                            if (enemy.get() == this)
                            {
                                continue;
                            }
                            if (!enemy)
                            {
                                throw System::NullReferenceException();
                            }
                            if (enemy->EnemyType() != MphRead::EnemyType::Petrasyl3)
                            {
                                continue;
                            }

                            Formats::CollisionResult discard{};
                            const CollisionVolume enemyHurtVolume = enemy->HurtVolume();
                            if (Formats::CollisionDetection::CheckVolumesOverlap(
                                &_hurtVolume, &enemyHurtVolume, discard))
                            {
                                _field184 = static_cast<Vector3>(Position)
                                    - static_cast<Vector3>(enemy->Position);
                                _field1A0 = 5 * 2;
                                break;
                            }
                        }
                    }
                    else
                    {
                        _field184.X *= -1.0F;
                        _field184.Y = Fixed::ToFloat(Rng::GetRandomInt2(0x1000))
                            - 0.5F - (toTarget.Y - _field1B0 / 2.0F) / _field1B0;
                        _field184.Z *= -1.0F;
                        _field1A0 = 5 * 2;
                    }
                }
                else
                {
                    _field184.X = Fixed::ToFloat(Rng::GetRandomInt2(0x1000))
                        - 0.5F - toTarget.X / _weaveOffset;
                    _field184.Y = -Fixed::ToFloat(Rng::GetRandomInt2(0x800)) - 0.5F;
                    _field184.Z = Fixed::ToFloat(Rng::GetRandomInt2(0x1000))
                        - 0.5F - toTarget.Z / _weaveOffset;
                    _field1A0 = 5 * 2;
                }
            }
            else
            {
                _field184.X = Fixed::ToFloat(Rng::GetRandomInt2(0x1000))
                    - 0.5F - toTarget.X / _weaveOffset;
                _field184.Y = Fixed::ToFloat(Rng::GetRandomInt2(0x800)) + 0.5F;
                _field184.Z = Fixed::ToFloat(Rng::GetRandomInt2(0x1000))
                    - 0.5F - toTarget.Z / _weaveOffset;
                _field1A0 = 5 * 2;
            }
        }

        const Vector3 prevFacing = FacingVector();
        if (_field184.X == 0.0F && _field184.Y == 0.0F)
        {
            _field184 = prevFacing;
        }
        else
        {
            _field184 = _field184.Normalized();
        }

        PlayerEntity& player = MainPlayer();
        const Vector3 between = static_cast<Vector3>(player.Position)
            - static_cast<Vector3>(Position);
        if (LengthSquared(between) >= 2.0F * 2.0F)
        {
            _field190 = WithY(_field184, 0.0F).Normalized();
        }
        else
        {
            _field190 = WithY(between, 0.0F).Normalized();
        }

        Vector3 newFacing = prevFacing;
        newFacing.X += (_field190.X - prevFacing.X) / 8.0F / 2.0F;
        newFacing.Z += (_field190.Z - prevFacing.Z) / 8.0F / 2.0F;
        if (newFacing.X == 0.0F && newFacing.Z == 0.0F)
        {
            newFacing = prevFacing;
        }
        assert(!(newFacing.X == 0.0F && newFacing.Y == 0.0F && newFacing.Z == 0.0F));
        newFacing = newFacing.Normalized();
        if (std::fabs(newFacing.X - prevFacing.X) < 1.0F / 4096.0F
            && std::fabs(newFacing.Z - prevFacing.Z) < 1.0F / 4096.0F)
        {
            newFacing.X += 0.125F / 2.0F;
            newFacing.Z -= 0.125F / 2.0F;
            if (newFacing.X == 0.0F && newFacing.Z == 0.0F)
            {
                newFacing.X += 0.125F / 2.0F;
                newFacing.Z -= 0.125F / 2.0F;
            }
            newFacing = newFacing.Normalized();
        }

        const Vector3 currentUp = UpVector();
        const Vector3 transformPosition = static_cast<Vector3>(Position);
        SetTransform(newFacing, currentUp, transformPosition);
        _speed = Divide(MphRead::Entities::Enemies::Scale(_field184, 0.05F), 2.0F);
        _targetY += _speed.Y / 2.0F;
        _speed.Y += ySpeedInc / 2.0F;
    }

    void Enemy05Entity::EnemyProcess()
    {
        CallStateProcess();
    }

    void Enemy05Entity::State0()
    {
        if (CallSubroutine<Enemy05Entity>(Metadata::Enemy05Subroutines, this))
        {
            UpdateState();
        }
    }

    void Enemy05Entity::State1()
    {
        UpdateMovement();

        PlayerEntity& hitPlayer = MainPlayer();
        const std::int32_t slotIndex = hitPlayer.SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            PlayerEntity& damagePlayer = MainPlayer();
            const Vector3 damageFacing = FacingVector();
            damagePlayer.TakeDamage(12, DamageFlags::None, damageFacing, this);
        }

        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        if (animInfo == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (animInfo->Index == nullptr)
        {
            throw System::NullReferenceException();
        }
        if ((*animInfo->Index)[0] != 0)
        {
            if (animInfo->Flags == nullptr)
            {
                throw System::NullReferenceException();
            }
            if (((*animInfo->Flags)[0] & AnimFlags::Ended) != AnimFlags::None)
            {
                _models[0].SetAnimation(
                    0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
            }
        }
    }

    bool Enemy05Entity::Behavior00()
    {
        return false;
    }

    bool Enemy05Entity::Behavior01()
    {
        if (_field170 == 0)
        {
            return true;
        }
        _field170--;
        return false;
    }

    bool Enemy05Entity::Behavior00(Enemy05Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy05Entity::Behavior01(Enemy05Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }
}
