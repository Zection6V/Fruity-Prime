#include "42_SlenchShield.hpp"

#include "41_Slench.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <cassert>
#include <cstdint>

using ::MphRead::NativeRuntime::RequireReference;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        [[nodiscard]] Enemy41Entity* CastSpawner(EntityBase* spawner) noexcept
        {
            Enemy41Entity* typedSpawner = dynamic_cast<Enemy41Entity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] Vector3 ScaleVector(Vector3 value, float scale) noexcept
        {
            return Vector3(
                value.X * scale,
                value.Y * scale,
                value.Z * scale);
        }
    }

    Enemy42Entity::Enemy42Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _slench(CastSpawner(data.Spawner))
    {
    }

    Enemy41Entity* Enemy42Entity::Slench() const noexcept
    {
        return _slench;
    }

    void Enemy42Entity::EnemyInitialize()
    {
        Enemy41Entity& slench = RequireReference(_slench);
        Transform = slench.Transform;
        _health = _healthMax = 255;
        Flags |= EnemyFlags::NoMaxDistance;
        SetHealthbarMessageId(2);
        _boundingRadius = 0.0F;
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, 1.0F);
    }

    void Enemy42Entity::EnemyProcess()
    {
        Enemy41Entity& slench = RequireReference(_slench);
        Vector3 facing = slench.FacingVector().Normalized();
        Vector3 position = slench.Position;
        float shieldOffset = slench.ShieldOffset();
        Position = position + ScaleVector(facing, shieldOffset);
    }

    bool Enemy42Entity::EnemyTakeDamage(EntityBase* source)
    {
        Enemy41Entity& slench = RequireReference(_slench);
        if (!TypeExtensions::TestFlag(slench.SlenchFlags(), SlenchFlags::EyeClosed)
            && TypeExtensions::TestFlag(slench.SlenchFlags(), SlenchFlags::Vulnerable))
        {
            slench.Flags &= ~EnemyFlags::Invincible;
            const std::int32_t damage = static_cast<std::int32_t>(_healthMax)
                - static_cast<std::int32_t>(_health);
            slench.TakeDamage(static_cast<std::uint32_t>(damage), source);
            slench.Flags |= EnemyFlags::Invincible;
        }
        else
        {
            (void)slench.ShieldTakeDamage(source);
        }
        _health = _healthMax;
        return false;
    }

    void Enemy42Entity::UpdateScanId(std::int32_t scanId) noexcept
    {
        _scanId = scanId;
    }
}
