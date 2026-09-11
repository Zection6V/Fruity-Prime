#include "43_SlenchNest.hpp"

#include <cassert>

#ifdef MPHREAD_NATIVE_ENEMY43_LOCAL_ENTITY_ADAPTER
namespace OpenTK::Mathematics
{
    const Vector3 Vector3::Zero{0.0F, 0.0F, 0.0F};
}
#endif

namespace MphRead::Entities::Enemies
{
    namespace
    {
        EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }
    }

    Enemy43Entity::Enemy43Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
    }

    void Enemy43Entity::EnemyInitialize()
    {
        Transform = _spawner->Transform;
        _health = _healthMax = 100;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoMaxDistance;
        HealthbarMessageId = 2;
        _boundingRadius = 0.0F;
        _hurtVolumeInit = CollisionVolume(OpenTK::Mathematics::Vector3::Zero, 0.0F);
        SetUpModel("BigEyeNest");
    }
}
