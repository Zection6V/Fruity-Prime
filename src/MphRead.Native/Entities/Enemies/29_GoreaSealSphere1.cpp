#include "29_GoreaSealSphere1.hpp"

#include "28_Gorea1B.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedSubtract;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        template <typename T>
        [[nodiscard]] const T& ManagedListAt(
            const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw Memory::Detail::ArgumentOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        template <typename T>
        [[nodiscard]] const T& ManagedArrayAt(
            const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

    }

    Enemy29Entity::Enemy29Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene)
    {
    }

    std::int32_t Enemy29Entity::Damage() const noexcept
    {
        return _damage;
    }

    std::int32_t Enemy29Entity::DamageTimer() const noexcept
    {
        return _damageTimer;
    }

    void Enemy29Entity::EnemyInitialize()
    {
        if (Enemy28Entity* owner = dynamic_cast<Enemy28Entity*>(_owner))
        {
            _gorea1B = owner;
            Flags &= ~EnemyFlags::Visible;
            Flags |= EnemyFlags::NoHomingNc;
            Flags |= EnemyFlags::NoHomingCo;
            Flags |= EnemyFlags::Invincible;
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            Flags |= EnemyFlags::NoMaxDistance;
            _state1 = _state2 = 255;
            SetHealthbarMessageId(7);

            ModelInstance& ownerModel
                = RequireReference(ManagedListAt(owner->GetModels(), 0));
            Model& model = RequireReference(ownerModel.Model());
            _attachNode = model.GetNodeByName("ChestBall1");

            const Vector3 facing = owner->FacingVector();
            const Vector3 up = owner->UpVector();
            const Vector3 position = owner->Position;
            SetTransform(facing, up, position);
            Position = static_cast<Vector3>(Position)
                + RequireReference(_attachNode).Position;
            _prevPos = Position;
            _boundingRadius = 1.0F;
            _hurtVolumeInit = CollisionVolume(
                Vector3::Zero, owner->Scale.X);
            _health = 65535;
            _healthMax = 3000;
            _scanId = 0;
        }
    }

    void Enemy29Entity::Activate()
    {
        const std::int32_t index
            = static_cast<std::int32_t>(EnemyType());
        _scanId = ManagedArrayAt(Metadata::EnemyScanIds, index);
        Position = RequireReference(_gorea1B).Position;
        Flags &= ~EnemyFlags::Visible;
        Flags |= EnemyFlags::CollidePlayer;
        Flags |= EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::NoHomingNc;
        Flags &= ~EnemyFlags::NoHomingCo;
    }

    void Enemy29Entity::Deactivate()
    {
        _scanId = 0;
        Flags &= ~EnemyFlags::Visible;
        Flags &= ~EnemyFlags::CollidePlayer;
        Flags &= ~EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::NoHomingCo;
    }

    void Enemy29Entity::EnemyProcess()
    {
        Enemy28Entity& gorea = RequireReference(_gorea1B);
        if (TypeExtensions::TestFlag(
            static_cast<EnemyFlags>(gorea.Flags), EnemyFlags::Visible))
        {
            const Matrix4 transform
                = GetNodeTransform(_gorea1B, _attachNode.get());
            Position = transform.Row3().Xyz();
        }
        if (_damageTimer > 0)
        {
            --_damageTimer;
        }
    }

    bool Enemy29Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;

        const std::int32_t change
            = 65535 - static_cast<std::int32_t>(_health);
        _damage = UncheckedAdd(_damage, change);
        if (_damage > static_cast<std::int32_t>(_healthMax))
        {
            _damage = static_cast<std::int32_t>(_healthMax);
        }
        std::int32_t prevDamage = UncheckedSubtract(_damage, change);
        _health = 65535;

        if (!TypeExtensions::TestFlag(
            static_cast<EnemyFlags>(Flags), EnemyFlags::Invincible))
        {
            std::int32_t damage = _damage;
            if (damage / 1000 == prevDamage / 1000)
            {
                damage %= 1000;
                prevDamage %= 1000;
                if (damage / 10 > prevDamage / 10)
                {
                    _soundSource.PlaySfx(SfxId::GOREA_1B_DAMAGE);
                    SpawnEffect(44, Position);
                }
            }

            _damageTimer = 10 * 2;
            Enemy28Entity& gorea = RequireReference(_gorea1B);
            ModelInstance& ownerModel
                = RequireReference(ManagedListAt(gorea.GetModels(), 0));
            Model& model = RequireReference(ownerModel.Model());
            Material& material
                = RequireReference(model.GetMaterialByName("ChestCore"));
            material.Ambient = Ambient;
            material.Diffuse = ColorRgb(31, 0, 0);
        }
        return false;
    }
}
