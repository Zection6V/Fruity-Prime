#include "21_CretaphidCrystal.hpp"

#include "19_Cretaphid.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Messaging.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <any>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        Enemy19Entity* CastCretaphid(EntityBase* spawner) noexcept
        {
            Enemy19Entity* owner = dynamic_cast<Enemy19Entity*>(spawner);
            assert(owner != nullptr);
            return owner;
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

        template <typename T>
        [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] std::shared_ptr<EntityBase> SharedEntity(
            Scene* scene, EntityBase* entity)
        {
            Scene& sceneRef = RequireReference(scene);
            auto enumerator = sceneRef.Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                std::shared_ptr<EntityBase> current = enumerator.Current();
                if (!current)
                {
                    throw System::NullReferenceException();
                }
                if (current.get() == entity)
                {
                    return current;
                }
            }
            throw SceneDetail::InvalidOperationException();
        }

        template <typename T>
        [[nodiscard]] T& VectorAt(std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] MessageObject BoxInt32(std::int32_t value)
        {
            return std::make_shared<const std::any>(value);
        }
    }

    Enemy21Entity::Enemy21Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _cretaphid(CastCretaphid(data.Spawner))
    {
    }

    void Enemy21Entity::SetUp(std::shared_ptr<Node> attachNode,
        std::int32_t scanId, std::uint32_t effectiveness,
        std::uint16_t health, Vector3 position)
    {
        SetHealthbarMessageId(1);
        _attachNode = attachNode;
        _scanId = scanId;
        Metadata::LoadEffectiveness(effectiveness, BeamEffectiveness);
        _health = _healthMax = health;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoMaxDistance;

        Node& node = RequireReference(attachNode);
        Matrix4 transform = GetTransformMatrix(
            node.Transform.Row2().Xyz(), node.Transform.Row1().Xyz());
        const Vector3 translation = node.Transform.Row3().Xyz() + position;
        transform.M41 = translation.X;
        transform.M42 = translation.Y;
        transform.M43 = translation.Z;
        Transform = transform;

        _hurtVolumeInit = CollisionVolume(Vector3::Zero, 1.0F);
        _boundingRadius = 1.0F;

        const std::shared_ptr<WeaponInfo> weapon = VectorAt(Weapons::BossWeapons, 0);
        std::shared_ptr<EquipInfo> equipInfo = std::make_shared<EquipInfo>();
        RequireReference(equipInfo).SetWeapon(weapon);
        RequireReference(equipInfo).SetBeams(RequireReference(_beams));
        _equipInfo = std::move(equipInfo);
        RequireReference(_equipInfo).SetGetAmmo([this]() { return _ammo; });
        RequireReference(_equipInfo).SetSetAmmo(
            [this](std::int32_t newAmmo) { _ammo = newAmmo; });
    }

    void Enemy21Entity::EnemyProcess()
    {
        Enemy19Entity& cretaphid = RequireReference(_cretaphid);
        cretaphid.UpdateTransforms(false);

        Node& attachNode = RequireReference(_attachNode);
        Position = attachNode.Animation.Row3().Xyz() + cretaphid.Position;

        if (_health > 0
            && !cretaphid.SoundSource().CheckEnvironmentSfx(5))
        {
            cretaphid.SoundSource().PlayEnvironmentSfx(6);
        }
    }

    void Enemy21Entity::SpawnBeam(std::uint16_t damage)
    {
        EquipInfo& equipInfo = RequireReference(_equipInfo);
        equipInfo.SetUnchargedDamage(damage);
        equipInfo.SetSplashDamage(damage);
        equipInfo.SetHeadshotDamage(damage);

        const Vector3 spawnDir
            = (AddY(MainPlayer().Position, 0.5F) - Position).Normalized();
        (void)BeamProjectileEntity::Spawn(
            SharedEntity(_scene, this), _equipInfo, Position, spawnDir,
            BeamSpawnFlags::None, RequireReference(_cretaphid).NodeRef, _scene);
    }

    bool Enemy21Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0)
        {
            _health = 1;
            Flags |= EnemyFlags::Invincible;
            RequireReference(_scene).SendMessage(
                Message::SetActive, this, _cretaphid,
                BoxInt32(0), BoxInt32(0));
        }
        return false;
    }
}
