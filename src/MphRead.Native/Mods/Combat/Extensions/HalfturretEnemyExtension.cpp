#include "HalfturretEnemyExtension.hpp"

#include "../../../Entities/EnemyInstanceEntity.hpp"
#include "../../../Entities/Players/PlayerEntity.hpp"
#include "../../../GameState.hpp"
#include "../../../Scene.hpp"

namespace MphRead::Mods::Combat::Extensions
{
    using namespace Entities;
    using OpenTK::Mathematics::LengthSquared;
    using OpenTK::Mathematics::Vector3;

    namespace
    {
        constexpr float RangeSquared = 15.0F * 15.0F;

        bool Enabled(const PlayerEntity& owner)
        {
            // Story hunter opponents must retain their cartridge targeting.
            return GameState::SinglePlayer() && !owner.IsBot();
        }

        bool Eligible(const EnemyInstanceEntity& enemy)
        {
            return enemy.Health() != 0
                && TypeExtensions::TestFlag(enemy.Flags(), EnemyFlags::CollideBeam)
                && !TypeExtensions::TestFlag(enemy.Flags(), EnemyFlags::Invincible)
                && enemy.GetEffectiveness(BeamType::Battlehammer) != Effectiveness::Zero;
        }
    }

    std::shared_ptr<EntityBase> HalfturretEnemyExtension::FindTarget(
        Scene& scene, const PlayerEntity& owner, Vector3 turretPosition)
    {
        if (!Enabled(owner)) return nullptr;
        std::shared_ptr<EntityBase> target;
        float closest = RangeSquared;
        auto enemies = scene.GetEnemyInstanceEntities().GetEnumerator();
        while (enemies.MoveNext())
        {
            const auto enemy = enemies.Current();
            if (!Eligible(*enemy)) continue;
            const float distance = LengthSquared(enemy->HurtVolume().GetCenter() - turretPosition);
            if (distance < closest)
            {
                closest = distance;
                target = enemy;
            }
        }
        return target;
    }

    bool HalfturretEnemyExtension::KeepTarget(Scene& scene,
        const PlayerEntity& owner, const EntityBase& target, Vector3 turretPosition)
    {
        // Native hunter/retaliation targeting is outside this extension.
        if (target.Type != EntityType::EnemyInstance) return true;
        if (!Enabled(owner)) return false;
        const auto& enemy = static_cast<const EnemyInstanceEntity&>(target);
        if (!Eligible(enemy)
            || LengthSquared(enemy.HurtVolume().GetCenter() - turretPosition) >= RangeSquared)
            return false;
        auto enemies = scene.GetEnemyInstanceEntities().GetEnumerator();
        while (enemies.MoveNext())
            if (enemies.Current().get() == &target) return true;
        return false;
    }

    Vector3 HalfturretEnemyExtension::AimPosition(EntityBase& target)
    {
        if (target.Type == EntityType::EnemyInstance)
        {
            Vector3 position;
            target.GetPosition(position); // Hurt-volume center, including offset bodies.
            return position;
        }
        return target.Position;
    }
}
