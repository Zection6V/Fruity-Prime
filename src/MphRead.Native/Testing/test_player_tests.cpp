#include "Testing/test_player.hpp"

#include <cassert>
#include <cstdint>

int main() {
    using namespace fruityprime::formats;
    using namespace fruityprime::testing::player;

    const auto controls = get_player_controls();
    assert(controls.size() == 5);
    assert(controls[0].flags == 0x76);
    assert(static_cast<std::uint16_t>(controls[0].left.button) == 0x20);
    assert(static_cast<std::uint16_t>(controls[0].right.button) == 0x10);
    assert(static_cast<std::uint16_t>(controls[0].up.button) == 0x40);
    assert(static_cast<std::uint16_t>(controls[0].down.button) == 0x80);
    assert(controls[0].field_88 == 32768);
    assert(controls[0].field_8c == 32768);
    assert(controls[0].field_90 == 409);
    assert(controls[0].field_94 == -2211);
    assert(controls[0].field_98 == -1138);

    const auto values = get_player_values();
    assert(values.size() == 8);
    for (const auto& value : values) {
        assert(value.bytes.size() == PlayerValues::Size);
    }
    const auto& first = values.front();
    assert(first.biped_traction_lr().value == 450);
    assert(first.biped_traction_fb().value == 450);
    assert(first.walk_h_speed_cap().value == 983);
    assert(first.regular_fov().value == 159744);
    assert(first.mp_ammo_cap() == 599);
    assert(first.ammo_recharge() == 15);
    assert(first.energy_start() == 100);
    assert(first.alt_ground_no_gravity() == 0);
    assert(first.alt_attack_cooldown() == 15);
    assert(first.alt_attack_cooldown()
           == first.read_u16(0x166));
    return 0;
}

