#include <cmath>
#include "Entities/gameplay.hpp"
#include "enemy_decode_common.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/42_SlenchShield.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "42_SlenchShield.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_slench_shield(EnemyState& agent) {
    // The shield follows the typed Slench linked-part controller.
    static_cast<void>(update_slench_part(agent));
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_42_slench_shield {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // Two hundred and fifty-five, given straight back after every hit:
    // the number is there so the healthbar has something to draw, not so
    // the shield can be worn down.
    agent.health = agent.health_max = 255;
    agent.body_radius = 0.0F;
}

void EnemyProcess(gameplay::EnemyState& agent,
                  const net::Vec3 slench_position,
                  const net::Vec3 slench_facing,
                  const float shield_offset) noexcept {
    const float length = std::sqrt(slench_facing.x * slench_facing.x
                                   + slench_facing.y * slench_facing.y
                                   + slench_facing.z * slench_facing.z);
    if (length <= 0.0001F) {
        agent.position = slench_position;
        return;
    }
    const float scale = shield_offset / length;
    agent.position = {slench_position.x + slench_facing.x * scale,
                      slench_position.y + slench_facing.y * scale,
                      slench_position.z + slench_facing.z * scale};
}

void UpdateScanId(gameplay::EnemyState& agent,
                  const std::uint16_t scan_id) noexcept {
    agent.scan_id = scan_id;
}

ShieldHit EnemyTakeDamage(gameplay::EnemyState& agent,
                          const bool eye_closed,
                          const bool vulnerable) noexcept {
    const ShieldHit result = !eye_closed && vulnerable
        ? ShieldHit::ThroughToSlench : ShieldHit::AgainstShield;
    // Whatever happened, the shield is whole again.  It reports which way
    // it was thrown; it is not a thing with energy.
    agent.health = agent.health_max;
    return result;
}

} // namespace fruityprime::enemy::module_42_slench_shield


static_assert(fruityprime::enemy::module_42_slench_shield::kModule.managed_class.size() != 0);

namespace fruityprime::enemy {

SlenchShieldProfile slench_shield_profile() noexcept {
    return SlenchShieldProfile{true, 255, 1.0F, 2.9F};
}

} // namespace fruityprime::enemy

