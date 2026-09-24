#include "50_HitZone.hpp"

#include "39_FireSpawn.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Messaging.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <any>
#include <cassert>
#include <cstdint>

using ::MphRead::NativeRuntime::RequireReference;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        [[nodiscard]] EnemyInstanceEntity* CastOwner(EntityBase* spawner) noexcept
        {
            EnemyInstanceEntity* owner = dynamic_cast<EnemyInstanceEntity*>(spawner);
            assert(owner != nullptr);
            return owner;
        }

        [[nodiscard]] Enemy39Entity& CastFireSpawn(EnemyInstanceEntity& owner)
        {
            Enemy39Entity* fireSpawn = dynamic_cast<Enemy39Entity*>(&owner);
            if (fireSpawn == nullptr)
            {
                throw Memory::Detail::InvalidCastException();
            }
            return *fireSpawn;
        }

        [[nodiscard]] Matrix4 ClearScale(Matrix4 transform)
        {
            const Vector3 row0
                = Vector3(transform.M11, transform.M12, transform.M13).Normalized();
            const Vector3 row1
                = Vector3(transform.M21, transform.M22, transform.M23).Normalized();
            const Vector3 row2
                = Vector3(transform.M31, transform.M32, transform.M33).Normalized();

            transform.M11 = row0.X;
            transform.M12 = row0.Y;
            transform.M13 = row0.Z;
            transform.M21 = row1.X;
            transform.M22 = row1.Y;
            transform.M23 = row1.Z;
            transform.M31 = row2.X;
            transform.M32 = row2.Y;
            transform.M33 = row2.Z;
            return transform;
        }
    }

    Enemy50Entity::Enemy50Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _enemyOwner(CastOwner(data.Spawner))
    {
    }

    void Enemy50Entity::EnemyProcess()
    {
        EnemyInstanceEntity& enemyOwner = RequireReference(_enemyOwner);
        Transform = ClearScale(static_cast<Matrix4>(enemyOwner.Transform));
        if (enemyOwner.EnemyType() == EnemyType::FireSpawn)
        {
            Enemy39Entity& fireSpawn = CastFireSpawn(enemyOwner);
            (void)ContactDamagePlayer(fireSpawn.Values().ContactDamage, true);
        }
    }

    bool Enemy50Entity::EnemyTakeDamage(EntityBase* source)
    {
        EnemyInstanceEntity& enemyOwner = RequireReference(_enemyOwner);
        if (enemyOwner.EnemyType() != EnemyType::FireSpawn)
        {
            const Effectiveness eff0 = enemyOwner.BeamEffectiveness[0];
            const Effectiveness eff1 = enemyOwner.BeamEffectiveness[1];
            const Effectiveness eff2 = enemyOwner.BeamEffectiveness[2];
            const Effectiveness eff3 = enemyOwner.BeamEffectiveness[3];
            const Effectiveness eff4 = enemyOwner.BeamEffectiveness[4];
            const Effectiveness eff5 = enemyOwner.BeamEffectiveness[5];
            const Effectiveness eff6 = enemyOwner.BeamEffectiveness[6];
            const Effectiveness eff7 = enemyOwner.BeamEffectiveness[7];
            const Effectiveness eff8 = enemyOwner.BeamEffectiveness[8];

            enemyOwner.BeamEffectiveness[0] = BeamEffectiveness[0];
            enemyOwner.BeamEffectiveness[1] = BeamEffectiveness[1];
            enemyOwner.BeamEffectiveness[2] = BeamEffectiveness[2];
            enemyOwner.BeamEffectiveness[3] = BeamEffectiveness[3];
            enemyOwner.BeamEffectiveness[4] = BeamEffectiveness[4];
            enemyOwner.BeamEffectiveness[5] = BeamEffectiveness[5];
            enemyOwner.BeamEffectiveness[6] = BeamEffectiveness[6];
            enemyOwner.BeamEffectiveness[7] = BeamEffectiveness[7];
            enemyOwner.BeamEffectiveness[8] = BeamEffectiveness[8];

            const std::int32_t damage
                = static_cast<std::int32_t>(_healthMax)
                - static_cast<std::int32_t>(_health);
            enemyOwner.TakeDamage(static_cast<std::uint32_t>(damage), source);

            enemyOwner.BeamEffectiveness[0] = eff0;
            enemyOwner.BeamEffectiveness[1] = eff1;
            enemyOwner.BeamEffectiveness[2] = eff2;
            enemyOwner.BeamEffectiveness[3] = eff3;
            enemyOwner.BeamEffectiveness[4] = eff4;
            enemyOwner.BeamEffectiveness[5] = eff5;
            enemyOwner.BeamEffectiveness[6] = eff6;
            enemyOwner.BeamEffectiveness[7] = eff7;
            enemyOwner.BeamEffectiveness[8] = eff8;
        }
        return _health > 0;
    }

    void Enemy50Entity::SetUp(
        std::uint16_t health, CollisionVolume hurtVolume, float boundingRadius)
    {
        _health = _healthMax = health;
        _hurtVolumeInit = hurtVolume;
        _boundingRadius = boundingRadius;
    }

    void Enemy50Entity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::SetActive)
        {
            EnemyInstanceEntity* enemy
                = dynamic_cast<EnemyInstanceEntity*>(info.Sender);
            if (enemy != nullptr && enemy->EnemyType() == EnemyType::FireSpawn)
            {
                const std::int32_t* value = info.Param1
                    ? std::any_cast<std::int32_t>(info.Param1.get())
                    : nullptr;
                if (value != nullptr && *value == 1)
                {
                    Flags |= EnemyFlags::CollidePlayer;
                    Flags |= EnemyFlags::CollideBeam;
                    // todo?: main player slot index for consistency?
                    HitPlayers[0] = true;
                }
                else
                {
                    Flags &= ~EnemyFlags::CollidePlayer;
                    Flags &= ~EnemyFlags::CollideBeam;
                    ClearHitPlayers();
                }
            }
        }
    }
}
