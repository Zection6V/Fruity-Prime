#pragma once

#include "enemy_module.hpp"
#include "35_Voldrum.hpp"
#include "Metadata/enemy_values.hpp"

namespace fruityprime::gameplay {

// Native counterpart of Enemy36Entity.
//
// The first Voldrum, plus a gun.  It rolls and hops the same way, but
// where the first one charges at a player it has seen, this one stops,
// tracks them -- with its aim clamped to half a unit of slope, so it
// cannot look straight up or down -- and fires a pair of shots at a time
// from either side of itself.
//
// Its numbers all come from Metadata.Enemy36Values rather than being
// written inline, which is what makes five different Voldrums out of one
// class.
class Enemy36Entity final : public Enemy35Entity {
public:
    Enemy36Entity(const EnemyScene& scene, EnemyState& agent,
                  net::PlayerState* main) noexcept;

    // Enemy36Entity.EnemyProcess and Setup.
    void EnemyProcess() override;
    void Setup() override;

protected:
    // Enemy36Entity.HandleCollision: states four and five here, rather
    // than five and six.
    bool HandleCollision() override;

private:
    // Enemy36Entity.UpdateFacing: stop, and look at the player with the
    // vertical clamped.
    void UpdateFacing();

    void State0();
    void State1();
    void State2();
    void State3();
    void State4();
    void State5();

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03();
    [[nodiscard]] bool Behavior04();
    [[nodiscard]] bool Behavior05();

    [[nodiscard]] bool CallSubroutine();
    void CallStateProcess();

    // The row of Metadata.Enemy36Values this Voldrum reads.
    [[nodiscard]] const metadata::Enemy36Values& values() const noexcept;

    // Behavior03 and Behavior04 both re-aim when they hop out of state
    // five, which is the same three lines twice in the managed class.
    void AimAtPlayerIfState5();
};

} // namespace fruityprime::gameplay


namespace fruityprime::enemy::module_36_voldrum_1 {

inline constexpr ModuleDescriptor kModule{
    36, "36_Voldrum.cs", "Enemy36Entity",
    PortStatus::SharedController, "update_voldrum1"};

// Enemy36Entity.EnemyInitialize, which is its own Setup.
void EnemyInitialize(gameplay::EnemyState& agent, std::uint8_t version,
                     std::uint8_t subtype,
                     std::uint16_t spawner_health) noexcept;

} // namespace fruityprime::enemy::module_36_voldrum_1

