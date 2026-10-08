#pragma once

#include "../../../NativeRuntime/OpenTK/Mathematics.hpp"
#include <memory>

namespace MphRead { class Scene; }
namespace MphRead::Entities { class EntityBase; class PlayerEntity; }

namespace MphRead::Mods::Combat::Extensions
{
    // Adventure extension, not EU1.1 ROM parity. Owns enemy acquisition and
    // validation only; native turret code retains firing, timers and physics.
    class HalfturretEnemyExtension final
    {
    public:
        HalfturretEnemyExtension() = delete;
        [[nodiscard]] static std::shared_ptr<Entities::EntityBase> FindTarget(
            Scene& scene, const Entities::PlayerEntity& owner,
            OpenTK::Mathematics::Vector3 turretPosition);
        [[nodiscard]] static bool KeepTarget(Scene& scene,
            const Entities::PlayerEntity& owner, const Entities::EntityBase& target,
            OpenTK::Mathematics::Vector3 turretPosition);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 AimPosition(
            Entities::EntityBase& target);
    };
}
