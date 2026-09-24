#include "43_SlenchNest.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <cassert>

using ::MphRead::NativeRuntime::ManagedAs;

namespace MphRead::Entities::Enemies
{
    namespace
    {
    }

    Enemy43Entity::Enemy43Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(ManagedAs<EnemySpawnEntity>(data.Spawner))
    {
        assert(_spawner != nullptr);
    }

    void Enemy43Entity::EnemyInitialize()
    {
        if (_spawner == nullptr)
        {
            throw System::NullReferenceException();
        }
        Transform = _spawner->Transform;
        _health = _healthMax = 100;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoMaxDistance;
        SetHealthbarMessageId(2);
        _boundingRadius = 0.0F;
        _hurtVolumeInit = CollisionVolume(OpenTK::Mathematics::Vector3::Zero, 0.0F);
        SetUpModel("BigEyeNest");
    }
}
