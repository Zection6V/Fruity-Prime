#include "40_EnemySpawner.hpp"

#include "../../Formats/Collision.hpp"
#include "../../Scene.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"
#include "../../Formats/Types.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

using ::MphRead::NativeRuntime::ManagedAs;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::Multiply;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

        [[nodiscard]] AnimFlags AnimationFlagsAt(ModelInstance& model, std::int32_t index)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            ManagedArray<AnimFlags>& flags = RequireReference(animInfo.Flags);
            if (index < 0 || static_cast<std::size_t>(index) >= flags.Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return flags[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] std::int32_t AnimationIndexAt(ModelInstance& model, std::int32_t index)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            ManagedArray<std::int32_t>& indices = RequireReference(animInfo.Index);
            if (index < 0 || static_cast<std::size_t>(index) >= indices.Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return indices[static_cast<std::size_t>(index)];
        }
    }

    Enemy40Entity::Enemy40Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(ManagedAs<EnemySpawnEntity>(data.Spawner))
    {
        assert(_spawner != nullptr);
    }

    Enemy40Entity::SpawnerModelType Enemy40Entity::ModelType() const noexcept
    {
        return _modelType;
    }

    void Enemy40Entity::EnemyInitialize()
    {
        Transform = RequireReference(_data.Spawner).Transform;

        EnemySpawnEntity* spawner = dynamic_cast<EnemySpawnEntity*>(_data.Spawner);
        if (spawner != nullptr && spawner->ParentEntCol)
        {
            _parentEntCol = spawner->ParentEntCol;
            _invTransform = Multiply(_transform, spawner->ParentEntCol->Inverse2);
        }

        _boundingRadius = Fixed::ToFloat(3072);
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, _boundingRadius);
        _hurtVolume = CollisionVolume::Transform(_hurtVolumeInit, Transform);

        EnemySpawnEntity& ownSpawner = RequireReference(_spawner);
        _healthMax = _health = ownSpawner.Data.SpawnerHealth;
        Flags |= EnemyFlags::Visible;
        if (!_parentEntCol)
        {
            Flags |= EnemyFlags::Static;
        }

        std::string model = "EnemySpawner";
        if (ownSpawner.Data.EnemyType == EnemyType::WarWasp
            || ownSpawner.Data.EnemyType == EnemyType::BarbedWarWasp)
        {
            model = "PlantCarnivarous_Pod";
            _modelType = SpawnerModelType::Nest;
        }

        ModelInstance& inst = SetUpModel(model);
        if (ownSpawner.Data.EnemyType != EnemyType::WarWasp
            && ownSpawner.Data.EnemyType != EnemyType::BarbedWarWasp)
        {
            if (TestFlag(ownSpawner.Flags, SpawnerFlags::Active))
            {
                inst.SetAnimation(1);
            }
            else
            {
                inst.SetAnimation(2, AnimFlags::Paused | AnimFlags::Ended);
            }
        }
        else
        {
            inst.SetAnimation(0);
        }
        ownSpawner.Flags |= SpawnerFlags::HasModel;
    }

    void Enemy40Entity::EnemyProcess()
    {
        if (_parentEntCol)
        {
            Transform = Multiply(_invTransform, _parentEntCol->Transform);
        }

        EnemySpawnEntity& spawner = RequireReference(_spawner);
        if (TestFlag(spawner.Flags, SpawnerFlags::Active)
            && !TestFlag(spawner.Flags, SpawnerFlags::Suspended))
        {
            Flags &= ~EnemyFlags::Invincible;
        }
        else
        {
            Flags |= EnemyFlags::Invincible;
        }

        if (TestFlag(spawner.Flags, SpawnerFlags::PlayAnimation))
        {
            spawner.Flags &= ~SpawnerFlags::PlayAnimation;
            if (_animTimer == 0)
            {
                _soundSource.PlaySfx(SfxId::ENEMY_SPAWNER_SPAWN);
            }
            _animTimer = 15 * 2;
            if (_modelType == SpawnerModelType::Spawner)
            {
                _models[0].SetAnimation(2, AnimFlags::NoLoop);
            }
        }
        else if (_modelType == SpawnerModelType::Spawner)
        {
            if (TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Invincible))
            {
                _models[0].SetAnimation(2, AnimFlags::Ended | AnimFlags::Paused);
            }
            else if (TestFlag(AnimationFlagsAt(_models[0], 0), AnimFlags::Ended)
                && AnimationIndexAt(_models[0], 0) != 1)
            {
                if (AnimationIndexAt(_models[0], 0) != 0)
                {
                    _models[0].SetAnimation(1);
                }
                else
                {
                    _health = 0;
                }
            }
        }

        if (_animTimer > 0)
        {
            _animTimer--;
        }
    }

    bool Enemy40Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0 && _modelType == SpawnerModelType::Spawner)
        {
            if (AnimationIndexAt(_models[0], 0) != 0)
            {
                _models[0].SetAnimation(0, AnimFlags::NoLoop);
            }
            _health = 1;
        }
        return false;
    }
}
