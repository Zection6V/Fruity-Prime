#include "Assets/game_assets.hpp"
#include "Entities/scene.hpp"
#include "Mods/Network/weapon_dps.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

int main() {
    const char* rom_value = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (rom_value == nullptr || rom_value[0] == '\0') {
        std::cout << "real weapon DPS test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }

    try {
        const auto assets = fruityprime::assets::Store::from_rom(rom_value);
        const fruityprime::scene::RoomDefinition definition{
            "UNIT1_C0", "archives/unit1_C0.arc", "unit1_c0_model.bin",
            "levels/textures/unit1_c0_tex.bin",
            "unit1_c0_collision.bin", "levels/entities/Unit1_C0_Ent.bin",
            {}, {}
        };
        const auto room = fruityprime::scene::Room::load(assets, definition);
        fruityprime::net::WeaponDpsOptions options;
        options.hunter = 0;
        options.beam_type = 0;
        options.seconds = 0.5;
        options.distance = 4.0F;
        const auto result = fruityprime::net::WeaponDps::run(room, options);
        assert(result.ok);
        assert(result.placed);
        assert(result.firing_frames > 0);
        assert(result.start_health > 0);
        assert(result.native_weapon == 0);
        assert(result.hits > 0);
        fruityprime::net::WeaponDps::print(std::cout, result);
        std::cout << "native weapon DPS test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "weapon DPS test failed: " << error.what() << '\n';
        return 1;
    }
}
