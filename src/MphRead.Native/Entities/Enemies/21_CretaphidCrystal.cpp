#include "21_CretaphidCrystal.hpp"

#include "19_Cretaphid.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Messaging.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <any>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;

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

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
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

        template <typename T>
        [[nodiscard]] const T& VectorAt(
            const std::vector<T>& values, std::int32_t index)
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

        const Weapons::WeaponList& bossWeapons
            = RequireReference(Weapons::BossWeapons);
        const std::shared_ptr<WeaponInfo> weapon = VectorAt(bossWeapons, 0);
        _equipInfo = std::make_shared<EquipInfo>(weapon, _beams);
        RequireReference(_equipInfo).GetAmmo = [this]() { return _ammo; };
        RequireReference(_equipInfo).SetAmmo
            = [this](std::int32_t newAmmo) { _ammo = newAmmo; };
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
        equipInfo.UnchargedDamage(damage);
        equipInfo.SplashDamage(damage);
        equipInfo.HeadshotDamage(damage);

        const Vector3 spawnDir
            = (AddY(MainPlayer().Position, 0.5F) - Position).Normalized();
        const std::shared_ptr<EntityBase> owner = SharedFrom<EntityBase>(this);
        const Formats::Culling::NodeRef nodeRef
            = RequireReference(_cretaphid).NodeRef;
        (void)BeamProjectileEntity::Spawn(
            owner, _equipInfo, Position, spawnDir,
            BeamSpawnFlags::None, nodeRef, _scene);
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
