#include "51_CarnivorousPlant.hpp"

#include "../../Metadata/Metadata.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <cassert>

using ::MphRead::NativeRuntime::RequireReference;

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

    Enemy51Entity::Enemy51Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
    }

    void Enemy51Entity::EnemyInitialize()
    {
        Transform = RequireReference(_data.Spawner).Transform;
        _prevPos = Position;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Static;
        Flags |= EnemyFlags::NoMaxDistance; // the game doesn't set this
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        _health = _healthMax = spawner.Data.Fields.S07().EnemyHealth;
        _boundingRadius = Fixed::ToFloat(1843);
        _hurtVolumeInit = CollisionVolume(
            ::OpenTK::Mathematics::Vector3(0.0F, Fixed::ToFloat(409), 0.0F), _boundingRadius);
        _hurtVolume = CollisionVolume::Transform(_hurtVolumeInit, Transform);
        const ObjectMetadata& meta = Metadata::GetObjectById(spawner.Data.Fields.S07().EnemySubtype);
        SetUpModel(meta.Name);
    }

    void Enemy51Entity::EnemyProcess()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        ContactDamagePlayer(spawner.Data.Fields.S07().EnemyDamage, false);
    }
}
