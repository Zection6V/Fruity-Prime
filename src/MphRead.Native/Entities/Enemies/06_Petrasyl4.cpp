#include "06_Petrasyl4.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::Divide;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::Scale;
using ::OpenTK::Mathematics::WithY;

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

        [[nodiscard]] Enemy06Entity& RequireEnemy(Enemy06Entity* enemy)
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
            if (player == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *player;
        }

        [[nodiscard]] AnimationInfo& RequireAnimInfo(ModelInstance& model)
        {
            return RequireReference(model.AnimInfo);
        }
    }
}

namespace MphRead::Entities::Enemies
{
    Enemy06Entity::Enemy06Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(5);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        (*processes)[2] = [this]() { State2(); };
        (*processes)[3] = [this]() { State3(); };
        (*processes)[4] = [this]() { State4(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy06Entity::EnemyInitialize()
    {
        _health = _healthMax = 8;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 0.5F;

        EnemySpawnEntity& spawner = RequireReference(_spawner);
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S04().Volume0);
        SetUpModel(Metadata::EnemyModelNames.at(6));
        _models[0].SetAnimation(
            9, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _models[0].SetAnimation(2, 1, SetFlags::Texcoord);

        Vector3 facing = spawner.Data.Header.FacingVector.ToFloatVector().Normalized();
        Vector3 position = spawner.Data.Fields.S04().Position.ToFloatVector()
            + spawner.Data.Header.Position.ToFloatVector();
        position = AddY(position, Fixed::ToFloat(5461));
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), position);
        _initialPos = position;
        _weaveOffset = Fixed::ToFloat(spawner.Data.Fields.S04().WeaveOffset);
        _field1B0 = Fixed::ToFloat(spawner.Data.Fields.S04().Field88);
        _field184 = facing;
        _field190 = facing;
        _bobOffset = Fixed::ToFloat(Rng::GetRandomInt2(0x1AAB) + 1365U) / 2.0F;
        _bobSpeed = Fixed::ToFloat(Rng::GetRandomInt2(0x3000)) + 1.0F;
        _targetY = static_cast<Vector3>(Position).Y;
        _field170 = 10 * 2;
        _field172 = 10 * 2;
        UpdateState();
    }

    void Enemy06Entity::UpdateState()
    {
        bool updateSpeed = false;
        if (_state2 == 0)
        {
            Flags |= EnemyFlags::Visible;
            _models[0].SetAnimation(
                7, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            _field170 = 10 * 2;
            _soundSource.PlaySfx(SfxId::MOCHTROID_TELEPORT_IN);
        }
        else if (_state2 == 1)
        {
            Flags &= ~EnemyFlags::Invincible;
            Flags &= ~EnemyFlags::NoHomingNc;
            _models[0].SetAnimation(
                0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
            if (_state1 == 0)
            {
                updateSpeed = true;
            }
        }
        else if (_state2 == 2)
        {
            Flags |= EnemyFlags::Invincible;
            Flags |= EnemyFlags::NoHomingNc;
            _models[0].SetAnimation(
                8, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            if (_state1 == 1)
            {
                _field172 = 10 * 2;
            }
            else
            {
                _field172 = static_cast<std::uint16_t>(10 * 2 - _field170);
                AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
                RequireReference(animInfo.Frame)[0]
                    = (10 * (10 - _field172 / 2) - 1) / 10;
                if (_state1 == 0)
                {
                    updateSpeed = true;
                }
            }
            _soundSource.PlaySfx(SfxId::MOCHTROID_TELEPORT_OUT);
        }
        else if (_state2 == 3)
        {
            Flags &= ~EnemyFlags::Visible;
            _models[0].SetAnimation(
                0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        }
        else if (_state2 == 4)
        {
            Flags |= EnemyFlags::Visible;
            _models[0].SetAnimation(
                7, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            if (_state1 == 3)
            {
                _field170 = 10 * 2;
            }
            else
            {
                _field170 = static_cast<std::uint16_t>(10 * 2 - _field172);
                AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
                RequireReference(animInfo.Frame)[0]
                    = (10 * (10 - _field170 / 2) - 1) / 10;
            }
            _soundSource.PlaySfx(SfxId::MOCHTROID_TELEPORT_IN);
        }

        if (updateSpeed)
        {
            static_cast<void>(Rng::GetRandomInt2(0x1000));
            static_cast<void>(Rng::GetRandomInt2(0x1000));
            static_cast<void>(Rng::GetRandomInt2(0x1000));

            Vector3 facing = FacingVector();
            _field184 = ::OpenTK::Mathematics::Scale(facing, 0.05F);
            if (_field184.X == 0.0F && _field184.Y == 0.0F)
            {
                _field184 = facing;
            }
            else
            {
                _field184 = _field184.Normalized();
            }
            _speed = Divide(::OpenTK::Mathematics::Scale(_field184, 0.05F), 2.0F);
        }
    }

    void Enemy06Entity::UpdateMovement()
    {
        const Vector3 currentPosition = static_cast<Vector3>(Position);
        const Vector3 toTarget(
            currentPosition.X - _initialPos.X,
            _targetY - _initialPos.Y,
            currentPosition.Z - _initialPos.Z);

        _bobAngle += _bobSpeed / 2.0F;
        if (_bobAngle >= 360.0F)
        {
            _targetY = static_cast<Vector3>(Position).Y;
            _bobAngle -= 360.0F;
        }
        const float ySin = std::sin(DegreesToRadians(_bobAngle));
        const float ySpeedInc = _targetY + ySin * _bobOffset
            - static_cast<Vector3>(Position).Y;

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
                        if (TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Visible))
                        {
                            auto enumerator = RequireReference(_scene)
                                .GetEnemyInstanceEntities().GetEnumerator();
                            while (enumerator.MoveNext())
                            {
                                const std::shared_ptr<EnemyInstanceEntity> enemyRef
                                    = enumerator.Current();
                                EnemyInstanceEntity& enemy = RequireReference(enemyRef);
                                if (&enemy == this || enemy.EnemyType() != EnemyType::Petrasyl3)
                                {
                                    continue;
                                }

                                Formats::CollisionResult discard{};
                                const CollisionVolume enemyHurtVolume = enemy.HurtVolume();
                                if (Formats::CollisionDetection::CheckVolumesOverlap(
                                    &_hurtVolume, &enemyHurtVolume, discard))
                                {
                                    _field184 = static_cast<Vector3>(Position)
                                        - static_cast<Vector3>(enemy.Position);
                                    _field1A0 = 5 * 2;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        _field184.X *= -1.0F;
                        _field184.Y = Fixed::ToFloat(Rng::GetRandomInt2(0x1000)) - 0.5F
                            - (toTarget.Y - _field1B0 / 2.0F) / _field1B0;
                        _field184.Z *= -1.0F;
                        _field1A0 = 5 * 2;
                    }
                }
                else
                {
                    _field184.X = Fixed::ToFloat(Rng::GetRandomInt2(0x1000)) - 0.5F
                        - toTarget.X / _weaveOffset;
                    _field184.Y = -Fixed::ToFloat(Rng::GetRandomInt2(0x800)) - 0.5F;
                    _field184.Z = Fixed::ToFloat(Rng::GetRandomInt2(0x1000)) - 0.5F
                        - toTarget.Z / _weaveOffset;
                    _field1A0 = 5 * 2;
                }
            }
            else
            {
                _field184.X = Fixed::ToFloat(Rng::GetRandomInt2(0x1000)) - 0.5F
                    - toTarget.X / _weaveOffset;
                _field184.Y = Fixed::ToFloat(Rng::GetRandomInt2(0x800)) + 0.5F;
                _field184.Z = Fixed::ToFloat(Rng::GetRandomInt2(0x1000)) - 0.5F
                    - toTarget.Z / _weaveOffset;
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
        if (LengthSquared(between) >= -458752.0F)
        {
            _field190 = WithY(_field184, 0.0F).Normalized();
        }
        else
        {
            assert(false);
            _field190 = WithY(between, 0.0F).Normalized();
        }

        Vector3 newFacing = prevFacing;
        newFacing.X += (_field190.X - prevFacing.X) / 8.0F / 2.0F;
        newFacing.Z += (_field190.Z - prevFacing.Z) / 8.0F / 2.0F;
        if (newFacing.X == 0.0F && newFacing.Z == 0.0F)
        {
            newFacing = prevFacing;
        }
        assert(!Equal(newFacing, Vector3::Zero));
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

        SetTransform(newFacing, UpVector(), static_cast<Vector3>(Position));
        _speed = Divide(::OpenTK::Mathematics::Scale(_field184, 0.05F), 2.0F);
        _targetY += _speed.Y / 2.0F;
        _speed.Y += ySpeedInc / 2.0F;
    }

    void Enemy06Entity::EnemyProcess()
    {
        CallStateProcess();
    }

    void Enemy06Entity::State0()
    {
        AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
        RequireReference(animInfo.Frame)[0]
            = (10 * (10 - _field170 / 2) - 1) / 10;
        if (CallSubroutine<Enemy06Entity>(Metadata::Enemy06Subroutines, this))
        {
            UpdateState();
        }
    }

    void Enemy06Entity::State1()
    {
        _soundSource.PlaySfx(SfxId::MOCHTROID_FLY, true);
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

        AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
        if (RequireReference(animInfo.Index)[0] != 0)
        {
            if (TestFlag(RequireReference(animInfo.Flags)[0], AnimFlags::Ended))
            {
                _models[0].SetAnimation(
                    0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
            }
        }

        if (CallSubroutine<Enemy06Entity>(Metadata::Enemy06Subroutines, this))
        {
            _soundSource.StopSfx(SfxId::MOCHTROID_FLY);
            UpdateState();
        }
    }

    void Enemy06Entity::State2()
    {
        UpdateMovement();
        if (CallSubroutine<Enemy06Entity>(Metadata::Enemy06Subroutines, this))
        {
            UpdateState();
        }
    }

    void Enemy06Entity::State3()
    {
        State2();
    }

    void Enemy06Entity::State4()
    {
        AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
        RequireReference(animInfo.Frame)[0]
            = (10 * (10 - _field170 / 2) - 1) / 10;
        UpdateMovement();
        if (CallSubroutine<Enemy06Entity>(Metadata::Enemy06Subroutines, this))
        {
            UpdateState();
        }
    }

    bool Enemy06Entity::Behavior00()
    {
        PlayerEntity& player = MainPlayer();
        const Vector3 between = static_cast<Vector3>(player.Position)
            - static_cast<Vector3>(Position);
        if (LengthSquared(between) >= 6.0F * 6.0F)
        {
            return false;
        }
        _soundSource.StopSfx(SfxId::MOCHTROID_FLY);
        return true;
    }

    bool Enemy06Entity::Behavior01()
    {
        return !Behavior00();
    }

    bool Enemy06Entity::Behavior02()
    {
        if (_field170 == 0)
        {
            return true;
        }
        _field170--;
        return false;
    }

    bool Enemy06Entity::Behavior03()
    {
        if (_field172 == 0)
        {
            return true;
        }
        _field172--;
        return false;
    }

    bool Enemy06Entity::Behavior00(Enemy06Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy06Entity::Behavior01(Enemy06Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy06Entity::Behavior02(Enemy06Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy06Entity::Behavior03(Enemy06Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }
}
