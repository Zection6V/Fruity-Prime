#pragma once

#include "enemy_module.hpp"
#include "46_LesserIthrak.hpp"

namespace fruityprime::gameplay {

// Native counterpart of Enemy47Entity.
//
// The Lesser Ithrak with three differences and nothing else: it reads its
// volumes out of a different authored record, it is immune to nothing
// rather than half-resistant to everything, and its mouth is darker.  The
// managed class is fifteen lines and a page of boilerplate; this is the
// fifteen lines.
class Enemy47Entity final : public Enemy46Entity {
public:
    Enemy47Entity(const EnemyScene& scene, EnemyState& agent,
                  net::PlayerState* main) noexcept;

    // Enemy47Entity.EnemyInitialize.
    void EnemyInitialize() override;

protected:
    // Enemy47Entity.CallSubroutine and UpdateMouthMaterial.
    bool CallSubroutine() override;
    void UpdateMouthMaterial() override;
};

} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_47_greater_ithrak {

inline constexpr ModuleDescriptor kModule{
    47, "47_GreaterIthrak.cs", "Enemy47Entity",
    PortStatus::SharedController, "update_greater_ithrak"};

// Enemy47Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_47_greater_ithrak

