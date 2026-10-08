#include "LockjawEnemyExtension.hpp"

#include "../LockjawCollision.hpp"
#include "../../../Entities/BombEntity.hpp"
#include "../../../Entities/EnemyInstanceEntity.hpp"
#include "../../../Entities/Players/PlayerEntity.hpp"
#include "../../../Formats/CollisionDetection.hpp"
#include "../../../Scene.hpp"

#include <any>
#include <cstdint>
#include <memory>

namespace MphRead::Mods::Combat::Extensions
{
    using namespace Entities;
    using NativeRuntime::RequireReference;

    bool LockjawEnemyExtension::ExplosionOverlaps(const BombEntity& bomb,
        const EnemyInstanceEntity& enemy)
    {
        const auto volume = enemy.HurtVolume();
        Formats::CollisionResult result{};
        return Formats::CollisionDetection::CheckSphereOverlapVolume(
            &volume, bomb.Position, bomb.Radius(), result);
    }

    bool LockjawEnemyExtension::TryHit(BombEntity& bomb,
        EnemyInstanceEntity& enemy, Scene& scene)
    {
        if (TypeExtensions::TestFlag(enemy.Flags(), EnemyFlags::Invincible)) return false;
        auto& owner = RequireReference(bomb.Owner());
        const auto volume = enemy.HurtVolume();
        const auto bombAt = [&owner](int index) -> BombEntity&
        {
            return RequireReference(NativeRuntime::ManagedAt(owner.SyluxBombs(), index));
        };
        if (bomb.BombIndex() == 0 && owner.SyluxBombCount() == 3)
        {
            const LockjawCollision::Triangle triangle{
                bombAt(0).Position, bombAt(1).Position, bombAt(2).Position};
            if (!LockjawCollision::SnareOverlapsVolume(triangle, volume)) return false;
            for (int i = 0; i < owner.SyluxBombCount(); ++i)
            {
                bombAt(i).SetDamage(60);
                bombAt(i).SetEnemyDamage(60);
            }
            return true;
        }
        for (int i = 0; i < bomb.BombIndex(); ++i)
        {
            if (LockjawCollision::WireOverlapsVolume(volume, bomb.Position, bombAt(i).Position))
            {
                enemy.TakeDamage(20, &bomb);
                scene.SendMessage(Message::Impact, &bomb, bomb.Owner(),
                    std::make_shared<const std::any>(static_cast<EntityBase*>(&enemy)),
                    std::make_shared<const std::any>(std::int32_t{0}));
                return true;
            }
        }
        return false;
    }
}
