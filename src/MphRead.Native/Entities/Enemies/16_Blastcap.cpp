#include "16_Blastcap.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

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

        template <typename T>
        [[nodiscard]] T& RequireReference(T* value)
        {
            if (value == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        [[nodiscard]] Enemy16Entity& RequireEnemy(Enemy16Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }
    }

    Enemy16Entity::Enemy16Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(4);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        (*processes)[2] = [this]() { State2(); };
        (*processes)[3] = [this]() { State3(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy16Entity::EnemyInitialize()
    {
        const auto header = RequireReference(_spawner).Data.Header;
        SetTransform(
            header.FacingVector.ToFloatVector(),
            Vector3(0.0F, 1.0F, 0.0F),
            header.Position.ToFloatVector());
        _health = _healthMax = 12;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _agitateTimer = 60 * 2; // todo: FPS stuff
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(
            RequireReference(_spawner).Data.Fields.S00().Volume0);
        if (16 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetUpModel(Metadata::EnemyModelNames[16], 2);
        _cloudTimer = 150 * 2; // todo: FPS stuff
    }

    void Enemy16Entity::EnemyProcess()
    {
        CallStateProcess();
    }

    bool Enemy16Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0)
        {
            _health = 1;
            _state2 = 3;
            _subId = _state2;
            Flags |= EnemyFlags::Invincible;
            Flags &= ~EnemyFlags::Visible;
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            RequireReference(_scene).SpawnEffect(
                4,
                Vector3(1.0F, 0.0F, 0.0F),
                Vector3(0.0F, 1.0F, 0.0F),
                Position);
            if (!_initialCloudHit)
            {
                const float radii
                    = MainPlayer().Volume().SphereRadius + _cloudRadius;
                const Vector3 position = static_cast<Vector3>(Position);
                const Vector3 spherePosition
                    = MainPlayer().Volume().SpherePosition;
                const Vector3 between = position - spherePosition;
                if (LengthSquared(between) < radii * radii)
                {
                    _initialCloudHit = true;
                    MainPlayer().TakeDamage(
                        2, DamageFlags::NoDmgInvuln, std::nullopt, this);
                }
            }
        }
        return false;
    }

    void Enemy16Entity::State0()
    {
        (void)CallSubroutine<Enemy16Entity>(Metadata::Enemy16Subroutines, this);
    }

    void Enemy16Entity::State1()
    {
        State0();
    }

    void Enemy16Entity::State2()
    {
        State0();
    }

    void Enemy16Entity::State3()
    {
        _cloudTick = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(_cloudTick) + 1U);
        State0();
    }

    bool Enemy16Entity::Behavior00()
    {
        if (_cloudTick % (10 * 2) != 0)
        {
            return false;
        }
        const float radii = MainPlayer().Volume().SphereRadius + _cloudRadius;
        const Vector3 position = static_cast<Vector3>(Position);
        const Vector3 spherePosition = MainPlayer().Volume().SpherePosition;
        const Vector3 between = position - spherePosition;
        if (LengthSquared(between) < radii * radii)
        {
            _initialCloudHit = true;
            MainPlayer().TakeDamage(2, DamageFlags::None, std::nullopt, this);
            return true;
        }
        return false;
    }

    bool Enemy16Entity::Behavior01()
    {
        if (_cloudTimer > 0)
        {
            --_cloudTimer;
        }
        else
        {
            _health = 0;
        }
        return false;
    }

    bool Enemy16Entity::Behavior02()
    {
        const float radii = MainPlayer().Volume().SphereRadius + _nearRadius;
        const Vector3 position = static_cast<Vector3>(Position);
        const Vector3 spherePosition = MainPlayer().Volume().SpherePosition;
        const Vector3 between = position - spherePosition;
        if (LengthSquared(between) < radii * radii)
        {
            return false;
        }
        _models[0].SetAnimation(2);
        return true;
    }

    bool Enemy16Entity::Behavior03()
    {
        const std::int32_t slotIndex = MainPlayer().SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (!HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            return false;
        }
        _initialCloudHit = true;
        MainPlayer().TakeDamage(2, DamageFlags::None, std::nullopt, this);
        TakeDamage(100, nullptr);
        return true;
    }

    bool Enemy16Entity::Behavior04()
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        if (animInfo == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (animInfo->Flags == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (((*animInfo->Flags)[0] & AnimFlags::Ended) == AnimFlags::None)
        {
            return false;
        }
        _models[0].SetAnimation(2);
        return true;
    }

    bool Enemy16Entity::Behavior05()
    {
        const float radii = MainPlayer().Volume().SphereRadius + _nearRadius;
        const Vector3 position = static_cast<Vector3>(Position);
        const Vector3 spherePosition = MainPlayer().Volume().SpherePosition;
        const Vector3 between = position - spherePosition;
        if (LengthSquared(between) < radii * radii)
        {
            _models[0].SetAnimation(1);
            return true;
        }
        return false;
    }

    bool Enemy16Entity::Behavior06()
    {
        if (_agitateTimer > 0)
        {
            --_agitateTimer;
            return false;
        }
        _soundSource.PlaySfx(SfxId::BLASTCAP_AGITATE);
        _agitateTimer = 60 * 2; // todo: FPS stuff
        _models[0].SetAnimation(0, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy16Entity::Behavior00(Enemy16Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy16Entity::Behavior01(Enemy16Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy16Entity::Behavior02(Enemy16Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy16Entity::Behavior03(Enemy16Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy16Entity::Behavior04(Enemy16Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy16Entity::Behavior05(Enemy16Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy16Entity::Behavior06(Enemy16Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }
}
