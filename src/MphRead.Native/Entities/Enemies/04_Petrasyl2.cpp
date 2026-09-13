#include "04_Petrasyl2.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

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

        [[nodiscard]] Enemy04Entity& RequireEnemy(Enemy04Entity* enemy)
        {
            if (enemy == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *enemy;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            PlayerEntity* player = PlayerEntity::Main();
            if (player == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *player;
        }

        [[nodiscard]] bool Equal(Vector3 left, Vector3 right) noexcept
        {
            return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
        }

        [[nodiscard]] Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] float DegreesToRadians(float degrees) noexcept
        {
            return degrees * (3.14159265358979323846F / 180.0F);
        }

        [[nodiscard]] std::int32_t UncheckedSubtractInt32(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }
    }
}

namespace MphRead::Metadata
{
    namespace
    {
        using Entities::EnemyBehavior;
        using Entities::EnemySubroutine;
        using Entities::Enemies::Enemy04Entity;

        const std::vector<EnemyBehavior<Enemy04Entity>> Enemy04State0{
            {1, static_cast<bool(*)(Enemy04Entity*)>(&Enemy04Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy04Entity>> Enemy04State1{
            {1, static_cast<bool(*)(Enemy04Entity*)>(&Enemy04Entity::Behavior00)}
        };
    }

    std::vector<EnemySubroutine<Enemy04Entity>> Enemy04Subroutines{
        EnemySubroutine<Enemy04Entity>(Enemy04State0),
        EnemySubroutine<Enemy04Entity>(Enemy04State1)
    };
}

namespace MphRead::Entities::Enemies
{
    Enemy04Entity::Enemy04Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(2);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy04Entity::EnemyInitialize()
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
        SetUpModel(Metadata::EnemyModelNames.at(4));
        _models[0].SetAnimation(
            0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _models[0].SetAnimation(2, 1, SetFlags::Texcoord);

        Vector3 facing = _spawner->Data.Header.FacingVector.ToFloatVector().Normalized();
        Vector3 position = _spawner->Data.Fields.S04().Position.ToFloatVector()
            + _spawner->Data.Header.Position.ToFloatVector();
        position = AddY(position, Fixed::ToFloat(5461));
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), position);
        _initialPos = position;
        _weaveOffset = Fixed::ToFloat(_spawner->Data.Fields.S04().WeaveOffset);
        _field188 = facing;
        _field194 = facing;
        _bobOffset = Fixed::ToFloat(Rng::GetRandomInt2(0x1AAB) + 1365U) / 2.0F;
        _bobSpeed = Fixed::ToFloat(Rng::GetRandomInt2(0x6000)) + 1.0F;
        UpdateState();
    }

    void Enemy04Entity::UpdateState()
    {
        if (_state2 == 0)
        {
            _models[0].SetAnimation(
                0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
            Flags &= ~EnemyFlags::NoHomingNc;
            Flags &= ~EnemyFlags::Invincible;
        }
        else if (_state2 == 1)
        {
            _models[0].SetAnimation(
                5, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            _field170 = 30 * 2;
            _soundSource.PlaySfx(SfxId::MOCHTROID_TELEPORT_IN);
        }
    }

    void Enemy04Entity::UpdateMovement()
    {
        _soundSource.PlaySfx(SfxId::MOCHTROID_FLY, true);

        if (_spawner == nullptr)
        {
            throw System::NullReferenceException();
        }
        _field188 = static_cast<Vector3>(_spawner->Position)
            - static_cast<Vector3>(Position);
        _field188 = Vector3(-_field188.Z, 0.0F, _field188.X);
        if (!Equal(_field188, Vector3::Zero))
        {
            _field188 = _field188.Normalized();
        }
        else
        {
            _field188 = FacingVector();
        }

        const std::shared_ptr<AnimationInfo> movementAnimInfo = _models[0].AnimInfo;
        if (movementAnimInfo == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (movementAnimInfo->Frame == nullptr)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t animFrame = (*movementAnimInfo->Frame)[0];

        if (_state1 != 0)
        {
            _weaveAngle += 1.5F / 2.0F;
        }
        else
        {
            const std::int32_t frameComplement = UncheckedSubtractInt32(30, animFrame);
            _weaveAngle += (1.5F * static_cast<float>(animFrame)
                + static_cast<float>(frameComplement)) / 30.0F / 2.0F;
        }
        if (_weaveAngle >= 360.0F)
        {
            _weaveAngle -= 360.0F;
        }

        const float angle = DegreesToRadians(_weaveAngle);
        const float xzSin = std::sin(angle);
        const float xzCos = std::cos(angle);
        if (_state1 != 0)
        {
            _speed.X = _initialPos.X + xzSin * _weaveOffset - Position.X;
            _speed.Z = _initialPos.Z + xzCos * _weaveOffset - Position.Z;
        }
        else
        {
            const float scaledWeave = _weaveOffset * static_cast<float>(animFrame) / 30.0F;
            _speed.X = _initialPos.X + xzSin * scaledWeave - Position.X;
            _speed.Z = _initialPos.Z + xzCos * scaledWeave - Position.Z;
        }

        _bobAngle += _bobSpeed / 2.0F;
        if (_bobAngle >= 360.0F)
        {
            _bobAngle -= 360.0F;
        }
        const float ySin = std::sin(DegreesToRadians(_bobAngle));
        _speed.Y = _initialPos.Y + ySin * _bobOffset - Position.Y;
        _speed.X /= 2.0F;
        _speed.Y /= 2.0F;
        _speed.Z /= 2.0F;

        PlayerEntity& player = MainPlayer();
        const Vector3 playerPosition = static_cast<Vector3>(player.Position);
        const Vector3 currentPositionForPlayer = static_cast<Vector3>(Position);
        const Vector3 between = playerPosition - currentPositionForPlayer;
        if (LengthSquared(between) >= 7.0F * 7.0F)
        {
            _field194 = WithY(_field188, 0.0F).Normalized();
        }
        else
        {
            _field194 = WithY(between, 0.0F).Normalized();
        }

        const Vector3 prevFacing = FacingVector();
        Vector3 newFacing = prevFacing;
        newFacing.X += (_field194.X - prevFacing.X) / 8.0F / 2.0F;
        newFacing.Z += (_field194.Z - prevFacing.Z) / 8.0F / 2.0F;
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

        const Vector3 currentUp = UpVector();
        const Vector3 currentPositionForTransform = static_cast<Vector3>(Position);
        SetTransform(newFacing, currentUp, currentPositionForTransform);
    }

    void Enemy04Entity::EnemyProcess()
    {
        CallStateProcess();
    }

    void Enemy04Entity::State0()
    {
        UpdateMovement();
        if (CallSubroutine<Enemy04Entity>(Metadata::Enemy04Subroutines, this))
        {
            UpdateState();
        }
    }

    void Enemy04Entity::State1()
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

    bool Enemy04Entity::Behavior00()
    {
        return false;
    }

    bool Enemy04Entity::Behavior01()
    {
        if (_field170 == 0)
        {
            return true;
        }
        _field170--;
        return false;
    }

    bool Enemy04Entity::Behavior00(Enemy04Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy04Entity::Behavior01(Enemy04Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }
}
