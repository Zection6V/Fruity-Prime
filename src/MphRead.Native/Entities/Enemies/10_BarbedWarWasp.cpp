#include "enemy_decode_common.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/10_BarbedWarWasp.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "10_BarbedWarWasp.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_barbed_warwasp(EnemyState& agent) {
    update_wasp_controller(agent, true);
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_10_barbed_war_wasp::kModule.managed_class.size() != 0);

namespace fruityprime::enemy {

BarbedWarWaspProfile decode_barbed_warwasp_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields) noexcept {
    BarbedWarWaspProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::BarbedWarWasp)
        || fields.size() < 8) {
        return result;
    }

    // Enemy10Entity reads EnemyVersion and EnemySubtype from the first two
    // words of S08, then indexes the fixed Enemy10Values table by subtype.
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 0), 10u);
    result.variant = static_cast<std::uint8_t>(std::min<std::uint32_t>(
        detail::read_u32(fields, 4), 2u));
    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::int32_t step_distance1;
        std::int32_t step_distance2;
        std::int32_t step_distance3;
        std::int32_t circle_increment;
        std::uint16_t min_shots;
        std::uint16_t max_shots;
        std::uint16_t scan_id;
        std::uint32_t effectiveness;
    };
    static constexpr std::array<Values, 3> table = {
        Values{50, 3, 0, 15, 1024, 819, 2457, 6144, 1, 2, 215, 0xEABA},
        Values{120, 10, 2, 10, 1433, 1638, 2457, 6144, 1, 3, 191, 0xCEAA},
        Values{120, 8, 0, 10, 614, 409, 1024, 6144, 1, 1, 192, 0xF2AA}};
    const auto& values = table[result.variant];
    result.health = values.health;
    result.beam_damage = values.beam_damage;
    result.splash_damage = values.splash_damage;
    result.contact_damage = values.contact_damage;
    result.step_distance1 = values.step_distance1;
    result.step_distance2 = values.step_distance2;
    result.step_distance3 = values.step_distance3;
    result.circle_increment = values.circle_increment;
    result.min_shots = values.min_shots;
    result.max_shots = values.max_shots;
    result.scan_id = values.scan_id;
    result.effectiveness = values.effectiveness;
    result.supported = true;
    return result;
}

} // namespace fruityprime::enemy

