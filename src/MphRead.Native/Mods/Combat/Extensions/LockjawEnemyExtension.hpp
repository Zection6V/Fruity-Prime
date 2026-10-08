#pragma once

namespace MphRead { class Scene; }
namespace MphRead::Entities { class BombEntity; class EnemyInstanceEntity; }

namespace MphRead::Mods::Combat::Extensions
{
    // Adventure wire/snare extension, not ROM parity. Uses LockjawCollision
    // for geometry and leaves chain homing/explosions to BombEntity.
    class LockjawEnemyExtension final
    {
    public:
        LockjawEnemyExtension() = delete;
        [[nodiscard]] static bool ExplosionOverlaps(const Entities::BombEntity& bomb,
            const Entities::EnemyInstanceEntity& enemy);
        [[nodiscard]] static bool TryHit(Entities::BombEntity& bomb,
            Entities::EnemyInstanceEntity& enemy, Scene& scene);
    };
}
