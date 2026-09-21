#include "30_Trocra.hpp"

#include "28_Gorea1B.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../ItemInstanceEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        template <typename T>
        [[nodiscard]] T& RequireReference(T* value)
        {
            if (value == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        [[nodiscard]] EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] bool Visible(const Enemy30Entity& enemy) noexcept
        {
            return TypeExtensions::TestFlag(
                static_cast<EnemyFlags>(enemy.Flags), EnemyFlags::Visible);
        }

        [[nodiscard]] bool HitMainPlayer(Enemy30Entity& enemy)
        {
            const std::int32_t slotIndex = MainPlayer().SlotIndex();
            if (slotIndex < 0
                || static_cast<std::size_t>(slotIndex) >= enemy.HitPlayers.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return enemy.HitPlayers[static_cast<std::size_t>(slotIndex)];
        }

        [[nodiscard]] float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] float Length(Vector3 value) noexcept
        {
            return std::sqrt(LengthSquared(value));
        }

        [[nodiscard]] float Clamp(float value, float min, float max) noexcept
        {
            if (value < min)
            {
                return min;
            }
            if (value > max)
            {
                return max;
            }
            return value;
        }

        [[nodiscard]] std::int32_t RoundToInt32ToEven(float value) noexcept
        {
            if (!std::isfinite(value))
            {
                return value > 0.0F
                    ? std::numeric_limits<std::int32_t>::max()
                    : std::numeric_limits<std::int32_t>::min();
            }
            const float floorValue = std::floor(value);
            const float fraction = value - floorValue;
            double rounded = floorValue;
            if (fraction > 0.5F)
            {
                rounded = static_cast<double>(floorValue) + 1.0;
            }
            else if (fraction == 0.5F)
            {
                const auto floorInteger = static_cast<std::int64_t>(floorValue);
                rounded = (floorInteger & 1LL) == 0
                    ? floorValue
                    : static_cast<double>(floorValue) + 1.0;
            }
            if (rounded >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            {
                return std::numeric_limits<std::int32_t>::max();
            }
            if (rounded <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
            {
                return std::numeric_limits<std::int32_t>::min();
            }
            return static_cast<std::int32_t>(rounded);
        }
    }

    Enemy30Entity::Enemy30Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
    }

    void Enemy30Entity::EnemyInitialize()
    {
        InitializeCommon(_spawner);
        Flags |= EnemyFlags::OnRadar;
        Flags &= ~EnemyFlags::NoHomingCo;
        _health = 15;
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, 1.0F);
        SetUpModel("PowerBomb");
        Gorea1B = nullptr;
    }

    void Enemy30Entity::EnemyProcess()
    {
        if (Visible(*this))
        {
            if (_health > 0 && HitMainPlayer(*this))
            {
                DieAndSpawnEffect(164);
            }
            if (_health > 0)
            {
                CollisionResult discard{};
                const Vector3 position = Position;
                const Vector3 travel = _prevPos - position;
                if (LengthSquared(travel) > 1.0F / 128.0F
                    && CollisionDetection::CheckBetweenPoints(
                        _prevPos, position, TestFlags::Beams, _scene, discard))
                {
                    DieAndSpawnEffect(164);
                }
            }
        }
    }

    void Enemy30Entity::DieAndSpawnEffect(std::int32_t effectId)
    {
        SpawnEffect(effectId, Position);

        const Vector3 playerPosition = MainPlayer().Position;
        const Vector3 entityPosition = Position;
        Vector3 between = playerPosition - entityPosition;
        const float distance = Length(between);
        if (distance < 2.0F)
        {
            CollisionResult discard{};
            const Vector3 position = Position;
            const Vector3 limitMin(
                position.X - 2.0F, position.Y - 2.0F, position.Z - 2.0F);
            const Vector3 limitMax(
                position.X + 2.0F, position.Y + 2.0F, position.Z + 2.0F);
            const auto candidates = CollisionDetection::GetCandidatesForLimits(
                nullptr, Vector3::Zero, 0, limitMin, limitMax, false, _scene);
            if (!CollisionDetection::CheckBetweenPoints(
                candidates, _prevPos, Position, TestFlags::Beams, _scene, discard))
            {
                std::int32_t damage = 15;
                float force = 1.0F;
                if (!HitMainPlayer(*this))
                {
                    const float factor = Clamp(distance / 2.0F, 0.0F, 1.0F);
                    damage -= RoundToInt32ToEven(
                        static_cast<float>(damage) - 15.0F * factor);
                    force -= factor;
                }
                if (distance > 1.0F / 128.0F)
                {
                    between = between.Normalized() * force;
                }
                else
                {
                    between = Vector3::UnitY * force;
                }
                MainPlayer().TakeDamage(
                    damage, DamageFlags::NoDmgInvuln, between, this);
            }
        }

        _soundSource.PlaySfx(
            SfxId::GOREA_ATTACK3B, false, false, -1.0F, true);
        _health = 0;
        Flags &= ~EnemyFlags::Visible;
        Flags &= ~EnemyFlags::CollidePlayer;
        Flags &= ~EnemyFlags::CollideBeam;
        Flags &= ~EnemyFlags::OnRadar;
        Flags |= EnemyFlags::Invincible;
        const Vector3 position = Position;
        Position = Vector3(position.X, 524288.0F, position.Z);
        _speed = Vector3::Zero;
    }

    bool Enemy30Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0)
        {
            if (Gorea1B != nullptr)
            {
                RequireReference(_scene).SendMessage(
                    Message::Destroyed, this, Gorea1B, 0, 0);
            }

            bool spawn = false;
            ItemType itemType = ItemType::None;
            const std::uint32_t random = Rng::GetRandomInt2(190);
            if (random < 10)
            {
                spawn = true;
                itemType = ItemType::HealthSmall;
            }
            else if (random < 70)
            {
                spawn = true;
                itemType = ItemType::UASmall;
            }
            if (spawn)
            {
                const std::int32_t despawnTime = 300 * 2;
                auto item = std::make_shared<ItemInstanceEntity>(
                    ItemInstanceEntityData(Position, itemType, despawnTime),
                    NodeRef, _scene);
                RequireReference(_scene).AddEntity(item);
            }
        }
        return false;
    }

    void Enemy30Entity::Explode()
    {
        DieAndSpawnEffect(75);
    }

    void Enemy30Entity::SetSpeed(Vector3 speed)
    {
        _speed = speed;
    }
}
